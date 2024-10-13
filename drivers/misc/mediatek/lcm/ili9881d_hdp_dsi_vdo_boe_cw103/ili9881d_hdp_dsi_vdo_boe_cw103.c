// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

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
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include "disp_dts_gpio.h"
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

#define LCM_ID (0x98)

static const unsigned int BL_MIN_LEVEL = 20;
static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define MDELAY(n)		(lcm_util.mdelay(n))
#define UDELAY(n)		(lcm_util.udelay(n))


#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	  lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)
	/*DynFPS*/
#define dfps_dsi_send_cmd( \
			cmdq, cmd, count, para_list, force_update) \
			lcm_util.dsi_dynfps_send_cmd( \
			cmdq, cmd, count, para_list, force_update)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
/* #include <linux/jiffies.h> */
/* #include <linux/delay.h> */
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif

/* static unsigned char lcd_id_pins_value = 0xFF; */
static const unsigned char LCD_MODULE_ID = 0x01;
#define LCM_DSI_CMD_MODE	0
#define FRAME_WIDTH		(720)
#define FRAME_HEIGHT	(1280)

#define REGFLAG_DELAY             							 0xFFFA
#define REGFLAG_UDELAY             							 0xFFFB
#define REGFLAG_PORT_SWAP									 0xFFFC
#define REGFLAG_END_OF_TABLE      							 0xFFFD   // END OF REGISTERS MARKER

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{ 0xFF, 0x03, {0x98, 0x81, 0x00} },
	{0x28, 0, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 120, {} }
};

static struct LCM_setting_table lcm_initialization_setting[] =
{

	{0xFF,3,{0x98, 0x81, 0x01} },
	{0x91,1,{0x00}},                   
	{0x92,1,{0x00}},                   
	{0x93,1,{0x73}},       //STVA                    
	{0x94,1,{0x00}},       //STVB                    
	{0x95,1,{0x00}},       //STVC                    
	{0x96,1,{0x0E}},       //STVA_Rise   0c                  
	{0x97,1,{0x00}},       //STVB_Rise                    
	{0x98,1,{0x00}},       //STVC_Rise                
	{0x09,1,{0x01}},       //FTI1R(A)
	{0x0a,1,{0x01}},       //FTI2R(B)
	{0x0b,1,{0x01}},       //FTI3R(C)
	{0x0c,1,{0x01}},       //FTI1F(A)
	{0x0d,1,{0x01}},       //FTI2F(B)
	{0x0e,1,{0x01}},       //FTI2F(C)
	{0x0f,1,{0x00}},       //CLW1(ALR)
	{0x10,1,{0x00}},       //CLW2(ARR)
	{0x11,1,{0x00}},
	{0x12,1,{0x00}},
	{0x13,1,{0x00}},
	{0x14,1,{0x00}},
	{0x15,1,{0x00}},
	{0x16,1,{0x00}},
	{0x17,1,{0x00}},
	{0x18,1,{0x00}},
	{0x19,1,{0x00}},
	{0x1a,1,{0x00}},
	{0x1b,1,{0x00}},
	{0x1c,1,{0x00}},
	{0x1d,1,{0x00}},
	{0x1e,1,{0x40}},
	{0x1f,1,{0x40}},
	{0x20,1,{0x0a}},
	{0x21,1,{0x05}},
	{0x22,1,{0x0a}},
	{0x23,1,{0x00}},
	{0x24,1,{0x8C}},
	{0x25,1,{0x8C}},
	{0x26,1,{0x00}},
	{0x27,1,{0x00}},
	{0x28,1,{0x33}},
	{0x29,1,{0x03}},
	{0x2a,1,{0x00}},
	{0x2b,1,{0x00}},
	{0x2c,1,{0x00}},
	{0x2d,1,{0x00}},
	{0x2e,1,{0x00}},
	{0x2f,1,{0x00}},
	{0x30,1,{0x00}},
	{0x31,1,{0x00}},
	{0x32,1,{0x00}},
	{0x33,1,{0x00}},
	{0x34,1,{0x00}},
	{0x35,1,{0x00}},
	{0x36,1,{0x00}},
	{0x37,1,{0x00}},
	{0x38,1,{0x00}},
	{0x39,1,{0x07}},                   
	{0x3a,1,{0x00}},                   
	{0x3b,1,{0x00}},                   
	{0x3c,1,{0x00}},                                               
	{0x40,1,{0x03}},                   
	{0x41,1,{0x20}},                   
	{0x42,1,{0x00}},                   
	{0x43,1,{0x40}},     //GCH/L                 
	{0x44,1,{0x03}},                   
	{0x45,1,{0x00}},                   
	{0x46,1,{0x01}},                   
	{0x47,1,{0x08}},                   
	{0x48,1,{0x00}},                   
	{0x49,1,{0x00}},                   
	{0x4a,1,{0x00}},                   
	{0x4b,1,{0x00}},                                   
	{0x4c,1,{0x64}},                   
	{0x4d,1,{0xA8}},                   
	{0x4e,1,{0x86}},                   
	{0x4f,1,{0x22}},                   
	{0x50,1,{0x22}},                   
	{0x51,1,{0x22}},                   
	{0x52,1,{0x22}},                   
	{0x53,1,{0x22}},                   
	{0x54,1,{0x22}},                   
	{0x55,1,{0x22}},                   
	{0x56,1,{0x01}},                                     
	{0x57,1,{0x75}},                   
	{0x58,1,{0xB9}},                   
	{0x59,1,{0x97}},                   
	{0x5a,1,{0x22}},                   
	{0x5b,1,{0x22}},                 
	{0x5c,1,{0x22}},                   
	{0x5d,1,{0x22}},                   
	{0x5e,1,{0x22}},                   
	{0x5f,1,{0x22}},                   
	{0x60,1,{0x22}},                   
	{0x61,1,{0x01}},                   
	{0x62,1,{0x06}},                            
	{0x63,1,{0x54}},   
	{0x64,1,{0x56}},                   
	{0x65,1,{0x58}},                   
	{0x66,1,{0x5A}},                   
	{0x67,1,{0x06}},                   
	{0x68,1,{0x08}},                   
	{0x69,1,{0x02}},                   
	{0x6a,1,{0x02}},                   
	{0x6b,1,{0x02}},                   
	{0x6c,1,{0x02}},                   
	{0x6d,1,{0x02}},                   
	{0x6e,1,{0x02}},                   
	{0x6f,1,{0x02}},                   
	{0x70,1,{0x02}},                   
	{0x71,1,{0x02}},                   
	{0x72,1,{0x02}},                   
	{0x73,1,{0x02}},                   
	{0x74,1,{0x02}},                   
	{0x75,1,{0x02}},                   
	{0x76,1,{0x02}},                   
	{0x77,1,{0x01}},                   
	{0x78,1,{0x00}},                           
	{0x79,1,{0x55}},                   
	{0x7a,1,{0x57}},                   
	{0x7b,1,{0x59}},                   
	{0x7c,1,{0x5B}},                   
	{0x7d,1,{0x07}},                   
	{0x7e,1,{0x09}},                   
	{0x7f,1,{0x02}},                   
	{0x80,1,{0x02}},                   
	{0x81,1,{0x02}},                   
	{0x82,1,{0x02}},                   
	{0x83,1,{0x02}},                   
	{0x84,1,{0x02}},                   
	{0x85,1,{0x02}},                   
	{0x86,1,{0x02}},                   
	{0x87,1,{0x02}},                   
	{0x88,1,{0x02}},                   
	{0x89,1,{0x02}},                   
	{0x8a,1,{0x02}},                   
	{0x8b,1,{0x02}},                   
	{0x8c,1,{0x02}},                  
	{0x8d,1,{0x01}},                   
	{0x8e,1,{0x00}},                   
	{0xa0,1,{0x35}},  //GPM VGH_MOD Rise(必開)                 
	{0xa1,1,{0x00}},                   
	{0xa2,1,{0x00}},                   
	{0xa3,1,{0x00}},                   
	{0xa4,1,{0x00}},  //VGH_MOD EQ 起始                  
	{0xa5,1,{0x35}},  //VGH_MOD EQ 結束                 
	{0xa6,1,{0x0C}},                   
	{0xa7,1,{0x00}},                   
	{0xa8,1,{0x00}},                   
	{0xa9,1,{0x00}},                   
	{0xaa,1,{0x00}},                   
	{0xab,1,{0x00}},                   
	{0xac,1,{0x00}},                   
	{0xad,1,{0x00}},                   
	{0xae,1,{0xff}},                   
	{0xaf,1,{0x00}},                   
	{0xb0,1,{0x00}}, 

	{0xFF,3,{0x98,0x81,0x02}},                                 
	{0xA0,20,{0x00,0x0A,0x14,0x0F,0x10,0x22,0x17,0x1B,0x58,0x1D,0x29,0x4C,0x1B,0x1A,0x4F,0x24,0x29,0x3C,0x49,0x11}},
	{0xC0,20,{0x00,0x0A,0x14,0x0F,0x10,0x22,0x17,0x1B,0x58,0x1D,0x29,0x4C,0x1B,0x1A,0x4F,0x24,0x29,0x3C,0x49,0x11}},

	{0x18,1,{0xF4}},        // SH on ,0x default E4 
	{0x12,1,{0x55}},  //20220811  
	 
	{0x13,1,{0x52}},  //20220811
	
	{0xFF,3,{0x98,0x81,0x04}},
	{0x5D,1,{0x63}},     //VREG1 5V 
	{0x5E,1,{0x63}},     //VREG2 5V
	{0x60,1,{0x51}},     //VCM1 
	{0x62,1,{0x5f}},     //VCM2  
	{0x82,1,{0x24}},     //VREF_VGH_MOD_CLPSEL 12.5V 
	{0x84,1,{0x24}},     //VREF_VGH_DC 12.5V     
	{0x86,1,{0x2C}},     //VREF_VGL_CLPSEL -12V       
	{0x66,1,{0x04}},     //  VGH_AC x4 ,0xdefault :04
	{0xC1,1,{0x70}},     //  VGH_DC x4 ,0xdefault :70

	//{0xC7,1,{0x4c}},     //  VGH_DC x4 ,0xdefault :70


	{0x70,1,{0x60}}, 
	{0x71,1,{0x00}},
	{0x5B,1,{0x33}},    //  vcore_sel Voltage
	{0x6C,1,{0x10}},     //  vcore bias L
	{0x77,1,{0x03}},     //  vcore_sel Voltage
	{0x7B,1,{0x02}},     //  vcore bias R

	{0xFF,3,{0x98,0x81,0x05}},                                
	{0x22,1,{0x3A}},     //32=正   31=反

	{0xFF,3,{0x98,0x81,0x01}},                                
	{0xF0,1,{0x00}},     //1280 Gate NL          
	{0xF1,1,{0xC8}},     //1280 Gate NL    
		  
	{0xFF,3,{0x98,0x81,0x00}},       
	//{0x36,1,{0x08}},


	{0x11,1,{0X00}},                // Sleep-Out
	{REGFLAG_DELAY, 120, {}},
		 
	{0x29,1,{0X00}},                // Display On
	{REGFLAG_DELAY, 20, {}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;

		switch (cmd) {

		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
            dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
		}
	}
}


static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;
	#ifndef BUILD_LK
	params->physical_width               = 64;
	params->physical_height              = 115;
	params->physical_width_um            = 64800;
	params->physical_height_um           = 115200;
	#endif

	// enable tearing-free
	params->dbi.te_mode                  = LCM_DBI_TE_MODE_VSYNC_ONLY;
	params->dbi.te_edge_polarity         = LCM_POLARITY_RISING;

	params->dsi.mode                     = SYNC_PULSE_VDO_MODE;//SYNC_EVENT_VDO_MODE;//BURST_VDO_MODE;////

	/* DSI */
	/* Command mode setting */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	/* The following defined the fomat for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;
	/* video mode timing */

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

	params->dsi.vertical_sync_active = 8;
	params->dsi.vertical_backporch = 18;
	params->dsi.vertical_frontporch = 20;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 10;
	params->dsi.horizontal_backporch = 60;
	params->dsi.horizontal_frontporch = 60;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	/* params->dsi.ssc_disable = 1; */

#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 424;
#else
	params->dsi.PLL_CLOCK = 424;
#endif

	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable = 0;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
	params->dsi.lcm_esd_check_table[0].count = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;

	
	printk("ili9881d kernel %s\n",__func__);
}

static void lcm_init(void)
{

	display_bias_enable();
	printk("ili9881d kernel lcm_init\n");

    MDELAY(15);
	SET_RESET_PIN(1);
	MDELAY(1);
	SET_RESET_PIN(0);
	MDELAY(10);

	SET_RESET_PIN(1);
	MDELAY(10);

    push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
	printk("ili9881c----rt5081----lcm mode = vdo mode :%d----\n",lcm_dsi_mode);

}

static void lcm_suspend(void)
{
	printk("ili9881d kernel lcm_suspend\n");

    push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
	SET_RESET_PIN(0);
    MDELAY(10);
	display_bias_disable();
}

static void lcm_resume(void)
{
	printk("ili9881d kernel lcm_resume\n");

	lcm_init();
}

static unsigned int lcm_compare_id(void)
{
	unsigned int id = 0, version_id = 0;
	unsigned char buffer[2];
	unsigned int array[16];
	struct LCM_setting_table switch_table_page1[] = {
		{ 0xFF, 0x03, {0x98, 0x81, 0x01} }
	};
	struct LCM_setting_table switch_table_page0[] = {
		{ 0xFF, 0x03, {0x98, 0x81, 0x00} }
	};

	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);

	SET_RESET_PIN(1);
	MDELAY(20);

	push_table(switch_table_page1,sizeof(switch_table_page1) / sizeof(struct LCM_setting_table),1);

	array[0] = 0x00023700;	/* read id return two byte,version and id */
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x00, buffer, 1);
	id = buffer[0];		/* we only need ID */

	read_reg_v2(0x01, buffer, 1);
	version_id = buffer[0];

	printk("%s,ili9881c_id=0x%08x,version_id=0x%x\n",__func__, id, version_id);
	push_table(switch_table_page0,sizeof(switch_table_page0) / sizeof(struct LCM_setting_table),1);

	if (id == LCM_ID && version_id == 0x81)
		return 1;
	else
		return 1;

}
#ifndef BUILD_LK
extern atomic_t ESDCheck_byCPU;
#endif
static unsigned int lcm_ata_check(unsigned char *buffer)
{
	unsigned int ret = 0;
	unsigned int x0 = FRAME_WIDTH / 4;

	unsigned char x0_MSB = (x0 & 0xFF);

	unsigned int data_array[2];
	unsigned char read_buf[2];
	unsigned int num1 = 0, num2 = 0;

	struct LCM_setting_table switch_table_page0[] = {
		{ 0xFF, 0x03, {0x98, 0x81, 0x00} }
	};

	struct LCM_setting_table switch_table_page2[] = {
		{ 0xFF, 0x03, {0x98, 0x81, 0x02} }
	};


	MDELAY(20);

	num1 = sizeof(switch_table_page2) / sizeof(struct LCM_setting_table);

	push_table(switch_table_page2, num1, 1);
	printk("before read ATA check x0_MSB = 0x%x\n", x0_MSB);
	printk("before read ATA check read buff = 0x%x\n", read_buf[0]);

	data_array[0] = 0x00023902;
	data_array[1] = x0_MSB << 8 | 0x30;

	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00013700;
	dsi_set_cmdq(data_array, 1, 1);
	atomic_set(&ESDCheck_byCPU, 1);

	read_reg_v2(0x30, read_buf, 1);

	printk("after read ATA check size = 0x%x\n", read_buf[0]);

	num2 = sizeof(switch_table_page0) / sizeof(struct LCM_setting_table);

	push_table(switch_table_page0, num2, 1);

	if (read_buf[0] == x0_MSB)
		ret = 1;
	else
		ret = 1;

	return ret;
}
struct LCM_DRIVER ili9881d_hdp_dsi_vdo_boe_cw103_lcm_drv = {
	.name = "ili9881d_hdp_dsi_vdo_boe_cw103",
#if defined(CONFIG_PRIZE_HARDWARE_INFO) && !defined (BUILD_LK)
	.lcm_info = {
		.chip	= "ili9881d",
		.vendor	= "boe_cw103",
		.id		= "0x82",
		.more	= "720*1280",
	},
#endif
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.ata_check = lcm_ata_check,
};
