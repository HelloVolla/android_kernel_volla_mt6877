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

        {0xFF,3,{0x98,0x81,0x03}},
        {0x01,1,{0x00}},
        {0x02,1,{0x00}},
        {0x03,1,{0x73}},
        {0x04,1,{0x73}},
        {0x05,1,{0x00}},
        {0x06,1,{0x06}},
        {0x07,1,{0x02}},
        {0x08,1,{0x00}},
        {0x09,1,{0x01}},
        {0x0A,1,{0x01}},
        {0x0B,1,{0x01}},
        {0x0C,1,{0x01}},
        {0x0D,1,{0x01}},
        {0x0E,1,{0x01}},
        {0x0F,1,{0x10}},
        {0x10,1,{0x10}},
        {0x11,1,{0x00}},
        {0x12,1,{0x00}},
        {0x13,1,{0x01}},
        {0x14,1,{0x00}},
        {0x15,1,{0x08}},
        {0x16,1,{0x08}},
        {0x17,1,{0x00}},
        {0x18,1,{0x08}},
        {0x19,1,{0x00}},
        {0x1A,1,{0x00}},
        {0x1B,1,{0x00}},
        {0x1C,1,{0x00}},
        {0x1D,1,{0x00}},
        {0x1E,1,{0xC0}},
        {0x1F,1,{0x80}},
        {0x20,1,{0x03}},
        {0x21,1,{0x04}},
        {0x22,1,{0x00}},
        {0x23,1,{0x00}},
        {0x24,1,{0x00}},
        {0x25,1,{0x00}},
        {0x26,1,{0x00}},
        {0x27,1,{0x00}},
        {0x28,1,{0x33}},
        {0x29,1,{0x02}},
        {0x2A,1,{0x00}},
        {0x2B,1,{0x00}},
        {0x2C,1,{0x00}},
        {0x2D,1,{0x00}},
        {0x2E,1,{0x00}},
        {0x2F,1,{0x00}},
        {0x30,1,{0x00}},
        {0x31,1,{0x00}},
        {0x32,1,{0x00}},
        {0x33,1,{0x00}},
        {0x34,1,{0x03}},
        {0x35,1,{0x00}},
        {0x36,1,{0x03}},
        {0x37,1,{0x00}},
        {0x38,1,{0x00}},
        {0x39,1,{0x38}},
        {0x3A,1,{0x00}},
        {0x3B,1,{0x40}},
        {0x3C,1,{0x00}},
        {0x3D,1,{0x00}},
        {0x3E,1,{0x00}},
        {0x3F,1,{0x00}},
        {0x40,1,{0x38}},
        {0x41,1,{0x88}},
        {0x42,1,{0x00}},
        {0x43,1,{0x00}},
        {0x44,1,{0x1F}},
        {0x50,1,{0x01}},
        {0x51,1,{0x23}},
        {0x52,1,{0x45}},
        {0x53,1,{0x67}},
        {0x54,1,{0x89}},
        {0x55,1,{0xAB}},
        {0x56,1,{0x01}},
        {0x57,1,{0x23}},
        {0x58,1,{0x45}},
        {0x59,1,{0x67}},
        {0x5A,1,{0x89}},
        {0x5B,1,{0xAB}},
        {0x5C,1,{0xCD}},
        {0x5D,1,{0xEF}},
        {0x5E,1,{0x12}},
        {0x5F,1,{0x09}},
        {0x60,1,{0x08}},
        {0x61,1,{0x0F}},
        {0x62,1,{0x0E}},
        {0x63,1,{0x0D}},
        {0x64,1,{0x0C}},
        {0x65,1,{0x02}},
        {0x66,1,{0x02}},
        {0x67,1,{0x02}},
        {0x68,1,{0x02}},
        {0x69,1,{0x02}},
        {0x6A,1,{0x02}},
        {0x6B,1,{0x02}},
        {0x6C,1,{0x02}},
        {0x6D,1,{0x02}},
        {0x6E,1,{0x02}},
        {0x6F,1,{0x02}},
        {0x70,1,{0x02}},
        {0x71,1,{0x06}},
        {0x72,1,{0x07}},
        {0x73,1,{0x02}},
        {0x74,1,{0x02}},
        {0x75,1,{0x06}},
        {0x76,1,{0x07}},
        {0x77,1,{0x0E}},
        {0x78,1,{0x0F}},
        {0x79,1,{0x0C}},
        {0x7A,1,{0x0D}},
        {0x7B,1,{0x02}},
        {0x7C,1,{0x02}},
        {0x7D,1,{0x02}},
        {0x7E,1,{0x02}},
        {0x7F,1,{0x02}},
        {0x80,1,{0x02}},
        {0x81,1,{0x02}},
        {0x82,1,{0x02}},
        {0x83,1,{0x02}},
        {0x84,1,{0x02}},
        {0x85,1,{0x02}},
        {0x86,1,{0x02}},
        {0x87,1,{0x09}},
        {0x88,1,{0x08}},
        {0x89,1,{0x02}},
        {0x8A,1,{0x02}},
        {0xFF,3,{0x98,0x81,0x04}},
        {0x6D,1,{0x08}},
        {0x6F,1,{0x05}},
        {0x70,1,{0x00}},
        {0x71,1,{0x00}},
        {0x66,1,{0xFE}},
        {0x82,1,{0x13}},
        {0x84,1,{0x13}},
        {0x85,1,{0x13}},
        {0x32,1,{0xAC}},
        {0x8C,1,{0x80}},
        {0x3C,1,{0xF5}},
        {0x3A,1,{0x24}},
        {0xB5,1,{0x02}},
        {0x31,1,{0x25}},
        {0x88,1,{0x33}},
        {0xFF,3,{0x98,0x81,0x01}},
        {0x22,1,{0x0A}},
        {0x31,1,{0x00}},
        {0x41,1,{0x24}},
        {0x53,1,{0x3B}},
        {0x55,1,{0x3A}},
        {0x50,1,{0x61}},
        {0x51,1,{0x61}},
        {0x60,1,{0x20}},
        {0x61,1,{0x00}},
        {0x62,1,{0x19}},
        {0x63,1,{0x00}},
        {0xA0,1,{0x08}},
        {0xA1,1,{0x25}},
        {0xA2,1,{0x36}},
        {0xA3,1,{0x18}},
        {0xA4,1,{0x1E}},
        {0xA5,1,{0x33}},
        {0xA6,1,{0x28}},
        {0xA7,1,{0x27}},
        {0xA8,1,{0xA3}},
        {0xA9,1,{0x1D}},
        {0xAA,1,{0x26}},
        {0xAB,1,{0x7C}},
        {0xAC,1,{0x17}},
        {0xAD,1,{0x14}},
        {0xAE,1,{0x49}},
        {0xAF,1,{0x1F}},
        {0xB0,1,{0x26}},
        {0xB1,1,{0x4D}},
        {0xB2,1,{0x61}},
        {0xB3,1,{0x39}},
        {0xC0,1,{0x08}},
        {0xC1,1,{0x25}},
        {0xC2,1,{0x37}},
        {0xC3,1,{0x17}},
        {0xC4,1,{0x1E}},
        {0xC5,1,{0x34}},
        {0xC6,1,{0x28}},
        {0xC7,1,{0x25}},
        {0xC8,1,{0xA3}},
        {0xC9,1,{0x1C}},
        {0xCA,1,{0x27}},
        {0xCB,1,{0x7D}},
        {0xCC,1,{0x17}},
        {0xCD,1,{0x16}},
        {0xCE,1,{0x49}},
        {0xCF,1,{0x20}},
        {0xD0,1,{0x26}},
        {0xD1,1,{0x4E}},
        {0xD2,1,{0x59}},
        {0xD3,1,{0x39}},
        {0xFF,3,{0x98,0x81,0x00}},
        {0x35,1,{0x00}},
        {0x11,0,{}},
        {REGFLAG_DELAY,120,{}},
        {0x29,0,{}},
        {REGFLAG_DELAY,20,{}},
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

	params->dsi.mode                     = BURST_VDO_MODE;//SYNC_EVENT_VDO_MODE;//BURST_VDO_MODE;////

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

	params->dsi.vertical_sync_active = 3;
	params->dsi.vertical_backporch = 16;
	params->dsi.vertical_frontporch = 16;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 16;
	params->dsi.horizontal_backporch = 48;
	params->dsi.horizontal_frontporch = 48;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	/* params->dsi.ssc_disable = 1; */

#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 296;
#else
	params->dsi.PLL_CLOCK = 296;
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
struct LCM_DRIVER ili9881d_hdp_dsi_vdo_boe_tongxingda_lcm_drv = {
	.name = "ili9881d_hdp_dsi_vdo_boe_tongxingda",
#if defined(CONFIG_PRIZE_HARDWARE_INFO) && !defined (BUILD_LK)
	.lcm_info = {
		.chip	= "ili9881d",
		.vendor	= "boe_tongxingda",
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
