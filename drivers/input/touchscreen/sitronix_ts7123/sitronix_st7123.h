

#define ST_CHIPID		0x83


/* common */
#define ST_DISPLAY_DUMP_OFF		0x1D7FE
#define ST_DISPLAY_SIZE			0x800

/* flash */
#define ST_FLASH_PAGE_SIZE		4096
#define ST_FLASH_SIZE			0x20000
#define ST_ICP_DISPLAY_FLASH_OFF	0x1D7FE

/* ICP */
#define ST_ADDR_MODE_I2C_ADDR		0x28

/* HDL */
#define ST_RAM_MODE_RETRY_MAX		3
#define ST_FW_LEN					0x010000
#define ST_HDL_PRAM_ADDR			0x7F0000
#define ST_HDL_URAM_ADDR			0x000000
#define ST_HDL_DISPLAY_RAM_ADDR		0x030000

/* IC register */
#define ST_MISC_INFO_SWU_FLAG	0x80
#define ST_MISC_INFO_PROX_FLAG	0x20



typedef enum {
	ST_SWU_GESTURE_NO				= 0,
	ST_SWU_GESTURE_RIGHT                         = 0xC0,
	ST_SWU_GESTURE_LEFT                          = 0xC1,
	ST_SWU_GESTURE_DOWN                          = 0xC2,
	ST_SWU_GESTURE_UP                            = 0xC3,	
	ST_SWU_GESTURE_ARROW_UP						 = 0xC4,
	ST_SWU_GESTURE_ARROW_RIGHT					 = 0xC5,
	ST_SWU_GESTURE_ARROW_DOWN					 = 0xC6,
	ST_SWU_GESTURE_ARROW_LEFT					 = 0xC7,
	ST_SWU_GESTURE_TWO_FINGER_DOWN				 = 0xC8,
	ST_SWU_GESTURE_DOUBLECLICK                   = 0xB0,
	ST_SWU_GESTURE_SINGLECLICK                   = 0xB1,
	ST_SWU_GESTURE_C                             = 0x63,
	ST_SWU_GESTURE_E                             = 0x65,
	ST_SWU_GESTURE_M                             = 0x6D,
	ST_SWU_GESTURE_O                             = 0x6F,
	ST_SWU_GESTURE_S                             = 0x73,
	ST_SWU_GESTURE_W                             = 0x77,
	ST_SWU_GESTURE_V                             = 0x76,
	ST_SWU_GESTURE_Z                             = 0x7A
} SITRONIX_SWU_ID;


typedef enum {
	ST_MODE_SWU		=	1,
	ST_MODE_GLOVE		=	2,
	ST_MODE_CHARGE		=	3,
	ST_MODE_JITTERSUPPRESS	=	4,
	ST_MODE_PALM		=	5,
	ST_MODE_HEADPHONE	=	6,
	ST_MODE_GRIP		=	7,
	ST_MODE_GRIP_ROTATE	=	8,
	ST_MODE_SIZE		=	9,
	ST_MODE_RESTORE_START	=	2,
	ST_MODE_SWITCH_OFF	=	0,
	ST_MODE_SWITCH_ON	=	0x80
} SITRONIX_MODE_SWITCH;

