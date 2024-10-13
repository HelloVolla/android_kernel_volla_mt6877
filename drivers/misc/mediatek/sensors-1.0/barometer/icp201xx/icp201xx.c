/*****************************************************************************
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * Accelerometer Sensor Driver
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 *****************************************************************************/

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/math64.h>
#include <linux/time.h>

#include <cust_baro.h>
#include "icp201xx.h"
#include "barometer.h"
/* #include <linux/hwmsen_helper.h> */

/* prize added by chenjiaxi, 8802 info, 20220429-start */
#ifdef CONFIG_PRIZE_HARDWARE_INFO
#include "../../../hardware_info/hardware_info.h"
extern struct hardware_info current_barosensor_info;
#endif
/* prize added by chenjiaxi, 8802 info, 20220429-end */

/* #define POWER_NONE_MACRO MT65XX_POWER_NONE */

/* sensor type */
enum SENSOR_TYPE_ENUM {
	ICP201XX_TYPE = 0x0,
	INVALID_TYPE = 0xff
};

/* power mode */
enum ICP_POWERMODE_ENUM {
	ICP_SUSPEND_MODE = 0x0,
	ICP_NORMAL_MODE,

	ICP_UNDEFINED_POWERMODE = 0xff
};

/* trace */
enum BAR_TRC {
	BAR_TRC_READ = 0x01,
	BAR_TRC_RAWDATA = 0x02,
	BAR_TRC_IOCTL = 0x04,
	BAR_TRC_FILTER = 0x08,
	BAR_TRC_INFO = 0x10,
};

icp201xx_chip_version_t icp201xx_dev_version = ICP201XX_CHIP_VERSION_A;

/* icp i2c client data */
struct icp_i2c_data {
	struct i2c_client *client;
	struct baro_hw hw;

	/* sensor info */
	u8 sensor_name[MAX_SENSOR_NAME];
	enum SENSOR_TYPE_ENUM sensor_type;
	//enum ICP_POWERMODE_ENUM power_mode;
	/*misc */
	struct mutex lock;
	atomic_t trace;
	atomic_t suspend;

    int16_t otp[4];
	uint32_t baroRate;
	uint8_t MeasureMode;
	uint8_t measure_cmd[2];
    uint64_t StartMeasureTime;      //us unit
    uint64_t MeasureTime;	         //us unit
    bool use_old_sample;
    unsigned int previous_sample;
    icp201xx_FIFO_readout_mode_t	fifo_readout_mode;
    int     pre_pressure_data;
    uint8_t fifo_packets_to_skip;
    uint16_t odr_setting;
/* prize added by chenjiaxi, barometer calibration, 20220423-start */
	s32 referenceValuePa;
	s32 staticCaliPa;
	s32 cali_sw;
	bool startCali;
/* prize added by chenjiaxi, barometer calibration, 20220423-end */
};

/* prize added by chenjiaxi, barometer calibration, 20220423-start */
#define MAXIMUM_TOLERENCE_ERROR 5000 //vendor recommend 1000 here.
#define REQUIRED_DATA_NUMBER 8
static s32 baro_cali_data[2] = {0};
/* prize added by chenjiaxi, barometer calibration, 20220423-end */

#define BAR_TAG                  "[barometer] "
#define BAR_FUN(f)               pr_err(BAR_TAG"%s\n", __func__)
#define BAR_ERR(fmt, args...) \
	pr_notice(BAR_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define BAR_LOG(fmt, args...)    pr_err(BAR_TAG fmt, ##args)

static DEFINE_MUTEX(i2c_mutex);
static DEFINE_MUTEX(get_data_mutex);

static struct i2c_driver icp_i2c_driver;
static struct icp_i2c_data *obj_i2c_data;
static const struct i2c_device_id icp_i2c_id[] = {
	{ICP_DEV_NAME, 0},
	{}
};

#ifdef CONFIG_MTK_LEGACY
static struct i2c_board_info icp_i2c_info __initdata = {
	I2C_BOARD_INFO(ICP_DEV_NAME, ICP201XX_I2C_ADDRESS)
};
#endif
static int icp_local_init(void);
static int icp_remove(void);
static int icp_init_flag = -1;
static struct baro_init_info icp_init_info = {
	.name = "icp",
	.init = icp_local_init,
	.uninit = icp_remove,
};

#define USE_POW_FUNCTION   1 /* set to 1 if usage of pow() function is preferred */
#define FIFO_DATA_BUFFER_SIZE		40
#define PRESS_TEMP_DATA_BUFFER_SIZE 20
#define ICP201XX_I2C_FIFO_SIZE      6

static uint8_t is_mode4_default_config_retrived  = 0;

static uint8_t m4_default_pres_osr, m4_default_temp_osr,  m4_default_HFOSC_on,	m4_default_DVDD_on , m4_default_IIR_filter_en, m4_default_FIR_filter_en,m4_default_pres_bs,m4_default_temp_bs;
static uint16_t m4_default_odr =0,m4_default_IIR_k = 0, m4_default_press_gain =0;
static bool need_warm_up;

/* I2C operation functions */
#if 0
static int icp_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	u8 reg_addr = addr;
	u8 *rxbuf = data;
	u8 left = len;
	u8 retry;
	u8 offset = 0;

	struct i2c_msg msg[2] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .buf = &reg_addr,
		 .len = 1,
		 },
		{
		 .addr = client->addr,
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
				BAR_ERR("i2c read register=%#x length=%d failed\n", addr + offset,
					len);
				return -EIO;
			}
		}
	}

	return 0;
}
#endif

#if 0
static int icp201xx_i2c_rx_read(struct i2c_client *client,
    u8 *data, u8 len)
{
    int res = 0;
    int retry = RETRY_CNT;

    if (!client) {
        return -EINVAL;
    } else if (len >  C_I2C_FIFO_SIZE) {
        BAR_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
        return -EINVAL;
    }

    do {
        res = i2c_master_recv(client, data, len);
        if (res < 0) {
            BAR_ERR("receive data error!!\n");
            return -EFAULT;
        } else
            res = 0;
    } while (res !=0 && --retry > 0);
    return res;
}

static long int get_current_time_us(void)
{
	struct timeval t;

	do_gettimeofday(&t);
	return (t.tv_sec & 0xFFF) * 1000000 + t.tv_usec;
}
#endif

//?M?????true,?????false
bool IsEven(unsigned int M)
{
	//return (M % 2 == 0 ) ? true : false;
	return ((M & 0x01) == 0) ? true : false;
}

//O(logN)
long int Pow_rec(long int X, unsigned int N)
{
	if (N == 0)
		return 1;
	if (IsEven(N))
		return Pow_rec(X*X, N / 2);
	else
		return Pow_rec(X*X, N / 2)*X;
}

static int icp201xx_i2c_read_register(struct i2c_client *client,
    u8 addr, u8 *data, u8 len)
{
    u8 beg = addr;
    u8 *rxbuf = data;
    u8 offset = 0;
    u8 left = len;
    int res = 0;
    struct i2c_msg msgs[2] = {{0}, {0} };
    int retry = RETRY_CNT;

    mutex_lock(&i2c_mutex);
    do{
        msgs[0].addr = client->addr;
        msgs[0].flags = 0;
        msgs[0].len = 1;
        msgs[0].buf = &beg;
        msgs[1].addr = client->addr;
        msgs[1].flags = I2C_M_RD;

        msgs[1].buf = &rxbuf[offset];
        if (!client) {
            mutex_unlock(&i2c_mutex);
            return -EINVAL;
        }

        // BECAUSE limtiaton of mtk C_I2C_FIFO_SIZE is 8 , we read 6 bytes each time
        if (left > ICP201XX_I2C_FIFO_SIZE) {
            msgs[1].len = ICP201XX_I2C_FIFO_SIZE;
			left -= ICP201XX_I2C_FIFO_SIZE;
			offset += ICP201XX_I2C_FIFO_SIZE;
        }
        else{
            msgs[1].len = left;
			left = 0;
        }
        do {
            res = i2c_transfer(client->adapter, msgs, sizeof(msgs)/sizeof(msgs[0]));
            if (res != 2) {
                BAR_ERR("i2c_transfer error: (%d %p %d) %d\n",
                    addr, data, len, res);
                res = -EIO;
            } else
                res = 0;
        } while (res !=0 && --retry > 0);
    }while(left > 0);
     mutex_unlock(&i2c_mutex);
     return res;
}

static int icp201xx_i2c_write_register(struct i2c_client *client,
    u8 addr, u8 *data, u8 len)
{
    /*because address also occupies one byte,
    the maximum length for write is 7 bytes*/
    int idx, num;
    int res = 0;
    char buf[C_I2C_FIFO_SIZE];
    int retry = RETRY_CNT;

    if (!client) {
        return -EINVAL;
    } else if (len >= C_I2C_FIFO_SIZE) {
        BAR_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
        return -EINVAL;
    }

    mutex_lock(&i2c_mutex);
    num = 0;
    buf[num++] = addr;
    for (idx = 0; idx < len; idx++)
        buf[num++] = data[idx];
    do {
        res = i2c_master_send(client, buf, num);
        if (res < 0) {
            BAR_ERR("send command error!!\n");
            mutex_unlock(&i2c_mutex);
            return -EFAULT;
        } else
            res = 0;
    } while (res !=0 && --retry > 0);
    mutex_unlock(&i2c_mutex);
    return res;
}


static int icp201xx_dummy_read(struct i2c_client *client)
{
	uint8_t rx_buf = 0;
	int status = 0;
	
    status = icp201xx_i2c_read_register(client, 0x00,&rx_buf,1);
	return status;
}

int inv_icp201xx_init(struct i2c_client *client)
{
	int status;
	u8 data;
	int i =0;

	data = 0xf0;
	/** dummy write transaction to address 0xEE to make sure we give enough time for ICP201xx to init **/
		do {
			status = icp201xx_i2c_write_register(client,0xEE,&data,1);
			if ( !status )
				break;
			usleep_range(10,20);
			i++;
		}while(i<10);

	return status;
}

/* get chip id type */
int inv_icp201xx_get_who_am_i(struct i2c_client *client)
{
	int status ;
    u8 who_am_i;
    struct icp_i2c_data *obj = i2c_get_clientdata(client);

	#if 0
	status = inv_icp201xx_rd_b1_who_am_i(&(s->serif),who_am_i);
	if ( status )
		return status;

	if ( *who_am_i != 0 )
		return INV_ERROR;  /* Error. Not A1 */

    #endif
	status = icp201xx_i2c_read_register(client, MPUREG_A1_WHO_AM_I,&who_am_i,1);
	if ( status <0 ){
	    status = icp201xx_i2c_read_register(client, MPUREG_A1_WHO_AM_I,&who_am_i,1);
	    status = icp201xx_i2c_read_register(client, MPUREG_A1_WHO_AM_I,&who_am_i,1);
	    status = icp201xx_i2c_read_register(client, MPUREG_A1_WHO_AM_I,&who_am_i,1);
	    status = icp201xx_i2c_read_register(client, MPUREG_A1_WHO_AM_I,&who_am_i,1);
	}
	if ( who_am_i == EXPECTED_DEVICE_ID ){
		obj->sensor_type = ICP201XX_TYPE;
		BAR_LOG( "[icp201xx] auto detect success %x\n", who_am_i);
		return 0;
	} else {
	    obj->sensor_type = INVALID_TYPE;
		BAR_LOG( "[icp201xx] auto detect fail %x\n", who_am_i);
		return -1;
	}
}

/* ########################################################################################################## */
int inv_icp201xx_rd_mode_sync_status(struct i2c_client *client, uint8_t *value)
{
	int status;
	uint8_t read_val;

	status = icp201xx_i2c_read_register(client, MPUREG_DEVICE_STATUS, &read_val,1);

	if ( status )
		return status;

	*value = read_val & BIT_DEVICE_STATUS_MODE_SYNC_STATUS_MASK ;

	return status;
}

static int icp201xx_rd_boot_up_status( struct i2c_client *client, uint8_t *value)
{
	int status;
	uint8_t reg_value = 0;

	status = icp201xx_i2c_read_register(client, REG_OTP_MTP_OTP_STATUS2, &reg_value,1);
	
	if(status)
		return status;
		
	*value = reg_value & BIT_OTP_STATUS2_BOOTUP_STATUS_MASK ;
	
	return status;
}

static int icp201xx_wr_boot_up_status(struct i2c_client *client, uint8_t new_value)
{
	int status;

	uint8_t reg_value = 0;
	uint8_t write_value ;

	status = icp201xx_i2c_read_register(client, REG_OTP_MTP_OTP_STATUS2, &reg_value,1);	

	if(status)
		return status;
	
	write_value = (reg_value & (~BIT_OTP_STATUS2_BOOTUP_STATUS_MASK)) | new_value;
	status = icp201xx_i2c_write_register(client, REG_OTP_MTP_OTP_STATUS2, &write_value,1);
	
	return status;
}

int inv_icp201xx_wr_mode_select(struct i2c_client *client, uint8_t new_value)
{
	int status;
	uint8_t read_value=0;
	uint8_t write_value = new_value;

	do {
		inv_icp201xx_rd_mode_sync_status(client,&read_value);
		if ( read_value  )
			break;
		usleep_range(500,600);
	}while(1);

	status = icp201xx_i2c_write_register(client, MPUREG_MODE_SELECT, &write_value,1);

	return status;
}

/********************************************
Register Name: OTP Config 1
Register Type: READ/WRITE
Register Address: 172 (Decimal); AC (Hex)
********************************************/
int inv_icp201xx_wr_otp_write_switch(struct i2c_client *client,uint8_t new_value)
{
	int status = 0;
	uint8_t reg_value = 0;
	status |= icp201xx_i2c_read_register(client, MPUREG_OTP_MTP_OTP_CFG1, &reg_value,1);

	reg_value = ( reg_value & (~BIT_OTP_CONFIG1_WRITE_SWITCH_MASK) ) | ( new_value << BIT_OTP_CONFIG1_WRITE_SWITCH_POS ) ;

    status |= icp201xx_i2c_write_register(client, MPUREG_OTP_MTP_OTP_CFG1, &reg_value,1);

	return status;

}

int inv_icp201xx_wr_otp_enable(struct i2c_client *client,uint8_t new_value)
{
	int status = 0;
	uint8_t reg_value = 0;

	status |= icp201xx_i2c_read_register(client, MPUREG_OTP_MTP_OTP_CFG1, &reg_value,1);

	reg_value = ( reg_value & (~BIT_OTP_CONFIG1_OTP_ENABLE_MASK) ) | ( new_value  ) ;

	status |= icp201xx_i2c_write_register(client, MPUREG_OTP_MTP_OTP_CFG1, &reg_value,1);

	return status;

}

/********************************************
Register Name: OTP Debug2
Register Type: READ/WRITE
Register Address:  180(Decimal); BC (Hex)
********************************************/
int inv_icp201xx_wr_otp_dbg2_reset(struct i2c_client *client,uint8_t new_value)
{
	int status = 0;
	uint8_t reg_value = 0;
	status |= icp201xx_i2c_read_register(client, MPUREG_OTP_DEBUG2, &reg_value,1);

	reg_value = ( reg_value & (~BIT_OTP_DBG2_RESET_MASK) ) | ( new_value << BIT_OTP_DBG2_RESET_POS  ) ;

	status |= icp201xx_i2c_write_register(client, MPUREG_OTP_DEBUG2, &reg_value,1);

	return status;

}

int inv_icp201xx_wr_pefe_offset_trim(struct i2c_client *client,uint8_t new_value)
{
	int status = 0;
	uint8_t reg_value;

	status |= icp201xx_i2c_read_register(client, MPUREG_TRIM1_MSB, &reg_value,1);

	reg_value = ( reg_value & (~BIT_PEFE_OFFSET_TRIM_MASK) ) | ( new_value  ) ;

    status |= icp201xx_i2c_write_register(client, MPUREG_TRIM1_MSB, &reg_value,1);

	return status;
}

int inv_icp201xx_wr_pefe_gain_trim(struct i2c_client *client,uint8_t new_value)
{
	int status = 0;
	uint8_t reg_value;

	status |= icp201xx_i2c_read_register(client, MPUREG_TRIM2_MSB, &reg_value,1);

	reg_value = ( reg_value & (~BIT_PEFE_GAIN_TRIM_MASK) ) | ( new_value <<BIT_PEFE_GAIN_TRIM_POS ) ;

    status |= icp201xx_i2c_write_register(client, MPUREG_TRIM2_MSB, &reg_value,1);

	return status;
}

int inv_icp201xx_wr_hfosc_trim(struct i2c_client *client,uint8_t new_value)
{
	int status = 0;
	uint8_t reg_value;

	status |= icp201xx_i2c_read_register(client, MPUREG_TRIM2_LSB, &reg_value,1);

	reg_value = ( reg_value & (~BIT_HFOSC_OFFSET_TRIM_MASK) ) | ( new_value  ) ;

    status |= icp201xx_i2c_write_register(client, MPUREG_TRIM2_LSB, &reg_value,1);

	return status;
}

static int inv_icp201xx_enable_write_switch_OTP_read(struct i2c_client *client)
{
	/*
	1)	Power-on the ASIC
2)	Bring the ASIC in power mode to activate the OTP power domain and get access to the main registers
•	mode_select.power_mode = 1
*/
	volatile uint8_t reg_value = 0;
	int ret =0;
	uint8_t databuf;

	/* set to stand by */
	ret |= inv_icp201xx_wr_mode_select(client,reg_value);

	msleep(2);
	/* set to power mode */

	reg_value = BIT_POWER_MODE_MASK;
	ret |= inv_icp201xx_wr_mode_select(client,reg_value);

	msleep(4);

	/*
	3)	Unlock the main registers
	•	master_lock.lock 	= 0x1f
	*/
	databuf = 0x1f;
	ret |= icp201xx_i2c_write_register(client,MPUREG_MASTER_LOCK, &databuf,1);

	/*
	4)	Enable the OTP and the write switch
	•	otp_config1.otp_enable = 1;
	•	otp_config1.otp_write_switch = 1;
	•	wait 10us;
	*/
	ret |= inv_icp201xx_wr_otp_enable(client,0x01);
	ret |= inv_icp201xx_wr_otp_write_switch(client,0x01);

	usleep_range(10,20);

	/*

	5)	Toggle the OTP reset pin
	•	otp_dbg2.reset = 1
	•	wait 10us
	•	otp_dbg2.reset = 0
	•	wait 10us

	*/

	ret |= inv_icp201xx_wr_otp_dbg2_reset(client,1);

	usleep_range(10,20);

	ret |= inv_icp201xx_wr_otp_dbg2_reset(client,0);

	usleep_range(10,20);;

	/*
	6)	Program redundant read
	•	otp_mra_lsb		= 0x04
	•	otp_mra_msb		= 0x04
	•	otp_mrb_lsb		= 0x21
	•	otp_mrb_msb		= 0x20
	•	otp_mr_lsb		= 0x10
	•	otp_mr_msb		= 0x80
	*/

    databuf = 0x04;
	ret |= icp201xx_i2c_write_register(client,MPUREG_OTP_MTP_MRA_LSB, &databuf,1);

	databuf = 0x04;
	ret |= icp201xx_i2c_write_register(client,MPUREG_OTP_MTP_MRA_MSB, &databuf,1);

	databuf = 0x21;
	ret |= icp201xx_i2c_write_register(client,MPUREG_OTP_MTP_MRB_LSB, &databuf,1);

	databuf = 0x20;
	ret |= icp201xx_i2c_write_register(client,MPUREG_OTP_MTP_MRB_MSB, &databuf,1);

    databuf = 0x10;
	ret |= icp201xx_i2c_write_register(client,MPUREG_OTP_MTP_MR_LSB, &databuf,1);

	databuf = 0x80;
	ret |= icp201xx_i2c_write_register(client,MPUREG_OTP_MTP_MR_MSB, &databuf,1);

    return ret;
}

#if 0
static void inv_icp201xx_disable_write_switch_OTP_read(struct i2c_client *client)
{
	/*
	10)	Disable OTP and write switch
	•	otp_config1_reg.otp_enable          = 0;
	•	otp_config1_reg.otp_write_switch    = 0;

	*/
	inv_icp201xx_wr_otp_enable(client,0x0);
	inv_icp201xx_wr_otp_write_switch(client,0x00);
	/* set to stand by */
	inv_icp201xx_wr_mode_select(client,0x00);

}
#endif 

static int icp201xx_rd_version(struct i2c_client *client,uint8_t *value)
{	
	return icp201xx_i2c_read_register(client, REG_VERSION,value ,1);
}

int inv_icp201xx_OTP_bootup_cfg(struct i2c_client *client)
{
	int status = 0;
	uint8_t otp_status,databuf;
	uint8_t offset = 0, gain = 0,Hfosc = 0;

	uint8_t version= 0;
	uint8_t bootup_status = 0;

	/* 	1.	Power-on the ASIC ( Asic is already powered on )
	    2.  Do init ( already initialized ) 
		*/
	/* 3. read version register */
	status = icp201xx_rd_version(client, &version);
	if (status)
		return status;

	if (version == 0x00){
		icp201xx_dev_version = ICP201XX_CHIP_VERSION_A;
	#if DEBUG_PHASE
		BAR_LOG( "[icp201xx] A1 version ASIC is detected\n");
		#endif
	}
	else if (version == 0xB2)
	{
		/* B2 version Asic is detected. Boot up sequence is not required for B2 Asic, so returning */
		icp201xx_dev_version = ICP201XX_CHIP_VERSION_B;
		
		#if DEBUG_PHASE
		BAR_LOG( "[icp201xx] B2 version ASIC is detected\n");
		#endif
		return 0;
	}
	
	/* 4. Read boot up status and avoid re running boot up sequence if it is already done */
	status = icp201xx_rd_boot_up_status(client,&bootup_status);
	if (status)
		return status;
	if (bootup_status)
	{
		/* Boot up sequence is already done, not required to repeat boot up sequence */
		#if DEBUG_PHASE
		BAR_LOG("[icp201xx] Boot up sequence is already done (init)\n");
		#endif	
		return 0;
	}

	/* Continue with boot up sequence for A1 */
	status  = inv_icp201xx_enable_write_switch_OTP_read(client);

	if(status < 0)
	    BAR_ERR("icp inv_icp201xx_enable_write_switch_OTP_read faile\n");

	/***************************************************************************/
	/*
	7)	Write the address content and read command
	•	otp_address_reg.address		= Address[7:0]
	•	otp_command_reg.address		= {boot,red=0,diff=0,Address[8]}
	•	otp_command_reg.command	= 1    // read action
	*/
	databuf = 0xf8;
	status |= icp201xx_i2c_write_register(client,MPUREG_OTP_MTP_OTP_ADDR, &databuf,1);

	databuf = 0x10;
	status |= icp201xx_i2c_write_register(client,MPUREG_OTP_MTP_OTP_CMD, &databuf,1);


	/*
	8)	Wait for the OTP read to finish
	•	Monitor otp_status.busy to be 0
	*/
	do 	{
		status |= icp201xx_i2c_read_register(client, MPUREG_OTP_MTP_OTP_STATUS, &otp_status,1);
		if ( otp_status == 0 ) {
			break;
		}
		usleep_range(1,5);
	}while(1);
	/*
	9)	Read the data from register otp_rdata_reg.value
	*/

	status |= icp201xx_i2c_read_register(client, MPUREG_OTP_MTP_RD_DATA, &offset,1);

	/****************************************************************************/
	/* 3 bit gain (OTP 249 [2:0] to MAIN, TRIM2_MSB [6:4]

	10)	Write the address content and read command
	•	otp_address_reg.address		= Address[7:0]
	•	otp_command_reg.address		= {boot,red=0,diff=0,Address[8]}
	•	otp_command_reg.command	= 1    // read action
	*/

    databuf = 0xf9;
	status |= icp201xx_i2c_write_register(client,MPUREG_OTP_MTP_OTP_ADDR, &databuf,1);

	databuf = 0x10;
	status |= icp201xx_i2c_write_register(client,MPUREG_OTP_MTP_OTP_CMD, &databuf,1);

	/*
	11)	Wait for the OTP read to finish
	•	Monitor otp_status.busy to be 0
	*/
	do 	{
		status |= icp201xx_i2c_read_register(client, MPUREG_OTP_MTP_OTP_STATUS, &otp_status,1);
		if ( otp_status == 0 ) {
			break;
		}
		usleep_range(1,5);
	}while(1);

	/*
	12)	Read the data from register otp_rdata_reg.value
	*/
	status |= icp201xx_i2c_read_register(client, MPUREG_OTP_MTP_RD_DATA, &gain,1);

	/****************************************************************************/
	/* HFOSC
		Write the address content and read command
	•	otp_address_reg.address		= Address[7:0]
	•	otp_command_reg.address		= {boot,red=0,diff=0,Address[8]}
	•	otp_command_reg.command	= 1    // read action
	*/

	databuf = 0xFA;
	status |= icp201xx_i2c_write_register(client,MPUREG_OTP_MTP_OTP_ADDR, &databuf,1);

	databuf = 0x10;
	status |= icp201xx_i2c_write_register(client,MPUREG_OTP_MTP_OTP_CMD, &databuf,1);

	/*
	13)	Wait for the OTP read to finish
	•	Monitor otp_status.busy to be 0
	*/
	do 	{
		status |= icp201xx_i2c_read_register(client, MPUREG_OTP_MTP_OTP_STATUS, &otp_status,1);
		if ( otp_status == 0 ) {
			break;
		}
		usleep_range(1,5);
	}while(1);

	/*
	14)	Read the data from register otp_rdata_reg.value
	*/

	status |= icp201xx_i2c_read_register(client, MPUREG_OTP_MTP_RD_DATA, &Hfosc,1);

	/*
	15) Disable OTP and write switch
	*/
	
	inv_icp201xx_wr_otp_enable(client,0x0);
	inv_icp201xx_wr_otp_write_switch(client,0x00);
	usleep_range(10,15);

	/**16,17,18 Updating main reg */
	status |= inv_icp201xx_wr_pefe_offset_trim(client,(offset & 0x3f));
	status |= inv_icp201xx_wr_pefe_gain_trim(client,(gain & 0x07));
	status |= inv_icp201xx_wr_hfosc_trim(client,(Hfosc & 0x7f));

	//inv_icp201xx_disable_write_switch_OTP_read(client);
	
	/* 22) Lock the main register */

	databuf = 0x00;
	status |= icp201xx_i2c_write_register(client, MPUREG_MASTER_LOCK, &databuf,1);

	/* 23) Move to stand by */
	inv_icp201xx_wr_mode_select(client,0x00);
	
	/* Update boot up status to 1 */
	status |= icp201xx_wr_boot_up_status(client,0x1);
	
	return status;

}

/* setting fifo watermark & settig fifo interrupt */
int inv_icp201xx_set_fifo_notification_config(struct i2c_client *client,uint8_t fifo_int_mask, uint8_t fifo_wmk_high,uint8_t fifo_wmk_low)
{
	uint8_t reg_value = 0;
	int status = 0;

	if ( fifo_wmk_high > 0xf || fifo_wmk_low > 0xf )
		return 0;
	/** FIFO config **/
	reg_value = (fifo_wmk_high << 4) | fifo_wmk_low;

	status |=  icp201xx_i2c_write_register(client, MPUREG_FIFO_CONFIG, &reg_value,1);

	// read int mask
	status |=icp201xx_i2c_read_register(client,MPUREG_INTERRUPT_MASK, &reg_value, 1);
	reg_value =  (reg_value |  (ICP201XX_INT_MASK_FIFO_WMK_HIGH | ICP201XX_INT_MASK_FIFO_OVER_FLOW |ICP201XX_INT_MASK_FIFO_WMK_LOW |ICP201XX_INT_MASK_FIFO_UNDER_FLOW)) & ~fifo_int_mask;

	status |=  icp201xx_i2c_write_register(client, MPUREG_INTERRUPT_MASK, &reg_value,1);

	return status;
}


int inv_icp201xx_wr_flush_fifo(struct i2c_client *client)
{
	int status = 0;
	uint8_t read_val = 0;

	status |= icp201xx_i2c_read_register(client, MPUREG_FIFO_FILL, &read_val,1);

	read_val |= 0x80;

    status |= icp201xx_i2c_write_register(client, MPUREG_FIFO_FILL, &read_val,1);

	return status;
}

int  inv_icp201xx_soft_reset(struct i2c_client *client)
{
	int status = 0;
	uint8_t int_status;
	status |= inv_icp201xx_wr_mode_select(client,0);
	msleep(2);

	status |= inv_icp201xx_wr_flush_fifo(client);
	status |= inv_icp201xx_set_fifo_notification_config(client,0,0,0);

	int_status = 0xFF;
	status |= icp201xx_i2c_write_register(client, MPUREG_INTERRUPT_MASK, &int_status,1);

	status |= icp201xx_i2c_read_register(client, MPUREG_INTERRUPT_STATUS, &int_status,1);
	if ( int_status )
		status |= icp201xx_i2c_write_register(client, MPUREG_INTERRUPT_STATUS, &int_status,1);  //clear int status

	return status;
}

 #if 0
static int icp_set_powermode(struct i2c_client *client, enum ICP_POWERMODE_ENUM power_mode)
{

	struct icp_i2c_data *obj = i2c_get_clientdata(client);
	u8 err = 0, data = 0, actual_power_mode = 0;

	if (atomic_read(&obj->trace) & BAR_TRC_INFO)
		BAR_LOG("[%s] power_mode = %d, old power_mode = %d\n", __func__,
			power_mode, obj->power_mode);

	if (power_mode == obj->power_mode)
		return 0;

	mutex_lock(&obj->lock);

	if (obj->sensor_type == ICP201XX_TYPE) {	/* ICP201XX */
		if (power_mode == ICP_SUSPEND_MODE) {
			actual_power_mode = ICP201XX_SLEEP_MODE;
		} else if (power_mode == ICP_NORMAL_MODE) {
			actual_power_mode = ICP201XX_NORMAL_MODE;
		} else {
			err = -EINVAL;
			BAR_ERR("invalid power mode = %d\n", power_mode);
			mutex_unlock(&obj->lock);
			return err;
		}
		err = icp_i2c_read_block(client, ICP201XX_CTRLMEAS_REG_MODE__REG, &data, 1);
		data = ICP_SET_BITSLICE(data, ICP201XX_CTRLMEAS_REG_MODE, actual_power_mode);
		err += icp_i2c_write_block(client, ICP201XX_CTRLMEAS_REG_MODE__REG, &data, 1);
	}

	if (err < 0)
		BAR_ERR("set power mode failed, err = %d, sensor name = %s\n",
			err, obj->sensor_name);
	else
		obj->power_mode = power_mode;

	mutex_unlock(&obj->lock);

	return 0;
}
#endif



/*
*get compensated pressure
*unit: hectopascal(hPa)
*/
int inv_icp201xx_get_fifo_count(struct i2c_client *client,uint8_t *fifo_count)
{

	int status = 0;
	uint8_t read_val = 0;

	status = icp201xx_i2c_read_register(client, MPUREG_FIFO_FILL,  &read_val,1);
	if ( status < 0)
		return status;
	*fifo_count = (uint8_t)( read_val & BIT_FIFO_LEVEL_MASK ) ;
	/* Max value for fifo level is 0x10 for any values higher than 0x10 function should return error */
	if (( *fifo_count & 0x10 )  && ( *fifo_count & 0x0F) )
		status = -1;
	return status;
}

int inv_icp201xx_rd_fifo(struct i2c_client *client, uint8_t len, uint8_t* value,uint8_t fifo_read_offset)
{
	return (icp201xx_i2c_read_register(client, (MPUREG_FIFO_BASE+fifo_read_offset), value,len));

}

int inv_icp201xx_get_fifo_data(struct i2c_client *client,uint8_t req_packet_cnt, uint8_t *databuff)
{
    struct icp_i2c_data *obj;
    int status;
    uint8_t fifo_read_offset;
	uint8_t packet_cnt_bytes = req_packet_cnt * 2 * 3 ;

    obj = i2c_get_clientdata(client);
    if(obj ==NULL)
        return -1;

    if(databuff == NULL)
        return -2;

    fifo_read_offset = (( obj->fifo_readout_mode == ICP201XX_FIFO_READOUT_MODE_PRES_ONLY )|| (obj->fifo_readout_mode == ICP201XX_FIFO_READOUT_MODE_TEMP_ONLY)) ? 3 : 0;

	status = inv_icp201xx_rd_fifo(client,packet_cnt_bytes,databuff,fifo_read_offset);
	if(status < 0)
        BAR_LOG("inv_icp201xx_rd_fifo status %d\n",  status);
	return status;
}

int inv_icp201xx_process_raw_data(struct icp_i2c_data *obj,uint8_t packet_cnt, uint8_t *data,int32_t * pressure, int32_t * temperature)
{
	uint8_t i,offset=0;

    if(obj ==NULL)
        return -1;

    if(data == NULL || pressure == NULL || temperature ==NULL){
        return -2;
    }
	for ( i = 0 ; i < packet_cnt ; i++ )
	{
		if ( obj->fifo_readout_mode == ICP201XX_FIFO_READOUT_MODE_PRES_TEMP)
		{
			pressure[i] = (int32_t)((( data[offset+2] & 0x0f) << 16) | (data[offset+1] << 8) | data[offset]) ;
			offset += 3;
			temperature[i] = (int32_t)(((data[offset+2] & 0x0f) << 16) | (data[offset+1] << 8) | data[offset]) ;
			offset += 3;
		} else if ( obj->fifo_readout_mode == ICP201XX_FIFO_READOUT_MODE_TEMP_ONLY) {
			temperature[i] = (int32_t)(((data[offset+2] & 0x0f) << 16) | (data[offset+1] << 8) | data[offset]) ;
			offset += 3;
		} else if( obj->fifo_readout_mode == ICP201XX_FIFO_READOUT_MODE_TEMP_PRES) {
			temperature[i] = (int32_t)(((data[offset+2] & 0x0f) << 16) | (data[offset+1] << 8) | data[offset]) ;
			offset += 3;
			pressure[i] = (int32_t)((( data[offset+2] & 0x0f) << 16) | (data[offset+1] << 8) | data[offset]) ;
			offset += 3;
		} else if( obj->fifo_readout_mode == ICP201XX_FIFO_READOUT_MODE_PRES_ONLY) {
			pressure[i] = (int32_t)((( data[offset+2] & 0x0f) << 16) | (data[offset+1] << 8) | data[offset]) ;
			offset += 3;
		}
	}
	return 0;
}

static void icp201xx_convert_pressure(int32_t rawPres, int *pres)
{
	/** P = (POUT/2^17)*40kPa + 70kPa **/
	if (rawPres & 0x080000 )
		rawPres |= 0xFFF00000;

	*pres = ((int64_t)(rawPres) *400 * 100 /131072) +700 * 100;
}

static int icp_get_pressure(struct i2c_client *client, int *pressure_value)
{
	struct icp_i2c_data *obj;
	uint8_t fifo_data[FIFO_DATA_BUFFER_SIZE] = {0};
    int32_t data_temp[PRESS_TEMP_DATA_BUFFER_SIZE],data_press[PRESS_TEMP_DATA_BUFFER_SIZE];
	uint8_t fifo_cnt;

    int real_pressure_pa;
	int status = 0;
	//s32 temperature = 0, upressure = 0, pressure = 0;
	//char temp_buf[ICP_BUFSIZE];
    //uint64_t SampleTime = get_current_time_us();
	/* prize added by chenjiaxi, barometer calibration, 20220423-start */
	static s32 sumValuePa = 0, averageValuePa = 0;
	static uint8_t inputCount = 0;
	s32 tmpCali = 0;
	/* prize added by chenjiaxi, barometer calibration, 20220423-end */

    //BAR_FUN();

	if (client == NULL) {
		return -2;
	}
    mutex_lock(&get_data_mutex);
    obj = i2c_get_clientdata(client);

    if(obj == NULL)
         BAR_LOG("icp_get_pressure obj is Null\n");

    status = inv_icp201xx_get_fifo_count(client,&fifo_cnt);

	
    #if DEBUG_PHASE
    BAR_LOG("fifo cnt %d \n",  fifo_cnt);
	#endif
    if(fifo_cnt != 0 && (fifo_cnt < 16)){

        if(fifo_data == NULL)
         BAR_LOG("icp_get_pressure fifo data is Null\n");

        status |= inv_icp201xx_get_fifo_data(client,fifo_cnt,fifo_data);
        #if 0
        BAR_LOG(" inv_icp201xx_get_fifo_data status %d\n", status);
	    #endif
		status |=icp201xx_dummy_read(client);	
		
        status |= inv_icp201xx_process_raw_data(obj,fifo_cnt,fifo_data,data_press,data_temp);
        #if 0
        BAR_LOG("inv_icp201xx_process_raw_data status %d\n",  status);
	    #endif
        icp201xx_convert_pressure(data_press[fifo_cnt-1], &real_pressure_pa);
    }
    else if(fifo_cnt == 0){
		status |=icp201xx_dummy_read(client);	
        real_pressure_pa = obj->pre_pressure_data;
    }
	else if(fifo_cnt == 16) {
		#if DEBUG_PHASE
		BAR_LOG( "[icp201xx] fifo overflow !!!\n");
		#endif
		status |= inv_icp201xx_wr_flush_fifo(client);
		status |= icp201xx_dummy_read(client);
	}
	
    if(need_warm_up){
        if(fifo_cnt <= obj->fifo_packets_to_skip)
            real_pressure_pa = 101500; //we use a fake pressuredata if data not ready at first
        need_warm_up = false;
    }
    obj->pre_pressure_data = real_pressure_pa;
    #if DEBUG_PHASE
    BAR_LOG("[icp201xx] real P data %d,,cnt %d\n",  real_pressure_pa,fifo_cnt);
	#endif
	//sprintf(buf, "%08x", real_pressure_pa);
	*pressure_value = real_pressure_pa;
    mutex_unlock(&get_data_mutex);

	/* prize added by chenjiaxi, barometer calibration, 20220423-start */
	if (obj_i2c_data->startCali) {
		inputCount++;
		if (inputCount <= REQUIRED_DATA_NUMBER) {
			if ((inputCount == 1) || (inputCount == 2) || (inputCount == 3)) { //discard the first three data
				sumValuePa += 0;
			} else {
				sumValuePa += *pressure_value;
				pr_err("%s cali raw:[%d %d], inputCount=%d\n", __func__, sumValuePa, *pressure_value, inputCount);
			}
			return status;
		} else {
			obj_i2c_data->startCali = false;
			inputCount = 0;
			averageValuePa = sumValuePa / (REQUIRED_DATA_NUMBER - 3); //discard the first three data
			tmpCali = obj_i2c_data->referenceValuePa - averageValuePa;
			sumValuePa = 0;
			if (abs(tmpCali) > MAXIMUM_TOLERENCE_ERROR) {
				pr_err("%s cali fail, sumValuePa=%d, pressure=%d]\n", __func__, sumValuePa, *pressure_value);
			} else {
				baro_cali_data[0] = obj_i2c_data->referenceValuePa;
				baro_cali_data[1] = obj_i2c_data->staticCaliPa = tmpCali;
				pr_err("%s cali success, referenceValuePa=%d, staticCaliPa=%d\n", 
					__func__, obj_i2c_data->referenceValuePa, obj_i2c_data->staticCaliPa);
			}
		}
	}
	*pressure_value += obj_i2c_data->staticCaliPa;
	/* prize added by chenjiaxi, barometer calibration, 20220423-end */
	
	return status;
}

int inv_icp201xx_wr_mode4_osr_temp(struct i2c_client *client, uint8_t new_value)
{
	int status;
	uint8_t reg_value = 0;
	status = icp201xx_i2c_read_register(client, MPUREG_MODE4_CONFIG1, &reg_value,1);

	if(status < 0)
		return status;

	reg_value = ( reg_value & (~BIT_MODE4_CONFIG1_OSR_TEMP_MASK) ) | ( new_value ) ;

	status |= icp201xx_i2c_write_register(client, MPUREG_MODE4_CONFIG1, &reg_value,1);
	return status;
}

int inv_icp201xx_rd_mode4_osr_temp(struct i2c_client *client, uint8_t *value)
{
	int status;
	uint8_t reg_value = 0;
	status = icp201xx_i2c_read_register(client, MPUREG_MODE4_CONFIG1, &reg_value,1);

	if(status <0)
		return status;

	*value = ( reg_value & BIT_MODE4_CONFIG1_OSR_TEMP_MASK ) ;

	return status;
}

/********************************************
Register Name: MODE4_CONFIG2
Register Type: READ/WRITE
Register Address: 47 (Decimal); 2F (Hex)
********************************************/
int inv_icp201xx_wr_mode4_odr_msb(struct i2c_client *client, uint8_t new_value)
{
	int status;
	uint8_t reg_value = 0;
	status = icp201xx_i2c_read_register(client, MPUREG_MODE4_CONFIG2, &reg_value,1);

	if(status < 0)
		return status;

	reg_value = ( reg_value & (~BIT_MODE4_CONFIG2_ODR_MSB_MASK) ) | ( new_value ) ;

	status = icp201xx_i2c_write_register(client, MPUREG_MODE4_CONFIG2, &reg_value,1);

	return status;
}

int inv_icp201xx_rd_mode4_odr_msb(struct i2c_client *client, uint8_t *value)
{
	int status;
	uint8_t reg_value = 0;
	status = icp201xx_i2c_read_register(client, MPUREG_MODE4_CONFIG2, &reg_value,1);

	if(status <0)
	return status;

	*value = ( reg_value & BIT_MODE4_CONFIG2_ODR_MSB_MASK ) ;

	return status;
}

int inv_icp201xx_wr_mode4_iir_enable(struct i2c_client *client, uint8_t new_value)
{
	int status;
	uint8_t reg_value = 0;
	status = icp201xx_i2c_read_register(client, MPUREG_MODE4_CONFIG1, &reg_value,1);

	if(status < 0)
		return status;

	reg_value = ( reg_value & (~BIT_MODE4_CONFIG1_IIR_EN_MASK) ) | ( new_value << BIT_MODE4_CONFIG1_IIR_EN_POS) ;

	status = icp201xx_i2c_write_register(client, MPUREG_MODE4_CONFIG1, &reg_value,1);

	return status;
}
int inv_icp201xx_rd_mode4_iir_enable(struct i2c_client *client, uint8_t *value)
{
	int status;
	uint8_t reg_value = 0;
	status = icp201xx_i2c_read_register(client, MPUREG_MODE4_CONFIG1, &reg_value,1);

	if(status < 0)
	return status;

	*value = ( reg_value & BIT_MODE4_CONFIG1_IIR_EN_MASK ) >> BIT_MODE4_CONFIG1_IIR_EN_POS ;

	return status;
}

int inv_icp201xx_wr_mode4_fir_enable(struct i2c_client *client, uint8_t new_value)
{
	int status;
	uint8_t reg_value = 0;
	status = icp201xx_i2c_read_register(client, MPUREG_MODE4_CONFIG1, &reg_value,1);

	if(status < 0)
	return status;

	reg_value = ( reg_value & (~BIT_MODE4_CONFIG1_FIR_EN_MASK) ) | ( new_value << BIT_MODE4_CONFIG1_FIR_EN_POS) ;

	status = icp201xx_i2c_write_register(client, MPUREG_MODE4_CONFIG1, &reg_value,1);

	return status;
}

int inv_icp201xx_rd_mode4_fir_enable(struct i2c_client *client, uint8_t *value)
{
	int status;
	uint8_t reg_value = 0;
	status = icp201xx_i2c_read_register(client, MPUREG_MODE4_CONFIG1, &reg_value,1);

	if(status < 0)
	return status;

	*value = ( reg_value & BIT_MODE4_CONFIG1_FIR_EN_MASK ) >> BIT_MODE4_CONFIG1_FIR_EN_POS ;

	return status;
}

int inv_icp201xx_wr_mode4_dvdd_on(struct i2c_client *client, uint8_t new_value)
{
	int status;
	uint8_t reg_value = 0;
	status = icp201xx_i2c_read_register(client, MPUREG_MODE4_CONFIG2, &reg_value,1);

	if(status < 0)
		return status;

	reg_value = ( reg_value & (~BIT_MODE4_CONFIG2_DVDD_ON_MASK) ) | ( new_value << BIT_MODE4_CONFIG2_DVDD_ON_POS) ;

	status = icp201xx_i2c_write_register(client, MPUREG_MODE4_CONFIG2, &reg_value,1);

	return status;
}
int inv_icp201xx_rd_mode4_dvdd_on(struct i2c_client *client, uint8_t *value)
{
	int status;
	uint8_t reg_value = 0;
	status = icp201xx_i2c_read_register(client, MPUREG_MODE4_CONFIG2,  &reg_value,1);

	if(status < 0)
	return status;

	*value = ( reg_value & BIT_MODE4_CONFIG2_DVDD_ON_MASK ) >> BIT_MODE4_CONFIG2_DVDD_ON_POS ;

	return status;
}

int inv_icp201xx_wr_mode4_hfosc_on(struct i2c_client *client, uint8_t new_value)
{
	int status;
	uint8_t reg_value = 0;
	status = icp201xx_i2c_read_register(client, MPUREG_MODE4_CONFIG2, &reg_value,1);

	if(status < 0)
		return status;

	reg_value = ( reg_value & (~BIT_MODE4_CONFIG2_HFOSC_ON_MASK) ) | ( new_value << BIT_MODE4_CONFIG2_HFOSC_ON_POS) ;

	status = icp201xx_i2c_write_register(client, MPUREG_MODE4_CONFIG2, &reg_value,1);

	return status;
}
int inv_icp201xx_rd_mode4_hfosc_on(struct i2c_client *client, uint8_t *value)
{
	int status;
	uint8_t reg_value = 0;
	status = icp201xx_i2c_read_register(client, MPUREG_MODE4_CONFIG2, &reg_value,1);

	if(status < 0)
	return status;

	*value = ( reg_value & BIT_MODE4_CONFIG2_HFOSC_ON_MASK ) >> BIT_MODE4_CONFIG2_HFOSC_ON_POS ;

	return status;
}

/********************************************
Register Name:
Register Type: READ/WRITE
Register Address: 48(Decimal); 30(Hex)
********************************************/

int inv_icp201xx_wr_mode4_bs_val_press(struct i2c_client *client, uint8_t new_value)
{
	int status;
	uint8_t reg_value = 0;
	status = icp201xx_i2c_read_register(client, MPUREG_MODE4_BS_VALUE, &reg_value ,1);

	if(status < 0)
	return status;

	reg_value = ( reg_value & (~BIT_MODE4_BS_VALUE_PRESS) ) | ( new_value ) ;

	status = icp201xx_i2c_write_register(client, MPUREG_MODE4_BS_VALUE, &reg_value,1);

	return status;
}

int inv_icp201xx_rd_mode4_bs_val_press(struct i2c_client *client, uint8_t *value)
{
	int status;
	uint8_t reg_value = 0;

	status = icp201xx_i2c_read_register(client, MPUREG_MODE4_BS_VALUE,  &reg_value,1);

	if ( status < 0)
	return status;

	*value = (reg_value & BIT_MODE4_BS_VALUE_PRESS )  ;

	return status;
}

int inv_icp201xx_wr_mode4_bs_val_temp(struct i2c_client *client, uint8_t new_value)
{
	int status;
	uint8_t reg_value = 0;
	status = icp201xx_i2c_read_register(client, MPUREG_MODE4_BS_VALUE,  &reg_value,1);

	if(status < 0)
	return status;

	reg_value = ( reg_value & (~BIT_MODE4_BS_VALUE_TEMP) ) | ( new_value << 4) ;

	status = icp201xx_i2c_write_register(client, MPUREG_MODE4_BS_VALUE, &reg_value,1);

	return status;
}

int inv_icp201xx_rd_mode4_bs_val_temp(struct i2c_client *client, uint8_t *value)
{
	int status;
	uint8_t reg_value = 0;

	status = icp201xx_i2c_read_register(client, MPUREG_MODE4_BS_VALUE, &reg_value,1);

	if ( status < 0)
	return status;

	*value = (reg_value & BIT_MODE4_BS_VALUE_TEMP ) >> 4 ;

	return status;
}

int inv_icp201xx_wr_pow_mode(struct i2c_client *client, icp201xx_power_mode_t new_value)
{
	int status;
	uint8_t reg_value = 0, read_value = 0;
	status = icp201xx_i2c_read_register(client, MPUREG_MODE_SELECT, &reg_value,1);

	if(status)
	return status;

	reg_value = ( reg_value & (~BIT_POWER_MODE_MASK) ) | ( new_value << BIT_FORCED_POW_MODE_POS) ;

	do {
		inv_icp201xx_rd_mode_sync_status(client,&read_value);
		if ( read_value  )
			break;
		usleep_range(500,600);
	}while(1);

	status = icp201xx_i2c_write_register(client, MPUREG_MODE_SELECT, &reg_value,1);

	return status;
}

int inv_icp201xx_wr_mode4_osr_press(struct i2c_client *client, uint8_t new_value)
{
	return icp201xx_i2c_write_register(client, MPUREG_MODE4_OSR_PRESS, &new_value,1);

}

int inv_icp201xx_wr_mode4_odr_lsb(struct i2c_client *client, uint8_t new_value)
{
	return icp201xx_i2c_write_register(client, MPUREG_MODE4_ODR_LSB, &new_value,1);
 }

int inv_icp201xx_wr_iir_k_factor_lsb(struct i2c_client *client, uint8_t new_value)
{
	return icp201xx_i2c_write_register(client, MPUREG_IIR_K_FACTOR_LSB, &new_value,1);
}


int inv_icp201xx_wr_iir_k_factor_msb(struct i2c_client *client, uint8_t new_value)
{
		return icp201xx_i2c_write_register(client, MPUREG_IIR_K_FACTOR_MSB, &new_value,1);
}



/********************************************
Register Name: MODE0_PRESS_GAIN_FACTOR_LSB
Register Type: READ/WRITE
Register Address: 132 (Decimal); 84 (Hex)
********************************************/
int inv_icp201xx_wr_mode4_press_gain_factor_lsb(struct i2c_client *client, uint8_t new_value)
{
	return icp201xx_i2c_write_register(client, MPUREG_MODE4_PRESS_GAIN_FACTOR_LSB, &new_value,1);
}

/********************************************
Register Name: MODE0_PRESS_GAIN_FACTOR_MSB
Register Type: READ/WRITE
Register Address: 133 (Decimal); 85 (Hex)
********************************************/
int inv_icp201xx_wr_mode4_press_gain_factor_msb(struct i2c_client *client, uint8_t new_value)
{
	return icp201xx_i2c_write_register(client, MPUREG_MODE4_PRESS_GAIN_FACTOR_MSB, &new_value,1);
}


int inv_icp201xx_set_mode4_config(struct i2c_client *client, uint8_t pres_osr, uint8_t temp_osr, uint16_t odr, uint8_t HFOSC_on,
									uint8_t DVDD_on , uint8_t IIR_filter_en, uint8_t FIR_filter_en, uint16_t IIR_k,
									uint8_t pres_bs,uint8_t temp_bs, uint16_t press_gain)
{
	int status = 0;

	status |= inv_icp201xx_wr_pow_mode(client,ICP201XX_POWER_MODE_ACTIVE);
	msleep(5);

	/* OSR */
	status |= inv_icp201xx_wr_mode4_osr_press(client, pres_osr);
	status |= inv_icp201xx_wr_mode4_osr_temp(client, temp_osr);

	/* ODR */
	status |= inv_icp201xx_wr_mode4_odr_lsb(client, (uint8_t)(0xFF & odr));
	status |= inv_icp201xx_wr_mode4_odr_msb(client, (uint8_t)(0x1F & (odr >> 8)));

	/* IIR */
	status |= inv_icp201xx_wr_mode4_iir_enable(client, IIR_filter_en);
	status |= inv_icp201xx_wr_iir_k_factor_lsb(client, (uint8_t)(IIR_k & 0xFF ));
	status |= inv_icp201xx_wr_iir_k_factor_msb(client, (uint8_t)((IIR_k >>8) & 0xFF ));

	/* FIR */
	status |= inv_icp201xx_wr_mode4_fir_enable(client, FIR_filter_en);

	/* dvdd */
	status |= inv_icp201xx_wr_mode4_dvdd_on(client, DVDD_on);

	/* dfosc */
	status |= inv_icp201xx_wr_mode4_hfosc_on(client, HFOSC_on);

	/* Barrel Shifter */
	status |= inv_icp201xx_wr_mode4_bs_val_press(client, pres_bs);
	status |= inv_icp201xx_wr_mode4_bs_val_temp(client, temp_bs);

	/* Pressure gain factor */
	status |= inv_icp201xx_wr_mode4_press_gain_factor_lsb(client, (uint8_t)( press_gain & 0xFF ));
	status |= inv_icp201xx_wr_mode4_press_gain_factor_msb(client, (uint8_t)( (press_gain >> 8) & 0xFF ));

	return status;
}

int inv_icp201xx_get_mode4_config(struct i2c_client *client, uint8_t *pres_osr, uint8_t *temp_osr, uint16_t *odr, uint8_t *HFOSC_on,
											uint8_t *DVDD_on , uint8_t *IIR_filter_en, uint8_t *FIR_filter_en, uint16_t *IIR_k,
											uint8_t *pres_bs,uint8_t *temp_bs, uint16_t *press_gain)
{
	int status = 0;
	uint8_t temp1,temp2;
	/* OSR */
	status |= icp201xx_i2c_read_register(client, MPUREG_MODE4_OSR_PRESS,pres_osr,1);
	status |= inv_icp201xx_rd_mode4_osr_temp(client, temp_osr);

	/* ODR */
	status |= icp201xx_i2c_read_register(client, MPUREG_MODE4_ODR_LSB,&temp1,1);
	status |= inv_icp201xx_rd_mode4_odr_msb(client, &temp2);
	*odr = (uint16_t) ((temp2 << 8) | temp1 );

	/* IIR */
	status |= inv_icp201xx_rd_mode4_iir_enable(client, IIR_filter_en);
	//inv_icp201xx_rd_iir_k_factor_lsb
	status |= icp201xx_i2c_read_register(client, MPUREG_IIR_K_FACTOR_LSB,&temp1,1);
	//inv_icp201xx_rd_iir_k_factor_msb
	status |= icp201xx_i2c_read_register(client, MPUREG_IIR_K_FACTOR_MSB, &temp2,1);
	*IIR_k = (uint16_t) ((temp2 << 8) | temp1 );

	/* FIR */
	status |= inv_icp201xx_rd_mode4_fir_enable(client, FIR_filter_en);

	/* dvdd */
	status |= inv_icp201xx_rd_mode4_dvdd_on(client, DVDD_on);

	/* dfosc */
	status |= inv_icp201xx_rd_mode4_hfosc_on(client, HFOSC_on);

	/* Barrel Shifter */
	status |= inv_icp201xx_rd_mode4_bs_val_press(client, pres_bs);
	status |= inv_icp201xx_rd_mode4_bs_val_temp(client, temp_bs);

	/* Gain Factor */
	//inv_icp201xx_rd_mode4_press_gain_factor_lsb
	status |= icp201xx_i2c_read_register(client, MPUREG_MODE4_PRESS_GAIN_FACTOR_LSB,&temp1,1);

    //inv_icp201xx_rd_mode4_press_gain_factor_msb
	status |= icp201xx_i2c_read_register(client, MPUREG_MODE4_PRESS_GAIN_FACTOR_MSB,&temp2,1);
	*press_gain = (uint16_t) ((temp2 << 8) | temp1 );


	return status;
}

int inv_icp201xx_app_pre_start_config(struct i2c_client *client, icp201xx_op_mode_t op_mode,icp201xx_meas_mode_t meas_mode)
{
	int status = 0;
	int i;
	uint8_t pres_bs = 0, temp_bs, max_idx, max_press_bs;
	uint16_t press_gain=0;
	
	uint8_t fir_en = ICP201XX_MODE4_CONFIG_FIR_EN;
    uint16_t setting_odr =0;
	uint32_t icp201xx_mode4_config_press_osr_val; // value of OSR 
	struct icp_i2c_data *obj = i2c_get_clientdata(client);
#if (!USE_POW_FUNCTION) 
	uint32_t temp1, temp2, press_gain_32bit;
#endif

	if ( op_mode != ICP201XX_OP_MODE4)
		return status;
	if ( !is_mode4_default_config_retrived )
	{
		inv_icp201xx_get_mode4_config(client,&m4_default_pres_osr, &m4_default_temp_osr, &m4_default_odr, &m4_default_HFOSC_on,	&m4_default_DVDD_on ,
										&m4_default_IIR_filter_en, &m4_default_FIR_filter_en, &m4_default_IIR_k,&m4_default_pres_bs,&m4_default_temp_bs,&m4_default_press_gain);

		is_mode4_default_config_retrived = 1;
	}
	if ( ICP201XX_MEAS_MODE_FORCED_TRIGGER ==  meas_mode )
		fir_en = 0;

	/** calculate gain factor from default m4 config **/

	// Calculate the barrel shifter values and pressure adjustable gain based on
	// the table and formula provided in section BARREL SHIFTER AND ADJUSTABLE GAIN in the App note

	// BS_SHIFT_VAL_PRESS = index, if pres_bs_cond[index+1] < OSR_PRESS <= pres_bs_cond[index]

	do{
		/* Look up table to calculate press bs based on press OSR ( refer App note for look up table and formula )
		---------------------------------------------------------------------
		|  BS_SHIFT_VAL_PRESS	  |        Condition for                    |
		|( look up table index)   |       pressure OSR value                |
		------------------------------------For A1 & B2 -------------------
		|       0                |  2^12.5 < OSR Value <= 2^13(8192)        |
		|       1                |  2^12 < OSR Value <= 2^12.5(5792.6)      |
		|       2                |  2^11.5 < OSR Value <= 2^12(4096)        |
		|       3                |  2^11 < OSR Value <= 2^11.5(2896.3)      |
		|       4                |  2^10.5 < OSR Value <= 2^11(2048)        |
		|       5                |  2^10 < OSR Value <= 2^10.5(1448.1)      |
		|       6                |  2^9.5 < OSR Value <= 2^10(1024)         |
		|       7                |  2^9 < OSR Value <= 2^9.5(724)           |
		|       8                |  2^8.5 < OSR Value <= 2^9(512)           |
		------------------------------------- Only For A1 ------------------|
		|       9 - 15           |            reserved                      |
		------------------------------------- Only For B2 -----------------=|
		|       9                |  2^8 < OSR Value <= 2^8.5(362)           |
		|       10               |  2^7.5 < OSR Value <= 2^8(256)           |
		|       11               |  2^7 < OSR Value <= 2^7.5(181)           |
		|       12               |  2^6.5 < OSR Value <= 2^7(128)           |
		|       13               |  2^6 < OSR Value <= 2^6.5(90.5)          |
		|       14               |  2^5.5 < OSR Value <= 2^6(64)            |
		|       15               |  2^5 < OSR Value <= 2^5.5(45.2)          |

		-------------------------------------------------------------------------------------------
		*/	/* Look up table to calculate press bs based on press OSR ( refer App note for look up table and formula )
		---------------------------------------------------------------------
		|  BS_SHIFT_VAL_PRESS	  |        Condition for                    |
		|( look up table index)   |       pressure OSR value                |
		------------------------------------For A1 & B2 -------------------
		|       0                |  2^12.5 < OSR Value <= 2^13(8192)        |
		|       1                |  2^12 < OSR Value <= 2^12.5(5792.6)      |
		|       2                |  2^11.5 < OSR Value <= 2^12(4096)        |
		|       3                |  2^11 < OSR Value <= 2^11.5(2896.3)      |
		|       4                |  2^10.5 < OSR Value <= 2^11(2048)        |
		|       5                |  2^10 < OSR Value <= 2^10.5(1448.1)      |
		|       6                |  2^9.5 < OSR Value <= 2^10(1024)         |
		|       7                |  2^9 < OSR Value <= 2^9.5(724)           |
		|       8                |  2^8.5 < OSR Value <= 2^9(512)           |
		------------------------------------- Only For A1 ------------------|
		|       9 - 15           |            reserved                      |
		------------------------------------- Only For B2 -----------------=|
		|       9                |  2^8 < OSR Value <= 2^8.5(362)           |
		|       10               |  2^7.5 < OSR Value <= 2^8(256)           |
		|       11               |  2^7 < OSR Value <= 2^7.5(181)           |
		|       12               |  2^6.5 < OSR Value <= 2^7(128)           |
		|       13               |  2^6 < OSR Value <= 2^6.5(90.5)          |
		|       14               |  2^5.5 < OSR Value <= 2^6(64)            |
		|       15               |  2^5 < OSR Value <= 2^5.5(45.2)          |

		-------------------------------------------------------------------------------------------
		*/

	    uint32_t fpress_bs_cond[] = {8192, 5793, 4096, 2896, 2048, 1448, 1024, 724, 512, 362, 256, 181, 128, 90, 64, 45, 32 };
		
		if (icp201xx_dev_version == ICP201XX_CHIP_VERSION_A)
		{
			max_idx = 8;
			max_press_bs = 8;
		}
		else
		{
			max_idx = 15;
			max_press_bs = 15;
		}
		/* converting OSR press register value to OSR value to calculate pressure bs using pressure osr value*/
		icp201xx_mode4_config_press_osr_val = (uint32_t)( (ICP201XX_MODE4_CONFIG_PRESS_OSR + 1 ) << 5) ;

	

		for( i= max_idx; i>=0; i--)
		{
			/* Parse look up table and compare PRESS_OSR reg value for press bs value **/
			if(icp201xx_mode4_config_press_osr_val <= fpress_bs_cond[i])
			{
				pres_bs = i;  /* index of look up table is press bs */
				break;
			}
		}

		if(ICP201XX_MODE4_CONFIG_TEMP_OSR == 31)
		{
			if (icp201xx_dev_version == ICP201XX_CHIP_VERSION_A)
				temp_bs = 6;
			else
				temp_bs = 7;
		}
		else
		{
			if (icp201xx_dev_version == ICP201XX_CHIP_VERSION_A)
				temp_bs = 8;
			else
				temp_bs = 9;
		}

	}while(0);

	#if ( USE_POW_FUNCTION == 1)
	//	press_gain = pow((m4_default_pres_osr+1), 2) * pow(2, m4_default_pres_bs) / (pow((ICP201XX_MODE4_CONFIG_PRESS_OSR+1), 2) * pow(2, pres_bs)) * m4_default_press_gain;

	do{
		unsigned int f_curgain;
		do {

			f_curgain = ((unsigned int)m4_default_press_gain) * Pow_rec(256,2) / ((Pow_rec((ICP201XX_MODE4_CONFIG_PRESS_OSR+1), 2) * Pow_rec(2, pres_bs))) ;
			/* if calculated gain is greater than 2.0, bs should be incremented by 1 and gain needs to be recalculated until gain is less than 2.0 */
			/* gain is in Q15 format so comparing with 2 << 15 **/
			if (f_curgain < (0x02 << 15)) 
			{
				break;
			}
			pres_bs++;
			BAR_LOG("icp inv_icp201xx_app_pre_start_config f_curgain %d, pres_bs %d\n",f_curgain,pres_bs);
	
		}while(1);
		
		press_gain = (uint16_t)(f_curgain);
	}while(0);

	BAR_LOG("icp inv_icp201xx_app_pre_start_config pres_bs %d, max_press_bs %d\n",pres_bs,max_press_bs);
	
	if (pres_bs > max_press_bs)
		return -1;

    #else
		temp1 = (uint32_t)((m4_default_pres_osr+1)*(m4_default_pres_osr+1)*(1<<m4_default_pres_bs));
		temp2 = (uint32_t)((ICP201XX_MODE4_CONFIG_PRESS_OSR+1)*(ICP201XX_MODE4_CONFIG_PRESS_OSR+1)*(1<<pres_bs));
		// Doing bit shifting to preserve precision.
		// max value for (OSR_pres+1)^2*2^BS_shift_value = 67712 (17bits)
		// 0.489 (33124/67712) < temp1/temp2 < 2.044 (67712/33124)
		// so left shift 14bits before doing division to maximize the precision without overflowing
		press_gain_32bit = ((temp1<<14) / temp2);
		press_gain_32bit *= m4_default_press_gain;
		press_gain_32bit >>= 14;

		press_gain = (uint16_t)press_gain_32bit;
	#endif

    if(obj->odr_setting !=0)
	    setting_odr =obj->odr_setting;
	else
	    setting_odr = ICP201XX_MODE4_CONFIG_ODR_SETTING;

	status = inv_icp201xx_set_mode4_config(client,
											ICP201XX_MODE4_CONFIG_PRESS_OSR,
											ICP201XX_MODE4_CONFIG_TEMP_OSR,
											setting_odr,
											ICP201XX_MODE4_CONFIG_HFOSC_EN,
											ICP201XX_MODE4_CONFIG_DVDD_EN,
											ICP201XX_MODE4_CONFIG_IIR_EN,
											fir_en,
											ICP201XX_MODE4_CONFIG_IIR_K_FACTOR,pres_bs,temp_bs,press_gain);

	return status;
}

int inv_icp201xx_wr_meas_mode(struct i2c_client *client, icp201xx_meas_mode_t new_value)
{
	int status;
	uint8_t reg_value = 0, read_value = 0;
	status = icp201xx_i2c_read_register(client, MPUREG_MODE_SELECT, &reg_value,1);

	if(status < 0)
	return status;

	reg_value = ( reg_value & (~BIT_MEAS_MODE_MASK) ) | ( new_value << BIT_FORCED_MEAS_MODE_POS) ;
	do {
		inv_icp201xx_rd_mode_sync_status(client,&read_value);
		if ( read_value  )
			break;
		usleep_range(500,600);
	}while(1);

	status = icp201xx_i2c_write_register(client, MPUREG_MODE_SELECT, &reg_value,1);

	return status;
}

int inv_icp201xx_wr_forced_meas_trigger(struct i2c_client *client, icp201xx_forced_meas_trigger_t new_value)
{
	int status;
	uint8_t reg_value = 0, read_value = 0;
	status = icp201xx_i2c_read_register(client, MPUREG_MODE_SELECT, &reg_value,1);

	if(status < 0)
	return status;

	reg_value = ( reg_value & (~BIT_FORCED_MEAS_TRIGGER_MASK) ) | ( new_value << BIT_FORCED_MEAS_TRIGGER_POS) ;
	do {
		inv_icp201xx_rd_mode_sync_status(client,&read_value);
		if ( read_value  )
			break;
		usleep_range(500,600);
	}while(1);

	status = icp201xx_i2c_write_register(client, MPUREG_MODE_SELECT, &reg_value,1);

	return status;
}

int inv_icp201xx_wr_fifo_readout_mode(struct i2c_client *client, icp201xx_FIFO_readout_mode_t new_value)
{
	int status;
	uint8_t reg_value = 0, read_value = 0;
	status = icp201xx_i2c_read_register(client, MPUREG_MODE_SELECT, &reg_value,1);

	if(status < 0)
	return status;

	reg_value = ( reg_value & (~BIT_FIFO_READOUT_MODE_MASK) ) | ( new_value ) ;

	do {
		inv_icp201xx_rd_mode_sync_status(client,&read_value);
		if ( read_value  )
			break;
		usleep_range(500,600);
	}while(1);

	status = icp201xx_i2c_write_register(client, MPUREG_MODE_SELECT, &reg_value,1);

	return status;
}

int inv_icp201xx_wr_meas_config(struct i2c_client *client, icp201xx_op_mode_t new_value)
{
	int status;
	uint8_t reg_value = 0, read_value = 0;
	status = icp201xx_i2c_read_register(client, MPUREG_MODE_SELECT, &reg_value,1);

	if(status < 0)
	return status;

	reg_value = ( reg_value & (~BIT_MEAS_CONFIG_MASK) ) | ( new_value << BIT_MEAS_CONFIG_POS) ;

	do {
		inv_icp201xx_rd_mode_sync_status(client,&read_value);
		if ( read_value  )
			break;
		usleep_range(500,600);
	}while(1);

	status = icp201xx_i2c_write_register(client, MPUREG_MODE_SELECT, &reg_value,1);

	return status;
}

int  inv_icp201xx_config(struct i2c_client *client,icp201xx_op_mode_t op_mode,icp201xx_FIFO_readout_mode_t fifo_read_mode)
{
    struct icp_i2c_data *obj;
	uint8_t reg_value = 0;
	int status = 0;
	if ( op_mode >= ICP201XX_OP_MODE_MAX )
		return -1 ;

	obj = i2c_get_clientdata(client);

	status |= inv_icp201xx_wr_mode_select(client,reg_value); /* Set Sensor in Standby mode */

	status |= inv_icp201xx_wr_flush_fifo(client);

	status |= icp201xx_i2c_read_register(client, MPUREG_INTERRUPT_STATUS, &reg_value,1);
	if ( reg_value )
		status |= icp201xx_i2c_write_register(client, MPUREG_INTERRUPT_STATUS, &reg_value,1);  //clear int status

	/** FIFO config **/

	status |= inv_icp201xx_wr_forced_meas_trigger(client,ICP201XX_FORCE_MEAS_STANDBY);

	status |= inv_icp201xx_wr_pow_mode(client,ICP201XX_POWER_MODE_NORMAL);

	status |= inv_icp201xx_wr_fifo_readout_mode(client,fifo_read_mode);
	obj->fifo_readout_mode = fifo_read_mode;
	obj->fifo_packets_to_skip = 1;    // no fir ,discard 1 sample only
	
	status |= inv_icp201xx_wr_meas_config(client,op_mode);
	status |= inv_icp201xx_wr_meas_mode(client,ICP201XX_MEAS_MODE_CONTINUOUS);	
	
	return status;
}

/* icp setting initialization */
static int icp_init_client(struct i2c_client *client)
{
	int err = 0;

	/* BAR_FUN(); */
	msleep(10);
	
	err = inv_icp201xx_get_who_am_i(client);
	if (err < 0) {
		BAR_ERR("get chip type failed, err = %d\n", err);
		return err;
	}
	
	err = inv_icp201xx_soft_reset(client);
	if (err < 0) {
		BAR_ERR("inv_icp201xx_soft_reset err = %d\n", err);
		return err;
	}

    err = inv_icp201xx_OTP_bootup_cfg(client);

	if (err < 0) {
		BAR_ERR("inv_icp201xx_OTP_bootup_cfg err = %d\n", err);
		return err;
	}

	return 0;
}

static ssize_t chipinfo_show(struct device_driver *ddri, char *buf)
{
	struct icp_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		BAR_ERR("icp i2c data pointer is null\n");
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", obj->sensor_name);
}

static ssize_t sensordata_show(struct device_driver *ddri, char *buf)
{
	struct icp_i2c_data *obj = obj_i2c_data;
	int pressure_data;
	char strbuf[ICP_BUFSIZE] = "";

	if (obj == NULL) {
		BAR_ERR("icp i2c data pointer is null\n");
		return 0;
	}

	icp_get_pressure(obj->client, &pressure_data);
	sprintf(strbuf, "%08x", pressure_data);

	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static ssize_t trace_show(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct icp_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		BAR_ERR("icp i2c data pointer is null\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

static ssize_t trace_store(struct device_driver *ddri, const char *buf, size_t count)
{
	struct icp_i2c_data *obj = obj_i2c_data;
	int trace;

	if (obj == NULL) {
		BAR_ERR("i2c_data obj is null\n");
		return 0;
	}

	if (sscanf(buf, "0x%x", &trace) == 1)
		atomic_set(&obj->trace, trace);
	else
		BAR_ERR("invalid content: '%s', length = %d\n", buf, (int)count);

	return count;
}

static ssize_t status_show(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct icp_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		BAR_ERR("icp i2c data pointer is null\n");
		return 0;
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "CUST: %d %d (%d %d)\n",
			obj->hw.i2c_num,
			obj->hw.direction, obj->hw.power_id, obj->hw.power_vol);

	len += snprintf(buf + len, PAGE_SIZE - len, "i2c addr:%#x,ver:%s\n",
			obj->client->addr, ICP_DRIVER_VERSION);

	return len;
}

static ssize_t powermode_show(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct icp_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		BAR_ERR("icp i2c data pointer is null\n");
		return 0;
	}

	return len;
}

static ssize_t powermode_store(struct device_driver *ddri, const char *buf, size_t count)
{
	struct icp_i2c_data *obj = obj_i2c_data;
	unsigned long power_mode;
	int err;

	if (obj == NULL) {
		BAR_ERR("icp i2c data pointer is null\n");
		return 0;
	}

	err = kstrtoul(buf, 10, &power_mode);

	return err;
}

static ssize_t selftest_show(struct device_driver *ddri, char *buf)
{
	struct icp_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		BAR_ERR("icp i2c data pointer is null\n");
		return 0;
	}

	return 0;
}

/* prize added by chenjiaxi, barometer calibration, 20220423-start */
static int icp_set_cali(uint8_t *data, uint8_t count)
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

static DRIVER_ATTR_RO(chipinfo);//, S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR_RO(sensordata);//, S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR_RW(trace);//, S_IWUSR | S_IRUGO, show_trace_value, store_trace_value);
static DRIVER_ATTR_RO(status);//, S_IRUGO, show_status_value, NULL);
static DRIVER_ATTR_RW(powermode);//, S_IWUSR | S_IRUGO, show_power_mode_value, store_power_mode_value);
static DRIVER_ATTR_RO(selftest);//, S_IRUGO, show_selftest_value, NULL);
/* prize added by chenjiaxi, barometer calibration, 20220423-start */
static DRIVER_ATTR_WO(test_cali);
static DRIVER_ATTR_RO(read_cali_data);
/* prize added by chenjiaxi, barometer calibration, 20220423-end */

static struct driver_attribute *icp_attr_list[] = {
	&driver_attr_chipinfo,	/* chip information */
	&driver_attr_sensordata,	/* dump sensor data */
	&driver_attr_trace,	/* trace log */
	&driver_attr_status,	/* cust setting */
	&driver_attr_powermode,	/* power mode */
	&driver_attr_selftest,	/* self test */
	/* prize added by chenjiaxi, barometer calibration, 20220423-start */
	&driver_attr_test_cali,  /* test cali */
	&driver_attr_read_cali_data, /* read cali data */
	/* prize added by chenjiaxi, barometer calibration, 20220423-end */
};

static int icp_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(icp_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, icp_attr_list[idx]);
		if (err) {
			BAR_ERR("driver_create_file (%s) = %d\n",
				icp_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int icp_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(icp_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, icp_attr_list[idx]);

	return err;
}

#ifdef CONFIG_ID_TEMPERATURE
int temperature_operate(void *self, uint32_t command, void *buff_in,
			int size_in, void *buff_out, int size_out, int *actualout)
{
	int err = 0;
	int value;
	struct icp_i2c_data *priv = (struct icp_i2c_data *)self;
	hwm_sensor_data *temperature_data;
	char buff[ICP_BUFSIZE];

	switch (command) {
	case SENSOR_DELAY:
		/* under construction */
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			BAR_ERR("enable sensor parameter error\n");
			err = -EINVAL;
		} else {
			/* value:[0--->suspend, 1--->normal] */
			value = *(int *)buff_in;
			BAR_LOG("sensor enable/disable command: %s\n",
				value ? "enable" : "disable");

			err = icp_set_powermode(priv->client, (enum ICP_POWERMODE_ENUM)(!!value));
			if (err)
				BAR_ERR("set power mode failed, err = %d\n", err);
		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) || (size_out < sizeof(hwm_sensor_data))) {
			BAR_ERR("get sensor data parameter error\n");
			err = -EINVAL;
		} else {
			temperature_data = (hwm_sensor_data *) buff_out;
			err = icp_get_temperature(priv->client, buff, ICP_BUFSIZE);
			if (err) {
				BAR_ERR("get compensated temperature value failed,err = %d\n", err);
				return -1;
			}
			if (kstrtoint(buff, 16, &temperature_data->values[0]) != 0)
				BAR_ERR("sscanf parsing fail\n");
			temperature_data->values[1] = temperature_data->values[2] = 0;
			temperature_data->status = SENSOR_STATUS_ACCURACY_HIGH;
			temperature_data->value_divide = 100;
		}
		break;

	default:
		BAR_ERR("temperature operate function no this parameter %d\n", command);
		err = -1;
		break;
	}

	return err;
}
#endif				/* CONFIG_ID_TEMPERATURE */

#ifdef CONFIG_PM_SLEEP
static int icp_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct icp_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	if (obj == NULL) {
		BAR_ERR("null pointer\n");
		return -EINVAL;
	}

	if (atomic_read(&obj->trace) & BAR_TRC_INFO)
		BAR_FUN();


	atomic_set(&obj->suspend, 1);
	//err = icp_set_powermode(obj->client, ICP_SUSPEND_MODE);
	if (err) {
		BAR_ERR("icp set suspend mode failed, err = %d\n", err);
		return err;
	}
	return err;
}

static int icp_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct icp_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	if (obj == NULL) {
		BAR_ERR("null pointer\n");
		return -EINVAL;
	}

	if (atomic_read(&obj->trace) & BAR_TRC_INFO)
		BAR_FUN();

	if (err) {
		BAR_ERR("initialize client fail\n");
		return err;
	}

	//err = icp_set_powermode(obj->client, ICP_NORMAL_MODE);
	if (err) {
		BAR_ERR("icp set normal mode failed, err = %d\n", err);
		return err;
	}

	atomic_set(&obj->suspend, 0);
	return 0;
}
#endif

static int icp_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strlcpy(info->type, ICP_DEV_NAME, sizeof(info->type));
	return 0;
}

static int icp_open_report_data(int open)
{
	/* should queuq work to report event if  is_report_input_direct=true */
	return 0;
}

static int icp_enable_nodata(int en)
{
    int ret =0;
    if(en){
        ret = inv_icp201xx_app_pre_start_config(obj_i2c_data->client,ICP201XX_OP_MODE4,ICP201XX_MEAS_MODE_CONTINUOUS);
        ret |= inv_icp201xx_set_fifo_notification_config(obj_i2c_data->client, 0 ,0,0);
        ret |= inv_icp201xx_config(obj_i2c_data->client,ICP201XX_OP_MODE4,ICP201XX_FIFO_READOUT_MODE_PRES_TEMP);

        if(ret < 0){
            BAR_LOG("icp_set_powermode fail!\n");
            return -1;
            }
        else
	        BAR_LOG("icp_set_powermode OK!\n");
	}
	else{
        ret = inv_icp201xx_soft_reset(obj_i2c_data->client);
	    if (ret < 0) {
		    BAR_ERR("icp disable err = %d\n", ret);
		    return ret;
	    }
	}
	need_warm_up = true;
	return ret;
}

static int icp_set_delay(u64 nanosec)
{
    unsigned int delay_ns =(unsigned int)nanosec ;
    unsigned int usec ;
    unsigned int msec;
    int ret =0;

    usec = delay_ns/1000;
    msec = usec/1000;

    BAR_LOG("icp_set_delay %d!\n",msec);

    // /* odr_setting = ( 8000 / ODR in Hz ) -1  : 25 Hz => ODR setting = 320(0x140) **/
    //currently max odr setting is 40hz
    if(msec < 25){
	    BAR_LOG("too high odr over 40hz %d!\n",msec);
        usec = 25 * 1000;
	}

    obj_i2c_data->odr_setting = (uint16_t) (8000/(1000000/usec)-1);

    BAR_LOG("icp odr_setting %d!\n",obj_i2c_data->odr_setting);

     inv_icp201xx_soft_reset(obj_i2c_data->client);
     ret |= inv_icp201xx_app_pre_start_config(obj_i2c_data->client,ICP201XX_OP_MODE4,ICP201XX_MEAS_MODE_CONTINUOUS);
     ret |= inv_icp201xx_set_fifo_notification_config(obj_i2c_data->client, 0 ,0,0);
     ret |= inv_icp201xx_config(obj_i2c_data->client,ICP201XX_OP_MODE4,ICP201XX_FIFO_READOUT_MODE_PRES_TEMP);

     msleep(50);
	 need_warm_up = true;

	 if(ret != 0)
	 	    BAR_LOG("icp_set_delay fail!\n");

	return ret;
}

static int icp_batch(int flag, int64_t samplingPeriodNs, int64_t maxBatchReportLatencyNs)
{
	return icp_set_delay((u64)samplingPeriodNs);
}

static int icp_flush(void)
{
	return baro_flush_report();
}

static int icp_get_data(int *value, int *status)
{
	//char buff[ICP_BUFSIZE];
	int pressure_data;
	int err = 0;

	err = icp_get_pressure(obj_i2c_data->client, &pressure_data);
    //BAR_ERR("icp_get_pressure %d ,value %d\n",err,pressure_data);
	if (err) {
		BAR_ERR("get data pressure value failed, err = %d\n", err);
		return -1;
	}

	*value = pressure_data;
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;
    //BAR_ERR("icp_get_dat2 %d\n",*value);
	return 0;
}

static int icp_factory_enable_sensor(bool enabledisable, int64_t sample_periods_ms)
{
	int err = 0;

	err = icp_enable_nodata(enabledisable == true ? 1 : 0);
	if (err) {
		BAR_ERR("%s enable sensor failed!\n", __func__);
		return -1;
	}
	err = icp_batch(0, sample_periods_ms * 1000000, 0);
	if (err) {
		BAR_ERR("%s enable set batch failed!\n", __func__);
		return -1;
	}
	return 0;
}
static int icp_factory_get_data(int32_t *data)
{
	int err = 0, status = 0;

	err = icp_get_data(data, &status);
	if (err < 0) {
		BAR_ERR("%s get data fail\n", __func__);
		return -1;
	}
	return 0;
}
static int icp_factory_get_raw_data(int32_t *data)
{
	return 0;
}
static int icp_factory_enable_calibration(void)
{
	return 0;
}
static int icp_factory_clear_cali(void)
{
	return 0;
}
static int icp_factory_set_cali(int32_t offset)
{
	return 0;
}
static int icp_factory_get_cali(int32_t *offset)
{
	return 0;
}
static int icp_factory_do_self_test(void)
{
	return 0;
}

static struct baro_factory_fops icp_factory_fops = {
	.enable_sensor = icp_factory_enable_sensor,
	.get_data = icp_factory_get_data,
	.get_raw_data = icp_factory_get_raw_data,
	.enable_calibration = icp_factory_enable_calibration,
	.clear_cali = icp_factory_clear_cali,
	.set_cali = icp_factory_set_cali,
	.get_cali = icp_factory_get_cali,
	.do_self_test = icp_factory_do_self_test,
};

static struct baro_factory_public icp_factory_device = {
	.gain = 1,
	.sensitivity = 1,
	.fops = &icp_factory_fops,
};

static int icp_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct icp_i2c_data *obj = NULL;
	struct baro_control_path ctl = { 0 };
	struct baro_data_path data = { 0 };
#ifdef CONFIG_ID_TEMPERATURE
	struct hwmsen_object sobj_t;
#endif
	int err = 0;
    int pow_value;

	BAR_FUN();

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	err = get_baro_dts_func(client->dev.of_node, &obj->hw);
	if (err < 0) {
		BAR_ERR("get cust_baro dts info fail\n");
		goto exit_init_client_failed;
	}

	obj_i2c_data = obj;
	obj->client = client;
	obj->fifo_packets_to_skip = 1;
	obj->fifo_readout_mode = ICP201XX_FIFO_READOUT_MODE_PRES_TEMP;
	obj->odr_setting = 0;

	i2c_set_clientdata(client, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);

	mutex_init(&obj->lock);

	err = icp_init_client(client);
	if (err)
		goto exit_init_client_failed;

	/* err = misc_register(&icp_device); */
	err = baro_factory_device_register(&icp_factory_device);
	if (err) {
		BAR_ERR("baro_factory device register failed, err = %d\n", err);
		goto exit_misc_device_register_failed;
	}

	err = icp_create_attr(&(icp_init_info.platform_diver_addr->driver));
	if (err) {
		BAR_ERR("create attribute failed, err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.is_use_common_factory = false;
	ctl.open_report_data = icp_open_report_data;
	ctl.enable_nodata = icp_enable_nodata;
	ctl.set_delay = icp_set_delay;
	/* prize added by chenjiaxi, barometer calibration, 20220423-start */
	ctl.set_cali = icp_set_cali;
	/* prize added by chenjiaxi, barometer calibration, 20220423-end */
	ctl.batch = icp_batch;
	ctl.flush = icp_flush;

	ctl.is_report_input_direct = false;
	ctl.is_support_batch = obj->hw.is_batch_supported;

	pow_value = Pow_rec(2,10);
	BAR_ERR("pow test A %d\n",pow_value);

	pow_value = Pow_rec(100,2);
	BAR_ERR("pow test B %d\n",pow_value);
	
	err = baro_register_control_path(&ctl);
	if (err) {
		BAR_ERR("register baro control path err\n");
		goto exit_hwmsen_attach_pressure_failed;
	}

	data.get_data = icp_get_data;
	data.vender_div = 100;
	err = baro_register_data_path(&data);
	if (err) {
		BAR_ERR("baro_register_data_path failed, err = %d\n", err);
		goto exit_hwmsen_attach_pressure_failed;
	}
#if 0
	err =
	    batch_register_support_info(ID_PRESSURE, obj->hw.is_batch_supported, data.vender_div,
					0);
	if (err) {
		BAR_ERR("register baro batch support err = %d\n", err);
		goto exit_hwmsen_attach_pressure_failed;
	}
#endif
#ifdef CONFIG_ID_TEMPERATURE
	sobj_t.self = obj;
	sobj_t.polling = 1;
	sobj_t.sensor_operate = temperature_operate;
	err = hwmsen_attach(ID_TEMPRERATURE, &sobj_t);
	if (err) {
		BAR_ERR("hwmsen attach failed, err = %d\n", err);
		goto exit_hwmsen_attach_temperature_failed;
	}
#endif				/* CONFIG_ID_TEMPERATURE */

	icp_init_flag = 0;
	need_warm_up = true;
/* prize added by chenjiaxi, 8802 info, 20220429-start */
#ifdef CONFIG_PRIZE_HARDWARE_INFO
	strcpy(current_barosensor_info.chip, "icp20100");
	sprintf(current_barosensor_info.id, "0x%x", EXPECTED_DEVICE_ID);
	strcpy(current_barosensor_info.vendor, "TDK InvenSense");
	strcpy(current_barosensor_info.more, "barometer");
#endif
/* prize added by chenjiaxi, 8802 info, 20220429-end */
	BAR_LOG("%s: OK\n", __func__);
	return 0;

#ifdef CONFIG_ID_TEMPERATURE
exit_hwmsen_attach_temperature_failed:
	hwmsen_detach(ID_PRESSURE);
#endif				/* CONFIG_ID_TEMPERATURE */
exit_hwmsen_attach_pressure_failed:
	icp_delete_attr(&(icp_init_info.platform_diver_addr->driver));
exit_create_attr_failed:
	/* misc_deregister(&icp_device); */
exit_misc_device_register_failed:
exit_init_client_failed:
	kfree(obj);
exit:
	obj = NULL;
	obj_i2c_data = NULL;
	BAR_ERR("err = %d\n", err);
	icp_init_flag = -1;
	return err;
}

static int icp_i2c_remove(struct i2c_client *client)
{
	int err = 0;

#ifdef CONFIG_ID_TEMPERATURE
	err = hwmsen_detach(ID_TEMPRERATURE);
	if (err)
		BAR_ERR("hwmsen_detach ID_TEMPRERATURE failed, err = %d\n", err);
#endif

	err = icp_delete_attr(&(icp_init_info.platform_diver_addr->driver));
	if (err)
		BAR_ERR("icp_delete_attr failed, err = %d\n", err);

	/* misc_deregister(&icp_device); */
	baro_factory_device_deregister(&icp_factory_device);

	obj_i2c_data = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}

static int icp_remove(void)
{
	/*struct baro_hw *hw = get_cust_baro(); */

	BAR_FUN();
	i2c_del_driver(&icp_i2c_driver);
	return 0;
}

static int icp_local_init(void)
{
	if (i2c_add_driver(&icp_i2c_driver)) {
		BAR_ERR("add driver error\n");
		return -1;
	}
	if (-1 == icp_init_flag)
		return -1;

	BAR_FUN();
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id baro_of_match[] = {
	{.compatible = "mediatek,barometer"},
	{.compatible = "tdk,icp201xx"},
	{},
};
#endif

#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops icp201xx_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(icp_suspend, icp_resume)
};
#endif


static struct i2c_driver icp_i2c_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = ICP_DEV_NAME,
#ifdef CONFIG_PM_SLEEP
		   .pm = &icp201xx_pm_ops,
#endif
#ifdef CONFIG_OF
		   .of_match_table = baro_of_match,
#endif
		   },
	.probe = icp_i2c_probe,
	.remove = icp_i2c_remove,
	.detect = icp_i2c_detect,
	.id_table = icp_i2c_id,
};

static int __init icp_init(void)
{
	BAR_FUN();
#ifdef CONFIG_MTK_LEGACY
	i2c_register_board_info(hw.i2c_num, &icp_i2c_info, 1);
#endif
	baro_driver_add(&icp_init_info);

	return 0;
}

static void __exit icp_exit(void)
{
	BAR_FUN();
}
module_init(icp_init);
module_exit(icp_exit);

/*MODULE_LICENSE("GPLv2");*/
MODULE_DESCRIPTION("ICP201XX I2C Driver");
MODULE_AUTHOR("jonny@invensense.com /Terry tao/");
MODULE_VERSION(ICP_DRIVER_VERSION);
