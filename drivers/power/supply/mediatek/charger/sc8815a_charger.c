/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */

#define pr_fmt(fmt)	"[sc8815a]:%s: " fmt, __func__

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/bitops.h>
#include <linux/math64.h>


#include "sc8815a_reg.h"
//prize add by lipengpeng 20220621 start 
//#include "mtk_charger.h"
//prize add by lipengpeng 20220621 end 
//#include "charger_class.h"
#include "mtk_charger_intf.h"
enum {
    PN_SC8815A, 
};

static int pn_data[] = {
	[PN_SC8815A] = 0x05,
};

struct chg_para{
	int vlim;
	int ilim;

	int vreg;
	int ichg;
};

struct sc8815a_platform_data {
	int iprechg;
	int iterm;

	int boostv;
	int boosti;

	struct chg_para usb;
};

struct sc8815a {
	struct device *dev;
	struct i2c_client *client;

	int part_no;
	int revision;

	const char *chg_dev_name;
	const char *eint_name;

    int irq_gpio;
	int chg_en_gpio;
	int pstop_gpio;

	int psy_usb_type;

	int status;
	int irq;

	struct mutex i2c_rw_lock;

	bool charge_enabled;	/* Register bit status */
	bool power_good;
	bool vbus_good;
	int input_curr_limit;

	struct sc8815a_platform_data *platform_data;
	struct charger_device *chg_dev;

	struct power_supply_desc psy_desc;
	struct power_supply *psy;
	//struct power_supply *chg_psy;
};

static const struct charger_properties sc8815a_chg_props = {
	.alias_name = "sc8815a",
};

static int __sc8815a_read_reg(struct sc8815a *sc, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(sc->client, reg);
	if (ret < 0) {
		pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __sc8815a_write_reg(struct sc8815a *sc, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(sc->client, reg, val);
	if (ret < 0) {
		pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
		       val, reg, ret);
		return ret;
	}
	return 0;
}

static int sc8815a_read_byte(struct sc8815a *sc, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sc8815a_read_reg(sc, reg, data);
	mutex_unlock(&sc->i2c_rw_lock);

	return ret;
}

static int sc8815a_write_byte(struct sc8815a *sc, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sc8815a_write_reg(sc, reg, data);
	mutex_unlock(&sc->i2c_rw_lock);

	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

	return ret;
}

static int sc8815a_update_bits(struct sc8815a *sc, u8 reg, u8 mask, u8 data)
{
	int ret;
	u8 tmp;

	mutex_lock(&sc->i2c_rw_lock);
	ret = __sc8815a_read_reg(sc, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= data & mask;

	ret = __sc8815a_write_reg(sc, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&sc->i2c_rw_lock);
	return ret;
}

static int sc8815a_set_key(struct sc8815a *sc)
{
	sc8815a_write_byte(sc, 0x7D, 0x48);
	sc8815a_write_byte(sc, 0x7D, 0x54);
	sc8815a_write_byte(sc, 0x7D, 0x53);
	return sc8815a_write_byte(sc, 0x7D, 0x38);
}

static int sc8815a_enable_otg(struct sc8815a *sc)
{
	//int ret =0;
	u8 val = SC8815A_OTG_ENABLE << SC8815A_OTG_CONFIG_SHIFT;
	//return ret;
	return sc8815a_update_bits(sc, SC8815A_REG_09,
				   SC8815A_OTG_CONFIG_MASK, val);
}

static int sc8815a_disable_otg(struct sc8815a *sc)
{
	u8 val = SC8815A_OTG_DISABLE << SC8815A_OTG_CONFIG_SHIFT;

	return sc8815a_update_bits(sc, SC8815A_REG_09,
				   SC8815A_OTG_CONFIG_MASK, val);
}

static int sc8815a_disable_hvdcp(struct sc8815a *sc)
{
	int ret;
	u8 val = SC8815A_HVDCP_DISABLE << SC8815A_HVDCPEN_SHIFT;

	ret = sc8815a_update_bits(sc, SC8815A_REG_02, 
				SC8815A_HVDCPEN_MASK, val);
	return ret;
}

static int sc8815a_enable_charger(struct sc8815a *sc)
{
	int ret;

	u8 val = SC8815A_CHG_ENABLE << SC8815A_CHG_CONFIG_SHIFT;

	ret = sc8815a_update_bits(sc, SC8815A_REG_03, 
				SC8815A_CHG_CONFIG_MASK, val);
	return ret;
}

static int sc8815a_disable_charger(struct sc8815a *sc)
{
	int ret;

	u8 val = SC8815A_CHG_DISABLE << SC8815A_CHG_CONFIG_SHIFT;

	ret = sc8815a_update_bits(sc, SC8815A_REG_03, 
				SC8815A_CHG_CONFIG_MASK, val);
	return ret;
}

static int sc8815a_adc_start(struct sc8815a *sc, bool oneshot)
{
	u8 val;
	int ret;
	
	ret = sc8815a_read_byte(sc, SC8815A_REG_02, &val);
	if (ret < 0) {
		dev_err(sc->dev, "%s failed to read register 0x02:%d\n", __func__, ret);
	}
	
	if (((val & SC8815A_CONV_RATE_MASK) >> SC8815A_CONV_RATE_SHIFT) == SC8815A_ADC_CONTINUE_ENABLE)
		return 0;

	if (oneshot) {
		ret = sc8815a_update_bits(sc, SC8815A_REG_02, SC8815A_CONV_START_MASK,
				SC8815A_CONV_START << SC8815A_CONV_START_SHIFT);
	}
	else {
		ret = sc8815a_update_bits(sc, SC8815A_REG_02, SC8815A_CONV_RATE_MASK,
				SC8815A_ADC_CONTINUE_ENABLE << SC8815A_CONV_RATE_SHIFT);
	}
	
	return ret;
}

static int sc8815a_adc_stop(struct sc8815a *sc)
{
	return sc8815a_update_bits(sc, SC8815A_REG_02, SC8815A_CONV_RATE_MASK,
				SC8815A_ADC_CONTINUE_DISABLE << SC8815A_CONV_RATE_SHIFT);
}

int sc8815a_set_chargecurrent(struct sc8815a *sc, int curr)
{
	u8 ichg;

	if (curr < SC8815A_ICHG_BASE)
		curr = SC8815A_ICHG_BASE;

    ichg = (curr - SC8815A_ICHG_BASE)/SC8815A_ICHG_LSB;

	return sc8815a_update_bits(sc, SC8815A_REG_04, 
						SC8815A_ICHG_MASK, ichg << SC8815A_ICHG_SHIFT);

}

int sc8815a_set_term_current(struct sc8815a *sc, int curr)
{
	u8 iterm;

	if (curr < SC8815A_ITERM_BASE)
		curr = SC8815A_ITERM_BASE;

    iterm = (curr - SC8815A_ITERM_BASE) / SC8815A_ITERM_LSB;

	return sc8815a_update_bits(sc, SC8815A_REG_05, 
						SC8815A_ITERM_MASK, iterm << SC8815A_ITERM_SHIFT);

}

int sc8815a_get_term_current(struct sc8815a *sc, int *curr)
{
    u8 reg_val;
	int iterm;
	int ret;

	ret = sc8815a_read_byte(sc, SC8815A_REG_05, &reg_val);
	if (!ret) {
		iterm = (reg_val & SC8815A_ITERM_MASK) >> SC8815A_ITERM_SHIFT;
        iterm = iterm * SC8815A_ITERM_LSB + SC8815A_ITERM_BASE;

		*curr = iterm * 1000;
	}
    return ret;
}

int sc8815a_set_prechg_current(struct sc8815a *sc, int curr)
{
	u8 iprechg;

	if (curr < SC8815A_IPRECHG_BASE)
		curr = SC8815A_IPRECHG_BASE;

    iprechg = (curr - SC8815A_IPRECHG_BASE) / SC8815A_IPRECHG_LSB;

	return sc8815a_update_bits(sc, SC8815A_REG_05, 
						SC8815A_IPRECHG_MASK, iprechg << SC8815A_IPRECHG_SHIFT);

}

int sc8815a_set_chargevolt(struct sc8815a *sc, int volt)
{
	u8 val;

	if (volt < SC8815A_VREG_BASE)
		volt = SC8815A_VREG_BASE;

	val = (volt - SC8815A_VREG_BASE)/SC8815A_VREG_LSB;
	return sc8815a_update_bits(sc, SC8815A_REG_06, 
						SC8815A_VREG_MASK, val << SC8815A_VREG_SHIFT);
}

int sc8815a_get_chargevol(struct sc8815a *sc, int *volt)
{
    u8 reg_val;
	int vchg;
	int ret;

	ret = sc8815a_read_byte(sc, SC8815A_REG_06, &reg_val);
	if (!ret) {
		vchg = (reg_val & SC8815A_VREG_MASK) >> SC8815A_VREG_SHIFT;
		vchg = vchg * SC8815A_VREG_LSB + SC8815A_VREG_BASE;
		*volt = vchg * 1000;
	}
    return ret;
}

int sc8815a_adc_read_vbus_volt(struct sc8815a *sc, u32 *vol)
{
	uint8_t val;
	int volt;
	int ret;
	ret = sc8815a_read_byte(sc, SC8815A_REG_11, &val);
	if (ret < 0) {
		dev_err(sc->dev, "read vbus voltage failed :%d\n", ret);
	} else{
		volt = SC8815A_VBUSV_BASE + ((val & SC8815A_VBUSV_MASK) >> SC8815A_VBUSV_SHIFT) * SC8815A_VBUSV_LSB ;
//prize add by lipengpeng 20220708 start 
		if(volt < 2700)
			volt=0;
//prize add by lipengpeng 20220708 end 
		*vol = volt * 1000;
	}

    return ret;
}

int sc8815a_adc_read_charge_current(struct sc8815a *sc, u32 *cur)
{
	uint8_t val;
	int curr;
	int ret;
	ret = sc8815a_read_byte(sc, SC8815A_REG_12, &val);
	if (ret < 0) {
		dev_err(sc->dev, "read charge current failed :%d\n", ret);
	} else{
		curr = (int)(SC8815A_ICHGR_BASE + ((val & SC8815A_ICHGR_MASK) >> SC8815A_ICHGR_SHIFT) * SC8815A_ICHGR_LSB) ;
		*cur = curr * 1000; 
	}

    return ret;
}

int sc8815a_set_force_vindpm(struct sc8815a *sc, bool en)
{
    u8 val;

    if (en)
        val = SC8815A_FORCE_VINDPM_ENABLE;
    else
        val = SC8815A_FORCE_VINDPM_DISABLE;
    

    return sc8815a_update_bits(sc, SC8815A_REG_0D, 
						SC8815A_FORCE_VINDPM_MASK, val << SC8815A_FORCE_VINDPM_SHIFT);
}

int sc8815a_set_input_volt_limit(struct sc8815a *sc, int volt)
{
	u8 val;

	if (volt < SC8815A_VINDPM_BASE)
		volt = SC8815A_VINDPM_BASE;
    
    sc8815a_set_force_vindpm(sc, true);

	val = (volt - SC8815A_VINDPM_BASE) / SC8815A_VINDPM_LSB;
	return sc8815a_update_bits(sc, SC8815A_REG_0D, 
						SC8815A_VINDPM_MASK, val << SC8815A_VINDPM_SHIFT);
}

int sc8815a_set_input_current_limit(struct sc8815a *sc, int curr)
{
	u8 val;

	if (curr > sc->input_curr_limit)
		curr = sc->input_curr_limit;

	if (curr < SC8815A_IINLIM_BASE)
		curr = SC8815A_IINLIM_BASE;

	val = (curr - SC8815A_IINLIM_BASE) / SC8815A_IINLIM_LSB;

	return sc8815a_update_bits(sc, SC8815A_REG_00, SC8815A_IINLIM_MASK, 
						val << SC8815A_IINLIM_SHIFT);
}

int sc8815a_get_input_volt_limit(struct sc8815a *sc, u32 *volt)
{
    u8 reg_val;
	int vchg;
	int ret;

	ret = sc8815a_read_byte(sc, SC8815A_REG_0D, &reg_val);
	if (!ret) {
		vchg = (reg_val & SC8815A_VINDPM_MASK) >> SC8815A_VINDPM_SHIFT;
		vchg = vchg * SC8815A_VINDPM_LSB + SC8815A_VINDPM_BASE;
		*volt = vchg * 1000;
	}
    return ret;
}

int sc8815a_get_input_current_limit(struct  sc8815a *sc, u32 *curr)
{
    u8 reg_val;
	int icl;
	int ret;

	ret = sc8815a_read_byte(sc, SC8815A_REG_00, &reg_val);
	if (!ret) {
		icl = (reg_val & SC8815A_IINLIM_MASK) >> SC8815A_IINLIM_SHIFT;
		icl = icl * SC8815A_IINLIM_LSB + SC8815A_IINLIM_BASE;
		*curr = icl * 1000;
	}

    return ret;
}

int sc8815a_set_watchdog_timer(struct sc8815a *sc, u8 timeout)
{
	u8 val;

	val = (timeout - SC8815A_WDT_BASE) / SC8815A_WDT_LSB;
	val <<= SC8815A_WDT_SHIFT;

	return sc8815a_update_bits(sc, SC8815A_REG_07, 
						SC8815A_WDT_MASK, val); 
}

int sc8815a_disable_watchdog_timer(struct sc8815a *sc)
{
	u8 val = SC8815A_WDT_DISABLE << SC8815A_WDT_SHIFT;

	return sc8815a_update_bits(sc, SC8815A_REG_07, 
						SC8815A_WDT_MASK, val);
}

int sc8815a_reset_watchdog_timer(struct sc8815a *sc)
{
	u8 val = SC8815A_WDT_RESET << SC8815A_WDT_RESET_SHIFT;

	return sc8815a_update_bits(sc, SC8815A_REG_03, 
						SC8815A_WDT_RESET_MASK, val);
}

int sc8815a_force_dpdm(struct sc8815a *sc)
{
	int ret;
	u8 val = SC8815A_FORCE_DPDM << SC8815A_FORCE_DPDM_SHIFT;

	ret = sc8815a_update_bits(sc, SC8815A_REG_02, 
						SC8815A_FORCE_DPDM_MASK, val);

	sc->power_good = false;
	pr_info("Force DPDM %s\n", !ret ? "successfully" : "failed");
	
	return ret;

}

int sc8815a_reset_chip(struct sc8815a *sc)
{
	int ret =0;
	u8 val = SC8815A_RESET << SC8815A_RESET_SHIFT;
	return ret;
	ret = sc8815a_update_bits(sc, SC8815A_REG_14, 
						SC8815A_RESET_MASK, val);
	return ret;
}

int sc8815a_enable_hiz_mode(struct sc8815a *sc, bool en)
{
	u8 val;
	return 0;
    if (en) {
        val = SC8815A_HIZ_ENABLE << SC8815A_ENHIZ_SHIFT;
    } else {
        val = SC8815A_HIZ_DISABLE << SC8815A_ENHIZ_SHIFT;
    }

	return sc8815a_update_bits(sc, SC8815A_REG_00, 
						SC8815A_ENHIZ_MASK, val);

}

int sc8815a_exit_hiz_mode(struct sc8815a *sc)
{

	u8 val = SC8815A_HIZ_DISABLE << SC8815A_ENHIZ_SHIFT;

	return sc8815a_update_bits(sc, SC8815A_REG_00, 
						SC8815A_ENHIZ_MASK, val);

}

int sc8815a_get_hiz_mode(struct sc8815a *sc, u8 *state)
{
	u8 val;
	int ret;

	ret = sc8815a_read_byte(sc, SC8815A_REG_00, &val);
	if (ret)
		return ret;
	*state = (val & SC8815A_ENHIZ_MASK) >> SC8815A_ENHIZ_SHIFT;

	return 0;
}

static int sc8815a_enable_term(struct sc8815a *sc, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = SC8815A_TERM_ENABLE << SC8815A_EN_TERM_SHIFT;
	else
		val = SC8815A_TERM_DISABLE << SC8815A_EN_TERM_SHIFT;

	ret = sc8815a_update_bits(sc, SC8815A_REG_07, 
						SC8815A_EN_TERM_MASK, val);

	return ret;
}
/*
0x00=0E  设置VBAT OVP 
0x01 ,0x02 设置VBUS 电压 ，使用IC内部调压
0x05 06    设置IBUS Limit
0x08     设置相关倍率参数
0x09     OTG 使能控制
0x0A    设置VBUS 用内部或者外部调压
0x0D,0x0E  VBUS ADC
0x11,0x12 IBUS ADC
(4 x 49 +3+1) x 2 mV = 400 mV
 400mV x 12.5 = 5V
 5000 000 
 400V x 125X100 = 5000 000V
*/
int sc8815a_set_boost_voltage(struct sc8815a *sc, int volt)
{
	u8 val = 0;
	u8 val_2 = 0;
	val = ((volt/125/100/2)-1)/4 ;
	val_2 = ((volt/125/100/2)-1)%4 ;
	sc8815a_update_bits(sc, SC8815A_REG_01, 
				0xff, val);
/*				
Reg[00] = 0x01
Reg[01] = 0x31
Reg[02] = 0x40
Reg[03] = 0x7c
Reg[04] = 0xc0
Reg[05] = 0xff
Reg[06] = 0xff
Reg[07] = 0x2c
Reg[08] = 0x38
Reg[09] = 0x84
Reg[0a] = 0x01
Reg[0b] = 0x01
Reg[0c] = 0x02
Reg[0d] = 0x00
Reg[0e] = 0x00
Reg[0f] = 0x00
Reg[10] = 0x00
Reg[11] = 0x00
Reg[12] = 0x00
Reg[13] = 0x00
Reg[14] = 0x00
Reg[15] = 0x00
Reg[16] = 0x00
Reg[17] = 0x89
*/
	pr_err("sc8815a_set_boost_voltage volt=%d,val=%d,val_2=%d\n",volt,val,val_2);
	
	sc8815a_update_bits(sc, SC8815A_REG_02, 
				0xff, val_2<<5);
	//msleep(10);
	//gpio_direction_output(sc->pstop_gpio, 0);
	return 0;
}
//05  IBUS_LIM_SET  (255+1)/256 x 3 x 10 mΩ / 10 mΩ = 3 A
int sc8815a_set_boost_current(struct sc8815a *sc, int curr)
{
	//gpio_direction_output(sc->pstop_gpio, 1);
	// u8 temp;
	//(curr /3*256 -1
	pr_err("sc8815a_set_boost_current curr=%d\n",curr);
	sc8815a_update_bits(sc, SC8815A_REG_05, 
				0xff, 0xff);
				
	sc8815a_set_boost_voltage(sc, curr);
	return 0;		
}


static int sc8815a_enable_ico(struct sc8815a* sc, bool enable)
{
	u8 val;
	int ret;

	if (enable)
		val = SC8815A_ICO_ENABLE << SC8815A_ICOEN_SHIFT;
	else
		val = SC8815A_ICO_DISABLE << SC8815A_ICOEN_SHIFT;

	ret = sc8815a_update_bits(sc, SC8815A_REG_02, SC8815A_ICOEN_MASK, val);

	return ret;

}


static int sc8815a_enable_safety_timer(struct sc8815a *sc)
{
	const u8 val = SC8815A_CHG_TIMER_ENABLE << SC8815A_EN_TIMER_SHIFT;

	return sc8815a_update_bits(sc, SC8815A_REG_07, SC8815A_EN_TIMER_MASK,
				   val);
}

static int sc8815a_disable_safety_timer(struct sc8815a *sc)
{
	const u8 val = SC8815A_CHG_TIMER_DISABLE << SC8815A_EN_TIMER_SHIFT;

	return sc8815a_update_bits(sc, SC8815A_REG_07, SC8815A_EN_TIMER_MASK,
				   val);

}

static struct sc8815a_platform_data *sc8815a_parse_dt(struct device_node *np,
						      struct sc8815a *sc)
{
	int ret;
	struct sc8815a_platform_data *pdata;
	pdata = devm_kzalloc(sc->dev, sizeof(struct sc8815a_platform_data),
			     GFP_KERNEL);
	if (!pdata)
		return NULL;

	if (of_property_read_string(np, "charger_name", &sc->chg_dev_name) < 0) {
		sc->chg_dev_name = "boost_chg";
		pr_warn("no charger name\n");
	}

	if (of_property_read_string(np, "eint_name", &sc->eint_name) < 0) {
		sc->eint_name = "chr_stat";
		pr_warn("no eint name\n");
	}

	sc->chg_en_gpio = of_get_named_gpio(np, "ce-gpio", 0);
	if (sc->chg_en_gpio < 0)
		pr_err("sc,chg-en-gpio is not available\n");
	gpio_direction_output(sc->chg_en_gpio, 0);

    sc->irq_gpio = of_get_named_gpio(np, "int-gpio", 0);
	if (sc->irq_gpio < 0)
		pr_err("sc,intr-gpio is not available\n");

	sc->pstop_gpio = of_get_named_gpio(np, "pstop-gpio", 0);
	if (sc->pstop_gpio < 0)
		pr_err("sc,chg-en-gpio is not available\n");
	gpio_direction_output(sc->pstop_gpio, 0);
	
	
	ret = of_property_read_u32(np, "sc,sc8815a,usb-vlim", &pdata->usb.vlim);
	if (ret) {
		pdata->usb.vlim = 4500;
		pr_err("Failed to read node of sc,sc8815a,usb-vlim\n");
	}

	ret = of_property_read_u32(np, "sc,sc8815a,usb-ilim", &pdata->usb.ilim);
	if (ret) {
		pdata->usb.ilim = 2000;
		pr_err("Failed to read node of sc,sc8815a,usb-ilim\n");
	}

	ret = of_property_read_u32(np, "sc,sc8815a,usb-vreg", &pdata->usb.vreg);
	if (ret) {
		pdata->usb.vreg = 4200;
		pr_err("Failed to read node of sc,sc8815a,usb-vreg\n");
	}

	ret = of_property_read_u32(np, "sc,sc8815a,usb-ichg", &pdata->usb.ichg);
	if (ret) {
		pdata->usb.ichg = 2000;
		pr_err("Failed to read node of sc,sc8815a,usb-ichg\n");
	}

	ret = of_property_read_u32(np, "sc,sc8815a,precharge-current",
				   &pdata->iprechg);
	if (ret) {
		pdata->iprechg = 180;
		pr_err("Failed to read node of sc,sc8815a,precharge-current\n");
	}

	ret = of_property_read_u32(np, "sc,sc8815a,termination-current",
				   &pdata->iterm);
	if (ret) {
		pdata->iterm = 180;
		pr_err
		    ("Failed to read node of sc,sc8815a,termination-current\n");
	}

	ret =
	    of_property_read_u32(np, "sc,sc8815a,boost-voltage",
				 &pdata->boostv);
	if (ret) {
		pdata->boostv = 5000;
		pr_err("Failed to read node of sc,sc8815a,boost-voltage\n");
	}

	ret =
	    of_property_read_u32(np, "sc,sc8815a,boost-current",
				 &pdata->boosti);
	if (ret) {
		pdata->boosti = 1200;
		pr_err("Failed to read node of sc,sc8815a,boost-current\n");
	}


	return pdata;
}
#if 0
static int sc8815a_get_charge_stat(struct sc8815a *sc, int *state)
{
    int ret =0;
	u8 val;
	return ret;
	ret = sc8815a_read_byte(sc, SC8815A_REG_0B, &val);
	if (!ret) {
        if ((val & SC8815A_VBUS_STAT_MASK) >> SC8815A_VBUS_STAT_SHIFT 
                == SC8815A_VBUS_TYPE_OTG) {
            *state = POWER_SUPPLY_STATUS_DISCHARGING;
            return ret;
        }
		val = val & SC8815A_CHRG_STAT_MASK;
		val = val >> SC8815A_CHRG_STAT_SHIFT;
		switch (val)
        {
        case SC8815A_CHRG_STAT_IDLE:
			ret = sc8815a_read_byte(sc, SC8815A_REG_11, &val);
			if (val & SC8815A_VBUS_GD_MASK) {
				*state = POWER_SUPPLY_STATUS_CHARGING;
			} else {
				*state = POWER_SUPPLY_STATUS_NOT_CHARGING;
			}
            break;
        case SC8815A_CHRG_STAT_PRECHG:
        case SC8815A_CHRG_STAT_FASTCHG:
            *state = POWER_SUPPLY_STATUS_CHARGING;
            break;
        case SC8815A_CHRG_STAT_CHGDONE:
            *state = POWER_SUPPLY_STATUS_FULL;
            break;
        default:
            *state = POWER_SUPPLY_STATUS_UNKNOWN;
            break;
        }
	}
	pr_err("%s---->%d  %02x\n", __func__, *state, val);

	return ret;
}

static int sc8815a_get_charger_type(struct sc8815a *sc, int *type)
{
	int ret=0;

	u8 reg_val = 0;
	int vbus_stat = 0;
	int chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	return ret;
	ret = sc8815a_read_byte(sc, SC8815A_REG_0B, &reg_val);

	if (ret)
		return ret;

	vbus_stat = (reg_val & SC8815A_VBUS_STAT_MASK);
	vbus_stat >>= SC8815A_VBUS_STAT_SHIFT;

	switch (vbus_stat) {

	case SC8815A_VBUS_TYPE_NONE:
		chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		sc->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	case SC8815A_VBUS_TYPE_SDP:
		chg_type = POWER_SUPPLY_USB_TYPE_SDP;
		sc->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		break;
	case SC8815A_VBUS_TYPE_CDP:
		chg_type = POWER_SUPPLY_USB_TYPE_CDP;
		sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case SC8815A_VBUS_TYPE_DCP:
    case SC8815A_VBUS_TYPE_HVDCP:
		chg_type = POWER_SUPPLY_USB_TYPE_DCP;
		sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	case SC8815A_VBUS_TYPE_UNKNOWN:
		chg_type = POWER_SUPPLY_USB_TYPE_DCP;
		sc->psy_desc.type = POWER_SUPPLY_TYPE_USB;
		break;
	case SC8815A_VBUS_TYPE_NON_STD:
		chg_type = POWER_SUPPLY_USB_TYPE_DCP;
		sc->psy_desc.type = POWER_SUPPLY_TYPE_USB_DCP;
		break;
	default:
		chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		sc->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	}
	
	pr_err("%s ---->0x%02x  %d  %d\n", __func__, reg_val, chg_type, sc->psy_desc.type);

	*type = chg_type;

	return 0;
}
#endif
static void sc8815a_dump_regs(struct sc8815a *sc);
static irqreturn_t sc8815a_irq_handler(int irq, void *data)
{
	int ret =0;
	u8 reg_val;
	bool prev_pg;
	bool prev_vbus_pg;
	struct sc8815a *sc = data;
	pr_err("sc8815a_irq_handler\n");
	return ret;
	ret = sc8815a_read_byte(sc, SC8815A_REG_0B, &reg_val);
	if (ret)
		return IRQ_HANDLED;

	prev_pg = sc->power_good;

	sc->power_good = !!(reg_val & SC8815A_PG_STAT_MASK);

	ret = sc8815a_read_byte(sc, SC8815A_REG_11, &reg_val);
        if (ret)
                return IRQ_HANDLED;

        prev_vbus_pg = sc->vbus_good;

        sc->vbus_good = !!(reg_val & SC8815A_VBUS_GD_MASK);

	if (!prev_vbus_pg && sc->vbus_good){
		sc->input_curr_limit = 3000;
		pr_err("adapter/usb inserted\n");
		sc8815a_adc_start(sc, false);
		//sc8815a_enable_charger(sc);
		//ret = sc8815a_get_charger_type(sc, &sc->psy_usb_type);
	} else if (prev_vbus_pg && !sc->vbus_good) {
        pr_err("adapter/usb removed\n");
		sc8815a_adc_stop(sc);
	//ret = sc8815a_get_charger_type(sc, &sc->psy_usb_type);
	}

	//if (!prev_pg && sc->power_good) {
	//ret = sc8815a_get_charger_type(sc, &sc->psy_usb_type);
	//}
	
	pr_err("%s", __func__);
	sc8815a_dump_regs(sc);

    //power_supply_changed(sc->psy);
/*	
	if (!sc->chg_psy) {
		sc->chg_psy = power_supply_get_by_name("mtk-master-charger");
	}
	
	if (sc->chg_psy) {
		pr_err("--->power_supply changed mtk-master-charger\n");
		power_supply_changed(sc->chg_psy);
	}
*/
	return IRQ_HANDLED;
}

static int sc8815a_register_interrupt(struct sc8815a *sc)
{
	int ret = 0;
	return ret;
    ret = devm_gpio_request(sc->dev, sc->irq_gpio, "chr-irq");
    if (ret < 0) {
        pr_err("failed to request GPIO%d ; ret = %d", sc->irq_gpio, ret);
        return ret;
    }

    ret = gpio_direction_input(sc->irq_gpio);
    if (ret < 0) {
        pr_err("failed to set GPIO%d ; ret = %d", sc->irq_gpio, ret);
        return ret;
    }

    sc->irq = gpio_to_irq(sc->irq_gpio);
    if (ret < 0) {
        pr_err("failed gpio to irq GPIO%d ; ret = %d", sc->irq_gpio, ret);
        return ret;
    }


	ret = devm_request_threaded_irq(sc->dev, sc->irq, NULL,
					sc8815a_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"chr_stat", sc);
	if (ret < 0) {
		pr_err("request thread irq failed:%d\n", ret);
		return ret;
	}else{
		pr_err("request thread irq pass:%d  sc->irq =%d\n", ret, sc->irq);
	}

	enable_irq_wake(sc->irq);

	return 0;
}

static int sc8815a_init_device(struct sc8815a *sc)
{
	int ret =0;
	return ret;
	sc8815a_reset_chip(sc);
	//hjw
	sc8815a_disable_charger(sc);

	sc8815a_set_key(sc);
	sc8815a_update_bits(sc, 0x88, 0x60, 0x60);

	sc8815a_set_key(sc);

	sc->input_curr_limit = 3000;

	sc8815a_disable_watchdog_timer(sc);
    sc8815a_disable_hvdcp(sc);
    sc8815a_enable_ico(sc, false);

	ret = sc8815a_set_prechg_current(sc, sc->platform_data->iprechg);
	if (ret)
		pr_err("Failed to set prechg current, ret = %d\n", ret);

	ret = sc8815a_set_term_current(sc, sc->platform_data->iterm);
	if (ret)
		pr_err("Failed to set termination current, ret = %d\n", ret);

	ret = sc8815a_set_boost_voltage(sc, sc->platform_data->boostv);
	if (ret)
		pr_err("Failed to set boost voltage, ret = %d\n", ret);

	ret = sc8815a_set_boost_current(sc, sc->platform_data->boosti);
	if (ret)
		pr_err("Failed to set boost current, ret = %d\n", ret);

	return 0;
}

static void determine_initial_status(struct sc8815a *sc)
{
	sc8815a_irq_handler(sc->irq, (void *) sc);
}

static int sc8815a_detect_device(struct sc8815a *sc)
{
	int ret;
	u8 data;
	return 0;
	ret = sc8815a_read_byte(sc, SC8815A_REG_14, &data);
	if (!ret) {
		sc->part_no = (data & SC8815A_PN_MASK) >> SC8815A_PN_SHIFT;
		sc->revision =
		    (data & SC8815A_DEV_REV_MASK) >> SC8815A_DEV_REV_SHIFT;
	}

	return ret;
}

static void sc8815a_dump_regs(struct sc8815a *sc)
{
	int addr;
	u8 val;
	int ret;

	for (addr = 0x0; addr <= 0x14; addr++) {
		ret = sc8815a_read_byte(sc, addr, &val);
		if (ret == 0)
			pr_err("Reg[%.2x] = 0x%.2x\n", addr, val);
	}
}

static ssize_t
sc8815a_show_registers(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct sc8815a *sc = dev_get_drvdata(dev);
	u8 addr;
	u8 val;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc8815a Reg");
	for (addr = 0x0; addr <= 0x17; addr++) {
		ret = sc8815a_read_byte(sc, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx,
				       "Reg[%.2x] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t
sc8815a_store_registers(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct sc8815a *sc = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg < 0x14) {
		sc8815a_write_byte(sc, (unsigned char) reg,
				   (unsigned char) val);
	}

	return count;
}

static ssize_t
sc8815a_show_pstop(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	struct sc8815a *sc = dev_get_drvdata(dev);
	return sprintf(buf, "pstop_gpio=%d\n",gpio_get_value(sc->pstop_gpio));
}

static ssize_t
sc8815a_store_pstop(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	struct sc8815a *sc = dev_get_drvdata(dev);
	int ret;
	unsigned int reg;
	//unsigned int val;
	ret = sscanf(buf, "%x", &reg);
	if (reg > 0) {
		gpio_direction_output(sc->pstop_gpio, 1);
	}
	else
	{
		gpio_direction_output(sc->pstop_gpio, 0);
	}
	return count;
}

static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR, sc8815a_show_registers,
		   sc8815a_store_registers);
		   
static DEVICE_ATTR(pstop, S_IRUGO | S_IWUSR, sc8815a_show_pstop,
		   sc8815a_store_pstop);

static struct attribute *sc8815a_attributes[] = {
	&dev_attr_registers.attr,
	&dev_attr_pstop.attr,
	NULL,
};

static const struct attribute_group sc8815a_attr_group = {
	.attrs = sc8815a_attributes,
};

static int sc8815a_charging(struct charger_device *chg_dev, bool enable)
{

	struct sc8815a *sc = dev_get_drvdata(&chg_dev->dev);
	int ret = 0;
	u8 val;
	return ret;
	if (enable)
		ret = sc8815a_enable_charger(sc);
	else
		ret = sc8815a_disable_charger(sc);

	pr_err("%s charger %s\n", enable ? "enable" : "disable",
	       !ret ? "successfully" : "failed");

	ret = sc8815a_read_byte(sc, SC8815A_REG_03, &val);

	if (!ret)
		sc->charge_enabled = !!(val & SC8815A_CHG_CONFIG_MASK);

	return ret;
}

static int sc8815a_plug_in(struct charger_device *chg_dev)
{

	int ret;

	ret = sc8815a_charging(chg_dev, true);

	if (ret)
		pr_err("Failed to enable charging:%d\n", ret);

	return ret;
}

static int sc8815a_plug_out(struct charger_device *chg_dev)
{
	int ret;

	ret = sc8815a_charging(chg_dev, false);

	if (ret)
		pr_err("Failed to disable charging:%d\n", ret);

	return ret;
}

static int sc8815a_dump_register(struct charger_device *chg_dev)
{
	struct sc8815a *sc = dev_get_drvdata(&chg_dev->dev);

	sc8815a_dump_regs(sc);

	return 0;
}

static int sc8815a_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	u8 val;
	int ret;
	struct sc8815a *sc = dev_get_drvdata(&chg_dev->dev);
	ret = sc8815a_read_byte(sc, SC8815A_REG_03, &val);

	if (!ret)
		sc->charge_enabled = !!(val & SC8815A_CHG_CONFIG_MASK);
	
	*en = sc->charge_enabled;
	pr_err("sc8815a_is_charging_enable %d\n",sc->charge_enabled);
	
	return sc->charge_enabled;
}

static int sc8815a_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	struct sc8815a *sc = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 val;

	ret = sc8815a_read_byte(sc, SC8815A_REG_0B, &val);
	if (!ret) {
		val = val & SC8815A_CHRG_STAT_MASK;
		val = val >> SC8815A_CHRG_STAT_SHIFT;
		*done = (val == SC8815A_CHRG_STAT_CHGDONE);
	}

	return ret;
}

static int sc8815a_set_ichg(struct charger_device *chg_dev, u32 curr)
{
	struct sc8815a *sc = dev_get_drvdata(&chg_dev->dev);

	pr_err("charge curr = %d\n", curr);

	return sc8815a_set_chargecurrent(sc, curr / 1000);
}

static int sc8815a_get_ichg(struct charger_device *chg_dev, u32 *curr)
{
	struct sc8815a *sc = dev_get_drvdata(&chg_dev->dev);
	u8 reg_val;
	int ichg;
	int ret;

	ret = sc8815a_read_byte(sc, SC8815A_REG_04, &reg_val);
	if (!ret) {
        ichg = (reg_val & SC8815A_ICHG_MASK) >> SC8815A_ICHG_SHIFT;
        ichg = ichg * SC8815A_ICHG_LSB + SC8815A_ICHG_BASE;
        *curr = ichg * 1000;
	}

	return ret;
}

static int sc8815a_get_min_ichg(struct charger_device *chg_dev, u32 *curr)
{
	*curr = 60 * 1000;

	return 0;
}

static int sc8815a_set_vchg(struct charger_device *chg_dev, u32 volt)
{
	struct sc8815a *sc = dev_get_drvdata(&chg_dev->dev);

	pr_err("charge volt = %d\n", volt);

	return sc8815a_set_chargevolt(sc, volt / 1000);
}

static int sc8815a_get_vchg(struct charger_device *chg_dev, u32 *volt)
{
	struct sc8815a *sc = dev_get_drvdata(&chg_dev->dev);

	return sc8815a_get_chargevol(sc, volt);
}

static int sc8815a_set_ivl(struct charger_device *chg_dev, u32 volt)
{
	struct sc8815a *sc = dev_get_drvdata(&chg_dev->dev);

	pr_err("vindpm volt = %d\n", volt);

	return sc8815a_set_input_volt_limit(sc, volt / 1000);
}

static int sc8815a_get_ivl(struct charger_device *chgdev, u32 *volt)
{
    struct sc8815a *sc = dev_get_drvdata(&chgdev->dev);

    return sc8815a_get_input_volt_limit(sc, volt);
}

static int sc8815a_get_vbus_adc(struct charger_device *chgdev, u32 *vbus)
{
	struct sc8815a *sc = dev_get_drvdata(&chgdev->dev);
	
	return sc8815a_adc_read_vbus_volt(sc, vbus);
}

static int sc8815a_get_ibus_adc(struct charger_device *chgdev, u32 *ibus)
{
	struct sc8815a *sc = dev_get_drvdata(&chgdev->dev);
	
	return sc8815a_adc_read_charge_current(sc, ibus);
}

static int sc8815a_set_icl(struct charger_device *chg_dev, u32 curr)
{
	struct sc8815a *sc = dev_get_drvdata(&chg_dev->dev);

	pr_err("indpm curr = %d\n", curr);

	return sc8815a_set_input_current_limit(sc, curr / 1000);
}

static int sc8815a_get_icl(struct charger_device *chg_dev, u32 *curr)
{
	struct sc8815a *sc = dev_get_drvdata(&chg_dev->dev);
	
    return sc8815a_get_input_current_limit(sc, curr);
}

static int sc8815a_kick_wdt(struct charger_device *chg_dev)
{
	struct sc8815a *sc = dev_get_drvdata(&chg_dev->dev);

	return sc8815a_reset_watchdog_timer(sc);
}

static int sc8815a_set_ieoc(struct charger_device *chgdev, u32 ieoc)
{
    struct sc8815a *sc = dev_get_drvdata(&chgdev->dev);
    
    return sc8815a_set_term_current(sc, ieoc / 1000);
}

static int sc8815a_enable_te(struct charger_device *chgdev, bool en)
{
    struct sc8815a *sc = dev_get_drvdata(&chgdev->dev);

    return sc8815a_enable_term(sc, en);
}

static int sc8815a_enable_hz(struct charger_device *chgdev, bool en)
{
    struct sc8815a *sc = dev_get_drvdata(&chgdev->dev);

    return sc8815a_enable_hiz_mode(sc, en);
}
//09  OTG   BIT7

static int sc8815a_set_vsell(struct sc8815a *sc);
static int sc8815a_set_csel(struct sc8815a *sc);

static int sc8815a_set_otg(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct sc8815a *sc = dev_get_drvdata(&chg_dev->dev);

	if (en) {
	    gpio_direction_output(sc->pstop_gpio, 1);
		sc8815a_set_vsell(sc);
		sc8815a_set_csel(sc);
        ret = sc8815a_enable_otg(sc);
		msleep(10);
		gpio_direction_output(sc->pstop_gpio, 0);
    }
	else {
        ret = sc8815a_disable_otg(sc);
    }

	pr_err("%s OTG %s\n", en ? "enable" : "disable",
	       !ret ? "successfully" : "failed");

	return ret;
}

static int sc8815a_set_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct sc8815a *sc = dev_get_drvdata(&chg_dev->dev);
	int ret;

	if (en)
		ret = sc8815a_enable_safety_timer(sc);
	else
		ret = sc8815a_disable_safety_timer(sc);

	return ret;
}

static int sc8815a_is_safety_timer_enabled(struct charger_device *chg_dev,
					   bool *en)
{
	struct sc8815a *sc = dev_get_drvdata(&chg_dev->dev);
	int ret;
	u8 reg_val;

	ret = sc8815a_read_byte(sc, SC8815A_REG_07, &reg_val);

	if (!ret)
		*en = !!(reg_val & SC8815A_EN_TIMER_MASK);

	return ret;
}

static int sc8815a_set_boost_ilmt(struct charger_device *chg_dev, u32 curr)
{
	struct sc8815a *sc = dev_get_drvdata(&chg_dev->dev);
	int ret;

	pr_err("otg curr = %d\n", curr);

	ret = sc8815a_set_boost_current(sc, curr);

	return ret;
}
#if 0
static enum power_supply_usb_type sc8815a_chg_psy_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_DCP,
};


static enum power_supply_property sc8815a_chg_psy_properties[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static int sc8815a_chg_property_is_writeable(struct power_supply *psy,
					    enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_ENERGY_EMPTY:
		return 1;
	default:
		return 0;
	}
	return 0;
}

static int sc8815a_chg_get_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   union power_supply_propval *val)
{
	int ret = 0;
	u32 _val;
	int data;
	struct sc8815a *sc = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = "SouthChip";
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		//val->intval = sc->power_good;
		val->intval = sc->vbus_good;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		//ret = sc8815a_get_charge_stat(sc, &data);
		if (ret < 0)
			break;
		val->intval = data;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
        val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = sc8815a_get_chargevol(sc, &data);
        if (ret < 0)
            break;
        val->intval = data;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sc8815a_get_input_current_limit(sc, &_val);
        if (ret < 0)
            break;
        val->intval = _val;
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = sc8815a_get_input_volt_limit(sc, &_val);
        if (ret < 0)
            break;
        val->intval = _val;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		ret = sc8815a_get_term_current(sc, &data);
        if (ret < 0)
            break;
        val->intval = data;
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		pr_err("---->POWER_SUPPLY_PROP_USB_TYPE : %d\n", sc->psy_usb_type);
		val->intval = sc->psy_usb_type;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		if (sc->psy_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = 500000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		if (sc->psy_desc.type == POWER_SUPPLY_TYPE_USB)
			val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_TYPE:
		pr_err("---->POWER_SUPPLY_PROP_TYPE : %d\n", sc->psy_desc.type);
		val->intval = sc->psy_desc.type;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int sc8815a_chg_set_property(struct power_supply *psy,
				   enum power_supply_property psp,
				   const union power_supply_propval *val)
{
	int ret = 0;
	struct sc8815a *sc = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		//sc8815a_force_dpdm(sc);
		break;
	case POWER_SUPPLY_PROP_STATUS:
        if (val->intval) {
            //ret = sc8815a_enable_charger(sc);
        } else {
            //ret = sc8815a_disable_charger(sc);
        }
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		//ret = sc8815a_set_chargecurrent(sc, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		//ret = sc8815a_set_chargevolt(sc, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		sc->input_curr_limit = val->intval / 1000;
		//ret = sc8815a_set_input_current_limit(sc, sc->input_curr_limit);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		//ret = sc8815a_set_input_volt_limit(sc, val->intval / 1000);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
        //ret = sc8815a_set_term_current(sc, val->intval / 1000);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static char *sc8815a_psy_supplied_to[] = {
	"mtk-boost-charger",
};

static const struct power_supply_desc sc8815a_psy_desc = {
	.type = POWER_SUPPLY_TYPE_USB,
	.usb_types = sc8815a_chg_psy_usb_types,
	.num_usb_types = ARRAY_SIZE(sc8815a_chg_psy_usb_types),
	.properties = sc8815a_chg_psy_properties,
	.num_properties = ARRAY_SIZE(sc8815a_chg_psy_properties),
	.property_is_writeable = sc8815a_chg_property_is_writeable,
	.get_property = sc8815a_chg_get_property,
	.set_property = sc8815a_chg_set_property,
};

static int sc8815a_chg_init_psy(struct sc8815a *sc)
{
	struct power_supply_config cfg = {
		.drv_data = sc,
		.of_node = sc->dev->of_node,
		.supplied_to = sc8815a_psy_supplied_to,
		.num_supplicants = ARRAY_SIZE(sc8815a_psy_supplied_to),
	};

	memcpy(&sc->psy_desc, &sc8815a_psy_desc, sizeof(sc->psy_desc));
	sc->psy_desc.name = dev_name(sc->dev);//dev_name(sc->dev);
	sc->psy = devm_power_supply_register(sc->dev, &sc->psy_desc,
						&cfg);
	return IS_ERR(sc->psy) ? PTR_ERR(sc->psy) : 0;
}
#endif
static struct charger_ops sc8815a_chg_ops = {
	/* cable plug in/out */
	.plug_in = sc8815a_plug_in,
	.plug_out = sc8815a_plug_out,
    /* enable */
	.enable = sc8815a_charging,
	.is_enabled = sc8815a_is_charging_enable,
    /* enable */
	.enable_chip = sc8815a_charging,
	.is_chip_enabled = sc8815a_is_charging_enable,
    /* charging current */
	.set_charging_current = sc8815a_set_ichg,
   	.get_charging_current = sc8815a_get_ichg,
	.get_min_charging_current = sc8815a_get_min_ichg,
	/* charging voltage */
	.set_constant_voltage = sc8815a_set_vchg,
	.get_constant_voltage = sc8815a_get_vchg,
	/* input current limit */
	.set_input_current = sc8815a_set_icl,
	.get_input_current = sc8815a_get_icl,
	.get_min_input_current = NULL,
	/* MIVR */
	.set_mivr = sc8815a_set_ivl,
	.get_mivr = sc8815a_get_ivl,
	.get_mivr_state = NULL,
	/* ADC */
	.get_adc = NULL,
	.get_vbus_adc = sc8815a_get_vbus_adc,
	.get_ibus_adc = sc8815a_get_ibus_adc,
	.get_ibat_adc = NULL,
	.get_tchg_adc = NULL,
	.get_zcv = NULL,
	/* charing termination */
	.set_eoc_current = sc8815a_set_ieoc,
	.enable_termination = sc8815a_enable_te,
	.reset_eoc_state = NULL,
	.safety_check = NULL,
	.is_charging_done = sc8815a_is_charging_done,
	/* power path */
	.enable_powerpath = NULL,
	.is_powerpath_enabled = NULL,
	/* timer */
	.enable_safety_timer = sc8815a_set_safety_timer,
	.is_safety_timer_enabled = sc8815a_is_safety_timer_enabled,
	.kick_wdt = sc8815a_kick_wdt,
	/* AICL */
	.run_aicl = NULL,
	/* PE+/PE+20 */
	.send_ta_current_pattern = NULL,
	.set_pe20_efficiency_table = NULL,
	.send_ta20_current_pattern = NULL,
	.reset_ta = NULL,
	.enable_cable_drop_comp = NULL,
	/* OTG */
	.set_boost_current_limit = sc8815a_set_boost_ilmt,
	.enable_otg = sc8815a_set_otg,
	.enable_discharge = NULL,
	/* charger type detection */
	.enable_chg_type_det = NULL,
	/* misc */
	.dump_registers = sc8815a_dump_register,
	.enable_hz = sc8815a_enable_hz,
	/* event */
	.event = NULL,
	/* 6pin battery */
//	.enable_6pin_battery_charging = NULL,
};

static struct of_device_id sc8815a_charger_match_table[] = {
	 {
	 .compatible = "sc,sc8815a_charger",
	 .data = &pn_data[PN_SC8815A],
	 },
	{},
};
MODULE_DEVICE_TABLE(of, sc8815a_charger_match_table);

static int sc8815a_set_vsell(struct sc8815a *sc)
{
	int ret = 0;
	u8 data = 0;
	/*
BIT 0 - 2
000: 4.1V                   
001: 4.2V (default)
010: 4.25V
011: 4.3V
100: 4.35V
101: 4.4V
110: 4.45V
111: 4.5V
*/
	ret = sc8815a_read_byte(sc, SC8815A_REG_00, &data);
	if(ret < 0){
		pr_err("gezi----%s--err:%d\n",__func__,ret);
	}
	
	data &= 0xF8;
    data |= 0x05;
	
	pr_err("gezi----%s--data:0x%x\n",__func__,data);
	
	ret = sc8815a_write_byte(sc, SC8815A_REG_00, data);
	if(ret < 0){
		pr_err("gezi----%s--err:%d\n",__func__,ret);
	}
	return ret;
}


static int sc8815a_set_csel(struct sc8815a *sc)
{
	
	int ret = 0;
	u8 data = 0;
	
	
	ret = sc8815a_read_byte(sc, SC8815A_REG_00, &data);
	if(ret < 0){
		pr_err("gezi----%s--err:%d\n",__func__,ret);
	}

/*
BIT 3 - 4
00: 1S battery (default)
01: 2S battery
10: 3S battery
11: 4S battery
*/

	data &= 0xE7;
	data |= 0x08;
	
	pr_err("gezi----%s--data:0x%x\n",__func__,data);

	ret = sc8815a_write_byte(sc, SC8815A_REG_00, data);
	if(ret < 0){
		pr_err("gezi----%s--err:%d\n",__func__,ret);
	}
	return ret;


}

static int sc8815a_charger_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct sc8815a *sc;
	const struct of_device_id *match;
	struct device_node *node = client->dev.of_node;

	int ret = 0;

	sc = devm_kzalloc(&client->dev, sizeof(struct sc8815a), GFP_KERNEL);
	if (!sc)
		return -ENOMEM;

	sc->dev = &client->dev;
	sc->client = client;

	i2c_set_clientdata(client, sc);

	mutex_init(&sc->i2c_rw_lock);

	ret = sc8815a_detect_device(sc);
	if (ret) {
		pr_err("No sc8815a device found!\n");
		return -ENODEV;
	}

	match = of_match_node(sc8815a_charger_match_table, node);
	if (match == NULL) {
		pr_err("device tree match not found\n");
		return -EINVAL;
	}

	sc->platform_data = sc8815a_parse_dt(node, sc);

	if (!sc->platform_data) {
		pr_err("No platform data provided.\n");
		return -EINVAL;
	}
#if 0
    ret = sc8815a_chg_init_psy(sc);
	if (ret < 0) {
		dev_err(sc->dev, "failed to init power supply\n");
		return -EINVAL;
	}
#endif
	ret = sc8815a_init_device(sc);
	if (ret) {
		pr_err("Failed to init device\n");
		return ret;
	}

	sc8815a_register_interrupt(sc);

	sc->chg_dev = charger_device_register(sc->chg_dev_name,
					      &client->dev, sc,
					      &sc8815a_chg_ops,
					      &sc8815a_chg_props);
	if (IS_ERR_OR_NULL(sc->chg_dev)) {
		ret = PTR_ERR(sc->chg_dev);
		return ret;
	}

	ret = sysfs_create_group(&sc->dev->kobj, &sc8815a_attr_group);
	if (ret)
		dev_err(sc->dev, "failed to register sysfs. err: %d\n", ret);

	determine_initial_status(sc);
	

	pr_err("sc8815a probe successfully, Part Num:%d, Revision:%d\n!",
	       sc->part_no, sc->revision);

	return 0;
}

static int sc8815a_charger_remove(struct i2c_client *client)
{
	struct sc8815a *sc = i2c_get_clientdata(client);

	sysfs_remove_group(&sc->dev->kobj, &sc8815a_attr_group);

	return 0;
}

static void sc8815a_charger_shutdown(struct i2c_client *client)
{
	struct sc8815a *sc = i2c_get_clientdata(client);
	
	sc8815a_adc_stop(sc);
}

static struct i2c_driver sc8815a_charger_driver = {
	.driver = {
		   .name = "sc8815a-charger",
		   .owner = THIS_MODULE,
		   .of_match_table = sc8815a_charger_match_table,
		   },

	.probe = sc8815a_charger_probe,
	.remove = sc8815a_charger_remove,
	.shutdown = sc8815a_charger_shutdown,

};

module_i2c_driver(sc8815a_charger_driver);

MODULE_DESCRIPTION("SC SC8815A Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("South Chip");

