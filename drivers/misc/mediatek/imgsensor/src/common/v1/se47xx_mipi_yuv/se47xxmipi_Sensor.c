/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
//#include <asm/system.h>
#include <linux/dma-mapping.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>

//#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_camera_typedef.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "se47xxmipi_Sensor.h"

/**************** Modify Following Strings for Debug ******************/
#define PFX "se47xx_sensor"
#define LOG_1 cam_pr_debug("SE47XX, 1LANE\n")
/********************   Modify end    *********************************/

#define cam_pr_debug(format, args...) \
	pr_err(PFX "[%s] " format, __func__, ##args)

/*prize add by zhaopengge 20201209---start*/
/*#define OUT_STREAM_BY_CMD*/
/*prize add by zhaopengge 20201209---end*/
static DEFINE_SPINLOCK(imgsensor_drv_lock);
/* used for shutter compensation */

static int isopen=0;
static int scantype = 1;

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = SE47XX_SENSOR_ID,
	.checksum_value = 0xdc9f7d95,
	.pre = {
		.pclk = 42000000,				//record different mode's pclk
		.linelength = 1688,				//record different mode's linelength
		.framelength = 832,			//record different mode's framelength
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1360 ,		//record different mode's width of grabwindow
		.grabwindow_height = 800 ,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 101,//unit , ns
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 42000000,
		.linelength = 1688,				//record different mode's linelength
		.framelength = 832,			//record different mode's framelength
		.startx = 0,
		.starty = 0,
  	       .grabwindow_width = 1360,
		.grabwindow_height = 800,
		.mipi_data_lp2hs_settle_dc = 101,//unit , ns
		.max_framerate = 300,
	},
	.cap1 = {
        .pclk = 42000000,
		.linelength = 1688,				//record different mode's linelength
		.framelength = 832,			//record different mode's framelength
		.startx = 0,
		.starty = 0,
  	       .grabwindow_width = 1360,
		.grabwindow_height = 800,
		.mipi_data_lp2hs_settle_dc = 101,//unit , ns
		.max_framerate = 300,  //less than 13M(include 13M),cap1 max framerate is 24fps,16M max framerate is 20fps, 20M max framerate is 15fps
	},
	.normal_video = {
		.pclk = 42000000,
		.linelength = 1688,
		.framelength = 832,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1360,
		.grabwindow_height = 800,
		.mipi_data_lp2hs_settle_dc = 101,//unit , ns
		.max_framerate = 300,
	},
	.hs_video = {
        	.pclk = 42000000,
        	.linelength = 1688,				//record different mode's linelength
		.framelength = 832,			//record different mode's framelength
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1360 ,		//record different mode's width of grabwindow
		.grabwindow_height = 800 ,
		.mipi_data_lp2hs_settle_dc = 101,//unit , ns
		.max_framerate = 600,
    },
    .slim_video = {
        .pclk = 42000000,
        .linelength = 1688,
        .framelength = 832,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 1360,
        .grabwindow_height = 800,
        .mipi_data_lp2hs_settle_dc = 101,//unit , ns
	.max_framerate = 300,
	},

    .margin = 4,            //sensor framelength & shutter margin
    .min_shutter = 1,        //min shutter
    .max_frame_length = 832,//max framelength by sensor register's limitation

	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,      //1, support; 0,not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
    .sensor_mode_num = 2,      //support sensor mode num

    .cap_delay_frame = 0,        //enter capture delay frame num
    .pre_delay_frame = 0,         //enter preview delay frame num
    .video_delay_frame = 0,        //enter video delay frame num
    .hs_video_delay_frame = 0,    //enter high speed video  delay frame num
    .slim_video_delay_frame = 0,//enter slim video delay frame num
	
	//.cap_delay_frame = 2,
	//.pre_delay_frame = 2,
	//.video_delay_frame = 2,
	//enter high speed video  delay frame num
	//.hs_video_delay_frame = 2,
	//enter slim video delay frame num
	//.slim_video_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
    .mipi_settle_delay_mode = MIPI_SETTLEDELAY_MANUAL,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW8_MONO,
	.mclk = 24,
    .mipi_lane_num = SENSOR_MIPI_1_LANE,//mipi lane num
    .i2c_addr_table = {0x5c, 0xff},//record sensor support all write id addr, only supprt 4must end with 0xff
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,
    .sensor_mode = IMGSENSOR_MODE_PREVIEW, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
    .shutter = 0x3D0,                    //current shutter
    .gain = 0x100,                        //current gain
	.dummy_pixel = 0,
	.dummy_line = 0,
//full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x5c,
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{ 1360, 800,	0,	  0, 1360, 800, 1360,	800, 0000, 0000, 1360,	800,	  0,	0, 1360,  800}, // Preview
	{ 1360, 800,	0,	  0, 1360, 800, 1360,	800, 0000, 0000, 1360,	800,	  0,	0, 1360,  800}, // capture
	{ 1360, 800,	0,	  0, 1360, 800, 1360,	800, 0000, 0000, 1360,	800,	  0,	0, 1360,  800}, // video
	{ 1360, 800,	0,	  0, 1360, 800, 1360,	800, 0000, 0000, 1360,	800,	  0,	0, 1360,  800}, //hight speed video
	{ 1360, 800,	0,	  0, 1360, 800, 1360,	800, 0000, 0000, 1360,	800,	  0,	0, 1360,  800}
};// slim video

extern int scan_i2c_transfer(struct i2c_msg *msg, int msg_num, u32 timing);



#ifdef OUT_STREAM_BY_CMD
static void send_cmd(u8 *buf,int len){
	struct i2c_msg msg;
	u8 buf_read[2]={0x0};
	int ret=0;
	int retry = 10;
	
	do{
		msg.addr = 0x5c;
		msg.flags = 0;
		msg.buf = buf;
		msg.len = len;
		ret = scan_i2c_transfer(&msg, 1, 100000);
		printk("[send_cmd] write cmd:(0x%x 0x%x 0x%x) ret:%d\n", buf[0], buf[1],buf[2],ret);
		if(ret <= 0){
			retry--;
			usleep_range(100,200);
			continue;
		}else
			break;

	}while(retry > 0);
	
	retry = 10;
	
	do{
			msg.flags = I2C_M_RD;
			msg.buf = buf_read;
			msg.len = 2;
			ret = scan_i2c_transfer(&msg, 1, 100000);
			printk("[SE47XXMIPIGetSensorID] read cmd:(0x%x 0x%x) ret:%d\n", buf_read[0], buf_read[1], ret);
			if(buf_read[0] == buf[0] && buf_read[1] == 0x80){
				break;
			}else{
				retry--;
				usleep_range(100,200);
			}
	}while(retry > 0);

		
	
	
}


static void  stream_on(){
	u8 sendbuf_enableHW[3]={0x84,0x01,0x7b};
	u8 sendbuf_enableMIPI[3]={0x86,0x03,0x77};
	u8 sendbuf_enableAimOn[3]={0x55,0x01,0xaa};
	u8 sendbuf_enableIllumOn[3]={0x59,0x01,0xa6};
	u8 sendbuf_enableAcqOn[3]={0x58,0x01,0xa7};
	printk("stream_on .... start \n");
	send_cmd(sendbuf_enableHW,3);
	send_cmd(sendbuf_enableMIPI,3);
	send_cmd(sendbuf_enableAcqOn,3);
	send_cmd(sendbuf_enableAimOn,3);
	send_cmd(sendbuf_enableIllumOn,3);
	printk("stream_on .... end \n");
}

static void  stream_off(){
	u8 sendbuf_AimOff[3]={0x55,0x00,0xab};
	u8 sendbuf_IllumOff[3]={0x59,0x00,0xa7};
	u8 sendbuf_AcqOff[3]={0x58,0x00,0xa8};
	printk("stream_off .... start \n");
	send_cmd(sendbuf_AimOff,3);
	send_cmd(sendbuf_IllumOff,3);
	send_cmd(sendbuf_AcqOff,3);
	printk("stream_off .... end \n");
}
#endif


static void preview_setting(void)
{
	printk("E  preview_setting \n");
	#ifdef OUT_STREAM_BY_CMD
	stream_on();
	#endif
	
}

static void capture_setting(kal_uint16 currefps)
{
	printk("E  capture_setting \n");
	/* System */
	#ifdef OUT_STREAM_BY_CMD
	//stream_on();
	#endif
	
}

/*----------------------------------------------------------------------------*/
 static int se4500_misc_open(struct inode *inode, struct file *file)
 {  
	cam_pr_debug("entry\n");
	 return 0;
 }
 /*----------------------------------------------------------------------------*/
 static int se4500_misc_release(struct inode *inode, struct file *file)
 {
	cam_pr_debug("entry\n");
	 return 0;
 }
 /*----------------------------------------------------------------------------*/
 static long se4500_misc_ioctl(struct file *file, unsigned int uiCmd,unsigned long ulArg)
 {
	struct i2c_rdwr_ioctl_data	I2CData;
	struct i2c_msg				I2CMsg;
	u8 __user*					pData;
	int							lRetVal;
	
	if ((uiCmd != I2C_RDWR) || !ulArg ){
		cam_pr_debug("moto_ioctl (uiCmd != I2C_RDWR) || !ulArg \n");
		return(-EINVAL);
	}

	// Copy data structure argument from user-space
	if ( copy_from_user(&I2CData, (struct i2c_rdwr_ioctl_data __user*) ulArg, sizeof(I2CData)) ){
		cam_pr_debug("moto_ioctl copy_from_user(&I2CData, (struct i2c_rdwr_ioctl_data __user*) ulArg, sizeof(I2CData))\n");
		return(-EFAULT);
	}

	// Only allow one message at a time
	if ( I2CData.nmsgs != 1 ){
		cam_pr_debug("moto_ioctl I2CData.nmsgs != 1\n");
		return(-EINVAL);
	}

	// Copy the message structure from user-space
	if ( copy_from_user(&I2CMsg, I2CData.msgs, sizeof(struct i2c_msg)) ){
		cam_pr_debug("moto_ioctl copy_from_user(&I2CMsg, I2CData.msgs, sizeof(struct i2c_msg))\n");
		return(-EFAULT);
	}

	lRetVal = 0;
	// Only allow transfers to the SE47xx, limit the size of the message and don't allow received length changes
	if ( (I2CMsg.addr != SE47XX_I2C_ADDR) || (I2CMsg.len > 256) ){
		cam_pr_debug("moto_ioctl (I2CMsg.addr != SE47XX_I2C_ADDR) || (I2CMsg.len > 256) || (I2CMsg.flags & I2C_M_RECV_LEN)\n");
		return(-EINVAL);
	}

	// Map the data buffer from user-space
	pData = (u8 __user*) I2CMsg.buf;
	I2CMsg.buf = memdup_user(pData, I2CMsg.len);
	if ( IS_ERR(I2CMsg.buf) ){
		cam_pr_debug("moto_ioctl IS_ERR(I2CMsg.buf)\n");
		return(PTR_ERR(I2CMsg.buf));
	}
	
	lRetVal = scan_i2c_transfer(&I2CMsg, 1, 100000);//100K
	cam_pr_debug("\n [xielin]moto_ioctl lRetVal %d\n",lRetVal);
	if(lRetVal > 0 && (I2CMsg.flags & I2C_M_RD)){
		if (copy_to_user(pData, I2CMsg.buf, I2CMsg.len) ){
			cam_pr_debug("moto_ioctl copy_to_user(pData, I2CMsg.buf, I2CMsg.len)\n");
			lRetVal = -EFAULT;
		}
	}
	
	kfree(I2CMsg.buf);
	return(lRetVal);
}

/*=======================================================================
  * platform driver for se4500
  *=======================================================================*/
static const struct file_operations se4500_misc_fops =
{
	//.owner			= THIS_MODULE,
	.unlocked_ioctl	= se4500_misc_ioctl,
	.open			= se4500_misc_open,
	.release		= se4500_misc_release,
};

static struct miscdevice se4500_misc_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "sdl_control",
    .fops = &se4500_misc_fops,
};


/*************************************************************************
* FUNCTION
*   SE47XXMIPIGetSensorID
*
* DESCRIPTION
*   This function get the sensor ID 
*
* PARAMETERS
*   *sensorID : return the sensor ID 
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
UINT32 SE47XXMIPIGetSensorID(UINT32 *sensorID) 
{
	struct i2c_msg msg;
	u8 abResp[10];
	u8 abParam[4] = {0x70, 0x00, 0x00, 0x90};
	int retry = 3, ret = 0;
		
	printk("Enter!SE47XXMIPIGetSensorID\n");
    // check if sensor ID correct
	do{
		msg.addr = 0x5c;
		msg.flags = 0;
		msg.buf = abParam;
		msg.len = 4;
		ret = scan_i2c_transfer(&msg, 1, 100000);
		printk("yanhao [SE47XXMIPIGetSensorID] write cmd:(0x%x 0x%x) ret:%d\n", abParam[0], abParam[1], ret);
		if(ret > 0){
			msleep(5);
			msg.flags = I2C_M_RD;
			msg.buf = abResp;
			msg.len = 10;
			ret = scan_i2c_transfer(&msg, 1, 100000);
			printk("yanhao [SE47XXMIPIGetSensorID] read cmd:(0x%x 0x%x) ret:%d\n", abResp[0], abResp[1], ret);
			if(abResp[0] == 0x70 && abResp[1] == 0x80){
				*sensorID = SE47XX_SENSOR_ID;
				break;
			}
		}
		msleep(10);
		retry--;
	}while(retry > 0);

    printk("Sensor ID: 0x%x ", *sensorID);
        
    if (*sensorID != SE47XX_SENSOR_ID) {
        // if Sensor ID is not correct, Must set *sensorID to 0xFFFFFFFF 
        *sensorID = 0xFFFFFFFF;
		cam_pr_debug("incorrectSensor ID: 0x%x ", *sensorID);
        return ERROR_SENSOR_CONNECT_FAIL;
    }
	
	if(isopen == 0){
		isopen = 1;
		scantype = abResp[8]-0x30;
		if(misc_register(&se4500_misc_device)){
			cam_pr_debug("se4500_misc_device register failed\n");
		} 
	}

 	cam_pr_debug("Exit!");
	
    return ERROR_NONE;
}


UINT32 SE47XXMIPIOpen(void)
{
//	 kal_uint16 sensor_id = 0; 
	
	cam_pr_debug("Enter!");
    // check if sensor ID correct
	
 	cam_pr_debug("Exit!");

    return ERROR_NONE;
}   /*  SE47XXMIPIOpen  */

static kal_uint32 SE47XXMIPIClose(void)
{
	cam_pr_debug("E\n");
	#ifdef OUT_STREAM_BY_CMD
	stream_off();
	#endif
	/* No Need to implement this function */
	return ERROR_NONE;
}


static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();

	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */
	return ERROR_NONE;
}


static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			cam_pr_debug("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				imgsensor.current_fps,
				imgsensor_info.cap.max_framerate / 10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_TRUE;

	spin_unlock(&imgsensor_drv_lock);
	capture_setting(imgsensor.current_fps);
	return ERROR_NONE;
}

static kal_uint32 SE47XXMIPIGetResolution(
		MSDK_SENSOR_RESOLUTION_INFO_STRUCT * sensor_resolution)
{
	cam_pr_debug("E\n");
	sensor_resolution->SensorFullWidth =
		imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight =
		imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth =
		imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight =
		imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth =
		imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight =
		imgsensor_info.normal_video.grabwindow_height;


	sensor_resolution->SensorHighSpeedVideoWidth =
		imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight =
		imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth =
		imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight =
		imgsensor_info.slim_video.grabwindow_height;
	return ERROR_NONE;
}

static kal_uint32 SE47XXMIPIGetInfo(enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MSDK_SENSOR_INFO_STRUCT *sensor_info,
	MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	cam_pr_debug("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;
	sensor_info->SensorResetActiveHigh = FALSE;
	sensor_info->SensorResetDelayCount = 5;

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame =
		imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame =
		imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame =
		imgsensor_info.slim_video_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent =
		imgsensor_info.isp_driving_current;
/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame =
		imgsensor_info.ae_shut_delay_frame;
/* The frame of setting sensor gain */
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	/*The frame of setting sensor gain*/
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine =
		imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum =
		imgsensor_info.sensor_mode_num;

	sensor_info->SensorMIPILaneNumber =
		imgsensor_info.mipi_lane_num;
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

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	return ERROR_NONE;
}	/*	normal_video   */
static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	return ERROR_NONE;
}    /*    hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	return ERROR_NONE;
}    /*    slim_video     */


static kal_uint32 SE47XXMIPIControl(enum MSDK_SCENARIO_ID_ENUM scenario_id,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	printk("yanhao scenario_id = %d\n", scenario_id);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		printk("preview\n");
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_CAMERA_ZSD:
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
		cam_pr_debug("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}

static kal_uint32 set_max_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	cam_pr_debug("scenario_id = %d, framerate = %d\n",
			scenario_id, framerate);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10 /
			imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
				imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
//		if (imgsensor.frame_length > imgsensor.shutter)
//			set_dummy();
	break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
	    frame_length = imgsensor_info.normal_video.pclk /
			framerate * 10 / imgsensor_info.normal_video.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.normal_video.framelength) ?
		(frame_length - imgsensor_info.normal_video.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.normal_video.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
//		if (imgsensor.frame_length > imgsensor.shutter)
//			set_dummy();
	break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps ==
				imgsensor_info.cap1.max_framerate) {
		frame_length = imgsensor_info.cap1.pclk / framerate * 10 /
				imgsensor_info.cap1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length >
			imgsensor_info.cap1.framelength) ?
			(frame_length - imgsensor_info.cap1.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.cap1.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps !=
				imgsensor_info.cap.max_framerate)
			printk("fps %d fps not support,use cap: %d fps!\n",
			framerate, imgsensor_info.cap.max_framerate/10);
			frame_length = imgsensor_info.cap.pclk /
				framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length >
				imgsensor_info.cap.framelength) ?
			(frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length =
				imgsensor_info.cap.framelength +
				imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
//		if (imgsensor.frame_length > imgsensor.shutter)
//			set_dummy();
	break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	    frame_length = imgsensor_info.hs_video.pclk /
			framerate * 10 / imgsensor_info.hs_video.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.hs_video.framelength) ? (frame_length -
			imgsensor_info.hs_video.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.hs_video.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
	
	break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
	    frame_length = imgsensor_info.slim_video.pclk /
			framerate * 10 / imgsensor_info.slim_video.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.slim_video.framelength) ? (frame_length -
			imgsensor_info.slim_video.framelength) : 0;
	    imgsensor.frame_length =
			imgsensor_info.slim_video.framelength +
			imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
//		if (imgsensor.frame_length > imgsensor.shutter)
//			set_dummy();
	break;
	default:  //coding with  preview scenario by default
	    frame_length = imgsensor_info.pre.pclk / framerate * 10 /
						imgsensor_info.pre.linelength;
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.dummy_line = (frame_length >
			imgsensor_info.pre.framelength) ?
			(frame_length - imgsensor_info.pre.framelength) : 0;
	    imgsensor.frame_length = imgsensor_info.pre.framelength +
				imgsensor.dummy_line;
	    imgsensor.min_frame_length = imgsensor.frame_length;
	    spin_unlock(&imgsensor_drv_lock);
//		if (imgsensor.frame_length > imgsensor.shutter)
//			set_dummy();
	    printk("error scenario_id = %d, we use preview scenario\n",
				scenario_id);
	break;
	}
	return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id,
	MUINT32 *framerate)
{
	cam_pr_debug("scenario_id = %d\n", scenario_id);

	if (framerate == NULL) {
		cam_pr_debug("---- framerate is NULL ---- check here\n");
		return ERROR_NONE;
	}
	spin_lock(&imgsensor_drv_lock);
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
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}



static kal_uint32 SE47XXMIPIFeatureControl(
	MSDK_SENSOR_FEATURE_ENUM feature_id,
	UINT8 *feature_para,
	UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;

	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

	if (feature_para == NULL) {
		cam_pr_debug(" ---- %s ---- error params1\n", __func__);
		return ERROR_NONE;
	}

	cam_pr_debug("feature_id = %d\n", feature_id);

	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		cam_pr_debug("feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n",
			imgsensor.pclk, imgsensor.current_fps);
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		break;
	case SENSOR_FEATURE_SET_GAIN:
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		SE47XXMIPIGetSensorID(feature_return_para_32); 
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *feature_data,
			*(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *(feature_data),
			(MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		cam_pr_debug("current fps :%d\n", (UINT32) *feature_data_32);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data_32;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		cam_pr_debug("ihdr enable :%d\n", (BOOL) * feature_data_32);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		cam_pr_debug("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n",
			(UINT32) *feature_data);

		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)
			(uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[1],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[2],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[3],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[4],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[0],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		cam_pr_debug("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16) *feature_data, (UINT16) *(feature_data + 1),
			(UINT16) *(feature_data + 2));
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
		*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = rate;
	}
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		cam_pr_debug("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		cam_pr_debug("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		break;
	case SENSOR_FEATURE_GET_PIXEL_RATE:
	{
		kal_uint32 rate;

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			rate = (imgsensor_info.cap.pclk /
			       (imgsensor_info.cap.linelength - 80))*
			       imgsensor_info.cap.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			rate = (imgsensor_info.normal_video.pclk /
			       (imgsensor_info.normal_video.linelength - 80))*
			       imgsensor_info.normal_video.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			rate = (imgsensor_info.hs_video.pclk /
			       (imgsensor_info.hs_video.linelength - 80))*
			       imgsensor_info.hs_video.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			rate = (imgsensor_info.slim_video.pclk /
			       (imgsensor_info.slim_video.linelength - 80))*
			       imgsensor_info.slim_video.grabwindow_width;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			rate = (imgsensor_info.pre.pclk /
			       (imgsensor_info.pre.linelength - 80))*
			       imgsensor_info.pre.grabwindow_width;
			break;
		}
		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
	}
		break;
	default:
		break;
	}

	return ERROR_NONE;
}



static struct SENSOR_FUNCTION_STRUCT  SensorFuncSE47XXMIPI=
{
    SE47XXMIPIOpen,
    SE47XXMIPIGetInfo,
    SE47XXMIPIGetResolution,
    SE47XXMIPIFeatureControl,
    SE47XXMIPIControl,
    SE47XXMIPIClose
};

UINT32 SE47XXMIPISensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	cam_pr_debug("E\n");
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &SensorFuncSE47XXMIPI;

	return ERROR_NONE;
}
