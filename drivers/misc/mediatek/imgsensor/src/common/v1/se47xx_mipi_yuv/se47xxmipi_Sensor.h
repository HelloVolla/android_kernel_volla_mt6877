/*****************************************************************************
 *
 * Filename:
 * ---------
 *   SE47XXMIPI_Sensor.h
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   CMOS sensor header file
 *
 ****************************************************************************/
#ifndef _SE47XXMIPI_SENSOR_H
#define _SE47XXMIPI_SENSOR_H

/* SENSOR PREVIEW/CAPTURE VT CLOCK */
#define SE47XXMIPI_PREVIEW_CLK                      74250000

#define SE47XXMIPI_VIDEO_START_X                   (0)
#define SE47XXMIPI_VIDEO_START_Y                   (0)
#define SE47XXMIPI_IMAGE_SENSOR_FULL_WIDTH         (1360)
#define SE4710YUV_IMAGE_SENSOR_FULL_HEIGHT		  (800)
#define SE4750YUV_IMAGE_SENSOR_FULL_HEIGHT		  (960)

// SE47XX commands
#define SE45OP_WRITEREGISTER		0x50
#define SE45OP_READREGISTER			0x51
#define SE45OP_AIM					0x55
#define SE45OP_AIMONEXPOSURE		0x56
#define SE45OP_RESET				0x57
#define SE45OP_ARMACQUISITION		0x58
#define SE45OP_ILLUMDURINGEXPOSURE	0x59
#define SE45OP_ACQUISITIONMODE		0x5B
#define SE45OP_FRAMERATE			0x5E
#define SE45OP_GETPARAM				0x70
#define SE45OP_SETPARAM				0x71
#define SE45OP_FATMODE				0x78
#define SE45OP_AUTOPOWERREDUCTION	0x74
#define SE45OP_TIMETOLOWPOWER		0x75
#define SE45OP_WRITESCRIPT			0x76
#define SE45OP_EXECSCRIPT			0x77
#define SE45OP_PICKLIST				0x7B
#define SE45OP_ILLUMPOWER			0xF0
#define SE45OP_EXTILLUMMODE			0xF1
#define SE45OP_AIMPOWER				0xF3
#define SE45OP_WRSENSOR 			0X50
#define SE45OP_RDSENSOR				0x51
#define SE45OP_INTERFACETYPE		0x86

// SE47XX command status values
#define SE45STS_ACK					0x80

// SE45OP_ACQUISITIONMODE option values
#define ACQMODE_DECODE				0
#define ACQMODE_IMAGE				1
#define ACQMODE_MOTION				2
#define ACQMODE_AIMCAPTURE			3
#define SE45_MAX_CMD_LEN			48

/* SENSOR READ/WRITE ID */
#define SE47XX_I2C_ADDR				0x5C

enum {
	IMGSENSOR_MODE_INIT,
	IMGSENSOR_MODE_PREVIEW,
	IMGSENSOR_MODE_CAPTURE,
	IMGSENSOR_MODE_VIDEO,
	IMGSENSOR_MODE_HIGH_SPEED_VIDEO,
	IMGSENSOR_MODE_SLIM_VIDEO,
};

struct imgsensor_mode_struct {
	kal_uint32 pclk;                /* record different mode's pclk */
	kal_uint32 linelength;          /* record different mode's linelength */
	kal_uint32 framelength;
	kal_uint8  startx;
	kal_uint8  starty;
	kal_uint16 grabwindow_width;
	kal_uint16 grabwindow_height;
	kal_uint32 mipi_pixel_rate;
	kal_uint8  mipi_data_lp2hs_settle_dc;
	/* following for GetDefaultFramerateByScenario() */
	kal_uint16 max_framerate;
};

/* SENSOR PRIVATE STRUCT FOR VARIABLES */
struct imgsensor_struct {
	kal_uint8 mirror;
	kal_uint8 sensor_mode;
	kal_uint32 shutter;
	kal_uint16 gain;
	kal_uint32 pclk;
	kal_uint32 frame_length;
	kal_uint32 line_length;
	kal_uint32 min_frame_length;
	kal_uint16 dummy_pixel;
	kal_uint16 dummy_line;
	kal_uint16 current_fps;
	kal_bool   autoflicker_en;
	kal_bool   test_pattern;
	enum MSDK_SCENARIO_ID_ENUM current_scenario_id;
	kal_uint8  ihdr_en;
	kal_uint8 i2c_write_id;
};

/* SENSOR PRIVATE STRUCT FOR CONSTANT */
struct imgsensor_info_struct {
	kal_uint32 sensor_id;
	kal_uint32 checksum_value;
	struct imgsensor_mode_struct pre;
	struct imgsensor_mode_struct cap;
	struct imgsensor_mode_struct cap1;
	struct imgsensor_mode_struct cap2;
	struct imgsensor_mode_struct normal_video;
	struct imgsensor_mode_struct hs_video;
	struct imgsensor_mode_struct slim_video;
	kal_uint8  ae_shut_delay_frame;
	kal_uint8  ae_sensor_gain_delay_frame;
	kal_uint8  ae_ispGain_delay_frame;
	kal_uint8  ihdr_support;
	kal_uint8  ihdr_le_firstline;
	kal_uint8  sensor_mode_num;
	kal_uint8  cap_delay_frame;
	kal_uint8  pre_delay_frame;
	kal_uint8  video_delay_frame;
	kal_uint8  hs_video_delay_frame;
	kal_uint8  slim_video_delay_frame;
	kal_uint8  margin;
	kal_uint32 min_shutter;
	kal_uint32 max_frame_length;
	kal_uint8  isp_driving_current;
	kal_uint8  sensor_interface_type;
	kal_uint8  mipi_sensor_type;
	kal_uint8  mipi_settle_delay_mode;
	kal_uint8  sensor_output_dataformat;
	kal_uint8  mclk;
	kal_uint8  mipi_lane_num;
	kal_uint8  i2c_addr_table[5];
};

#endif 
