#ifndef SC8551_H
#define SC8551_H

#include <mt-plat/v1/charger_class.h>
#include <linux/printk.h>

#define CP_DBG_EN		1
#define CP_INFO_EN		1
#define CP_ERR_EN		1

#define CP_DBG(fmt, ...) \
	do { \
		if (CP_DBG_EN) \
			pr_info("[%s][%d] " fmt, __func__,__LINE__, ##__VA_ARGS__); \
	} while (0)

#define CP_INFO(fmt, ...) \
	do { \
		if (CP_INFO_EN) \
			pr_info("[%s][%d] " fmt, __func__,__LINE__, ##__VA_ARGS__); \
	} while (0)

#define CP_ERR(fmt, ...) \
	do { \
		if (CP_ERR_EN) \
			pr_info("[%s][%d] " fmt,__func__,__LINE__, ##__VA_ARGS__); \
	} while (0)

/*Vendor ID*/
#define SC8551_DV2_DVCHG_CONVERT_RATIO 		(210)
#define SC8551_DV2_IBATOCP_RATIO 			 		(120)
/* Information */
#define SC8551_DRV_VERSION	"1.0.8_MTK"
/*Vendor ID*/
#define SC8551_DEVID_A					(0x08)
#define SC8551_DEVID_B					(0x00)
#define SC8551_DEVID_C					(0x01)
/* Registers */
#define SC8551_REG_VBATOVP			0x00
#define SC8551_REG_VBATOVP_ALM	0x01
#define SC8551_REG_IBATOCP			0x02
#define SC8551_REG_IBATOCP_ALM	0x03
#define SC8551_REG_IBATUCP_ALM	0x04
#define SC8551_REG_ACPROTECT		0x05
#define SC8551_REG_VBUSOVP			0x06
#define SC8551_REG_VBUSOVP_ALM	0x07
#define SC8551_REG_IBUSOCUCP		0x08
#define SC8551_REG_IBUSOCP_ALM	0x09

#define SC8551_REG_CONVSTAT			0x0A
#define SC8551_REG_CHGCTRL0			0x0B
#define SC8551_REG_CHGCTRL1			0x0C
#define SC8551_REG_INTSTAT			0x0D
#define SC8551_REG_INTFLAG			0x0E
#define SC8551_REG_INTMASK			0x0F
#define SC8551_REG_FLTSTAT			0x10
#define SC8551_REG_FLTFLAG			0x11
#define SC8551_REG_FLTMASK			0x12
#define SC8551_REG_DEVINFO			0x13
#define SC8551_REG_ADCCTRL			0x14
#define SC8551_REG_ADCEN				0x15
#define SC8551_REG_IBUSADC1			0x16
#define SC8551_REG_IBUSADC0			0x17
#define SC8551_REG_VBUSADC1			0x18
#define SC8551_REG_VBUSADC0			0x19
#define SC8551_REG_VACADC1			0x1A
#define SC8551_REG_VACADC0			0x1B
#define SC8551_REG_VOUTADC1			0x1C
#define SC8551_REG_VOUTADC0			0x1D
#define SC8551_REG_VBATADC1			0x1E
#define SC8551_REG_VBATADC0			0x1F
#define SC8551_REG_IBATADC1			0x20
#define SC8551_REG_IBATADC0			0x21
#define SC8551_REG_TSBUSADC1		0x22
#define SC8551_REG_TSBUSADC0		0x23
#define SC8551_REG_TSBATADC1		0x24
#define SC8551_REG_TSBATADC0		0x25
#define SC8551_REG_TDIEADC1			0x26
#define SC8551_REG_TDIEADC0			0x27
#define SC8551_REG_TSBUSOTP			0x28
#define SC8551_REG_TSBATOTP			0x29
#define SC8551_REG_TDIEALM			0x2A
#define SC8551_REG_REGCTRL			0x2B
#define SC8551_REG_REGTHRES			0x2C
#define SC8551_REG_REGFLAGMASK	0x2D
#define SC8551_REG_BUSDEGLH			0x2E
#define SC8551_REG_OTHER1				0x30
#define SC8551_REG_SYSCTRL1			0x42
#define SC8551_REG_PASSWORD0		0x90
#define SC8551_REG_PASSWORD1		0x91

/* Control bits */
#define SC8551_CHGEN_MASK	BIT				(7)
#define SC8551_CHGEN_SHFT						(7)
#define SC8551_ADCEN_MASK	BIT				(7)
#define SC8551_WDTEN_MASK	BIT				(2)
#define SC8551_WDTMR_MASK						(0x03)
#define SC8551_VBUSOVP_MASK					(0x7F)
#define SC8551_IBUSOCP_MASK					(0x0F)
#define SC8551_VBATOVP_MASK					(0x3F)
#define SC8551_IBATOCP_MASK					(0x7F)
#define SC8551_VBATOVP_ALM_MASK			(0x3F)
#define SC8551_VBATOVP_ALMDIS_MASK	BIT(7)
#define SC8551_VBUSOVP_ALM_MASK			(0x7F)
#define SC8551_VBUSOVP_ALMDIS_MASK	BIT(7)
#define SC8551_VBUSLOWERR_FLAG_SHFT	(2)
#define SC8551_VBUSLOWERR_STAT_SHFT	(5)



enum SC8551_irqidx {
	SC8551_IRQIDX_VACOVP = 0,
	SC8551_IRQIDX_IBUSUCPF,
	SC8551_IRQIDX_IBUSUCPR,
	SC8551_IRQIDX_CFLYDIAG,
	SC8551_IRQIDX_CONOCP,
	SC8551_IRQIDX_SWITCHING,
	SC8551_IRQIDX_IBUSUCPTOUT,
	SC8551_IRQIDX_VBUSHERR,
	SC8551_IRQIDX_VBUSLERR,
	SC8551_IRQIDX_TDIEOTP,
	SC8551_IRQIDX_WDT,
	SC8551_IRQIDX_ADCDONE,
	SC8551_IRQIDX_VOUTINSERT,
	SC8551_IRQIDX_VACINSERT,
	SC8551_IRQIDX_IBATUCPALM,
	SC8551_IRQIDX_IBUSOCPALM,
	SC8551_IRQIDX_VBUSOVPALM,
	SC8551_IRQIDX_IBATOCPALM,
	SC8551_IRQIDX_VBATOVPALM,
	SC8551_IRQIDX_TDIEOTPALM,
	SC8551_IRQIDX_TSBUSOTP,
	SC8551_IRQIDX_TSBATOTP,
	SC8551_IRQIDX_TSBUSBATOTPALM,
	SC8551_IRQIDX_IBUSOCP,
	SC8551_IRQIDX_VBUSOVP,
	SC8551_IRQIDX_IBATOCP,
	SC8551_IRQIDX_VBATOVP,
	SC8551_IRQIDX_VOUTOVP,
	SC8551_IRQIDX_VDROVP,
	SC8551_IRQIDX_IBATREG,
	SC8551_IRQIDX_VBATREG,
	SC8551_IRQIDX_MAX,
};

enum SC8551_notify {
	SC8551_NOTIFY_IBUSUCPF = 0,
	SC8551_NOTIFY_VBUSOVPALM,
	SC8551_NOTIFY_VBATOVPALM,
	SC8551_NOTIFY_IBUSOCP,
	SC8551_NOTIFY_VBUSOVP,
	SC8551_NOTIFY_IBATOCP,
	SC8551_NOTIFY_VBATOVP,
	SC8551_NOTIFY_VOUTOVP,
	SC8551_NOTIFY_VDROVP,
	SC8551_NOTIFY_MAX,
};

enum SC8551_statflag_idx {
	SC8551_SF_ACPROTECT = 0,
	SC8551_SF_IBUSOCUCP,
	SC8551_SF_CONVSTAT,
	SC8551_SF_CHGCTRL0,
	SC8551_SF_INTFLAG,
	SC8551_SF_INTSTAT,
	SC8551_SF_FLTFLAG,
	SC8551_SF_FLTSTAT,
	SC8551_SF_REGFLAGMASK,
	SC8551_SF_REGTHRES,
	SC8551_SF_OTHER1,
	SC8551_SF_MAX,
};

enum SC8551_type {
	SC8551_TYPE_STANDALONE = 0,
	SC8551_TYPE_SLAVE,
	SC8551_TYPE_MASTER,
	SC8551_TYPE_MAX,
};


struct SC8551_reg_defval {
	u8 reg;
	u8 value;
	u8 mask;
};


struct SC8551_desc {
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
	bool adc_dis;
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

struct SC8551_chip {
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
	struct SC8551_desc *desc;
	struct gpio_desc *irq_gpio;
	struct task_struct *notify_task;
	int irq;
	int notify;
	u8 revision;
	u32 flag;
	u32 stat;
	u32 hm_cnt;
	enum SC8551_type type;
	bool is_sc8551;
	bool wdt_en;
	bool force_adc_en;
	bool stop_thread;
	wait_queue_head_t wq;

#ifdef CONFIG_RT_REGMAP
	struct rt_regmap_device *rm_dev;
	struct rt_regmap_properties *rm_prop;
#endif /* CONFIG_RT_REGMAP */
};


enum SC8551_adc_channel {
	SC8551_ADC_IBUS = 0,
	SC8551_ADC_VBUS,
	SC8551_ADC_VAC,
	SC8551_ADC_VOUT,
	SC8551_ADC_VBAT,
	SC8551_ADC_IBAT,
	SC8551_ADC_TSBUS,
	SC8551_ADC_TSBAT,
	SC8551_ADC_TDIE,
	SC8551_ADC_MAX,
	SC8551_ADC_NOTSUPP = SC8551_ADC_MAX,
};

#define SC8551_DT_VALPROP(name, reg, shft, mask, func, base) \
	{#name, offsetof(struct SC8551_desc, name), reg, shft, mask, func, base}

struct SC8551_dtprop {
	const char *name;
	size_t offset;
	u8 reg;
	u8 shft;
	u8 mask;
	u8 (*toreg)(u32 val);
	u8 base;
};
struct irq_map_desc {
	const char *name;
	int (*hdlr)(struct SC8551_chip *chip);
	u8 flag_idx;
	u8 stat_idx;
	u8 flag_mask;
	u8 stat_mask;
	u32 irq_idx;
	bool stat_only;
};

#define SC8551_IRQ_DESC(_name, _flag_i, _stat_i, _flag_s, _stat_s, _irq_idx, \
			_stat_only) \
	{.name = #_name, .hdlr = SC8551_##_name##_irq_handler, \
	 .flag_idx = _flag_i, .stat_idx = _stat_i, \
	 .flag_mask = (1 << _flag_s), .stat_mask = (1 << _stat_s), \
	 .irq_idx = _irq_idx, .stat_only = _stat_only}

/*
 * RSS: Reister index of flag, Shift of flag, Shift of state
 * RRS: Register index of flag, Register index of state, Shift of flag
 * RS: Register index of flag, Shift of flag
 * RSSO: Register index of state, Shift of state, State Only
 */
#define SC8551_IRQ_DESC_RSS(_name, _flag_i, _flag_s, _stat_s, _irq_idx) \
	SC8551_IRQ_DESC(_name, _flag_i, _flag_i, _flag_s, _stat_s, _irq_idx, \
			false)

#define SC8551_IRQ_DESC_RRS(_name, _flag_i, _stat_i, _flag_s, _irq_idx) \
	SC8551_IRQ_DESC(_name, _flag_i, _stat_i, _flag_s, _flag_s, _irq_idx, \
			false)

#define SC8551_IRQ_DESC_RS(_name, _flag_i, _flag_s, _irq_idx) \
	SC8551_IRQ_DESC(_name, _flag_i, _flag_i, _flag_s, _flag_s, _irq_idx, \
			false)

#define SC8551_IRQ_DESC_RSSO(_name, _flag_i, _flag_s, _irq_idx) \
	SC8551_IRQ_DESC(_name, _flag_i, _flag_i, _flag_s, _flag_s, _irq_idx, \
			true)
			
#define I2C_ACCESS_MAX_RETRY	5
#endif