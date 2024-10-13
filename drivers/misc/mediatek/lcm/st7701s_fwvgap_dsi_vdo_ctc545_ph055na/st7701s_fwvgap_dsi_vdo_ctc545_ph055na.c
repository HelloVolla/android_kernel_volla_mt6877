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

#define SET_RESET_PIN(v)			(lcm_util.set_reset_pin((v)))
#define MDELAY(n)					(lcm_util.mdelay(n))

/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
	lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) \
	lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
	lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
	lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)
#define set_gpio_lcd_enp(cmd) \
		lcm_util.set_gpio_lcd_enp_bias(cmd)
		

static const unsigned char LCD_MODULE_ID = 0x01;
/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
#define LCM_DSI_CMD_MODE	0
#define FRAME_WIDTH  										 (480)
#define FRAME_HEIGHT 										 (960)

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
    {REGFLAG_DELAY, 50, {}},
    {0x10, 1, {0x00}},
    {REGFLAG_DELAY, 120, {} },
};

static struct LCM_setting_table lcm_initialization_setting[] =
{
{0xFF,0x05,{0x77,0x01,0x00,0x00,0x13}},
{0xEF,0x01,{0x08}},
{0xFF,0x05,{0x77,0x01,0x00,0x00,0x10}},
{0xC0,0x02,{0xF7,0x01}},
{0xC1,0x02,{0x07,0x02}},
{0xC2,0x02,{0x07,0x02}},
{0xCc,0x01,{0x30}},
{0xB0,0x10,{0x00,0x02,0x07,0x0F,0x15,0x0A,0x00,0x08,0x09,0x16,0x05,0x12,0x0A,0x09,0x0C,0x1F}},
{0xB1,0x10,{0x00,0x01,0x06,0x0E,0x12,0x07,0x00,0x07,0x06,0x1B,0x06,0x17,0x19,0x0E,0x12,0x1F}},
{0xFF,0x05,{0x77,0x01,0x00,0x00,0x11}},
{0xB0,0x01,{0x3D}},
//{{0xB1,0x01,{0x89}}, //vcom 烧录后屏蔽
{0xB2,0x01,{0x87}},
{0xB3,0x01,{0x80}},
{0xB5,0x01,{0x4D}},
{0xB7,0x01,{0x8A}},
{0xB8,0x01,{0x21}},
{0xC0,0x01,{0x03}},
{0xC1,0x01,{0x78}},
{0xC2,0x01,{0x78}},
{0xD0,0x01,{0x88}},
{REGFLAG_DELAY, 10, {}},
{0xE0,0x02,{0x80,0x00,0x02}},
{0xE1,0x0B,{0x01,0x93,0x00,0x00,0x02,0x93,0x00,0x00,0x00,0x23,0x23}},
{0xE2,0x0D,{0x30,0x30,0x20,0x20,0xC7,0x93,0x00,0x00,0xC8,0x93,0x00,0x00,0x00}},
{0xE3,0x04,{0x00,0x00,0x33,0x33}},
{0xE4,0x02,{0x44,0x44}},
{0xE5,0x10,{0x07,0xC3,0x93,0x93,0x05,0xC9,0x93,0x93,0x03,0xC7,0x93,0x93,0x09,0xC5,0x93,0x93}},
{0xE6,0x04,{0x00,0x00,0x33,0x33}},
{0xE7,0x02,{0x44,0x44}},
{0xE8,0x10,{0x08,0xC4,0x93,0x93,0x06,0xCA,0x93,0x93,0x04,0xC8,0x93,0x93,0x0A,0xC8,0x93,0x93}},
{0xEB,0x07,{0x00,0x02,0x4E,0x4E,0x99,0x33,0x00}},
{0xEC,0x02,{0x00,0x00}},
{0xED,0x10,{0x2F,0x0A,0xB7,0x65,0x4F,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xF4,0x56,0x7B,0xA0,0xF2}},
{0xFF,0x05,{0x77,0x01,0x00,0x00,0x13}},
{0xe8,0x02,{0x00,0x0e}},
{0xFF,0x05,{0x77,0x01,0x00,0x00,0x00}},

{0x11,0x01, {0x00}},
{0xFF,0x05,{0x77,0x01,0x00,0x00,0x13}},
{0xe8,0x02,{0x00,0x0c}},
{REGFLAG_DELAY, 10, {}},
{0xe8,0x02,{0x00,0x00}},
{0xFF,0x05,{0x77,0x01,0x00,0x00,0x00}},
{0x36,0x01,{0x00}},
{0x35,0x01,{0x00}},

{0x29,0x01, {0x00}},
{REGFLAG_DELAY, 120, {}},

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
	params->physical_width               = 62;
	params->physical_height              = 124;
	params->physical_width_um            = 61908;	//LCM_PHYSICAL_WIDTH; = sqrt((size*25.4)^2/(18^2+9^2))*9*1000
	params->physical_height_um           = 123816;	//LCM_PHYSICAL_HEIGHT; = sqrt((size*25.4)^2/(18^2+9^2))*18*1000
	params->density                      = 320;
	#endif

	// enable tearing-free
	params->dbi.te_mode                  = LCM_DBI_TE_MODE_VSYNC_ONLY;
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
	params->dsi.LANE_NUM                 = LCM_TWO_LANE;
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
	params->dsi.vertical_sync_active                = 8;
	params->dsi.vertical_backporch                  = 10;
	params->dsi.vertical_frontporch                 = 20;
	params->dsi.vertical_active_line                = FRAME_HEIGHT;
	params->dsi.horizontal_sync_active              = 10;
	params->dsi.horizontal_backporch                = 30;
	params->dsi.horizontal_frontporch               = 30;
	
	params->dsi.horizontal_active_pixel  = FRAME_WIDTH;
	/* params->dsi.ssc_disable = 1; */

	params->dsi.PLL_CLOCK                = 235;//300;//258;
	params->dsi.ssc_disable                         = 0;
	params->dsi.ssc_range                           = 4;
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd          = 0x0a;
	params->dsi.lcm_esd_check_table[0].count        = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9c;
}


#if 0
#define AUXADC_COMPARE_ID          0
#define AUXADC_LCM_VOLTAGE_CHANNEL (2)
#define MIN_VOLTAGE (200)
#define MAX_VOLTAGE (400)
static unsigned int lcm_compare_id(void)
{

    int array[4];
    char buffer[4]={0,0,0,0};
	char buffer1[4]={0,0,0,0};
	char buffer2[4]={0,0,0,0};
    int id=0;
	int rawdata = 0;
	int ret = 0;
	int res = 0;
	int i=0;
    int lcm_vol = 0;
    int data[4] = {0,0,0,0};
	int avg = 3;
	res = IMM_GetOneChannelValue(AUXADC_LCM_VOLTAGE_CHANNEL, data, &rawdata);
	return 1;
	printf("zgj:[cpu-adc_uboot]: lcm_vol = %d==XXXXX",lcm_vol);
	if(res < 0)
	{
	#ifdef BUILD_LK
	printf("cjx:[adc_uboot]: res = %d, get data error\n",res);
	#else
	printk("cjx:[adc_kernel]: get data error\n");
	#endif
	return 0;
	}
	lcm_vol = data[0]*1000+data[1]*10;
	#ifdef BUILD_LK
	printf("zgj:[adc_uboot]: lcm_vol= %d , file : %s, line : %d\n", lcm_vol, __FILE__, __LINE__);
	#else
	printk("zgj:[adc_kernel]: lcm_vol= %d , file : %s, line : %d\n", lcm_vol, __FILE__, __LINE__);
	#endif
	struct LCM_setting_table switch_table_page6[] = {
		{ 0xFF, 0x03, {0x98, 0x82, 0x06} }
	};
//	struct LCM_setting_table switch_table_page0[] = {
//		{ 0xFF, 0x03, {0x98, 0x81, 0x00} }
//	};
	 
    display_ldo18_enable(1);
    display_bias_vpos_enable(1);
    display_bias_vneg_enable(1);
    MDELAY(10);
    display_bias_vpos_set(5600);
	display_bias_vneg_set(5600);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(10);
    SET_RESET_PIN(0);
    MDELAY(20);
    SET_RESET_PIN(1);
    MDELAY(120);

	push_table(switch_table_page6,sizeof(switch_table_page6) / sizeof(struct LCM_setting_table),1);

    MDELAY(5);
    array[0] = 0x00013700;
    dsi_set_cmdq(array, 1, 1);
    //MDELAY(10);
    read_reg_v2(0xF0, buffer, 1);//    NC 0x00  0x98 
	
	MDELAY(5);
    array[0] = 0x00013700;
    dsi_set_cmdq(array, 1, 1);
    //MDELAY(10);
    read_reg_v2(0xF1, buffer1, 1);//    NC 0x00  0x82 
	
	MDELAY(5);
    array[0] = 0x00013700;
    dsi_set_cmdq(array, 1, 1);
    //MDELAY(10);
    read_reg_v2(0xF2, buffer2, 1);//    NC 0x00  0x0d 
    id = (buffer[0]<<16) | (buffer1[0]<<8) |buffer2[0];
	
#ifdef BUILD_LK
	dprintf(CRITICAL, "%s, LK debug: ili9882 id = 0x%08x\n", __func__, id);
#else
	printk("%s: ili9882 id = 0x%08x \n", __func__, id);
#endif

  
    if(0x988210 == id)
	{
		//if (lcm_vol >= MIN_VOLTAGE && lcm_vol <= MAX_VOLTAGE)
		//{
		//printf("zgj:lcm->gc9702_hdp_dsi_vdo_incell_truly_lcm_drv\n");
		return 1;
		//}
	   // return 0;
	}
    else{
		return 0;
	}

}
#endif

static unsigned int lcm_compare_id(void)
{
	unsigned int id = 0;
	unsigned char buffer[4];
	unsigned int data_array[16];
    display_bias_vpos_enable(1);
    display_bias_vneg_enable(1);
    MDELAY(10);
    display_bias_vpos_set(5600);
	display_bias_vneg_set(5600);
	MDELAY(10);
	SET_RESET_PIN(1);
    MDELAY(10);
    SET_RESET_PIN(0);
    MDELAY(20);
    SET_RESET_PIN(1);
    MDELAY(120);

	data_array[0] = 0x00043700;
	dsi_set_cmdq(data_array, 1, 1);
 	read_reg_v2(0x04, buffer, 3);
	id = (buffer[1]<<8)|buffer[2];

	#if defined(BUILD_LK)
		printf("st7701s wxs %s id = 0x%04x   0x%04x,0x%04x,0x%04x\n", __func__, id,buffer[0],buffer[1],buffer[2]);
	#else
		printk("st7701s %s id = 0x%04x\n", __func__, id);
	#endif

	if(id == 0x9702)
		return 1;
	else
		return 0;
}

static void lcm_init(void)
{
	display_ldo18_enable(1);
	display_ldo28_enable(1);

	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(10);
    SET_RESET_PIN(0);
    MDELAY(10);
    SET_RESET_PIN(1);
    MDELAY(50);

    push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
    push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);

    SET_RESET_PIN(1);
    MDELAY(5);

	//display_ldo18_enable(0);//st7701s cause standby leak 2mA
	display_ldo28_enable(0);
}

static void lcm_resume(void)
{
    lcm_init();
}

struct LCM_DRIVER st7701s_fwvgap_dsi_vdo_ctc545_ph055na_lcm_drv =
{
    .name			= "st7701s_fwvgap_dsi_vdo_ctc545_ph055na",
#if defined(CONFIG_PRIZE_HARDWARE_INFO) && !defined (BUILD_LK)
	.lcm_info = {
		.chip	= "st7701s_ctc545_ph055na",
		.vendor	= "st7701s",
		.id		= "0x82",
		.more	= "480*960",
	},
#endif
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init,
    .suspend        = lcm_suspend,
    .resume         = lcm_resume,
    .compare_id     = lcm_compare_id,
};
