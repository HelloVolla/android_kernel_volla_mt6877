/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/gpio/consumer.h>

#include "mtk_charger_intf.h"
#ifdef CONFIG_RT_REGMAP
#include <mt-plat/rt-regmap.h>
#endif /* CONFIG_RT_REGMAP */

#include <mt-plat/v1/mtk_battery.h>
#include "sc8551.h"
#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
#include "../../../misc/mediatek/hardware_info/hardware_info.h"
extern struct hardware_info current_cp_info;
#endif
/*Global define start*/

static const char *SC8551_type_name[SC8551_TYPE_MAX] = {
	"standalone", "slave", "master",
};

static u32 gCpID[2]={0};

static const u32 SC8551_chgdev_notify_map[SC8551_NOTIFY_MAX] = {
	CHARGER_DEV_NOTIFY_IBUSUCP_FALL,
	CHARGER_DEV_NOTIFY_VBUSOVP_ALARM,
	CHARGER_DEV_NOTIFY_VBATOVP_ALARM,
	CHARGER_DEV_NOTIFY_IBUSOCP,
	CHARGER_DEV_NOTIFY_VBUS_OVP,
	CHARGER_DEV_NOTIFY_IBATOCP,
	CHARGER_DEV_NOTIFY_BAT_OVP,
	CHARGER_DEV_NOTIFY_VOUTOVP,
	CHARGER_DEV_NOTIFY_VDROVP,
};

static const u8 SC8551_reg_sf[SC8551_SF_MAX] = {
	SC8551_REG_ACPROTECT,
	SC8551_REG_IBUSOCUCP,
	SC8551_REG_CONVSTAT,
	SC8551_REG_CHGCTRL0,
	SC8551_REG_INTFLAG,
	SC8551_REG_INTSTAT,
	SC8551_REG_FLTFLAG,
	SC8551_REG_FLTSTAT,
	SC8551_REG_REGFLAGMASK,
	SC8551_REG_REGTHRES,
	SC8551_REG_OTHER1,
};

static const struct SC8551_reg_defval SC8551_init_chip_check_reg[] = {
	{
		.reg = SC8551_REG_VBATOVP,
		.value = 0x22,
		.mask = SC8551_VBATOVP_MASK,
	},
	{
		.reg = SC8551_REG_IBATOCP,
		.value = 0x3D,
		.mask = SC8551_IBATOCP_MASK,
	},
	{
		.reg = SC8551_REG_CHGCTRL0,
		.value = 0x00,
		.mask = SC8551_WDTMR_MASK,
	},
};

static const struct SC8551_desc SC8551_desc_defval = {
	.chg_name = "primary_divider_chg",
	.rm_name = "sc8551m",
	.rm_slave_addr = 0x66,
	.vbatovp = 4350000,
	.vbatovp_alm = 4200000,
	.ibatocp = 8100000,
	.ibatocp_alm = 8000000,
	.ibatucp_alm = 2000000,
	.vbusovp = 8900000,
	.vbusovp_alm = 8800000,
	.ibusocp = 4250000,
	.ibusocp_alm = 4000000,
	.vacovp = 11000000,
	.wdt = 500000,
	.ibat_rsense = 0,	/* 2mohm */
	.ibusucpf_deglitch = 0,	/* 10us */
	.vbatovp_dis = false,
	.vbatovp_alm_dis = false,
	.ibatocp_dis = false,
	.ibatocp_alm_dis = false,
	.ibatucp_alm_dis = false,
	.vbusovp_alm_dis = false,
	.ibusocp_dis = false,
	.ibusocp_alm_dis = false,
	.wdt_dis = false,
	.tsbusotp_dis = false,
	.tsbatotp_dis = false,
	.tdieotp_dis = false,
	.reg_en = false,
	.voutovp_dis = false,
};

static const u8 SC8551_adc_reg[SC8551_ADC_MAX] = {
	SC8551_REG_IBUSADC1,
	SC8551_REG_VBUSADC1,
	SC8551_REG_VACADC1,
	SC8551_REG_VOUTADC1,
	SC8551_REG_VBATADC1,
	SC8551_REG_IBATADC1,
	SC8551_REG_TSBUSADC1,
	SC8551_REG_TSBATADC1,
	SC8551_REG_TDIEADC1,
};

static const char *SC8551_adc_name[SC8551_ADC_MAX] = {
	"Ibus", "Vbus", "VAC", "Vout", "Vbat", "Ibat", "TSBus", "TSBat", "TDie",
};

static const u32 SC8551_adc_accuracy_tbl[SC8551_ADC_MAX] = {
	150000,	/* IBUS */
	35000,	/* VBUS */
	35000,	/* VAC */
	20000,	/* VOUT */
	20000,	/* VBAT */
	200000,	/* IBAT */
	1,	/* TSBUS */
	1,	/* TSBAT */
	4,	/* TDIE */
};
static int __SC8551_update_status(struct SC8551_chip *chip);
static int __SC8551_init_chip(struct SC8551_chip *chip);

static const u8 SC8551_hm_password[2] = {0x69, 0x96};
static const u32 SC8551_wdt[] = {
	500000, 1000000, 5000000, 3000000,
};
/*Global define end*/

static int SC8551_read_device(void *client, u32 addr, int len, void *dst)
{
	int ret;
	struct i2c_client *i2c = (struct i2c_client *)client;
	struct SC8551_chip *chip = i2c_get_clientdata(i2c);

	pm_stay_awake(chip->dev);
	mutex_lock(&chip->suspend_lock);
	ret = i2c_smbus_read_i2c_block_data(i2c, addr, len, dst);
	mutex_unlock(&chip->suspend_lock);
	pm_relax(chip->dev);
	return ret;
}

static int SC8551_write_device(void *client, u32 addr, int len, const void *src)
{
	int ret;
	struct i2c_client *i2c = (struct i2c_client *)client;
	struct SC8551_chip *chip = i2c_get_clientdata(i2c);

	pm_stay_awake(chip->dev);
	mutex_lock(&chip->suspend_lock);
	ret = i2c_smbus_write_i2c_block_data(i2c, addr, len, src);
	mutex_unlock(&chip->suspend_lock);
	pm_relax(chip->dev);
	return ret;
}

#ifdef CONFIG_RT_REGMAP
RT_REG_DECL(SC8551_REG_VBATOVP, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_VBATOVP_ALM, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_IBATOCP, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_IBATOCP_ALM, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_IBATUCP_ALM, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_ACPROTECT, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_VBUSOVP, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_VBUSOVP_ALM, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_IBUSOCUCP, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_IBUSOCP_ALM, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_CONVSTAT, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_CHGCTRL0, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_CHGCTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_INTSTAT, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_INTFLAG, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_INTMASK, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_FLTSTAT, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_FLTFLAG, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_FLTMASK, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_DEVINFO, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_ADCCTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_ADCEN, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_IBUSADC1, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_IBUSADC0, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_VBUSADC1, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_VBUSADC0, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_VACADC1, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_VACADC0, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_VOUTADC1, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_VOUTADC0, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_VBATADC1, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_VBATADC0, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_IBATADC1, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_IBATADC0, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_TSBUSADC1, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_TSBUSADC0, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_TSBATADC1, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_TSBATADC0, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_TDIEADC1, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_TDIEADC0, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_TSBUSOTP, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_TSBATOTP, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_TDIEALM, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_REGCTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_REGTHRES, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_REGFLAGMASK, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_BUSDEGLH, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_OTHER1, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_SYSCTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_PASSWORD0, 1, RT_VOLATILE, {});
RT_REG_DECL(SC8551_REG_PASSWORD1, 1, RT_VOLATILE, {});

static const rt_register_map_t SC8551_regmap[] = {
	RT_REG(SC8551_REG_VBATOVP),
	RT_REG(SC8551_REG_VBATOVP_ALM),
	RT_REG(SC8551_REG_IBATOCP),
	RT_REG(SC8551_REG_IBATOCP_ALM),
	RT_REG(SC8551_REG_IBATUCP_ALM),
	RT_REG(SC8551_REG_ACPROTECT),
	RT_REG(SC8551_REG_VBUSOVP),
	RT_REG(SC8551_REG_VBUSOVP_ALM),
	RT_REG(SC8551_REG_IBUSOCUCP),
	RT_REG(SC8551_REG_IBUSOCP_ALM),
	RT_REG(SC8551_REG_CONVSTAT),
	RT_REG(SC8551_REG_CHGCTRL0),
	RT_REG(SC8551_REG_CHGCTRL1),
	RT_REG(SC8551_REG_INTSTAT),
	RT_REG(SC8551_REG_INTFLAG),
	RT_REG(SC8551_REG_INTMASK),
	RT_REG(SC8551_REG_FLTSTAT),
	RT_REG(SC8551_REG_FLTFLAG),
	RT_REG(SC8551_REG_FLTMASK),
	RT_REG(SC8551_REG_DEVINFO),
	RT_REG(SC8551_REG_ADCCTRL),
	RT_REG(SC8551_REG_ADCEN),
	RT_REG(SC8551_REG_IBUSADC1),
	RT_REG(SC8551_REG_IBUSADC0),
	RT_REG(SC8551_REG_VBUSADC1),
	RT_REG(SC8551_REG_VBUSADC0),
	RT_REG(SC8551_REG_VACADC1),
	RT_REG(SC8551_REG_VACADC0),
	RT_REG(SC8551_REG_VOUTADC1),
	RT_REG(SC8551_REG_VOUTADC0),
	RT_REG(SC8551_REG_VBATADC1),
	RT_REG(SC8551_REG_VBATADC0),
	RT_REG(SC8551_REG_IBATADC1),
	RT_REG(SC8551_REG_IBATADC0),
	RT_REG(SC8551_REG_TSBUSADC1),
	RT_REG(SC8551_REG_TSBUSADC0),
	RT_REG(SC8551_REG_TSBATADC1),
	RT_REG(SC8551_REG_TSBATADC0),
	RT_REG(SC8551_REG_TDIEADC1),
	RT_REG(SC8551_REG_TDIEADC0),
	RT_REG(SC8551_REG_TSBUSOTP),
	RT_REG(SC8551_REG_TSBATOTP),
	RT_REG(SC8551_REG_TDIEALM),
	RT_REG(SC8551_REG_REGCTRL),
	RT_REG(SC8551_REG_REGTHRES),
	RT_REG(SC8551_REG_REGFLAGMASK),
	RT_REG(SC8551_REG_BUSDEGLH),
	RT_REG(SC8551_REG_OTHER1),
	RT_REG(SC8551_REG_SYSCTRL1),
	RT_REG(SC8551_REG_PASSWORD0),
	RT_REG(SC8551_REG_PASSWORD1),
};

static struct rt_regmap_fops SC8551_rm_fops = {
	.read_device = SC8551_read_device,
	.write_device = SC8551_write_device,
};

static int SC8551_register_regmap(struct SC8551_chip *chip)
{
	struct i2c_client *client = chip->client;
	struct rt_regmap_properties *prop = NULL;

	CP_INFO("[%s]Enter\n",client->name);

	prop = devm_kzalloc(&client->dev, sizeof(*prop), GFP_KERNEL);
	if (!prop)
		return -ENOMEM;

	prop->name = chip->desc->rm_name;
	prop->aliases = chip->desc->rm_name;
	prop->register_num = ARRAY_SIZE(SC8551_regmap);
	prop->rm = SC8551_regmap;
	prop->rt_regmap_mode = RT_SINGLE_BYTE | RT_CACHE_DISABLE |
			       RT_IO_PASS_THROUGH;
	prop->io_log_en = 0;

	chip->rm_prop = prop;
	chip->rm_dev = rt_regmap_device_register_ex(chip->rm_prop,
						    &SC8551_rm_fops, chip->dev,
						    client,
						    chip->desc->rm_slave_addr,
						    chip);
	if (!chip->rm_dev) {
		CP_ERR("[%s] register regmap dev fail\n",chip->client->name);
		return -EINVAL;
	}

	return 0;
}
#endif /* CONFIG_RT_REGMAP */

static inline int __SC8551_i2c_write8(struct SC8551_chip *chip, u8 reg, u8 data)
{
	int ret, retry = 0;

	do {
#ifdef CONFIG_RT_REGMAP
		ret = rt_regmap_block_write(chip->rm_dev, reg, 1, &data);
#else
		ret = SC8551_write_device(chip->client, reg, 1, &data);
#endif /* CONFIG_RT_REGMAP */
		retry++;
		if (ret < 0)
			usleep_range(10, 15);
	} while (ret < 0 && retry < I2C_ACCESS_MAX_RETRY);

	if (ret < 0) {
		CP_ERR("[%s]I2CW[0x%02X] = 0x%02X fail\n",chip->client->name,reg, data);
		return ret;
	}
	dev_dbg_ratelimited(chip->dev, "%s I2CW[0x%02X] = 0x%02X\n", __func__,
			reg, data);
	return 0;
}

static int SC8551_i2c_write8(struct SC8551_chip *chip, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&chip->io_lock);
	ret = __SC8551_i2c_write8(chip, reg, data);
	mutex_unlock(&chip->io_lock);

	return ret;
}

static inline int __SC8551_i2c_read8(struct SC8551_chip *chip, u8 reg, u8 *data)
{
	int ret, retry = 0;

	do {
#ifdef CONFIG_RT_REGMAP
		ret = rt_regmap_block_read(chip->rm_dev, reg, 1, data);
#else
		ret = SC8551_read_device(chip->client, reg, 1, data);
#endif /* CONFIG_RT_REGMAP */
		retry++;
		if (ret < 0)
			usleep_range(10, 15);
	} while (ret < 0 && retry < I2C_ACCESS_MAX_RETRY);

	if (ret < 0) {
		CP_ERR("[%s]I2CR[0x%02X] fail\n",chip->client->name,reg);
		return ret;
	}
	dev_dbg_ratelimited(chip->dev, "%s I2CR[0x%02X] = 0x%02X\n", __func__,
			reg, *data);
	return 0;
}

static int SC8551_i2c_read8(struct SC8551_chip *chip, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&chip->io_lock);
	ret = __SC8551_i2c_read8(chip, reg, data);
	mutex_unlock(&chip->io_lock);

	return ret;
}

static inline int __SC8551_i2c_write_block(struct SC8551_chip *chip, u8 reg,
					   u32 len, const u8 *data)
{
	int ret;

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_write(chip->rm_dev, reg, len, data);
#else
	ret = SC8551_write_device(chip->client, reg, len, data);
#endif /* CONFIG_RT_REGMAP */

	return ret;
}

static int SC8551_i2c_write_block(struct SC8551_chip *chip, u8 reg, u32 len,
				  const u8 *data)
{
	int ret;

	mutex_lock(&chip->io_lock);
	ret = __SC8551_i2c_write_block(chip, reg, len, data);
	mutex_unlock(&chip->io_lock);

	return ret;
}

static inline int __SC8551_i2c_read_block(struct SC8551_chip *chip, u8 reg,
					  u32 len, u8 *data)
{
	int ret;

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_read(chip->rm_dev, reg, len, data);
#else
	ret = SC8551_read_device(chip->client, reg, len, data);
#endif /* CONFIG_RT_REGMAP */

	return ret;
}

static int SC8551_i2c_read_block(struct SC8551_chip *chip, u8 reg, u32 len,
				 u8 *data)
{
	int ret;

	mutex_lock(&chip->io_lock);
	ret = __SC8551_i2c_read_block(chip, reg, len, data);
	mutex_unlock(&chip->io_lock);

	return ret;
}

static int SC8551_i2c_test_bit(struct SC8551_chip *chip, u8 reg, u8 shft,
			       bool *one)
{
	int ret;
	u8 data;

	ret = SC8551_i2c_read8(chip, reg, &data);
	if (ret < 0) {
		*one = false;
		return ret;
	}

	*one = (data & (1 << shft)) ? true : false;
	return 0;
}

static int SC8551_i2c_update_bits(struct SC8551_chip *chip, u8 reg, u8 data,
				  u8 mask)
{
	int ret;
	u8 _data;

	mutex_lock(&chip->io_lock);
	ret = __SC8551_i2c_read8(chip, reg, &_data);
	if (ret < 0)
		goto out;
	_data &= ~mask;
	_data |= (data & mask);
	ret = __SC8551_i2c_write8(chip, reg, _data);
out:
	mutex_unlock(&chip->io_lock);
	return ret;
}

static inline int SC8551_set_bits(struct SC8551_chip *chip, u8 reg, u8 mask)
{
	return SC8551_i2c_update_bits(chip, reg, mask, mask);
}

static inline int SC8551_clr_bits(struct SC8551_chip *chip, u8 reg, u8 mask)
{
	return SC8551_i2c_update_bits(chip, reg, 0x00, mask);
}

static inline u8 SC8551_val_toreg(u32 min, u32 max, u32 step, u32 target,
				  bool ru)
{
	if (target <= min)
		return 0;

	if (target >= max)
		return (max - min) / step;

	if (ru)
		return (target - min + step) / step;
	return (target - min) / step;
}

static inline u8 SC8551_val_toreg_via_tbl(const u32 *tbl, int tbl_size,
					  u32 target)
{
	int i;

	if (target < tbl[0])
		return 0;

	for (i = 0; i < tbl_size - 1; i++) {
		if (target >= tbl[i] && target < tbl[i + 1])
			return i;
	}

	return tbl_size - 1;
}

static u8 SC8551_vbatovp_toreg(u32 uV)
{
	return SC8551_val_toreg(3500000, 5075000, 25000, uV, false);
}

static u8 SC8551_ibatocp_toreg(u32 uA)
{
	return SC8551_val_toreg(2000000, 10000000, 100000, uA, true);
}

static u8 SC8551_ibatucp_toreg(u32 uA)
{
	return SC8551_val_toreg(0, 6350000, 50000, uA, false);
}

static u8 SC8551_vbusovp_toreg(u32 uV)
{
	return SC8551_val_toreg(6000000, 12350000, 50000, uV, true);
}

static u8 SC8551_ibusocp_toreg(u32 uA)
{
	return SC8551_val_toreg(1000000, 4750000, 250000, uA, true);
}

static u8 SC8551_ibusocp_alm_toreg(u32 uA)
{
	return SC8551_val_toreg(0, 6350000, 50000, uA, true);
}


static u8 SC8551_wdt_toreg(u32 uS)
{
	return SC8551_val_toreg_via_tbl(SC8551_wdt, ARRAY_SIZE(SC8551_wdt), uS);
}

static u8 SC8551_vacovp_toreg(u32 uV)
{
	if (uV < 11000000)
		return 0x07;
	return SC8551_val_toreg(11000000, 17000000, 1000000, uV, true);
}


static int __maybe_unused __SC8551_enter_hidden_mode(struct SC8551_chip *chip,
						     bool en)
{
	int ret = 0;

	mutex_lock(&chip->hm_lock);

	if (en) {
		if (chip->hm_cnt == 0) {
			ret = SC8551_i2c_write_block(chip, SC8551_REG_PASSWORD0,
						     2, SC8551_hm_password);
			if (ret < 0)
				goto err;
		}
		chip->hm_cnt++;
	} else {
		if (chip->hm_cnt == 1) /* last one */
			ret = SC8551_i2c_write8(chip, SC8551_REG_PASSWORD0,
						0x00);
		if (chip->hm_cnt > 0)
			chip->hm_cnt--;
		if (ret < 0)
			goto err;
	}
	CP_INFO("[%s]en = %d\n",chip->client->name,en);
	goto out;

err:
	CP_INFO("[%s]en = %d,failed :%d\n",chip->client->name,en,ret);
out:
	mutex_unlock(&chip->hm_lock);
	return ret;
}

/* Must be called while holding a lock */
static int SC8551_enable_wdt(struct SC8551_chip *chip, bool en)
{
	int ret;

	if (chip->wdt_en == en)
		return 0;
	ret = (en ? SC8551_clr_bits : SC8551_set_bits)
		(chip, SC8551_REG_CHGCTRL0, SC8551_WDTEN_MASK);
	if (ret < 0)
		return ret;
	chip->wdt_en = en;
	return 0;
}

static int __SC8551_get_adc(struct SC8551_chip *chip,
			    enum SC8551_adc_channel chan, int *val)
{
	int ret;
	u8 data[2];

	ret = SC8551_set_bits(chip, SC8551_REG_ADCCTRL, SC8551_ADCEN_MASK);
	if (ret < 0)
		goto out;
	usleep_range(12000, 15000);
	ret = SC8551_i2c_read_block(chip, SC8551_adc_reg[chan], 2, data);
	if (ret < 0)
		goto out_dis;
	switch (chan) {
	case SC8551_ADC_IBUS:
		if (chip->is_sc8551)
			*val = ((data[0] << 8) + data[1]) * 15625 / 10;
		else
			*val = ((data[0] << 8) + data[1]) * 1000;
		break;
	case SC8551_ADC_VBUS:
		if (chip->is_sc8551)
			*val = ((data[0] << 8) + data[1]) * 3750;
		else
			*val = ((data[0] << 8) + data[1]) * 1000;
		break;
	case SC8551_ADC_VAC:
		if (chip->is_sc8551)
			*val = ((data[0] << 8) + data[1]) * 5000;
		else
			*val = ((data[0] << 8) + data[1]) * 1000;
		break;
	case SC8551_ADC_VOUT:
		if (chip->is_sc8551)
			*val = ((data[0] << 8) + data[1]) * 1250;
		else
			*val = ((data[0] << 8) + data[1]) * 1000;
		break;
	case SC8551_ADC_VBAT:
	/*prize add by lvyuanchuan for master-slave charger switching ,20221123 start*/
#if 0
		*val = battery_get_bat_voltage()*1000;
#else		
		if (chip->is_sc8551)
			*val = ((data[0] << 8) + data[1]) * 1250;
		else
			*val = ((data[0] << 8) + data[1]) * 1000;
#endif			
	/*prize add by lvyuanchuan for master-slave charger switching ,20221123 end*/
		break;
	case SC8551_ADC_IBAT:
#if 0
		if (chip->is_sc8551)
			*val = ((data[0] << 8) + data[1]) * 3125;
		else
			*val = ((data[0] << 8) + data[1]) * 1000;
		CP_INFO("[%s][%s] ADC_IBAT[%d]\n",chip->client->name,SC8551_adc_name[chan], *val);
#endif
		*val = battery_get_bat_current()*1000;
		break;
	case SC8551_ADC_TDIE:
		*val = (data[0] << 7) + (data[1] >> 1);
		break;
	case SC8551_ADC_TSBAT:
	case SC8551_ADC_TSBUS:
	default:
		ret = -ENOTSUPP;
		break;
	}
	if (ret < 0)
		CP_ERR("[%s][%s} fail(%d)\n",chip->client->name,SC8551_adc_name[chan], ret);
	else
		CP_INFO("[%s] adc_name[%d]:[%s] = [%d]\n",chip->client->name,chan,SC8551_adc_name[chan], *val);
out_dis:
	if (!chip->force_adc_en && !chip->is_sc8551)
		ret = SC8551_clr_bits(chip, SC8551_REG_ADCCTRL,
				      SC8551_ADCEN_MASK);
out:
	return ret;
}

static int SC8551_enable_chg(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct SC8551_chip *chip = charger_get_data(chg_dev);
	u32 err_check = BIT(SC8551_IRQIDX_VBUSOVP) |
			BIT(SC8551_IRQIDX_VACOVP) |
			BIT(SC8551_IRQIDX_VDROVP) |
			BIT(SC8551_IRQIDX_VBUSOVP) |
			BIT(SC8551_IRQIDX_TDIEOTP) |
			BIT(SC8551_IRQIDX_VBUSLERR) |
			BIT(SC8551_IRQIDX_VBUSHERR) |
			BIT(SC8551_IRQIDX_VOUTOVP) |
			BIT(SC8551_IRQIDX_TSBUSOTP) |
			BIT(SC8551_IRQIDX_TSBATOTP);
	u32 stat_check = BIT(SC8551_IRQIDX_VACINSERT) |
			 BIT(SC8551_IRQIDX_VOUTINSERT);

	CP_INFO("[%s]en = %d\n",chip->client->name,en);
	mutex_lock(&chip->adc_lock);
	chip->force_adc_en = en;
	if (chip->is_sc8551)
		chip->force_adc_en = true;

	if (!en) {
		ret = SC8551_clr_bits(chip, SC8551_REG_CHGCTRL1,
				      SC8551_CHGEN_MASK);
		if (ret < 0)
			goto out_unlock;
		if (!chip->is_sc8551) {
			ret = SC8551_clr_bits(chip, SC8551_REG_ADCCTRL,
				      SC8551_ADCEN_MASK);
			if (ret < 0)
				goto out_unlock;
		}

		ret = SC8551_enable_wdt(chip, false);
		goto out_unlock;
	}
	/* Enable ADC to check status before enable charging */
	ret = SC8551_set_bits(chip, SC8551_REG_ADCCTRL, SC8551_ADCEN_MASK);
	if (ret < 0)
		goto out_unlock;
	mutex_unlock(&chip->adc_lock);
	usleep_range(12000, 15000);

	mutex_lock(&chip->stat_lock);
	__SC8551_update_status(chip);
	if ((chip->stat & err_check) ||
	    ((chip->stat & stat_check) != stat_check)) {
		CP_INFO("[%s] error(0x%08X,0x%08X,0x%08X)\n",chip->client->name,chip->stat, err_check, stat_check);
		ret = -EINVAL;
		mutex_unlock(&chip->stat_lock);
		goto out;
	}
	mutex_unlock(&chip->stat_lock);
	if (!chip->desc->wdt_dis) {
		ret = SC8551_enable_wdt(chip, true);
		if (ret < 0)
			goto out;
	}
	ret = SC8551_set_bits(chip, SC8551_REG_CHGCTRL1, SC8551_CHGEN_MASK);

	goto out;
out_unlock:
	mutex_unlock(&chip->adc_lock);
out:
	return ret;
}

static int SC8551_is_chg_enabled(struct charger_device *chg_dev, bool *en)
{
	int ret;
	struct SC8551_chip *chip = charger_get_data(chg_dev);

	ret = SC8551_i2c_test_bit(chip, SC8551_REG_CHGCTRL1, SC8551_CHGEN_SHFT,en);
	CP_INFO("[%s] EN %d, ret(%d)\n",chip->client->name,*en, ret);
	return ret;
}

static inline enum SC8551_adc_channel to_SC8551_adc(enum adc_channel chan)
{
	switch (chan) {
	case ADC_CHANNEL_VBUS:
		return SC8551_ADC_VBUS;
	case ADC_CHANNEL_VBAT:
		return SC8551_ADC_VBAT;
	case ADC_CHANNEL_IBUS:
		return SC8551_ADC_IBUS;
	case ADC_CHANNEL_IBAT:
		return SC8551_ADC_IBAT;
	case ADC_CHANNEL_TEMP_JC:
		return SC8551_ADC_TDIE;
	case ADC_CHANNEL_VOUT:
		return SC8551_ADC_VOUT;
	default:
		break;
	}
	return SC8551_ADC_NOTSUPP;
}

static int SC8551_get_adc(struct charger_device *chg_dev, enum adc_channel chan,
			  int *min, int *max)
{
	int ret;
	struct SC8551_chip *chip = charger_get_data(chg_dev);
	enum SC8551_adc_channel _chan = to_SC8551_adc(chan);

	if (_chan == SC8551_ADC_NOTSUPP)
		return -EINVAL;
	mutex_lock(&chip->adc_lock);
	ret = __SC8551_get_adc(chip, _chan, max);
	if (ret < 0)
		goto out;
	if (min != max)
		*min = *max;
out:
	mutex_unlock(&chip->adc_lock);
	return ret;
}

static int SC8551_get_adc_accuracy(struct charger_device *chg_dev,
				   enum adc_channel chan, int *min, int *max)
{
	enum SC8551_adc_channel _chan = to_SC8551_adc(chan);

	if (_chan == SC8551_ADC_NOTSUPP)
		return -EINVAL;
	*min = *max = SC8551_adc_accuracy_tbl[_chan];
	return 0;
}

static int SC8551_set_vbusovp(struct charger_device *chg_dev, u32 uV)
{
	struct SC8551_chip *chip = charger_get_data(chg_dev);
	u8 reg = SC8551_vbusovp_toreg(uV);

	CP_INFO("[%s] uv %d => reg(0x%02X)\n",chip->client->name,uV, reg);
	return SC8551_i2c_update_bits(chip, SC8551_REG_VBUSOVP, reg,
				      SC8551_VBUSOVP_MASK);
}

static int SC8551_set_ibusocp(struct charger_device *chg_dev, u32 uA)
{
	struct SC8551_chip *chip = charger_get_data(chg_dev);
	u8 reg = SC8551_ibusocp_toreg(uA);

	CP_INFO("[%s] uA %d => reg(0x%02X)\n",chip->client->name, uA, reg);
	return SC8551_i2c_update_bits(chip, SC8551_REG_IBUSOCUCP, reg,
				      SC8551_IBUSOCP_MASK);
}

static int SC8551_set_vbatovp(struct charger_device *chg_dev, u32 uV)
{
	struct SC8551_chip *chip = charger_get_data(chg_dev);
	u8 reg = SC8551_vbatovp_toreg(uV);

	CP_INFO("[%s] uv %d => reg(0x%02X)\n",chip->client->name, uV, reg);
	return SC8551_i2c_update_bits(chip, SC8551_REG_VBATOVP, reg,
				      SC8551_VBATOVP_MASK);
}

static int SC8551_set_vbatovp_alarm(struct charger_device *chg_dev, u32 uV)
{
	struct SC8551_chip *chip = charger_get_data(chg_dev);
	u8 reg = SC8551_vbatovp_toreg(uV);

	CP_INFO("[%s] uV %d\n",chip->client->name,uV);
	return SC8551_i2c_update_bits(chip, SC8551_REG_VBATOVP_ALM, reg,
				      SC8551_VBATOVP_ALM_MASK);
}

static int SC8551_reset_vbatovp_alarm(struct charger_device *chg_dev)
{
	int ret;
	struct SC8551_chip *chip = charger_get_data(chg_dev);
	u8 data;

	CP_INFO("[%s]enter\n",chip->client->name);
	mutex_lock(&chip->io_lock);
	ret = __SC8551_i2c_read8(chip, SC8551_REG_VBATOVP_ALM, &data);
	if (ret < 0)
		goto out;
	data |= SC8551_VBATOVP_ALMDIS_MASK;
	ret = __SC8551_i2c_write8(chip, SC8551_REG_VBATOVP_ALM, data);
	if (ret < 0)
		goto out;
	data &= ~SC8551_VBATOVP_ALMDIS_MASK;
	ret = __SC8551_i2c_write8(chip, SC8551_REG_VBATOVP_ALM, data);
out:
	mutex_unlock(&chip->io_lock);
	return ret;
}

static int SC8551_set_vbusovp_alarm(struct charger_device *chg_dev, u32 uV)
{
	struct SC8551_chip *chip = charger_get_data(chg_dev);
	u8 reg = SC8551_vbusovp_toreg(uV);

	CP_INFO("[%s] uV %d\n",chip->client->name,uV);
	return SC8551_i2c_update_bits(chip, SC8551_REG_VBUSOVP_ALM, reg,
				      SC8551_VBUSOVP_ALM_MASK);
}

static int SC8551_is_vbuslowerr(struct charger_device *chg_dev, bool *err)
{
	int ret;
	struct SC8551_chip *chip = charger_get_data(chg_dev);
	CP_INFO("[%s]enter\n",chip->client->name);
	mutex_lock(&chip->adc_lock);
	ret = SC8551_set_bits(chip, SC8551_REG_ADCCTRL, SC8551_ADCEN_MASK);
	if (ret < 0)
		goto out;
	usleep_range(12000, 15000);
	ret = SC8551_i2c_test_bit(chip, SC8551_REG_OTHER1,
				  SC8551_VBUSLOWERR_FLAG_SHFT, err);
	if (ret < 0 || *err)
		goto out_dis;
	ret = SC8551_i2c_test_bit(chip, SC8551_REG_CONVSTAT,
				  SC8551_VBUSLOWERR_STAT_SHFT, err);
out_dis:
	if (!chip->force_adc_en && !chip->is_sc8551)
		SC8551_clr_bits(chip, SC8551_REG_ADCCTRL, SC8551_ADCEN_MASK);
out:
	mutex_unlock(&chip->adc_lock);
	return ret;
}

static int SC8551_reset_vbusovp_alarm(struct charger_device *chg_dev)
{
	int ret;
	struct SC8551_chip *chip = charger_get_data(chg_dev);
	u8 data;

	CP_INFO("[%s]enter\n",chip->client->name);
	mutex_lock(&chip->io_lock);
	ret = __SC8551_i2c_read8(chip, SC8551_REG_VBUSOVP_ALM, &data);
	if (ret < 0)
		goto out;
	data |= SC8551_VBUSOVP_ALMDIS_MASK;
	ret = __SC8551_i2c_write8(chip, SC8551_REG_VBUSOVP_ALM, data);
	if (ret < 0)
		goto out;
	data &= ~SC8551_VBUSOVP_ALMDIS_MASK;
	ret = __SC8551_i2c_write8(chip, SC8551_REG_VBUSOVP_ALM, data);
out:
	mutex_unlock(&chip->io_lock);
	return ret;
}

static int SC8551_set_ibatocp(struct charger_device *chg_dev, u32 uA)
{
	struct SC8551_chip *chip = charger_get_data(chg_dev);
	u8 reg = SC8551_ibatocp_toreg(uA);

	CP_INFO("[%s] uA:%d(0x%02X)\n",chip->client->name,uA, reg);
	return SC8551_i2c_update_bits(chip, SC8551_REG_IBATOCP, reg,
				      SC8551_IBATOCP_MASK);
}

static int SC8551_init_chip(struct charger_device *chg_dev)
{
	int i, ret;
	struct SC8551_chip *chip = charger_get_data(chg_dev);
	const struct SC8551_reg_defval *reg_defval;
	u8 val;

	for (i = 0; i < ARRAY_SIZE(SC8551_init_chip_check_reg); i++) {
		reg_defval = &SC8551_init_chip_check_reg[i];
		ret = SC8551_i2c_read8(chip, reg_defval->reg, &val);
		if (ret < 0)
			return ret;
		if ((val & reg_defval->mask) == reg_defval->value) {
			CP_INFO("[%s]chip reset happened, reinit\n",chip->client->name);
			return __SC8551_init_chip(chip);
		}
	}
	return 0;
}

static int SC8551_vacovp_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	return 0;
}

static inline void SC8551_set_notify(struct SC8551_chip *chip,
				     enum SC8551_notify notify)
{
	mutex_lock(&chip->notify_lock);
	chip->notify |= BIT(notify);
	mutex_unlock(&chip->notify_lock);
}
/*

*/
static int SC8551_ibusucpf_irq_handler(struct SC8551_chip *chip)
{
	bool ucpf = !!(chip->stat & BIT(SC8551_IRQIDX_IBUSUCPF));

	CP_INFO("[%s]IBUSUCPF %d\n",chip->client->name,ucpf);
	if (ucpf)
		SC8551_set_notify(chip, SC8551_NOTIFY_IBUSUCPF);
	return 0;
}
/*

*/
static int SC8551_ibusucpr_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]IBUSUCPR %d\n",chip->client->name,!!(chip->stat & BIT(SC8551_IRQIDX_IBUSUCPR)));
	return 0;
}
/*

*/
static int SC8551_cflydiag_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	return 0;
}
/*

*/
static int SC8551_conocp_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	return 0;
}
/*

*/
static int SC8551_switching_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]SWITCHING %d\n",chip->client->name,!!(chip->stat & BIT(SC8551_IRQIDX_SWITCHING)));
	return 0;
}
/*

*/
static int SC8551_ibusucptout_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	return 0;
}
/*

*/
static int SC8551_vbushigherr_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]VBUSHERR %d\n",chip->client->name,!!(chip->stat & BIT(SC8551_IRQIDX_VBUSHERR)));
	return 0;
}
/*

*/
static int SC8551_vbuslowerr_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]VBUSLERR %d\n",chip->client->name,!!(chip->stat & BIT(SC8551_IRQIDX_VBUSLERR)));
	return 0;
}
/*

*/
static int SC8551_tdieotp_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	return 0;
}
/*

*/
static int SC8551_wdt_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	return 0;
}
/*

*/
static int SC8551_adcdone_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	return 0;
}
/*

*/
static int SC8551_voutinsert_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]VOUTINSERT %d\n",chip->client->name,!!(chip->stat & BIT(SC8551_IRQIDX_VOUTINSERT)));
	return 0;
}
/*

*/
static int SC8551_vacinsert_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]VACINSERT %d\n",chip->client->name,!!(chip->stat & BIT(SC8551_IRQIDX_VACINSERT)));
	return 0;
}
/*

*/
static int SC8551_ibatucpalm_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	return 0;
}
/*

*/
static int SC8551_ibusocpalm_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	return 0;
}
/*

*/
static int SC8551_vbusovpalm_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	SC8551_set_notify(chip, SC8551_NOTIFY_VBUSOVPALM);
	return 0;
}
/*

*/
static int SC8551_ibatocpalm_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	return 0;
}
/*

*/
static int SC8551_vbatovpalm_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	SC8551_set_notify(chip, SC8551_NOTIFY_VBATOVPALM);
	return 0;
}
/*

*/
static int SC8551_tdieotpalm_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	return 0;
}
/*

*/
static int SC8551_tsbusotp_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	return 0;
}
/*

*/
static int SC8551_tsbatotp_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	return 0;
}
/*

*/
static int SC8551_tsbusbatotpalm_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	return 0;
}
/*

*/
static int SC8551_ibusocp_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	SC8551_set_notify(chip, SC8551_NOTIFY_IBUSOCP);
	return 0;
}
/*

*/
static int SC8551_vbusovp_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	SC8551_set_notify(chip, SC8551_NOTIFY_VBUSOVP);
	return 0;
}
/*

*/
static int SC8551_ibatocp_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	SC8551_set_notify(chip, SC8551_NOTIFY_IBATOCP);
	return 0;
}
/*

*/
static int SC8551_vbatovp_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	SC8551_set_notify(chip, SC8551_NOTIFY_VBATOVP);
	return 0;
}
/*

*/
static int SC8551_voutovp_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	SC8551_set_notify(chip, SC8551_NOTIFY_VOUTOVP);
	return 0;
}
/*

*/
static int SC8551_vdrovp_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	SC8551_set_notify(chip, SC8551_NOTIFY_VDROVP);
	return 0;
}
/*

*/
static int SC8551_ibatreg_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	return 0;
}
/*

*/
static int SC8551_vbatreg_irq_handler(struct SC8551_chip *chip)
{
	CP_INFO("[%s]enter\n",chip->client->name);
	return 0;
}
/*

*/
static const struct irq_map_desc SC8551_irq_map_tbl[SC8551_IRQIDX_MAX] = {
	SC8551_IRQ_DESC_RSS(vacovp, SC8551_SF_ACPROTECT, 6, 7,
			    SC8551_IRQIDX_VACOVP),
	SC8551_IRQ_DESC_RS(ibusucpf, SC8551_SF_IBUSOCUCP, 4,
			   SC8551_IRQIDX_IBUSUCPF),
	SC8551_IRQ_DESC_RS(ibusucpr, SC8551_SF_IBUSOCUCP, 6,
			   SC8551_IRQIDX_IBUSUCPR),
	SC8551_IRQ_DESC_RS(cflydiag, SC8551_SF_CONVSTAT, 0,
			   SC8551_IRQIDX_CFLYDIAG),
	SC8551_IRQ_DESC_RS(conocp, SC8551_SF_CONVSTAT, 1, SC8551_IRQIDX_CONOCP),
	SC8551_IRQ_DESC_RSSO(switching, SC8551_SF_CONVSTAT, 2,
			     SC8551_IRQIDX_SWITCHING),
	SC8551_IRQ_DESC_RS(ibusucptout, SC8551_SF_CONVSTAT, 3,
			   SC8551_IRQIDX_IBUSUCPTOUT),
	SC8551_IRQ_DESC_RSSO(vbushigherr, SC8551_SF_CONVSTAT, 4,
			     SC8551_IRQIDX_VBUSHERR),
	SC8551_IRQ_DESC_RSSO(vbuslowerr, SC8551_SF_CONVSTAT, 5,
			     SC8551_IRQIDX_VBUSLERR),
	SC8551_IRQ_DESC_RSS(tdieotp, SC8551_SF_CONVSTAT, 7, 6,
			    SC8551_IRQIDX_TDIEOTP),
	SC8551_IRQ_DESC_RS(wdt, SC8551_SF_CHGCTRL0, 3, SC8551_IRQIDX_WDT),
	SC8551_IRQ_DESC_RRS(adcdone, SC8551_SF_INTFLAG, SC8551_SF_INTSTAT, 0,
			    SC8551_IRQIDX_ADCDONE),
	SC8551_IRQ_DESC_RRS(voutinsert, SC8551_SF_INTFLAG,
			    SC8551_SF_INTSTAT, 1, SC8551_IRQIDX_VOUTINSERT),
	SC8551_IRQ_DESC_RRS(vacinsert, SC8551_SF_INTFLAG, SC8551_SF_INTSTAT, 2,
			    SC8551_IRQIDX_VACINSERT),
	SC8551_IRQ_DESC_RRS(ibatucpalm, SC8551_SF_INTFLAG,
			    SC8551_SF_INTSTAT, 3, SC8551_IRQIDX_IBATUCPALM),
	SC8551_IRQ_DESC_RRS(ibusocpalm, SC8551_SF_INTFLAG,
			    SC8551_SF_INTSTAT, 4, SC8551_IRQIDX_IBUSOCPALM),
	SC8551_IRQ_DESC_RRS(vbusovpalm, SC8551_SF_INTFLAG,
			    SC8551_SF_INTSTAT, 5, SC8551_IRQIDX_VBUSOVPALM),
	SC8551_IRQ_DESC_RRS(ibatocpalm, SC8551_SF_INTFLAG,
			    SC8551_SF_INTSTAT, 6, SC8551_IRQIDX_IBATOCPALM),
	SC8551_IRQ_DESC_RRS(vbatovpalm, SC8551_SF_INTFLAG,
			    SC8551_SF_INTSTAT, 7, SC8551_IRQIDX_VBATOVPALM),
	SC8551_IRQ_DESC_RRS(tdieotpalm, SC8551_SF_FLTFLAG,
			    SC8551_SF_FLTSTAT, 0, SC8551_IRQIDX_TDIEOTPALM),
	SC8551_IRQ_DESC_RRS(tsbusotp, SC8551_SF_FLTFLAG, SC8551_SF_FLTSTAT, 1,
			    SC8551_IRQIDX_TSBUSOTP),
	SC8551_IRQ_DESC_RRS(tsbatotp, SC8551_SF_FLTFLAG, SC8551_SF_FLTSTAT, 2,
			    SC8551_IRQIDX_TSBATOTP),
	SC8551_IRQ_DESC_RRS(tsbusbatotpalm, SC8551_SF_FLTFLAG,
			    SC8551_SF_FLTSTAT, 3, SC8551_IRQIDX_TSBUSBATOTPALM),
	SC8551_IRQ_DESC_RRS(ibusocp, SC8551_SF_FLTFLAG, SC8551_SF_FLTSTAT, 4,
			    SC8551_IRQIDX_IBUSOCP),
	SC8551_IRQ_DESC_RRS(vbusovp, SC8551_SF_FLTFLAG, SC8551_SF_FLTSTAT, 5,
			    SC8551_IRQIDX_VBUSOVP),
	SC8551_IRQ_DESC_RRS(ibatocp, SC8551_SF_FLTFLAG, SC8551_SF_FLTSTAT, 6,
			    SC8551_IRQIDX_IBATOCP),
	SC8551_IRQ_DESC_RRS(vbatovp, SC8551_SF_FLTFLAG, SC8551_SF_FLTSTAT, 7,
			    SC8551_IRQIDX_VBATOVP),
	SC8551_IRQ_DESC(voutovp, SC8551_SF_REGFLAGMASK, SC8551_SF_REGTHRES,
			4, 0, SC8551_IRQIDX_VOUTOVP, false),
	SC8551_IRQ_DESC(vdrovp, SC8551_SF_REGFLAGMASK, SC8551_SF_REGTHRES,
			5, 1, SC8551_IRQIDX_VDROVP, false),
	SC8551_IRQ_DESC(ibatreg, SC8551_SF_REGFLAGMASK, SC8551_SF_REGTHRES,
			6, 2, SC8551_IRQIDX_IBATREG, false),
	SC8551_IRQ_DESC(vbatreg, SC8551_SF_REGFLAGMASK, SC8551_SF_REGTHRES,
			7, 3, SC8551_IRQIDX_VBATREG, false),
};
/*

*/
static int __SC8551_update_status(struct SC8551_chip *chip)
{
	int i;
	u8 sf[SC8551_SF_MAX] = {0};
	const struct irq_map_desc *desc;

	for (i = 0; i < SC8551_SF_MAX; i++)
		SC8551_i2c_read8(chip, SC8551_reg_sf[i], &sf[i]);

	for (i = 0; i < ARRAY_SIZE(SC8551_irq_map_tbl); i++) {
		desc = &SC8551_irq_map_tbl[i];
		if (sf[desc->flag_idx] & desc->flag_mask) {
			if (!desc->stat_only)
				chip->flag |= BIT(desc->irq_idx);
		}
		if (sf[desc->stat_idx] & desc->stat_mask) {
			if (desc->stat_only &&
			    !(chip->stat & BIT(desc->irq_idx)))
				chip->flag |= BIT(desc->irq_idx);
			chip->stat |= BIT(desc->irq_idx);
		} else {
			if (desc->stat_only &&
			    (chip->stat & BIT(desc->irq_idx)))
				chip->flag |= BIT(desc->irq_idx);
			chip->stat &= ~BIT(desc->irq_idx);
		}
	}
	return 0;
}
/*

*/
static int __maybe_unused SC8551_update_status(struct SC8551_chip *chip)
{
	int ret;

	mutex_lock(&chip->stat_lock);
	ret = __SC8551_update_status(chip);
	mutex_unlock(&chip->stat_lock);
	return ret;
}
/*

*/
static int SC8551_notify_task_threadfn(void *data)
{
	int i;
	struct SC8551_chip *chip = data;

	while (!kthread_should_stop()) {
		wait_event_interruptible(chip->wq, chip->notify != 0 ||
					 kthread_should_stop());
		if (kthread_should_stop())
			goto out;
		pm_stay_awake(chip->dev);
		mutex_lock(&chip->notify_lock);
		for (i = 0; i < SC8551_NOTIFY_MAX; i++) {
			if (chip->notify & BIT(i)) {
				chip->notify &= ~BIT(i);
				mutex_unlock(&chip->notify_lock);
				charger_dev_notify(chip->chg_dev,
						   SC8551_chgdev_notify_map[i]);
				mutex_lock(&chip->notify_lock);
			}
		}
		mutex_unlock(&chip->notify_lock);
		pm_relax(chip->dev);
	}
out:
	return 0;
}
/*

*/
static irqreturn_t SC8551_irq_handler(int irq, void *data)
{
	int i;
	struct SC8551_chip *chip = data;
	const struct irq_map_desc *desc;

	pm_stay_awake(chip->dev);
	mutex_lock(&chip->stat_lock);
	__SC8551_update_status(chip);
	for (i = 0; i < ARRAY_SIZE(SC8551_irq_map_tbl); i++) {
		desc = &SC8551_irq_map_tbl[i];
		if ((chip->flag & (1 << desc->irq_idx)) && desc->hdlr)
			desc->hdlr(chip);
	}
	chip->flag = 0;
	wake_up_interruptible(&chip->wq);
	mutex_unlock(&chip->stat_lock);
	pm_relax(chip->dev);
	return IRQ_HANDLED;
}
/*

*/
static const struct charger_ops SC8551_chg_ops = {
	.enable = SC8551_enable_chg,
	.is_enabled = SC8551_is_chg_enabled,
	.get_adc = SC8551_get_adc,
	.set_vbusovp = SC8551_set_vbusovp,
	.set_ibusocp = SC8551_set_ibusocp,
	.set_vbatovp = SC8551_set_vbatovp,
	.set_ibatocp = SC8551_set_ibatocp,
	.init_chip = SC8551_init_chip,
	.set_vbatovp_alarm = SC8551_set_vbatovp_alarm,
	.reset_vbatovp_alarm = SC8551_reset_vbatovp_alarm,
	.set_vbusovp_alarm = SC8551_set_vbusovp_alarm,
	.reset_vbusovp_alarm = SC8551_reset_vbusovp_alarm,
	.is_vbuslowerr = SC8551_is_vbuslowerr,
	.get_adc_accuracy = SC8551_get_adc_accuracy,
};
/*

*/
static int SC8551_register_chgdev(struct SC8551_chip *chip)
{
	chip->chg_prop.alias_name = chip->desc->chg_name;
	chip->chg_dev = charger_device_register(chip->desc->chg_name, chip->dev,
						chip, &SC8551_chg_ops,
						&chip->chg_prop);
	if (!chip->chg_dev)
		return -EINVAL;
	return 0;
}
/*

*/
static int SC8551_clearall_irq(struct SC8551_chip *chip)
{
	int i, ret;
	u8 data;

	for (i = 0; i < SC8551_SF_MAX; i++) {
		ret = SC8551_i2c_read8(chip, SC8551_reg_sf[i], &data);
		if (ret < 0)
			return ret;
	}
	return 0;
}
/*

*/
static int SC8551_init_irq(struct SC8551_chip *chip)
{
	int ret = 0, len = 0;
	char *name = NULL;

	CP_INFO("[%s]enter\n",chip->client->name);
	ret = SC8551_clearall_irq(chip);
	if (ret < 0) {
		CP_ERR("[%s]clr all irq fail(%d)\n",chip->client->name,ret);
		return ret;
	}

	len = strlen(chip->desc->chg_name);
	chip->irq = gpiod_to_irq(chip->irq_gpio);
	if (chip->irq < 0) {
		CP_ERR("[%s]irq mapping fail(%d)\n",chip->client->name,chip->irq);
		return ret;
	}
	CP_INFO("[%s]irq = %d\n",chip->client->name,chip->irq);

	/* Request threaded IRQ */
	name = devm_kzalloc(chip->dev, len + 5, GFP_KERNEL);
	snprintf(name, len + 5, "%s_irq", chip->desc->chg_name);

	ret = devm_request_threaded_irq(chip->dev, chip->irq, NULL,
		SC8551_irq_handler, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, name,
		chip);
	if (ret < 0) {
		CP_ERR("[%s]request thread irq fail(%d)\n",chip->client->name,ret);
		return ret;
	}
	device_init_wakeup(chip->dev, true);
	return 0;
}
/*

*/
static inline void SC8551_parse_dt_u32(struct device_node *np, void *desc,
				       const struct SC8551_dtprop *props,
				       int prop_cnt)
{
	int i;

	for (i = 0; i < prop_cnt; i++) {
		if (unlikely(!props[i].name))
			continue;
		of_property_read_u32(np, props[i].name, desc + props[i].offset);
	}
}
/*

*/
static inline void SC8551_parse_dt_bool(struct device_node *np, void *desc,
					const struct SC8551_dtprop *props,
					int prop_cnt)
{
	int i;

	for (i = 0; i < prop_cnt; i++) {
		if (unlikely(!props[i].name))
			continue;
		*((bool *)(desc + props[i].offset)) =
			of_property_read_bool(np, props[i].name);
	}
}
/*

*/
static inline int SC8551_apply_dt(struct SC8551_chip *chip, void *desc,
				  const struct SC8551_dtprop *props,
				  int prop_cnt)
{
	int i, ret;
	u32 val;

	for (i = 0; i < prop_cnt; i++) {
		val = *(u32 *)(desc + props[i].offset);
		if (props[i].toreg)
			val = props[i].toreg(val);
		val += props[i].base;
		ret = SC8551_i2c_update_bits(chip, props[i].reg,
					     val << props[i].shft,
					     props[i].mask);
		if (ret < 0)
			return ret;
	}
	return 0;
}
/*

*/
static const struct SC8551_dtprop SC8551_dtprops_u32[] = {
	SC8551_DT_VALPROP(vbatovp, SC8551_REG_VBATOVP, 0, 0x3f,
			  SC8551_vbatovp_toreg, 0),
	SC8551_DT_VALPROP(vbatovp_alm, SC8551_REG_VBATOVP_ALM, 0, 0x3f,
			  SC8551_vbatovp_toreg, 0),
	SC8551_DT_VALPROP(ibatocp, SC8551_REG_IBATOCP, 0, 0x7f,
			  SC8551_ibatocp_toreg, 0),
	SC8551_DT_VALPROP(ibatocp_alm, SC8551_REG_IBATOCP_ALM, 0, 0x7f,
			  SC8551_ibatocp_toreg, 0),
	SC8551_DT_VALPROP(ibatucp_alm, SC8551_REG_IBATUCP_ALM, 0, 0x7f,
			  SC8551_ibatucp_toreg, 0),
	SC8551_DT_VALPROP(vbusovp, SC8551_REG_VBUSOVP, 0, 0x7f,
			  SC8551_vbusovp_toreg, 0),
	SC8551_DT_VALPROP(vbusovp_alm, SC8551_REG_VBUSOVP_ALM, 0, 0x7f,
			  SC8551_vbusovp_toreg, 0),
	SC8551_DT_VALPROP(ibusocp, SC8551_REG_IBUSOCUCP, 0, 0x0f,
			  SC8551_ibusocp_toreg, 0),
	SC8551_DT_VALPROP(ibusocp_alm, SC8551_REG_IBUSOCP_ALM, 0, 0x7f,
			  SC8551_ibusocp_alm_toreg, 0),
	SC8551_DT_VALPROP(wdt, SC8551_REG_CHGCTRL0, 0, 0x03,
			  SC8551_wdt_toreg, 0),
	SC8551_DT_VALPROP(vacovp, SC8551_REG_ACPROTECT, 0, 0x07,
			  SC8551_vacovp_toreg, 0),
	SC8551_DT_VALPROP(ibat_rsense, SC8551_REG_REGCTRL, 1, 0x02, NULL, 0),
	SC8551_DT_VALPROP(ibusucpf_deglitch, SC8551_REG_BUSDEGLH, 3, 0x08, NULL,
			  1),
};
/*

*/
static const struct SC8551_dtprop SC8551_dtprops_bool[] = {
	SC8551_DT_VALPROP(vbatovp_dis, SC8551_REG_VBATOVP, 7, 0x80, NULL, 0),
	SC8551_DT_VALPROP(vbatovp_alm_dis, SC8551_REG_VBATOVP_ALM, 7, 0x80,
			  NULL, 0),
	SC8551_DT_VALPROP(ibatocp_dis, SC8551_REG_IBATOCP, 7, 0x80, NULL, 0),
	SC8551_DT_VALPROP(ibatocp_alm_dis, SC8551_REG_IBATOCP_ALM, 7, 0x80,
			  NULL, 0),
	SC8551_DT_VALPROP(ibatucp_alm_dis, SC8551_REG_IBATUCP_ALM, 7, 0x80,
			  NULL, 0),
	SC8551_DT_VALPROP(vbusovp_alm_dis, SC8551_REG_VBUSOVP_ALM, 7, 0x80,
			  NULL, 0),
	SC8551_DT_VALPROP(ibusocp_dis, SC8551_REG_IBUSOCUCP, 7, 0x80, NULL, 0),
	SC8551_DT_VALPROP(ibusocp_alm_dis, SC8551_REG_IBUSOCP_ALM, 7, 0x80,
			  NULL, 0),
	SC8551_DT_VALPROP(wdt_dis, SC8551_REG_CHGCTRL0, 2, 0x04, NULL, 0),
	SC8551_DT_VALPROP(tsbusotp_dis, SC8551_REG_CHGCTRL1, 2, 0x04, NULL, 1),
	SC8551_DT_VALPROP(tsbatotp_dis, SC8551_REG_CHGCTRL1, 1, 0x02, NULL, 1),
	SC8551_DT_VALPROP(tdieotp_dis, SC8551_REG_CHGCTRL1, 0, 0x01, NULL, 0),
	SC8551_DT_VALPROP(reg_en, SC8551_REG_REGCTRL, 4, 0x10, NULL, 0),
	SC8551_DT_VALPROP(voutovp_dis, SC8551_REG_REGCTRL, 3, 0x08, NULL, 0),
	SC8551_DT_VALPROP(adc_dis, SC8551_REG_ADCCTRL, 7, 0x80, NULL, 0),
	SC8551_DT_VALPROP(ibusadc_dis, SC8551_REG_ADCCTRL, 0, 0x01, NULL, 0),
	SC8551_DT_VALPROP(tdieadc_dis, SC8551_REG_ADCEN, 0, 0x01, NULL, 0),
	SC8551_DT_VALPROP(tsbatadc_dis, SC8551_REG_ADCEN, 1, 0x02, NULL, 0),
	SC8551_DT_VALPROP(tsbusadc_dis, SC8551_REG_ADCEN, 2, 0x04, NULL, 0),
	SC8551_DT_VALPROP(ibatadc_dis, SC8551_REG_ADCEN, 3, 0x08, NULL, 0),
	SC8551_DT_VALPROP(vbatadc_dis, SC8551_REG_ADCEN, 4, 0x10, NULL, 0),
	SC8551_DT_VALPROP(voutadc_dis, SC8551_REG_ADCEN, 5, 0x20, NULL, 0),
	SC8551_DT_VALPROP(vacadc_dis, SC8551_REG_ADCEN, 6, 0x40, NULL, 0),
	SC8551_DT_VALPROP(vbusadc_dis, SC8551_REG_ADCEN, 7, 0x80, NULL, 0),
};
/*
			parse dtsi
*/
static int SC8551_parse_dt(struct SC8551_chip *chip)
{
	struct SC8551_desc *desc;
	struct device_node *np = chip->dev->of_node;
	struct device_node *child_np;

	if (!np)
		return -ENODEV;
	if(!strcmp(chip->client->name,"sc8551m")){
		chip->irq_gpio = devm_gpiod_get(chip->dev, "sc8551m,intr", GPIOD_IN);
		if (IS_ERR(chip->irq_gpio))
			return PTR_ERR(chip->irq_gpio);
	}
	if(!strcmp(chip->client->name,"sc8551s")){
		chip->irq_gpio = devm_gpiod_get(chip->dev, "sc8551s,intr", GPIOD_IN);
		if (IS_ERR(chip->irq_gpio))
			return PTR_ERR(chip->irq_gpio);
	}
	desc = devm_kzalloc(chip->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	memcpy(desc, &SC8551_desc_defval, sizeof(*desc));

	if (of_property_read_string(np, "rm_name", &desc->rm_name) < 0)
		CP_ERR("[%s] no rm name\n",chip->client->name);

	if (of_property_read_u8(np, "rm_slave_addr", &desc->rm_slave_addr) < 0)
		CP_ERR("[%s] no slave addr\n",chip->client->name);

	CP_INFO("[%s]Find SC8551's type [%d - %s] in dtsi",chip->client->name,chip->type,SC8551_type_name[chip->type]);
	child_np = of_get_child_by_name(np, SC8551_type_name[chip->type]);
	if (!child_np) {
		CP_ERR("[%s]no node(%s) found\n",chip->client->name,SC8551_type_name[chip->type]);
		return -ENODEV;
	}
	if (of_property_read_string(child_np, "chg_name", &desc->chg_name) < 0)
		CP_ERR("[%s]no chg name\n",chip->client->name);
	SC8551_parse_dt_u32(child_np, (void *)desc, SC8551_dtprops_u32,
			    ARRAY_SIZE(SC8551_dtprops_u32));
	SC8551_parse_dt_bool(child_np, (void *)desc, SC8551_dtprops_bool,
			     ARRAY_SIZE(SC8551_dtprops_bool));
	chip->desc = desc;
	return 0;
}
/*

*/
static int __SC8551_init_chip(struct SC8551_chip *chip)
{
	int i;
	u8 data = 0;
	int ret;

	CP_INFO("[%s]enter \n",chip->client->name);
	ret = SC8551_apply_dt(chip, (void *)chip->desc, SC8551_dtprops_u32,
			      ARRAY_SIZE(SC8551_dtprops_u32));
	if (ret < 0)
		return ret;
	ret = SC8551_apply_dt(chip, (void *)chip->desc, SC8551_dtprops_bool,
			      ARRAY_SIZE(SC8551_dtprops_bool));
	if (ret < 0)
		return ret;

	if (chip->is_sc8551) {
		SC8551_set_bits(chip, 0x34, 0x01);

		CP_INFO("[%s]DumpReg:\n",chip->client->name);
		for (i = 0; i < 0x36; i++) {
			SC8551_i2c_read8(chip, i, &data);
			CP_INFO("[%s] Reg[0x%02x] = 0x%02x \n",chip->client->name, i, data);
		}
	}
	chip->wdt_en = !chip->desc->wdt_dis;
	return chip->wdt_en ? SC8551_enable_wdt(chip, false) : 0;
}
/*

*/
static int SC8551_check_devinfo(struct i2c_client *client, u8 *chip_rev,
				enum SC8551_type *type)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, SC8551_REG_DEVINFO);
	if (ret < 0)
		return ret;

	if ((ret & 0x0f) != SC8551_DEVID_A && (ret & 0x0f) != SC8551_DEVID_B && (ret & 0x0f) != SC8551_DEVID_C)
		return -ENODEV;

	*chip_rev = ret;

	ret = i2c_smbus_read_byte_data(client, SC8551_REG_CHGCTRL1);
	if (ret < 0)
		return ret;
	*type = (ret & 0x60) >> 5;
	CP_INFO("[%s] Rev(0x%02X), type(%s)\n",client->name, *chip_rev, SC8551_type_name[*type]);
	return 0;
}
/*

*/
static int SC8551_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	int ret;
	struct SC8551_chip *chip;
	u8 chip_rev = 0;
	enum SC8551_type type;

	CP_INFO("[%s](%s)\n",client->name,SC8551_DRV_VERSION);

	ret = SC8551_check_devinfo(client, &chip_rev, &type);
	if (ret < 0)
		return ret;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	chip->dev = &client->dev;
	chip->client = client;
	chip->revision = (chip_rev & 0xf0) >> 4;
	chip->type = type;
	chip->is_sc8551 = false;

	if ((chip_rev & 0x0f) == SC8551_DEVID_B || (chip_rev & 0x0f) == SC8551_DEVID_C)
		chip->is_sc8551 = true;
	mutex_init(&chip->io_lock);
	mutex_init(&chip->adc_lock);
	mutex_init(&chip->stat_lock);
	mutex_init(&chip->hm_lock);
	mutex_init(&chip->suspend_lock);
	mutex_init(&chip->notify_lock);
	init_waitqueue_head(&chip->wq);
	i2c_set_clientdata(client, chip);

	ret = SC8551_parse_dt(chip);
	if (ret < 0) {
		CP_ERR("[%s] parse dt fail(%d)\n",client->name, ret);
		goto err;
	}
#ifdef CONFIG_RT_REGMAP
	ret = SC8551_register_regmap(chip);
	if (ret < 0) {
		CP_ERR("[%s]regmap fail(%d)\n",client->name,ret);
		goto err;
	}
#endif /* CONFIG_RT_REGMAP */
	ret = __SC8551_init_chip(chip);
	if (ret < 0) {
		CP_ERR("[%s]init chip fail(%d)\n",client->name,ret);
		goto err_initchip;
	}

	ret = SC8551_register_chgdev(chip);
	if (ret < 0) {
		CP_ERR("[%s]reg chgdev fail(%d)\n",client->name,ret);
		goto err_initchip;
	}

	chip->notify_task = kthread_run(SC8551_notify_task_threadfn, chip,"SC8551M_notify_thread");
	if (IS_ERR(chip->notify_task)) {
		CP_ERR("[%s]run notify thread fail(%d)\n",client->name,ret);
		ret = PTR_ERR(chip->notify_task);
		goto err_initirq;
	}

	ret = SC8551_init_irq(chip);
	if (ret < 0) {
		CP_ERR("[%s]init irq fail(%d)\n",client->name,ret);
		goto err_initirq;
	}

#if IS_ENABLED(CONFIG_PRIZE_HARDWARE_INFO)
	strcpy(current_cp_info.chip,"SC8551");
	if(!strcmp(client->name,"sc8551m")){
		gCpID[0] = (chip_rev << 8);
	}else	if(!strcmp(client->name,"sc8551s")){
		gCpID[1] = (chip_rev);
	}
	sprintf(current_cp_info.id,"0x%04x",(gCpID[0]|gCpID[1]));
	strcpy(current_cp_info.vendor,"SouthChip");
	strcpy(current_cp_info.more,"ChargePump");
#endif

	CP_INFO("[%s] Successfully\n",client->name);
	return 0;
err_initirq:
	charger_device_unregister(chip->chg_dev);
err_initchip:
#ifdef CONFIG_RT_REGMAP
	rt_regmap_device_unregister(chip->rm_dev);
#endif /* CONFIG_RT_REGMAP */
err:
	mutex_destroy(&chip->notify_lock);
	mutex_destroy(&chip->suspend_lock);
	mutex_destroy(&chip->hm_lock);
	mutex_destroy(&chip->stat_lock);
	mutex_destroy(&chip->adc_lock);
	mutex_destroy(&chip->io_lock);
	return ret;
}
/*

*/
static void SC8551_i2c_shutdown(struct i2c_client *client)
{
   int ret;
   struct SC8551_chip *chip = i2c_get_clientdata(client);
   if(chip->is_sc8551)
      ret=SC8551_clr_bits(chip, SC8551_REG_ADCCTRL,SC8551_ADCEN_MASK);
}
/*

*/
static int SC8551_i2c_remove(struct i2c_client *client)
{
	int ret;
	struct SC8551_chip *chip = i2c_get_clientdata(client);

 	CP_INFO("[%s]\n",client->name);
	if (!chip)
		return 0;

	if(chip->is_sc8551)
		ret=SC8551_clr_bits(chip,SC8551_REG_ADCCTRL,SC8551_ADCEN_MASK);

	if (chip->notify_task)
		kthread_stop(chip->notify_task);
	charger_device_unregister(chip->chg_dev);
#ifdef CONFIG_RT_REGMAP
	rt_regmap_device_unregister(chip->rm_dev);
#endif /* CONFIG_RT_REGMAP */
	mutex_destroy(&chip->notify_lock);
	mutex_destroy(&chip->suspend_lock);
	mutex_destroy(&chip->hm_lock);
	mutex_destroy(&chip->stat_lock);
	mutex_destroy(&chip->adc_lock);
	mutex_destroy(&chip->io_lock);
	return 0;
}
/*

*/
static int __maybe_unused SC8551_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct SC8551_chip *chip = i2c_get_clientdata(client);

 	CP_INFO("[%s]\n",client->name);
	mutex_lock(&chip->suspend_lock);
	if (device_may_wakeup(dev))
		enable_irq_wake(chip->irq);
	return 0;
}
/*

*/
static int __maybe_unused SC8551_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct SC8551_chip *chip = i2c_get_clientdata(client);

	CP_INFO("[%s]\n",client->name);
	mutex_unlock(&chip->suspend_lock);
	if (device_may_wakeup(dev))
		disable_irq_wake(chip->irq);
	return 0;
}
/*

*/
static SIMPLE_DEV_PM_OPS(SC8551_pm_ops, SC8551_i2c_suspend, SC8551_i2c_resume);
/*

*/
static const struct of_device_id SC8551_of_id[] = {
	{ .compatible = "sc,sc8551m" },
	{ .compatible = "sc,sc8551s" },
	{},
};
MODULE_DEVICE_TABLE(of, SC8551_of_id);
/*

*/
static const struct i2c_device_id SC8551_i2c_id[] = {
	{ "sc8551m", 0},
	{ "sc8551s", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, SC8551_i2c_id);
/*

*/
static struct i2c_driver SC8551_i2c_driver = {
	.driver = {
		.name 	= "sc8551",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(SC8551_of_id),
		.pm = &SC8551_pm_ops,
	},
	.probe 		= SC8551_i2c_probe,
	.shutdown = SC8551_i2c_shutdown,
	.remove 	= SC8551_i2c_remove,
	.id_table = SC8551_i2c_id,
};
module_i2c_driver(SC8551_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Northchip SC8551 Charger Driver");
MODULE_AUTHOR("ShuFan Lee<shufan_lee@richtek.com>");
MODULE_VERSION(SC8551_DRV_VERSION);