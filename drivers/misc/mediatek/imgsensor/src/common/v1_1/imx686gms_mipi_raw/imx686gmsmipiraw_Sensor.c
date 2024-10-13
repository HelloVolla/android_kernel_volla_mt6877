/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 IMX686GMSmipi_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

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

#include "imx686gmsmipiraw_Sensor.h"
//#include "imx616_eeprom.h"
//#define XUNHU_LPS_TEKHW_SUPPORT

//drv add by lipengpeng 20230323 start 
#include <linux/of_gpio.h>
#include <linux/gpio.h>
//drv add by lipengpeng 20230323 end
#define PFX "IMX686GMS_camera_sensor"

#define DEVICE_VERSION_IMX686GMS     "imx686gms"
#define LOG_INF(format, args...)	pr_info(PFX "[%s] " format, __func__, ##args)
static DEFINE_SPINLOCK(imgsensor_drv_lock);
//#define LRC_SQC_DBG
#ifdef LRC_SQC_DBG
#define LRC_SQC_LOG(format, args...)                                               \
	pr_info(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LRC_SQC_LOG(format, args...)   
#endif
//extern void IMX686GMS_MIPI_update_awb(kal_uint8 i2c_write_id);
//extern bool Imx586CheckVersion(kal_uint32 imgsensor_writeid);

#define Long_Exp_16s 0

#define PDAF_MODE_SUPPORT  0
#define DATA_SIZE 3024 //2304
#define BYTE      unsigned char
static BYTE imx686gms_qsc_data[DATA_SIZE] = { 0 };
//extern bool imx686gms_read_otp_qsc(BYTE* data);

#define LRC_SIZE 504 //384
static BYTE imx686gms_lrc_data[LRC_SIZE] = { 0 };
//extern bool imx686gms_read_otp_lrc(BYTE* data);
static unsigned char LRC_SQC_readed = 0;
static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = IMX686GMS_SENSOR_ID,

	//.module_id = 0x01,  /* 0x01 Sunny,0x05 QTEK */

	.checksum_value = 0x60d720a7,

	.pre = {
		/*setting for normal binning*/
		.pclk = 1110000000,
		.linelength = 10208,
		.framelength = 3624,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4624,
		.grabwindow_height = 3472,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 708000000,
		.max_framerate = 300,
	},
	.cap = {
		/*setting for normal binning*/
		.pclk = 1110000000,
		.linelength = 10208,
		.framelength = 3624,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4624,
		.grabwindow_height = 3472,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 708000000,
		.max_framerate = 300,
	},
	.normal_video = {
		/*setting for normal binning*/
		.pclk = 1110000000,
		.linelength = 10208,
		.framelength = 3624,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4624,
		.grabwindow_height = 3472,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 708000000,
		.max_framerate = 300,
	},
	.hs_video = {
		/*setting for HS 1080P binning*/ 
		.pclk = 1110000000,
		.linelength = 5488,
		.framelength = 1684,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1920,
		.grabwindow_height = 1080,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 495600000,
		.max_framerate = 1200,
	},
	.slim_video = {
		/*setting for normal binning*/
		.pclk = 1110000000,
		.linelength = 5488,
		.framelength = 1684,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 495600000,
		.max_framerate = 1200,
	},
	.custom1 = {
		/*setting for normal binning*/
		.pclk = 1110000000,
		.linelength = 10208,
		.framelength = 3624,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4624,
		.grabwindow_height = 3472,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 708000000,
		.max_framerate = 300,
	},
	.custom2 = {
		/*setting for normal binning*/
		.pclk = 1110000000,
		.linelength = 10208,
		.framelength = 3624,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4624,
		.grabwindow_height = 3472,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 708000000,
		.max_framerate = 300,
	},	
	.custom3 = {
		/*setting for 16M*/
		.pclk = 1290000000,
		.linelength = 15168,
		.framelength = 7087,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 9248,
		.grabwindow_height = 6944,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 975600000,
		.max_framerate = 120,
	},
	
	.margin = 64,
	.min_gain = 64, /*1x gain*/
	.max_gain = 4096,
	.min_gain_iso = 100,
	.exp_step = 2,
	.gain_step = 4, /*minimum step = 4 in 1x~2x gain*/
	.gain_type = 0,/*to be modify,no gain table for sony*/
	.min_shutter = 16,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	  /* 1, support; 0,not support */
	.ihdr_le_firstline = 0,  /* 1,le first ; 0, se first */
	.sensor_mode_num = 8,	  /* support sensor mode num */
	.cap_delay_frame = 2,
	.pre_delay_frame = 2,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,
	
	.custom1_delay_frame = 2,	/*32M */
	.custom2_delay_frame = 2,	/*48M@15fps*/
	.custom3_delay_frame = 2,	/*stero@34fps*/
	.frame_time_delay_frame = 3,
	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2, /* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_settle_delay_mode = 1, /* 0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL */

	//[agold][xfl][20190313][start]
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_R,//SENSOR_OUTPUT_FORMAT_RAW_R,//
	//[agold][xfl][20190313][end]

	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x20,0x34, 0xff},
	.i2c_speed = 400, /* i2c read/write speed */
};


static struct imgsensor_struct imgsensor = {
	//[agold][xfl][20190313][start]
	.mirror = IMAGE_NORMAL,				/* mirrorflip information */
	//[agold][xfl][20190313][end]
	
	/* IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview,
	*  Capture, Video,High Speed Video, Slim Video
	*/
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,					/* current shutter */
	.gain = 0x100,						/* current gain */
	.dummy_pixel = 0,					/* current dummypixel */
	.dummy_line = 0,					/* current dummyline */
	.current_fps = 0,  /* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	/* auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker */
	.autoflicker_en = KAL_FALSE,
	/* test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output */
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,/* current scenario id */
	.ihdr_mode = 0, /* sensor need support LE, SE with HDR feature */
	.hdr_mode = 0, /* HDR mODE : 0: disable HDR, 1:IHDR, 2:HDR, 9:ZHDR */
	.i2c_write_id = 0x6c,//0x20,
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[8] = {
{ 9248,  6944,  0,  0,  9248,  6944,  4624,  3472,  0000,  0000,  4624,  3472,  0,  0,  4624,  3472},/*Preview*/
{ 9248,  6944,  0,  0,  9248,  6944,  4624,  3472,  0000,  0000,  4624,  3472,  0,  0,  4624,  3472}, /* capture */
{ 9248,  6944,  0,  0,  9248,  6944,  4624,  3472,  0000,  0000,  4624,  3472,  0,  0,  4624,  3472}, /* video */
{ 9248,  6944,  0,  1312,  9248,  4320,  2312,  1080,  196,  0000,  1920,  1080,  0,  0,  1920,  1080}, /*hs video*/
{ 9248,  6944,  0,  2032,  9248,  2880,  2312,  720,  516,  0000,  1280,  720,  0,  0,  1280,  720}, /* slim video*/
{ 9248,  6944,  0,  0,  9248,  6944,  4624,  3472,  0000,  0000,  4624,  3472,  0,  0,  4624,  3472}, /*custom1  */
{ 9248,  6944,  0,  0,  9248,  6944,  4624,  3472,  0000,  0000,  4624,  3472,  0,  0,  4624,  3472}, /*custom2 */
{ 9248,  6944,  0,  0,  9248,  6944,  9248,  6944,  0000,  0000,  9248,  6944,  0,  0,  9248,  6944}, /* custom3 */

};

static struct SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[3] = {
	/* Preview mode setting */
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1210, 0x0D90, 0x00, 0x12, 0x0E10, 0x0002, /*VC0:raw, VC1:Embedded data*/
	 0x00, 0x30, 0x05A0, 0x06B8, 0x00, 0x32, 0x0E10, 0x0001}, /*VC2:Y HIST(3HDR), VC3:AE HIST(3HDR)*/
	 //0x00, 0x33, 0x0E10, 0x0001, 0x00, 0x00, 0x0000, 0x0000}, /*VC4:Flicker(3HDR), VC5:no data*/
	/* Capture mode setting */
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1210, 0x0D90, 0x00, 0x00, 0x0000, 0x0000,
	 0x00, 0x30, 0x05A0, 0x06B8, 0x00, 0x00, 0x0000, 0x0000},
	/* Video mode setting */
	{0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
	 0x00, 0x2b, 0x1210, 0x0D90, 0x00, 0x00, 0x0000, 0x0000,
	 0x00, 0x30, 0x05A0, 0x06B8, 0x00, 0x00, 0x0000, 0x0000}
};
/* If mirror flip */
static struct SET_PD_BLOCK_INFO_T  imgsensor_pd_info = {
	.i4OffsetX = 17,
	.i4OffsetY = 16,
	.i4PitchX  =  8,
	.i4PitchY  = 16,
	.i4PairNum  = 8,
	.i4SubBlkW  = 8,
	.i4SubBlkH  = 2,
	.i4PosL = { {20, 17}, {18, 19}, {22, 21}, {24, 23},
		   {20, 25}, {18, 27}, {22, 29}, {24, 31} },
	.i4PosR = { {19, 17}, {17, 19}, {21, 21}, {23, 23},
		   {19, 25}, {17, 27}, {21, 29}, {23, 31} },
	.i4BlockNumX = 574,
	.i4BlockNumY = 215,
	.iMirrorFlip = 0,
	.i4Crop = { {0, 0}, {0, 0}, {8, 440}, {0, 0}, {8, 440},
		    {0, 0}, {0, 0}, {784, 872}, {0, 0}, {0, 0} },
};


#define IMX686GMSMIPI_MaxGainIndex (223)
kal_uint16 IMX686GMSMIPI_sensorGainMapping[IMX686GMSMIPI_MaxGainIndex][2] ={
	{72,114},
	{76,162},
	{80,205},
	{84,244},
	{88,279},
	{92,312},
	{96,341},
	{100,369},
	{104,394},
	{108,417},
	{112,439},
	{116,459},
	{120,478},
	{124,495},
	{128,512},
	{132,528},
	{136,542},
	{140,556},
	{144,569},
	{148,581},
	{152,593},
	{156,604},
	{160,614},
	{164,624},
	{168,634},
	{172,643},
	{176,652},
	{180,660},
	{184,668},
	{188,675},
	{192,683},
	{196,690},
	{200,696},
	{204,703},
	{208,709},
	{212,715},
	{216,721},
	{220,726},
	{224,731},
	{228,737},
	{232,742},
	{236,746},
	{240,751},
	{244,755},
	{248,760},
	{252,764},
	{256,768},
	{260,772},
	{264,776},
	{267,779},
	{272,783},
	{277,787},
	{280,790},
	{284,793},
	{287,796},
	{293,800},
	{297,803},
	{301,806},
	{303,808},
	{308,811},
	{312,814},
	{317,817},
	{320,819},
	{324,822},
	{328,824},
	{333,827},
	{336,829},
	{340,831},
	{343,833},
	{349,836},
	{352,838},
	{356,840},
	{360,842},
	{364,844},
	{368,846},
	{372,848},
	{377,850},
	{381,852},
	{383,853},
	{388,855},
	{392,857},
	{397,859},
	{400,860},
	{405,862},
	{407,863},
	{412,865},
	{415,866},
	{420,868},
	{423,869},
	{428,871},
	{431,872},
	{437,874},
	{440,875},
	{443,876},
	{449,878},
	{452,879},
	{455,880},
	{462,882},
	{465,883},
	{468,884},
	{471,885},
	{475,886},
	{478,887},
	{485,889},
	{489,890},
	{493,891},
	{496,892},
	{500,893},
	{504,894},
	{508,895},
	{512,896},
	{516,897},
	{520,898},
	{524,899},
	{529,900},
	{533,901},
	{537,902},
	{542,903},
	{546,904},
	{551,905},
	{555,906},
	{560,907},
	{565,908},
	{570,909},
	{575,910},
	{580,911},
	{585,912},
	{590,913},
	{596,914},
	{601,915},
	{607,916},
	{612,917},
	{618,918},
	{624,919},
	{630,920},
	{636,921},
	{643,922},
	{649,923},
	{655,924},
	{662,925},
	{669,926},
	{676,927},
	{683,928},
	{690,929},
	{697,930},
	{705,931},
	{712,932},
	{720,933},
	{728,934},
	{736,935},
	{745,936},
	{753,937},
	{762,938},
	{771,939},
	{780,940},
	{790,941},
	{799,942},
	{809,943},
	{819,944},
	{830,945},
	{840,946},
	{851,947},
	{862,948},
	{874,949},
	{886,950},
	{898,951},
	{910,952},
	{923,953},
	{936,954},
	{950,955},
	{964,956},
	{978,957},
	{993,958},
	{1008,959},
	{1024,960},
	{1040,961},
	{1057,962},
	{1074,963},
	{1092,964},
	{1111,965},
	{1130,966},
	{1150,967},
	{1170,968},
	{1192,969},
	{1214,970},
	{1237,971},
	{1260,972},
	{1285,973},
	{1311,974},
	{1337,975},
	{1365,976},
	{1394,977},
	{1425,978},
	{1456,979},
	{1489,980},
	{1524,981},
	{1560,982},
	{1598,983},
	{1638,984},
	{1680,985},
	{1725,986},
	{1771,987},
	{1820,988},
	{1872,989},
	{1928,990},
	{1986,991},
	{2048,992},
	{2114,993},
	{2185,994},
	{2260,995},
	{2341,996},
	{2427,997},
	{2521,998},
	{2621,999},
	{2731,1000},
	{2849,1001},
	{2979,1002},
	{3121,1003},
	{3277,1004},
	{3449,1005},
	{3641,1006},
	{3855,1007},
	{4096,1008},
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 2, imgsensor.i2c_write_id);
	return ((get_byte<<8)&0xff00)|((get_byte>>8)&0x00ff);
}


static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
	char pusendcmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para >> 8), (char)(para & 0xFF)};

	/*kdSetI2CSpeed(imgsensor_info.i2c_speed);*/ /* Add this func to set i2c speed by each sensor */
	iWriteRegI2C(pusendcmd, 4, imgsensor.i2c_write_id);
}

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	/*kdSetI2CSpeed(imgsensor_info.i2c_speed);*/ /* Add this func to set i2c speed by each sensor */
	iReadRegI2C(pusendcmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}

static bool selective_read_eeprom(kal_uint16 addr, BYTE *data)
{
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };
	if (iReadRegI2C(pusendcmd, 2, (u8 *) data,
			1, 0xA0) < 0) {
		return false;
	}
	return true;
}
static bool read_imx686gms_eeprom(kal_uint16 addr, BYTE *data, int size)
{
	int i = 0;
	int offset = addr;
	for (i = 0; i < size; i++) {
		if (!selective_read_eeprom(offset, &data[i]))
			return false;
		offset++;
	}
	return true;
}
unsigned int read_imx686gms_LRC( BYTE *data )
{
	read_imx686gms_eeprom(0x0D51,data,504);
	return 504;
}
unsigned int read_imx686gms_SQC( BYTE *data )
{
	read_imx686gms_eeprom(0x0F4B,data,3024);
	return 3024;
}
static void imx686gms_get_pdaf_reg_setting(MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		regDa[idx + 1] = read_cmos_sensor_8(regDa[idx]);
		pr_debug("%x %x", regDa[idx], regDa[idx+1]);
	}
}
static void imx686gms_set_pdaf_reg_setting(MUINT32 regNum, kal_uint16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		write_cmos_sensor_8(regDa[idx], regDa[idx + 1]);
		pr_debug("%x %x", regDa[idx], regDa[idx+1]);
	}
}

static void set_dummy(void)
{
	LOG_INF("frame_length = %d, line_length = %d\n",
		imgsensor.frame_length,
		imgsensor.line_length);

	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
//	write_cmos_sensor_8(0x0342, imgsensor.line_length >> 8);
//	write_cmos_sensor_8(0x0343, imgsensor.line_length & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);
}	/*	set_dummy  */

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{

	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INF("framerate = %d, min framelength should enable %d\n", framerate, min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	if (frame_length >= imgsensor.min_frame_length)
		imgsensor.frame_length = frame_length;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */

static void write_shutter(kal_uint16 shutter)
{
	kal_uint16 realtime_fps = 0;


	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);

	if (shutter < imgsensor_info.min_shutter)
		shutter = imgsensor_info.min_shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
		/* Extend frame length */
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);
	}
	} else {
		/* Extend frame length */
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);
	}

	/* Update Shutter */
	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor_8(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor_8(0x0203, shutter  & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);
	LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);

}	/*	write_shutter  */



/*************************************************************************
* FUNCTION
*	set_shutter
*
* DESCRIPTION
*	This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*	iShutter : exposured lines
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	write_shutter(shutter);
}	/*	set_shutter */

static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint8 i;

	for (i = 0; i < IMX686GMSMIPI_MaxGainIndex; i++) {
		if(gain <= IMX686GMSMIPI_sensorGainMapping[i][0]){
			break;
		}
	}
	if(gain != IMX686GMSMIPI_sensorGainMapping[i][0])
		LOG_INF("Gain mapping don't correctly:%d %d \n", gain, IMX686GMSMIPI_sensorGainMapping[i][0]);
	return IMX686GMSMIPI_sensorGainMapping[i][1];
}


/*************************************************************************
* FUNCTION
*	set_gain
*
* DESCRIPTION
*	This function is to set global gain to sensor.
*
* PARAMETERS
*	iGain : sensor global gain(base: 0x40)
*
* RETURNS
*	the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

    /* gain=1024;//for test */
    /* return; //for test */

	//if (gain < BASEGAIN || gain > 16 * BASEGAIN) {
	//if (gain < 72 || gain > 64 * BASEGAIN) {
	if (gain < imgsensor_info.min_gain || gain > imgsensor_info.max_gain) {
		LOG_INF("Error gain setting");

	if (gain < imgsensor_info.min_gain)
		gain = imgsensor_info.min_gain;
	else if (gain > imgsensor_info.max_gain)
		gain = imgsensor_info.max_gain;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

	write_cmos_sensor_8(0x0104, 0x01);
	write_cmos_sensor_8(0x0204, (reg_gain>>8) & 0xFF);
	write_cmos_sensor_8(0x0205, reg_gain & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);

	return gain;
}	/*	set_gain  */

/* ITD: Modify Dualcam By Jesse 190924 Start */
static void set_shutter_frame_length(kal_uint16 shutter, kal_uint16 target_frame_length)
{

	spin_lock(&imgsensor_drv_lock);
	if (target_frame_length > 1)
		imgsensor.dummy_line = target_frame_length - imgsensor.frame_length;
	imgsensor.frame_length = imgsensor.frame_length + imgsensor.dummy_line;
	imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_shutter(shutter);
}
/* ITD: Modify Dualcam By Jesse 190924 End */
//static void ihdr_write_shutter_gain(kal_uint16 le,
//				kal_uint16 se, kal_uint16 gain)
//{
//}
#if 0
static void set_mirror_flip(kal_uint8 image_mirror)
{
	kal_uint8 itemp;

	LOG_INF("image_mirror = %d\n", image_mirror);
	itemp = read_cmos_sensor_8(0x0101);
	itemp &= ~0x03;

	switch (image_mirror) {

	case IMAGE_NORMAL:
	write_cmos_sensor_8(0x0101, itemp | 0x00);
	break;

	case IMAGE_V_MIRROR:
	write_cmos_sensor_8(0x0101, itemp | 0x02);
	break;

	case IMAGE_H_MIRROR:
	write_cmos_sensor_8(0x0101, itemp | 0x01);
	break;

	case IMAGE_HV_MIRROR:
	write_cmos_sensor_8(0x0101, itemp | 0x03);
	break;
	}
}
#endif
//#endif
/*************************************************************************
* FUNCTION
*	night_mode
*
* DESCRIPTION
*	This function night mode of sensor.
*
* PARAMETERS
*	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
#if 0
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}	/*	night_mode	*/
#endif

#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 765	/* trans# max is 255, each 3 bytes */
#else
#define I2C_BUFFER_LEN 3

#endif

static kal_uint16 imx686gms_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;

	tosend = 0;
	IDX = 0;

	while (len > IDX) {
		addr = para[IDX];

		{
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;

		}
#if MULTI_WRITE
		/* Write when remain buffer size is less than 3 bytes or reach end of data */
		if ((I2C_BUFFER_LEN - tosend) < 3 || IDX == len || addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd,
								tosend,
								imgsensor.i2c_write_id,
								3,
								imgsensor_info.i2c_speed);
			tosend = 0;
		}
#else
		iWriteRegI2C(puSendCmd, 3, imgsensor.i2c_write_id);
		tosend = 0;

#endif
	}
	return 0;
}

static void write_imx686gms_LRC_Data(void)
{
	kal_uint16 i;	

	for (i = 0; i < 252; i++) {
		write_cmos_sensor_8(0x7B00 + i, imx686gms_lrc_data[i]);
		LRC_SQC_LOG("imx686gms_lrc_data[%d] = 0x%x\n", i, imx686gms_lrc_data[i]);
	}
	
	for (i = 252; i < LRC_SIZE; i++) {
		write_cmos_sensor_8(0x7C00 + i, imx686gms_lrc_data[i]);
		LRC_SQC_LOG("imx686gms_lrc_data[%d] = 0x%x\n", i, imx686gms_lrc_data[i]);
	}
}

static void write_imx686gms_QSC_Data(void)
{
	kal_uint16 i;	

	for (i = 0; i < DATA_SIZE; i++) {
		write_cmos_sensor_8(0xCA00 + i, imx686gms_qsc_data[i]);
		LRC_SQC_LOG("imx686gms_qsc_data[%d] = 0x%x\n", i, imx686gms_qsc_data[i]);
	}
}

kal_uint16 addr_data_pair_init_imx686gms_Ver6[] = {
	0x0136, 0x18,
	0x0137, 0x00,
	0x33F0, 0x02,
	0x33F1, 0x06,
	0x0111, 0x02,
	0x3062, 0x00,
	0x3063, 0x30,
	0x3076, 0x00,
	0x3077, 0x30,
	0x31C0, 0x01,
	0x328B, 0x29,
	0x32D9, 0x01,
	0x33BC, 0x00,
	0x4008, 0x10,
	0x4009, 0x10,
	0x400A, 0x10,
	0x400B, 0x10,
	0x400C, 0x10,
	0x400F, 0x01,
	0x4011, 0x01,
	0x4013, 0x01,
	0x4015, 0x01,
	0x4017, 0x40,
	0x4328, 0x00,
	0x4329, 0xB4,
	0x4E08, 0x4B,
	0x4E21, 0x35,
	0x4E25, 0x10,
	0x4E2F, 0x25,
	0x4E3B, 0xB5,
	0x4E49, 0x21,
	0x4E57, 0x3F,
	0x4E63, 0xAB,
	0x4E6B, 0x44,
	0x4E6F, 0x19,
	0x4E71, 0x62,
	0x4E73, 0x5D,
	0x4E75, 0xAB,
	0x4E87, 0x2B,
	0x4E8B, 0x10,
	0x4E91, 0xAF,
	0x4E95, 0x4E,
	0x4EA1, 0xF1,
	0x4EAB, 0x4C,
	0x4EBF, 0x4E,
	0x4EC3, 0x19,
	0x4EC5, 0x71,
	0x4EC7, 0x5D,
	0x4EC9, 0xF1,
	0x4ECB, 0x6F,
	0x4ECD, 0x5D,
	0x4EDF, 0x2B,
	0x4EE3, 0x0E,
	0x4EED, 0x27,
	0x4EF9, 0xAB,
	0x4F01, 0x4E,
	0x4F05, 0x19,
	0x4F07, 0x4A,
	0x4F09, 0x5D,
	0x4F0B, 0xAB,
	0x4F19, 0x83,
	0x4F1D, 0x3C,
	0x4F26, 0x01,
	0x4F27, 0x07,
	0x4F32, 0x04,
	0x4F33, 0x11,
	0x4F3C, 0x4B,
	0x4F59, 0x2D,
	0x4F5D, 0x5A,
	0x4F63, 0x46,
	0x4F69, 0x9E,
	0x4F6E, 0x03,
	0x4F6F, 0x23,
	0x4F81, 0x27,
	0x4F85, 0x5A,
	0x4F8B, 0x62,
	0x4F91, 0x9E,
	0x4F96, 0x03,
	0x4F97, 0x39,
	0x4F9F, 0x41,
	0x4FA3, 0x19,
	0x4FA5, 0xA3,
	0x4FA7, 0x5D,
	0x4FA8, 0x03,
	0x4FA9, 0x39,
	0x4FBF, 0x4A,
	0x4FC3, 0x5A,
	0x4FC5, 0xE5,
	0x4FC9, 0x83,
	0x4FCF, 0x9E,
	0x4FD5, 0xD0,
	0x4FE5, 0x9E,
	0x4FE9, 0xE5,
	0x4FF3, 0x41,
	0x4FF7, 0x19,
	0x4FF9, 0x98,
	0x4FFB, 0x5D,
	0x4FFD, 0xD0,
	0x4FFF, 0xA5,
	0x5001, 0x5D,
	0x5003, 0xE5,
	0x5017, 0x07,
	0x5021, 0x36,
	0x5035, 0x2D,
	0x5039, 0x19,
	0x503B, 0x63,
	0x503D, 0x5D,
	0x5051, 0x42,
	0x5055, 0x5A,
	0x505B, 0xC7,
	0x5061, 0x9E,
	0x5067, 0xD5,
	0x5079, 0x15,
	0x5083, 0x20,
	0x509D, 0x5A,
	0x509F, 0x5A,
	0x50A1, 0x5A,
	0x50A5, 0x5A,
	0x50B5, 0x9E,
	0x50B7, 0x9E,
	0x50B9, 0x9E,
	0x50BD, 0x9E,
	0x50C7, 0x9E,
	0x544A, 0xE0,
	0x544D, 0xE2,
	0x551C, 0x03,
	0x551F, 0x64,
	0x5521, 0xD2,
	0x5523, 0x64,
	0x5549, 0x5A,
	0x554B, 0x9E,
	0x554D, 0x5A,
	0x554F, 0x9E,
	0x5551, 0x5A,
	0x5553, 0x9E,
	0x5559, 0x5A,
	0x555B, 0x9E,
	0x5561, 0x9E,
	0x55CD, 0x5A,
	0x55CF, 0x9E,
	0x55D1, 0x5A,
	0x55D3, 0x9E,
	0x55D5, 0x5A,
	0x55D7, 0x9E,
	0x55DD, 0x5A,
	0x55DF, 0x9E,
	0x55E7, 0x9E,
	0x571A, 0x00,
	0x581B, 0x46,
	0x5839, 0x8A,
	0x5852, 0x00,
	0x59C7, 0x10,
	0x59CB, 0x40,
	0x59D1, 0x01,
	0x59EB, 0x00,
	0x5A27, 0x01,
	0x5A46, 0x09,
	0x5A47, 0x09,
	0x5A48, 0x09,
	0x5A49, 0x13,
	0x5A50, 0x0D,
	0x5A51, 0x0D,
	0x5A52, 0x0D,
	0x5A53, 0x0D,
	0x5A54, 0x03,
	0x5B0A, 0x04,
	0x5B0B, 0x04,
	0x5B0C, 0x04,
	0x5B0D, 0x04,
	0x5B0E, 0x04,
	0x5B0F, 0x04,
	0x5B10, 0x04,
	0x5B11, 0x04,
	0x5B12, 0x04,
	0x5B13, 0x04,
	0x5B1A, 0x08,
	0x5B1E, 0x04,
	0x5B1F, 0x04,
	0x5B20, 0x04,
	0x5B21, 0x04,
	0x5B22, 0x08,
	0x5B23, 0x08,
	0x5B24, 0x04,
	0x5B25, 0x08,
	0x5B26, 0x04,
	0x5B27, 0x08,
	0x5B32, 0x04,
	0x5B33, 0x04,
	0x5B34, 0x04,
	0x5B35, 0x04,
	0x5B38, 0x04,
	0x5B3A, 0x04,
	0x5B3E, 0x10,
	0x5B40, 0x10,
	0x5B46, 0x08,
	0x5B47, 0x04,
	0x5B48, 0x04,
	0x5B49, 0x08,
	0x5B4C, 0x08,
	0x5B4E, 0x08,
	0x5B52, 0x1F,
	0x5B53, 0x1F,
	0x5B57, 0x04,
	0x5B58, 0x04,
	0x5B5E, 0x1F,
	0x5B5F, 0x1F,
	0x5B63, 0x08,
	0x5B64, 0x08,
	0x5B68, 0x1F,
	0x5B69, 0x1F,
	0x5B6C, 0x1F,
	0x5B6D, 0x1F,
	0x5B72, 0x06,
	0x5B76, 0x07,
	0x5B7E, 0x10,
	0x5B7F, 0x10,
	0x5B81, 0x10,
	0x5B83, 0x10,
	0x5B86, 0x07,
	0x5B88, 0x07,
	0x5B8A, 0x07,
	0x5B98, 0x08,
	0x5B99, 0x08,
	0x5B9A, 0x09,
	0x5B9B, 0x08,
	0x5B9C, 0x07,
	0x5B9D, 0x08,
	0x5B9F, 0x10,
	0x5BA2, 0x10,
	0x5BA5, 0x10,
	0x5BA8, 0x10,
	0x5BAA, 0x10,
	0x5BAC, 0x0C,
	0x5BAD, 0x0C,
	0x5BAE, 0x0A,
	0x5BAF, 0x0C,
	0x5BB0, 0x07,
	0x5BB1, 0x0C,
	0x5BC0, 0x11,
	0x5BC1, 0x10,
	0x5BC4, 0x10,
	0x5BC5, 0x10,
	0x5BC7, 0x10,
	0x5BC8, 0x10,
	0x5BCC, 0x0B,
	0x5BCD, 0x0C,
	0x5BE5, 0x03,
	0x5BE6, 0x03,
	0x5BE7, 0x03,
	0x5BE8, 0x03,
	0x5BE9, 0x03,
	0x5BEA, 0x03,
	0x5BEB, 0x03,
	0x5BEC, 0x03,
	0x5BED, 0x03,
	0x5BF3, 0x03,
	0x5BF4, 0x03,
	0x5BF5, 0x03,
	0x5BF6, 0x03,
	0x5BF7, 0x03,
	0x5BF8, 0x03,
	0x5BF9, 0x03,
	0x5BFA, 0x03,
	0x5BFB, 0x03,
	0x5C01, 0x03,
	0x5C02, 0x03,
	0x5C03, 0x03,
	0x5C04, 0x03,
	0x5C05, 0x03,
	0x5C06, 0x03,
	0x5C07, 0x03,
	0x5C08, 0x03,
	0x5C09, 0x03,
	0x5C0F, 0x03,
	0x5C10, 0x03,
	0x5C11, 0x03,
	0x5C12, 0x03,
	0x5C13, 0x03,
	0x5C14, 0x03,
	0x5C15, 0x03,
	0x5C16, 0x03,
	0x5C17, 0x03,
	0x5C1A, 0x03,
	0x5C1B, 0x03,
	0x5C1C, 0x03,
	0x5C1D, 0x03,
	0x5C1E, 0x03,
	0x5C1F, 0x03,
	0x5C20, 0x03,
	0x5C21, 0x03,
	0x5C22, 0x03,
	0x5C25, 0x03,
	0x5C26, 0x03,
	0x5C27, 0x03,
	0x5C28, 0x03,
	0x5C29, 0x03,
	0x5C2A, 0x03,
	0x5C2B, 0x03,
	0x5C2C, 0x03,
	0x5C2D, 0x03,
	0x5C2E, 0x03,
	0x5C2F, 0x03,
	0x5C30, 0x03,
	0x5C31, 0x03,
	0x5C32, 0x03,
	0x5C33, 0x03,
	0x5C34, 0x03,
	0x5C35, 0x03,
	0x5C46, 0x62,
	0x5C4D, 0x6C,
	0x5C53, 0x62,
	0x5C58, 0x62,
	0x5EDD, 0x05,
	0x5EDE, 0x05,
	0x5EDF, 0x05,
	0x5EE3, 0x05,
	0x5EEA, 0x05,
	0x5EEB, 0x05,
	0x5EEC, 0x05,
	0x5EF0, 0x05,
	0x5EF7, 0x05,
	0x5EF8, 0x05,
	0x5EF9, 0x05,
	0x5EFD, 0x05,
	0x5F04, 0x05,
	0x5F05, 0x05,
	0x5F06, 0x05,
	0x5F0A, 0x05,
	0x5F0E, 0x05,
	0x5F0F, 0x05,
	0x5F10, 0x05,
	0x5F14, 0x05,
	0x5F18, 0x05,
	0x5F19, 0x05,
	0x5F1A, 0x05,
	0x5F1E, 0x05,
	0x5F20, 0x05,
	0x5F24, 0x05,
	0x5F36, 0x1E,
	0x5F38, 0x1E,
	0x5F3A, 0x1E,
	0x6081, 0x10,
	0x6082, 0x10,
	0x6085, 0x10,
	0x6088, 0x10,
	0x608B, 0x10,
	0x608D, 0x10,
	0x6095, 0x0C,
	0x6096, 0x0C,
	0x6099, 0x0C,
	0x609C, 0x0C,
	0x609D, 0x04,
	0x609E, 0x04,
	0x609F, 0x0C,
	0x60A1, 0x0C,
	0x60A2, 0x04,
	0x60A9, 0x0C,
	0x60AA, 0x0C,
	0x60AB, 0x10,
	0x60AC, 0x10,
	0x60AD, 0x0C,
	0x60AE, 0x10,
	0x60AF, 0x10,
	0x60B0, 0x0C,
	0x60B1, 0x04,
	0x60B2, 0x04,
	0x60B3, 0x0C,
	0x60B5, 0x0C,
	0x60B6, 0x04,
	0x60B9, 0x04,
	0x60BA, 0x04,
	0x60BB, 0x0C,
	0x60BC, 0x0C,
	0x60BE, 0x0C,
	0x60BF, 0x0C,
	0x60C0, 0x04,
	0x60C1, 0x04,
	0x60C5, 0x04,
	0x60C6, 0x04,
	0x60C7, 0x0C,
	0x60C8, 0x0C,
	0x60CA, 0x0C,
	0x60CB, 0x0C,
	0x60CC, 0x04,
	0x60CD, 0x04,
	0x60CF, 0x04,
	0x60D0, 0x04,
	0x60D3, 0x04,
	0x60D4, 0x04,
	0x60DD, 0x19,
	0x60E1, 0x19,
	0x60E9, 0x19,
	0x60EB, 0x19,
	0x60EF, 0x19,
	0x60F1, 0x19,
	0x60F9, 0x19,
	0x60FD, 0x19,
	0x610D, 0x2D,
	0x610F, 0x2D,
	0x6115, 0x2D,
	0x611B, 0x2D,
	0x6121, 0x2D,
	0x6125, 0x2D,
	0x6135, 0x3C,
	0x6137, 0x3C,
	0x613D, 0x3C,
	0x6143, 0x3C,
	0x6145, 0x5A,
	0x6147, 0x5A,
	0x6149, 0x3C,
	0x614D, 0x3C,
	0x614F, 0x5A,
	0x615D, 0x3C,
	0x615F, 0x3C,
	0x6161, 0x2D,
	0x6163, 0x2D,
	0x6165, 0x3C,
	0x6167, 0x2D,
	0x6169, 0x2D,
	0x616B, 0x3C,
	0x616D, 0x5A,
	0x616F, 0x5A,
	0x6171, 0x3C,
	0x6175, 0x3C,
	0x6177, 0x5A,
	0x617D, 0x5A,
	0x617F, 0x5A,
	0x6181, 0x3C,
	0x6183, 0x3C,
	0x6187, 0x3C,
	0x6189, 0x3C,
	0x618B, 0x5A,
	0x618D, 0x5A,
	0x6195, 0x5A,
	0x6197, 0x5A,
	0x6199, 0x3C,
	0x619B, 0x3C,
	0x619F, 0x3C,
	0x61A1, 0x3C,
	0x61A3, 0x5A,
	0x61A5, 0x5A,
	0x61A9, 0x5A,
	0x61AB, 0x5A,
	0x61B1, 0x5A,
	0x61B3, 0x5A,
	0x61BD, 0x5D,
	0x61C1, 0x5D,
	0x61C9, 0x5D,
	0x61CB, 0x5D,
	0x61CF, 0x5D,
	0x61D1, 0x5D,
	0x61D9, 0x5D,
	0x61DD, 0x5D,
	0x61ED, 0x71,
	0x61EF, 0x71,
	0x61F5, 0x71,
	0x61FB, 0x71,
	0x6201, 0x71,
	0x6205, 0x71,
	0x6215, 0x80,
	0x6217, 0x80,
	0x621D, 0x80,
	0x6223, 0x80,
	0x6225, 0x9E,
	0x6227, 0x9E,
	0x6229, 0x80,
	0x622D, 0x80,
	0x622F, 0x9E,
	0x623D, 0x80,
	0x623F, 0x80,
	0x6241, 0x71,
	0x6243, 0x71,
	0x6245, 0x80,
	0x6247, 0x71,
	0x6249, 0x71,
	0x624B, 0x80,
	0x624D, 0x9E,
	0x624F, 0x9E,
	0x6251, 0x80,
	0x6255, 0x80,
	0x6257, 0x9E,
	0x625D, 0x9E,
	0x625F, 0x9E,
	0x6261, 0x80,
	0x6263, 0x80,
	0x6267, 0x80,
	0x6269, 0x80,
	0x626B, 0x9E,
	0x626D, 0x9E,
	0x6275, 0x9E,
	0x6277, 0x9E,
	0x6279, 0x80,
	0x627B, 0x80,
	0x627F, 0x80,
	0x6281, 0x80,
	0x6283, 0x9E,
	0x6285, 0x9E,
	0x6289, 0x9E,
	0x628B, 0x9E,
	0x6291, 0x9E,
	0x6293, 0x9E,
	0x629B, 0x5D,
	0x629F, 0x5D,
	0x62A1, 0x5D,
	0x62A5, 0x5D,
	0x62BF, 0x9E,
	0x62CD, 0x9E,
	0x62D3, 0x9E,
	0x62D9, 0x9E,
	0x62DD, 0x9E,
	0x62E3, 0x9E,
	0x62E5, 0x9E,
	0x62E7, 0x9E,
	0x62E9, 0x9E,
	0x62EB, 0x9E,
	0x62F3, 0x28,
	0x630E, 0x28,
	0x6481, 0x0D,
	0x648A, 0x0D,
	0x648B, 0x0D,
	0x64A3, 0x0B,
	0x64A4, 0x0B,
	0x64A5, 0x0B,
	0x64A9, 0x0B,
	0x64AF, 0x0B,
	0x64B0, 0x0B,
	0x64B1, 0x0B,
	0x64B5, 0x0B,
	0x64BB, 0x0B,
	0x64BC, 0x0B,
	0x64BD, 0x0B,
	0x64C1, 0x0B,
	0x64C7, 0x0B,
	0x64C8, 0x0B,
	0x64C9, 0x0B,
	0x64CD, 0x0B,
	0x64D0, 0x0B,
	0x64D1, 0x0B,
	0x64D2, 0x0B,
	0x64D6, 0x0B,
	0x64D9, 0x0B,
	0x64DA, 0x0B,
	0x64DB, 0x0B,
	0x64DF, 0x0B,
	0x64E0, 0x0B,
	0x64E4, 0x0B,
	0x64ED, 0x05,
	0x64EE, 0x05,
	0x64EF, 0x05,
	0x64F3, 0x05,
	0x64F9, 0x05,
	0x64FA, 0x05,
	0x64FB, 0x05,
	0x64FF, 0x05,
	0x6505, 0x05,
	0x6506, 0x05,
	0x6507, 0x05,
	0x650B, 0x05,
	0x6511, 0x05,
	0x6512, 0x05,
	0x6513, 0x05,
	0x6517, 0x05,
	0x651A, 0x05,
	0x651B, 0x05,
	0x651C, 0x05,
	0x6520, 0x05,
	0x6523, 0x05,
	0x6524, 0x05,
	0x6525, 0x05,
	0x6529, 0x05,
	0x652A, 0x05,
	0x652E, 0x05,
	0x7314, 0x02,
	0x7315, 0x40,
	0x7600, 0x03,
	0x7630, 0x04,
	0x8744, 0x00,
	0x9003, 0x08,
	0x9004, 0x18,
	0x9210, 0xEA,
	0x9211, 0x7A,
	0x9212, 0xEA,
	0x9213, 0x7D,
	0x9214, 0xEA,
	0x9215, 0x80,
	0x9216, 0xEA,
	0x9217, 0x83,
	0x9218, 0xEA,
	0x9219, 0x86,
	0x921A, 0xEA,
	0x921B, 0xB8,
	0x921C, 0xEA,
	0x921D, 0xB9,
	0x921E, 0xEA,
	0x921F, 0xBE,
	0x9220, 0xEA,
	0x9221, 0xBF,
	0x9222, 0xEA,
	0x9223, 0xC4,
	0x9224, 0xEA,
	0x9225, 0xC5,
	0x9226, 0xEA,
	0x9227, 0xCA,
	0x9228, 0xEA,
	0x9229, 0xCB,
	0x922A, 0xEA,
	0x922B, 0xD0,
	0x922C, 0xEA,
	0x922D, 0xD1,
	0x922E, 0x91,
	0x922F, 0x2A,
	0x9230, 0xE2,
	0x9231, 0xC0,
	0x9232, 0xE2,
	0x9233, 0xC1,
	0x9234, 0xE2,
	0x9235, 0xC2,
	0x9236, 0xE2,
	0x9237, 0xC3,
	0x9238, 0xE2,
	0x9239, 0xD4,
	0x923A, 0xE2,
	0x923B, 0xD5,
	0xB0BE, 0x04,
	0xC5C6, 0x01,
	0xC5D8, 0x3F,
	0xC5DA, 0x35,
	0xE70E, 0x06,
	0xE70F, 0x0C,
	0xE710, 0x00,
	0xE711, 0x00,
	0xE712, 0x00,
	0xE713, 0x00,
	0x3547, 0x00,
	0x3549, 0x00,
	0x354B, 0x00,
	0x354D, 0x00,
	0x85B1, 0x01,
	0x9865, 0xA0,
	0x9866, 0x14,
	0x9867, 0x0A,
	0x98DA, 0xA0,
	0x98DB, 0x78,
	0x98DC, 0x50,
	0x99B8, 0x17,
	0x99BA, 0x17,
	0x9A12, 0x15,
	0x9A13, 0x15,
	0x9A14, 0x15,
	0x9A15, 0x0B,
	0x9A16, 0x0B,
	0x9A49, 0x0B,
	0x9A4A, 0x0B,
	0xA539, 0x03,
	0xA53A, 0x03,
	0xA53B, 0x03,
	0xA575, 0x03,
	0xA576, 0x03,
	0xA577, 0x03,
	0xA57D, 0x80,
	0xA660, 0x01,
	0xA661, 0x69,
	0xA66C, 0x01,
	0xA66D, 0x27,
	0xA673, 0x40,
	0xA675, 0x40,
	0xA677, 0x43,
	0xA67D, 0x06,
	0xA6DE, 0x01,
	0xA6DF, 0x69,
	0xA6EA, 0x01,
	0xA6EB, 0x27,
	0xA6F1, 0x40,
	0xA6F3, 0x40,
	0xA6F5, 0x43,
	0xA6FB, 0x06,
	0xA773, 0x40,
	0xA775, 0x40,
	0xA777, 0x43,
	0xAA37, 0x76,
	0xAA39, 0xAC,
	0xAA3B, 0xC8,
	0xAA3D, 0x76,
	0xAA3F, 0xAC,
	0xAA41, 0xC8,
	0xAA43, 0x76,
	0xAA45, 0xAC,
	0xAA47, 0xC8,
	0xAD1C, 0x01,
	0xAD1D, 0x3D,
	0xAD23, 0x4F,
	0xAD4C, 0x01,
	0xAD4D, 0x3D,
	0xAD53, 0x4F,
	0xAD7C, 0x01,
	0xAD7D, 0x3D,
	0xAD83, 0x4F,
	0xADAC, 0x01,
	0xADAD, 0x3D,
	0xADB3, 0x4F,
	0xAE00, 0x01,
	0xAE01, 0xA9,
	0xAE02, 0x01,
	0xAE03, 0xA9,
	0xAE05, 0x86,
	0xAE0D, 0x10,
	0xAE0F, 0x10,
	0xAE11, 0x10,
	0xAE24, 0x03,
	0xAE25, 0x03,
	0xAE26, 0x02,
	0xAE27, 0x49,
	0xAE28, 0x01,
	0xAE29, 0x3B,
	0xAE31, 0x10,
	0xAE33, 0x10,
	0xAE35, 0x10,
	0xAE48, 0x02,
	0xAE4A, 0x01,
	0xAE4B, 0x80,
	0xAE4D, 0x80,
	0xAE55, 0x10,
	0xAE57, 0x10,
	0xAE59, 0x10,
	0xAE6C, 0x01,
	0xAE6D, 0xC1,
	0xAE6F, 0xA5,
	0xAE79, 0x10,
	0xAE7B, 0x10,
	0xAE7D, 0x13,
	0xAE90, 0x04,
	0xAE91, 0xB0,
	0xAE92, 0x01,
	0xAE93, 0x70,
	0xAE94, 0x01,
	0xAE95, 0x3B,
	0xAE9D, 0x10,
	0xAE9F, 0x10,
	0xAEA1, 0x10,
	0xAEB4, 0x02,
	0xAEB5, 0xCB,
	0xAEB6, 0x01,
	0xAEB7, 0x58,
	0xAEB9, 0xB4,
	0xAEC1, 0x10,
	0xAEC3, 0x10,
	0xAEC5, 0x10,
	0xAF01, 0x13,
	0xAF02, 0x00,
	0xAF08, 0x78,
	0xAF09, 0x6E,
	0xAF0A, 0x64,
	0xAF0B, 0x5A,
	0xAF0C, 0x50,
	0xAF0D, 0x46,
	0xAF0E, 0x3C,
	0xAF0F, 0x32,
	0xAF10, 0x28,
	0xAF11, 0x00,
	0xAF17, 0x50,
	0xAF18, 0x3C,
	0xAF19, 0x28,
	0xAF1A, 0x14,
	0xAF1B, 0x00,
	0xAF26, 0xA0,
	0xAF27, 0x96,
	0xAF28, 0x8C,
	0xAF29, 0x82,
	0xAF2A, 0x78,
	0xAF2B, 0x6E,
	0xAF2C, 0x64,
	0xAF2D, 0x5A,
	0xAF2E, 0x50,
	0xAF2F, 0x00,
	0xAF31, 0x96,
	0xAF32, 0x8C,
	0xAF33, 0x82,
	0xAF34, 0x78,
	0xAF35, 0x6E,
	0xAF36, 0x64,
	0xAF38, 0x3C,
	0xAF39, 0x00,
	0xAF3A, 0xA0,
	0xAF3B, 0x96,
	0xAF3C, 0x8C,
	0xAF3D, 0x82,
	0xAF3E, 0x78,
	0xAF3F, 0x6E,
	0xAF40, 0x64,
	0xAF41, 0x50,
	0xAF94, 0x03,
	0xAF95, 0x02,
	0xAF96, 0x02,
	0xAF99, 0x01,
	0xAF9B, 0x02,
	0xAFA5, 0x01,
	0xAFA7, 0x03,
	0xAFB4, 0x02,
	0xAFB5, 0x02,
	0xAFB6, 0x03,
	0xAFB7, 0x03,
	0xAFB8, 0x03,
	0xAFB9, 0x04,
	0xAFBA, 0x04,
	0xAFBC, 0x03,
	0xAFBD, 0x03,
	0xAFBE, 0x02,
	0xAFBF, 0x02,
	0xAFC0, 0x02,
	0xAFC3, 0x01,
	0xAFC5, 0x03,
	0xAFC6, 0x04,
	0xAFC7, 0x04,
	0xAFC8, 0x03,
	0xAFC9, 0x03,
	0xAFCA, 0x02,
	0xAFCC, 0x01,
	0xAFCE, 0x02,
	0xB02A, 0x00,
	0xB02E, 0x02,
	0xB030, 0x02,
	0xB501, 0x02,
	0xB503, 0x02,
	0xB505, 0x02,
	0xB507, 0x02,
	0xB515, 0x00,
	0xB517, 0x00,
	0xB519, 0x02,
	0xB51F, 0x00,
	0xB521, 0x01,
	0xB527, 0x02,
	0xB53D, 0x01,
	0xB53F, 0x02,
	0xB541, 0x02,
	0xB543, 0x02,
	0xB545, 0x02,
	0xB547, 0x02,
	0xB54B, 0x03,
	0xB54D, 0x03,
	0xB551, 0x02,
	0xB553, 0x02,
	0xB555, 0x02,
	0xB557, 0x02,
	0xB559, 0x02,
	0xB55B, 0x02,
	0xB55D, 0x01,
	0xB563, 0x02,
	0xB565, 0x03,
	0xB567, 0x03,
	0xB569, 0x02,
	0xB56B, 0x02,
	0xB58D, 0xE7,
	0xB58F, 0xCC,
	0xB591, 0xAD,
	0xB593, 0x88,
	0xB595, 0x66,
	0xB597, 0x88,
	0xB599, 0xAD,
	0xB59B, 0xCC,
	0xB59D, 0xE7,
	0xB5A1, 0x2A,
	0xB5A3, 0x1A,
	0xB5A5, 0x27,
	0xB5A7, 0x1A,
	0xB5A9, 0x2A,
	0xB5AB, 0x3C,
	0xB5AD, 0x59,
	0xB5AF, 0x77,
	0xB5B1, 0x9A,
	0xB5B3, 0xE9,
	0xB5C9, 0x5B,
	0xB5CB, 0x73,
	0xB5CD, 0x9D,
	0xB5CF, 0xBA,
	0xB5D1, 0xD9,
	0xB5D3, 0xED,
	0xB5D5, 0xF9,
	0xB5D7, 0xFE,
	0xB5D8, 0x01,
	0xB5D9, 0x00,
	0xB5DA, 0x01,
	0xB5DB, 0x00,
	0xB5DD, 0xF6,
	0xB5DF, 0xE9,
	0xB5E1, 0xD1,
	0xB5E3, 0xBB,
	0xB5E5, 0x9A,
	0xB5E7, 0x77,
	0xB5E9, 0x59,
	0xB5EB, 0x77,
	0xB5ED, 0x9A,
	0xB5EF, 0xE9,
	0xB600, 0x01,
	0xB601, 0x00,
	0xB603, 0xFE,
	0xB605, 0xF8,
	0xB607, 0xED,
	0xB609, 0xD4,
	0xB60B, 0xB7,
	0xB60D, 0x93,
	0xB60F, 0xB7,
	0xB611, 0xD4,
	0xB612, 0x00,
	0xB613, 0xFE,
	0xB628, 0x00,
	0xB629, 0xAA,
	0xB62A, 0x00,
	0xB62B, 0x78,
	0xB62D, 0x55,
	0xB62F, 0x3E,
	0xB631, 0x2B,
	0xB633, 0x20,
	0xB635, 0x18,
	0xB637, 0x12,
	0xB639, 0x0E,
	0xB63B, 0x06,
	0xB63C, 0x02,
	0xB63D, 0xAA,
	0xB63E, 0x02,
	0xB63F, 0x00,
	0xB640, 0x01,
	0xB641, 0x99,
	0xB642, 0x01,
	0xB643, 0x24,
	0xB645, 0xCC,
	0xB647, 0x66,
	0xB649, 0x38,
	0xB64B, 0x21,
	0xB64D, 0x14,
	0xB64F, 0x0E,
	0xB664, 0x00,
	0xB665, 0xCC,
	0xB666, 0x00,
	0xB667, 0x92,
	0xB669, 0x66,
	0xB66B, 0x4B,
	0xB66D, 0x34,
	0xB66F, 0x28,
	0xB671, 0x1E,
	0xB673, 0x18,
	0xB675, 0x11,
	0xB677, 0x08,
	0xB678, 0x04,
	0xB679, 0x00,
	0xB67A, 0x04,
	0xB67B, 0x00,
	0xB67C, 0x02,
	0xB67D, 0xAA,
	0xB67E, 0x02,
	0xB67F, 0x00,
	0xB680, 0x01,
	0xB681, 0x99,
	0xB682, 0x01,
	0xB683, 0x24,
	0xB685, 0xCC,
	0xB687, 0x66,
	0xB689, 0x38,
	0xB68B, 0x0E,
	0xB68C, 0x02,
	0xB68D, 0xAA,
	0xB68E, 0x02,
	0xB68F, 0x00,
	0xB690, 0x01,
	0xB691, 0x99,
	0xB692, 0x01,
	0xB693, 0x24,
	0xB695, 0xE3,
	0xB697, 0x9D,
	0xB699, 0x71,
	0xB69B, 0x37,
	0xB69D, 0x1F,
	0xE869, 0x00,
	0xE877, 0x00,
	0xEE01, 0x30,
	0xEE03, 0x30,
	0xEE07, 0x08,
	0xEE09, 0x08,
	0xEE0B, 0x08,
	0xEE0D, 0x30,
	0xEE0F, 0x30,
	0xEE12, 0x00,
	0xEE13, 0x10,
	0xEE14, 0x00,
	0xEE15, 0x10,
	0xEE16, 0x00,
	0xEE17, 0x10,
	0xEE31, 0x30,
	0xEE33, 0x30,
	0xEE3D, 0x30,
	0xEE3F, 0x30,
	0xF645, 0x40,
	0xF646, 0x01,
	0xF647, 0x00,
	0x0350, 0x01,
};

kal_uint16 addr_data_pair_preview_imx686gms[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x0342, 0x27,
	0x0343, 0xE0,
	0x0340, 0x0E,
	0x0341, 0x28,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x24,
	0x0349, 0x1F,
	0x034A, 0x1B,
	0x034B, 0x1F,
	0x0220, 0x62,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x30D8, 0x04,
	0x3200, 0x41,
	0x3201, 0x41,
	0x350C, 0x00,
	0x350D, 0x00,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x12,
	0x040D, 0x10,
	0x040E, 0x0D,
	0x040F, 0x90,
	0x034C, 0x12,
	0x034D, 0x10,
	0x034E, 0x0D,
	0x034F, 0x90,
	0x0301, 0x08,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xB9,
	0x030B, 0x02,
	0x030D, 0x04,
	0x030E, 0x02,
	0x030F, 0x4E,
	0x0310, 0x01,
	0x30D9, 0x00,
	0x32D5, 0x00,
	0x32D6, 0x00,
	0x403D, 0x05,
	0x403E, 0x00,
	0x403F, 0x78,
	0x40A0, 0x00,
	0x40A1, 0x00,
	0x40A4, 0x00,
	0x40A5, 0x00,
	0x40A6, 0x00,
	0x40A7, 0x00,
	0x40B8, 0x01,
	0x40B9, 0x54,
	0x40BC, 0x01,
	0x40BD, 0x6D,
	0x40BE, 0x00,
	0x40BF, 0x00,
	0x40AA, 0x00,
	0x40AB, 0x00,
	0xAF06, 0x07,
	0xAF07, 0xF1,
	0x0202, 0x0D,
	0x0203, 0xE8,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3116, 0x01,
	0x3117, 0xF4,
	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x3118, 0x00,
	0x3119, 0x00,
	0x311A, 0x01,
	0x311B, 0x00,
	0x4018, 0x04,
	0x4019, 0x7C,
	0x401A, 0x00,
	0x401B, 0x01,
	0x3400, 0x02,
	0x3091, 0x00,
	0x3092, 0x01,
	0x4324, 0x02,
	0x4325, 0x40,

#if Long_Exp_16s
	0x0202, 0x6A,
	0x0203, 0x44,
	0x3100, 0x06,
#endif
};

kal_uint16 addr_data_pair_custom3_imx686gms[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x0342, 0x3B,
	0x0343, 0x40,
	0x0340, 0x1B,
	0x0341, 0xAF,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x24,
	0x0349, 0x1F,
	0x034A, 0x1B,
	0x034B, 0x1F,
	0x0220, 0x62,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x0A,
	0x30D8, 0x00,
	0x3200, 0x01,
	0x3201, 0x01,
	0x350C, 0x00,
	0x350D, 0x00,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x24,
	0x040D, 0x20,
	0x040E, 0x1B,
	0x040F, 0x20,
	0x034C, 0x24,
	0x034D, 0x20,
	0x034E, 0x1B,
	0x034F, 0x20,
	0x0301, 0x08,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xD7,
	0x030B, 0x02,
	0x030D, 0x04,
	0x030E, 0x03,
	0x030F, 0x2D,
	0x0310, 0x01,
	0x30D9, 0x01,
	0x32D5, 0x01,
	0x32D6, 0x01,
	0x403D, 0x10,
	0x403E, 0x00,
	0x403F, 0x78,
	0x40A0, 0x03,
	0x40A1, 0xD4,
	0x40A4, 0x00,
	0x40A5, 0x00,
	0x40A6, 0x00,
	0x40A7, 0x00,
	0x40B8, 0x01,
	0x40B9, 0xC2,
	0x40BC, 0x01,
	0x40BD, 0x40,
	0x40BE, 0x01,
	0x40BF, 0x40,
	0x40AA, 0x00,
	0x40AB, 0x00,
	0xAF06, 0x03,
	0xAF07, 0xFB,
	0x0202, 0x1B,
	0x0203, 0x6F,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3116, 0x01,
	0x3117, 0xF4,
	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x3118, 0x00,
	0x3119, 0x00,
	0x311A, 0x01,
	0x311B, 0x00,
	0x4018, 0x04,
	0x4019, 0x7C,
	0x401A, 0x00,
	0x401B, 0x01,
	0x3400, 0x02,
	0x3091, 0x00,
	0x3092, 0x01,
};

kal_uint16 addr_data_pair_custom2_imx686gms[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x0342, 0x27,
	0x0343, 0xE0,
	0x0340, 0x0E,
	0x0341, 0x28,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x24,
	0x0349, 0x1F,
	0x034A, 0x1B,
	0x034B, 0x1F,
	0x0220, 0x62,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x30D8, 0x04,
	0x3200, 0x41,
	0x3201, 0x41,
	0x350C, 0x00,
	0x350D, 0x00,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x12,
	0x040D, 0x10,
	0x040E, 0x0D,
	0x040F, 0x90,
	0x034C, 0x12,
	0x034D, 0x10,
	0x034E, 0x0D,
	0x034F, 0x90,
	0x0301, 0x08,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xB9,
	0x030B, 0x02,
	0x030D, 0x04,
	0x030E, 0x02,
	0x030F, 0x4E,
	0x0310, 0x01,
	0x30D9, 0x00,
	0x32D5, 0x00,
	0x32D6, 0x00,
	0x403D, 0x05,
	0x403E, 0x00,
	0x403F, 0x78,
	0x40A0, 0x00,
	0x40A1, 0x00,
	0x40A4, 0x00,
	0x40A5, 0x00,
	0x40A6, 0x00,
	0x40A7, 0x00,
	0x40B8, 0x01,
	0x40B9, 0x54,
	0x40BC, 0x01,
	0x40BD, 0x6D,
	0x40BE, 0x00,
	0x40BF, 0x00,
	0x40AA, 0x00,
	0x40AB, 0x00,
	0xAF06, 0x07,
	0xAF07, 0xF1,
	0x0202, 0x0D,
	0x0203, 0xE8,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3116, 0x01,
	0x3117, 0xF4,
	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x3118, 0x00,
	0x3119, 0x00,
	0x311A, 0x01,
	0x311B, 0x00,
	0x4018, 0x04,
	0x4019, 0x7C,
	0x401A, 0x00,
	0x401B, 0x01,
	0x3400, 0x02,
	0x3091, 0x00,
	0x3092, 0x01,
	0x4324, 0x02,
	0x4325, 0x40,
};

kal_uint16 addr_data_pair_capture_imx686gms[] = {
	0x0112, 0x0A,
	0x0113, 0x0A,
	0x0114, 0x03,
	0x0342, 0x27,
	0x0343, 0xE0,
	0x0340, 0x0E,
	0x0341, 0x28,
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x24,
	0x0349, 0x1F,
	0x034A, 0x1B,
	0x034B, 0x1F,
	0x0220, 0x62,
	0x0221, 0x11,
	0x0222, 0x01,
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x08,
	0x30D8, 0x04,
	0x3200, 0x41,
	0x3201, 0x41,
	0x350C, 0x00,
	0x350D, 0x00,
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x12,
	0x040D, 0x10,
	0x040E, 0x0D,
	0x040F, 0x90,
	0x034C, 0x12,
	0x034D, 0x10,
	0x034E, 0x0D,
	0x034F, 0x90,
	0x0301, 0x08,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x00,
	0x0307, 0xB9,
	0x030B, 0x02,
	0x030D, 0x04,
	0x030E, 0x02,
	0x030F, 0x4E,
	0x0310, 0x01,
	0x30D9, 0x00,
	0x32D5, 0x00,
	0x32D6, 0x00,
	0x403D, 0x05,
	0x403E, 0x00,
	0x403F, 0x78,
	0x40A0, 0x00,
	0x40A1, 0x00,
	0x40A4, 0x00,
	0x40A5, 0x00,
	0x40A6, 0x00,
	0x40A7, 0x00,
	0x40B8, 0x01,
	0x40B9, 0x54,
	0x40BC, 0x01,
	0x40BD, 0x6D,
	0x40BE, 0x00,
	0x40BF, 0x00,
	0x40AA, 0x00,
	0x40AB, 0x00,
	0xAF06, 0x07,
	0xAF07, 0xF1,
	0x0202, 0x0D,
	0x0203, 0xE8,
	0x0224, 0x01,
	0x0225, 0xF4,
	0x3116, 0x01,
	0x3117, 0xF4,
	0x0204, 0x00,
	0x0205, 0x00,
	0x0216, 0x00,
	0x0217, 0x00,
	0x0218, 0x01,
	0x0219, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,
	0x3118, 0x00,
	0x3119, 0x00,
	0x311A, 0x01,
	0x311B, 0x00,
	0x4018, 0x04,
	0x4019, 0x7C,
	0x401A, 0x00,
	0x401B, 0x01,
	0x3400, 0x02,
	0x3091, 0x00,
	0x3092, 0x01,	
	0x4324, 0x02,
	0x4325, 0x40,
};


kal_uint16 addr_data_pair_hs_video_imx686gms[] = {
	0x0112,0x0A,
	0x0113,0x0A,
	0x0114,0x03,
	0x0342,0x15,
	0x0343,0x70,
	0x0340,0x06,
	0x0341,0x94,
	0x0344,0x00,
	0x0345,0x00,
	0x0346,0x05,
	0x0347,0x20,
	0x0348,0x24,
	0x0349,0x1F,
	0x034A,0x15,
	0x034B,0xFF,
	0x0220,0x62,
	0x0221,0x11,
	0x0222,0x01,
	0x0900,0x01,
	0x0901,0x44,
	0x0902,0x08,
	0x30D8,0x00,
	0x3200,0x43,
	0x3201,0x43,
	0x350C,0x00,
	0x350D,0x00,
	0x0408,0x00,
	0x0409,0xC4,
	0x040A,0x00,
	0x040B,0x00,
	0x040C,0x07,
	0x040D,0x80,
	0x040E,0x04,
	0x040F,0x38,
	0x034C,0x07,
	0x034D,0x80,
	0x034E,0x04,
	0x034F,0x38,
	0x0301,0x08,
	0x0303,0x02,
	0x0305,0x04,
	0x0306,0x00,
	0x0307,0xB9,
	0x030B,0x04,
	0x030D,0x04,
	0x030E,0x03,
	0x030F,0x3A,
	0x0310,0x01,
	0x30D9,0x01,
	0x32D5,0x00,
	0x32D6,0x00,
	0x403D,0x06,
	0x403E,0x00,
	0x403F,0x5A,
	0x40A0,0x00,
	0x40A1,0xC8,
	0x40A4,0x01,
	0x40A5,0x2C,
	0x40A6,0x00,
	0x40A7,0x00,
	0x40B8,0x01,
	0x40B9,0x54,
	0x40BC,0x01,
	0x40BD,0x0E,
	0x40BE,0x01,
	0x40BF,0x0E,
	0x40AA,0x00,
	0x40AB,0x00,
	0xAF06,0xFF,
	0xAF07,0xFF,
	0x0202,0x06,
	0x0203,0x54,
	0x0224,0x01,
	0x0225,0xF4,
	0x3116,0x01,
	0x3117,0xF4,
	0x0204,0x00,
	0x0205,0x00,
	0x0216,0x00,
	0x0217,0x00,
	0x0218,0x01,
	0x0219,0x00,
	0x020E,0x01,
	0x020F,0x00,
	0x3118,0x00,
	0x3119,0x00,
	0x311A,0x01,
	0x311B,0x00,
	0x4018,0x00,
	0x4019,0x00,
	0x401A,0x00,
	0x401B,0x00,
	0x3400,0x01,
	0x3091,0x00,
	0x3092,0x00,
};
	
kal_uint16 addr_data_pair_slim_video_imx686gms[] = {
	0x0112,0x0A,
	0x0113,0x0A,
	0x0114,0x03,
	0x0342,0x15,
	0x0343,0x70,
	0x0340,0x06,
	0x0341,0x94,
	0x0344,0x00,
	0x0345,0x00,
	0x0346,0x07,
	0x0347,0xF0,
	0x0348,0x24,
	0x0349,0x1F,
	0x034A,0x13,
	0x034B,0x2F,
	0x0220,0x62,
	0x0221,0x11,
	0x0222,0x01,
	0x0900,0x01,
	0x0901,0x44,
	0x0902,0x08,
	0x30D8,0x00,
	0x3200,0x43,
	0x3201,0x43,
	0x350C,0x00,
	0x350D,0x00,
	0x0408,0x02,
	0x0409,0x04,
	0x040A,0x00,
	0x040B,0x00,
	0x040C,0x05,
	0x040D,0x00,
	0x040E,0x02,
	0x040F,0xD0,
	0x034C,0x05,
	0x034D,0x00,
	0x034E,0x02,
	0x034F,0xD0,
	0x0301,0x08,
	0x0303,0x02,
	0x0305,0x04,
	0x0306,0x00,
	0x0307,0xB9,
	0x030B,0x04,
	0x030D,0x04,
	0x030E,0x03,
	0x030F,0x3A,
	0x0310,0x01,
	0x30D9,0x01,
	0x32D5,0x00,
	0x32D6,0x00,
	0x403D,0x06,
	0x403E,0x00,
	0x403F,0x5A,
	0x40A0,0x00,
	0x40A1,0xC8,
	0x40A4,0x01,
	0x40A5,0x2C,
	0x40A6,0x00,
	0x40A7,0x00,
	0x40B8,0x01,
	0x40B9,0x54,
	0x40BC,0x01,
	0x40BD,0x0E,
	0x40BE,0x01,
	0x40BF,0x0E,
	0x40AA,0x00,
	0x40AB,0x00,
	0xAF06,0xFF,
	0xAF07,0xFF,
	0x0202,0x06,
	0x0203,0x54,
	0x0224,0x01,
	0x0225,0xF4,
	0x3116,0x01,
	0x3117,0xF4,
	0x0204,0x00,
	0x0205,0x00,
	0x0216,0x00,
	0x0217,0x00,
	0x0218,0x01,
	0x0219,0x00,
	0x020E,0x01,
	0x020F,0x00,
	0x3118,0x00,
	0x3119,0x00,
	0x311A,0x01,
	0x311B,0x00,
	0x4018,0x00,
	0x4019,0x00,
	0x401A,0x00,
	0x401B,0x00,
	0x3400,0x01,
	0x3091,0x00,
	0x3092,0x00,
};

static void sensor_init(void)
{
	LOG_INF("E init\n");
	imx686gms_table_write_cmos_sensor(addr_data_pair_init_imx686gms_Ver6,
		sizeof(addr_data_pair_init_imx686gms_Ver6) / sizeof(kal_uint16));
	LOG_INF("L\n");
} /*	sensor_init  */

#if 0
static void capture_setting(void)
{
} /*	capture_setting  */
#endif

static void preview_setting(void)
{
	LOG_INF("E binning_normal_setting\n");

#if 1	
	write_cmos_sensor(0x0100,0x00); //standby

	imx686gms_table_write_cmos_sensor(addr_data_pair_preview_imx686gms,
         sizeof(addr_data_pair_preview_imx686gms) / sizeof(kal_uint16));  

	//write_cmos_sensor(0x0100,0x01); //steaming
	
#endif
	
	LOG_INF("L\n");
}	/*	preview_setting  */

static void capture_setting(kal_uint16 currefps, kal_bool stream_on)
{
	LOG_INF("E currefps:%d\n", currefps);
	
	write_cmos_sensor(0x0100,0x00); //standby
	
	imx686gms_table_write_cmos_sensor(addr_data_pair_capture_imx686gms,
		sizeof(addr_data_pair_capture_imx686gms) / sizeof(kal_uint16));
	
    //write_cmos_sensor(0x0100,0x01); //streaming

//	capture_setting();
	LOG_INF("L!\n");
}

static void normal_video_setting(void)
{
	LOG_INF("E\n");
	preview_setting();
}

static void hs_video_setting(void)
{
	LOG_INF("E\n");
	imx686gms_table_write_cmos_sensor(addr_data_pair_hs_video_imx686gms,
		sizeof(addr_data_pair_hs_video_imx686gms) / sizeof(kal_uint16));
}

static void slim_video_setting(void)
{
	LOG_INF("E\n");
	imx686gms_table_write_cmos_sensor(addr_data_pair_slim_video_imx686gms,
		sizeof(addr_data_pair_slim_video_imx686gms) / sizeof(kal_uint16));
}

static void custom1_setting(void)
{
	/* custom1 32M setting */
	LOG_INF("E\n");
#if 0
	imx686gms_table_write_cmos_sensor(addr_data_pair_custom1_imx686gms,
		sizeof(addr_data_pair_custom1_imx686gms) / sizeof(kal_uint16));
#else
	preview_setting();
#endif
}

static void custom2_setting(void)
{
	/* custom2 48M@15fps setting */
	LOG_INF("E\n");
	imx686gms_table_write_cmos_sensor(addr_data_pair_custom2_imx686gms,
		sizeof(addr_data_pair_custom2_imx686gms) / sizeof(kal_uint16));
}

static void custom3_setting(void)
{
	/* custom3 stero@34fps setting */
	LOG_INF("E\n");
	imx686gms_table_write_cmos_sensor(addr_data_pair_custom3_imx686gms,
		sizeof(addr_data_pair_custom3_imx686gms) / sizeof(kal_uint16));
}

/*************************************************************************
* FUNCTION
*	get_imgsensor_id
*
* DESCRIPTION
*	This function get the sensor ID
*
* PARAMETERS
*	*sensorID : return the sensor ID
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
//extern int g_CameraSensorIdx;
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;

    /*sensor have two i2c address 0x34 & 0x20,
     *we should detect the module used i2c address
     */
#if 0	 
    if (g_CameraSensorIdx != 0) {
        LOG_INF("g_CameraSensorIdx != 0, skip check imgsensor id\n");
        *sensor_id = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }
#endif
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
			gpio_direction_input(24+46);
			gpio_direction_input(26+46);
			printk("lpp--imx686-gpio_get_value(24)=%d (26)=%d\n",gpio_get_value(70),gpio_get_value(72));
			if( gpio_get_value(70)==0)
			{
				*sensor_id = 0xFFFFFFFF;
        		return ERROR_SENSOR_CONNECT_FAIL;
			}
            *sensor_id = ((read_cmos_sensor_8(0x0016) << 8)
                    | read_cmos_sensor_8(0x0017))+1;
            LOG_INF(
                "lhh read_0x0000=0x%x, 0x0001=0x%x,0x0000_0001=0x%x\n",
                read_cmos_sensor_8(0x0016),
                read_cmos_sensor_8(0x0017),
                read_cmos_sensor(0x0000));
            if (*sensor_id == imgsensor_info.sensor_id) {
                LOG_INF("lhh gms i2c write id: 0x%x, sensor id: 0x%x\n",
                    imgsensor.i2c_write_id, *sensor_id);
				if(LRC_SQC_readed != 1) {
					LOG_INF("lhh Read LRC,SQC start LRC_SQC_readed=%d \n", LRC_SQC_readed);
					if((LRC_SIZE == read_imx686gms_LRC(imx686gms_lrc_data)) && (DATA_SIZE == read_imx686gms_SQC(imx686gms_qsc_data)))
						LRC_SQC_readed = 1;
					LOG_INF("lhh Read LRC,SQC end LRC_SQC_readed=%d \n", LRC_SQC_readed);
				}		
                return ERROR_NONE;
            }

            LOG_INF("lhh Read sensor id fail, id: 0x%x\n",
                imgsensor.i2c_write_id);
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

/*************************************************************************
* FUNCTION
*	open
*
* DESCRIPTION
*	This function initialize the registers of CMOS sensor
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0;
	unsigned char lrc_reg;
	/* sensor have two i2c address 0x35 0x34 & 0x21 0x20, we should detect the module used i2c address */
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            sensor_id = ((read_cmos_sensor_8(0x0016) << 8)
                    | read_cmos_sensor_8(0x0017))+1;
            if (sensor_id == imgsensor_info.sensor_id) {
                LOG_INF("lhh gms i2c write id: 0x%x, sensor id: 0x%x\n",
                    imgsensor.i2c_write_id, sensor_id);
                break;
            }
            LOG_INF("lhh Read sensor id fail, id: 0x%x\n",
                imgsensor.i2c_write_id);
            retry--;
        } while (retry > 0);
        i++;
        if (sensor_id == imgsensor_info.sensor_id)
            break;
        retry = 2;
    }
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;
	/* initail sequence write in  */

	sensor_init();
	
	lrc_reg = read_cmos_sensor_8(0x340D);//LRC¨º1?¨¹reg
	LOG_INF("lhh LRC_SQC_readed:%d lrc_reg:%d \n",LRC_SQC_readed,lrc_reg);
	if(LRC_SQC_readed){
		LOG_INF("lhh write write_imx686gms_LRC_Data start \n");
		write_imx686gms_LRC_Data();
		LOG_INF("lhh write write_imx686gms_LRC_Data end \n");
	}
	
	//IMX686GMS_MIPI_update_awb(imgsensor.i2c_write_id);

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.shutter = 0x3D0;
	imgsensor.gain = 0x100;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_mode = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}	/*	open  */



/*************************************************************************
* FUNCTION
*	close
*
* DESCRIPTION
*
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 close(void)
{
	LOG_INF("E\n");
	/*No Need to implement this function*/

	return ERROR_NONE;
}	/*	close  */


/*************************************************************************
* FUNCTION
* preview
*
* DESCRIPTION
*	This function start the sensor preview.
*
* PARAMETERS
*	*image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E imgsensor.hdr_mode=%d\n", imgsensor.hdr_mode);

	LOG_INF("E preview normal\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	//set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*	capture
*
* DESCRIPTION
*	This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	/* PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M */
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				imgsensor.current_fps, imgsensor_info.cap.max_framerate/10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	if(LRC_SQC_readed){
		LOG_INF("lhh write write_imx686gms_QSC_Data start \n");
		write_imx686gms_QSC_Data();
		LOG_INF("lhh write write_imx686gms_QSC_Data end \n");
	}
	capture_setting(imgsensor.current_fps, 1);
	//set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

		LOG_INF("E preview normal\n");
		spin_lock(&imgsensor_drv_lock);
		imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
		imgsensor.pclk = imgsensor_info.normal_video.pclk;
		imgsensor.line_length = imgsensor_info.normal_video.linelength;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength;
		imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
		spin_unlock(&imgsensor_drv_lock);
		normal_video_setting();
		//set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	normal_video   */

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
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();

	//set_mirror_flip(imgsensor.mirror);


	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	//set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}	/*	slim_video	 */

static kal_uint32 custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	if (imgsensor.current_fps == imgsensor_info.custom1.max_framerate) {
		imgsensor.pclk = imgsensor_info.custom1.pclk;
		imgsensor.line_length = imgsensor_info.custom1.linelength;
		imgsensor.frame_length = imgsensor_info.custom1.framelength;
		imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();
	//set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	if (imgsensor.current_fps == imgsensor_info.custom2.max_framerate) {
		imgsensor.pclk = imgsensor_info.custom2.pclk;
		imgsensor.line_length = imgsensor_info.custom2.linelength;
		imgsensor.frame_length = imgsensor_info.custom2.framelength;
		imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	
	//write_imx686gms_QSC_Data();
	
	custom2_setting();
	//set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}

static kal_uint32 custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	if (imgsensor.current_fps == imgsensor_info.custom3.max_framerate) {
		imgsensor.pclk = imgsensor_info.custom3.pclk;
		imgsensor.line_length = imgsensor_info.custom3.linelength;
		imgsensor.frame_length = imgsensor_info.custom3.framelength;
		imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
	custom3_setting();
	//set_mirror_flip(imgsensor.mirror);
	
	if(LRC_SQC_readed){
		LOG_INF("lhh write write_imx686gms_QSC_Data start \n");
		write_imx686gms_QSC_Data();
		LOG_INF("lhh write write_imx686gms_QSC_Data end \n");
	}
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


	sensor_resolution->SensorHighSpeedVideoWidth	 = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight	 = imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth	 = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight	 = imgsensor_info.slim_video.grabwindow_height;
	
	sensor_resolution->SensorCustom1Width = imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height = imgsensor_info.custom1.grabwindow_height;
	sensor_resolution->SensorCustom2Width = imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height = imgsensor_info.custom2.grabwindow_height;
	sensor_resolution->SensorCustom3Width = imgsensor_info.custom3.grabwindow_width;
	sensor_resolution->SensorCustom3Height = imgsensor_info.custom3.grabwindow_height;


	return ERROR_NONE;
}	/*	get_resolution	*/

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
	sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
	sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
	sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
	/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	/* The frame of setting sensor gain */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
#if PDAF_MODE_SUPPORT
	sensor_info->PDAF_Support = 2;
#else
	sensor_info->PDAF_Support = 0;
#endif	
	sensor_info->HDR_Support = 0;	/*0: NO HDR, 1: iHDR, 2:mvHDR, 3:zHDR, 4:four-cell mVHDR*/

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
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x */
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
		case MSDK_SCENARIO_ID_CUSTOM1:
			sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
				
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;
				
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;
			break;

		default:
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
				imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			break;
	}

	return ERROR_NONE;
}	/*	get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
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
	case MSDK_SCENARIO_ID_CUSTOM1:
		custom1(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		custom2(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		custom3(image_window, sensor_config_data);
		break;
	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);
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

	LOG_INF("scenario_id = %d, framerate = %d, hdr_mode = %d\n", scenario_id, framerate, imgsensor.hdr_mode);

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
			{
				return ERROR_NONE;
			}
				frame_length = imgsensor_info.normal_video.pclk / framerate * 10 /
					imgsensor_info.normal_video.linelength;
				spin_lock(&imgsensor_drv_lock);
				imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ?
					(frame_length - imgsensor_info.normal_video.framelength) : 0;
				imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
				imgsensor.min_frame_length = imgsensor.frame_length;
				spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
					LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
					framerate, imgsensor_info.cap.max_framerate/10);
					frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
					spin_lock(&imgsensor_drv_lock);
					imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ?
						(frame_length - imgsensor_info.cap.framelength) : 0;
					imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
					imgsensor.min_frame_length = imgsensor.frame_length;
					spin_unlock(&imgsensor_drv_lock);
			set_dummy();
	    	break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			frame_length = imgsensor_info.hs_video.pclk / framerate * 10 /
				imgsensor_info.hs_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ?
				(frame_length - imgsensor_info.hs_video.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			frame_length = imgsensor_info.slim_video.pclk / framerate * 10 /
				imgsensor_info.slim_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ?
				(frame_length - imgsensor_info.slim_video.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;			
		case MSDK_SCENARIO_ID_CUSTOM1:
			frame_length = imgsensor_info.custom1.pclk / framerate * 10 / 
				imgsensor_info.custom1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength) ? 
				(frame_length - imgsensor_info.custom1.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			frame_length = imgsensor_info.custom2.pclk / framerate * 10 / 
				imgsensor_info.custom2.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.custom2.framelength) ? 
				(frame_length - imgsensor_info.custom2.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;
		case MSDK_SCENARIO_ID_CUSTOM3:
			frame_length = imgsensor_info.custom3.pclk / framerate * 10 / 
				imgsensor_info.custom3.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.custom3.framelength) ? 
				(frame_length - imgsensor_info.custom3.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.custom3.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();
			break;
		
		default:  /* coding with  preview scenario by default */
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
	case MSDK_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		*framerate = imgsensor_info.custom3.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable) {
		/* 0x5E00[8]: 1 enable,  0 disable */
		/* 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK */
		//write_cmos_sensor_8(0x0601, 0x02); //Color Bar
	} else {
		/* 0x5E00[8]: 1 enable,  0 disable */
		/* 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK */
		//write_cmos_sensor_8(0x0601, 0x00);
	}
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static void hdr_write_tri_shutter(kal_uint16 le, kal_uint16 me, kal_uint16 se)
{
	kal_uint16 realtime_fps = 0;

	LOG_INF("E! le:0x%x, me:0x%x, se:0x%x\n", le, me, se);
	spin_lock(&imgsensor_drv_lock);
	if (le > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = le + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	if (le < imgsensor_info.min_shutter)
		le = imgsensor_info.min_shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			write_cmos_sensor_8(0x0104, 0x01);
			write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8); /*FRM_LENGTH_LINES[15:8]*/
			write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF); /*FRM_LENGTH_LINES[7:0]*/
			write_cmos_sensor_8(0x0104, 0x00);
		}
	} else {
		write_cmos_sensor_8(0x0104, 0x01);
		write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor_8(0x0104, 0x00);
	}

	write_cmos_sensor_8(0x0104, 0x01);
	/* Long exposure */
	write_cmos_sensor_8(0x0202, (le >> 8) & 0xFF);
	write_cmos_sensor_8(0x0203, le & 0xFF);
	/* Muddle exposure */
	write_cmos_sensor_8(0x3FE0, (me >> 8) & 0xFF); /*MID_COARSE_INTEG_TIME[15:8]*/
	write_cmos_sensor_8(0x3FE1, me & 0xFF); /*MID_COARSE_INTEG_TIME[7:0]*/
	/* Short exposure */
	write_cmos_sensor_8(0x0224, (se >> 8) & 0xFF);
	write_cmos_sensor_8(0x0225, se & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);

	LOG_INF("L! le:0x%x, me:0x%x, se:0x%x\n", le, me, se);

}

static void hdr_write_tri_gain(kal_uint16 lg, kal_uint16 mg, kal_uint16 sg)
{
	kal_uint16 reg_lg, reg_mg, reg_sg;

	if (lg < BASEGAIN || lg > 16 * BASEGAIN) {
		LOG_INF("Error gain setting");

		if (lg < BASEGAIN)
			lg = BASEGAIN;
		else if (lg > 16 * BASEGAIN)
			lg = 16 * BASEGAIN;
	}

	reg_lg = gain2reg(lg);
	reg_mg = gain2reg(mg);
	reg_sg = gain2reg(sg);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.gain = reg_lg;
	spin_unlock(&imgsensor_drv_lock);
	write_cmos_sensor_8(0x0104, 0x01);
	/* Long Gian */
	write_cmos_sensor_8(0x0204, (reg_lg>>8) & 0xFF);
	write_cmos_sensor_8(0x0205, reg_lg & 0xFF);
	/* Middle Gian */
	write_cmos_sensor_8(0x3FE2, (reg_mg>>8) & 0xFF);
	write_cmos_sensor_8(0x3FE3, reg_mg & 0xFF);
	/* Short Gian */
	write_cmos_sensor_8(0x0216, (reg_sg>>8) & 0xFF);
	write_cmos_sensor_8(0x0217, reg_sg & 0xFF);
	write_cmos_sensor_8(0x0104, 0x00);
#if 0
	if (lg > mg) {
		LOG_INF("long gain > medium gain\n");
		write_cmos_sensor_8(0xEB06, 0x00);
		write_cmos_sensor_8(0xEB08, 0x00);
		write_cmos_sensor_8(0xEB0A, 0x00);
		write_cmos_sensor_8(0xEB12, 0x00);
		write_cmos_sensor_8(0xEB14, 0x00);
		write_cmos_sensor_8(0xEB16, 0x00);

		write_cmos_sensor_8(0xEB07, 0x08);
		write_cmos_sensor_8(0xEB09, 0x08);
		write_cmos_sensor_8(0xEB0B, 0x08);
		write_cmos_sensor_8(0xEB13, 0x10);
		write_cmos_sensor_8(0xEB15, 0x10);
		write_cmos_sensor_8(0xEB17, 0x10);
	} else {
		LOG_INF("long gain <= medium gain\n");
		write_cmos_sensor_8(0xEB06, 0x00);
		write_cmos_sensor_8(0xEB08, 0x00);
		write_cmos_sensor_8(0xEB0A, 0x00);
		write_cmos_sensor_8(0xEB12, 0x01);
		write_cmos_sensor_8(0xEB14, 0x01);
		write_cmos_sensor_8(0xEB16, 0x01);

		write_cmos_sensor_8(0xEB07, 0xC8);
		write_cmos_sensor_8(0xEB09, 0xC8);
		write_cmos_sensor_8(0xEB0B, 0xC8);
		write_cmos_sensor_8(0xEB13, 0x2C);
		write_cmos_sensor_8(0xEB15, 0x2C);
		write_cmos_sensor_8(0xEB17, 0x2C);
	}
#endif
	LOG_INF("lg:0x%x, mg:0x%x, sg:0x%x, reg_lg:0x%x, reg_mg:0x%x, reg_sg:0x%x\n",
			lg, mg, sg, reg_lg, reg_mg, reg_sg);

}

static void imx686gms_set_lsc_reg_setting(kal_uint8 index, kal_uint16 *regDa, MUINT32 regNum)
{



}

static void set_imx686gms_ATR(kal_uint16 LimitGain, kal_uint16 LtcRate, kal_uint16 PostGain)
{


}

static kal_uint32 imx686gms_awb_gain(struct SET_SENSOR_AWB_GAIN *pSetSensorAWB)
{


	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
	LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable)
		write_cmos_sensor_8(0x0100, 0x01);
	else
		write_cmos_sensor_8(0x0100, 0x00);

	mdelay(10);
	return ERROR_NONE;
}

static kal_uint32 get_sensor_temperature(void)
{
	//UINT32 temperature = 0;
	//INT32 temperature_convert = 0;

	pr_debug("nothing \n");

	return 25;
}
#if 0
static kal_uint32 seamless_switch(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	kal_uint32 shutter, kal_uint32 gain,
	kal_uint32 shutter_2ndframe, kal_uint32 gain_2ndframe)
{
	
	pr_debug("nothing \n");
	return ERROR_NONE;
}
#endif
static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
							 UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	INT32 *feature_return_para_i32 = (INT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;

	struct SET_PD_BLOCK_INFO_T *PDAFinfo;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	//UINT32 *pAeCtrls = NULL;
	//UINT32 *pScenarios = NULL;
	struct SET_SENSOR_AWB_GAIN *pSetSensorAWB = (struct SET_SENSOR_AWB_GAIN *) feature_para;

	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
			// huangjiwu for  captrue black --begin	
		case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:		
			*(feature_data + 1) = imgsensor_info.min_gain;		
			*(feature_data + 2) = imgsensor_info.max_gain;		
			break;	
		case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:		
			*(feature_data + 0) = imgsensor_info.min_gain_iso;		
			*(feature_data + 1) = imgsensor_info.gain_step;		
			*(feature_data + 2) = imgsensor_info.gain_type;		
			break;	
		case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:		
				*(feature_data + 1) = imgsensor_info.min_shutter;		
				*(feature_data + 2) = imgsensor_info.exp_step;		
				break;	
		case SENSOR_FEATURE_GET_BINNING_TYPE:		
					switch (*(feature_data + 1)) 
						{		
						case MSDK_SCENARIO_ID_CUSTOM3:			
							*feature_return_para_32 = 1; /*BINNING_NONE*/			
							break;		
							case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:		
							case MSDK_SCENARIO_ID_VIDEO_PREVIEW:		
							case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:		
							case MSDK_SCENARIO_ID_SLIM_VIDEO:		
							case MSDK_SCENARIO_ID_CAMERA_PREVIEW:		
							case MSDK_SCENARIO_ID_CUSTOM4:		
								default:			
								*feature_return_para_32 = 1; /*BINNING_AVERAGED*/			
								break;	
						}		
					pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",*feature_return_para_32);		
					*feature_para_len = 4;		
					break;
		case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:	
			switch (*feature_data) {	
				case MSDK_SCENARIO_ID_CUSTOM3:			
					*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;			
					break;		
					default:			
						*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;			
						break;		
						}		
			break;
			case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:		
				/*		 * 1, if driver support new sw frame sync		
				* set_shutter_frame_length() support third para auto_extend_en		 */		
				*(feature_data + 1) = 1;		/* margin info by scenario */		
				*(feature_data + 2) = imgsensor_info.margin;		
				break;	
				// huangjiwu for  captrue black --end	
		case SENSOR_FEATURE_GET_PERIOD:
			*feature_return_para_16++ = imgsensor.line_length;
			*feature_return_para_16 = imgsensor.frame_length;
			*feature_para_len = 4;
			break;
		case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
			LOG_INF("feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n",
				imgsensor.pclk, imgsensor.current_fps);
			*feature_return_para_32 = imgsensor.pclk;
			*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.pclk;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.pclk;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.pclk;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.pclk;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.cap.framelength << 16)
				+ imgsensor_info.cap.linelength;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.normal_video.framelength << 16)
				+ imgsensor_info.normal_video.linelength;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.hs_video.framelength << 16)
				+ imgsensor_info.hs_video.linelength;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.hs_video.framelength << 16)
				+ imgsensor_info.slim_video.linelength;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;
		case SENSOR_FEATURE_SET_ESHUTTER:
			set_shutter(*feature_data);
			break;
		case SENSOR_FEATURE_SET_NIGHTMODE:
			 /* night_mode((BOOL) *feature_data); */
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
			set_auto_flicker_mode((BOOL)*feature_data_16, *(feature_data_16+1));
			break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
			(MUINT32 *)(uintptr_t)(*(feature_data+1)));
			break;
		case SENSOR_FEATURE_GET_PDAF_DATA:
			LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
			/* read_3P3_eeprom((kal_uint16 )(*feature_data),(char*)(uintptr_t)(*(feature_data+1)),
			 *(kal_uint32)(*(feature_data+2)));
			 */
			break;
		case SENSOR_FEATURE_SET_TEST_PATTERN:
			set_test_pattern_mode((BOOL)*feature_data);
			break;
		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: /* for factory mode auto testing */
			*feature_return_para_32 = imgsensor_info.checksum_value;
			*feature_para_len = 4;
			break;
		case SENSOR_FEATURE_SET_FRAMERATE:
			LOG_INF("current fps :%d\n", *feature_data_32);
			spin_lock(&imgsensor_drv_lock);
			imgsensor.current_fps = (UINT16)*feature_data_32;
			spin_unlock(&imgsensor_drv_lock);
			break;
		case SENSOR_FEATURE_GET_CROP_INFO:
			/* LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32)*feature_data); */
			wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
			switch (*feature_data_32) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_CUSTOM1:
				memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[5],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_CUSTOM2:
				memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[6],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_CUSTOM3:
				memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[7],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			}
			break;
		/*HDR CMD */
			case SENSOR_FEATURE_GET_PDAF_INFO:
				LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%llu\n", *feature_data);
				PDAFinfo= (struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
			
				switch (*feature_data) {
					case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
					case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
					case MSDK_SCENARIO_ID_SLIM_VIDEO:
					case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
						memcpy((void *)PDAFinfo,(void *)&imgsensor_pd_info,sizeof(struct SET_PD_BLOCK_INFO_T));
						break;
					default:
						break;
				}
				break;

		case SENSOR_FEATURE_SET_HDR_ATR:
			LOG_INF("SENSOR_FEATURE_SET_HDR_ATR Limit_Gain=%d, LTC Rate=%d, Post_Gain=%d\n",
					(UINT16)*feature_data,
					(UINT16)*(feature_data + 1),
					(UINT16)*(feature_data + 2));
			set_imx686gms_ATR((UINT16)*feature_data,
						(UINT16)*(feature_data + 1),
						(UINT16)*(feature_data + 2));
			break;
		case SENSOR_FEATURE_SET_HDR:
			LOG_INF("hdr enable :%d\n", *feature_data_32);
			spin_lock(&imgsensor_drv_lock);
			imgsensor.hdr_mode = (UINT8)*feature_data_32;
			spin_unlock(&imgsensor_drv_lock);
			break;
		case SENSOR_FEATURE_SET_HDR_SHUTTER:
			LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d, no support\n",
				(UINT16) *feature_data,	(UINT16) *(feature_data + 1));
			/*hdr_write_shutter((UINT16) *feature_data, (UINT16) *(feature_data + 1),
			*	(UINT16) *(feature_data + 2));
			*/
			break;
		case SENSOR_FEATURE_SET_HDR_TRI_SHUTTER:
			LOG_INF("SENSOR_FEATURE_SET_HDR_TRI_SHUTTER LE=%d, ME=%d, SE=%d\n",
					(UINT16) *feature_data,
					(UINT16) *(feature_data + 1),
					(UINT16) *(feature_data + 2));
			hdr_write_tri_shutter((UINT16)*feature_data,
								(UINT16)*(feature_data+1),
								(UINT16)*(feature_data+2));
			break;
		case SENSOR_FEATURE_SET_HDR_TRI_GAIN:
			LOG_INF("SENSOR_FEATURE_SET_HDR_TRI_GAIN LGain=%d, SGain=%d, MGain=%d\n",
					(UINT16) *feature_data,
					(UINT16) *(feature_data + 1),
					(UINT16) *(feature_data + 2));
			hdr_write_tri_gain((UINT16)*feature_data, (UINT16)*(feature_data+1), (UINT16)*(feature_data+2));
			break;
		case SENSOR_FEATURE_GET_VC_INFO:
            LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n", 
			(UINT16)*feature_data);
            pvcinfo = 
			(struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
            switch (*feature_data_32) {
            case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[1],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
                break;
            case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[2],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
                break;
            case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            default:
                memcpy((void *)pvcinfo,(void *)&SENSOR_VC_INFO[0],
				sizeof(struct SENSOR_VC_INFO_STRUCT));
                break;
            }
            break;
		case SENSOR_FEATURE_SET_AWB_GAIN:
			imx686gms_awb_gain(pSetSensorAWB);
			break;
		case SENSOR_FEATURE_SET_LSC_TBL:
			{
				kal_uint8 index = *(((kal_uint8 *)feature_para) + (*feature_para_len));

				imx686gms_set_lsc_reg_setting(index, feature_data_16, (*feature_para_len)/sizeof(UINT16));
			}
			break;
		case SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY:
			/*
			  * SENSOR_VHDR_MODE_NONE  = 0x0,
			  * SENSOR_VHDR_MODE_IVHDR = 0x01,
			  * SENSOR_VHDR_MODE_MVHDR = 0x02,
			  * SENSOR_VHDR_MODE_ZVHDR = 0x09
			  * SENSOR_VHDR_MODE_4CELL_MVHDR = 0x0A
			*/
			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x2;
				break;
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
			case MSDK_SCENARIO_ID_CUSTOM1:
			case MSDK_SCENARIO_ID_CUSTOM2:
			case MSDK_SCENARIO_ID_CUSTOM3:
			default:
				*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0x0;
				break;
			}
			LOG_INF("SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY scenarioId:%llu, HDR:%llu\n"
				, *feature_data, *(feature_data+1));
			break;
			/*END OF HDR CMD */
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
			case MSDK_SCENARIO_ID_CUSTOM1:
				rate = imgsensor_info.custom2.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM2:
				rate = imgsensor_info.custom2.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CUSTOM3:
				rate = imgsensor_info.custom3.mipi_pixel_rate;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				rate = imgsensor_info.pre.mipi_pixel_rate;
				break;
			}
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
		}
		break;
		
		case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
			pr_debug(
			"SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n",
				(UINT16) *feature_data);
			/*PDAF capacity enable or not, 2p8 only full size support PDAF*/
			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				/* video & capture use same setting */
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_CUSTOM1:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_CUSTOM2:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			case MSDK_SCENARIO_ID_CUSTOM3:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			default:
				*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
				break;
			}
			break;

		case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
			pr_debug("SENSOR_FEATURE_GET_PDAF_REG_SETTING %d",
				(*feature_para_len));
			imx686gms_get_pdaf_reg_setting((*feature_para_len) / sizeof(UINT32)
						   , feature_data_16);
			break;			
		case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
			pr_debug("SENSOR_FEATURE_SET_PDAF_REG_SETTING %d",
				(*feature_para_len));
			imx686gms_set_pdaf_reg_setting((*feature_para_len) / sizeof(UINT32)
						   , feature_data_16);
			break;
		case SENSOR_FEATURE_SET_PDAF:
			LOG_INF("PDAF mode :%d\n", *feature_data_16);
			//imgsensor.pdaf_mode= *feature_data_16;
			break;
		case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
			*feature_return_para_i32 = get_sensor_temperature();
			*feature_para_len = 4;
			break;		
/* ITD: Modify Dualcam By Jesse 190924 Start */
		case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
			pr_debug("SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME\n");
			set_shutter_frame_length((UINT16)*feature_data, (UINT16)*(feature_data+1));
			break;
/* ITD: Modify Dualcam By Jesse 190924 End */
		case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
			LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
			streaming_control(KAL_FALSE);
			break;
		case SENSOR_FEATURE_SET_STREAMING_RESUME:
			LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n", *feature_data);
			if (*feature_data != 0)
				set_shutter(*feature_data);
			streaming_control(KAL_TRUE);
			break;

		default:
			break;
	}

	return ERROR_NONE;
}	/*	feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 IMX686GMS_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{

	if (pfFunc != NULL)
		*pfFunc =  &sensor_func;
	return ERROR_NONE;
}	
