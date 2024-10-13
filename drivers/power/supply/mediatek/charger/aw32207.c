/*
 * aw32207.c   aw32207 charge module
 *
 * Copyright (c) 2018 AWINIC Technology CO., LTD
 *
 *  Author: Joseph <zhangzetao@awinic.com.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/version.h>
#include <linux/i2c.h>

#define AW32207_DRIVER_VERSION "V2.0.4"

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#endif
#include "upmu_common.h"
#include "aw32207.h"
#include "mtk_charger_intf.h"

const unsigned int VBAT_CVTH[] = {
	3500000, 3520000, 3540000, 3560000,
	3580000, 3600000, 3620000, 3640000,
	3660000, 3680000, 3700000, 3720000,
	3740000, 3760000, 3780000, 3800000,
	3820000, 3840000, 3860000, 3880000,
	3900000, 3920000, 3940000, 3960000,
	3980000, 4000000, 4020000, 4040000,
	4060000, 4080000, 4100000, 4120000,
	4140000, 4160000, 4180000, 4200000,
	4220000, 4240000, 4260000, 4280000,
	4300000, 4320000, 4340000, 4360000,
	4380000, 4400000, 4420000, 4440000,
	4460000, 4480000, 4500000
};

const unsigned int CSTH56[] = {
	485000, 607000, 850000, 971000,
	1092000, 1214000, 1336000, 1457000,
	1578000, 1699000, 1821000, 1821000,
	1821000, 1821000, 1821000, 1821000
};

const unsigned int INPUT_CSTH[] = {
	100000, 500000, 800000, 5000000
};

const unsigned int BOOST_CURRENT_LIMIT[] = {
	500, 750, 1200, 1400, 1650, 1875, 2150,
};

#ifdef AW32207_CHARGE
static int aw32207_driver_probe(struct i2c_client *client,
						const struct i2c_device_id *id);

static const struct i2c_device_id aw32207_i2c_id[] = { {"aw32207", 1}, {} };

#ifdef CONFIG_OF
static const struct of_device_id aw32207_of_match[] = {
	{.compatible = "awinic,aw32207"},
	{},
};
#else
static struct i2c_board_info i2c_aw32207 __initdata = {
	I2C_BOARD_INFO("aw32207", (aw32207_SLAVE_ADDR_WRITE >> 1))
};
#endif
#endif

#define aw32207_SLAVE_ADDR_WRITE	0xD4
#define aw32207_SLAVE_ADDR_Read		0xD5
#define AW_I2C_RETRIES			5
#define AW_I2C_RETRY_DELAY		2

static struct i2c_driver aw32207_driver = {
	.driver = {
		.name = "aw32207",
#ifdef CONFIG_OF
		.of_match_table = aw32207_of_match,
#endif
	},
	.probe = aw32207_driver_probe,
	.id_table = aw32207_i2c_id, /* .remove = aw32207_i2c_remove */
};

struct aw32207_info {
	struct charger_device *chg_dev;
	struct power_supply *psy;
	struct charger_properties chg_props;
	struct device *dev;
	struct gtimer otg_kthread_gtimer;
	struct workqueue_struct *otg_boost_workq;
	struct work_struct kick_work;
	unsigned int polling_interval;
	bool polling_enabled;
	const char *chg_dev_name;
};

static struct aw32207_info *g_info;
static struct i2c_client *new_client;

static void enable_boost_polling(bool poll_en);
static void usbotg_boost_kick_work(struct work_struct *work);
static int usbotg_gtimer_func(struct gtimer *data);

unsigned int charging_value_to_parameter(const unsigned int *parameter,
			const unsigned int array_size, const unsigned int val)
{
	if (val < array_size)
		return parameter[val];
	pr_info("Can't find the parameter\n");
	return parameter[0];
}

unsigned int charging_parameter_to_value(const unsigned int *parameter,
			const unsigned int array_size, const unsigned int val)
{
	unsigned int i;

	pr_debug_ratelimited("array_size = %d\n", array_size);

	for (i = 0; i < array_size; i++) {
		if (val == *(parameter + i))
			return i;
	}

	pr_info("NO register value match\n");
	/* TODO: ASSERT(0); not find the value */
	return 0;
}

static unsigned int bmt_find_closest_level(const unsigned int *pList,
			unsigned int number, unsigned int level)
{
	unsigned int i;
	unsigned int max_value_in_last_element;

	if (pList[0] < pList[1])
		max_value_in_last_element = 1;
	else
		max_value_in_last_element = 0;

	if (max_value_in_last_element == 1) {
		/* max value in the last element */
		for (i = (number - 1); i != 0; i--) {
			if (pList[i] <= level) {
				pr_debug_ratelimited("zzf_%d<=%d, i=%d\n",
				pList[i], level, i);
				return pList[i];
			}
		}
		pr_info("Can't find closest level\n");
		return pList[0]; /* return 000; */
	} else {
		/* max value in the first element */
		for (i = 0; i < number; i++) {
			if (pList[i] <= level)
				return pList[i];
		}
		pr_info("Can't find closest level\n");
		return pList[number - 1];
		/* return 000; */
	}
}

unsigned char aw32207_reg[AW32207_REG_NUM] = { 0 };
static DEFINE_MUTEX(aw32207_i2c_access);
static DEFINE_MUTEX(aw32207_access_lock);

static int aw32207_read_byte(u8 reg_addr, u8 *rd_buf, int rd_len)
{
	int ret = 0;
	unsigned char cnt = 0;
	struct i2c_adapter *adap = new_client->adapter;
	struct i2c_msg msg[2];
	u8 *w_buf = NULL;
	u8 *r_buf = NULL;

	memset(msg, 0, 2 * sizeof(struct i2c_msg));

	w_buf = kzalloc(1, GFP_KERNEL);
	if (w_buf == NULL)
		return -1;
	r_buf = kzalloc(rd_len, GFP_KERNEL);
	if (r_buf == NULL)
		return -1;

	*w_buf = reg_addr;

	msg[0].addr = new_client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = w_buf;

	msg[1].addr = new_client->addr;
	msg[1].flags = 1;
	msg[1].len = rd_len;
	msg[1].buf = r_buf;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_transfer(adap, msg, 2);
		if (ret < 0)
			pr_err("%s: i2c read byte error, ret = %d\n",
								__func__, ret);
		else
			break;

		cnt++;
		mdelay(AW_I2C_RETRY_DELAY);
	}

	memcpy(rd_buf, r_buf, rd_len);

	kfree(w_buf);
	kfree(r_buf);
	return ret;
}

int aw32207_write_byte(unsigned char reg_num, u8 *wr_buf, int wr_len)
{
	int ret = 0;
	unsigned char cnt = 0;
	struct i2c_adapter *adap = new_client->adapter;
	struct i2c_msg msg;
	u8 *w_buf = NULL;

	memset(&msg, 0, sizeof(struct i2c_msg));

	w_buf = kzalloc(wr_len, GFP_KERNEL);
	if (w_buf == NULL)
		return -1;
	w_buf[0] = reg_num;
	memcpy(w_buf + 1, wr_buf, wr_len);

	msg.addr = new_client->addr;
	msg.flags = 0;
	msg.len = wr_len;
	msg.buf = w_buf;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_transfer(adap, &msg, 1);
		if (ret < 0)
			pr_err("%s: i2c write byte error, ret = %d\n",
								__func__, ret);
		else
			break;

		cnt++;
		mdelay(AW_I2C_RETRY_DELAY);
	}

	kfree(w_buf);
	return ret;
}

unsigned int aw32207_read_interface(unsigned char reg_num, unsigned char *val,
					unsigned char MASK, unsigned char SHIFT)
{
	unsigned char aw32207_reg = 0;
	unsigned int ret = 0;

	ret = aw32207_read_byte(reg_num, &aw32207_reg, 1);
	pr_debug_ratelimited("[aw32207_read_interface] Reg[%x]=0x%x\n", reg_num,
								aw32207_reg);
	aw32207_reg &= (MASK << SHIFT);
	*val = (aw32207_reg >> SHIFT);
	pr_debug_ratelimited("[aw32207_read_interface] val=0x%x\n", *val);

	return ret;
}

unsigned int aw32207_config_interface(unsigned char reg_num, unsigned char val,
					unsigned char MASK, unsigned char SHIFT)
{
	unsigned char aw32207_reg = 0;
	unsigned char aw32207_reg_ori = 0;
	unsigned int ret = 0;

	mutex_lock(&aw32207_access_lock);
	ret = aw32207_read_byte(reg_num, &aw32207_reg, 1);
	aw32207_reg_ori = aw32207_reg;

	aw32207_reg &= ~(MASK << SHIFT);
	aw32207_reg |= (val << SHIFT);

	if (reg_num == AW32207_CON4)
		aw32207_reg &= ~(1 << CON4_RESET_SHIFT);

	ret = aw32207_write_byte(reg_num, &aw32207_reg, 2);
	mutex_unlock(&aw32207_access_lock);

	return ret;
}

/* write one register directly */
unsigned int aw32207_reg_config_interface(unsigned char reg_num,
							unsigned char val)
{
	unsigned char aw32207_reg = val;

	return aw32207_write_byte(reg_num, &aw32207_reg, 2);
}

unsigned int aw32207_get_otg_status(void)
{
	unsigned char val = 0;

	aw32207_read_interface((unsigned char)(AW32207_CON0),
				(unsigned char *)(&val),
				(unsigned char)(CON0_OTG_MASK),
				(unsigned char)(CON0_OTG_SHIFT)
				);
	return val;
}

void aw32207_set_en_stat(unsigned int val)
{
	aw32207_config_interface((unsigned char)(AW32207_CON0),
				(unsigned char)(val),
				(unsigned char)(CON0_EN_STAT_MASK),
				(unsigned char)(CON0_EN_STAT_SHIFT)
				);
}

unsigned int aw32207_get_chip_status(void)
{
	unsigned char val = 0;

	aw32207_read_interface((unsigned char)(AW32207_CON0),
				(unsigned char *)(&val),
				(unsigned char)(CON0_STAT_MASK),
				(unsigned char)(CON0_STAT_SHIFT)
				);
	return val;
}

unsigned int aw32207_get_boost_status(void)
{
	unsigned char val = 0;

	aw32207_read_interface((unsigned char)(AW32207_CON0),
				(unsigned char *)(&val),
				(unsigned char)(CON0_BOOST_MASK),
				(unsigned char)(CON0_BOOST_SHIFT)
				);
	return val;

}

unsigned int aw32207_get_chg_fault_status(void)
{
	unsigned char val = 0;

	aw32207_read_interface((unsigned char)(AW32207_CON0),
				(unsigned char *)(&val),
				(unsigned char)(CON0_CHG_FAULT_MASK),
				(unsigned char)(CON0_CHG_FAULT_SHIFT)
				);
	return val;
}

void aw32207_set_te(unsigned int val)
{
	aw32207_config_interface((unsigned char)(AW32207_CON1),
				(unsigned char)(val),
				(unsigned char)(CON1_TE_MASK),
				(unsigned char)(CON1_TE_SHIFT)
				);
}

void aw32207_set_cen(unsigned int val)
{
	aw32207_config_interface((unsigned char)(AW32207_CON1),
				(unsigned char)(val),
				(unsigned char)(CON1_CEN_MASK),
				(unsigned char)(CON1_CEN_SHIFT)
				);
}

void aw32207_set_hz_mode(unsigned int val)
{
	aw32207_config_interface((unsigned char)(AW32207_CON1),
				(unsigned char)(val),
				(unsigned char)(CON1_HZ_MODE_MASK),
				(unsigned char)(CON1_HZ_MODE_SHIFT)
				);
}

void aw32207_set_opa_mode(unsigned int val)
{
	aw32207_config_interface((unsigned char)(AW32207_CON1),
				(unsigned char)(val),
				(unsigned char)(CON1_OPA_MODE_MASK),
				(unsigned char)(CON1_OPA_MODE_SHIFT)
				);
}

void aw32207_set_voreg(unsigned int val)
{
	aw32207_config_interface((unsigned char)(AW32207_CON2),
				(unsigned char)(val),
				(unsigned char)(CON2_VOREG_MASK),
				(unsigned char)(CON2_VOREG_SHIFT)
				);
}
void aw32207_set_otg_pl(unsigned int val)
{
	aw32207_config_interface((unsigned char)(AW32207_CON2),
				(unsigned char)(val),
				(unsigned char)(CON2_OTG_PL_MASK),
				(unsigned char)(CON2_OTG_PL_SHIFT)
				);
}
void aw32207_set_otg_en(unsigned int val)
{
	aw32207_config_interface((unsigned char)(AW32207_CON2),
				(unsigned char)(val),
				(unsigned char)(CON2_OTG_EN_MASK),
				(unsigned char)(CON2_OTG_EN_SHIFT)
				);
}

unsigned int aw32207_get_vender_code(void)
{
	unsigned char val = 0;

	aw32207_read_interface((unsigned char)(AW32207_CON3),
				(unsigned char *)(&val),
				(unsigned char)(CON3_VENDER_MASK),
				(unsigned char)(CON3_VENDER_SHIFT)
				);
	return val;
}
unsigned int aw32207_get_pn(void)
{
	unsigned char val = 0;

	aw32207_read_interface((unsigned char)(AW32207_CON3),
				(unsigned char *)(&val),
				(unsigned char)(CON3_PN_MASK),
				(unsigned char)(CON3_PN_SHIFT)
				);
	return val;
}

unsigned int aw32207_get_revision(void)
{
	unsigned char val = 0;

	aw32207_read_interface((unsigned char)(AW32207_CON3),
				(unsigned char *)(&val),
				(unsigned char)(CON3_REVISION_MASK),
				(unsigned char)(CON3_REVISION_SHIFT)
				);
	return val;
}

void aw32207_set_reset(unsigned int val)
{
	aw32207_config_interface((unsigned char)(AW32207_CON4),
				(unsigned char)(val),
				(unsigned char)(CON4_RESET_MASK),
				(unsigned char)(CON4_RESET_SHIFT)
				);
	mdelay(50);
}

void aw32207_set_iocharge(unsigned int val)
{
	aw32207_config_interface((unsigned char)(AW32207_CON4),
				(unsigned char)(val),
				(unsigned char)(CON4_I_CHR_MASK),
				(unsigned char)(CON4_I_CHR_SHIFT)
				);
}

void aw32207_set_iterm(unsigned int val)
{
	aw32207_config_interface((unsigned char)(AW32207_CON4),
				(unsigned char)(val),
				(unsigned char)(CON4_I_TERM_MASK),
				(unsigned char)(CON4_I_TERM_SHIFT)
				);
}

unsigned int aw32207_get_sp_status(void)
{
	unsigned char val = 0;

	aw32207_read_interface((unsigned char)(AW32207_CON5),
				(unsigned char *)(&val),
				(unsigned char)(CON5_DPM_STATUS_MASK),
				(unsigned char)(CON5_DPM_STATUS_SHIFT)
				);
	return val;
}

unsigned int aw32207_get_en_level(void)
{
	unsigned char val = 0;

	aw32207_read_interface((unsigned char)(AW32207_CON5),
				(unsigned char *)(&val),
				(unsigned char)(CON5_CD_STATUS_MASK),
				(unsigned char)(CON5_CD_STATUS_SHIFT)
				);
	return val;
}

void aw32207_set_vsp(unsigned int val)
{
	aw32207_config_interface((unsigned char)(AW32207_CON5),
				(unsigned char)(val),
				(unsigned char)(CON5_VSP_MASK),
				(unsigned char)(CON5_VSP_SHIFT)
				);
}

void aw32207_set_i_safe(unsigned int val)
{
	aw32207_config_interface((unsigned char)(AW32207_CON6),
				(unsigned char)(val),
				(unsigned char)(CON6_ISAFE_MASK),
				(unsigned char)(CON6_ISAFE_SHIFT)
				);
}

void aw32207_set_v_safe(unsigned int val)
{
	aw32207_config_interface((unsigned char)(AW32207_CON6),
				(unsigned char)(val),
				(unsigned char)(CON6_VSAFE_MASK),
				(unsigned char)(CON6_VSAFE_SHIFT)
				);
}

static int aw32207_dump_register(struct charger_device *chg_dev)
{
	int i;

	for (i = 0; i < AW32207_REG_NUM; i++) {
		aw32207_read_byte(i, &aw32207_reg[i], 1);
		pr_info("aw32207_dump_kernel_[0x%x]=0x%x\n", i, aw32207_reg[i]);
	}
	pr_debug("\n");

	return 0;
}

static int aw32207_parse_dt(struct aw32207_info *info, struct device *dev)
{
	struct device_node *np = dev->of_node;

	pr_info("%s\n", __func__);

	if (!np) {
		pr_err("%s: no of node\n", __func__);
		return -ENODEV;
	}

	if (of_property_read_string(np, "charger_name",
						&info->chg_dev_name) < 0) {
		info->chg_dev_name = "primary_chg";
		pr_warn("%s: no charger name\n", __func__);
	}

	if (of_property_read_string(np, "alias_name", &
					(info->chg_props.alias_name)) < 0) {
		info->chg_props.alias_name = "aw32207";
		pr_warn("%s: no alias name\n", __func__);
	}

	return 0;
}

static int aw32207_do_event(struct charger_device *chg_dev, unsigned int event,
							unsigned int args)
{
	if (chg_dev == NULL)
		return -EINVAL;

	pr_info("%s: event = %d\n", __func__, event);
	switch (event) {
	case EVENT_EOC:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
		break;
	case EVENT_RECHARGE:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
		break;
	default:
		break;
	}

	return 0;
}

static int aw32207_enable_charging(struct charger_device *chg_dev, bool en)
{
	unsigned int status = 0;

	if (en) {
		aw32207_set_cen(0);
		aw32207_set_hz_mode(0);
		aw32207_set_opa_mode(0);
	} else {
		aw32207_set_cen(1);
	}

	return status;
}

static int aw32207_set_cv_voltage(struct charger_device *chg_dev, u32 cv)
{
	int status = 0;
	unsigned short int array_size;
	unsigned int set_cv_voltage;
	unsigned short int register_value;
	/*static kal_int16 pre_register_value; */
	array_size = ARRAY_SIZE(VBAT_CVTH);
	/*pre_register_value = -1; */
	set_cv_voltage = bmt_find_closest_level(VBAT_CVTH, array_size, cv);

	register_value = charging_parameter_to_value(VBAT_CVTH, array_size,
								set_cv_voltage);
	pr_info("charging_set_cv_voltage register_value=0x%x %d %d\n",
					register_value, cv, set_cv_voltage);
	aw32207_set_voreg(register_value);
	return status;
}

static int aw32207_get_current(struct charger_device *chg_dev, u32 *ichg)
{
	int status = 0;
	unsigned int array_size;
	unsigned char reg_value;

	array_size = ARRAY_SIZE(CSTH56);
	aw32207_read_interface(AW32207_CON4, &reg_value, CON4_I_CHR_MASK,
							CON4_I_CHR_SHIFT);
	*ichg = charging_value_to_parameter(CSTH56, array_size, reg_value);
	return status;
}

static int aw32207_set_current(struct charger_device *chg_dev,
							u32 current_value)
{
	unsigned int status = 0;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned int register_value;

	array_size = ARRAY_SIZE(CSTH56);
	set_chr_current = bmt_find_closest_level(CSTH56, array_size,
								current_value);
	register_value = charging_parameter_to_value(CSTH56, array_size,
							set_chr_current);
	/*register_value = 0x0a;*/ /*0x0=494mA------(0x0A-0XF)=1854mA;*/
	aw32207_set_iocharge(register_value);
	return status;
}

static int aw32207_set_input_current(struct charger_device *chg_dev,
							u32 current_value)
{
	unsigned int status = 0;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned int register_value;

	if (current_value > 50000) {
		register_value = 0x3;
	} else {
		array_size = ARRAY_SIZE(INPUT_CSTH);
		set_chr_current = bmt_find_closest_level(INPUT_CSTH, array_size,
								current_value);
		register_value = charging_parameter_to_value(INPUT_CSTH,
						array_size, set_chr_current);
	}
#ifdef	CONFIG_HIGH_BATTERY_VOLTAGE_SUPPORT
	aw32207_reg_config_interface(0x06, 0xf9);
#else
	aw32207_reg_config_interface(0x06, 0x70);
#endif

	return status;
}

static int aw32207_get_charging_status(struct charger_device *chg_dev,
								bool *is_done)
{
	unsigned int status = 0;
	unsigned int ret_val;

	ret_val = aw32207_get_chip_status();

	if (ret_val == 0x2)
		*is_done = true;
	else
		*is_done = false;

	return status;
}


static int aw32207_charger_enable_otg(struct charger_device *chg_dev, bool en)
{
#if 1
	aw32207_set_opa_mode(en);
#else
	unsigned char aw32207_reg = 0;
	unsigned int val = 0;

	aw32207_set_otg_en(en);
	val = aw32207_read_byte(0x00, &aw32207_reg, 1) && 0x80;
	aw32207_set_otg_pl(val);
#endif
	enable_boost_polling(en);
	return 0;
}

static void enable_boost_polling(bool poll_en)
{
	if (g_info) {
		if (poll_en) {
			gtimer_start(&g_info->otg_kthread_gtimer,
						g_info->polling_interval);
			g_info->polling_enabled = true;
		} else {
			g_info->polling_enabled = false;
			gtimer_stop(&g_info->otg_kthread_gtimer);
		}
	}
}

static void usbotg_boost_kick_work(struct work_struct *work)
{

	struct aw32207_info *boost_manager = container_of(work,
						struct aw32207_info, kick_work);
	pr_debug_ratelimited("usbotg_boost_kick_work\n");

	if (boost_manager->polling_enabled == true) {
				gtimer_start(&boost_manager->otg_kthread_gtimer,
				boost_manager->polling_interval);
	}
}

static int usbotg_gtimer_func(struct gtimer *data)
{
	struct aw32207_info *boost_manager = container_of(data,
				struct aw32207_info, otg_kthread_gtimer);
	queue_work(boost_manager->otg_boost_workq, &boost_manager->kick_work);

	return 0;
}

static struct charger_ops aw32207_chg_ops = {
	/* Normal charging */
	.dump_registers = aw32207_dump_register,
	.enable = aw32207_enable_charging,
	.get_charging_current = aw32207_get_current,
	.set_charging_current = aw32207_set_current,
	/* .get_input_current = aw32207_get_input_current */
	.set_input_current = aw32207_set_input_current,
	/* .get_constant_voltage = aw32207_get_battery_voreg */
	.set_constant_voltage = aw32207_set_cv_voltage,
	/* .kick_wdt = aw32207_reset_watch_dog_timer */
	.is_charging_done = aw32207_get_charging_status,
	/* OTG */
	.enable_otg = aw32207_charger_enable_otg,
	.event = aw32207_do_event,
};

#ifdef AW32207_CHARGE
/* prize modified by chenjiaxi, for kernel-4.19 modified, 20220414-start */
// static ssize_t aw32207_get_reg(struct device *cd, struct device_attribute *attr,
								// char *buf)
static ssize_t reg_show(struct device *cd, struct device_attribute *attr,
								char *buf)
/* prize modified by chenjiaxi, for kernel-4.19 modified, 20220414-end */
{
	ssize_t len = 0;
	int idx;

	for (idx = 0; idx < AW32207_REG_NUM; idx++) {
		aw32207_read_byte(idx, &aw32207_reg[idx], 1);
		len += snprintf(buf+len, PAGE_SIZE-len, "reg[0x%x] = 0x%x\n",
				idx, aw32207_reg[idx]);
	}
	return len;
}
/* prize modified by chenjiaxi, for kernel-4.19 modified, 20220414-start */
// static ssize_t aw32207_set_reg(struct device *cd, struct device_attribute *attr,
				// const char *buf, size_t count)
static ssize_t reg_store(struct device *cd, struct device_attribute *attr,
				const char *buf, size_t count)
/* prize modified by chenjiaxi, for kernel-4.19 modified, 20220414-end */
{
	unsigned int databuf[2];

	sscanf(buf, "%x %x", &databuf[0], &databuf[1]);
	aw32207_reg_config_interface(databuf[0], databuf[1]);
	return count;
}

/* prize modified by chenjiaxi, for kernel-4.19 modified, 20220414-start */
// static ssize_t aw32207_set_sw_rst(struct device *cd,
		// struct device_attribute *attr, const char *buf, size_t count)
static ssize_t sw_rst_store(struct device *cd,
		struct device_attribute *attr, const char *buf, size_t count)
/* prize modified by chenjiaxi, for kernel-4.19 modified, 20220414-end */
{
	unsigned int databuf[1];

	sscanf(buf, "%d", &databuf[0]);
	if (databuf[0] == 1)
		aw32207_set_reset(1);
	return count;
}

/* prize modified by chenjiaxi, for kernel-4.19 modified, 20220414-start */
static DEVICE_ATTR_RW(reg);//, 0660, aw32207_get_reg,  aw32207_set_reg);
static DEVICE_ATTR_WO(sw_rst);//, 0660, NULL,  aw32207_set_sw_rst);
/* prize modified by chenjiaxi, for kernel-4.19 modified, 20220414-end */

static int aw32207_create_attr(struct i2c_client *client)
{
	int err;
	struct device *dev = &(client->dev);

	err = device_create_file(dev, &dev_attr_reg);
	err = device_create_file(dev, &dev_attr_sw_rst);
	return err;
}
#endif

static int aw32207_driver_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	int ret = 0;
	struct aw32207_info *info = NULL;
#ifdef	AW32207_CHARGE
	int err = 0;
#endif

	pr_info("[aw32207_driver_probe]\n");
	pr_info("aw32207 driver version %s\n", AW32207_DRIVER_VERSION);
	info = devm_kzalloc(&client->dev, sizeof(struct aw32207_info),
								GFP_KERNEL);

	if (!info)
		return -ENOMEM;

	new_client = client;
	info->dev = &client->dev;
	ret = aw32207_parse_dt(info, &client->dev);
	if (ret < 0)
		return ret;
	/* Register charger device */
	info->chg_dev = charger_device_register(info->chg_dev_name,
			&client->dev, info, &aw32207_chg_ops, &info->chg_props);

	if (IS_ERR_OR_NULL(info->chg_dev)) {
		pr_err("%s: register charger device failed\n", __func__);
		ret = PTR_ERR(info->chg_dev);
		return ret;
	}

	ret = aw32207_get_vender_code();
	if (ret != 2) {
		pr_err("%s: get vendor id failed\n", __func__);
		return -ENODEV;
	}
	/* aw32207_hw_init(); //move to charging_hw_xxx.c */
	info->psy = power_supply_get_by_name("charger");
	if (!info->psy) {
		pr_err("%s: get power supply failed\n", __func__);
		return -EINVAL;
	}

	aw32207_dump_register(info->chg_dev);
#ifdef	CONFIG_HIGH_BATTERY_VOLTAGE_SUPPORT
	aw32207_reg_config_interface(0x06, 0xf9);
#else
	aw32207_reg_config_interface(0x06, 0x70);
#endif
	aw32207_reg_config_interface(0x00, 0xd0);
	aw32207_reg_config_interface(0x01, 0x38);

	aw32207_reg_config_interface(0x08, 0x8e);

	aw32207_dump_register(info->chg_dev);
	gtimer_init(&info->otg_kthread_gtimer, info->dev, "otg_boost");
	info->otg_kthread_gtimer.callback = usbotg_gtimer_func;

	info->otg_boost_workq =
			create_singlethread_workqueue("otg_boost_workq");

	INIT_WORK(&info->kick_work, usbotg_boost_kick_work);
	info->polling_interval = 20;
	g_info = info;
#ifdef AW32207_CHARGE
	err = aw32207_create_attr(client);
	if (err) {
		pr_err("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
#endif

	return 0;
#ifdef AW32207_CHARGE
exit_create_attr_failed:
	return err;
#endif
}

static int __init aw32207_init(void)
{
	pr_info("aw32207 driver version %s\n", AW32207_DRIVER_VERSION);

	if (i2c_add_driver(&aw32207_driver) != 0)
		pr_info("Failed to register aw32207 i2c driver.\n");
	else
		pr_info("Success to register aw32207 i2c driver.\n");

	return 0;
}

static void __exit aw32207_exit(void)
{
	i2c_del_driver(&aw32207_driver);
}

module_init(aw32207_init);
module_exit(aw32207_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("I2C aw32207 Driver");
MODULE_AUTHOR("Joseph zhangzetao@awinic.com.cn>");

