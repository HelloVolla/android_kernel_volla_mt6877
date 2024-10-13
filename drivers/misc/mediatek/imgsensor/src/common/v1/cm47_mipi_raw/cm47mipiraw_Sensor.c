/*********************************************************
 Copyright (C),2014-2020,Electronic Technology Co.,Ltd.
 File name: 		cm26_sensor.c
 Author: 			Txl
 Version: 			1.0
 Date: 				2017-3-27
 Description: 		
 History: 			
 					
   1.Date:	 		2017-3-27
 	 Author:	 	Txl
 	 Modification:  Created file
 	 
*********************************************************/
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/gpio.h>
#include <linux/types.h>
#include <mt-plat/mtk_boot.h>
#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_feature.h"

#include "cm47mipiraw_Sensor.h"

#define STREAM_ENABLE       1

#undef	DBG
#define DBG		1
#define PFX     "CM47"
#define LOG_1   LOG_INF("CM47,MIPI 1LANE\n")
#define LOG_2   LOG_INF("preview 1280*800@60fps\n")
#if DBG
#define LOG_DBG(format, args...)    printk(PFX "[%s] " format, __FUNCTION__, ##args)
#define LOG_INF(format, args...)    printk(PFX "[%s] " format, __FUNCTION__, ##args)
#define LOG_WRN(format, args...)    printk(PFX "[%s] " format, __FUNCTION__, ##args)
#define LOG_ERR(format, args...)    printk(PFX "[%s] " format, __FUNCTION__, ##args)
#else
#define LOG_DBG(format, args...) 
#define LOG_INF(format, args...)
#define LOG_WRN(format, args...)    pr_warn(PFX "[%s] " format, __FUNCTION__, ##args)
#define LOG_ERR(format, args...)    pr_err(PFX "[%s] " format, __FUNCTION__, ##args)
#endif

#define CM47_CHIP           0xf0
#define MIPI_SETTLE         65
extern int mtkcam_scan_33_set(int Val);

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static int mProbe = 0;

static struct imgsensor_info_struct imgsensor_info = {
    .sensor_id = CM47_SENSOR_ID,        //record sensor id defined in Kd_imgsensor.h

    .checksum_value = 0xf7375923,        //checksum value for Camera Auto Test

    .pre = {
        .pclk = 38000000,                //record different mode's pclk
        .linelength = 1488,                //record different mode's linelength
        .framelength = 827,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width = 1280,		//record different mode's width of grabwindow
        .grabwindow_height = 800,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
		.mipi_data_lp2hs_settle_dc = MIPI_SETTLE,
		.mipi_pixel_rate = 31000000,
        /*     following for GetDefaultFramerateByScenario()    */
		.max_framerate = 300,	
    },
    .cap = {
        .pclk = 38000000,                //record different mode's pclk
        .linelength = 1488,                //record different mode's linelength
        .framelength = 827,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width = 1280,		//record different mode's width of grabwindow
        .grabwindow_height = 800,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
		.mipi_data_lp2hs_settle_dc = MIPI_SETTLE,
		.mipi_pixel_rate = 31000000,
        /*     following for GetDefaultFramerateByScenario()    */
		.max_framerate = 300,	
    },
    .cap1 = {
        .pclk = 38000000,                //record different mode's pclk
        .linelength = 1488,                //record different mode's linelength
        .framelength = 827,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width = 1280,		//record different mode's width of grabwindow
        .grabwindow_height = 800,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
		.mipi_data_lp2hs_settle_dc = MIPI_SETTLE,
		.mipi_pixel_rate = 31000000,
        /*     following for GetDefaultFramerateByScenario()    */
		.max_framerate = 600,	
    },
    .normal_video = {
        .pclk = 12000000,                //record different mode's pclk
        .linelength = 1488,                //record different mode's linelength
        .framelength = 827,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width = 1280,		//record different mode's width of grabwindow
        .grabwindow_height = 800,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
		.mipi_data_lp2hs_settle_dc = MIPI_SETTLE,
        /*     following for GetDefaultFramerateByScenario()    */
		.max_framerate = 600,	
    },
    .hs_video = {
        .pclk = 12000000,                //record different mode's pclk
        .linelength = 1488,                //record different mode's linelength
        .framelength = 827,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width = 1280,		//record different mode's width of grabwindow
        .grabwindow_height = 800,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
		.mipi_data_lp2hs_settle_dc = MIPI_SETTLE,
        /*     following for GetDefaultFramerateByScenario()    */
		.max_framerate = 600,	
    },
    .slim_video = {
        .pclk = 12000000,                //record different mode's pclk
        .linelength = 1488,                //record different mode's linelength
        .framelength = 827,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width = 1280,		//record different mode's width of grabwindow
        .grabwindow_height = 800,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
		.mipi_data_lp2hs_settle_dc = MIPI_SETTLE,
		.mipi_pixel_rate = 31000000,
        /*     following for GetDefaultFramerateByScenario()    */
		.max_framerate = 300,	
    },
	.margin = 4,			//sensor framelength & shutter margin
	.min_shutter = 1,		//min shutter
	.max_frame_length = 0x7fff,//max framelength by sensor register's limitation
    .ae_shut_delay_frame = 0,    //shutter delay frame for AE cycle, 2 frame with ispGain_delay-shut_delay=2-0=2
    .ae_sensor_gain_delay_frame = 0,//sensor gain delay frame for AE cycle,2 frame with ispGain_delay-sensor_gain_delay=2-0=2
    .ae_ispGain_delay_frame = 2,//isp gain delay frame for AE cycle
    .ihdr_support = 0,      //1, support; 0,not support
    .ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 5,	  //support sensor mode num

    .cap_delay_frame = 3,        //enter capture delay frame num
    .pre_delay_frame = 3,         //enter preview delay frame num
    .video_delay_frame = 3,        //enter video delay frame num
    .hs_video_delay_frame = 3,    //enter high speed video  delay frame num
    .slim_video_delay_frame = 3,//enter slim video delay frame num

    .isp_driving_current = ISP_DRIVING_8MA, //mclk driving current
    .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,//sensor_interface_type
    .mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
    .mipi_settle_delay_mode = 1,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
//    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_MONO,//sensor output first pixel color
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr, 
    .mclk = 24,//mclk value, suggest 24 or 26 for 24Mhz or 26Mhz
    .mipi_lane_num = SENSOR_MIPI_1_LANE,//mipi lane num
    .i2c_addr_table = {0x24,0xff},//record sensor support all write id addr, only supprt 4must end with 0xff
};


static struct imgsensor_struct imgsensor = {
    .mirror = IMAGE_NORMAL,                 //mirrorflip information
    .sensor_mode = IMGSENSOR_MODE_INIT,     //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	.shutter = 0x3D0,					//current shutter
	.gain = 0x100,						//current gain
    .dummy_pixel = 0,                    //current dummypixel
    .dummy_line = 0,                    //current dummyline
    .current_fps = 600,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
    .autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
    .test_pattern = KAL_FALSE,        //test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
    .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
    .ihdr_en = 0, //sensor need support LE, SE with HDR feature
    .i2c_write_id = 0xC0,//record current sensor's i2c write id
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] =
{
    { 1280, 800,	  0,	0, 1280, 800, 1280,  800, 0000, 0000, 1280,  800,	  0,	0, 1280,  800}, // Preview 
    { 1280, 800,	  0,	0, 1280, 800, 1280,  800, 0000, 0000, 1280,  800,	  0,	0, 1280,  800}, // capture 
    { 1280, 800,	  0,	0, 1280, 800, 1280,  800, 0000, 0000, 1280,  800,	  0,	0, 1280,  800}, // video 
    { 1280, 800,	  0,	0, 1280, 800, 1280,  800, 0000, 0000, 1280,  800,	  0,	0, 1280,  800}, //hight speed video 
    { 1280, 800,	  0,	0, 1280, 800, 1280,  800, 0000, 0000, 1280,  800,	  0,	0, 1280,  800}  // slim video 
}; 

#if STREAM_ENABLE
static int mtk_i2c_write(u16 i2cId, u8* data, int data_length)
{
    return iWriteRegI2C(data, data_length, i2cId);
}
#endif

#if STREAM_ENABLE
 void CM47StreamOn(void)
 {
    u8 data[2] = {0xfd, 0x03};
//	u8 data1[2] = {0xf5, 0x03};
  printk("------- dsy CM47_MIPI_RAW_SensorInit %s %d ------- \n", __func__,__LINE__);
    mtk_i2c_write(imgsensor.i2c_write_id, data, 2);
//	mtk_i2c_write(imgsensor.i2c_write_id, data1, 2);
}
#endif

/*************************************************
 Function:		write_cmos_sensor_i2c
 Descroption:	  
 Input: 
    1.i2c
	2.addr
	3.para
 Output: 
 Return: 	
 Other:  
*************************************************/
static void write_cmos_sensor_i2c(kal_uint32 i2c,kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
//	iWriteRegI2C(pu_send_cmd, 3, i2c);
  printk("------- dsy CM47_MIPI_RAW_SensorInit %s %d ------- \n", __func__,__LINE__);
    iWriteRegI2CTiming(pu_send_cmd, 3, i2c,100);
}

/*************************************************
 Function:		write_cmos_sensor
 Descroption:	 
 Input: 
	1.addr
	2.para
 Output: 
 Return: 	
 Other:  
*************************************************/
static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
      printk("------- dsy CM47_MIPI_RAW_SensorInit %s %d ------- \n", __func__,__LINE__);
    write_cmos_sensor_i2c(imgsensor.i2c_write_id,addr,para);
}


/*************************************************
 Function:		read_cmos_sensor
 Descroption:	 
 Input: 
	1.addr
 Output: 
 Return: 	
 Other:  
*************************************************/
static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;

	char pu_send_cmd = addr;
      printk("------- dsy CM47_MIPI_RAW_SensorInit %s %d ------- \n", __func__,__LINE__);
	//iReadRegI2C(&pu_send_cmd, 1, (u8*)&get_byte, 1, imgsensor.i2c_write_id);
	iReadRegI2CTiming(&pu_send_cmd, 1, (u8*)&get_byte, 1, imgsensor.i2c_write_id,100);
	return get_byte;
}

static void set_dummy(void)
{
	 u8 data[2] = {0x00, 0x00};
//	u8 data1[2] = {0xf5, 0x03};
  	printk("------- dsy CM47_MIPI_RAW_Sensor set_dummy %s %d ------- \n", __func__,__LINE__);
    mtk_i2c_write(imgsensor.i2c_write_id, data, 2);

} 
/*************************************************
 Function:		return_sensor_id
 Descroption:	 
 Input: 		None
 Output: 
 Return: 	
 Other:  
*************************************************/
static kal_uint32 return_sensor_id(void)
{
    printk("------- dsy CM47_MIPI_RAW_SensorInit %s %d ------- \n", __func__,__LINE__);
    return read_cmos_sensor(CM47_CHIP);
}
static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{
    //kal_int16 dummy_line;
    kal_uint32 frame_length = imgsensor.frame_length;
    //unsigned long flags;

    frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;

    spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length)
		? frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
    if (min_framelength_en)
        imgsensor.min_frame_length = imgsensor.frame_length;
    spin_unlock(&imgsensor_drv_lock);
    set_dummy();
}  
/*************************************************************************
* FUNCTION
*    set_shutter
*
* DESCRIPTION
*    This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*    iShutter : exposured lines
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
	 
//	u8 data1[2] = {0xf5, 0x03};
  	printk("------- dsy CM47_MIPI_RAW_Sensor set_shutter %s %d ------- \n", __func__,__LINE__);
    

}


/*************************************************************************
* FUNCTION
*    night_mode
*
* DESCRIPTION
*    This function night mode of sensor.
*
* PARAMETERS
*    bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}    /*    night_mode    */

/*************************************************************************
* FUNCTION
*    set_gain
*
* DESCRIPTION
*    This function is to set global gain to sensor.
*
* PARAMETERS
*    iGain : sensor global gain(base: 0x40)
*
* RETURNS
*    the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/

#define ANALOG_GAIN_1 64   // 1.00x
#define ANALOG_GAIN_2 92   // 1.445x

static kal_uint16 set_gain(kal_uint16 gain)
{


	 printk("------- dsy CM47_MIPI_RAW_SensorInit %s %d ------- \n", __func__,__LINE__);
	return gain;

}
 /*************************************************
 Function:		preview_setting
 Descroption:	 
 Input: 		None
 Output: 
 Return: 	
 Other:  
*************************************************/
static void preview_setting(void)
{
      printk("------- dsy CM47_MIPI_RAW_SensorInit %s %d ------- \n", __func__,__LINE__);
    #if STREAM_ENABLE
    CM47StreamOn();
    #endif
}  

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
    LOG_INF("enable: %d\n", enable);

    if (enable) {
		
		 printk("-------  CM47_MIPI_RAW %s enable %d ------- \n", __func__,__LINE__);
    } else {
		 printk("-------  CM47_MIPI_RAW %s disable %d ------- \n", __func__,__LINE__);

    }
    spin_lock(&imgsensor_drv_lock);
    imgsensor.test_pattern = enable;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*    get_imgsensor_id
*
* DESCRIPTION
*    This function get the sensor ID
*
* PARAMETERS
*    *sensorID : return the sensor ID
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry_total_cnt = 10;
	kal_uint8 retry = retry_total_cnt;
      printk("------- dsy CM47_MIPI_RAW_SensorInit %s %d ------- \n", __func__,__LINE__);
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
            *sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {			
                mProbe = 1;
				printk("cam CM47_MIPI_RAW_SensorInit,i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);	  
                return ERROR_NONE;
			}	
            printk("cam CM47_MIPI_RAW_SensorInit,Read sensor id fail, write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
			retry--;
		} while(retry > 0);
		i++;
		retry = retry_total_cnt;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

/*************************************************
 Function:		open
 Descroption:	 
 Input: 		None
 Output: 
 Return: 	
 Other:  
*************************************************/
static kal_uint32 open(void)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
    kal_uint32 sensor_id = 0;
    LOG_1;
    LOG_2;
    printk("------- dsy CM47_MIPI_RAW_SensorInit %s %d ------- \n", __func__,__LINE__);
    if(mProbe == 0)
    {
    	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
    		spin_lock(&imgsensor_drv_lock);
    		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
    		spin_unlock(&imgsensor_drv_lock);
    		do {
                sensor_id = return_sensor_id();
    			if (sensor_id == imgsensor_info.sensor_id) {				
    				printk("cam CM47_MIPI_RAW_SensorInit i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);	  
    				break;
    			}	
                printk("cam CM47_MIPI_RAW_SensorInit  Read sensor id fail, write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
    			retry--;
    		} while(retry > 0);
    		i++;
    		if (sensor_id == imgsensor_info.sensor_id)
    			break;
    		retry = 2;
    	}		 
        if (imgsensor_info.sensor_id != sensor_id)
        {
            return ERROR_SENSOR_CONNECT_FAIL;
        }
    }

    spin_lock(&imgsensor_drv_lock);
    imgsensor.autoflicker_en= KAL_FALSE;
    imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.shutter = 0x3D0;
	imgsensor.gain = 0x100;
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


    
/*************************************************
 Function:		close
 Descroption:	  
 Input: 		None
 Output: 
 Return: 	
 Other:  
*************************************************/
static kal_uint32 close(void)
{
    printk("------- dsy CM47_MIPI_RAW_SensorInit %s %d ------- \n", __func__,__LINE__);
    //mtkcam_scan_33_set(0);
    return ERROR_NONE;
}    

/*************************************************
 Function:		preview
 Descroption:	 
 Input: 
	1.*image_window
	2.*sensor_config_data
 Output: 
 Return: 	
 Other:  
*************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    printk("------- dsy CM47_MIPI_RAW_SensorInit %s %d ------- \n", __func__,__LINE__);
    spin_lock(&imgsensor_drv_lock);
    //mtkcam_scan_33_set(1);
    imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    preview_setting();
    return ERROR_NONE;
} 

/*************************************************
 Function:		get_resolution
 Descroption:	 
 Input: 
	1.*sensor_resolution
 Output: 
 Return: 	
 Other:  
*************************************************/
static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
    sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
    sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;
printk("------- dsy CM47_MIPI_RAW_SensorInit %s %d ------- \n", __func__,__LINE__);
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

/*************************************************
 Function:		get_info
 Descroption:	 
 Input: 
	1.scenario_id
	2.*sensor_info
	3.*sensor_config_data
 Output: 
 Return: 	
 Other:  
*************************************************/
static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                      MSDK_SENSOR_INFO_STRUCT *sensor_info,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
    sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
    sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorInterruptDelayLines = 4; /* not use */
    sensor_info->SensorResetActiveHigh = FALSE; /* not use */
    sensor_info->SensorResetDelayCount = 5; /* not use */
printk("------- dsy CM47_MIPI_RAW_SensorInit %s %d ------- \n", __func__,__LINE__);
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

    sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;          /* The frame of setting shutter default 0 for TG int */
    sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;    /* The frame of setting sensor gain */
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
    sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
    sensor_info->SensorHightSampling = 0;    // 0 is default 1x
    sensor_info->SensorPacketECCOrder = 1;

    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
            break;
            
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.cap.mipi_data_lp2hs_settle_dc;
            break;
            
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;
            break;
            
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;
            break;
            
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;
            break;
            
        default:
            sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
            break;
    }

    return ERROR_NONE;
}

/*************************************************
 Function:		control
 Descroption:	 
 Input: 
	1.scenario_id
	2.*image_window
	3.*sensor_config_data
 Output: 
 Return: 	
 Other:  
*************************************************/
static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)                      
{
    spin_lock(&imgsensor_drv_lock);
    imgsensor.current_scenario_id = scenario_id;
    spin_unlock(&imgsensor_drv_lock);
    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            preview(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            preview(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            preview(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            preview(image_window, sensor_config_data);
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            preview(image_window, sensor_config_data);
            break;

        default:
            preview(image_window, sensor_config_data);
            return ERROR_INVALID_SCENARIO_ID;
    }
    return ERROR_NONE;
}   


static kal_uint32 set_video_mode(UINT16 framerate)
{//This Function not used after ROME
    LOG_INF("framerate = %d\n ", framerate);
    // SetVideoMode Function should fix framerate
    if (framerate == 0)
        // Dynamic frame rate
        return ERROR_NONE;
    spin_lock(&imgsensor_drv_lock);
    if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
        imgsensor.current_fps = 296;
    else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
        imgsensor.current_fps = 146;
    else
        imgsensor.current_fps = framerate;
    spin_unlock(&imgsensor_drv_lock);
    set_max_framerate(imgsensor.current_fps,1);

    return ERROR_NONE;
}


static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
    LOG_INF("enable = %d, framerate = %d \n", enable, framerate);
    spin_lock(&imgsensor_drv_lock);
    if (enable) //enable auto flicker
        imgsensor.autoflicker_en = KAL_TRUE;
    else //Cancel Auto flick
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
            imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            set_dummy();
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            if(framerate == 0)
                return ERROR_NONE;
            frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ? (frame_length - imgsensor_info.normal_video.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            set_dummy();
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        	  if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
                frame_length = imgsensor_info.cap1.pclk / framerate * 10 / imgsensor_info.cap1.linelength;
                spin_lock(&imgsensor_drv_lock);
		            imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength) ? (frame_length - imgsensor_info.cap1.framelength) : 0;
		            imgsensor.frame_length = imgsensor_info.cap1.framelength + imgsensor.dummy_line;
		            imgsensor.min_frame_length = imgsensor.frame_length;
		            spin_unlock(&imgsensor_drv_lock);
            } else {
        		    if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
                    LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",framerate,imgsensor_info.cap.max_framerate/10);
                frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
                spin_lock(&imgsensor_drv_lock);
		            imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
		            imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
		            imgsensor.min_frame_length = imgsensor.frame_length;
		            spin_unlock(&imgsensor_drv_lock);
            }
            set_dummy();
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            set_dummy();
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength): 0;
            imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            set_dummy();
            break;
        default:  //coding with  preview scenario by default
            frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            set_dummy();
            LOG_INF("error scenario_id = %d, we use preview scenario \n", scenario_id);
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
                             UINT8 *feature_para,UINT32 *feature_para_len)
{
    UINT16 *feature_return_para_16=(UINT16 *) feature_para;
    UINT16 *feature_data_16=(UINT16 *) feature_para;
    UINT32 *feature_return_para_32=(UINT32 *) feature_para;
    UINT32 *feature_data_32=(UINT32 *) feature_para;
    unsigned long long *feature_data=(unsigned long long *) feature_para;

    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
    MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data=(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

    LOG_INF("feature_id = %d\n", feature_id);
    switch (feature_id) {
        case SENSOR_FEATURE_GET_PERIOD:
            *feature_return_para_16++ = imgsensor.line_length;
            *feature_return_para_16 = imgsensor.frame_length;
            *feature_para_len=4;
            break;

        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
            *feature_return_para_32 = imgsensor.pclk;
            *feature_para_len=4;
            break;
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
            night_mode((BOOL) *feature_data);
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
			LOG_INF("adb_i2c_read 0x%x = 0x%x\n",sensor_reg_data->RegAddr,sensor_reg_data->RegData);//travis add
            break;

        case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
            *feature_return_para_32=LENS_DRIVER_ID_DO_NOT_CARE;
            *feature_para_len=4;
            break;

        case SENSOR_FEATURE_SET_VIDEO_MODE:
            set_video_mode(*feature_data);
            break;

        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            get_imgsensor_id(feature_return_para_32);
            break;

        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            set_auto_flicker_mode((BOOL)*feature_data_16,*(feature_data_16+1));
            break;

        case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
            set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
            break;
        case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
            get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data), (MUINT32 *)(uintptr_t)(*(feature_data+1)));
            break;

        case SENSOR_FEATURE_SET_TEST_PATTERN:
            set_test_pattern_mode((BOOL)*feature_data);
            break;

        case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
            *feature_return_para_32 = imgsensor_info.checksum_value;
            *feature_para_len=4;
            break;

        case SENSOR_FEATURE_SET_FRAMERATE:
            spin_lock(&imgsensor_drv_lock);
            imgsensor.current_fps = *feature_data;
            spin_unlock(&imgsensor_drv_lock);
            break;

		case SENSOR_FEATURE_SET_HDR:
			spin_lock(&imgsensor_drv_lock);
			imgsensor.ihdr_en = (bool)*feature_data;
			spin_unlock(&imgsensor_drv_lock);
            break;

        case SENSOR_FEATURE_GET_CROP_INFO:
            wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
            switch (*feature_data_32) {
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[1],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[2],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[3],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_SLIM_VIDEO:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[4],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                default:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[0],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
            }

        case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
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

/*************************************************
 Function:		CM47_MIPI_RAW_SensorInit
 Descroption:	 
 Input: 
	1.*pfFunc
 Output: 
 Return: 	
 Other:  
*************************************************/
UINT32 CM47_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
    printk("------- dsy CM47_MIPI_RAW_SensorInit %s %d ------- \n", __func__,__LINE__);
    if (pfFunc!=NULL)
    {
        *pfFunc=&sensor_func;
    }
    
    return ERROR_NONE;
}
