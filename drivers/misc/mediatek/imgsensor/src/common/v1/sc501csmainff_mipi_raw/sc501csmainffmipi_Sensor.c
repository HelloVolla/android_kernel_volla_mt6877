 /*
 *
 * Filename:
 * ---------
 *     sc501csmainffmipi_Sensor.c
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     Source code of Sensor driver
 *
 *
 *-----------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 */

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "sc501csmainffmipi_Sensor.h"

/************************** Modify Following Strings for Debug **************************/
#define PFX "sc501csmainff_camera_sensor_JF"
#define LOG_1 LOG_INF("SC501CSMAINFF, MIPI 2LANE\n")
/****************************   Modify end    *******************************************/

#define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __func__, ##args)

/*prize add by zhuzhengjiang for mtk otp 20210730 start*/
#if SC501CSMAINFF_OTP_FOR_MTK
#define SC501CSMAINFF_OTP_DATA_DUMP    0
struct imgsensor_otp_info_struct sc501csmainff_otp_info ={0};
char sc501csmainff_otp_data[SC501CSMAINFF_OTP_DATA_SIZE]={0};
#endif
/*prize add by zhuzhengjiang for mtk otp 20210730 end*/
static DEFINE_SPINLOCK(imgsensor_drv_lock);
extern int curr_sensor_id;
static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = SC501CSMAINFFMIPI_SENSOR_ID,
	.checksum_value = 0x61a7930e,

	.pre = {
		.pclk = 90000000,
		.linelength = 1500,
		.framelength = 2000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 60000000,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 90000000,
		.linelength = 1500,
		.framelength = 2000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 60000000,
		.max_framerate = 300,
	},
	.cap1 = {
		.pclk = 90000000,
		.linelength = 1500,
		.framelength = 2500,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 60000000,
		.max_framerate = 240,
	},
	.normal_video = {
		.pclk = 90000000,
		.linelength = 1500,
		.framelength = 2000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 60000000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 90000000,
		.linelength = 1500,
		.framelength = 2000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 60000000,
		.max_framerate = 300,
	},
	.slim_video = {
		.pclk = 90000000,
		.linelength = 1500,
		.framelength = 2000,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 60000000,
		.max_framerate = 300,
	},

	.margin = 5,
	.min_shutter = 2,
	.max_frame_length = 0x3fff,

	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,
	.ihdr_le_firstline = 0,
	.sensor_mode_num = 5,

	.cap_delay_frame = 2,
	.pre_delay_frame = 2,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_2MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
#if SC501CSMAINFF_MIRROR_FLIP_ENABLE
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
#else
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
#endif
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_2_LANE,
	.i2c_addr_table = {0x20, 0x6C, 0xff},
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x7d0,
	.gain = 0x40,
	.dummy_pixel = 0,
	.dummy_line = 0,
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x6c,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{2592, 1944, 0, 0, 2592, 1944, 2592, 1944, 0000, 0000, 2592, 1944, 0, 0, 2592, 1944},/* Preview */
	{2592, 1944, 0, 0, 2592, 1944, 2592, 1944, 0000, 0000, 2592, 1944, 0, 0, 2592, 1944},/* capture */
	{2592, 1944, 0, 0, 2592, 1944, 2592, 1944, 0000, 0000, 2592, 1944, 0, 0, 2592, 1944},/* video  */
	{2592, 1944, 0, 0, 2592, 1944, 2592, 1944, 0000, 0000, 2592, 1944, 0, 0, 2592, 1944},/* hs video */
	{2592, 1944, 0, 0, 2592, 1944, 2592, 1944, 0000, 0000, 2592, 1944, 0, 0, 2592, 1944} /*slim video */
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	kal_uint32 frame_length = imgsensor.frame_length;
	write_cmos_sensor(0x320e, (frame_length >> 8) & 0xff);
	write_cmos_sensor(0x320f, frame_length & 0xff);
	LOG_INF("Exit! framelength = %d\n", frame_length);
}

static kal_uint32 return_sensor_id(void)
{
	return ((read_cmos_sensor(0x3107) << 8) | read_cmos_sensor(0x3108));
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	/* kal_int16 dummy_line;*/
	kal_uint32 frame_length = imgsensor.frame_length;
	/* unsigned long flags; */

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;

	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length)
		? frame_length : imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	}
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}

static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	shutter = shutter*2;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	/* if shutter bigger than frame_length, should extend frame length first */
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length * 2 - imgsensor_info.margin)
		imgsensor.frame_length = (shutter + imgsensor_info.margin + 1) / 2;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length*2 - imgsensor_info.margin)) ?
		(imgsensor_info.max_frame_length*2 - imgsensor_info.margin) : shutter;

	realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
/*
*	if (imgsensor.autoflicker_en) {
*		if (realtime_fps >= 297 && realtime_fps <= 305)
*			set_max_framerate(296, 0);
*		else if (realtime_fps >= 147 && realtime_fps <= 150)
*			set_max_framerate(146, 0);
*		else
*			set_max_framerate(realtime_fps, 0);
*	} else
*		set_max_framerate(realtime_fps, 0);
*/
	write_cmos_sensor(0x3e20, (shutter >> 20) & 0x0F);
	write_cmos_sensor(0x3e00, (shutter >> 12) & 0xFF);
	write_cmos_sensor(0x3e01, (shutter >>  4) & 0xFF);
	write_cmos_sensor(0x3e02, (shutter <<  4) & 0xF0);

	LOG_INF("Exit! shutter = %d, framelength = %d\n", shutter, imgsensor.frame_length);
}

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint16 reg_gain = gain << 4;

	if (reg_gain < SC501CSMAINFF_SENSOR_GAIN_BASE)
		reg_gain = SC501CSMAINFF_SENSOR_GAIN_BASE;
	else if (reg_gain > SC501CSMAINFF_SENSOR_GAIN_MAX)
		reg_gain = SC501CSMAINFF_SENSOR_GAIN_MAX;

	return (kal_uint16)reg_gain;
}
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;
	kal_uint32 temp_gain;
	kal_int16 gain_index;
	kal_uint16 SC501CSMAINFF_AGC_Param[SC501CSMAINFF_SENSOR_GAIN_MAP_SIZE][2] = {
		{  1024,  0x00 },
		{  2048,  0x01 },
		{  4096,  0x03 },
		{  8192,  0x07 },
        { 16384,  0x0f },
        { 32768,  0x1f },
	};

	reg_gain = gain2reg(gain);

	for (gain_index = SC501CSMAINFF_SENSOR_GAIN_MAX_VALID_INDEX - 1; gain_index >= 0; gain_index--)
		if (reg_gain >= SC501CSMAINFF_AGC_Param[gain_index][0])
			break;

	write_cmos_sensor(0x3e09, SC501CSMAINFF_AGC_Param[gain_index][1]);
	temp_gain = reg_gain * SC501CSMAINFF_SENSOR_GAIN_BASE / SC501CSMAINFF_AGC_Param[gain_index][0];
	write_cmos_sensor(0x3e07, (temp_gain >> 3) & 0xff);
	
	LOG_INF("Exit! SC501CSMAINFF_AGC_Param[gain_index][1] = 0x%x, temp_gain = 0x%x, gain = 0x%x, reg_gain = %d\n",
		SC501CSMAINFF_AGC_Param[gain_index][1], temp_gain, gain, reg_gain);

	return reg_gain;
}

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le: 0x%x, se: 0x%x, gain: 0x%x\n", le, se, gain);
}


/*static void set_mirror_flip(kal_uint8 image_mirror)
*{
*	LOG_INF("image_mirror = %d\n", image_mirror);
*}
*/

static void night_mode(kal_bool enable)
{
	/* No Need to implement this function */
}

static void sensor_init(void)
{
	LOG_INF("init begin\n");
     write_cmos_sensor(0x0103,0x01); // stream off
}

static void preview_setting(void)
{
	LOG_INF("E\n");
	/* System */
     write_cmos_sensor(0x0103,0x01);
     mdelay(10);
     write_cmos_sensor(0x0100,0x00);
     write_cmos_sensor(0x36e9,0x80);
     write_cmos_sensor(0x37f9,0x80);
     write_cmos_sensor(0x36e9,0x23);
     write_cmos_sensor(0x37f9,0x23);
     write_cmos_sensor(0x301f,0x01);
     write_cmos_sensor(0x3253,0x10);
     write_cmos_sensor(0x3301,0x12);
     write_cmos_sensor(0x3306,0x38);
     write_cmos_sensor(0x330b,0xa8);
     write_cmos_sensor(0x3333,0x10);
     write_cmos_sensor(0x3364,0x56);
     write_cmos_sensor(0x3390,0x0b);
     write_cmos_sensor(0x3391,0x0f);
     write_cmos_sensor(0x3392,0x1f);
     write_cmos_sensor(0x3393,0x20);
     write_cmos_sensor(0x3394,0x40);
     write_cmos_sensor(0x3395,0x58);
     write_cmos_sensor(0x33b3,0x40);
     write_cmos_sensor(0x349f,0x1e);
     write_cmos_sensor(0x34a6,0x09);
     write_cmos_sensor(0x34a7,0x0f);
     write_cmos_sensor(0x34a8,0x38);
     write_cmos_sensor(0x34a9,0x28);
     write_cmos_sensor(0x34f8,0x1f);
     write_cmos_sensor(0x34f9,0x28);
     write_cmos_sensor(0x3630,0xa0);
     write_cmos_sensor(0x3633,0x43);
     write_cmos_sensor(0x3637,0x45);
     write_cmos_sensor(0x363c,0xc1);
     write_cmos_sensor(0x3670,0x4a);
     write_cmos_sensor(0x3674,0xc0);
     write_cmos_sensor(0x3675,0xa8);
     write_cmos_sensor(0x3676,0xac);
     write_cmos_sensor(0x367c,0x08);
     write_cmos_sensor(0x367d,0x0b);
     write_cmos_sensor(0x3690,0x53);
     write_cmos_sensor(0x3691,0x53);
     write_cmos_sensor(0x3692,0x63);
     write_cmos_sensor(0x3698,0x85);
     write_cmos_sensor(0x3699,0x8c);
     write_cmos_sensor(0x369a,0x9b);
     write_cmos_sensor(0x369b,0xb8);
     write_cmos_sensor(0x369c,0x0f);
     write_cmos_sensor(0x369d,0x1f);
     write_cmos_sensor(0x36a2,0x09);
     write_cmos_sensor(0x36a3,0x0b);
     write_cmos_sensor(0x36a4,0x0f);
     write_cmos_sensor(0x36b0,0x4c);
     write_cmos_sensor(0x36b1,0xd8);
     write_cmos_sensor(0x36b2,0x01);
     write_cmos_sensor(0x3722,0x03);
     write_cmos_sensor(0x3724,0xa1);
     write_cmos_sensor(0x3903,0xa0);
     write_cmos_sensor(0x3905,0x4c);
     write_cmos_sensor(0x391d,0x04);
     write_cmos_sensor(0x3926,0x21);
     write_cmos_sensor(0x393f,0x80);
     write_cmos_sensor(0x3940,0x80);
     write_cmos_sensor(0x3941,0x00);
     write_cmos_sensor(0x3942,0x7f);
     write_cmos_sensor(0x3943,0x7f);
     write_cmos_sensor(0x3e00,0x00);
     write_cmos_sensor(0x3e01,0xf9);
     write_cmos_sensor(0x3e02,0x60);
     write_cmos_sensor(0x4402,0x02);
     write_cmos_sensor(0x4403,0x0a);
     write_cmos_sensor(0x4404,0x1c);
     write_cmos_sensor(0x4405,0x24);
     write_cmos_sensor(0x440c,0x2e);
     write_cmos_sensor(0x440d,0x2e);
     write_cmos_sensor(0x440e,0x22);
     write_cmos_sensor(0x440f,0x39);
     write_cmos_sensor(0x4424,0x01);
     write_cmos_sensor(0x4509,0x30);
     write_cmos_sensor(0x450d,0x18);
     write_cmos_sensor(0x5000,0x0e);
     write_cmos_sensor(0x5787,0x08);
     write_cmos_sensor(0x5788,0x07);
     write_cmos_sensor(0x5789,0x02);
     write_cmos_sensor(0x3221,SC501CSMAINFF_MIRROR);
     write_cmos_sensor(0x0100,0x01);
     mdelay(10);
}

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E\n");
     write_cmos_sensor(0x0103,0x01);
     mdelay(10);
     write_cmos_sensor(0x0100,0x00);
     write_cmos_sensor(0x36e9,0x80);
     write_cmos_sensor(0x37f9,0x80);
     write_cmos_sensor(0x36e9,0x23);
     write_cmos_sensor(0x37f9,0x23);
     write_cmos_sensor(0x301f,0x01);
     write_cmos_sensor(0x3253,0x10);
     write_cmos_sensor(0x3301,0x12);
     write_cmos_sensor(0x3306,0x38);
     write_cmos_sensor(0x330b,0xa8);
     write_cmos_sensor(0x3333,0x10);
     write_cmos_sensor(0x3364,0x56);
     write_cmos_sensor(0x3390,0x0b);
     write_cmos_sensor(0x3391,0x0f);
     write_cmos_sensor(0x3392,0x1f);
     write_cmos_sensor(0x3393,0x20);
     write_cmos_sensor(0x3394,0x40);
     write_cmos_sensor(0x3395,0x58);
     write_cmos_sensor(0x33b3,0x40);
     write_cmos_sensor(0x349f,0x1e);
     write_cmos_sensor(0x34a6,0x09);
     write_cmos_sensor(0x34a7,0x0f);
     write_cmos_sensor(0x34a8,0x38);
     write_cmos_sensor(0x34a9,0x28);
     write_cmos_sensor(0x34f8,0x1f);
     write_cmos_sensor(0x34f9,0x28);
     write_cmos_sensor(0x3630,0xa0);
     write_cmos_sensor(0x3633,0x43);
     write_cmos_sensor(0x3637,0x45);
     write_cmos_sensor(0x363c,0xc1);
     write_cmos_sensor(0x3670,0x4a);
     write_cmos_sensor(0x3674,0xc0);
     write_cmos_sensor(0x3675,0xa8);
     write_cmos_sensor(0x3676,0xac);
     write_cmos_sensor(0x367c,0x08);
     write_cmos_sensor(0x367d,0x0b);
     write_cmos_sensor(0x3690,0x53);
     write_cmos_sensor(0x3691,0x53);
     write_cmos_sensor(0x3692,0x63);
     write_cmos_sensor(0x3698,0x85);
     write_cmos_sensor(0x3699,0x8c);
     write_cmos_sensor(0x369a,0x9b);
     write_cmos_sensor(0x369b,0xb8);
     write_cmos_sensor(0x369c,0x0f);
     write_cmos_sensor(0x369d,0x1f);
     write_cmos_sensor(0x36a2,0x09);
     write_cmos_sensor(0x36a3,0x0b);
     write_cmos_sensor(0x36a4,0x0f);
     write_cmos_sensor(0x36b0,0x4c);
     write_cmos_sensor(0x36b1,0xd8);
     write_cmos_sensor(0x36b2,0x01);
     write_cmos_sensor(0x3722,0x03);
     write_cmos_sensor(0x3724,0xa1);
     write_cmos_sensor(0x3903,0xa0);
     write_cmos_sensor(0x3905,0x4c);
     write_cmos_sensor(0x391d,0x04);
     write_cmos_sensor(0x3926,0x21);
     write_cmos_sensor(0x393f,0x80);
     write_cmos_sensor(0x3940,0x80);
     write_cmos_sensor(0x3941,0x00);
     write_cmos_sensor(0x3942,0x7f);
     write_cmos_sensor(0x3943,0x7f);
     write_cmos_sensor(0x3e00,0x00);
     write_cmos_sensor(0x3e01,0xf9);
     write_cmos_sensor(0x3e02,0x60);
     write_cmos_sensor(0x4402,0x02);
     write_cmos_sensor(0x4403,0x0a);
     write_cmos_sensor(0x4404,0x1c);
     write_cmos_sensor(0x4405,0x24);
     write_cmos_sensor(0x440c,0x2e);
     write_cmos_sensor(0x440d,0x2e);
     write_cmos_sensor(0x440e,0x22);
     write_cmos_sensor(0x440f,0x39);
     write_cmos_sensor(0x4424,0x01);
     write_cmos_sensor(0x4509,0x30);
     write_cmos_sensor(0x450d,0x18);
     write_cmos_sensor(0x5000,0x0e);
     write_cmos_sensor(0x5787,0x08);
     write_cmos_sensor(0x5788,0x07);
     write_cmos_sensor(0x5789,0x02);
     write_cmos_sensor(0x3221,SC501CSMAINFF_MIRROR);
	if (currefps == 240) { /* PIP */
		write_cmos_sensor(0x320e, 0x09);
		write_cmos_sensor(0x320f, 0xc4);
	} else {
		write_cmos_sensor(0x320e, 0x07);
		write_cmos_sensor(0x320f, 0xd0);
	}
     write_cmos_sensor(0x0100,0x01);
     mdelay(10);	          
}

static void normal_video_setting(void)
{
	LOG_INF("E\n");
     write_cmos_sensor(0x0103,0x01);
     mdelay(10);
     write_cmos_sensor(0x0100,0x00);
     write_cmos_sensor(0x36e9,0x80);
     write_cmos_sensor(0x37f9,0x80);
     write_cmos_sensor(0x36e9,0x23);
     write_cmos_sensor(0x37f9,0x23);
     write_cmos_sensor(0x301f,0x01);
     write_cmos_sensor(0x3253,0x10);
     write_cmos_sensor(0x3301,0x12);
     write_cmos_sensor(0x3306,0x38);
     write_cmos_sensor(0x330b,0xa8);
     write_cmos_sensor(0x3333,0x10);
     write_cmos_sensor(0x3364,0x56);
     write_cmos_sensor(0x3390,0x0b);
     write_cmos_sensor(0x3391,0x0f);
     write_cmos_sensor(0x3392,0x1f);
     write_cmos_sensor(0x3393,0x20);
     write_cmos_sensor(0x3394,0x40);
     write_cmos_sensor(0x3395,0x58);
     write_cmos_sensor(0x33b3,0x40);
     write_cmos_sensor(0x349f,0x1e);
     write_cmos_sensor(0x34a6,0x09);
     write_cmos_sensor(0x34a7,0x0f);
     write_cmos_sensor(0x34a8,0x38);
     write_cmos_sensor(0x34a9,0x28);
     write_cmos_sensor(0x34f8,0x1f);
     write_cmos_sensor(0x34f9,0x28);
     write_cmos_sensor(0x3630,0xa0);
     write_cmos_sensor(0x3633,0x43);
     write_cmos_sensor(0x3637,0x45);
     write_cmos_sensor(0x363c,0xc1);
     write_cmos_sensor(0x3670,0x4a);
     write_cmos_sensor(0x3674,0xc0);
     write_cmos_sensor(0x3675,0xa8);
     write_cmos_sensor(0x3676,0xac);
     write_cmos_sensor(0x367c,0x08);
     write_cmos_sensor(0x367d,0x0b);
     write_cmos_sensor(0x3690,0x53);
     write_cmos_sensor(0x3691,0x53);
     write_cmos_sensor(0x3692,0x63);
     write_cmos_sensor(0x3698,0x85);
     write_cmos_sensor(0x3699,0x8c);
     write_cmos_sensor(0x369a,0x9b);
     write_cmos_sensor(0x369b,0xb8);
     write_cmos_sensor(0x369c,0x0f);
     write_cmos_sensor(0x369d,0x1f);
     write_cmos_sensor(0x36a2,0x09);
     write_cmos_sensor(0x36a3,0x0b);
     write_cmos_sensor(0x36a4,0x0f);
     write_cmos_sensor(0x36b0,0x4c);
     write_cmos_sensor(0x36b1,0xd8);
     write_cmos_sensor(0x36b2,0x01);
     write_cmos_sensor(0x3722,0x03);
     write_cmos_sensor(0x3724,0xa1);
     write_cmos_sensor(0x3903,0xa0);
     write_cmos_sensor(0x3905,0x4c);
     write_cmos_sensor(0x391d,0x04);
     write_cmos_sensor(0x3926,0x21);
     write_cmos_sensor(0x393f,0x80);
     write_cmos_sensor(0x3940,0x80);
     write_cmos_sensor(0x3941,0x00);
     write_cmos_sensor(0x3942,0x7f);
     write_cmos_sensor(0x3943,0x7f);
     write_cmos_sensor(0x3e00,0x00);
     write_cmos_sensor(0x3e01,0xf9);
     write_cmos_sensor(0x3e02,0x60);
     write_cmos_sensor(0x4402,0x02);
     write_cmos_sensor(0x4403,0x0a);
     write_cmos_sensor(0x4404,0x1c);
     write_cmos_sensor(0x4405,0x24);
     write_cmos_sensor(0x440c,0x2e);
     write_cmos_sensor(0x440d,0x2e);
     write_cmos_sensor(0x440e,0x22);
     write_cmos_sensor(0x440f,0x39);
     write_cmos_sensor(0x4424,0x01);
     write_cmos_sensor(0x4509,0x30);
     write_cmos_sensor(0x450d,0x18);
     write_cmos_sensor(0x5000,0x0e);
     write_cmos_sensor(0x5787,0x08);
     write_cmos_sensor(0x5788,0x07);
     write_cmos_sensor(0x5789,0x02);
     write_cmos_sensor(0x3221,SC501CSMAINFF_MIRROR);
     write_cmos_sensor(0x0100,0x01);
     mdelay(10);
}

static void hs_video_setting(void)
{
	LOG_INF("E\n");
     write_cmos_sensor(0x0103,0x01);
     mdelay(10);
     write_cmos_sensor(0x0100,0x00);
     write_cmos_sensor(0x36e9,0x80);
     write_cmos_sensor(0x37f9,0x80);
     write_cmos_sensor(0x36e9,0x23);
     write_cmos_sensor(0x37f9,0x23);
     write_cmos_sensor(0x301f,0x01);
     write_cmos_sensor(0x3253,0x10);
     write_cmos_sensor(0x3301,0x12);
     write_cmos_sensor(0x3306,0x38);
     write_cmos_sensor(0x330b,0xa8);
     write_cmos_sensor(0x3333,0x10);
     write_cmos_sensor(0x3364,0x56);
     write_cmos_sensor(0x3390,0x0b);
     write_cmos_sensor(0x3391,0x0f);
     write_cmos_sensor(0x3392,0x1f);
     write_cmos_sensor(0x3393,0x20);
     write_cmos_sensor(0x3394,0x40);
     write_cmos_sensor(0x3395,0x58);
     write_cmos_sensor(0x33b3,0x40);
     write_cmos_sensor(0x349f,0x1e);
     write_cmos_sensor(0x34a6,0x09);
     write_cmos_sensor(0x34a7,0x0f);
     write_cmos_sensor(0x34a8,0x38);
     write_cmos_sensor(0x34a9,0x28);
     write_cmos_sensor(0x34f8,0x1f);
     write_cmos_sensor(0x34f9,0x28);
     write_cmos_sensor(0x3630,0xa0);
     write_cmos_sensor(0x3633,0x43);
     write_cmos_sensor(0x3637,0x45);
     write_cmos_sensor(0x363c,0xc1);
     write_cmos_sensor(0x3670,0x4a);
     write_cmos_sensor(0x3674,0xc0);
     write_cmos_sensor(0x3675,0xa8);
     write_cmos_sensor(0x3676,0xac);
     write_cmos_sensor(0x367c,0x08);
     write_cmos_sensor(0x367d,0x0b);
     write_cmos_sensor(0x3690,0x53);
     write_cmos_sensor(0x3691,0x53);
     write_cmos_sensor(0x3692,0x63);
     write_cmos_sensor(0x3698,0x85);
     write_cmos_sensor(0x3699,0x8c);
     write_cmos_sensor(0x369a,0x9b);
     write_cmos_sensor(0x369b,0xb8);
     write_cmos_sensor(0x369c,0x0f);
     write_cmos_sensor(0x369d,0x1f);
     write_cmos_sensor(0x36a2,0x09);
     write_cmos_sensor(0x36a3,0x0b);
     write_cmos_sensor(0x36a4,0x0f);
     write_cmos_sensor(0x36b0,0x4c);
     write_cmos_sensor(0x36b1,0xd8);
     write_cmos_sensor(0x36b2,0x01);
     write_cmos_sensor(0x3722,0x03);
     write_cmos_sensor(0x3724,0xa1);
     write_cmos_sensor(0x3903,0xa0);
     write_cmos_sensor(0x3905,0x4c);
     write_cmos_sensor(0x391d,0x04);
     write_cmos_sensor(0x3926,0x21);
     write_cmos_sensor(0x393f,0x80);
     write_cmos_sensor(0x3940,0x80);
     write_cmos_sensor(0x3941,0x00);
     write_cmos_sensor(0x3942,0x7f);
     write_cmos_sensor(0x3943,0x7f);
     write_cmos_sensor(0x3e00,0x00);
     write_cmos_sensor(0x3e01,0xf9);
     write_cmos_sensor(0x3e02,0x60);
     write_cmos_sensor(0x4402,0x02);
     write_cmos_sensor(0x4403,0x0a);
     write_cmos_sensor(0x4404,0x1c);
     write_cmos_sensor(0x4405,0x24);
     write_cmos_sensor(0x440c,0x2e);
     write_cmos_sensor(0x440d,0x2e);
     write_cmos_sensor(0x440e,0x22);
     write_cmos_sensor(0x440f,0x39);
     write_cmos_sensor(0x4424,0x01);
     write_cmos_sensor(0x4509,0x30);
     write_cmos_sensor(0x450d,0x18);
     write_cmos_sensor(0x5000,0x0e);
     write_cmos_sensor(0x5787,0x08);
     write_cmos_sensor(0x5788,0x07);
     write_cmos_sensor(0x5789,0x02);
     write_cmos_sensor(0x3221,SC501CSMAINFF_MIRROR);
     write_cmos_sensor(0x0100,0x01);
     mdelay(10);
}

static void slim_video_setting(void)
{
	LOG_INF("E\n");
     write_cmos_sensor(0x0103,0x01);
     mdelay(10);
     write_cmos_sensor(0x0100,0x00);
     write_cmos_sensor(0x36e9,0x80);
     write_cmos_sensor(0x37f9,0x80);
     write_cmos_sensor(0x36e9,0x23);
     write_cmos_sensor(0x37f9,0x23);
     write_cmos_sensor(0x301f,0x01);
     write_cmos_sensor(0x3253,0x10);
     write_cmos_sensor(0x3301,0x12);
     write_cmos_sensor(0x3306,0x38);
     write_cmos_sensor(0x330b,0xa8);
     write_cmos_sensor(0x3333,0x10);
     write_cmos_sensor(0x3364,0x56);
     write_cmos_sensor(0x3390,0x0b);
     write_cmos_sensor(0x3391,0x0f);
     write_cmos_sensor(0x3392,0x1f);
     write_cmos_sensor(0x3393,0x20);
     write_cmos_sensor(0x3394,0x40);
     write_cmos_sensor(0x3395,0x58);
     write_cmos_sensor(0x33b3,0x40);
     write_cmos_sensor(0x349f,0x1e);
     write_cmos_sensor(0x34a6,0x09);
     write_cmos_sensor(0x34a7,0x0f);
     write_cmos_sensor(0x34a8,0x38);
     write_cmos_sensor(0x34a9,0x28);
     write_cmos_sensor(0x34f8,0x1f);
     write_cmos_sensor(0x34f9,0x28);
     write_cmos_sensor(0x3630,0xa0);
     write_cmos_sensor(0x3633,0x43);
     write_cmos_sensor(0x3637,0x45);
     write_cmos_sensor(0x363c,0xc1);
     write_cmos_sensor(0x3670,0x4a);
     write_cmos_sensor(0x3674,0xc0);
     write_cmos_sensor(0x3675,0xa8);
     write_cmos_sensor(0x3676,0xac);
     write_cmos_sensor(0x367c,0x08);
     write_cmos_sensor(0x367d,0x0b);
     write_cmos_sensor(0x3690,0x53);
     write_cmos_sensor(0x3691,0x53);
     write_cmos_sensor(0x3692,0x63);
     write_cmos_sensor(0x3698,0x85);
     write_cmos_sensor(0x3699,0x8c);
     write_cmos_sensor(0x369a,0x9b);
     write_cmos_sensor(0x369b,0xb8);
     write_cmos_sensor(0x369c,0x0f);
     write_cmos_sensor(0x369d,0x1f);
     write_cmos_sensor(0x36a2,0x09);
     write_cmos_sensor(0x36a3,0x0b);
     write_cmos_sensor(0x36a4,0x0f);
     write_cmos_sensor(0x36b0,0x4c);
     write_cmos_sensor(0x36b1,0xd8);
     write_cmos_sensor(0x36b2,0x01);
     write_cmos_sensor(0x3722,0x03);
     write_cmos_sensor(0x3724,0xa1);
     write_cmos_sensor(0x3903,0xa0);
     write_cmos_sensor(0x3905,0x4c);
     write_cmos_sensor(0x391d,0x04);
     write_cmos_sensor(0x3926,0x21);
     write_cmos_sensor(0x393f,0x80);
     write_cmos_sensor(0x3940,0x80);
     write_cmos_sensor(0x3941,0x00);
     write_cmos_sensor(0x3942,0x7f);
     write_cmos_sensor(0x3943,0x7f);
     write_cmos_sensor(0x3e00,0x00);
     write_cmos_sensor(0x3e01,0xf9);
     write_cmos_sensor(0x3e02,0x60);
     write_cmos_sensor(0x4402,0x02);
     write_cmos_sensor(0x4403,0x0a);
     write_cmos_sensor(0x4404,0x1c);
     write_cmos_sensor(0x4405,0x24);
     write_cmos_sensor(0x440c,0x2e);
     write_cmos_sensor(0x440d,0x2e);
     write_cmos_sensor(0x440e,0x22);
     write_cmos_sensor(0x440f,0x39);
     write_cmos_sensor(0x4424,0x01);
     write_cmos_sensor(0x4509,0x30);
     write_cmos_sensor(0x450d,0x18);
     write_cmos_sensor(0x5000,0x0e);
     write_cmos_sensor(0x5787,0x08);
     write_cmos_sensor(0x5788,0x07);
     write_cmos_sensor(0x5789,0x02);
     write_cmos_sensor(0x3221,SC501CSMAINFF_MIRROR);
     write_cmos_sensor(0x0100,0x01);
     mdelay(10);
}

static kal_uint32 set_test_pattern_mode(kal_uint32 modes,
	struct SET_SENSOR_PATTERN_SOLID_COLOR *pdata)
{
	LOG_INF("modes: %d\n", modes);

	if (modes) {
		if (modes == 5 && (pdata != NULL)) {
			write_cmos_sensor(0x4501,0xcc);
			write_cmos_sensor(0x3902,0x80);
			write_cmos_sensor(0x3908,0x00);
			write_cmos_sensor(0x390a,0xff);
			write_cmos_sensor(0x3909,0xff);
			write_cmos_sensor(0x391d,0x18);
			write_cmos_sensor(0x3e07,0x50);
		}
		else
		{
			write_cmos_sensor(0x4501, 0xbc);
			write_cmos_sensor(0x3902, 0x85);
			write_cmos_sensor(0x3909, 0x0f);
			write_cmos_sensor(0x5000, 0x00);
		}
	}
	else {
		write_cmos_sensor(0x4501, 0xb4);
		write_cmos_sensor(0x5000, 0x0e);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = modes;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

/*prize add by zhuzhengjiang for mtk otp 20210730 start*/
#if SC501CSMAINFF_OTP_FOR_MTK
/*page 0: 0x8000 ~ 0x87ff  page 1:0x8800 ~0x8fff
  otp group 1:0x80ec~0x87f3  0x8800~0x8859
  otp group 2:0x8874~0x8fd5
*/
static int sc501csmainff_custom_otp_grop1page1_read(void) {
	kal_uint16 delay=0;
	kal_uint16 i=0;
	int checksum = 0;

	write_cmos_sensor(0x4408,0x00); //start address
	write_cmos_sensor(0x4409,0x00);
	write_cmos_sensor(0x440a,0x07); //end address
	write_cmos_sensor(0x440b,0xff);

	write_cmos_sensor(0x4401, 0x1f); //0x1f:load 0x8000~0x87ff 0x1e:load 0x8800~x08fff
	write_cmos_sensor(0x4400, 0x11); //manual load
	mdelay(50);

	while((read_cmos_sensor(0x4420)&0x01) == 0x01) {//bit[0]: 0:load finish 1:loading
		delay++;
		if(delay == 1000) {
			printk("sc501csmainff otp data is still busy for loading. R0x4420[0]!=0 \n");
			return 0;
		}
	}
	if(read_cmos_sensor(SC501CSMAINFF_OTP_GROUP1_PAGE1_ADDR_OFFSET) == 1) { //group1 is valid
		for(i = 0x00; i < SC501CSMAINFF_OTP_GROUP1_PAGE1_DATA_SIZE;i++) { // read 0x80ec~0x87f3
			sc501csmainff_otp_data[i] = read_cmos_sensor(SC501CSMAINFF_OTP_GROUP1_PAGE1_ADDR_OFFSET+i);
			checksum += sc501csmainff_otp_data[i];
			#if SC501CSMAINFF_OTP_DATA_DUMP
				printk("sc501csmainff otp data group1 page1 reg:0x%x data[%d]=0x%x checksum=0x%x",SC501CSMAINFF_OTP_GROUP1_PAGE1_ADDR_OFFSET+i,i,sc501csmainff_otp_data[i],checksum);
			#endif
		}
	}
	else {
		printk("sc501csmainff otp group1 data is Invalid. need read from group2 \n");
		return 0;
	}
	return checksum;
}
static int sc501csmainff_custom_otp_grop1page2_read(void) {
	kal_uint16 delay=0;
	kal_uint16 i=0;
	int checksum = 0;

	write_cmos_sensor(0x4408,0x08); //start address
	write_cmos_sensor(0x4409,0x00);
	write_cmos_sensor(0x440a,0x0f); //end address
	write_cmos_sensor(0x440b,0xff);
		
	write_cmos_sensor(0x4401, 0x1e);//0x1f:load 0x8000~0x87ff 0x1e:load 0x8800~x08fff
	write_cmos_sensor(0x4400, 0x11);//manual load
	mdelay(50);

	while((read_cmos_sensor(0x4420)&0x01) == 0x01) {//bit[0]: 0:load finish 1:loading
		delay++;
		if(delay == 1000) {
			printk("sc501csmainff otp data is still busy for loading. R0x4420[0]!=0 \n");
			return 0;
		}
	}
	for(i = 0x00; i < SC501CSMAINFF_OTP_GROUP1_PAGE2_DATA_SIZE;i++) {// read 0x8800~8868
		sc501csmainff_otp_data[SC501CSMAINFF_OTP_GROUP1_PAGE1_DATA_SIZE+i] = read_cmos_sensor(SC501CSMAINFF_OTP_GROUP1_PAGE2_ADDR_OFFSET+i);
		checksum += sc501csmainff_otp_data[SC501CSMAINFF_OTP_GROUP1_PAGE1_DATA_SIZE+i];	
		#if SC501CSMAINFF_OTP_DATA_DUMP
			printk("sc501csmainff otp data group1 page2 reg:0x%x data[%d]=0x%x checksum=0x%x",SC501CSMAINFF_OTP_GROUP1_PAGE2_ADDR_OFFSET+i,i,sc501csmainff_otp_data[SC501CSMAINFF_OTP_GROUP1_PAGE1_DATA_SIZE+i],checksum);
		#endif
	}

	sc501csmainff_otp_info.year = read_cmos_sensor(0x886D);
	sc501csmainff_otp_info.month = read_cmos_sensor(0x886E);
	sc501csmainff_otp_info.day = read_cmos_sensor(0x886F);

	return checksum;
}
static int sc501csmainff_custom_otp_grop2_read(void) {
	kal_uint16 delay=0;
	kal_uint16 i=0;
	int checksum = 0;

	write_cmos_sensor(0x4408,0x08); //start address
	write_cmos_sensor(0x4409,0x00);
	write_cmos_sensor(0x440a,0x0f); //end address
	write_cmos_sensor(0x440b,0xff);
		
	write_cmos_sensor(0x4401, 0x1e);//0x1f:load 0x8000~0x87ff 0x1e:load 0x8800~x08fff
	write_cmos_sensor(0x4400, 0x11);//manual load
	mdelay(50);
	
	while((read_cmos_sensor(0x4420)&0x01) == 0x01) {//bit[0]: 0:load finish 1:loading
		delay++;
		if(delay == 1000) {
			printk("sc501csmainff otp data is still busy for loading. R0x4420[0]!=0 \n");
			return 0;
		}
	}
	if(read_cmos_sensor(SC501CSMAINFF_OTP_GROUP2_INFO_FLAG_ADDR_OFFSET) == 1) { //group2 is valid
		for(i = 0x00; i < SC501CSMAINFF_OTP_DATA_SIZE;i++) { // read 0x8874~0x8fef , becase checksum = sum(0x8874~0x8fe4) 
			sc501csmainff_otp_data[i] = read_cmos_sensor(SC501CSMAINFF_OTP_GROUP2_INFO_FLAG_ADDR_OFFSET+i);
			checksum += sc501csmainff_otp_data[i];
			#if SC501CSMAINFF_OTP_DATA_DUMP
				printk("sc501csmainff otp data group2 reg:0x%x data[%d]=0x%x checksum=0x%x",SC501CSMAINFF_OTP_GROUP2_INFO_FLAG_ADDR_OFFSET+i,i,sc501csmainff_otp_data[i],checksum);
			#endif
		}	
	}
	else {
		printk("sc501csmainff otp group2 data is Invalid.\n");
		return 0;
	}
	sc501csmainff_otp_info.year = read_cmos_sensor(0x8FE9);
	sc501csmainff_otp_info.month = read_cmos_sensor(0x8FEA);
	sc501csmainff_otp_info.day = read_cmos_sensor(0x8FEB);

	return checksum;
}

static int sc501csmainff_custom_read_otp_all_data(void)
{
	int checksum = 0;
	int checksumvalue = 0;

	/*read group 1*/
	//group1 R1
	printk("sc501csmainff otp data read group1 R1 start");
	memset(&sc501csmainff_otp_info, 0, sizeof(struct imgsensor_otp_info_struct));
	memset(&sc501csmainff_otp_data, 0, sizeof(sc501csmainff_otp_data));

	write_cmos_sensor(0x36b0, 0x4c);
	write_cmos_sensor(0x36b1, 0xd8);
	write_cmos_sensor(0x36b2, 0x01);

    checksum = sc501csmainff_custom_otp_grop1page1_read() + sc501csmainff_custom_otp_grop1page2_read();
	checksumvalue =  read_cmos_sensor(0x8870) + (read_cmos_sensor(0x8871) << 8) + (read_cmos_sensor(0x8872) << 16) + (read_cmos_sensor(0x8873) << 24);
#if SC501CSMAINFF_OTP_DATA_DUMP
	printk("sc501csmainff otp group1 R1 reg(0x8870~8873): 0x%x 0x%x 0x%x 0x%x ,checksum=0x%x checksumvalue=0x%x",
		  read_cmos_sensor(0x8870),read_cmos_sensor(0x8871),read_cmos_sensor(0x8872),read_cmos_sensor(0x8873),checksum,checksumvalue);
#endif
	if(checksum == checksumvalue ) {
		printk("sc501csmainff otp data read group1 R1 success");
		return 1;		
    }

	//group1 R2
	printk("sc501csmainff otp data read group1 R2 start");
	checksum = 0;
	memset(&sc501csmainff_otp_info, 0, sizeof(struct imgsensor_otp_info_struct));
	memset(&sc501csmainff_otp_data, 0, sizeof(sc501csmainff_otp_data));

	write_cmos_sensor(0x36b0, 0x4c);
	write_cmos_sensor(0x36b1, 0x98);
	write_cmos_sensor(0x36b2, 0x01);

    checksum = sc501csmainff_custom_otp_grop1page1_read() + sc501csmainff_custom_otp_grop1page2_read();
	checksumvalue =  read_cmos_sensor(0x8870) + (read_cmos_sensor(0x8871) << 8) + (read_cmos_sensor(0x8872) << 16) + (read_cmos_sensor(0x8873) << 24);
#if SC501CSMAINFF_OTP_DATA_DUMP
	printk("sc501csmainff otp group1 R2 reg(0x8870~8873): 0x%x 0x%x 0x%x 0x%x ,checksum=0x%x checksumvalue=0x%x",
		  read_cmos_sensor(0x8870),read_cmos_sensor(0x8871),read_cmos_sensor(0x8872),read_cmos_sensor(0x8873),checksum,checksumvalue);
#endif
	if(checksum == checksumvalue ) {
		printk("sc501csmainff otp data read group1 R2 success");
		return 1;		
    }

	//group1 R3
	printk("sc501csmainff otp data read group1 R3 start");
	checksum = 0;
	memset(&sc501csmainff_otp_info, 0, sizeof(struct imgsensor_otp_info_struct));
	memset(&sc501csmainff_otp_data, 0, sizeof(sc501csmainff_otp_data));

	write_cmos_sensor(0x36b0, 0x4c);
	write_cmos_sensor(0x36b1, 0x58);
	write_cmos_sensor(0x36b2, 0x01);

    checksum = sc501csmainff_custom_otp_grop1page1_read() + sc501csmainff_custom_otp_grop1page2_read();
	checksumvalue = read_cmos_sensor(0x8870) + (read_cmos_sensor(0x8871) << 8) + (read_cmos_sensor(0x8872) << 16) + (read_cmos_sensor(0x8873) << 24);
#if SC501CSMAINFF_OTP_DATA_DUMP
	printk("sc501csmainff otp group1 R3 reg(0x8870~8873): 0x%x 0x%x 0x%x 0x%x ,checksum=0x%x checksumvalue=0x%x",
		  read_cmos_sensor(0x8870),read_cmos_sensor(0x8871),read_cmos_sensor(0x8872),read_cmos_sensor(0x8873),checksum,checksumvalue);
#endif
	if(checksum == checksumvalue ) {
		printk("sc501csmainff otp data read group1 R3 success");
		return 1;		
    }

	printk("sc501csmainff otp data read group1 failed,then read from group2");

	/*read group 2*/
	//group2 R1
	printk("sc501csmainff otp data read group2 R1 start");
	checksum = 0;
	memset(&sc501csmainff_otp_info, 0, sizeof(struct imgsensor_otp_info_struct));
	memset(&sc501csmainff_otp_data, 0, sizeof(sc501csmainff_otp_data));

	write_cmos_sensor(0x36b0, 0x4c);
	write_cmos_sensor(0x36b1, 0xd8);
	write_cmos_sensor(0x36b2, 0x01);
	
	checksum = sc501csmainff_custom_otp_grop2_read();
	checksumvalue =  read_cmos_sensor(0x8FEC) + (read_cmos_sensor(0x8FED) << 8) + (read_cmos_sensor(0x8FEE) << 16) + (read_cmos_sensor(0x8FEF) << 24);
#if SC501CSMAINFF_OTP_DATA_DUMP
	printk("sc501csmainff otp group2 R1 reg(0x8FEC~8FEF): 0x%x 0x%x 0x%x 0x%x ,checksum=0x%x checksumvalue=0x%x",
		  read_cmos_sensor(0x8FEC),read_cmos_sensor(0x8FED),read_cmos_sensor(0x8FEE),read_cmos_sensor(0x8FEF),checksum,checksumvalue);
#endif
	if(checksum == checksumvalue ) {
		printk("sc501csmainff otp data read group2 R1 success");
		return 1;		
    }

	//group2 R2
	printk("sc501csmainff otp data read group2 R2 start");
	checksum = 0;
	memset(&sc501csmainff_otp_info, 0, sizeof(struct imgsensor_otp_info_struct));
	memset(&sc501csmainff_otp_data, 0, sizeof(sc501csmainff_otp_data));

	write_cmos_sensor(0x36b0, 0x4c);
	write_cmos_sensor(0x36b1, 0x98);
	write_cmos_sensor(0x36b2, 0x01);
	
	checksum = sc501csmainff_custom_otp_grop2_read();
	checksumvalue =  read_cmos_sensor(0x8FEC) + (read_cmos_sensor(0x8FED) << 8) + (read_cmos_sensor(0x8FEE) << 16) + (read_cmos_sensor(0x8FEF) << 24);
#if SC501CSMAINFF_OTP_DATA_DUMP
	printk("sc501csmainff otp group2 R2 reg(0x8FEC~8FEF): 0x%x 0x%x 0x%x 0x%x ,checksum=0x%x checksumvalue=0x%x",
		  read_cmos_sensor(0x8FEC),read_cmos_sensor(0x8FED),read_cmos_sensor(0x8FEE),read_cmos_sensor(0x8FEF),checksum,checksumvalue);
#endif
	if(checksum == checksumvalue ) {
		printk("sc501csmainff otp data read group2 R2 success");
		return 1;		
    }

	//group2 R3
	printk("sc501csmainff otp data read group2 R3 start");
	checksum = 0;
	memset(&sc501csmainff_otp_info, 0, sizeof(struct imgsensor_otp_info_struct));
	memset(&sc501csmainff_otp_data, 0, sizeof(sc501csmainff_otp_data));

	write_cmos_sensor(0x36b0, 0x4c);
	write_cmos_sensor(0x36b1, 0x58);
	write_cmos_sensor(0x36b2, 0x01);
	
	checksum = sc501csmainff_custom_otp_grop2_read();
	checksumvalue =  read_cmos_sensor(0x8FEC) + (read_cmos_sensor(0x8FED) << 8) + (read_cmos_sensor(0x8FEE) << 16) + (read_cmos_sensor(0x8FEF) << 24);
#if SC501CSMAINFF_OTP_DATA_DUMP
	printk("sc501csmainff otp group2 R3 reg(0x8FEC~8FEF): 0x%x 0x%x 0x%x 0x%x ,checksum=0x%x checksumvalue=0x%x",
		  read_cmos_sensor(0x8FEC),read_cmos_sensor(0x8FED),read_cmos_sensor(0x8FEE),read_cmos_sensor(0x8FEF),checksum,checksumvalue);
#endif
	if(checksum == checksumvalue ) {
		printk("sc501csmainff otp data read group2 R3 success");
		return 1;		
    }

	printk("sc501cs otp read otp failed !!!");
	return 0;
}
static void sc501csmainff_custom_get_otp_data(void)
{
	// get awb/lsc data
	sc501csmainff_otp_info.info_flag = 1;
	memcpy((void *)&sc501csmainff_otp_info.awb.awb_flag, (void *)&sc501csmainff_otp_data[8], SC501CSMAINFF_OTP_AWB_DATA_SIZE);
	memcpy((void *)&sc501csmainff_otp_info.lsc[0], (void *)&sc501csmainff_otp_data[0x17], SC501CSMAINFF_OTP_LSC_DATA_SIZE);


	printk("sc501csmainff otp otp date: %d-%d-%d \n",sc501csmainff_otp_info.year,sc501csmainff_otp_info.month,sc501csmainff_otp_info.day);
	printk("sc501csmainff otp otp awb unit (R/Gr/Gb/B): 0x%x 0x%x 0x%x 0x%x ,golden(R/Gr/Gb/B): 0x%x 0x%x 0x%x 0x%x \n",
		sc501csmainff_otp_info.awb.unit_r,sc501csmainff_otp_info.awb.unit_gr,
		sc501csmainff_otp_info.awb.unit_gb,sc501csmainff_otp_info.awb.unit_b,
		sc501csmainff_otp_info.awb.golden_r,sc501csmainff_otp_info.awb.golden_gr,
		sc501csmainff_otp_info.awb.golden_gb,sc501csmainff_otp_info.awb.golden_b);
}

#endif
/*prize add by zhuzhengjiang for mtk otp 20210730 end*/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	if(curr_sensor_id  != 0) {
		printk("curr sensor idx mismatch physics camera ,so return error");
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_sensor_id();
			/*prize add by zhuzhengjiang start*/
			if(*sensor_id == 0xEE45)
			{
				*sensor_id = 0xEE45+2;
			}
			/*prize add by zhuzhengjiang end*/
			if (*sensor_id == imgsensor_info.sensor_id) {
				printk("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
				/*prize add by zhuzhengjiang for mtk otp 20210730 start*/
			#if SC501CSMAINFF_OTP_FOR_MTK
				if(sc501csmainff_custom_read_otp_all_data()){
					sc501csmainff_custom_get_otp_data();
				}
				else
					printk("otp data read failed,not apply otp data");
			#endif
				/*prize add by zhuzhengjiang for mtk otp 20210730 end*/
				return ERROR_NONE;
			}
			printk("Read sensor id fail, write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;

	LOG_1;
	if(curr_sensor_id  != 0) {
		printk("curr sensor idx mismatch physics camera ,so return error");
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_sensor_id() + 2;
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail, write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
	    retry = 2;
	}

	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	//sc501csmainff_check_version();

	/* Don't Remove!! */
	//sc501csmainff_identify_otp();

	/* initail sequence write in */
	sensor_init();

	/* write registers to sram */
	//sc501csmainff_update_otp();

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 close(void)
{
	LOG_INF("E\n");
	/* No Need to implement this function */
	return ERROR_NONE;
}

static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	return ERROR_NONE;
}

static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				imgsensor.current_fps, imgsensor_info.cap.max_framerate / 10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);
	return ERROR_NONE;
}

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting();
	return ERROR_NONE;
}

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	return ERROR_NONE;
}

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	return ERROR_NONE;
}

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E\n");
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;
	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;
	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;
	sensor_resolution->SensorHighSpeedVideoWidth = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight = imgsensor_info.hs_video.grabwindow_height;
	sensor_resolution->SensorSlimVideoWidth = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight = imgsensor_info.slim_video.grabwindow_height;
	return ERROR_NONE;
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_INFO_STRUCT *sensor_info,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; /* inverse with datasheet */
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  /* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;  /* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.cap.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;
		break;
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		slim_video(image_window, sensor_config_data);
		break;
	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}

static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d\n", framerate);
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0)
	/* Dynamic frame rate */
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) /* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else /* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length =
			imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ?
			(frame_length - imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
			frame_length = imgsensor_info.cap1.pclk / framerate * 10 / imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength) ?
				(frame_length - imgsensor_info.cap1.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap1.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
				LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
					framerate, imgsensor_info.cap.max_framerate / 10);
			frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ?
				(frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ?
			(frame_length - imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ?
			(frame_length - imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	default:
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		LOG_INF("error scenario_id = %d, we use preview scenario\n", scenario_id);
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;
	default:
		break;
	}
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
	UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *)feature_para;
	UINT16 *feature_data_16 = (UINT16 *)feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *)feature_para;
	UINT32 *feature_data_32 = (UINT32 *)feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *)feature_para;

	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	/* prize add by zhuzhengjiang for SENSOR_FEATURE_GET_PIXEL_RATE 20190603 start*/
	case SENSOR_FEATURE_GET_PIXEL_RATE:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.cap.pclk /
			(imgsensor_info.cap.linelength - 80))*
			imgsensor_info.cap.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.normal_video.pclk /
			(imgsensor_info.normal_video.linelength - 80))*
			imgsensor_info.normal_video.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.hs_video.pclk /
			(imgsensor_info.hs_video.linelength - 80))*
			imgsensor_info.hs_video.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.slim_video.pclk /
			(imgsensor_info.slim_video.linelength - 80))*
			imgsensor_info.slim_video.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.pre.pclk /
			(imgsensor_info.pre.linelength - 80))*
			imgsensor_info.pre.grabwindow_width;
			break;
		}
		break;
	/* prize add by zhuzhengjiang for SENSOR_FEATURE_GET_PIXEL_RATE 20190603 end*/
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		{
			kal_uint32 rate;

			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				rate = imgsensor_info.cap.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				rate = imgsensor_info.normal_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				rate = imgsensor_info.hs_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				rate = imgsensor_info.slim_video.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				rate = imgsensor_info.pre.mipi_pixel_rate;
				break;
			}
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
		}
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
		LOG_INF("adb_i2c_read 0x%x = 0x%x\n", sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE */
		/* if EEPROM does not exist in camera module. */
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL)*feature_data_16, *(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((UINT32)*feature_data,
			(struct SET_SENSOR_PATTERN_SOLID_COLOR *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		//LOG_INF("current fps: %d\n", (UINT32)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		//LOG_INF("ihdr enable: %d\n", (BOOL)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (UINT8)*feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId: %d\n", (UINT32)*feature_data);
		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data + 1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE = %d, SE = %d, Gain = %d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data + 1), (UINT16)*(feature_data + 2));
		ihdr_write_shutter_gain((UINT16)*feature_data, (UINT16)*(feature_data + 1),
			(UINT16)*(feature_data + 2));
		break;
	default:
		break;
	}
	return ERROR_NONE;
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 SC501CSMAINFFMIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
