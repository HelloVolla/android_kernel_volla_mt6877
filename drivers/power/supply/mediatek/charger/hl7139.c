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
//Antaiui <AI_BSP_CHG> <hehl> <2021-03-01> modify For 2005R 30w fast charger start
//#include <mt-plat/mtk_battery.h>

/* Information */
#define HL7139_DRV_VERSION	"1.0.8_MTK"

//Antaiui <AI_BSP_CHG> <hehl> <2021-03-01> modify For 2005R 30w fast charger end
/* Registers */
#define HL7139_REG_VBATOVP	0x08 //hhl modify
#define HL7139_REG_VBATOVP_ALM	0x01
#define HL7139_REG_IBATOCP	0x02
#define HL7139_REG_IBATOCP_ALM	0x03
#define HL7139_REG_IBATUCP_ALM	0x04
#define HL7139_REG_ACPROTECT	0x05 // VBUS_OVP
#define HL7139_REG_VBUSOVP	0x0B      //VIN_OVP
#define HL7139_REG_VBUSOVP_ALM	0x07
#define HL7139_REG_IBUSOCUCP	0x0E  //IIN_OCP_UCP
#define HL7139_REG_IBUSOCP_ALM	0x09
#define HL7139_REG_CONVSTAT	0x0A
#define HL7139_REG_CHGCTRL0	0x0B
#define HL7139_REG_CHGCTRL1	0x0C
#define HL7139_REG_INTSTAT	0x0D
#define HL7139_REG_INTFLAG	0x0E
#define HL7139_REG_INTMASK	0x0F
#define HL7139_REG_FLTSTAT	0x10
#define HL7139_REG_FLTFLAG	0x11
#define HL7139_REG_FLTMASK	0x12

#define HL7139_REG_DEVINFO	0x13

#define HL7139_REG_ADCCTRL	0x14
#define HL7139_REG_ADCEN	0x15

#define HL7139_REG_IBUSADC1	0x44
#define HL7139_REG_IBUSADC0	0x45
#define HL7139_REG_VBUSADC1	0x42
#define HL7139_REG_VBUSADC0	0x43

#define HL7139_REG_VACADC1	0x1A
#define HL7139_REG_VACADC0	0x1B

#define HL7139_REG_VOUTADC1	0x4C
#define HL7139_REG_VOUTADC0	0x4D

#define HL7139_REG_VBATADC1	0x46
#define HL7139_REG_VBATADC0	0x47
#define HL7139_REG_IBATADC1	0x48
#define HL7139_REG_IBATADC0	0x49

#define HL7139_REG_TSBUSADC1	0x22
#define HL7139_REG_TSBUSADC0	0x23
#define HL7139_REG_TSBATADC1	0x24
#define HL7139_REG_TSBATADC0	0x25

#define HL7139_REG_TDIEADC1	0x4E
#define HL7139_REG_TDIEADC0	0x4F

#define HL7139_REG_TSBUSOTP	0x28 //TSBUS_FLT
#define HL7139_REG_TSBATOTP	0x29//TSBAT_FLT

#define HL7139_REG_TDIEALM	0x2A

#define HL7139_REG_REGCTRL	0x2B
#define HL7139_REG_REGTHRES	0x2C
#define HL7139_REG_REGFLAGMASK	0x2D

#define HL7139_REG_BUSDEGLH	0x2E

////////////////////////////////////////
#define HL7130_PMID_VOUT_UV_OV	0x2F
////////////////////////////////////////

#define HL7139_REG_OTHER1	0x30 //CONTROL2

//////////////////////////////////////
#define  HL7130_REG_FLAG_MASK1	0x31
#define  HL7130_CLK_SYNC	0x32
#define  HL7130_ADC_CTL1	0x33
///////////////////////////////////////

///////////////////////////////////////
#define HL7139_REG_SYSCTRL1	0x42

#define HL7139_REG_PASSWORD0	0xA7
//#define HL7139_REG_PASSWORD1	0x91
//Antaiui <AI_BSP_CHG> <hehl> <2021-03-01> modify For 2005R 30w fast charger start

#define HL7139_REG_PMID_VOUT_UV_OV	0x2F
///////////////////////////////////////
//Antaiui <AI_BSP_CHG> <hehl> <2021-03-01> modify For 2005R 30w fast charger end
/* Control bits */

#define HL7139_CHGEN_MASK	BIT(7)
#define HL7139_CHGEN_SHFT	7

#define HL7139_ADCEN_MASK	0xFF

#define HL7139_WDTEN_MASK	BIT(3)

#define HL7139_WDTMR_MASK	0x03

#define HL7139_VBUSOVP_MASK	0x0F
#define HL7139_IBUSOCP_MASK	0x3F

#define HL7139_VBATOVP_MASK	0x3F
#define HL7139_IBATOCP_MASK	0x7F

#define HL7139_VBATOVP_ALM_MASK	0x3F
#define HL7139_VBATOVP_ALMDIS_MASK	BIT(7)
#define HL7139_VBUSOVP_ALM_MASK	0x7F
#define HL7139_VBUSOVP_ALMDIS_MASK	BIT(7)

#define HL7139_VBUSLOWERR_FLAG_SHFT	2
#define HL7139_VBUSLOWERR_STAT_SHFT	5
///////////////////////////////////////////////////////////////////
//HL7130 Register map

#define HL7139_DEVID_R			  0x0a//0x23


#define HL7139_DEVID	                0x00

#define HL7139_REG_INT	                0x01
#define HL7139_REG_MSK	                0x02
#define HL7139_REG_INT_STS_A	  0x03
#define HL7139_REG_INT_STS_B	  0x04

#define HL7139_REG_STS_A	         0x05
#define HL7139_REG_STS_B	         0x06 
#define HL7139_REG_STS_C	         0x07

#define HL7139_REG_VBAT_REG       0x08
#define HL7139_REG_STBY_CTRL      0x09
#define HL7139_IBAT_REG	         0x0A

#define HL7139_REG_VBUS_OVP	  0x0B
#define HL7139_REG_VIN_OVP	         0x0C

#define HL7139_REG_RSVD	         0x0D
#define HL7139_REG_IIN_REG	         0x0E
#define HL7139_REG_IIN_OC            0x0F

#define HL7139_REG_CTRL0	         0x10 
#define HL7139_REG_CTRL1	         0x11
#define HL7139_CTRL0                      0x12
#define HL7139_CTRL1                      0x13
#define HL7139_CTRL2	                0x14
#define HL7139_CTRL3	                0x15

#define HL7139_REG_TRACK_OVUV                0x16
#define HL7139_REG_TS_TH	                      0x17


#define HL7139_DEVICE_ID	         	        0x36
#define HL7139_VOOC_CTRL	                      0x37
#define HL7139_VOOC_WDATA_H	               0x38
#define HL7139_VOOC_WDATA_L	               0x39
#define HL7139_VOOC_RDATA                         0x3A
#define HL7139_VOOC_FLAG                           0x3B
#define HL7139_VOOC_FLAG_M	                0x3C
#define HL7139_VOOC_PREDATA_H	                0x3D
#define HL7139_VOOC_PREDATA_L                 0x3E
#define HL7139_VOOC_OPTOIN	                       0x3F

#define HL7139_REG_ADC_CTRL0	         0x40
#define HL7139_REG_ADC_CTRL1	         0x41

#define HL7139_REG_ADC_VIN_0	         0x42 
#define HL7139_REG_ADC_VIN_1	         0x43

#define HL7139_REG_ADC_IIN_0	         0x42 
#define HL7139_REG_ADC_IIN_1	         0x43

#define HL7139_REG_ADC_VBAT_0	         0x46
#define HL7139_REG_ADC_VBAT_1	         0x47

#define HL7139_REG_ADC_IBAT_0	         0x48 
#define HL7139_REG_ADC_IBAT_1	         0x49
//HL7130 Register map
//////////////////////////////////////////////////////////////////

static struct HL7139_chip *chg_en_chip;
enum HL7139_irqidx {
	HL7139_IRQIDX_VACOVP = 0,
	HL7139_IRQIDX_IBUSUCPF,
	HL7139_IRQIDX_IBUSUCPR,
	HL7139_IRQIDX_CFLYDIAG,
	HL7139_IRQIDX_CONOCP,
	HL7139_IRQIDX_SWITCHING,
	HL7139_IRQIDX_IBUSUCPTOUT,
	HL7139_IRQIDX_VBUSHERR,
	HL7139_IRQIDX_VBUSLERR,
	HL7139_IRQIDX_TDIEOTP,
	HL7139_IRQIDX_WDT,
	HL7139_IRQIDX_ADCDONE,
	HL7139_IRQIDX_VOUTINSERT,
	HL7139_IRQIDX_VACINSERT,
	HL7139_IRQIDX_IBATUCPALM,
	HL7139_IRQIDX_IBUSOCPALM,
	HL7139_IRQIDX_VBUSOVPALM,
	HL7139_IRQIDX_IBATOCPALM,
	HL7139_IRQIDX_VBATOVPALM,
	HL7139_IRQIDX_TDIEOTPALM,
	HL7139_IRQIDX_TSBUSOTP,
	HL7139_IRQIDX_TSBATOTP,
	HL7139_IRQIDX_TSBUSBATOTPALM,
	HL7139_IRQIDX_IBUSOCP,
	HL7139_IRQIDX_VBUSOVP,
	HL7139_IRQIDX_IBATOCP,
	HL7139_IRQIDX_VBATOVP,
	HL7139_IRQIDX_VOUTOVP,
	HL7139_IRQIDX_VDROVP,
	HL7139_IRQIDX_IBATREG,
	HL7139_IRQIDX_VBATREG,
	HL7139_IRQIDX_MAX,
};

enum HL7139_notify {
	HL7139_NOTIFY_IBUSUCPF = 0,
	HL7139_NOTIFY_VBUSOVPALM,
	HL7139_NOTIFY_VBATOVPALM,
	HL7139_NOTIFY_IBUSOCP,
	HL7139_NOTIFY_VBUSOVP,
	HL7139_NOTIFY_IBATOCP,
	HL7139_NOTIFY_VBATOVP,
	HL7139_NOTIFY_VOUTOVP,
	HL7139_NOTIFY_VDROVP,
	HL7139_NOTIFY_MAX,
};

//HL7139 INT Registor 0x01
enum HL7139_statflag_idx {
	HL7139_SF_ACPROTECT = 0,
	HL7139_SF_IBUSOCUCP,
	HL7139_SF_CONVSTAT,
	HL7139_SF_CHGCTRL0,
	HL7139_SF_INTFLAG,
	HL7139_SF_INTSTAT,
	HL7139_SF_FLTFLAG,
	HL7139_SF_FLTSTAT,
	HL7139_SF_REGFLAGMASK,
	HL7139_SF_REGTHRES,
	HL7139_SF_OTHER1,
	HL7139_SF_MAX = 11,
};
//HL7139 INT Registor 0x01

enum HL7139_type {
	HL7139_TYPE_STANDALONE = 0,
	HL7139_TYPE_SLAVE,
	HL7139_TYPE_MASTER,
	HL7139_TYPE_MAX,
};
#if 1
static const char *HL7139_type_name[HL7139_TYPE_MAX] = {
	"standalone", "slave", "master",
};
#endif
static const u32 HL7139_chgdev_notify_map[HL7139_NOTIFY_MAX] = {
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

static const u8 HL7139_reg_sf[HL7139_SF_MAX] = {
	HL7139_REG_ACPROTECT,
	HL7139_REG_IBUSOCUCP,
	HL7139_REG_CONVSTAT,
	HL7139_REG_CHGCTRL0,
	HL7139_REG_INTFLAG,
	HL7139_REG_INTSTAT,
	HL7139_REG_FLTFLAG,
	HL7139_REG_FLTSTAT,
	HL7139_REG_REGFLAGMASK,
	HL7139_REG_REGTHRES,
	HL7139_REG_OTHER1,
};

struct HL7139_reg_defval {
	u8 reg;
	u8 value;
	u8 mask;
};

static const struct HL7139_reg_defval HL7139_init_chip_check_reg[] = {
#if 0
	{
		.reg = HL7139_REG_VBATOVP,
		.value = 0x22,
		.mask = HL7139_VBATOVP_MASK,
	},
	{
		.reg = HL7139_REG_IBATOCP,
		.value = 0x3D,
		.mask = HL7139_IBATOCP_MASK,
	},
	{
		.reg = HL7139_REG_CHGCTRL0,//0x0B
		.value = 0x00,
		.mask = HL7139_WDTMR_MASK,
	},
#endif
};

struct HL7139_dese {
	const char *chg_name;
	const char *rm_name;
	u8 rm_slave_addr;
	u32 vbatovp;
	u32 vbatovp_alm;
	u32 ibatocp;
	u32 ibatocp_alm;
	u32 ibatucp_alm;
	u32 vbusovp;
	u32 vbusovp_alm;
	u32 ibusocp;
	u32 ibusocp_alm;
	u32 vacovp;
	u32 wdt;
	u32 ibat_rsense;
	u32 ibusucpf_deglitch;
	bool vbatovp_dis;
	bool vbatovp_alm_dis;
	bool ibatocp_dis;
	bool ibatocp_alm_dis;
	bool ibatucp_alm_dis;
	bool vbusovp_alm_dis;
	bool ibusocp_dis;
	bool ibusocp_alm_dis;
	bool wdt_dis;
	bool tsbusotp_dis;
	bool tsbatotp_dis;
	bool tdieotp_dis;
	bool reg_en;
	bool voutovp_dis;
	bool ibusadc_dis;
	bool vbusadc_dis;
	bool vacadc_dis;
	bool voutadc_dis;
	bool vbatadc_dis;
	bool ibatadc_dis;
	bool tsbusadc_dis;
	bool tsbatadc_dis;
	bool tdieadc_dis;
};

static const struct HL7139_dese HL7139_dese_defval = {
	.chg_name = "divider_charger",
	.rm_name = "rt9759",
//Antaiui <AI_BSP_CHG> <hehl> <2021-03-01> modify For 2005R 30w fast charger start	
	.rm_slave_addr = 0x5f,
//Antaiui <AI_BSP_CHG> <hehl> <2021-03-01> modify For 2005R 30w fast charger end	
	.vbatovp = 4500000,
	.vbatovp_alm = 4450000,
	.ibatocp = 8100000,
	.ibatocp_alm = 8000000,
	.ibatucp_alm = 2000000,
	.vbusovp = 11000000,
	.vbusovp_alm = 11000000,
	.ibusocp = 4250000,
	.ibusocp_alm = 4000000,
	.vacovp = 11000000,
	.wdt = 500000,
	.ibat_rsense = 1,	/* 2mohm */
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
	.tsbusotp_dis = true,
	.tsbatotp_dis = false,
	.tdieotp_dis = false,
	.reg_en = false,
	.voutovp_dis = false,
};

struct HL7139_chip {
	struct device *dev;
	struct i2c_client *client;
	struct mutex io_lock;
	struct mutex adc_lock;
	struct mutex stat_lock;
	struct mutex hm_lock;
	struct mutex suspend_lock;
	struct mutex notify_lock;
	struct charger_device *chg_dev;
	struct charger_properties chg_prop;
	struct HL7139_dese *desc;
	struct gpio_desc *irq_gpio;
	struct task_struct *notify_task;
	int irq;
	int notify;
	u8 revision;
	u32 flag;
	u32 stat;
	u32 hm_cnt;
	enum HL7139_type type;
	bool wdt_en;
	bool force_adc_en;
	bool stop_thread;
	wait_queue_head_t wq;

#ifdef CONFIG_RT_REGMAP
	struct rt_regmap_device *rm_dev;
	struct rt_regmap_properties *rm_prop;
#endif /* CONFIG_RT_REGMAP */
};

enum HL7139_adc_channel {
	HL7139_ADC_IBUS = 0,
	HL7139_ADC_VBUS,
	HL7139_ADC_VAC,
	HL7139_ADC_VOUT,
	HL7139_ADC_VBAT,
	HL7139_ADC_IBAT,
	HL7139_ADC_TSBUS,
	HL7139_ADC_TSBAT,
	HL7139_ADC_TDIE,
	HL7139_ADC_MAX,
	HL7139_ADC_NOTSUPP = HL7139_ADC_MAX,
};

static const u8 HL7139_adc_reg[HL7139_ADC_MAX] = {
	HL7139_REG_IBUSADC1,
	HL7139_REG_VBUSADC1,
	HL7139_REG_VACADC1,
	HL7139_REG_VOUTADC1,
	HL7139_REG_VBATADC1,
	HL7139_REG_IBATADC1,
	HL7139_REG_TSBUSADC1,
	HL7139_REG_TSBATADC1,
	HL7139_REG_TDIEADC1,
};

static const char *HL7139_adc_name[HL7139_ADC_MAX] = {
	"Ibus", "Vbus", "VAC", "Vout", "Vbat", "Ibat", "TSBus", "TSBat", "TDie",
};

static const u32 HL7139_adc_accuracy_tbl[HL7139_ADC_MAX] = {
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

static int HL7139_read_device(void *client, u32 addr, int len, void *dst)
{
	int ret;
	struct i2c_client *i2c = (struct i2c_client *)client;
	struct HL7139_chip *chip = i2c_get_clientdata(i2c);

	pm_stay_awake(chip->dev);
	mutex_lock(&chip->suspend_lock);
	ret = i2c_smbus_read_i2c_block_data(i2c, addr, len, dst);
	mutex_unlock(&chip->suspend_lock);
	pm_relax(chip->dev);
	return ret;
}

static int HL7139_write_device(void *client, u32 addr, int len, const void *src)
{
	int ret;
	struct i2c_client *i2c = (struct i2c_client *)client;
	struct HL7139_chip *chip = i2c_get_clientdata(i2c);

	pm_stay_awake(chip->dev);
	mutex_lock(&chip->suspend_lock);
	ret = i2c_smbus_write_i2c_block_data(i2c, addr, len, src);
	mutex_unlock(&chip->suspend_lock);
	pm_relax(chip->dev);
	return ret;
}

#ifdef CONFIG_RT_REGMAP
RT_REG_DECL(HL7139_REG_VBATOVP, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_VBATOVP_ALM, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_IBATOCP, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_IBATOCP_ALM, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_IBATUCP_ALM, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_ACPROTECT, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_VBUSOVP, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_VBUSOVP_ALM, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_IBUSOCUCP, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_IBUSOCP_ALM, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_CONVSTAT, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_CHGCTRL0, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_CHGCTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_INTSTAT, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_INTFLAG, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_INTMASK, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_FLTSTAT, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_FLTFLAG, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_FLTMASK, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_DEVINFO, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_ADCCTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_ADCEN, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_IBUSADC1, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_IBUSADC0, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_VBUSADC1, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_VBUSADC0, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_VACADC1, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_VACADC0, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_VOUTADC1, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_VOUTADC0, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_VBATADC1, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_VBATADC0, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_IBATADC1, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_IBATADC0, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_TSBUSADC1, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_TSBUSADC0, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_TSBATADC1, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_TSBATADC0, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_TDIEADC1, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_TDIEADC0, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_TSBUSOTP, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_TSBATOTP, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_TDIEALM, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_REGCTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_REGTHRES, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_REGFLAGMASK, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_BUSDEGLH, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_OTHER1, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_SYSCTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(HL7139_REG_PASSWORD0, 1, RT_VOLATILE, {});
//RT_REG_DECL(HL7139_REG_PASSWORD1, 1, RT_VOLATILE, {});

static const rt_register_map_t HL7139_regmap[] = {
	RT_REG(HL7139_REG_VBATOVP),
	RT_REG(HL7139_REG_VBATOVP_ALM),
	RT_REG(HL7139_REG_IBATOCP),
	RT_REG(HL7139_REG_IBATOCP_ALM),
	RT_REG(HL7139_REG_IBATUCP_ALM),
	RT_REG(HL7139_REG_ACPROTECT),
	RT_REG(HL7139_REG_VBUSOVP),
	RT_REG(HL7139_REG_VBUSOVP_ALM),
	RT_REG(HL7139_REG_IBUSOCUCP),
	RT_REG(HL7139_REG_IBUSOCP_ALM),
	RT_REG(HL7139_REG_CONVSTAT),
	RT_REG(HL7139_REG_CHGCTRL0),
	RT_REG(HL7139_REG_CHGCTRL1),
	RT_REG(HL7139_REG_INTSTAT),
	RT_REG(HL7139_REG_INTFLAG),
	RT_REG(HL7139_REG_INTMASK),
	RT_REG(HL7139_REG_FLTSTAT),
	RT_REG(HL7139_REG_FLTFLAG),
	RT_REG(HL7139_REG_FLTMASK),
	RT_REG(HL7139_REG_DEVINFO),
	RT_REG(HL7139_REG_ADCCTRL),
	RT_REG(HL7139_REG_ADCEN),
	RT_REG(HL7139_REG_IBUSADC1),
	RT_REG(HL7139_REG_IBUSADC0),
	RT_REG(HL7139_REG_VBUSADC1),
	RT_REG(HL7139_REG_VBUSADC0),
	RT_REG(HL7139_REG_VACADC1),
	RT_REG(HL7139_REG_VACADC0),
	RT_REG(HL7139_REG_VOUTADC1),
	RT_REG(HL7139_REG_VOUTADC0),
	RT_REG(HL7139_REG_VBATADC1),
	RT_REG(HL7139_REG_VBATADC0),
	RT_REG(HL7139_REG_IBATADC1),
	RT_REG(HL7139_REG_IBATADC0),
	RT_REG(HL7139_REG_TSBUSADC1),
	RT_REG(HL7139_REG_TSBUSADC0),
	RT_REG(HL7139_REG_TSBATADC1),
	RT_REG(HL7139_REG_TSBATADC0),
	RT_REG(HL7139_REG_TDIEADC1),
	RT_REG(HL7139_REG_TDIEADC0),
	RT_REG(HL7139_REG_TSBUSOTP),
	RT_REG(HL7139_REG_TSBATOTP),
	RT_REG(HL7139_REG_TDIEALM),
	RT_REG(HL7139_REG_REGCTRL),
	RT_REG(HL7139_REG_REGTHRES),
	RT_REG(HL7139_REG_REGFLAGMASK),
	RT_REG(HL7139_REG_BUSDEGLH),
	RT_REG(HL7139_REG_OTHER1),
	RT_REG(HL7139_REG_SYSCTRL1),
	
	RT_REG(HL7139_REG_PASSWORD0),
//	RT_REG(HL7139_REG_PASSWORD1),
};

static struct rt_regmap_fops HL7139_rm_fops = {
	.read_device = HL7139_read_device,
	.write_device = HL7139_write_device,
};

static int HL7139_register_regmap(struct HL7139_chip *chip)
{
	struct i2c_client *client = chip->client;
	struct rt_regmap_properties *prop = NULL;

	dev_info(chip->dev, "%s\n", __func__);

	prop = devm_kzalloc(&client->dev, sizeof(*prop), GFP_KERNEL);
	if (!prop)
		return -ENOMEM;

	prop->name = chip->desc->rm_name;
	prop->aliases = chip->desc->rm_name;
	prop->register_num = ARRAY_SIZE(HL7139_regmap);
	prop->rm = HL7139_regmap;
	prop->rt_regmap_mode = RT_SINGLE_BYTE | RT_CACHE_DISABLE |
			       RT_IO_PASS_THROUGH;
	prop->io_log_en = 0;

	chip->rm_prop = prop;
	chip->rm_dev = rt_regmap_device_register_ex(chip->rm_prop,
						    &HL7139_rm_fops, chip->dev,
						    client,
						    chip->desc->rm_slave_addr,
						    chip);
	if (!chip->rm_dev) {
		dev_notice(chip->dev, "%s register regmap dev fail\n",
			__func__);
		return -EINVAL;
	}

	return 0;
}
#endif /* CONFIG_RT_REGMAP */

#define I2C_ACCESS_MAX_RETRY	5
static inline int __HL7139_i2c_write8(struct HL7139_chip *chip, u8 reg, u8 data)
{
	int ret, retry = 0;
	
	do {
#ifdef CONFIG_RT_REGMAP
		ret = rt_regmap_block_write(chip->rm_dev, reg, 1, &data);
#else
		ret = HL7139_write_device(chip->client, reg, 1, &data);
#endif /* CONFIG_RT_REGMAP */
		retry++;
		if (ret < 0)
			usleep_range(10, 15);
	} while (ret < 0 && retry < I2C_ACCESS_MAX_RETRY);
	
	if (ret < 0) {
		dev_notice(chip->dev, "%s I2CW[0x%02X] = 0x%02X fail\n",
			__func__, reg, data);
			printk("%s I2CW[0x%02X] = 0x%02X fail\n",__func__,reg, data);
		return ret;
	}
	printk("%s I2CW[0x%02X] = 0x%02X\n",__func__,reg, data);
	dev_dbg_ratelimited(chip->dev, "%s I2CW[0x%02X] = 0x%02X\n", __func__,
			reg, data);
	return 0;
}

static int HL7139_i2c_write8(struct HL7139_chip *chip, u8 reg, u8 data)
{
	int ret;

	mutex_lock(&chip->io_lock);
	ret = __HL7139_i2c_write8(chip, reg, data);
	mutex_unlock(&chip->io_lock);

	return ret;
}

static inline int __HL7139_i2c_read8(struct HL7139_chip *chip, u8 reg, u8 *data)
{
	int ret, retry = 0;

	do {
#ifdef CONFIG_RT_REGMAP
		ret = rt_regmap_block_read(chip->rm_dev, reg, 1, data);
#else
		ret = HL7139_read_device(chip->client, reg, 1, data);
#endif /* CONFIG_RT_REGMAP */
		retry++;
		if (ret < 0)
			usleep_range(10, 15);
	} while (ret < 0 && retry < I2C_ACCESS_MAX_RETRY);

	if (ret < 0) {
		dev_notice(chip->dev, "%s I2CR[0x%02X] fail\n", __func__, reg);
		return ret;
	}
	dev_dbg_ratelimited(chip->dev, "%s I2CR[0x%02X] = 0x%02X\n", __func__,
			reg, *data);
	return 0;
}

int hl7139_dump_register(void)
{
	unsigned int status = 0;
	int i;
	unsigned char reg_val;

    printk("%s enter\n",__func__);

	for(i = 0; i < 0x4F; i++)
	{
		__HL7139_i2c_read8(chg_en_chip, i, &reg_val);
		chr_err("hl7139 reg[0x%x] = 0x%x\n", i, reg_val);
	}

	return status;
}
int hl7139_disable_charger(void)
{
    printk("%s enter\n",__func__);
    __HL7139_i2c_write8(chg_en_chip,HL7139_CTRL0 , 0x27);
    return 0;
}

static int HL7139_i2c_read8(struct HL7139_chip *chip, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&chip->io_lock);
	ret = __HL7139_i2c_read8(chip, reg, data);
	mutex_unlock(&chip->io_lock);

	return ret;
}

static inline int __HL7139_i2c_write_block(struct HL7139_chip *chip, u8 reg,
					   u32 len, const u8 *data)
{
	int ret;

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_write(chip->rm_dev, reg, len, data);
#else
	ret = HL7139_write_device(chip->client, reg, len, data);
#endif /* CONFIG_RT_REGMAP */

	return ret;
}

static int HL7139_i2c_write_block(struct HL7139_chip *chip, u8 reg, u32 len,
				  const u8 *data)
{
	int ret;

	mutex_lock(&chip->io_lock);
	ret = __HL7139_i2c_write_block(chip, reg, len, data);
	mutex_unlock(&chip->io_lock);

	return ret;
}

static inline int __HL7139_i2c_read_block(struct HL7139_chip *chip, u8 reg,
					  u32 len, u8 *data)
{
	int ret;

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_read(chip->rm_dev, reg, len, data);
#else
	ret = HL7139_read_device(chip->client, reg, len, data);
#endif /* CONFIG_RT_REGMAP */

	return ret;
}

static int HL7139_i2c_read_block(struct HL7139_chip *chip, u8 reg, u32 len,
				 u8 *data)
{
	int ret;

	mutex_lock(&chip->io_lock);
	ret = __HL7139_i2c_read_block(chip, reg, len, data);
	mutex_unlock(&chip->io_lock);

	return ret;
}
#if 0
static int HL7139_i2c_test_bit(struct HL7139_chip *chip, u8 reg, u8 shft,
			       bool *one)
{
	int ret;
	u8 data;

	ret = HL7139_i2c_read8(chip, reg, &data);
	if (ret < 0) {
		*one = false;
		return ret;
	}

	*one = (data & (1 << shft)) ? true : false;
	return 0;
}
#endif
static int HL7139_i2c_update_bits(struct HL7139_chip *chip, u8 reg, u8 data,
				  u8 mask)
{
	int ret;
	u8 _data;

	mutex_lock(&chip->io_lock);
	ret = __HL7139_i2c_read8(chip, reg, &_data);
	if (ret < 0)
		goto out;
	_data &= ~mask;
	_data |= (data & mask);
	ret = __HL7139_i2c_write8(chip, reg, _data);
out:
	mutex_unlock(&chip->io_lock);
	return ret;
}

//Antaiui <AI_BSP_CHG> <hehl> <2022-01-15> #37743 begin
static int HL7139_chg_en_update_bits(struct HL7139_chip *chip, u8 reg, u8 en)
{
    int ret;
	u8 data;

	mutex_lock(&chip->io_lock);
	ret = __HL7139_i2c_read8(chip, reg, &data);
	if (ret < 0)
		goto out;
	data |=  en << 7;
	ret = __HL7139_i2c_write8(chip, reg, data);
out:
	mutex_unlock(&chip->io_lock);
	return ret;
}    
//Antaiui <AI_BSP_CHG> <hehl> <2022-01-15> #37743 end

static inline int HL7139_set_bits(struct HL7139_chip *chip, u8 reg, u8 mask)
{
	return HL7139_i2c_update_bits(chip, reg, mask, mask);
}

static inline int HL7139_clr_bits(struct HL7139_chip *chip, u8 reg, u8 mask)
{
	return HL7139_i2c_update_bits(chip, reg, 0x00, mask);
}

static inline u8 HL7139_val_toreg(u32 min, u32 max, u32 step, u32 target,
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

static inline u8 HL7139_val_toreg_via_tbl(const u32 *tbl, int tbl_size,
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

static u8 HL7139_vbatovp_toreg(u32 uV)
{
	return HL7139_val_toreg(3500000, 5075000, 25000, uV, false);
}

static u8 HL7139_ibatocp_toreg(u32 uA)
{
	return HL7139_val_toreg(2000000, 10000000, 100000, uA, true);
}

static u8 HL7139_ibatucp_toreg(u32 uA)
{
	return HL7139_val_toreg(0, 6350000, 50000, uA, false);
}

static u8 HL7139_vbusovp_toreg(u32 uV)
{
	return HL7139_val_toreg(6000000, 12350000, 50000, uV, true);
}

static u8 HL7139_ibusocp_toreg(u32 uA)
{
	return HL7139_val_toreg(1000000, 4750000, 250000, uA, true);
}

static u8 HL7139_ibusocp_alm_toreg(u32 uA)
{
	return HL7139_val_toreg(0, 6350000, 50000, uA, true);
}

static const u32 HL7139_wdt[] = {
	500000, 1000000, 5000000, 3000000,
};

static u8 HL7139_wdt_toreg(u32 uS)
{
	return HL7139_val_toreg_via_tbl(HL7139_wdt, ARRAY_SIZE(HL7139_wdt), uS);
}

static u8 HL7139_vacovp_toreg(u32 uV)
{
	if (uV < 11000000)
		return 0x07;
	return HL7139_val_toreg(11000000, 17000000, 1000000, uV, true);
}

static int __HL7139_update_status(struct HL7139_chip *chip);
static int __HL7139_init_chip(struct HL7139_chip *chip);

static const u8 HL7139_hm_password[1] = {0xF9,};
static int __maybe_unused __HL7139_enter_hidden_mode(struct HL7139_chip *chip,
						     bool en)
{
	int ret = 0;

	mutex_lock(&chip->hm_lock);

	if (en) {
		if (chip->hm_cnt == 0) {
			ret = HL7139_i2c_write_block(chip, HL7139_REG_PASSWORD0,
						     2, HL7139_hm_password);
			if (ret < 0)
				goto err;
		}
		chip->hm_cnt++;
	} else {
		if (chip->hm_cnt == 1) /* last one */
			ret = HL7139_i2c_write8(chip, HL7139_REG_PASSWORD0,
						0x00);
		if (chip->hm_cnt > 0)
			chip->hm_cnt--;
		if (ret < 0)
			goto err;
	}
	dev_info(chip->dev, "%s en = %d\n", __func__, en);
	goto out;

err:
	dev_notice(chip->dev, "%s en = %d fail(%d)\n", __func__, en, ret);
out:
	mutex_unlock(&chip->hm_lock);
	return ret;
}
#if 0
/* Must be called while holding a lock */
static int HL7139_enable_wdt(struct HL7139_chip *chip, bool en)
{
	int ret;

	if (chip->wdt_en == en)
		return 0;
	ret = (en ? HL7139_clr_bits : HL7139_set_bits)
		(chip, HL7139_CTRL2, HL7139_WDTEN_MASK);
	if (ret < 0)
		return ret;
	chip->wdt_en = en;
	return 0;
}
#endif
static int __HL7139_get_adc(struct HL7139_chip *chip,
			    enum HL7139_adc_channel chan, int *val)
{
//Antaiui <AI_BSP_CHG> <hehl> <2021-06-09> Probabilistic not up to 9v begin
	int ret=0;
	u8 data[2];
	dev_info(chip->dev, "%s chan=%d\n", __func__,chan);			
		
	if (chan == HL7139_ADC_IBAT) {
		*val = battery_get_bat_current() * 100;
		if(*val < 0)
			*val = 10;
		dev_info(chip->dev, "HL7139 %s %s %d\n", __func__,
			 HL7139_adc_name[chan], *val);
		goto out;
	}
//Antaiui <AI_BSP_CHG> <hehl> <2021-06-09> Probabilistic not up to 9v end	

	//ret = HL7139_set_bits(chip, HL7139_REG_ADCCTRL, HL7139_ADCEN_MASK);
	if (ret < 0)
		goto out;
	usleep_range(12000, 15000);
	
	ret = HL7139_i2c_read_block(chip, HL7139_adc_reg[chan], 2, data);
	if (ret < 0)
		goto out_dis;
	//Antai <AI_BSP_Charger> <yaoyc> <2021-11-16> remove float data for ddr test begin 
	switch (chan) {
	case HL7139_ADC_IBUS:
		*val = ((data[0] * 16) + data[1]) * 1100;	
		break;
	case HL7139_ADC_VBUS:
		*val = ((data[0] * 16) + data[1]) * 4 * 1000;
		break;
	case HL7139_ADC_VAC:
	case HL7139_ADC_VOUT:
	case HL7139_ADC_VBAT:
		*val = ((data[0] * 16) + data[1]) * 1250;
		break;
	case HL7139_ADC_IBAT:
		*val = ((data[0] * 16) + data[1])  * 2200;
		break;
	case HL7139_ADC_TDIE:
		*val = ((data[0] * 16) + data[1]) / 16;  
		break;
	//Antai <AI_BSP_Charger> <yaoyc> <2021-11-16> remove float data for ddr test end
	case HL7139_ADC_TSBAT:
	case HL7139_ADC_TSBUS:
	default:
		ret = -ENOTSUPP;
		break;
	}
	if (ret < 0)
		dev_notice(chip->dev, "%s %s fail(%d)\n", __func__,
			HL7139_adc_name[chan], ret);
	else
		dev_info(chip->dev, "%s %s %d %d %d\n", __func__,
			 HL7139_adc_name[chan], data[0],data[1],*val);
out_dis:
	if (!chip->force_adc_en)
		ret = HL7139_clr_bits(chip, HL7139_REG_ADC_CTRL1,
				      HL7139_ADCEN_MASK);
out:
	return ret;
}

//Antaiui <AI_BSP_USB> <hehl> <2021-03-16> add uisoc limit charge interface #37743 begin

void HL7139_chg_en_config(u8 en)
{
	if(en)
		HL7139_chg_en_update_bits(chg_en_chip,HL7139_CTRL0,en);
	else
		HL7139_chg_en_update_bits(chg_en_chip,HL7139_CTRL0,en);
	printk("hl7139 chg en is %d\n",en);
}
//Antaiui <AI_BSP_USB> <hehl> <2021-03-16> add uisoc limit charge interface #37743 end

static int HL7139_enable_chg(struct charger_device *chg_dev, bool en)
{
	int ret;
	struct HL7139_chip *chip = charger_get_data(chg_dev);
	dev_info(chip->dev, "%s %d\n", __func__, en);

    if(en)
       ret = __HL7139_i2c_write8(chip,HL7139_CTRL0 , 0xA7);
   else
       ret = __HL7139_i2c_write8(chip,HL7139_CTRL0 , 0x27);
	return ret;
}

static int HL7139_is_chg_enabled(struct charger_device *chg_dev, bool *en)
{
#if 0
	int ret;
	struct HL7139_chip *chip = charger_get_data(chg_dev);

	ret = HL7139_i2c_test_bit(chip, HL7139_REG_CHGCTRL1, HL7139_CHGEN_SHFT,
				   en);
	dev_info(chip->dev, "%s %d, ret(%d)\n", __func__, *en, ret);
	return ret;
#endif
      return 0;
}

static inline enum HL7139_adc_channel to_HL7139_adc(enum adc_channel chan)
{	
	switch (chan) {
	case ADC_CHANNEL_VBUS:  //0
		return HL7139_ADC_VBUS;
	case ADC_CHANNEL_VBAT:   //2
		return HL7139_ADC_VBAT;
	case ADC_CHANNEL_IBUS:   //3
		return HL7139_ADC_IBUS;
	case ADC_CHANNEL_IBAT: //4
		return HL7139_ADC_IBAT;
	case ADC_CHANNEL_TEMP_JC: //5
		return HL7139_ADC_TDIE;
	case ADC_CHANNEL_VOUT:  //9
		return HL7139_ADC_VOUT;
	default:
		break;
	}
	return HL7139_ADC_NOTSUPP;
}

static int HL7139_get_adc(struct charger_device *chg_dev, enum adc_channel chan,
			  int *min, int *max)
{
	int ret;
	struct HL7139_chip *chip = charger_get_data(chg_dev);
	enum HL7139_adc_channel _chan = to_HL7139_adc(chan);
#if 0
	u8 data;
	int i=0;
	for(i=0;i<0x19;i++)
	{
		__HL7139_i2c_read8(chip, i, &data);
		dev_info(chip->dev, "%s read register 0x%x=0x%x\n", __func__,i,data);
	}
	for(i=0x40;i<0x50;i++)
	{
		__HL7139_i2c_read8(chip, i, &data);
		dev_info(chip->dev, "%s read register 0x%x=0x%x\n", __func__,i,data);
	}
#endif	
	if (_chan == HL7139_ADC_NOTSUPP)
	{
		dev_info(chip->dev, "%s _chan error\n", __func__);
		return -EINVAL;
	}	
	mutex_lock(&chip->adc_lock);
	ret = __HL7139_get_adc(chip, _chan, max);
	if (ret < 0)
		goto out;
	if (min != max)
		*min = *max;
out:
	mutex_unlock(&chip->adc_lock);
	return ret;
}

static int HL7139_get_adc_accuracy(struct charger_device *chg_dev,
				   enum adc_channel chan, int *min, int *max)
{
	enum HL7139_adc_channel _chan = to_HL7139_adc(chan);

	if (_chan == HL7139_ADC_NOTSUPP)
		return -EINVAL;
	*min = *max = HL7139_adc_accuracy_tbl[_chan];
	return 0;
}

/////////////////////////////////////////////////////////////////////////
static int HL7139_set_vbusovp(struct charger_device *chg_dev, u32 uV)
{
#if 1
	struct HL7139_chip *chip = charger_get_data(chg_dev);
	
	u8 reg = HL7139_vbusovp_toreg(uV);
	dev_info(chip->dev, "%s %d(0x%02X)\n", __func__, uV, reg);
	
	return HL7139_i2c_update_bits(chip, HL7139_REG_VBUSOVP, reg,
				      HL7139_VBUSOVP_MASK);
#endif
     return 0;
}

static int HL7139_set_ibusocp(struct charger_device *chg_dev, u32 uA)
{
#if 0
	struct HL7139_chip *chip = charger_get_data(chg_dev);
	u8 reg = HL7139_ibusocp_toreg(uA);

	dev_info(chip->dev, "%s %d(0x%02X)\n", __func__, uA, reg);
	return HL7139_i2c_update_bits(chip, HL7139_REG_IBUSOCUCP, reg,
				      HL7139_IBUSOCP_MASK);
#endif 
     return 0;
}

static int HL7139_set_vbatovp(struct charger_device *chg_dev, u32 uV)
{
#if 0
	struct HL7139_chip *chip = charger_get_data(chg_dev);
	u8 reg = HL7139_vbatovp_toreg(uV);

	dev_info(chip->dev, "%s %d(0x%02X)\n", __func__, uV, reg);
	return HL7139_i2c_update_bits(chip, HL7139_REG_VBATOVP, reg,
				      HL7139_VBATOVP_MASK);
#endif 
      return 0;
}

static int HL7139_set_vbatovp_alarm(struct charger_device *chg_dev, u32 uV)
{
#if 0
	struct HL7139_chip *chip = charger_get_data(chg_dev);
	u8 reg = HL7139_vbatovp_toreg(uV);

	dev_info(chip->dev, "%s %d\n", __func__, uV);
	return HL7139_i2c_update_bits(chip, HL7139_REG_VBATOVP_ALM, reg,
				      HL7139_VBATOVP_ALM_MASK);
#endif 
       return 0;
}
///////////////////////////////////////////////////////////////////////////////



static int HL7139_reset_vbatovp_alarm(struct charger_device *chg_dev)
{
#if 0
	int ret;
	struct HL7139_chip *chip = charger_get_data(chg_dev);
	u8 data;

	dev_info(chip->dev, "%s\n", __func__);
	mutex_lock(&chip->io_lock);
	ret = __HL7139_i2c_read8(chip, HL7139_REG_VBATOVP_ALM, &data);
	if (ret < 0)
		goto out;
	data |= HL7139_VBATOVP_ALMDIS_MASK;
	ret = __HL7139_i2c_write8(chip, HL7139_REG_VBATOVP_ALM, data);
	if (ret < 0)
		goto out;
	data &= ~HL7139_VBATOVP_ALMDIS_MASK;
	ret = __HL7139_i2c_write8(chip, HL7139_REG_VBATOVP_ALM, data);
out:
	mutex_unlock(&chip->io_lock);
	return ret;
#endif
       return 0;
}

static int HL7139_set_vbusovp_alarm(struct charger_device *chg_dev, u32 uV)
{
#if 0
	struct HL7139_chip *chip = charger_get_data(chg_dev);
	u8 reg = HL7139_vbusovp_toreg(uV);

	dev_info(chip->dev, "%s %d\n", __func__, uV);
	return HL7139_i2c_update_bits(chip, HL7139_REG_VBUSOVP_ALM, reg,
				      HL7139_VBUSOVP_ALM_MASK);
#endif
     return 0;
}

static int HL7139_is_vbuslowerr(struct charger_device *chg_dev, bool *err)
{
#if 0
       int ret;
	struct HL7139_chip *chip = charger_get_data(chg_dev);

	mutex_lock(&chip->adc_lock);
	ret = HL7139_set_bits(chip, HL7139_REG_ADCCTRL, HL7139_ADCEN_MASK);
	if (ret < 0)
		goto out;
	usleep_range(12000, 15000);
	ret = HL7139_i2c_test_bit(chip, HL7139_REG_OTHER1,
				  HL7139_VBUSLOWERR_FLAG_SHFT, err);
	if (ret < 0 || *err)
		goto out_dis;
	ret = HL7139_i2c_test_bit(chip, HL7139_REG_CONVSTAT,
				  HL7139_VBUSLOWERR_STAT_SHFT, err);//zhanggeng HL7130 the diff valve seting 
out_dis:
	if (!chip->force_adc_en)
		HL7139_clr_bits(chip, HL7139_REG_ADCCTRL, HL7139_ADCEN_MASK);
out:
	mutex_unlock(&chip->adc_lock);
	return ret;
#endif
		*err = 0;
       return 0;
}

static int HL7139_reset_vbusovp_alarm(struct charger_device *chg_dev)
{
#if 0
	int ret;
	struct HL7139_chip *chip = charger_get_data(chg_dev);
	u8 data;

	dev_info(chip->dev, "%s\n", __func__);
	mutex_lock(&chip->io_lock);
	ret = __HL7139_i2c_read8(chip, HL7139_REG_VBUSOVP_ALM, &data);
	if (ret < 0)
		goto out;
	data |= HL7139_VBUSOVP_ALMDIS_MASK;
	ret = __HL7139_i2c_write8(chip, HL7139_REG_VBUSOVP_ALM, data);
	if (ret < 0)
		goto out;
	data &= ~HL7139_VBUSOVP_ALMDIS_MASK;
	ret = __HL7139_i2c_write8(chip, HL7139_REG_VBUSOVP_ALM, data);
out:
	mutex_unlock(&chip->io_lock);
	return ret;
#endif
      return 0;
}

static int HL7139_set_ibatocp(struct charger_device *chg_dev, u32 uA)
{
#if 0
	struct HL7139_chip *chip = charger_get_data(chg_dev);
	u8 reg = HL7139_ibatocp_toreg(uA);

	dev_info(chip->dev, "%s %d(0x%02X)\n", __func__, uA, reg);
	return HL7139_i2c_update_bits(chip, HL7139_REG_IBATOCP, reg,
				      HL7139_IBATOCP_MASK);
#endif 
     return 0;
}

static int HL7139_init_chip(struct charger_device *chg_dev)
{

#if 0
	int i, ret;
	struct HL7139_chip *chip = charger_get_data(chg_dev);
	const struct HL7139_reg_defval *reg_defval;
	u8 val;

	for (i = 0; i < ARRAY_SIZE(HL7139_init_chip_check_reg); i++) {
		reg_defval = &HL7139_init_chip_check_reg[i];
		ret = HL7139_i2c_read8(chip, reg_defval->reg, &val);
		if (ret < 0)
			return ret;
		if ((val & reg_defval->mask) == reg_defval->value) {
			dev_info(chip->dev,
				"%s chip reset happened, reinit\n", __func__);
			return __HL7139_init_chip(chip);
		}
	}
#endif

	return 0;
}

static int HL7139_vacovp_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static inline void HL7139_set_notify(struct HL7139_chip *chip,
				     enum HL7139_notify notify)
{
	mutex_lock(&chip->notify_lock);
	chip->notify |= BIT(notify);
	mutex_unlock(&chip->notify_lock);
}

static int HL7139_ibusucpf_irq_handler(struct HL7139_chip *chip)
{
	bool ucpf = !!(chip->stat & BIT(HL7139_IRQIDX_IBUSUCPF));

	dev_info(chip->dev, "%s %d\n", __func__, ucpf);
	if (ucpf)
		HL7139_set_notify(chip, HL7139_NOTIFY_IBUSUCPF);
	return 0;
}

static int HL7139_ibusucpr_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s %d\n", __func__,
		 !!(chip->stat & BIT(HL7139_IRQIDX_IBUSUCPR)));
	return 0;
}

static int HL7139_cflydiag_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int HL7139_conocp_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int HL7139_switching_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s %d\n", __func__,
		 !!(chip->stat & BIT(HL7139_IRQIDX_SWITCHING)));
	return 0;
}

static int HL7139_ibusucptout_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int HL7139_vbushigherr_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s %d\n", __func__,
		 !!(chip->stat & BIT(HL7139_IRQIDX_VBUSHERR)));
	return 0;
}

static int HL7139_vbuslowerr_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s %d\n", __func__,
		 !!(chip->stat & BIT(HL7139_IRQIDX_VBUSLERR)));
	return 0;
}

static int HL7139_tdieotp_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int HL7139_wdt_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int HL7139_adcdone_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int HL7139_voutinsert_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s %d\n", __func__,
		 !!(chip->stat & BIT(HL7139_IRQIDX_VOUTINSERT)));
	return 0;
}

static int HL7139_vacinsert_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s %d\n", __func__,
		 !!(chip->stat & BIT(HL7139_IRQIDX_VACINSERT)));
	return 0;
}

static int HL7139_ibatucpalm_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int HL7139_ibusocpalm_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int HL7139_vbusovpalm_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	HL7139_set_notify(chip, HL7139_NOTIFY_VBUSOVPALM);
	return 0;
}

static int HL7139_ibatocpalm_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int HL7139_vbatovpalm_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	HL7139_set_notify(chip, HL7139_NOTIFY_VBATOVPALM);
	return 0;
}

static int HL7139_tdieotpalm_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}
//Antaiui <AI_BSP_CHG> <hehl> <2021-03-01> modify For 2005R 30w fast charger start
#if 0
static int HL7139_tsbusotp_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int HL7139_tsbatotp_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int HL7139_tsbusbatotpalm_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}
#endif
//Antaiui <AI_BSP_CHG> <hehl> <2021-03-01> modify For 2005R 30w fast charger end
static int HL7139_ibusocp_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	//HL7139_set_notify(chip, HL7139_NOTIFY_IBUSOCP);
	return 0;
}

static int HL7139_vbusovp_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	//HL7139_set_notify(chip, HL7139_NOTIFY_VBUSOVP);
	return 0;
}

static int HL7139_ibatocp_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	//HL7139_set_notify(chip, HL7139_NOTIFY_IBATOCP);
	return 0;
}

static int HL7139_vbatovp_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	//HL7139_set_notify(chip, HL7139_NOTIFY_VBATOVP);
	return 0;
}

static int HL7139_voutovp_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	//HL7139_set_notify(chip, HL7139_NOTIFY_VOUTOVP);
	return 0;
}

static int HL7139_vdrovp_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	//HL7139_set_notify(chip, HL7139_NOTIFY_VDROVP);
	return 0;
}

static int HL7139_ibatreg_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int HL7139_vbatreg_irq_handler(struct HL7139_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

struct irq_map_desc {
	const char *name;
	int (*hdlr)(struct HL7139_chip *chip);
	u8 flag_idx;
	u8 stat_idx;
	u8 flag_mask;
	u8 stat_mask;
	u32 irq_idx;
	bool stat_only;
};

#define HL7139_IRQ_DESC(_name, _flag_i, _stat_i, _flag_s, _stat_s, _irq_idx, \
			_stat_only) \
	{.name = #_name, .hdlr = HL7139_##_name##_irq_handler, \
	 .flag_idx = _flag_i, .stat_idx = _stat_i, \
	 .flag_mask = (1 << _flag_s), .stat_mask = (1 << _stat_s), \
	 .irq_idx = _irq_idx, .stat_only = _stat_only}

/*
 * RSS: Reister index of flag, Shift of flag, Shift of state
 * RRS: Register index of flag, Register index of state, Shift of flag
 * RS: Register index of flag, Shift of flag
 * RSSO: Register index of state, Shift of state, State Only
 */
#define HL7139_IRQ_DESC_RSS(_name, _flag_i, _flag_s, _stat_s, _irq_idx) \
	HL7139_IRQ_DESC(_name, _flag_i, _flag_i, _flag_s, _stat_s, _irq_idx, \
			false)

#define HL7139_IRQ_DESC_RRS(_name, _flag_i, _stat_i, _flag_s, _irq_idx) \
	HL7139_IRQ_DESC(_name, _flag_i, _stat_i, _flag_s, _flag_s, _irq_idx, \
			false)

#define HL7139_IRQ_DESC_RS(_name, _flag_i, _flag_s, _irq_idx) \
	HL7139_IRQ_DESC(_name, _flag_i, _flag_i, _flag_s, _flag_s, _irq_idx, \
			false)

#define HL7139_IRQ_DESC_RSSO(_name, _flag_i, _flag_s, _irq_idx) \
	HL7139_IRQ_DESC(_name, _flag_i, _flag_i, _flag_s, _flag_s, _irq_idx, \
			true)

static const struct irq_map_desc HL7139_irq_map_tbl[HL7139_IRQIDX_MAX] = {
	HL7139_IRQ_DESC_RSS(vacovp, HL7139_SF_ACPROTECT, 6, 7,
			    HL7139_IRQIDX_VACOVP),
	HL7139_IRQ_DESC_RS(ibusucpf, HL7139_SF_IBUSOCUCP, 4,
			   HL7139_IRQIDX_IBUSUCPF),
	HL7139_IRQ_DESC_RS(ibusucpr, HL7139_SF_IBUSOCUCP, 6,
			   HL7139_IRQIDX_IBUSUCPR),
	HL7139_IRQ_DESC_RS(cflydiag, HL7139_SF_CONVSTAT, 0,
			   HL7139_IRQIDX_CFLYDIAG),
	HL7139_IRQ_DESC_RS(conocp, HL7139_SF_CONVSTAT, 1, HL7139_IRQIDX_CONOCP),
	HL7139_IRQ_DESC_RSSO(switching, HL7139_SF_CONVSTAT, 2,
			     HL7139_IRQIDX_SWITCHING),
	HL7139_IRQ_DESC_RS(ibusucptout, HL7139_SF_CONVSTAT, 3,
			   HL7139_IRQIDX_IBUSUCPTOUT),
	HL7139_IRQ_DESC_RSSO(vbushigherr, HL7139_SF_CONVSTAT, 4,
			     HL7139_IRQIDX_VBUSHERR),
	HL7139_IRQ_DESC_RSSO(vbuslowerr, HL7139_SF_CONVSTAT, 5,
			     HL7139_IRQIDX_VBUSLERR),
	HL7139_IRQ_DESC_RSS(tdieotp, HL7139_SF_CONVSTAT, 7, 6,
			    HL7139_IRQIDX_TDIEOTP),
	HL7139_IRQ_DESC_RS(wdt, HL7139_SF_CHGCTRL0, 3, HL7139_IRQIDX_WDT),
	HL7139_IRQ_DESC_RRS(adcdone, HL7139_SF_INTFLAG, HL7139_SF_INTSTAT, 0,
			    HL7139_IRQIDX_ADCDONE),
	HL7139_IRQ_DESC_RRS(voutinsert, HL7139_SF_INTFLAG,
			    HL7139_SF_INTSTAT, 1, HL7139_IRQIDX_VOUTINSERT),
	HL7139_IRQ_DESC_RRS(vacinsert, HL7139_SF_INTFLAG, HL7139_SF_INTSTAT, 2,
			    HL7139_IRQIDX_VACINSERT),
	HL7139_IRQ_DESC_RRS(ibatucpalm, HL7139_SF_INTFLAG,
			    HL7139_SF_INTSTAT, 3, HL7139_IRQIDX_IBATUCPALM),
	HL7139_IRQ_DESC_RRS(ibusocpalm, HL7139_SF_INTFLAG,
			    HL7139_SF_INTSTAT, 4, HL7139_IRQIDX_IBUSOCPALM),
	HL7139_IRQ_DESC_RRS(vbusovpalm, HL7139_SF_INTFLAG,
			    HL7139_SF_INTSTAT, 5, HL7139_IRQIDX_VBUSOVPALM),
	HL7139_IRQ_DESC_RRS(ibatocpalm, HL7139_SF_INTFLAG,
			    HL7139_SF_INTSTAT, 6, HL7139_IRQIDX_IBATOCPALM),
	HL7139_IRQ_DESC_RRS(vbatovpalm, HL7139_SF_INTFLAG,
			    HL7139_SF_INTSTAT, 7, HL7139_IRQIDX_VBATOVPALM),
	HL7139_IRQ_DESC_RRS(tdieotpalm, HL7139_SF_FLTFLAG,
			    HL7139_SF_FLTSTAT, 0, HL7139_IRQIDX_TDIEOTPALM),
//Antaiui <AI_BSP_CHG> <hehl> <2021-03-01> modify For 2005R 30w fast charger start				
#if 0
	HL7139_IRQ_DESC_RRS(tsbusotp, HL7139_SF_FLTFLAG, HL7139_SF_FLTSTAT, 1,
			    HL7139_IRQIDX_TSBUSOTP),
	HL7139_IRQ_DESC_RRS(tsbatotp, HL7139_SF_FLTFLAG, HL7139_SF_FLTSTAT, 2,
			    HL7139_IRQIDX_TSBATOTP),
	HL7139_IRQ_DESC_RRS(tsbusbatotpalm, HL7139_SF_FLTFLAG,
			    HL7139_SF_FLTSTAT, 3, HL7139_IRQIDX_TSBUSBATOTPALM),
#endif
//Antaiui <AI_BSP_CHG> <hehl> <2021-03-01> modify For 2005R 30w fast charger end
	HL7139_IRQ_DESC_RRS(ibusocp, HL7139_SF_FLTFLAG, HL7139_SF_FLTSTAT, 4,
			    HL7139_IRQIDX_IBUSOCP),
	HL7139_IRQ_DESC_RRS(vbusovp, HL7139_SF_FLTFLAG, HL7139_SF_FLTSTAT, 5,
			    HL7139_IRQIDX_VBUSOVP),
	HL7139_IRQ_DESC_RRS(ibatocp, HL7139_SF_FLTFLAG, HL7139_SF_FLTSTAT, 6,
			    HL7139_IRQIDX_IBATOCP),
	HL7139_IRQ_DESC_RRS(vbatovp, HL7139_SF_FLTFLAG, HL7139_SF_FLTSTAT, 7,
			    HL7139_IRQIDX_VBATOVP),
	HL7139_IRQ_DESC(voutovp, HL7139_SF_REGFLAGMASK, HL7139_SF_REGTHRES,
			4, 0, HL7139_IRQIDX_VOUTOVP, false),
	HL7139_IRQ_DESC(vdrovp, HL7139_SF_REGFLAGMASK, HL7139_SF_REGTHRES,
			5, 1, HL7139_IRQIDX_VDROVP, false),
	HL7139_IRQ_DESC(ibatreg, HL7139_SF_REGFLAGMASK, HL7139_SF_REGTHRES,
			6, 2, HL7139_IRQIDX_IBATREG, false),
	HL7139_IRQ_DESC(vbatreg, HL7139_SF_REGFLAGMASK, HL7139_SF_REGTHRES,
			7, 3, HL7139_IRQIDX_VBATREG, false),
};

static int __HL7139_update_status(struct HL7139_chip *chip)
{
	int i;
	u8 sf[HL7139_SF_MAX] = {0};
	const struct irq_map_desc *desc;

	for (i = 0; i < HL7139_SF_MAX; i++)
		HL7139_i2c_read8(chip, HL7139_reg_sf[i], &sf[i]);

	for (i = 0; i < ARRAY_SIZE(HL7139_irq_map_tbl); i++) {

		desc = &HL7139_irq_map_tbl[i];
		
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

static int __maybe_unused HL7139_update_status(struct HL7139_chip *chip)
{
	int ret;

	mutex_lock(&chip->stat_lock);
	ret = __HL7139_update_status(chip);
	mutex_unlock(&chip->stat_lock);
	return ret;
}

static int HL7139_notify_task_threadfn(void *data)
{
	int i;
	struct HL7139_chip *chip = data;

	while (!kthread_should_stop()) {
		wait_event_interruptible(chip->wq, chip->notify != 0 ||
					 kthread_should_stop());
		if (kthread_should_stop())
			goto out;
		pm_stay_awake(chip->dev);
		mutex_lock(&chip->notify_lock);
		
		for (i = 0; i < HL7139_NOTIFY_MAX; i++) {
			if (chip->notify & BIT(i)) {
				chip->notify &= ~BIT(i);
				mutex_unlock(&chip->notify_lock);
				charger_dev_notify(chip->chg_dev,
						   HL7139_chgdev_notify_map[i]);
				mutex_lock(&chip->notify_lock);
			}
		}
		mutex_unlock(&chip->notify_lock);
		pm_relax(chip->dev);
	}
out:
	return 0;
}

static irqreturn_t HL7139_irq_handler(int irq, void *data)
{
	int i;
	struct HL7139_chip *chip = data;
	const struct irq_map_desc *desc;
	
	dev_info(chip->dev, "%s\n", __func__);
	
	pm_stay_awake(chip->dev);
	mutex_lock(&chip->stat_lock);
	__HL7139_update_status(chip);
	for (i = 0; i < ARRAY_SIZE(HL7139_irq_map_tbl); i++) {
		desc = &HL7139_irq_map_tbl[i];
		if ((chip->flag & (1 << desc->irq_idx)) && desc->hdlr)
			desc->hdlr(chip);
	}
	chip->flag = 0;
	wake_up_interruptible(&chip->wq);
	mutex_unlock(&chip->stat_lock);
	pm_relax(chip->dev);
	return IRQ_HANDLED;
}
//Antaiui <AI_BSP_CHG> <hehl> <2021-12-24> modify begin
static const struct charger_ops HL7139_chg_ops = {
	.enable = HL7139_enable_chg,
	.is_enabled = HL7139_is_chg_enabled,
	
	.get_adc = HL7139_get_adc,
	
	.set_vbusovp = HL7139_set_vbusovp,
	
	.set_ibusocp = HL7139_set_ibusocp,
	
	.set_vbatovp = HL7139_set_vbatovp,
	
	.set_ibatocp = HL7139_set_ibatocp,
	
	.init_chip = HL7139_init_chip,
	
/////////////////////////////////////////////////////////////
	.set_vbatovp_alarm = HL7139_set_vbatovp_alarm,
	.reset_vbatovp_alarm = HL7139_reset_vbatovp_alarm,
	.set_vbusovp_alarm = HL7139_set_vbusovp_alarm,
	.reset_vbusovp_alarm = HL7139_reset_vbusovp_alarm,
//////////////////////////////////////////////////////////
	
	.is_vbuslowerr = HL7139_is_vbuslowerr,	
	.get_adc_accuracy = HL7139_get_adc_accuracy,
};
//Antaiui <AI_BSP_CHG> <hehl> <2021-12-24> modify end
static int HL7139_register_chgdev(struct HL7139_chip *chip)
{
	chip->chg_prop.alias_name = chip->desc->chg_name;
	chip->chg_dev = charger_device_register(chip->desc->chg_name, chip->dev,
						chip, &HL7139_chg_ops,
						&chip->chg_prop);
	if (!chip->chg_dev)
		return -EINVAL;
	return 0;
}

static int HL7139_clearall_irq(struct HL7139_chip *chip)
{
	int i, ret;
	u8 data;

	for (i = 0; i < HL7139_SF_MAX; i++) {
		ret = HL7139_i2c_read8(chip, HL7139_reg_sf[i], &data);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int HL7139_init_irq(struct HL7139_chip *chip)
{
	int ret = 0, len = 0;
	char *name = NULL;

	dev_info(chip->dev, "%s\n", __func__);
	ret = HL7139_clearall_irq(chip);
	if (ret < 0) {
		dev_notice(chip->dev, "%s clr all irq fail(%d)\n",
			__func__, ret);
		return ret;
	}
	if (chip->type == HL7139_TYPE_SLAVE)
		return 0;

	len = strlen(chip->desc->chg_name);
	chip->irq = gpiod_to_irq(chip->irq_gpio);
	if (chip->irq < 0) {
		dev_notice(chip->dev, "%s irq mapping fail(%d)\n", __func__,
			chip->irq);
		return ret;
	}
	dev_info(chip->dev, "%s irq = %d\n", __func__, chip->irq);

	/* Request threaded IRQ */
	name = devm_kzalloc(chip->dev, len + 5, GFP_KERNEL);
	snprintf(name, len + 5, "%s_irq", chip->desc->chg_name);
	ret = devm_request_threaded_irq(chip->dev, chip->irq, NULL,
		HL7139_irq_handler, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, name,
		chip);
	if (ret < 0) {
		dev_notice(chip->dev, "%s request thread irq fail(%d)\n",
			__func__, ret);
		return ret;
	}
	device_init_wakeup(chip->dev, true);
	return 0;
}

#define HL7139_DT_VALPROP(name, reg, shft, mask, func, base) \
	{#name, offsetof(struct HL7139_dese, name), reg, shft, mask, func, base}

struct HL7139_dtprop {
	const char *name;
	size_t offset;
	u8 reg;
	u8 shft;
	u8 mask;
	u8 (*toreg)(u32 val);
	u8 base;
};

static inline void HL7139_parse_dt_u32(struct device_node *np, void *desc,
				       const struct HL7139_dtprop *props,
				       int prop_cnt)
{
	int i;

	for (i = 0; i < prop_cnt; i++) {
		if (unlikely(!props[i].name))
			continue;
		of_property_read_u32(np, props[i].name, desc + props[i].offset);
	}
}

static inline void HL7139_parse_dt_bool(struct device_node *np, void *desc,
					const struct HL7139_dtprop *props,
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

static inline int HL7139_apply_dt(struct HL7139_chip *chip, void *desc,
				  const struct HL7139_dtprop *props,
				  int prop_cnt)
{
	int i, ret;
	u32 val;

	for (i = 0; i < prop_cnt; i++) {
		val = *(u32 *)(desc + props[i].offset);
		if (props[i].toreg)
			val = props[i].toreg(val);
		val += props[i].base;
		ret = HL7139_i2c_update_bits(chip, props[i].reg,
					     val << props[i].shft,
					     props[i].mask);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static const struct HL7139_dtprop HL7139_dtprops_u32[] = {
	HL7139_DT_VALPROP(vbatovp, HL7139_REG_VBATOVP, 0, 0x3f,
			  HL7139_vbatovp_toreg, 0),
	HL7139_DT_VALPROP(vbatovp_alm, HL7139_REG_VBATOVP_ALM, 0, 0x3f,
			  HL7139_vbatovp_toreg, 0),
	HL7139_DT_VALPROP(ibatocp, HL7139_REG_IBATOCP, 0, 0x7f,
			  HL7139_ibatocp_toreg, 0),
	HL7139_DT_VALPROP(ibatocp_alm, HL7139_REG_IBATOCP_ALM, 0, 0x7f,
			  HL7139_ibatocp_toreg, 0),
	HL7139_DT_VALPROP(ibatucp_alm, HL7139_REG_IBATUCP_ALM, 0, 0x7f,
			  HL7139_ibatucp_toreg, 0),
	HL7139_DT_VALPROP(vbusovp, HL7139_REG_VBUSOVP, 0, 0x7f,
			  HL7139_vbusovp_toreg, 0),
	HL7139_DT_VALPROP(vbusovp_alm, HL7139_REG_VBUSOVP_ALM, 0, 0x7f,
			  HL7139_vbusovp_toreg, 0),
	HL7139_DT_VALPROP(ibusocp, HL7139_REG_IBUSOCUCP, 0, 0x0f,
			  HL7139_ibusocp_toreg, 0),
	HL7139_DT_VALPROP(ibusocp_alm, HL7139_REG_IBUSOCP_ALM, 0, 0x7f,
			  HL7139_ibusocp_alm_toreg, 0),
	HL7139_DT_VALPROP(wdt, HL7139_REG_CHGCTRL0, 0, 0x03,
			  HL7139_wdt_toreg, 0),
	HL7139_DT_VALPROP(vacovp, HL7139_REG_ACPROTECT, 0, 0x07,
			  HL7139_vacovp_toreg, 0),
	HL7139_DT_VALPROP(ibat_rsense, HL7139_REG_REGCTRL, 1, 0x02, NULL, 0),
	HL7139_DT_VALPROP(ibusucpf_deglitch, HL7139_REG_BUSDEGLH, 3, 0x08, NULL,
			  0),
};

static const struct HL7139_dtprop HL7139_dtprops_bool[] = {
	//ÐèÒª×ö¶ÔÓ¦µÄÐÞ¸Ä
	HL7139_DT_VALPROP(vbatovp_dis, HL7139_REG_VBATOVP, 7, 0x80, NULL, 0),
		
	HL7139_DT_VALPROP(vbatovp_alm_dis, HL7139_REG_VBATOVP_ALM, 7, 0x80,NULL, 0),
	
	HL7139_DT_VALPROP(ibatocp_dis, HL7139_REG_IBATOCP, 7, 0x80, NULL, 0),
	
	HL7139_DT_VALPROP(ibatocp_alm_dis, HL7139_REG_IBATOCP_ALM, 7, 0x80,NULL, 0),
	
	HL7139_DT_VALPROP(ibatucp_alm_dis, HL7139_REG_IBATUCP_ALM, 7, 0x80,NULL, 0),
	
	HL7139_DT_VALPROP(vbusovp_alm_dis, HL7139_REG_VBUSOVP_ALM, 7, 0x80,NULL, 0),
	
	HL7139_DT_VALPROP(ibusocp_dis, HL7139_REG_IBUSOCUCP, 7, 0x80, NULL, 0),
	
	HL7139_DT_VALPROP(ibusocp_alm_dis, HL7139_REG_IBUSOCP_ALM, 7, 0x80,NULL, 0),
	
	HL7139_DT_VALPROP(wdt_dis, HL7139_REG_CHGCTRL0, 2, 0x04, NULL, 0),
	
	HL7139_DT_VALPROP(tsbusotp_dis, HL7139_REG_CHGCTRL1, 2, 0x04, NULL, 0),
	
	HL7139_DT_VALPROP(tsbatotp_dis, HL7139_REG_CHGCTRL1, 1, 0x02, NULL, 0),
	
	HL7139_DT_VALPROP(tdieotp_dis, HL7139_REG_CHGCTRL1, 0, 0x01, NULL, 0),
	
	HL7139_DT_VALPROP(reg_en, HL7139_REG_REGCTRL, 4, 0x10, NULL, 0),
	
	HL7139_DT_VALPROP(voutovp_dis, HL7139_REG_REGCTRL, 3, 0x08, NULL, 0),
	
	HL7139_DT_VALPROP(ibusadc_dis, HL7139_REG_ADCCTRL, 0, 0x01, NULL, 0),
	
	HL7139_DT_VALPROP(tdieadc_dis, HL7139_REG_ADCEN, 0, 0x01, NULL, 0),
	
	HL7139_DT_VALPROP(tsbatadc_dis, HL7139_REG_ADCEN, 1, 0x02, NULL, 0),
	
	HL7139_DT_VALPROP(tsbusadc_dis, HL7139_REG_ADCEN, 2, 0x04, NULL, 0),
	
	HL7139_DT_VALPROP(ibatadc_dis, HL7139_REG_ADCEN, 3, 0x08, NULL, 0),
	
	HL7139_DT_VALPROP(vbatadc_dis, HL7139_REG_ADCEN, 4, 0x10, NULL, 0),
	
	HL7139_DT_VALPROP(voutadc_dis, HL7139_REG_ADCEN, 5, 0x20, NULL, 0),
	
	HL7139_DT_VALPROP(vacadc_dis, HL7139_REG_ADCEN, 6, 0x40, NULL, 0),
	
	HL7139_DT_VALPROP(vbusadc_dis, HL7139_REG_ADCEN, 7, 0x80, NULL, 0),
};

static int HL7139_parse_dt(struct HL7139_chip *chip)
{
	struct HL7139_dese *desc;
	struct device_node *np = chip->dev->of_node;
	struct device_node *child_np;
	dev_info(chip->dev, "%s start\n", __func__);
	if (!np)
		return -ENODEV;

	//if (chip->type == HL7139_TYPE_SLAVE)
	//	goto ignore_intr;
	dev_info(chip->dev, "%s np\n", __func__);


//ignore_intr:
	desc = devm_kzalloc(chip->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	memcpy(desc, &HL7139_dese_defval, sizeof(*desc));
	if (of_property_read_string(np, "rm_name", &desc->rm_name) < 0)
		dev_info(chip->dev, "%s no rm name\n", __func__);
	
	if(strcmp(desc->rm_name, "rt9759_slave") == 0)
	{
		chip->type = 1;
	}
	if(strcmp(desc->rm_name, "rt9759_master") == 0)
	{
		chip->type = 2;
	}
	if(strcmp(desc->rm_name, "rt9759_standalone") == 0)
	{
		chip->type = 0;
	}
	dev_info(chip->dev, "rm name=%s\n", desc->rm_name);
	if((chip->type == 2)||(chip->type == 0))
	{
		chip->irq_gpio = devm_gpiod_get(chip->dev, "rt9759,intr", GPIOD_IN);
		if (IS_ERR(chip->irq_gpio))
			return PTR_ERR(chip->irq_gpio);
	}
	if (of_property_read_u8(np, "rm_slave_addr", &desc->rm_slave_addr) < 0)
		dev_info(chip->dev, "%s no regmap slave addr\n", __func__);
	child_np = of_get_child_by_name(np, HL7139_type_name[chip->type]);
	
	if (!child_np) {
		dev_info(chip->dev, "%s no node(%s) found\n", __func__,
			HL7139_type_name[chip->type]);
		return -ENODEV;
	}
	if (of_property_read_string(child_np, "chg_name", &desc->chg_name) < 0)
		dev_info(chip->dev, "%s no chg name\n", __func__);
	
	HL7139_parse_dt_u32(child_np, (void *)desc, HL7139_dtprops_u32,
			    ARRAY_SIZE(HL7139_dtprops_u32));
	
	HL7139_parse_dt_bool(child_np, (void *)desc, HL7139_dtprops_bool,
			     ARRAY_SIZE(HL7139_dtprops_bool));
	chip->desc = desc;
	return 0;
}

static int __HL7139_init_chip(struct HL7139_chip *chip)
{
	//int ret;

	dev_info(chip->dev, "%s\n", __func__);
//Antaiui <AI_BSP_CHG> <hehl> <2021-03-01> modify For 2005R 30w fast charger start
//	ret = HL7139_i2c_write8(chip, HL7139_REG_PMID_VOUT_UV_OV,0x0E);
//Antaiui <AI_BSP_CHG> <hehl> <2021-03-01> modify For 2005R 30w fast charger end
#if 0
	ret = HL7139_apply_dt(chip, (void *)chip->desc, HL7139_dtprops_u32,
			      ARRAY_SIZE(HL7139_dtprops_u32));
	if (ret < 0)
		return ret;
	ret = HL7139_apply_dt(chip, (void *)chip->desc, HL7139_dtprops_bool,
			      ARRAY_SIZE(HL7139_dtprops_bool));
	if (ret < 0)
		return ret;
	
	chip->wdt_en = !chip->desc->wdt_dis;
	return chip->wdt_en ? HL7139_enable_wdt(chip, false) : 0;
#endif
/*	__HL7139_i2c_write8(chip,HL7139_REG_VBAT_REG , 0x7C);  //register 0x08
	__HL7139_i2c_write8(chip,HL7139_REG_STBY_CTRL , 0x00);	//register 0x09
	__HL7139_i2c_write8(chip,HL7139_IBAT_REG , 0x7E);		//register 0x0a
	__HL7139_i2c_write8(chip,HL7139_REG_VBUS_OVP , 0xBF);	//register 0x0b
	__HL7139_i2c_write8(chip,HL7139_REG_VIN_OVP , 0x0F);	//register 0x0c
	__HL7139_i2c_write8(chip,HL7139_REG_IIN_REG , 0x32);	//register 0x0e
	__HL7139_i2c_write8(chip,HL7139_REG_IIN_OC , 0xEF);		//register 0x0f
	__HL7139_i2c_write8(chip,HL7139_REG_CTRL0 , 0x30);		//register 0x10
	__HL7139_i2c_write8(chip,HL7139_REG_CTRL1 , 0x3C);		//register 0x11
	__HL7139_i2c_write8(chip,HL7139_CTRL2 , 0x08);			//register 0x14
	__HL7139_i2c_write8(chip,HL7139_CTRL3 , 0x00);			//register 0x15
	__HL7139_i2c_write8(chip,HL7139_REG_ADC_CTRL0 , 0x05);	//register 0x40
	__HL7139_i2c_write8(chip,HL7139_REG_ADC_CTRL1 , 0x00);	//register 0x41*/
	
	__HL7139_i2c_write8(chip,0x12,0x05);
	__HL7139_i2c_write8(chip,0xA0,0xF9);
	__HL7139_i2c_write8(chip,0xA0,0x9F);
	__HL7139_i2c_write8(chip,0xA7,0x04);
	__HL7139_i2c_write8(chip,0xA0,0x00);
	__HL7139_i2c_write8(chip,0x02,0xE0);
//Antaiui <AI_BSP_CHG> <hehl> <2021-06-09> 9v drop to 5v start	
	__HL7139_i2c_write8(chip,0x08,0xA9);//0x08
//Antaiui <AI_BSP_CHG> <hehl> <2021-06-09> 9v drop to 5v end	
	__HL7139_i2c_write8(chip,0x0A,0xAE);
	__HL7139_i2c_write8(chip,0x0B,0x88);
	__HL7139_i2c_write8(chip,0x0C,0x0F);
	__HL7139_i2c_write8(chip,0x0E,0x32);
	__HL7139_i2c_write8(chip,0x10,0xE0);
	__HL7139_i2c_write8(chip,0x11,0xDC);
	__HL7139_i2c_write8(chip,0x13,0x00);
	__HL7139_i2c_write8(chip,0x14,0x08);
	__HL7139_i2c_write8(chip,0x15,0x00);
	__HL7139_i2c_write8(chip,0x16,0xFF);//3B
	__HL7139_i2c_write8(chip,0x41,0x08);	
	__HL7139_i2c_write8(chip,0x40,0x05);
	return 0;
}

static int HL7139_check_devinfo(struct i2c_client *client, u8 *chip_rev,
				enum HL7139_type *type)
{
	int ret;
	ret = i2c_smbus_read_byte_data(client, HL7139_DEVID);
	if (ret < 0)
	{
		dev_info(&client->dev, "%s i2c read error\n", __func__);
		return ret;
	}
	if ((ret & 0x0f) != HL7139_DEVID_R)
	{
		dev_info(&client->dev, "%s id=0x%x not match\n", __func__,(ret & 0x0f));
		return -ENODEV;
	}
	*chip_rev = (ret & 0xf0) >> 4;
#if 0
	ret = i2c_smbus_read_byte_data(client, HL7139_REG_CHGCTRL1);
	if (ret < 0)
		return ret;
	*type = (ret & 0x60) >> 5;
	dev_info(&client->dev, "%s rev(0x%02X), type(%s)\n", __func__,
		 *chip_rev, HL7139_type_name[*type]);
#endif
	dev_info(&client->dev, "%s rev(0x%02X)\n", __func__,
		 *chip_rev);
	return 0;
}
//Antaiui <AI_BSP_CHG> <hehl> <2021-12-23> #37147 begin
bool hl7139_is_load = false;
//Antaiui <AI_BSP_CHG> <hehl> <2021-12-23> #37147 end
static int HL7139_i2c_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	int ret;
	struct HL7139_chip *chip;
	u8 chip_rev;
	enum HL7139_type type;

	dev_info(&client->dev, "%s(%s)\n", __func__, HL7139_DRV_VERSION);//	"1.0.8_MTK"

	ret = HL7139_check_devinfo(client, &chip_rev, &type);       
	if (ret < 0)
		return ret;
	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	chip->dev = &client->dev;
	chip->client = client;
	chip->revision = chip_rev;
//	chip->type = type;
	mutex_init(&chip->io_lock);
	mutex_init(&chip->adc_lock);
	mutex_init(&chip->stat_lock);
	mutex_init(&chip->hm_lock);
	mutex_init(&chip->suspend_lock);
	mutex_init(&chip->notify_lock);
	init_waitqueue_head(&chip->wq);
	i2c_set_clientdata(client, chip);

	ret = HL7139_parse_dt(chip);                   
	if (ret < 0) {
		dev_notice(chip->dev, "%s parse dt fail(%d)\n", __func__, ret);
		goto err;
	}
	
#ifdef CONFIG_RT_REGMAP
	ret = HL7139_register_regmap(chip);   
	if (ret < 0) {
		dev_notice(chip->dev, "%s reg regmap fail(%d)\n",
			__func__, ret);
		goto err;
	}
#endif /* CONFIG_RT_REGMAP */

	ret = __HL7139_init_chip(chip);
	if (ret < 0) {
		dev_notice(chip->dev, "%s init chip fail(%d)\n", __func__, ret);
		goto err_initchip;
	}

	ret = HL7139_register_chgdev(chip);
	if (ret < 0) {
		dev_notice(chip->dev, "%s reg chgdev fail(%d)\n",
			__func__, ret);
		goto err_initchip;
	}

	chip->notify_task = kthread_run(HL7139_notify_task_threadfn, chip,
					"notify_thread");
	if (IS_ERR(chip->notify_task)) {
		dev_notice(chip->dev, "%s run notify thread fail(%d)\n",
			__func__, ret);
		ret = PTR_ERR(chip->notify_task);
		goto err_initirq;
	}
	
	ret = HL7139_init_irq(chip);
	if (ret < 0) {
		dev_notice(chip->dev, "%s init irq fail(%d)\n", __func__, ret);
		goto err_initirq;
	}
	
//Antaiui <AI_BSP_CHG> <hehl> <2021-05-31> add mmi charger ic display start
	#ifdef CONFIG_AI_BSP_MTK_DEVICE_CHECK
	{
		#include <linux/ai_device_check.h>
		struct ai_device_info ai_chg_hw_info;
		ai_chg_hw_info.ai_dev_type = AI_DEVICE_TYPE_CHARGER;
		strcpy(ai_chg_hw_info.name, "HL7139");
		ai_set_device_info(ai_chg_hw_info);
	}
	#endif
//Antaiui <AI_BSP_CHG> <hehl> <2021-05-31> add mmi charger ic display end

//Antaiui <AI_BSP_USB> <hehl> <2021-03-16> add uisoc limit charge interface begin
	chg_en_chip = chip;
//Antaiui <AI_BSP_USB> <hehl> <2021-03-16> add uisoc limit charge interface end
//Antaiui <AI_BSP_CHG> <hehl> <2021-12-23> #37147 begin
	hl7139_is_load = true;
//Antaiui <AI_BSP_CHG> <hehl> <2021-12-23> #37147 end	
	dev_info(chip->dev, "%s successfully\n", __func__);
	return 0;

err_initirq:
	charger_device_unregister(chip->chg_dev);
err_initchip:
	
#if 0
#ifdef CONFIG_HL_REGMAP
	hl_regmap_device_unregister(chip->rm_dev);
#endif /* CONFIG_HL_REGMAP */
#endif 

err:
	mutex_destroy(&chip->notify_lock);
	mutex_destroy(&chip->suspend_lock);
	mutex_destroy(&chip->hm_lock);
	mutex_destroy(&chip->stat_lock);
	mutex_destroy(&chip->adc_lock);
	mutex_destroy(&chip->io_lock);
	return ret;
}

static void HL7139_i2c_shutdown(struct i2c_client *client)
{
	dev_info(&client->dev, "%s\n", __func__);
}

static int HL7139_i2c_remove(struct i2c_client *client)
{
	struct HL7139_chip *chip = i2c_get_clientdata(client);
	
       dev_info(&client->dev, "%s\n", __func__);
	if (!chip)
		return 0;
	if (chip->notify_task)
		kthread_stop(chip->notify_task);
	charger_device_unregister(chip->chg_dev);
	
#if 0	
#ifdef CONFIG_HL_REGMAP
	hl_regmap_device_unregister(chip->rm_dev);
#endif /* CONFIG_HL_REGMAP */
#endif

	mutex_destroy(&chip->notify_lock);
	mutex_destroy(&chip->suspend_lock);
	mutex_destroy(&chip->hm_lock);
	mutex_destroy(&chip->stat_lock);
	mutex_destroy(&chip->adc_lock);
	mutex_destroy(&chip->io_lock);
	return 0;
}

static int __maybe_unused HL7139_i2c_suspend(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct HL7139_chip *chip = i2c_get_clientdata(i2c);

	dev_info(dev, "%s\n", __func__);
	mutex_lock(&chip->suspend_lock);
	if (device_may_wakeup(dev))
		enable_irq_wake(chip->irq);
	return 0;
}

static int __maybe_unused HL7139_i2c_resume(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct HL7139_chip *chip = i2c_get_clientdata(i2c);

	dev_info(dev, "%s\n", __func__);
	mutex_unlock(&chip->suspend_lock);
	if (device_may_wakeup(dev))
		disable_irq_wake(chip->irq);
	return 0;
}

static SIMPLE_DEV_PM_OPS(HL7139_pm_ops, HL7139_i2c_suspend, HL7139_i2c_resume);

static const struct of_device_id HL7139_of_id[] = {
	{ .compatible = "richtek,rt9759" },
	{},
};
MODULE_DEVICE_TABLE(of, HL7139_of_id);

static const struct i2c_device_id HL7139_i2c_id[] = {
	{ "rt9759", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, HL7139_i2c_id);

static struct i2c_driver HL7139_i2c_driver = {
	.driver = {
		.name = "rt9759",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(HL7139_of_id),
		.pm = &HL7139_pm_ops,
	},
	.probe = HL7139_i2c_probe,
	.shutdown = HL7139_i2c_shutdown,
	.remove = HL7139_i2c_remove,
	.id_table = HL7139_i2c_id,
};
module_i2c_driver(HL7139_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Halomicro HL7139 Charger Driver");
MODULE_AUTHOR("Devin Wu<Devin.Wu@Halomicro.com>");
MODULE_VERSION(HL7139_DRV_VERSION);

/*
 * 1.0.8_MTK
 * (1) Add init_chip ops, if register reset happened, init_chip again.
 *
 * 1.0.7_MTK
 * (1) Add ibusucpf_deglitch in dtsi
 *
 * 1.0.6_MTK
 * (1) Add ibat_rsense in dtsi
 *
 * 1.0.5_MTK
 * (1) Modify IBUS ADC accuracy to 150mA
 * (2) Move adc_lock from __HL7139_get_adc to HL7139_get_adc
 * (3) Check force_adc_en before disabling adc in HL7139_is_vbuslowerr
 *
 * 1.0.4_MTK
 * (1) Add get_adc_accuracy ops
 *
 * 1.0.3_MTK
 * (1) Modify xxx_to_reg to support round up/down
 * (2) Show register value when set protection
 *
 * 1.0.2_MTK
 * (1) Notify ibusucpf/vbusovpalm/vbatovpalm/ibusocp/vbusovp/ibatocp/vbatovp/
 *     voutovp/vdrovp event
 * (2) Add checking vbuslowerr ops
 * (3) Create a thread to handle notification
 *
 * 1.0.1_MTK
 * (1) Modify maximum IBUSOCP from 3750 to 4750mA
 * (2) Remove operation of enabling sBase before enabling charging and ADC
 * (3) Add Master/Slave/Standalone mode's operation
 * (4) Add RSSO flag/state description and handle state only event
 *     only if state has changed
 * (5) If WDT is enabled in dtsi, only enable it right before enabling CHG_EN
 *     and disable it right after disabling CHG_EN
 *
 * 1.0.0_MTK
 * Initial release
 */
