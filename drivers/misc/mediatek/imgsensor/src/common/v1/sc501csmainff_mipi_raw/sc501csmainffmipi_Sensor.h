/*****************************************************************************
 *
 * Filename:
 * ---------
 *     SC501CSMAINFFmipi_Sensor.h
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     CMOS sensor header file
 *
 ****************************************************************************/
#ifndef _SC501CSMAINFFMIPI_SENSOR_H
#define _SC501CSMAINFFMIPI_SENSOR_H

/* SENSOR MIRROR FLIP INFO */
#define SC501CSMAINFF_MIRROR_FLIP_ENABLE    1
#if SC501CSMAINFF_MIRROR_FLIP_ENABLE
#define SC501CSMAINFF_MIRROR 0x66
#else
#define SC501CSMAINFF_MIRROR 0x00
#endif

/* SENSOR PRIVATE INFO FOR GAIN SETTING */
#define SC501CSMAINFF_SENSOR_GAIN_BASE             0x400
#define SC501CSMAINFF_SENSOR_GAIN_MAX              (1575 * SC501CSMAINFF_SENSOR_GAIN_BASE / 100)
#define SC501CSMAINFF_SENSOR_GAIN_MAX_VALID_INDEX  4
#define SC501CSMAINFF_SENSOR_GAIN_MAP_SIZE         6
#define SC501CSMAINFF_SENSOR_DGAIN_BASE            0x400

/*prize add by zhuzhengjiang for mtk otp 20210730 start*/
#define SC501CSMAINFF_OTP_FOR_MTK       1
#define SC501CSMAINFF_OTP_GROUP1_PAGE1_ADDR_OFFSET        0x80EC
#define SC501CSMAINFF_OTP_GROUP1_PAGE1_DATA_SIZE          0x708 // 0X80EC~0x87F3

#define SC501CSMAINFF_OTP_GROUP1_PAGE2_ADDR_OFFSET        0x8800
#define SC501CSMAINFF_OTP_GROUP1_PAGE2_DATA_SIZE          0x69 // 0X8800~0x8868

#define SC501CSMAINFF_OTP_GROUP2_INFO_FLAG_ADDR_OFFSET    0x8874
#define SC501CSMAINFF_OTP_DATA_SIZE                0x771 //0x8874~0x8FE4


#define SC501CSMAINFF_OTP_AWB_FLAG_ADDR_OFFSET     0x80F4
#define SC501CSMAINFF_OTP_AWB_DATA_ADDR_OFFSET     0x80F5
#define SC501CSMAINFF_OTP_AWB_GROUP_OFFSET         0x788
#define SC501CSMAINFF_OTP_AWB_DATA_SIZE            9

//#define SC501CSMAINFF_OTP_LSC_FLAG_ADDR_OFFSET     0x1880
#define SC501CSMAINFF_OTP_LSC_GROUP1_DATA0_ADDR_OFFSET     0x8103
#define SC501CSMAINFF_OTP_LSC_GROUP1_DATA1_ADDR_OFFSET     0x8800
#define SC501CSMAINFF_OTP_LSC_DATA0_SIZE            	   0x6F0

#define SC501CSMAINFF_OTP_LSC_GROUP2_DATA_ADDR_OFFSET      0x888B
#define SC501CSMAINFF_OTP_LSC_DATA_SIZE            1868
/*prize add by zhuzhengjiang for mtk otp 20210730 end*/

enum {
	IMGSENSOR_MODE_INIT,
	IMGSENSOR_MODE_PREVIEW,
	IMGSENSOR_MODE_CAPTURE,
	IMGSENSOR_MODE_VIDEO,
	IMGSENSOR_MODE_HIGH_SPEED_VIDEO,
	IMGSENSOR_MODE_SLIM_VIDEO,
};

struct imgsensor_mode_struct {
	kal_uint32 pclk;
	kal_uint32 linelength;
	kal_uint32 framelength;
	kal_uint8 startx;
	kal_uint8 starty;
	kal_uint16 grabwindow_width;
	kal_uint16 grabwindow_height;
	kal_uint32 mipi_pixel_rate;
	kal_uint8 mipi_data_lp2hs_settle_dc;
	kal_uint16 max_framerate;
};

/* SENSOR PRIVATE STRUCT FOR VARIABLES */
struct imgsensor_struct {
	kal_uint8 mirror;               /* mirrorflip information */

	kal_uint8 sensor_mode;          /* record IMGSENSOR_MODE enum value */

	kal_uint32 shutter;             /* current shutter */
	kal_uint16 gain;                /* current gain */

	kal_uint32 pclk;                /* current pclk */

	kal_uint32 frame_length;        /* current framelength */
	kal_uint32 line_length;         /* current linelength */

	kal_uint32 min_frame_length;    /* current min  framelength to max framerate */
	kal_uint16 dummy_pixel;         /* current dummypixel */
	kal_uint16 dummy_line;          /* current dummline */

	kal_uint16 current_fps;         /* current max fps */
	kal_bool   autoflicker_en;      /* record autoflicker enable or disable */
	kal_bool test_pattern;          /* record test pattern mode or not */
enum MSDK_SCENARIO_ID_ENUM current_scenario_id; /* current scenario id */
	kal_uint8  ihdr_en;             /* ihdr enable or disable */
	kal_uint8 i2c_write_id;         /* record current sensor's i2c write id */
};

/* SENSOR PRIVATE STRUCT FOR CONSTANT */
struct imgsensor_info_struct {
	kal_uint32 sensor_id;               /* record sensor id defined in Kd_imgsensor.h */
	kal_uint32 checksum_value;          /* checksum value for Camera Auto Test */
	struct imgsensor_mode_struct pre;          /* preview scenario relative information */
	struct imgsensor_mode_struct cap;          /* capture scenario relative information */
	/* capture for PIP 24fps relative information, capture1 mode must use same framelength,
	linelength with Capture mode for shutter calculate */
	struct imgsensor_mode_struct cap1;
	struct imgsensor_mode_struct normal_video; /* normal video  scenario relative information */
	struct imgsensor_mode_struct hs_video;     /* high speed video scenario relative information */
	struct imgsensor_mode_struct slim_video;   /* slim video for VT scenario relative information */

	kal_uint8  ae_shut_delay_frame;     /* shutter delay frame for AE cycle */
	kal_uint8  ae_sensor_gain_delay_frame; /* sensor gain delay frame for AE cycle */
	kal_uint8  ae_ispGain_delay_frame;  /* isp gain delay frame for AE cycle */
	kal_uint8  ihdr_support;            /* 1, support; 0,not support */
	kal_uint8  ihdr_le_firstline;       /* 1,le first ; 0, se first */
	kal_uint8  sensor_mode_num;         /* support sensor mode num */

	kal_uint8  cap_delay_frame;        /* enter capture delay frame num */
	kal_uint8  pre_delay_frame;        /* enter preview delay frame num */
	kal_uint8  video_delay_frame;      /* enter video delay frame num */
	kal_uint8  hs_video_delay_frame;   /* enter high speed video  delay frame num */
	kal_uint8  slim_video_delay_frame; /* enter slim video delay frame num */

	kal_uint8  margin;                /* sensor framelength & shutter margin */
	kal_uint32 min_shutter;           /* min shutter */
	kal_uint32 max_frame_length;      /* max framelength by sensor register's limitation */

	kal_uint8  isp_driving_current;   /* mclk driving current */
	kal_uint8  sensor_interface_type; /* sensor_interface_type */
	/* 0,MIPI_OPHY_NCSI2; 1,MIPI_OPHY_CSI2, default is NCSI2, don't modify this para */
	kal_uint8  mipi_sensor_type;
	/* 0, high speed signal auto detect; 1, use settle delay,unit is ns,
	default is auto detect, don't modify this para */
	kal_uint8  mipi_settle_delay_mode;
	kal_uint8  sensor_output_dataformat;/* sensor output first pixel color */
	kal_uint8  mclk;                  /* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */

	kal_uint8  mipi_lane_num;         /* mipi lane num */
	kal_uint8  i2c_addr_table[5];     /* record sensor support all write id addr, only supprt 4must end with 0xff */
};

/*prize add by zhuzhengjiang for mtk otp 20210730 start*/
#if SC501CSMAINFF_OTP_FOR_MTK
struct otp_awb_info_struct {
	kal_uint8  awb_flag;
	kal_uint8  unit_r;
	kal_uint8  unit_gr;
	kal_uint8  unit_gb;
	kal_uint8  unit_b;
	kal_uint8  golden_r;
	kal_uint8  golden_gr;
	kal_uint8  golden_gb;
	kal_uint8  golden_b;
};

struct imgsensor_otp_info_struct {
	kal_uint8  info_flag;
	kal_uint8  supply_id;
	kal_uint8  module_id;
	kal_uint8  lends_id;
	kal_uint8  vcm_ld;
	kal_uint8  driver_id;
	kal_uint8  module_version;
	kal_uint8  software_version;
	kal_uint8  year;
	kal_uint8  month;
	kal_uint8  day;
	kal_uint8  reserved0;
	kal_uint8  reserved1;
	kal_uint8  checksum_of_info;
	struct otp_awb_info_struct awb;
	kal_uint8  lsc_flag;
	kal_uint8  lsc[1868];
	kal_uint8  checksum_of_lsc;
};
#endif
/*prize add by zhuzhengjiang for mtk otp 20210730 end*/
extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
extern int iWriteReg(u16 a_u2Addr, u32 a_u4Data, u32 a_u4Bytes, u16 i2cId);

#endif
