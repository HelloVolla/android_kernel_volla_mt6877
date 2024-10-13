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
#define FRAME_WIDTH  										 (1080)
#define FRAME_HEIGHT 										 (1920)

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
    {REGFLAG_DELAY, 10, {}},
    {0x10, 1, {0x00}},
    {REGFLAG_DELAY, 60, {} },
};

static struct LCM_setting_table lcm_initialization_setting[] =
{
		//unlock manufacturing command write.
		{0xB0, 1, {0x04}},
		//remove NVM reload after sleep
		{0xD6, 1, {0x01}},
		{0xB3,8, {0x14,0x40,0x00,0x00,0x00,0x00,0x00,0x00}},
		//mipi lane number
		{0xB4, 2, {0x0C,0x00}},
		//mipi clock
		{0xB6,2,{0x0B,0xD3}},
		{0xC1,42,{0x0C,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x02,0x0F,0x04,0x00,0xE8,0x17,0x00,0x01,0x00,0x00,0x00,0x62,0x30,0x40,0xA5,0x00,0x04,0x20}},
		{0xC2,9, {0x20,0xF0,0x07,0x80,0x10,0x10,0x00,0x00,0x00}},
		{0xC4,21,{0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x01,0x02}},
		{0xC6,21,{0x78,0x3C,0x3C,0x07,0x01,0x07,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0A,0x1E,0x07,0xC8}},
		{0xC7,30,{0x02,0x09,0x0F,0x18,0x24,0x31,0x3A,0x49,0x2F,0x38,0x49,0x5A,0x64,0x6F,0x7C,0x02,0x09,0x0F,0x17,0x24,0x31,0x3A,0x49,0x2F,0x38,0x49,0x5A,0x64,0x6F,0x7C}},
		{0xCB,13,{0xFF,0xFF,0x0F,0xFF,0xFF,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
		{0xCC,1,{0x1B}},
		{0xCD,41,{0x01,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x12,0x05,0x05,0x12,0x11,0x10,0x0F,0x0E,0x0D,0x0C,0x0B,0x0A,0x09,0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0x00}},
		{0xD0,13,{0x11,0x59,0xBB,0x68,0x99,0x4C,0x19,0x11,0x19,0x0C,0xCD,0x14,0x00}},
		{0xD2,2,{0x08,0x2F}},
		{0xD3,29,{0xA3,0x33,0xBB,0xDD,0xD5,0x33,0x33,0x33,0x00,0x00,0x0A,0x98,0x98,0x20,0x20,0x43,0x43,0x33,0x33,0x37,0x60,0xFD,0xFE,0x07,0x10,0x00,0x00,0x00,0x52}},
		{0xD5,13,{0x06,0x00,0x00,0x00,0x70,0x00,0x70,0x01,0x00,0x00,0x00,0x00,0x00}},
		{0xD7,23,{0x10,0xF0,0x04,0x2A,0x04,0x10,0x00,0x80,0x03,0x1D,0xC0,0x00,0xAC,0x10,0x00,0x60,0x10,0x07,0x00,0x00,0x00,0x00,0x00}},
		{0xEB,31,{0x00,0x55,0x55,0x54,0x50,0x55,0x55,0x55,0x55,0x51,0x11,0x44,0x44,0x44,0x10,0x21,0x0E,0x00,0x00,0x00,0x00,0x00,0x00,0x44,0x44,0x44,0x44,0x33,0x37,0x44,0x05}},
		{0x35, 1, {0x00}},
		//LED pwm output enable
		{0x53, 1, {0x24}},

		//display brightness
		{0x51, 1, {0x00}},

		// Display ON
		{0x29, 0, {}},
		//Sleep Out
		{0x11, 0, {}},
		{REGFLAG_DELAY, 120, {}},
		{REGFLAG_END_OF_TABLE, 0x00, {}}

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
	params->physical_width               = 64;
	params->physical_height              = 115;
	params->physical_width_um            = 64800;
	params->physical_height_um           = 115200;
//	params->density                      = 320;
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

	params->dsi.vertical_sync_active     = 2;
	params->dsi.vertical_backporch       = 14;
	params->dsi.vertical_frontporch      = 16;
	params->dsi.vertical_active_line     = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active   = 10;
	params->dsi.horizontal_backporch     = 80;//36
	params->dsi.horizontal_frontporch    = 100;//78
	params->dsi.horizontal_active_pixel  = FRAME_WIDTH;
	/* params->dsi.ssc_disable = 1; */

	params->dsi.PLL_CLOCK                = 486;
	
	params->dsi.esd_check_enable = 0;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd          = 0x0a;
	params->dsi.lcm_esd_check_table[0].count        = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9c;
}

#define LCM_ID_ILI7807D                (0x780704)

static unsigned int lcm_compare_id(void)
 {
 
     int array[4];
     char buffer[5];
     char id_high=0;
     char id_midd=0;
     char id_low=0;
     int id=0;

	 return 1;
     //首先进行复位操作
     SET_RESET_PIN(1);
     MDELAY(20);   
     SET_RESET_PIN(0);
     MDELAY(20);
     SET_RESET_PIN(1);
     MDELAY(120);
 
     //enable CMD2 Page6
     array[0]=0x00043902;
     array[1]=0x010778ff;
     dsi_set_cmdq(array, 2, 1);
     MDELAY(10);
 
 
     array[0]=0x00033700;
     dsi_set_cmdq(array, 1, 1);
 
     read_reg_v2(0x00, buffer,1);
     id_high = buffer[0]; ///////////////////////0x98
 
     read_reg_v2(0x01, buffer,1);
     id_midd = buffer[0]; ///////////////////////0x81
 
     read_reg_v2(0x02, buffer,1);
     id_low = buffer[0]; ////////////////////////0x0d
 
     id =(id_high << 16) | (id_midd << 8) | id_low;
 
     #if defined(BUILD_LK)
     printf("ILI7807D compare-LK:0x%02x,0x%02x,0x%02x,0x%02x\n", id_high, id_midd, id_low, id);
     #else
     printk("ILI7807D compare:0x%02x,0x%02x,0x%02x,0x%02x\n", id_high, id_midd, id_low, id);
     #endif
 
     return (id == LCM_ID_ILI7807D)?1:0;
 }

static void lcm_init(void)
{
	display_bias_enable();
	
    MDELAY(5);
    SET_RESET_PIN(1);
    MDELAY(5);
    SET_RESET_PIN(0);
    MDELAY(5);
    SET_RESET_PIN(1);
    MDELAY(10);
    
    push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
	//fts_enter_low_power();
	MDELAY(10);
    push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);

	SET_RESET_PIN(0);
    MDELAY(10);
	display_bias_disable();
}

static void lcm_resume(void)
{
    lcm_init();
}

struct LCM_DRIVER r61322_fhd_dsi_vdo_sharp_lcm_drv =
{
    .name			= "r61322_fhd_dsi_vdo_sharp",
#if defined(CONFIG_PRIZE_HARDWARE_INFO) && !defined (BUILD_LK)
	.lcm_info = {
		.chip	= "r61322",
		.vendor	= "renesas",
		.id		= "0x82",
		.more	= "1080*1920",
	},
#endif
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init,
    .suspend        = lcm_suspend,
    .resume         = lcm_resume,
    .compare_id     = lcm_compare_id,
};
