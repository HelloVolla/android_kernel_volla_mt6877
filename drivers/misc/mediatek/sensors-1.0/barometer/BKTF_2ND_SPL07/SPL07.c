// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * 
 * v1.0 2022.04.20  add pressure_last_value ,if read value without effective clock;
 * v1.1 2022.06.24  debug 0x08 error work mode
 */

#define pr_fmt(fmt) "<SPL07> " fmt

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kobject.h>
#include <linux/math64.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#include "barometer.h"
#include "SPL07.h"
#include <cust_baro.h>
#include "asm/div64.h"
#include <linux/math64.h>
/* #include <linux/hwmsen_helper.h> */

/* prize added by chenjiaxi, 8802 info, 20220429-start */
#ifdef CONFIG_PRIZE_HARDWARE_INFO
#include "../../../hardware_info/hardware_info.h"
extern struct hardware_info current_barosensor_info;
#endif
/* prize added by chenjiaxi, 8802 info, 20220429-end */

/* #define POWER_NONE_MACRO MT65XX_POWER_NONE */

// #define DRIVER_ATTR(_name, _mode, _show, _store) \
        // struct driver_attribute driver_attr_##_name = \
        // __ATTR(_name, _mode, _show, _store)

/* sensor type */
enum SENSOR_TYPE_ENUM {
	SPL07_TYPE = 0x0,

	INVALID_TYPE = 0xff
};

/* power mode */
enum SPL_POWERMODE_ENUM {
	SPL_SUSPEND_MODE = 0x0,
	SPL_NORMAL_MODE,

	SPL_UNDEFINED_POWERMODE = 0xff
};


/* oversampling */
enum SPL_OVERSAMPLING_ENUM {
	SPL_OVERSAMPLING_SINGLE,
	SPL_OVERSAMPLING_2X,
	SPL_OVERSAMPLING_4X,
	SPL_OVERSAMPLING_8X,
	SPL_OVERSAMPLING_16X,
	SPL_OVERSAMPLING_32X,
	SPL_OVERSAMPLING_64X,

	SPL_UNDEFINED_OVERSAMPLING = 0xff
};

/* sampling */
enum SPL_SAMPLING_ENUM {
	SPL_SAMPLING_1Hz,
	SPL_SAMPLING_2Hz,
	SPL_SAMPLING_4Hz,
	SPL_SAMPLING_8Hz,
	SPL_SAMPLING_16Hz,
	SPL_SAMPLING_32Hz,
	SPL_SAMPLING_64Hz,
	SPL_SAMPLING_128Hz,
	SPL_SAMPLING_25_16Hz,
	SPL_SAMPLING_25_8Hz,
	SPL_SAMPLING_25_4Hz,
	SPL_SAMPLING_25_2Hz,
	SPL_SAMPLING_25Hz,
	SPL_SAMPLING_50Hz,
	SPL_SAMPLING_100Hz,
	SPL_SAMPLING_200Hz,

	SPL_UNDEFINED_SAMPLING = 0xff
};

/* trace */
enum BAR_TRC {
	BAR_TRC_READ = 0x01,
	BAR_TRC_RAWDATA = 0x02,
	BAR_TRC_IOCTL = 0x04,
	BAR_TRC_FILTER = 0x08,
	BAR_TRC_INFO = 0x10,
};

/* s/w filter */
struct data_filter {
	u32 raw[C_MAX_FIR_LENGTH][SPL_DATA_NUM];
	int sum[SPL_DATA_NUM];
	int num;
	int idx;
};

/* SPL07 calibration */
struct SPL07_calibration_data {
	SPL07_S16_t C0;
	SPL07_S16_t C1;
	SPL07_S32_t C00;
	SPL07_S32_t C10;
	SPL07_S16_t C01;
	SPL07_S16_t C11;
	SPL07_S16_t C20;
	SPL07_S16_t C21;
	SPL07_S16_t C30;
	SPL07_S16_t C31;
	SPL07_S16_t C40;
};

/* SPL i2c client data */
struct SPL_i2c_data {
	struct i2c_client *client;
	struct baro_hw hw;

	/* sensor info */
	u8 sensor_name[MAX_SENSOR_NAME];
	enum SENSOR_TYPE_ENUM sensor_type;
	enum SPL_POWERMODE_ENUM power_mode;
	u8 oversampling_p;
	u8 oversampling_t;
	u8 sampling_p;
	u8 sampling_t;
	u8 samp_times;
	s32 KP;
	s32 KT;
	s32 last_pressure_value;
	unsigned long last_pressure_measurement;
	unsigned long pressure_measurement_period;
	struct SPL07_calibration_data SPL07_cali;

	/* calculated temperature correction coefficient */
	s32 t_fine;

	/*misc */
	struct mutex lock;
	atomic_t trace;
	atomic_t suspend;
	atomic_t filter;

#if defined(CONFIG_SPL_LOWPASS)
	atomic_t firlen;
	atomic_t fir_en;
	struct data_filter fir;
#endif

/* prize added by chenjiaxi, barometer calibration, 20220423-start */
	s32 referenceValuePa;
	s32 staticCaliPa;
	s32 cali_sw;
	bool startCali;
/* prize added by chenjiaxi, barometer calibration, 20220423-end */
};

/* prize added by chenjiaxi, barometer calibration, 20220423-start */
#define MAXIMUM_TOLERENCE_ERROR 5000 //vendor recommend 1000 here.
#define REQUIRED_DATA_NUMBER 5
static s32 baro_cali_data[2] = {0};
/* prize added by chenjiaxi, barometer calibration, 20220423-end */

static struct i2c_driver SPL_i2c_driver;
static struct SPL_i2c_data *obj_i2c_data;
static const struct i2c_device_id SPL_i2c_id[] = {
	{SPL_DEV_NAME, 0},
	{}
};

#ifdef CONFIG_MTK_LEGACY
static struct i2c_board_info SPL_i2c_info __initdata = {
	I2C_BOARD_INFO(SPL_DEV_NAME, SPL07_I2C_ADDRESS)
};
#endif
static int SPL_local_init(void);
static int SPL_remove(void);
static int SPL_init_flag = -1;
static struct baro_init_info SPL_init_info = {
	.name = "SPL",
	.init = SPL_local_init,
	.uninit = SPL_remove,
};

/* I2C operation functions */
static int SPL_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data,
			      u8 len)
{
	u8 reg_addr = addr;
	u8 *rxbuf = data;
	u8 left = len;
	u8 retry;
	u8 offset = 0;

	struct i2c_msg msg[2] = {
		{
			.addr = SPL07_I2C_ADDRESS,
			.flags = 0,
			.buf = &reg_addr,
			.len = 1,
		},
		{
		 .addr = SPL07_I2C_ADDRESS,
		 .flags = I2C_M_RD,
		},
	};

	if (rxbuf == NULL)
		return -1;

	while (left > 0) {
		retry = 0;
		reg_addr = addr + offset;
		msg[1].buf = &rxbuf[offset];

		if (left > C_I2C_FIFO_SIZE) {
			msg[1].len = C_I2C_FIFO_SIZE;
			left -= C_I2C_FIFO_SIZE;
			offset += C_I2C_FIFO_SIZE;
		} else {
			msg[1].len = left;
			left = 0;
		}

		while (i2c_transfer(client->adapter, &msg[0], 2) != 2) {
			retry++;

			if (retry == 20) {
				pr_err("i2c read reg=%#x length=%d failed\n",
					addr + offset, len);
				return -EIO;
			}
		}
	}

	return 0;
}

static int SPL_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data,
			       u8 len)
{
	u8 buffer[C_I2C_FIFO_SIZE];
	u8 *txbuf = data;
	u8 left = len;
	u8 offset = 0;
	u8 retry = 0;

	struct i2c_msg msg = {
		.addr = SPL07_I2C_ADDRESS, .flags = 0, .buf = buffer,
	};

	if (txbuf == NULL)
		return -1;

	while (left > 0) {
		retry = 0;
		/* register address */
		buffer[0] = addr + offset;

		if (left >= C_I2C_FIFO_SIZE) {
			memcpy(&buffer[1], &txbuf[offset], C_I2C_FIFO_SIZE - 1);
			msg.len = C_I2C_FIFO_SIZE;
			left -= C_I2C_FIFO_SIZE - 1;
			offset += C_I2C_FIFO_SIZE - 1;
		} else {
			memcpy(&buffer[1], &txbuf[offset], left);
			msg.len = left + 1;
			left = 0;
		}

		while (i2c_transfer(client->adapter, &msg, 1) != 1) {
			retry++;

			if (retry == 20) {
				pr_err("i2c write reg=%#x length=%d failed\n",
					buffer[0], len);
				return -EIO;
			}

			pr_debug("i2c write addr %#x, retry %d\n", buffer[0],
				retry);
		}
	}

	return 0;
}

/* get chip type */
static int SPL_get_chip_type(struct i2c_client *client)
{
	int err = 0;
	u8 chip_id = 0;
	struct SPL_i2c_data *obj = i2c_get_clientdata(client);

	/* pr_debug("%s\n", __func__);*/

	err = SPL_i2c_read_block(client, SPL_CHIP_ID_REG, &chip_id, 0x01);
	if (err != 0)
		return err;

	switch (chip_id) {
	case SPL07_CHIP_ID:
		obj->sensor_type = SPL07_TYPE;
		strlcpy(obj->sensor_name, "spl07", sizeof(obj->sensor_name));
		break;
	default:
		obj->sensor_type = INVALID_TYPE;
		strlcpy(obj->sensor_name, "unknown sensor",
			sizeof(obj->sensor_name));
		break;
	}

	if (atomic_read(&obj->trace) & BAR_TRC_INFO)
		pr_debug("[%s]chip id = %#x, sensor name = %s\n", __func__,
			chip_id, obj->sensor_name);

	if (obj->sensor_type == INVALID_TYPE) {
		pr_err("unknown pressure sensor\n");
		return -1;
	}
	return 0;
}

static int SPL_get_calibration_data(struct i2c_client *client)
{
	struct SPL_i2c_data *obj =
		(struct SPL_i2c_data *)i2c_get_clientdata(client);
	int status = 0;

	if (obj->sensor_type == SPL07_TYPE) {
		u8 a_data_u8r[SPL07_CALIBRATION_DATA_LENGTH] = {0};

		status = SPL_i2c_read_block(
			client, SPL07_CALIBRATION_DATA_START, a_data_u8r,
			SPL07_CALIBRATION_DATA_LENGTH);
		if (status < 0)
			return status;

		obj->SPL07_cali.C0 = (SPL07_S16_t)(
			(((SPL07_S16_t)a_data_u8r[0])
			 << SHIFT_LEFT_4_POSITION) |
			(a_data_u8r[1]>> SHIFT_RIGHT_4_POSITION));
		obj->SPL07_cali.C0 = (obj->SPL07_cali.C0 & 0x0800) ?
			(0xF000 | obj->SPL07_cali.C0) :
			obj->SPL07_cali.C0;
		obj->SPL07_cali.C1 = (SPL07_S16_t)(
			(((SPL07_S16_t)(a_data_u8r[1]&0x0F))
			 << SHIFT_LEFT_8_POSITION) |
			a_data_u8r[2]);
		obj->SPL07_cali.C1 = (obj->SPL07_cali.C1 & 0x0800) ?
			(0xF000 | obj->SPL07_cali.C1) :
			obj->SPL07_cali.C1;
		obj->SPL07_cali.C00 = (SPL07_S32_t)(
			(((SPL07_S32_t)(a_data_u8r[3]))
			 << SHIFT_LEFT_12_POSITION) |
			(((SPL07_S32_t)a_data_u8r[4])
			<< SHIFT_LEFT_4_POSITION)| 
			(((SPL07_S32_t)a_data_u8r[5])
				>> SHIFT_RIGHT_4_POSITION));
		obj->SPL07_cali.C00 = (obj->SPL07_cali.C00 & 0x080000) ?
			(0xFFF00000 | obj->SPL07_cali.C00) :
			obj->SPL07_cali.C00;
		obj->SPL07_cali.C10 = (SPL07_S32_t)(
			(((SPL07_S32_t)(a_data_u8r[5] & 0x0F))
				<< SHIFT_LEFT_16_POSITION) |
			(((SPL07_S32_t)a_data_u8r[6])
				<< SHIFT_LEFT_8_POSITION) |
			a_data_u8r[7]);
		obj->SPL07_cali.C10 = (obj->SPL07_cali.C10 & 0x080000) ?
			(0xFFF00000 | obj->SPL07_cali.C10) :
			obj->SPL07_cali.C10;
		obj->SPL07_cali.C01 = (SPL07_S16_t)(
			(((SPL07_S16_t)a_data_u8r[8])
			 << SHIFT_LEFT_8_POSITION) |
			a_data_u8r[9]);
		obj->SPL07_cali.C11 = (SPL07_S16_t)(
			(((SPL07_S16_t)a_data_u8r[10])
			 << SHIFT_LEFT_8_POSITION) |
			a_data_u8r[11]);
		obj->SPL07_cali.C20 = (SPL07_S16_t)(
			(((SPL07_S16_t)a_data_u8r[12])
			 << SHIFT_LEFT_8_POSITION) |
			a_data_u8r[13]);
		obj->SPL07_cali.C21 = (SPL07_S16_t)(
			(((SPL07_S16_t)a_data_u8r[14])
			 << SHIFT_LEFT_8_POSITION) |
			a_data_u8r[15]);
		obj->SPL07_cali.C30 = (SPL07_S16_t)(
			(((SPL07_S16_t)a_data_u8r[16])
			 << SHIFT_LEFT_8_POSITION) |
			a_data_u8r[17]);
		obj->SPL07_cali.C31 = (SPL07_S16_t)(
			(((SPL07_S16_t)a_data_u8r[18])
				<< SHIFT_LEFT_4_POSITION) |
			(a_data_u8r[19] >> SHIFT_RIGHT_4_POSITION));
		obj->SPL07_cali.C31 = (obj->SPL07_cali.C31 & 0x0800) ?
			(0xF000 | obj->SPL07_cali.C31) :
			obj->SPL07_cali.C31;
		obj->SPL07_cali.C40 = (SPL07_S16_t)(
			(((SPL07_S16_t)a_data_u8r[19] & 0x0F)
				<< SHIFT_LEFT_8_POSITION) |
			a_data_u8r[20]);
		obj->SPL07_cali.C40 = (obj->SPL07_cali.C40 & 0x0800) ?
			(0xF000 | obj->SPL07_cali.C40) :
			obj->SPL07_cali.C40;
	}
	return 0;
}

/*V1.2 background mode to idle mode*/
static int SPL_set_powermode(struct i2c_client *client,
			     enum SPL_POWERMODE_ENUM power_mode)
{
	struct SPL_i2c_data *obj = i2c_get_clientdata(client);
	u8 err = 0, data = 0, actual_power_mode = 0;

	if (atomic_read(&obj->trace) & BAR_TRC_INFO)
		pr_debug("[%s] p_m = %d, old p_m = %d\n", __func__,
			power_mode, obj->power_mode);

	if (power_mode == obj->power_mode)
		return 0;

	mutex_lock(&obj->lock);

	if (obj->sensor_type == SPL07_TYPE) { /* SPL07 */
		if (power_mode == SPL_SUSPEND_MODE) {
			actual_power_mode = SPL07_SLEEP_MODE;
			data = 0x09;
			SPL_i2c_write_block(
				client, SPL07_soft_reset_REG, &data, 1);
			mdelay(10);
		} else if (power_mode == SPL_NORMAL_MODE) {
			actual_power_mode = SPL07_NORMAL_MODE;

			data = 0x44;
			SPL_i2c_write_block(
				client, SPL07_PRS_CFG_REG, &data, 1);

			data = 0x40;
			SPL_i2c_write_block(
				client, SPL07_TEM_CFG_REG, &data, 1);

			data = 0x04;
			SPL_i2c_write_block(
				client, SPL07_T_P_SHIFT_CFG_REG, &data, 1);
		} else {
			err = -EINVAL;
			pr_err("invalid power mode = %d\n", power_mode);
			mutex_unlock(&obj->lock);
			return err;
		}
		data = actual_power_mode;
		err += SPL_i2c_write_block(
			client, SPL07_POWER_MODE_REG, &data, 1);
		pr_err("change the power mode, power mode = %d\n",
			data);

	}

	if (err < 0)
		pr_err("set power mode failed, err = %d, sensor name = %s\n",
			err, obj->sensor_name);
	else
		obj->power_mode = power_mode;
	if (actual_power_mode == SPL07_NORMAL_MODE)
	{
		obj->last_pressure_measurement = jiffies;
	}

	mutex_unlock(&obj->lock);
	return err;
}



static int SPL_set_sampling_p(struct i2c_client* client,
	enum SPL_SAMPLING_ENUM sampling_p)
{
	struct SPL_i2c_data* obj = i2c_get_clientdata(client);
	u8 err = 0, data = 0, actual_sampling_p = 0;

	if (atomic_read(&obj->trace) & BAR_TRC_INFO)
		pr_debug("[%s] sampling_p = %d, old sampling_p = %d\n",
			__func__, sampling_p, obj->sampling_p);

	if (sampling_p == obj->sampling_p)
		return 0;

	mutex_lock(&obj->lock);

	if (obj->sensor_type == SPL07_TYPE) { /* SPL07 */

		if (sampling_p == SPL_SAMPLING_1Hz)
		{
			actual_sampling_p = SPL07_SAMPLING_1Hz;
			obj->samp_times = SPL07_SAMPLING_1;
		}
		else if (sampling_p == SPL_SAMPLING_2Hz)
		{
			actual_sampling_p = SPL07_SAMPLING_2Hz;
			obj->samp_times = SPL07_SAMPLING_2;
		}
		else if (sampling_p == SPL_SAMPLING_4Hz)
		{
			actual_sampling_p = SPL07_SAMPLING_4Hz;
			obj->samp_times = SPL07_SAMPLING_4;
		}
		else if (sampling_p == SPL_SAMPLING_8Hz)
		{
			actual_sampling_p = SPL07_SAMPLING_8Hz;
			obj->samp_times = SPL07_SAMPLING_8;
		}
		else if (sampling_p == SPL_SAMPLING_16Hz)
		{
			actual_sampling_p = SPL07_SAMPLING_16Hz;
			obj->samp_times = SPL07_SAMPLING_16;
		}
		else if (sampling_p == SPL_SAMPLING_32Hz)
		{
			actual_sampling_p = SPL07_SAMPLING_32Hz;
			obj->samp_times = SPL07_SAMPLING_32;
		}
		else if (sampling_p == SPL_SAMPLING_64Hz)
		{
			actual_sampling_p = SPL07_SAMPLING_64Hz;
			obj->samp_times = SPL07_SAMPLING_64;
		}
		else if (sampling_p == SPL_SAMPLING_128Hz)
		{
			actual_sampling_p = SPL07_SAMPLING_128Hz;
			obj->samp_times = SPL07_SAMPLING_128;
		}
		else if (sampling_p == SPL_SAMPLING_25_16Hz)
		{
			actual_sampling_p = SPL07_SAMPLING_25_16Hz;
			obj->samp_times = SPL07_SAMPLING_25_16;
		}
		else if (sampling_p == SPL_SAMPLING_25_8Hz)
		{
			actual_sampling_p = SPL07_SAMPLING_25_8Hz;
			obj->samp_times = SPL07_SAMPLING_25_8;
		}
		else if (sampling_p == SPL_SAMPLING_25_4Hz)
		{
			actual_sampling_p = SPL07_SAMPLING_25_4Hz;
			obj->samp_times = SPL07_SAMPLING_25_4;
		}
		else if (sampling_p == SPL_SAMPLING_25_2Hz)
		{
			actual_sampling_p = SPL07_SAMPLING_25_2Hz;
			obj->samp_times = SPL07_SAMPLING_25_2;
		}
		else if (sampling_p == SPL_SAMPLING_25Hz)
		{
			actual_sampling_p = SPL07_SAMPLING_25Hz;
			obj->samp_times = SPL07_SAMPLING_25;
		}
		else if (sampling_p == SPL_SAMPLING_50Hz)
		{
			actual_sampling_p = SPL07_SAMPLING_50Hz;
			obj->samp_times = SPL07_SAMPLING_50;
		}
		else if (sampling_p == SPL_SAMPLING_100Hz)
		{
			actual_sampling_p = SPL07_SAMPLING_100Hz;
			obj->samp_times = SPL07_SAMPLING_100;
		}
		else if (sampling_p == SPL_SAMPLING_200Hz)
		{
			actual_sampling_p = SPL07_SAMPLING_200Hz;
			obj->samp_times = SPL07_SAMPLING_200;
		}
		else {
			err = -EINVAL;
			pr_err("invalid sampling_p = %d\n",
				sampling_p);
			mutex_unlock(&obj->lock);
			return err;
		}
		err = SPL_i2c_read_block(client, SPL07_PRS_CFG_REG_ODR__REG,
			&data, 1);
		data = SPL_SET_BITSLICE(data, SPL07_PRS_CFG_REG_ODR,
			actual_sampling_p);
		err += SPL_i2c_write_block(
			client, SPL07_PRS_CFG_REG_ODR__REG, &data, 1);
	}

	if (err < 0)
		pr_err("set pressure sampling failed, err = %d,sensor name = %s\n",
			err, obj->sensor_name);
	else
		obj->sampling_p = sampling_p;
	obj->pressure_measurement_period = HZ / obj->samp_times;
	mutex_unlock(&obj->lock);
	return err;
}

static int SPL_set_sampling_t(struct i2c_client* client,
	enum SPL_SAMPLING_ENUM sampling_t)
{
	struct SPL_i2c_data* obj = i2c_get_clientdata(client);
	u8 err = 0, data = 0, actual_sampling_t = 0;

	if (atomic_read(&obj->trace) & BAR_TRC_INFO)
		pr_debug("[%s] sampling_t = %d, old sampling_t = %d\n",
			__func__, sampling_t, obj->sampling_t);

	if (sampling_t == obj->sampling_t)
		return 0;

	mutex_lock(&obj->lock);

	if (obj->sensor_type == SPL07_TYPE) { /* SPL07 */

		if (sampling_t == SPL_SAMPLING_1Hz)
			actual_sampling_t = SPL07_SAMPLING_1Hz;
		else if (sampling_t == SPL_SAMPLING_2Hz)
			actual_sampling_t = SPL07_SAMPLING_2Hz;
		else if (sampling_t == SPL_SAMPLING_4Hz)
			actual_sampling_t = SPL07_SAMPLING_4Hz;
		else if (sampling_t == SPL_SAMPLING_8Hz)
			actual_sampling_t = SPL07_SAMPLING_8Hz;
		else if (sampling_t == SPL_SAMPLING_16Hz)
			actual_sampling_t = SPL07_SAMPLING_16Hz;
		else if (sampling_t == SPL_SAMPLING_32Hz)
			actual_sampling_t = SPL07_SAMPLING_32Hz;
		else if (sampling_t == SPL_SAMPLING_64Hz)
			actual_sampling_t = SPL07_SAMPLING_64Hz;
		else if (sampling_t == SPL_SAMPLING_128Hz)
			actual_sampling_t = SPL07_SAMPLING_128Hz;
		else if (sampling_t == SPL_SAMPLING_25_16Hz)
			actual_sampling_t = SPL07_SAMPLING_25_16Hz;
		else if (sampling_t == SPL_SAMPLING_25_8Hz)
			actual_sampling_t = SPL07_SAMPLING_25_8Hz;
		else if (sampling_t == SPL_SAMPLING_25_4Hz)
			actual_sampling_t = SPL07_SAMPLING_25_4Hz;
		else if (sampling_t == SPL_SAMPLING_25_2Hz)
			actual_sampling_t = SPL07_SAMPLING_25_2Hz;
		else if (sampling_t == SPL_SAMPLING_25Hz)
			actual_sampling_t = SPL07_SAMPLING_25Hz;
		else if (sampling_t == SPL_SAMPLING_50Hz)
			actual_sampling_t = SPL07_SAMPLING_50Hz;
		else if (sampling_t == SPL_SAMPLING_100Hz)
			actual_sampling_t = SPL07_SAMPLING_100Hz;
		else if (sampling_t == SPL_SAMPLING_200Hz)
			actual_sampling_t = SPL07_SAMPLING_200Hz;
		else {
			err = -EINVAL;
			pr_err("invalid sampling_p = %d\n",
				sampling_t);
			mutex_unlock(&obj->lock);
			return err;
		}
		err = SPL_i2c_read_block(client, SPL07_TEM_CFG_REG_ODR__REG,
			&data, 1);
		data = SPL_SET_BITSLICE(data, SPL07_TEM_CFG_REG_ODR,
			actual_sampling_t);
		err += SPL_i2c_write_block(
			client, SPL07_TEM_CFG_REG_ODR__REG, &data, 1);
	}

	if (err < 0)
		pr_err("set pressure sampling failed, err = %d,sensor name = %s\n",
			err, obj->sensor_name);
	else
		obj->sampling_t = sampling_t;

	mutex_unlock(&obj->lock);
	return err;
}

static int SPL_set_oversampling_p(struct i2c_client *client,
				  enum SPL_OVERSAMPLING_ENUM oversampling_p)
{
	struct SPL_i2c_data *obj = i2c_get_clientdata(client);
	u8 err = 0, data = 0, actual_oversampling_p = 0;

	if (atomic_read(&obj->trace) & BAR_TRC_INFO)
		pr_debug("[%s] oversampling_p = %d, old oversampling_p = %d\n",
			__func__, oversampling_p, obj->oversampling_p);

	if (oversampling_p == obj->oversampling_p)
		return 0;

	mutex_lock(&obj->lock);

	if (obj->sensor_type == SPL07_TYPE) { /* SPL07 */

		if (oversampling_p == SPL_OVERSAMPLING_SINGLE)
		{
			actual_oversampling_p = SPL07_OVERSAMPLING_SINGLE;
			obj->KP = SPL07_OVERSAMPLING_SINGLE_CSF;
		}
		else if (oversampling_p == SPL_OVERSAMPLING_2X)
		{
			actual_oversampling_p = SPL07_OVERSAMPLING_2X;
			obj->KP = SPL07_OVERSAMPLING_2X_CSF;
		}
		else if (oversampling_p == SPL_OVERSAMPLING_4X)
		{
			actual_oversampling_p = SPL07_OVERSAMPLING_4X;
			obj->KP = SPL07_OVERSAMPLING_4X_CSF;
		}
		else if (oversampling_p == SPL_OVERSAMPLING_8X)
		{
			actual_oversampling_p = SPL07_OVERSAMPLING_8X;
			obj->KP = SPL07_OVERSAMPLING_8X_CSF;
		}
		else if (oversampling_p == SPL_OVERSAMPLING_16X)
		{
			actual_oversampling_p = SPL07_OVERSAMPLING_16X;
			obj->KP = SPL07_OVERSAMPLING_16X_CSF;
		}
		else if (oversampling_p == SPL_OVERSAMPLING_32X)
		{
			actual_oversampling_p = SPL07_OVERSAMPLING_32X;
			obj->KP = SPL07_OVERSAMPLING_32X_CSF;
		}
		else if (oversampling_p == SPL_OVERSAMPLING_64X)
		{
			actual_oversampling_p = SPL07_OVERSAMPLING_64X;
			obj->KP = SPL07_OVERSAMPLING_64X_CSF;
		}
		else {
			err = -EINVAL;
			pr_err("invalid oversampling_p = %d\n",
				oversampling_p);
			mutex_unlock(&obj->lock);
			return err;
		}
		err = SPL_i2c_read_block(client, SPL07_PRS_CFG_REG_OSRSP__REG,
					 &data, 1);
		data = SPL_SET_BITSLICE(data, SPL07_PRS_CFG_REG_OSRSP,
					actual_oversampling_p);
		err += SPL_i2c_write_block(
			client, SPL07_PRS_CFG_REG_OSRSP__REG, &data, 1);
	}

	if (err < 0)
		pr_err("set pressure oversampling failed, err = %d,sensor name = %s\n",
			err, obj->sensor_name);
	else
		obj->oversampling_p = oversampling_p;
	if (obj->oversampling_p>3)
	{
		err = SPL_i2c_read_block(client, SPL07_P_SHIFT_CFG__REG,
			&data, 1);
		data = SPL_SET_BITSLICE(data, SPL07_P_SHIFT_CFG,
			0x01);
		err += SPL_i2c_write_block(
			client, SPL07_P_SHIFT_CFG__REG, &data, 1);
	}
	else
	{
		err = SPL_i2c_read_block(client, SPL07_P_SHIFT_CFG__REG,
			&data, 1);
		data = SPL_SET_BITSLICE(data, SPL07_P_SHIFT_CFG,
			0x0);
		err += SPL_i2c_write_block(
			client, SPL07_P_SHIFT_CFG__REG, &data, 1);
	}
	mutex_unlock(&obj->lock);
	return err;
}

static int SPL_set_oversampling_t(struct i2c_client *client,
				  enum SPL_OVERSAMPLING_ENUM oversampling_t)
{
	struct SPL_i2c_data *obj = i2c_get_clientdata(client);
	u8 err = 0, data = 0, actual_oversampling_t = 0;

	if (atomic_read(&obj->trace) & BAR_TRC_INFO)
		pr_debug("[%s] oversampling_t = %d, old oversampling_t = %d\n",
			__func__, oversampling_t, obj->oversampling_t);

	if (oversampling_t == obj->oversampling_t)
		return 0;

	mutex_lock(&obj->lock);

	if (obj->sensor_type == SPL07_TYPE) { /* SPL07 */
		if (oversampling_t == SPL_OVERSAMPLING_SINGLE)
		{
			actual_oversampling_t = SPL07_OVERSAMPLING_SINGLE;
			obj->KT = SPL07_OVERSAMPLING_SINGLE_CSF;
		}
		else if (oversampling_t == SPL_OVERSAMPLING_2X)
		{
			actual_oversampling_t = SPL07_OVERSAMPLING_2X;
			obj->KT = SPL07_OVERSAMPLING_2X_CSF;
		}
		else if (oversampling_t == SPL_OVERSAMPLING_4X)
		{
			actual_oversampling_t = SPL07_OVERSAMPLING_4X;
			obj->KT = SPL07_OVERSAMPLING_4X_CSF;
		}
		else if (oversampling_t == SPL_OVERSAMPLING_8X)
		{
			actual_oversampling_t = SPL07_OVERSAMPLING_8X;
			obj->KT = SPL07_OVERSAMPLING_8X_CSF;
		}
		else if (oversampling_t == SPL_OVERSAMPLING_16X)
		{
			actual_oversampling_t = SPL07_OVERSAMPLING_16X;
			obj->KT = SPL07_OVERSAMPLING_16X_CSF;
		}
		else if (oversampling_t == SPL_OVERSAMPLING_32X)
		{
			actual_oversampling_t = SPL07_OVERSAMPLING_32X;
			obj->KT = SPL07_OVERSAMPLING_32X_CSF;
		}
		else if (oversampling_t == SPL_OVERSAMPLING_64X)
		{
			actual_oversampling_t = SPL07_OVERSAMPLING_64X;
			obj->KT = SPL07_OVERSAMPLING_64X_CSF;
		}
		else {
			err = -EINVAL;
			pr_err("invalid oversampling_t = %d\n",oversampling_t);
			mutex_unlock(&obj->lock);
			return err;
		}
		err = SPL_i2c_read_block(client, SPL07_TEM_CFG_REG_OSRSP__REG,
					 &data, 1);
		data = SPL_SET_BITSLICE(data, SPL07_TEM_CFG_REG_OSRSP,
					actual_oversampling_t);
		err += SPL_i2c_write_block(
			client, SPL07_TEM_CFG_REG_OSRSP__REG, &data, 1);
	}

	if (err < 0)
		pr_err("set temperature oversampling failed, err = %d, sensor name = %s\n",
			err, obj->sensor_name);
	else
		obj->oversampling_t = oversampling_t;

	if (obj->oversampling_t > 3)
	{
		err = SPL_i2c_read_block(client, SPL07_T_SHIFT_CFG__REG,
			&data, 1);
		data = SPL_SET_BITSLICE(data, SPL07_T_SHIFT_CFG,
			0x01);
		err += SPL_i2c_write_block(
			client, SPL07_T_SHIFT_CFG__REG, &data, 1);
	}
	else
	{
		err = SPL_i2c_read_block(client, SPL07_T_SHIFT_CFG__REG,
			&data, 1);
		data = SPL_SET_BITSLICE(data, SPL07_T_SHIFT_CFG,
			0x00);
		err += SPL_i2c_write_block(
			client, SPL07_T_SHIFT_CFG__REG, &data, 1);
	}
	mutex_unlock(&obj->lock);
	return err;
}

static int SPL_read_raw_temperature(struct i2c_client *client, s32 *temperature)
{
	struct SPL_i2c_data *obj;
	s32 err = 0;
	s32 utemperature = 0;

	if (client == NULL) {
		err = -EINVAL;
		return err;
	}

	obj = i2c_get_clientdata(client);

	mutex_lock(&obj->lock);

	if (obj->sensor_type == SPL07_TYPE) { /* SPL07 */
		unsigned char a_data_u8r[3] = {0};

		err = SPL_i2c_read_block(client, SPL07_TEMPERATURE_MSB_REG,
					 a_data_u8r, 3);
		if (err < 0) {
			pr_err("read raw temperature failed, err = %d\n", err);
			mutex_unlock(&obj->lock);
			return err;
		}
		utemperature = (SPL07_S32_t)((((SPL07_U32_t)a_data_u8r[0])
			<< SHIFT_LEFT_16_POSITION) |
			(((SPL07_U32_t)a_data_u8r[1])
				<< SHIFT_LEFT_8_POSITION) |
			(SPL07_U32_t)a_data_u8r[2]);
		utemperature = (utemperature & 0x800000) ? (0xFF000000 | utemperature) : utemperature;
		*temperature = utemperature;
	}
	mutex_unlock(&obj->lock);

	return err;
}

static int SPL_read_raw_pressure(struct i2c_client *client, s32 *pressure)
{
	struct SPL_i2c_data *priv;
	s32 err = 0;
	s32 upressure = 0;

	if (client == NULL) {
		err = -EINVAL;
		return err;
	}

	priv = i2c_get_clientdata(client);

	mutex_lock(&priv->lock);

	if (priv->sensor_type == SPL07_TYPE) { /* SPL07 */
		unsigned char a_data_u8r[3] = {0};

		err = SPL_i2c_read_block(client, SPL07_PRESSURE_MSB_REG,
					 a_data_u8r, 3);
		if (err < 0) {
			pr_err("read raw pressure failed, err = %d\n", err);
			mutex_unlock(&priv->lock);
			return err;
		}
		upressure = (SPL07_S32_t)((((SPL07_U32_t)(a_data_u8r[0]))
					    << SHIFT_LEFT_16_POSITION) |
					   (((SPL07_U32_t)(a_data_u8r[1]))
					    << SHIFT_LEFT_8_POSITION) |
					   ((SPL07_U32_t)(a_data_u8r[2])));
		upressure = (upressure & 0x800000) ? (0xFF000000 | upressure) : upressure;
		*pressure = upressure;
	}
#ifdef CONFIG_SPL_LOWPASS
	/*
	 *Example: firlen = 16, filter buffer = [0] ... [15],
	 *when 17th data come, replace [0] with this new data.
	 *Then, average this filter buffer and report average value to upper
	 *layer.
	 */
	if (atomic_read(&priv->filter)) {
		if (atomic_read(&priv->fir_en) &&
		    !atomic_read(&priv->suspend)) {
			int idx, firlen = atomic_read(&priv->firlen);

			if (priv->fir.num < firlen) {
				priv->fir.raw[priv->fir.num][SPL_PRESSURE] =
					*pressure;
				priv->fir.sum[SPL_PRESSURE] += *pressure;
				if (atomic_read(&priv->trace) &
				    BAR_TRC_FILTER) {
					pr_debug("add [%2d] [%5d] => [%5d]\n",
						priv->fir.num,
						priv->fir.raw[priv->fir.num]
							     [SPL_PRESSURE],
						priv->fir.sum[SPL_PRESSURE]);
				}
				priv->fir.num++;
				priv->fir.idx++;
			} else {
				idx = priv->fir.idx % firlen;
				priv->fir.sum[SPL_PRESSURE] -=
					priv->fir.raw[idx][SPL_PRESSURE];
				priv->fir.raw[idx][SPL_PRESSURE] = *pressure;
				priv->fir.sum[SPL_PRESSURE] += *pressure;
				priv->fir.idx++;
				*pressure =
					priv->fir.sum[SPL_PRESSURE] / firlen;
				if (atomic_read(&priv->trace) &
				    BAR_TRC_FILTER) {
					pr_debug("add [%2d][%5d]=>[%5d]:[%5d]\n",
						idx,
						priv->fir
							.raw[idx][SPL_PRESSURE],
						priv->fir.sum[SPL_PRESSURE],
						*pressure);
				}
			}
		}
	}
#endif

	mutex_unlock(&priv->lock);
	return err;
}

/*
 *get compensated temperature
 *unit:10 degrees centigrade
 */
static int SPL_get_temperature(struct i2c_client *client, char *buf,
			       int bufsize)
{
	struct SPL_i2c_data *obj;
	int status;
	SPL07_S32_t utemp = 0; /* uncompensated temperature */
	SPL07_S64_t temperature = 0;
	SPL07_S64_t tem_left = 0;
	SPL07_S64_t REM = 0;
	SPL07_S32_t fTSC = 0;

	if (buf == NULL)
		return -1;

	if (client == NULL) {
		*buf = 0;
		return -2;
	}

	obj = i2c_get_clientdata(client);

	status = SPL_read_raw_temperature(client, &utemp);
	if (status != 0)
		return status;

	if (obj->sensor_type == SPL07_TYPE) { /* SPL07 */

		tem_left = ((s64)utemp) << 16;
		REM = div_s64(tem_left, obj->KT);
		fTSC = REM;

		/* Actual temperature should be divided by 256*/
		temperature = ((s32)obj->SPL07_cali.C0 << 7) + ((s32)((s64)obj->SPL07_cali.C1 * fTSC) >> 8);
		/* The result temperature unit should be 0.01deg */
		temperature = (temperature * 100) / 256;
	}

	sprintf(buf, "%08x", temperature);
	if (atomic_read(&obj->trace) & BAR_TRC_IOCTL) {
		pr_debug("temperature: %d\n", temperature);
		// pr_debug("temperature/100: %d\n", temperature / 100);
		pr_debug("compensated temperature value: %s\n", buf);
	}

	return status;
}

/*
 *get compensated pressure
 *unit: hectopascal(hPa)
 */
static int SPL_get_pressure(struct i2c_client* client, char* buf, int bufsize)
{
	struct SPL_i2c_data* obj;
	int status = 0;
	s32 upressure = 0, pressure = 0, utemp = 0;
	SPL07_S64_t fPsc = 0, fTsc = 0;
	SPL07_S64_t qua2, qua3,qua4;
	// int samp_hz = 0;
	SPL07_S64_t REM = 0;
	SPL07_S64_t tem_left = 0;
	SPL07_S64_t pres_left = 0;
	/* prize added by chenjiaxi, barometer calibration, 20220423-start */
	static s32 sumValuePa = 0, averageValuePa = 0;
	static uint8_t inputCount = 0;
	s32 tmpCali = 0;
	/* prize added by chenjiaxi, barometer calibration, 20220423-end */

	if (buf == NULL)
		return -1;

	if (client == NULL) {
		*buf = 0;
		return -2;
	}

	obj = i2c_get_clientdata(client);

	/* update the ambient temperature according to the given meas. period */
	/* below method will have false problem when jiffies wrap around.Barometer get baro
	 *so replace.
	 */
	if (time_before_eq((unsigned long)(obj->last_pressure_measurement +
		obj->pressure_measurement_period),
		jiffies)) {

		status = SPL_read_raw_temperature(client, &utemp);

		status += SPL_read_raw_pressure(client, &upressure);
		if (status != 0)
			goto exit;

		if (obj->sensor_type == SPL07_TYPE) { /* SPL07 */

			tem_left = ((s64)utemp) << 16;
			REM = div_s64(tem_left, obj->KT);
			fTsc = REM;

			pres_left = ((s64)upressure) << 16;
			REM = (div_s64(pres_left, obj->KP));
			fPsc = REM;

			qua4 = ((s32)obj->SPL07_cali.C30 << 8) + ((s32)((s64)obj->SPL07_cali.C40 * (s64)fPsc) >> 8);
			qua2 = ((s32)obj->SPL07_cali.C20 << 8) + (s32)(((s64)qua4 * (s64)fPsc) >> 16);
			qua3 = ((s32)obj->SPL07_cali.C10 << 8) + (s32)(((s64)qua2 * (s64)fPsc) >> 16);
			pressure = ((s32)obj->SPL07_cali.C00 << 8) + (s32)(((s64)qua3 * (s64)fPsc) >> 16);
			pressure += ((s32)((s32)obj->SPL07_cali.C01 * fTsc) >> 8);

			qua4 = ((s32)obj->SPL07_cali.C21 << 8) + ((s32)((s64)obj->SPL07_cali.C31 * (s64)fPsc) >> 8);
			qua2 = ((s64)obj->SPL07_cali.C11 << 8) + (s64)(((s64)qua4 * (s64)fPsc) >> 16);
			qua3 = (s32)(((s64)qua2 * (s64)fPsc) >> 16);
			pressure += (s32)(((s64)qua3 * (s64)fTsc) >> 16);
			/* Actual temperature should be divided by 256*/
			pressure = (pressure >> 8);
			obj->last_pressure_value = pressure;

		}

		sprintf(buf, "%08x", pressure);
		if (atomic_read(&obj->trace) & BAR_TRC_IOCTL) {
			pr_debug("pressure: %d\n", pressure);
			pr_debug("pressure/100: %d\n", pressure / 100);
			pr_debug("compensated pressure value: %s\n", buf);
		}
		obj->last_pressure_measurement = jiffies;
    }
	pressure = obj->last_pressure_value;
	/* prize added by chenjiaxi, barometer calibration, 20220423-start */
	if (obj_i2c_data->startCali) {
		inputCount++;
		if (inputCount <= REQUIRED_DATA_NUMBER) {
			sumValuePa += pressure;
			pr_err("%s cali raw:[%d %d]\n", __func__, sumValuePa, pressure);
			return status;
		} else {
			obj_i2c_data->startCali = false;
			inputCount = 0;
			averageValuePa = sumValuePa / REQUIRED_DATA_NUMBER;
			tmpCali = obj_i2c_data->referenceValuePa - averageValuePa;
			sumValuePa = 0;
			if (abs(tmpCali) > MAXIMUM_TOLERENCE_ERROR) {
				pr_err("%s cali fail, sumValuePa=%d, pressure=%d]\n", __func__, sumValuePa, pressure);
			} else {
				baro_cali_data[0] = obj_i2c_data->referenceValuePa;
				baro_cali_data[1] = obj_i2c_data->staticCaliPa = tmpCali;
				pr_err("%s cali success, referenceValuePa=%d, staticCaliPa=%d\n", 
					__func__, obj_i2c_data->referenceValuePa, obj_i2c_data->staticCaliPa);
			}
		}
	}
	pressure += obj_i2c_data->staticCaliPa;
	/* prize added by chenjiaxi, barometer calibration, 20220423-end */
	sprintf(buf, "%08x", pressure);
	// pr_err("pressure = %d\n", pressure);
	exit:
		return status;
}


/* SPL setting initialization */
static int SPL_init_client(struct i2c_client *client)
{
	int err = 0;

	/* pr_debug("%s\n", __func__); */

	err = SPL_get_chip_type(client);
	if (err < 0) {
		pr_err("get chip type failed, err = %d\n", err);
		return err;
	}

	err = SPL_get_calibration_data(client);
	if (err < 0) {
		pr_err("get calibration data failed, err = %d\n", err);
		return err;
	}

	err = SPL_set_oversampling_p(client, SPL_OVERSAMPLING_16X);
	if (err < 0) {
		pr_err("set pressure oversampling failed, err = %d\n", err);
		return err;
	}

	err = SPL_set_oversampling_t(client, SPL_OVERSAMPLING_SINGLE);
	if (err < 0) {
		pr_err("set temperature oversampling failed, err = %d\n", err);
		return err;
    }


		err = SPL_set_sampling_p(client, SPL_SAMPLING_16Hz);
		if (err < 0) {
			pr_err("set pressure sampling failed, err = %d\n", err);
			return err;
		}

		err = SPL_set_sampling_t(client, SPL_SAMPLING_16Hz);
		if (err < 0) {
			pr_err("set temperature sampling failed, err = %d\n", err);
			return err;
        }

	//err = SPL_set_powermode(client, SPL_SUSPEND_MODE);
		//if (err < 0) {
			//pr_err("set power mode failed, err = %d\n", err);
			//return err;
	//	}



	return 0;
}

static int SPL07_verify_i2c_disable_switch(struct SPL_i2c_data* obj)
{
	int err = 0;
	u8 reg_val = 0xFF;

	err = SPL_i2c_read_block(obj->client, SPL07_I2C_DISABLE_SWITCH,
				 &reg_val, 1);
	if (err < 0) {
		err = -EIO;
		pr_err("bus read failed\n");
		return err;
	}

	if (reg_val == 0x00) {
		pr_debug("SPL07 i2c interface is available\n");
		return 0;
	}

	pr_err("verification of i2c interface is failure\n");
	return -1;  
}

static int SPL_check_calib_param(struct SPL_i2c_data *obj)
{
	struct SPL07_calibration_data *cali = &(obj->SPL07_cali);

	/* verify that not all calibration parameters are 0 */
	if (cali->C0 == 0 && cali->C1 == 0 && cali->C00 == 0 &&
	    cali->C10 == 0 && cali->C01 == 0 && cali->C11 == 0 &&
	    cali->C20 == 0 && cali->C21 == 0 && cali->C30 == 0 &&
	    cali->C31 == 0 && cali->C40 == 0) {
		pr_err("all calibration parameters are zero\n");
		return -2;
	}

	/* verify whether all the calibration parameters are within range 
	if (cali->dig_T1 < 19000 || cali->dig_T1 > 35000)
		return -3;
	else if (cali->dig_T2 < 22000 || cali->dig_T2 > 30000)
		return -4;
	else if (cali->dig_T3 < -3000 || cali->dig_T3 > -1000)
		return -5;
	else if (cali->dig_P1 < 30000 || cali->dig_P1 > 42000)
		return -6;
	else if (cali->dig_P2 < -12970 || cali->dig_P2 > -8000)
		return -7;
	else if (cali->dig_P3 < -5000 || cali->dig_P3 > 8000)
		return -8;
	else if (cali->dig_P4 < -10000 || cali->dig_P4 > 18000)
		return -9;
	else if (cali->dig_P5 < -500 || cali->dig_P5 > 1100)
		return -10;
	else if (cali->dig_P6 < -1000 || cali->dig_P6 > 1000)
		return -11;
	else if (cali->dig_P7 < -32768 || cali->dig_P7 > 32767)
		return -12;
	else if (cali->dig_P8 < -30000 || cali->dig_P8 > 10000)
		return -13;
	else if (cali->dig_P9 < -10000 || cali->dig_P9 > 30000)
		return -14;

	pr_debug("calibration parameters are OK\n");*/
	return 0;
}

static int SPL_check_pt(struct SPL_i2c_data *obj)
{
	int err = 0;
	int temperature = -5000;
	int pressure = -1;
	char t[SPL_BUFSIZE] = "", p[SPL_BUFSIZE] = "";

	err = SPL_set_powermode(obj->client, SPL_NORMAL_MODE);
	if (err < 0) {
		pr_err("set power mode failed, err = %d\n", err);
		return -15;
	}

	mdelay(50);

	/* check ut and t */
	SPL_get_temperature(obj->client, t, SPL_BUFSIZE);
	if (kstrtoint(t, 16, &temperature) != 1)
		pr_err("sscanf parsing fail\n");
	if (temperature <= -40 * 100 || temperature >= 85 * 100) {
		pr_err("temperature value is out of range:%d*0.01degree\n",
			temperature);
		return -16;
	}

	/* check up and p */
	SPL_get_pressure(obj->client, p, SPL_BUFSIZE);
	if (kstrtoint(p, 16, &pressure) != 1)
		pr_err("sscanf parsing fail\n");
	if (pressure <= 300 * 100 || pressure >= 1100 * 100) {
		pr_err("pressure value is out of range:%d Pa\n", pressure);
		return -17;
	}

	pr_debug("SPL07 temperature and pressure values are OK\n");
	return 0;
}

static int SPL_do_selftest(struct SPL_i2c_data *obj)
{
	int err = 0;
	/* 0: failed, 1: success */
	u8 selftest;

	err = SPL07_verify_i2c_disable_switch(obj);
	if (err) {
		selftest = 0;
		pr_err("SPL07_verify_i2c_disable_switch:err=%d\n", err);
		goto exit;
	}

	err = SPL_check_calib_param(obj);
	if (err) {
		selftest = 0;
		pr_err("SPL_check_calib_param:err=%d\n", err);
		goto exit;
	}

	err = SPL_check_pt(obj);
	if (err) {
		selftest = 0;
		pr_err("SPL_check_pt:err=%d\n", err);
		goto exit;
	}

	/* selftest is OK */
	selftest = 1;
	pr_debug("SPL07 self test is OK\n");
exit:
	return selftest;
}

static ssize_t chipinfo_show(struct device_driver *ddri, char *buf)
{
	struct SPL_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		pr_err("SPL i2c data pointer is null\n");
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", obj->sensor_name);
}

static ssize_t sensordata_show(struct device_driver *ddri, char *buf)
{
	struct SPL_i2c_data *obj = obj_i2c_data;
	char strbuf[SPL_BUFSIZE] = "";

	if (obj == NULL) {
		pr_err("SPL i2c data pointer is null\n");
		return 0;
	}

	SPL_get_pressure(obj->client, strbuf, SPL_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static ssize_t trace_show(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct SPL_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		pr_err("SPL i2c data pointer is null\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

static ssize_t trace_store(struct device_driver *ddri, const char *buf,
				 size_t count)
{
	struct SPL_i2c_data *obj = obj_i2c_data;
	int trace;

	if (obj == NULL) {
		pr_err("i2c_data obj is null\n");
		return 0;
	}

	if (sscanf(buf, "0x%x", &trace) == 1)
		atomic_set(&obj->trace, trace);
	else
		pr_err("invalid content: '%s', length = %d\n", buf,
			(int)count);

	return count;
}

static ssize_t status_show(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct SPL_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		pr_err("SPL i2c data pointer is null\n");
		return 0;
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "CUST: %d %d (%d %d)\n",
			obj->hw.i2c_num, obj->hw.direction, obj->hw.power_id,
			obj->hw.power_vol);

	len += snprintf(buf + len, PAGE_SIZE - len, "i2c addr:%#x,ver:%s\n",
			obj->client->addr, SPL_DRIVER_VERSION);

	return len;
}

static ssize_t powermode_show(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct SPL_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		pr_err("SPL i2c data pointer is null\n");
		return 0;
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "%s mode\n",
			obj->power_mode == SPL_NORMAL_MODE ? "normal"
							   : "suspend");

	return len;
}

static ssize_t powermode_store(struct device_driver *ddri,
				      const char *buf, size_t count)
{
	struct SPL_i2c_data *obj = obj_i2c_data;
	unsigned long power_mode;
	int err;

	if (obj == NULL) {
		pr_err("SPL i2c data pointer is null\n");
		return 0;
	}

	err = kstrtoul(buf, 10, &power_mode);

	if (err == 0) {
		err = SPL_set_powermode(
			obj->client, (enum SPL_POWERMODE_ENUM)(!!(power_mode)));
		if (err)
			return err;
		return count;
	}
	return err;
}

static ssize_t selftest_show(struct device_driver *ddri, char *buf)
{
	struct SPL_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		pr_err("SPL i2c data pointer is null\n");
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", SPL_do_selftest(obj));
}

/* prize added by chenjiaxi, barometer calibration, 20220423-start */
static int SPL_set_cali(uint8_t *data, uint8_t count)
{
	int32_t *buf = (int32_t *)data;

	obj_i2c_data->referenceValuePa = buf[0];
	obj_i2c_data->staticCaliPa = buf[1];
	pr_err("cjx:%s referenceValuePa=%d, staticCaliPa=%d\n", 
		__func__, obj_i2c_data->referenceValuePa, obj_i2c_data->staticCaliPa);

	return 0;
}

static ssize_t test_cali_store(struct device_driver *ddri,
				      const char *buf, size_t count)
{
	int enable = 0, ret = 0;
	
	ret = kstrtoint(buf, 10, &enable);
	if (ret != 0) {
		pr_err("%s, kstrtoint fail\n", __func__);
		return ret;
	}
	pr_err("cjx:%s enable=%d\n", __func__, enable);
	if (enable == 1)
		obj_i2c_data->startCali = true;
	return count;
}


static ssize_t read_cali_data_show(struct device_driver *ddri, char *buf)
{
	ssize_t res = 0;

	baro_cali_report((int32_t *)baro_cali_data);
	res = snprintf(buf, PAGE_SIZE, "%d %d\n", baro_cali_data[0], baro_cali_data[1]);
	
	return res < PAGE_SIZE ? res : -EINVAL;
}
/* prize added by chenjiaxi, barometer calibration, 20220423-end */

static DRIVER_ATTR_RO(chipinfo);//, 0444, show_chipinfo_value, NULL);
static DRIVER_ATTR_RO(sensordata);//, 0444, show_sensordata_value, NULL);
static DRIVER_ATTR_RW(trace);//, 0644, show_trace_value, store_trace_value);
static DRIVER_ATTR_RO(status);//, 0444, show_status_value, NULL);
static DRIVER_ATTR_RW(powermode);//, 0644, show_power_mode_value, store_power_mode_value);
static DRIVER_ATTR_RO(selftest);//, 0444, show_selftest_value, NULL);
/* prize added by chenjiaxi, barometer calibration, 20220423-start */
static DRIVER_ATTR_WO(test_cali);
static DRIVER_ATTR_RO(read_cali_data);
/* prize added by chenjiaxi, barometer calibration, 20220423-end */

static struct driver_attribute *SPL_attr_list[] = {
	&driver_attr_chipinfo,   /* chip information */
	&driver_attr_sensordata, /* dump sensor data */
	&driver_attr_trace,      /* trace log */
	&driver_attr_status,     /* cust setting */
	&driver_attr_powermode,  /* power mode */
	&driver_attr_selftest,   /* self test */
	/* prize added by chenjiaxi, barometer calibration, 20220423-start */
	&driver_attr_test_cali,  /* test cali */
	&driver_attr_read_cali_data, /* read cali data */
	/* prize added by chenjiaxi, barometer calibration, 20220423-end */
};

static int SPL_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(SPL_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, SPL_attr_list[idx]);
		if (err) {
			pr_err("driver_create_file (%s) = %d\n",
				SPL_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int SPL_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(SPL_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, SPL_attr_list[idx]);

	return err;
}

#ifdef CONFIG_ID_TEMPERATURE
int temperature_operate(void *self, uint32_t command, void *buff_in,
			int size_in, void *buff_out, int size_out,
			int *actualout)
{
	int err = 0;
	int value;
	struct SPL_i2c_data *priv = (struct SPL_i2c_data *)self;
	hwm_sensor_data *temperature_data;
	char buff[SPL_BUFSIZE];

	switch (command) {
	case SENSOR_DELAY:
		/* under construction */
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			pr_err("enable sensor parameter error\n");
			err = -EINVAL;
		} else {
			/* value:[0--->suspend, 1--->normal] */
			value = *(int *)buff_in;
			pr_debug("sensor enable/disable command: %s\n",
				value ? "enable" : "disable");

			err = SPL_set_powermode(
				priv->client,
				(enum SPL_POWERMODE_ENUM)(!!value));
			if (err)
				pr_err("set power mode failed, err = %d\n",
					err);
		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) ||
		    (size_out < sizeof(hwm_sensor_data))) {
			pr_err("get sensor data parameter error\n");
			err = -EINVAL;
		} else {
			temperature_data = (hwm_sensor_data *)buff_out;
			err = SPL_get_temperature(priv->client, buff,
						  SPL_BUFSIZE);
			if (err) {
				pr_err("get compensated temperature value failed,err = %d\n",
					err);
				return -1;
			}
			if (kstrtoint(buff, 16, &temperature_data->values[0]) !=
			    1)
				pr_err("sscanf parsing fail\n");
			temperature_data->values[1] =
				temperature_data->values[2] = 0;
			temperature_data->status = SENSOR_STATUS_ACCURACY_HIGH;
			temperature_data->value_divide = 100;
		}
		break;

	default:
		pr_err("temperature operate function no this parameter %d\n",
			command);
		err = -1;
		break;
	}

	return err;
}
#endif /* CONFIG_ID_TEMPERATURE */

#ifdef CONFIG_PM_SLEEP
static int SPL_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct SPL_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	if (obj == NULL) {
		pr_err("null pointer\n");
		return -EINVAL;
	}

	if (atomic_read(&obj->trace) & BAR_TRC_INFO)
		pr_debug("%s\n", __func__);

	atomic_set(&obj->suspend, 1);
	err = SPL_set_powermode(obj->client, SPL_SUSPEND_MODE);
	if (err) {
		pr_err("SPL set suspend mode failed, err = %d\n", err);
		return err;
	}
	return err;
}

static int SPL_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct SPL_i2c_data *obj = i2c_get_clientdata(client);
	int err;

	if (obj == NULL) {
		pr_err("null pointer\n");
		return -EINVAL;
	}

	if (atomic_read(&obj->trace) & BAR_TRC_INFO)
		pr_debug("%s\n", __func__);

	err = SPL_init_client(obj->client);
	if (err) {
		pr_err("initialize client fail\n");
		return err;
	}

	err = SPL_set_powermode(obj->client, SPL_NORMAL_MODE);
	if (err) {
		pr_err("SPL set normal mode failed, err = %d\n", err);
		return err;
	}
#ifdef CONFIG_SPL_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));
#endif

	atomic_set(&obj->suspend, 0);
	return 0;
}
#endif

static int SPL_i2c_detect(struct i2c_client *client,
			  struct i2c_board_info *info)
{
	strlcpy(info->type, SPL_DEV_NAME, sizeof(info->type));
	return 0;
}

static int SPL_open_report_data(int open)
{
	/* should queuq work to report event if  is_report_input_direct=true */
	return 0;
}

static int SPL_enable_nodata(int en)
{
	struct SPL_i2c_data *obj = i2c_get_clientdata(obj_i2c_data->client);
	int res = 0;
	int retry = 0;
	bool power = false;

	if (en == 1)
		power = true;

	if (en == 0)
		power = false;

	for (retry = 0; retry < 3; retry++) {
		res = SPL_set_powermode(obj_i2c_data->client,
					(enum SPL_POWERMODE_ENUM)(!!power));
		if (res == 0) {
			pr_debug("SPL_set_powermode done\n");
			break;
		}
		pr_err("SPL_set_powermode fail\n");
	}
	obj->last_pressure_measurement = jiffies - obj->pressure_measurement_period;

	if (res != 0) {
		pr_err("SPL_set_powermode fail!\n");
		return -1;
	}
	pr_debug("SPL_set_powermode OK!\n");
	return 0;
}

static int SPL_set_delay(u64 ns)
{
	return 0;
}

static int SPL_batch(int flag, int64_t samplingPeriodNs,
		     int64_t maxBatchReportLatencyNs)
{
	return SPL_set_delay(samplingPeriodNs);
}

static int SPL_flush(void)
{
	return baro_flush_report();
}

static int SPL_get_data(int *value, int *status)
{
	char buff[SPL_BUFSIZE];
	int err = 0;

	err = SPL_get_pressure(obj_i2c_data->client, buff, SPL_BUFSIZE);
	if (err) {
		pr_err("get compensated pressure value failed, err = %d\n",
			err);
		return -1;
	}
	if (kstrtoint(buff, 16, value) != 0)
		pr_err("sscanf parsing fail\n");
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;

	return 0;
}

static int SPL_factory_enable_sensor(bool enabledisable,
				     int64_t sample_periods_ms)
{
	int err = 0;

	err = SPL_enable_nodata(enabledisable == true ? 1 : 0);
	if (err) {
		pr_err("%s enable sensor failed!\n", __func__);
		return -1;
	}
	err = SPL_batch(0, sample_periods_ms * 1000000, 0);
	if (err) {
		pr_err("%s enable set batch failed!\n", __func__);
		return -1;
	}
	return 0;
}
static int SPL_factory_get_data(int32_t *data)
{
	int err = 0, status = 0;

	err = SPL_get_data(data, &status);
	if (err < 0) {
		pr_err("%s get data fail\n", __func__);
		return -1;
	}
	return 0;
}
static int SPL_factory_get_raw_data(int32_t *data)
{
	return 0;
}
static int SPL_factory_enable_calibration(void)
{
	return 0;
}
static int SPL_factory_clear_cali(void)
{
	return 0;
}
static int SPL_factory_set_cali(int32_t offset)
{
	return 0;
}
static int SPL_factory_get_cali(int32_t *offset)
{
	return 0;
}
static int SPL_factory_do_self_test(void)
{
	return 0;
}

static struct baro_factory_fops SPL_factory_fops = {
	.enable_sensor = SPL_factory_enable_sensor,
	.get_data = SPL_factory_get_data,
	.get_raw_data = SPL_factory_get_raw_data,
	.enable_calibration = SPL_factory_enable_calibration,
	.clear_cali = SPL_factory_clear_cali,
	.set_cali = SPL_factory_set_cali,
	.get_cali = SPL_factory_get_cali,
	.do_self_test = SPL_factory_do_self_test,
};

static struct baro_factory_public SPL_factory_device = {
	.gain = 1, .sensitivity = 1, .fops = &SPL_factory_fops,
};

static int SPL_i2c_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct SPL_i2c_data *obj = NULL;
	struct baro_control_path ctl = {0};
	struct baro_data_path data = {0};
#ifdef CONFIG_ID_TEMPERATURE
	struct hwmsen_object sobj_t;
#endif
	int err = 0;

	pr_debug("%s\n", __func__);

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	err = get_baro_dts_func(client->dev.of_node, &obj->hw);
	if (err < 0) {
		pr_err("get cust_baro dts info fail\n");
		goto exit_init_client_failed;
	}

	obj_i2c_data = obj;
	obj->client = client;
	i2c_set_clientdata(client, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);
	obj->power_mode = SPL_UNDEFINED_POWERMODE;
	obj->oversampling_p = SPL_UNDEFINED_OVERSAMPLING;
	obj->oversampling_t = SPL_UNDEFINED_OVERSAMPLING;
	obj->sampling_p = SPL_UNDEFINED_SAMPLING;
	obj->sampling_t = SPL_UNDEFINED_SAMPLING;
	obj->last_pressure_measurement = 0;
	obj->pressure_measurement_period = 1 * HZ; /* temperature update period:1s */
	mutex_init(&obj->lock);

#ifdef CONFIG_SPL_LOWPASS
	if (obj->hw.firlen > C_MAX_FIR_LENGTH)
		atomic_set(&obj->firlen, C_MAX_FIR_LENGTH);
	else
		atomic_set(&obj->firlen, obj->hw.firlen);

	if (atomic_read(&obj->firlen) > 0)
		atomic_set(&obj->fir_en, 1);
#endif

	err = SPL_init_client(client);
	if (err)
		goto exit_init_client_failed;

	/* err = misc_register(&SPL_device); */
	err = baro_factory_device_register(&SPL_factory_device);
	if (err) {
		pr_err("baro_factory device register failed, err = %d\n", err);
		goto exit_misc_device_register_failed;
	}

	err = SPL_create_attr(&(SPL_init_info.platform_diver_addr->driver));
	if (err) {
		pr_err("create attribute failed, err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.is_use_common_factory = false;
	ctl.open_report_data = SPL_open_report_data;
	ctl.enable_nodata = SPL_enable_nodata;
	ctl.set_delay = SPL_set_delay;
	/* prize added by chenjiaxi, barometer calibration, 20220423-start */
	ctl.set_cali = SPL_set_cali;
	/* prize added by chenjiaxi, barometer calibration, 20220423-end */
	ctl.batch = SPL_batch;
	ctl.flush = SPL_flush;

	ctl.is_report_input_direct = false;
	ctl.is_support_batch = obj->hw.is_batch_supported;

	err = baro_register_control_path(&ctl);
	if (err) {
		pr_err("register baro control path err\n");
		goto exit_hwmsen_attach_pressure_failed;
	}

	data.get_data = SPL_get_data;
	data.vender_div = 100;
	err = baro_register_data_path(&data);
	if (err) {
		pr_err("baro_register_data_path failed, err = %d\n", err);
		goto exit_hwmsen_attach_pressure_failed;
	}

#ifdef CONFIG_ID_TEMPERATURE
	sobj_t.self = obj;
	sobj_t.polling = 1;
	sobj_t.sensor_operate = temperature_operate;
	err = hwmsen_attach(ID_TEMPRERATURE, &sobj_t);
	if (err) {
		pr_err("hwmsen attach failed, err = %d\n", err);
		goto exit_hwmsen_attach_temperature_failed;
	}
#endif /* CONFIG_ID_TEMPERATURE */

	SPL_init_flag = 0;
	/* prize added by chenjiaxi, 8802 info, 20220429-start */
#ifdef CONFIG_PRIZE_HARDWARE_INFO
	strcpy(current_barosensor_info.chip, "spl07");
	sprintf(current_barosensor_info.id, "0x%x", SPL07_CHIP_ID);
	strcpy(current_barosensor_info.vendor, "Goermicro");
	strcpy(current_barosensor_info.more, "barometer");
#endif
/* prize added by chenjiaxi, 8802 info, 20220429-end */
	pr_debug("%s: OK\n", __func__);
	return 0;

#ifdef CONFIG_ID_TEMPERATURE
exit_hwmsen_attach_temperature_failed:
	hwmsen_detach(ID_PRESSURE);
#endif /* CONFIG_ID_TEMPERATURE */
exit_hwmsen_attach_pressure_failed:
	SPL_delete_attr(&(SPL_init_info.platform_diver_addr->driver));
exit_create_attr_failed:
/* misc_deregister(&SPL_device); */
exit_misc_device_register_failed:
exit_init_client_failed:
	kfree(obj);
exit:
	obj = NULL;
	obj_i2c_data = NULL;
	pr_err("err = %d\n", err);
	SPL_init_flag = -1;
	return err;
}

static int SPL_i2c_remove(struct i2c_client *client)
{
	int err = 0;

#ifdef CONFIG_ID_TEMPERATURE
	err = hwmsen_detach(ID_TEMPRERATURE);
	if (err)
		pr_err("hwmsen_detach ID_TEMPRERATURE failed, err = %d\n",
			err);
#endif

	err = SPL_delete_attr(&(SPL_init_info.platform_diver_addr->driver));
	if (err)
		pr_err("SPL_delete_attr failed, err = %d\n", err);

	/* misc_deregister(&SPL_device); */
	baro_factory_device_deregister(&SPL_factory_device);

	obj_i2c_data = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}

static int SPL_remove(void)
{
	/*struct baro_hw *hw = get_cust_baro(); */

	pr_debug("%s\n", __func__);
	i2c_del_driver(&SPL_i2c_driver);
	return 0;
}

static int SPL_local_init(void)
{
	if (i2c_add_driver(&SPL_i2c_driver)) {
		pr_err("add driver error\n");
		return -1;
	}
	if (-1 == SPL_init_flag)
		return -1;

	/* pr_debug("fwq loccal init---\n"); */
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id baro_of_match[] = {
	{.compatible = "mediatek,barometer"},
	{.compatible = "goermicro,spl07"},
	{},
};
#endif

#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops SPL07_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(SPL_suspend, SPL_resume)};
#endif

static struct i2c_driver SPL_i2c_driver = {
	.driver = {

			.owner = THIS_MODULE,
			.name = SPL_DEV_NAME,
#ifdef CONFIG_PM_SLEEP
			.pm = &SPL07_pm_ops,
#endif
#ifdef CONFIG_OF
			.of_match_table = baro_of_match,
#endif
		},
	.probe = SPL_i2c_probe,
	.remove = SPL_i2c_remove,
	.detect = SPL_i2c_detect,
	.id_table = SPL_i2c_id,
};

static int __init SPL_init(void)
{
	pr_debug("%s\n", __func__);
#ifdef CONFIG_MTK_LEGACY
	i2c_register_board_info(hw.i2c_num, &SPL_i2c_info, 1);
#endif
	baro_driver_add(&SPL_init_info);

	return 0;
}

static void __exit SPL_exit(void)
{
	pr_debug("%s\n", __func__);
}
module_init(SPL_init);
module_exit(SPL_exit);

MODULE_DESCRIPTION("SPL07 I2C Driver");
MODULE_VERSION(SPL_DRIVER_VERSION);