#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"

#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)            (lcm_util.set_reset_pin((v)))
#define MDELAY(n)                   (lcm_util.mdelay(n))

/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)     lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)        lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)                                       lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)                   lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)                                        lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)                lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
#define LCM_DSI_CMD_MODE	0
#define FRAME_WIDTH  										 (720)
#define FRAME_HEIGHT 										 (1600)

#define REGFLAG_DELAY             							 0xFFA
#define REGFLAG_UDELAY             							 0xFFB
#define REGFLAG_PORT_SWAP									 0xFFC
#define REGFLAG_END_OF_TABLE      							 0xFFD   // END OF REGISTERS MARKER

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */

struct LCM_setting_table
{
    unsigned int cmd;
    unsigned char count;
    unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] =
{
    {0x28, 1, {0x00}},
    {REGFLAG_DELAY, 10, {}},
    {0x10, 1, {0x00}},
    {REGFLAG_DELAY, 60, {} },
};

static struct LCM_setting_table lcm_initialization_setting[] =
{
#if 1
{0xFF,03,{0x98,0x82,0x01}},  //3H 
{0x00,01,{0x47}},  //STVA    
{0x01,01,{0x32}},  //STVA Duty 4H
{0x02,01,{0x00}},  //45%   CLK duty 
{0x03,01,{0x00}},  //45%   CLK duty
{0x04,01,{0x04}},  //STVB
{0x05,01,{0x32}},  //STV Duty 4H
{0x06,01,{0x00}},  //45%   CLK duty
{0x07,01,{0x00}},  //45%   CLK duty
{0x08,01,{0x85}},  //CLK RISE    
{0x09,01,{0x04}},  //CLK FALL   
{0x0a,01,{0x72}}, //CLK Duty 4H 
{0x0b,01,{0x00}},
{0x0c,01,{0x00}},  //45%   CLK duty
{0x0d,01,{0x00}}, //45%   CLK duty   
{0x0e,01,{0x00}},
{0x0f,01,{0x00}},
{0x28,01,{0x48}},  //STCH1   
{0x29,01,{0x88}},
{0x2A,01,{0x48}},   //STCH2 
{0x2B,01,{0x88}},

//FW
{0x31,01,{0x0C}},     // RST_L
{0x32,01,{0x02}},     // VGL
{0x33,01,{0x02}},     // VGL
{0x34,01,{0x23}},     // GLV
{0x35,01,{0x02}},     // VGL_L
{0x36,01,{0x08}},     // STV1_L
{0x37,01,{0x0A}},     // STV2_L
{0x38,01,{0x06}},    // VDD
{0x39,01,{0x06}},     // VDD
{0x3A,01,{0x10}},     // CLK1_L
{0x3B,01,{0x10}},     // CLK1_L
{0x3C,01,{0x12}},     // CLK2_L
{0x3D,01,{0x12}},     // CLK2_L
{0x3E,01,{0x14}},     // CK1B_L
{0x3F,01,{0x14}},     // CK1B_L
{0x40,01,{0x16}},     // CK2B_L
{0x41,01,{0x16}},     // CK2B_L
{0x42,01,{0x07}},    // 
{0x43,01,{0x07}},     // 
{0x44,01,{0x07}},     // 
{0x45,01,{0x07}},     // 
{0x46,01,{0x07}},     // 
//FW
{0x47,01,{0x0D}},     // RST_R
{0x48,01,{0x02}},     // VGL
{0x49,01,{0x02}},     // VGL
{0x4A,01,{0x23}},     // GLV
{0x4B,01,{0x02}},     // VGL_R
{0x4C,01,{0x09}},     // STV1_R
{0x4D,01,{0x0B}},     // STV2_R
{0x4E,01,{0x06}},     // VDD
{0x4F,01,{0x06}},     // VDD
{0x50,01,{0x11}},     // CLK1_R
{0x51,01,{0x11}},     // CLK1_R
{0x52,01,{0x13}},    // CLK2_R
{0x53,01,{0x13}},     // CLK2_R
{0x54,01,{0x15}},     // CK1B_R
{0x55,01,{0x15}},     // CK1B_R
{0x56,01,{0x17}},     // CK2B_R
{0x57,01,{0x17}},     // CK2B_R
{0x58,01,{0x07}},     // 
{0x59,01,{0x07}},     // 
{0x5A,01,{0x07}},     // 
{0x5B,01,{0x07}},     // 
{0x5C,01,{0x07}},     // 
//BW
{0x61,01,{0x0C}},     // RST_L
{0x62,01,{0x02}},     // VGL
{0x63,01,{0x02}},     // VGL
{0x64,01,{0x23}},    // GLV
{0x65,01,{0x02}},     // VGL_L
{0x66,01,{0x08}},     // STV1_L
{0x67,01,{0x0A}},     // STV2_L
{0x68,01,{0x06}},     // VDD
{0x69,01,{0x06}},     // VDD
{0x6A,01,{0x10}},     // CLK1_L
{0x6B,01,{0x10}},     // CLK1_L
{0x6C,01,{0x12}},     // CLK2_L
{0x6D,01,{0x12}},     // CLK2_L
{0x6E,01,{0x14}},     // CK1B_L
{0x6F,01,{0x14}},     // CK1B_L
{0x70,01,{0x16}},     // CK2B_L
{0x71,01,{0x16}},     // CK2B_L
{0x72,01,{0x07}},     // 
{0x73,01,{0x07}},     // 
{0x74,01,{0x07}},     // 
{0x75,01,{0x07}},     // 
{0x76,01,{0x07}},     // 
//BW
{0x77,01,{0x0D}},     // RST_R
{0x78,01,{0x02}},     // VGL
{0x79,01,{0x02}},     // VGL
{0x7A,01,{0x23}},     // GLV
{0x7B,01,{0x02}},     // VGL_R
{0x7C,01,{0x09}},     // STV1_R
{0x7D,01,{0x0B}},     // STV2_R
{0x7E,01,{0x06}},     // VDD
{0x7F,01,{0x06}},     // VDD
{0x80,01,{0x11}},     // CLK1_R
{0x81,01,{0x11}},     // CLK1_R
{0x82,01,{0x13}},     // CLK2_R
{0x83,01,{0x13}},     // CLK2_R
{0x84,01,{0x15}},     // CK1B_R
{0x85,01,{0x15}},     // CK1B_R
{0x86,01,{0x17}},     // CK2B_R
{0x87,01,{0x17}},     // CK2B_R
{0x88,01,{0x07}},     // 
{0x89,01,{0x07}},     // 
{0x8A,01,{0x07}},     // 
{0x8B,01,{0x07}},     // 
{0x8C,01,{0x07}},     // 
{0xB0,01,{0x33}},
{0xB1,01,{0x33}},
{0xB2,01,{0x00}},
{0xD0,01,{0x01}},
{0xD1,01,{0x00}},
{0xE2,01,{0x00}},
{0xE6,01,{0x22}},
{0xE7,01,{0x54}},

// RTN. Internal VBP, Internal VFP
{0xFF,03,{0x98,0x82,0x02}},
{0xF1,01,{0x1C}},    // Tcon ESD option
{0x4B,01,{0x5A}},    // line_chopper
{0x50,01,{0xCA}},    // line_chopper
{0x51,01,{0x00}},     // line_chopper
{0x06,01,{0x8F}},     // Internal Line Time (RTN)
{0x0B,01,{0xA0}},     // Internal VFP[9]
{0x0C,01,{0x00}},     // Internal VFP[8]
{0x0D,01,{0x14}},     // Internal VBP
{0x0E,01,{0xE6}},     // Internal VFP
{0x4E,01,{0x11}},     // SRC BIAS
// REGISTER,4D,01,CE     // Power Saving Off

// Power Setting
{0xFF,03,{0x98,0x82,0x05}},
{0x03,01,{0x01}},    // Vcom
{0x04,01,{0x2C}},    // Vcom
{0x58,01,{0x61}},    // VGL 2x
{0x63,01,{0x8D}},     // GVDDN = -5.3V
{0x64,01,{0x8D}},     // GVDDP = 5.3V
{0x68,01,{0xA1}},     // VGHO = 15V
{0x69,01,{0xA7}},     // VGH = 16V
{0x6A,01,{0x79}},     // VGLO = -10V
{0x6B,01,{0x6B}},     // VGL = -11V
{0x85,01,{0x37}},      // HW RESET option
{0x46,01,{0x00}},      // LVD HVREG option

// Resolution
{0xFF,03,{0x98,0x82,0x06}},
{0xD9,01,{0x1F}},     // 4Lane
// REGISTER,08,01,00     // PLL
{0xC0,01,{0x40}},     // NL = 1600
{0xC1,01,{0x16}},     // NL = 1600

// Gamma Register
{0xFF,03,{0x98,0x82,0x08}},								
{0xE0,27,{0x00,0x24,0x78,0xAD,0xEF,0x55,0x23,0x4B,0x7B,0xA1,0xA9,0xDE,0x10,0x3D,0x67,0xEA,0x93,0xC6,0xE5,0x0B,0xFF,0x2C,0x55,0x86,0xB3,0x03,0xEC}},							
{0xE1,27,{0x00,0x24,0x78,0xAD,0xEF,0x55,0x23,0x4B,0x7B,0xA1,0xA9,0xDE,0x10,0x3D,0x67,0xEA,0x93,0xC6,0xE5,0x0B,0xFF,0x2C,0x55,0x86,0xB3,0x03,0xEC}},						

// OSC Auto Trim Setting
{0xFF,03,{0x98,0x82,0x0B}},
{0x9A,01,{0x44}},
{0x9B,01,{0x81}},
{0x9C,01,{0x03}},
{0x9D,01,{0x03}},
{0x9E,01,{0x70}},
{0x9F,01,{0x70}},
{0xAB,01,{0xE0}},     // AutoTrimType

{0xFF,03,{0x98,0x82,0x0E}},
{0x11,01,{0x10}},     // TSVD Rise position
{0x13,01,{0x10}},     // LV mode TSHD Rise position
{0x00,01,{0xA0}},      // LV mode

{0xFF,03,{0x98,0x82,0x00}},
{0x35,1,{0x00}},
#else
{0xFF,3,{0x98,0x82,0x01}},
{0x00,1,{0x40}},
{0x01,1,{0x00}},
{0x02,1,{0x10}},
{0x03,1,{0x00}},
{0x08,1,{0x01}},
{0x09,1,{0x05}},
{0x0a,1,{0x40}},
{0x0b,1,{0x00}},
{0x0c,1,{0x10}},
{0x0d,1,{0x10}},
{0x0e,1,{0x00}},
{0x0f,1,{0x00}},
{0x10,1,{0x00}},
{0x11,1,{0x00}},
{0x12,1,{0x00}},
{0x31,1,{0x07}},
{0x32,1,{0x07}},
{0x33,1,{0x07}},
{0x34,1,{0x02}},
{0x35,1,{0x02}},
{0x36,1,{0x02}},
{0x37,1,{0x02}},
{0x38,1,{0x2B}},
{0x39,1,{0x2A}},
{0x3A,1,{0x14}},
{0x3B,1,{0x13}},
{0x3C,1,{0x12}},
{0x3D,1,{0x11}},
{0x3E,1,{0x10}},
{0x3F,1,{0x08}},
{0x40,1,{0x07}},
{0x41,1,{0x07}},
{0x42,1,{0x07}},
{0x43,1,{0x02}},
{0x44,1,{0x02}},
{0x45,1,{0x06}},
{0x46,1,{0x06}},
{0x47,1,{0x07}},
{0x48,1,{0x07}},
{0x49,1,{0x07}},
{0x4A,1,{0x02}},
{0x4B,1,{0x02}},
{0x4C,1,{0x02}},
{0x4D,1,{0x02}},
{0x4E,1,{0x2B}},
{0x4F,1,{0x2A}},
{0x50,1,{0x14}},
{0x51,1,{0x13}},
{0x52,1,{0x12}},
{0x53,1,{0x11}},
{0x54,1,{0x10}},
{0x55,1,{0x08}},
{0x56,1,{0x07}},
{0x57,1,{0x07}},
{0x58,1,{0x07}},
{0x59,1,{0x02}},
{0x5A,1,{0x02}},
{0x5B,1,{0x06}},
{0x5C,1,{0x06}},
{0x61,1,{0x07}},
{0x62,1,{0x07}},
{0x63,1,{0x07}},
{0x64,1,{0x02}},
{0x65,1,{0x02}},
{0x66,1,{0x02}},
{0x67,1,{0x02}},
{0x68,1,{0x2B}},
{0x69,1,{0x2A}},
{0x6A,1,{0x10}},
{0x6B,1,{0x11}},
{0x6C,1,{0x12}},
{0x6D,1,{0x13}},
{0x6E,1,{0x14}},
{0x6F,1,{0x08}},
{0x70,1,{0x07}},
{0x71,1,{0x07}},
{0x72,1,{0x07}},
{0x73,1,{0x02}},
{0x74,1,{0x02}},
{0x75,1,{0x06}},
{0x76,1,{0x06}},
{0x77,1,{0x07}},
{0x78,1,{0x07}},
{0x79,1,{0x07}},
{0x7A,1,{0x02}},
{0x7B,1,{0x02}},
{0x7C,1,{0x02}},
{0x7D,1,{0x02}},
{0x7E,1,{0x2B}},
{0x7F,1,{0x2A}},
{0x80,1,{0x10}},
{0x81,1,{0x11}},
{0x82,1,{0x12}},
{0x83,1,{0x13}},
{0x84,1,{0x14}},
{0x85,1,{0x08}},
{0x86,1,{0x07}},
{0x87,1,{0x07}},
{0x88,1,{0x07}},
{0x89,1,{0x02}},
{0x8A,1,{0x02}},
{0x8B,1,{0x06}},
{0x8C,1,{0x06}},
{0xb0,1,{0x33}},
{0xb1,1,{0x33}},
{0xb2,1,{0x00}},
{0xc3,1,{0xff}},
{0xca,1,{0x44}},
{0xd0,1,{0x01}},
{0xd1,1,{0x12}},
{0xd2,1,{0x10}},
{0xd5,1,{0x55}},
{0xd8,1,{0x01}},
{0xd9,1,{0x02}},
{0xda,1,{0x80}},
{0xdc,1,{0x10}},
{0xdd,1,{0x40}},
{0xdf,1,{0xb6}},
{0xe2,1,{0x75}},
{0xe6,1,{0x22}},
{0xe7,1,{0x54}},
{0xFF,3,{0x98,0x82,0x02}},
{0xF1,1,{0x1C}},
{0x4B,1,{0x5A}},
{0x50,1,{0xCA}},
{0x51,1,{0x00}},
{0x06,1,{0x8F}},
{0x0B,1,{0xA0}},
{0x0C,1,{0x00}},
{0x0D,1,{0x0E}},
{0x0E,1,{0xE6}},
{0x4E,1,{0x11}},
{0xFF,3,{0x98,0x82,0x05}},
{0x03,1,{0x01}},
{0x04,1,{0x17}},
{0x50,1,{0x0F}},
{0x58,1,{0x61}},
{0x63,1,{0x9C}},
{0x64,1,{0x9C}},
{0x68,1,{0x29}},
{0x69,1,{0x2F}},
{0x6A,1,{0x3D}},
{0x6B,1,{0x2F}},
{0x85,1,{0x37}},
{0x46,1,{0x00}},
{0xFF,3,{0x98,0x82,0x06}},
{0xD9,1,{0x1F}},
{0xC0,1,{0x40}},
{0xC1,1,{0x16}},
{0x48,1,{0x0F}},
{0x4D,1,{0x80}},
{0x4E,1,{0x40}},
{0x83,1,{0x00}},
{0x84,1,{0x00}},
{0xC7,1,{0x05}},
{0xFF,3,{0x98,0x82,0x08}},
{0xE0,27,{0x40,0x24,0xAE,0xE9,0x2C,0x55,0x5E,0x85,0xB2,0xD5,0xAA,0x0B,0x35,0x5B,0x80,0xEA,0xA6,0xD4,0xF0,0x14,0xFF,0x31,0x57,0x85,0xAB,0x03,0xEC}},
{0xE1,27,{0x40,0x24,0xAE,0xE9,0x2C,0x55,0x5E,0x85,0xB2,0xD5,0xAA,0x0B,0x35,0x5B,0x80,0xEA,0xA6,0xD4,0xF0,0x14,0xFF,0x31,0x57,0x85,0xAB,0x03,0xEC}},
{0xFF,3,{0x98,0x82,0x0B}},
{0x9A,1,{0x44}},
{0x9B,1,{0x84}},
{0x9C,1,{0x03}},
{0x9D,1,{0x03}},
{0x9E,1,{0x71}},
{0x9F,1,{0x71}},
{0xAB,1,{0xE0}},
{0xFF,3,{0x98,0x82,0x0E}},
{0x11,1,{0x10}},
{0x13,1,{0x10}},
{0x00,1,{0xA0}},
{0xFF,3,{0x98,0x82,0x00}},
{0x35,1,{0x00}},
#endif
{0x11,0,{0x00}},
{REGFLAG_DELAY,120,{}},
{0x29,0,{0x00}},
{REGFLAG_DELAY,20,{}},
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
    unsigned int i;

    for (i = 0; i < count; i++)
    {
        unsigned cmd;
        cmd = table[i].cmd;

        switch (cmd)
        {
        case REGFLAG_DELAY:
            if (table[i].count <= 10)
                MDELAY(table[i].count);
            else
                MDELAY(table[i].count);
            break;

        case REGFLAG_END_OF_TABLE:
            break;

        default:
            dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
        }
    }
}

/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type                         = LCM_TYPE_DSI;
	params->width                        = FRAME_WIDTH;
	params->height                       = FRAME_HEIGHT;

	#ifndef BUILD_LK
	params->physical_width               = 68;     //LCM_PHYSICAL_WIDTH/1000;
	params->physical_height              = 151;    //LCM_PHYSICAL_HEIGHT/1000;
	params->physical_width_um            = 67930;  //LCM_PHYSICAL_WIDTH; = sqrt((size*25.4)^2/(18^2+9^2))*9*1000
	params->physical_height_um           = 150960; //LCM_PHYSICAL_HEIGHT; = sqrt((size*25.4)^2/(18^2+9^2))*18*1000
#endif

    // enable tearing-free
    params->dbi.te_mode                  = LCM_DBI_TE_MODE_DISABLED;
    params->dbi.te_edge_polarity         = LCM_POLARITY_RISING;

	#if (LCM_DSI_CMD_MODE)
	params->dsi.mode                     = CMD_MODE;
	params->dsi.switch_mode              = SYNC_PULSE_VDO_MODE;
	#else
	params->dsi.mode                     = SYNC_PULSE_VDO_MODE;//SYNC_EVENT_VDO_MODE;//BURST_VDO_MODE;////
	#endif

	// DSI
	/* Command mode setting */
	//1 Three lane or Four lane
	params->dsi.LANE_NUM                 = LCM_FOUR_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.color_order  = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq    = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding      = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format       = LCM_DSI_FORMAT_RGB888;

	params->dsi.PS                       = LCM_PACKED_PS_24BIT_RGB888;

	#if (LCM_DSI_CMD_MODE)
	params->dsi.intermediat_buffer_num   = 0;//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage
	params->dsi.word_count               = FRAME_WIDTH * 3; //DSI CMD mode need set these two bellow params, different to 6577
	#else
	params->dsi.intermediat_buffer_num   = 2;	//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage
	#endif

	// Video mode setting
	params->dsi.packet_size              = 256;
	params->dsi.vertical_sync_active                = 2;
	params->dsi.vertical_backporch                  = 16;
	params->dsi.vertical_frontporch                 = 230;
	params->dsi.vertical_active_line                = FRAME_HEIGHT;
	params->dsi.horizontal_sync_active              = 8;
	params->dsi.horizontal_backporch                = 20;
	params->dsi.horizontal_frontporch               = 20;
	
	params->dsi.horizontal_active_pixel  = FRAME_WIDTH;
	/* params->dsi.ssc_disable = 1; */

	params->dsi.PLL_CLOCK                = 285;//300;//258;
	params->dsi.ssc_disable                         = 1;
	params->dsi.ssc_range                           = 4;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count        = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9c;
}
#if 0
//#define AUXADC_LCM_VOLTAGE_CHANNEL (2)
//#define MIN_VOLTAGE (1350000)
//#define MAX_VOLTAGE (1550000)
//extern int IMM_GetOneChannelValue_Cali(int Channel, int *voltage);
static unsigned int lcm_compare_id(void)
{
    int array[4];
    char buffer[4]={0,0,0,0};
    int id=0;

    SET_RESET_PIN(1);
    MDELAY(1);
    SET_RESET_PIN(0);
    MDELAY(1);
    SET_RESET_PIN(1);
    MDELAY(120);

    array[0]=0x00043902;
    array[1]=0x008198ff;
    dsi_set_cmdq(array, 2, 1);

    MDELAY(10);
    array[0] = 0x00083700;
    dsi_set_cmdq(array, 1, 1);
    //MDELAY(10);
    read_reg_v2(0x4, &buffer[0], 3);//    NC 0x00  0x98 0x16

    id = (buffer[0]<<16) | (buffer[1]<<8) |buffer[2];
	return 1;
#ifdef BUILD_LK
	dprintf(CRITICAL, "%s, LK debug: ili9882n id = 0x%08x\n", __func__, id);
#else
	printk("%s: ili9882n id = 0x%08x \n", __func__, id);
#endif

	return (0x0098820d == id)?1:0;
}
#endif
//#define AUXADC_LCM_VOLTAGE_CHANNEL (2)
//#define MIN_VOLTAGE (1350000)
//#define MAX_VOLTAGE (1550000)
//extern int IMM_GetOneChannelValue_Cali(int Channel, int *voltage);
static unsigned int lcm_compare_id(void)
{
    int array[4];
    char buffer[4]={0,0,0,0};
    int id=0;
//	printk("dantangweiili9882n\n");
    SET_RESET_PIN(1);
    MDELAY(1);
    SET_RESET_PIN(0);
    MDELAY(1);
    SET_RESET_PIN(1);
    MDELAY(120);

    array[0]=0x00043902;
    array[1]=0x008198ff;
    dsi_set_cmdq(array, 2, 1);

    MDELAY(10);
    array[0] = 0x00083700;
    dsi_set_cmdq(array, 1, 1);
    //MDELAY(10);
    read_reg_v2(0x4, &buffer[0], 3);//    NC 0x00  0x98 0x16

    id = (buffer[0]<<16) | (buffer[1]<<8) |buffer[2];
//	return 1;
#ifdef BUILD_LK
	dprintf(CRITICAL, "%s, LK debug: ili9882n id = 0x%08x\n", __func__, id);
#else
	printk("%s: ili9882n id = 0x%08x \n", __func__, id);
#endif

	return (0x0098820d == id)?1:0;
}
static void lcm_init(void)
{
    printk("lcm ili9882n----init\n");
	//SET_RESET_PIN(0);
    display_bias_enable_v(5800);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(10);
    SET_RESET_PIN(0);
    MDELAY(20);
    SET_RESET_PIN(1);
    MDELAY(60);
    
    push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
    printk("ili9882n----lcm_suspend\n");
	push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
	//SET_RESET_PIN(1);
	//MDELAY(1);
	//SET_RESET_PIN(0);
	//MDELAY(1);
//prize add focal toucp gesture by yangcong  20201221 begin
	display_bias_disable();
//prize add focal toucp gesture by yangcong 20201221 end
//#endif
}

#if defined(CONFIG_PRIZE_LCM_POWEROFF_AFTER_TP)
static void lcm_poweroff_ext(void){
	display_ldo18_enable(0);
	display_bias_disable();
}
#endif

static void lcm_resume(void)
{
    lcm_init();
}

static void lcm_init_power(void)
{
	//display_bias_enable_v(5800);
}

static void lcm_suspend_power(void)
{
	//display_bias_disable();
}

static void lcm_resume_power(void)
{
	SET_RESET_PIN(0); 
	//display_bias_enable_v(5800);
}

#if (LCM_DSI_CMD_MODE)
static void lcm_update(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
    unsigned int x0 = x;
    unsigned int y0 = y;
    unsigned int x1 = x0 + width - 1;
    unsigned int y1 = y0 + height - 1;

    unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
    unsigned char x0_LSB = (x0 & 0xFF);
    unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
    unsigned char x1_LSB = (x1 & 0xFF);
    unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
    unsigned char y0_LSB = (y0 & 0xFF);
    unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
    unsigned char y1_LSB = (y1 & 0xFF);

    unsigned int data_array[16];

    data_array[0] = 0x00053902;
    data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
    data_array[2] = (x1_LSB);
    dsi_set_cmdq(data_array, 3, 1);

    data_array[0] = 0x00053902;
    data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
    data_array[2] = (y1_LSB);
    dsi_set_cmdq(data_array, 3, 1);

    data_array[0] = 0x002c3909;
    dsi_set_cmdq(data_array, 1, 0);
}
#endif

struct LCM_DRIVER ili9882n_hdp_dsi_vdo_truly6517_c5t_lcm_drv =
{
    .name			= "ili9882n_hdp_dsi_vdo_truly6517_c5t",
#if defined(CONFIG_PRIZE_HARDWARE_INFO) && !defined (BUILD_LK)
	.lcm_info = {
		.chip	= "ili9882n_hdp",
		.vendor	= "ili9882",
		.id		= "0x82",
		.more	= "720*1600",
	},
#endif
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init,
    .suspend        = lcm_suspend,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
    .resume         = lcm_resume,
    .compare_id     = lcm_compare_id,
#if (LCM_DSI_CMD_MODE)
    .update         = lcm_update,
#endif
#if defined(CONFIG_PRIZE_LCM_POWEROFF_AFTER_TP)
	.poweroff_ext	= lcm_poweroff_ext,
#endif
};
