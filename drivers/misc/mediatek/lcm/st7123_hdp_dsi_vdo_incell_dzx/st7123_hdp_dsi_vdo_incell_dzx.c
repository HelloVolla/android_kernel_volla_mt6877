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
    {REGFLAG_DELAY, 120, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_initialization_setting[] =
{
	{0x60, 03, {0x71, 0x23, 0xA2}},
	{0x60, 03, {0x71, 0x23, 0xA3}},
	{0x60, 03, {0x71, 0x23, 0xA4}},
	{0x67, 01, {0x83}},
	{0x75, 01, {0x2B}},
	{0x85, 06, {0x48, 0x87, 0xE7, 0x04, 0x87, 0x46}},
	{0x86, 12, {0xA5, 0x19, 0x40, 0x15, 0xF5, 0x01, 0x00, 0xA5, 0xD9, 0x55, 0xF5, 0x0D}},
	{0x89, 02, {0x00, 0x00}},

	{0xA3, 39, {0x88,0x01,0x8C,0x00,0x45,0x00,0x00,0x00,0x00,0x00,0x46,0x00,0x00,0x1E,0x5C,0x1E,0x80,0x00,0x6F,0x45,0x00,0x00,0x00,0x00,0x00,0x46,0x00,0x00,0x1E,0x5C,0x1E,0x80,0x00,0x80,0x08,0x00,0x00,0x00,0xFF}},
	{0xA6, 55, {0x38,0x12,0x04,0x77,0x38,0x00,0x39,0x00,0x40,0x40,0x91,0xFF,0x02,0x04,0x55,0x00,0x06,0x00,0x07,0x40,0x40,0x91,0xFF,0x52,0x84,0x60,0x06,0x0c,0x00,0x00,0x40,0x40,0x91,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,0x80,0x00,0x00,0x00,0x00,0x00}},  //      ##ST5_SHT=0x06
	{0xA7, 60, {0x1C, 0x1C, 0x00, 0x44, 0x00, 0x07, 0x16, 0x01, 0x02, 0x00, 0x00, 0x40, 0x40, 0x91, 0xFF, 0x00, 0x00, 0x44, 0x00, 0x25, 0x34, 0x03, 0x04, 0x06, 0x05, 0x40, 0x40, 0x91, 0xFF, 0x00, 0x00, 0x44, 0x00, 0x4B, 0xC3, 0x06, 0x0E, 0x0D, 0x05, 0x9E, 0x9E, 0x91, 0xFF, 0x00, 0x00, 0x44, 0x00, 0x69, 0xE1, 0x08, 0x10, 0x2B, 0x03, 0x9E, 0x9E, 0x91, 0xFF, 0x00, 0x00, 0x53}},
	{0xAC, 44, {0x1c,0x00,0x1c,0x00,0x00,0x00,0x03,0x01,0x04,0x00,0x10,0x12,0x0B,0x09,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1c,0x00,0x01C,0x00,0x00,0x00,0x00,0x02,0x04,0x00,0x11,0x13,0x0A,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{0xAE, 07, {0xC5,0x3D,0x00,0xC5,0x3D,0x00,0x00}},
	{0xB0, 07, {0x22, 0x57, 0x11, 0x75, 0x25, 0x57, 0x57}},   //disp VGH=14V(57h)/VGL=57h=14V, tpvgh=17v(75), TPvgl=15V(61h)
	{0xB2, 15, {0x15, 0x19, 0x05, 0x23, 0x41, 0xFF, 0x03, 0x52, 0x5F, 0xD2, 0xFF, 0x10, 0x20, 0xE9, 0x10}},
	{0xB7, 06, {0x00, 0x00, 0x64, 0x64, 0x44, 0x1A}},        //AGT_VOP 5.0V
	{0xBF, 01, {0x50}},  //vcom   OTP优先
	{0xC8, 37, {0x00, 0x00, 0x0C, 0x17, 0x29, 0x00, 0x4D, 0x04, 0xA2, 0x06, 0x11, 0x0A, 0x09, 0x87, 0x03, 0x21, 0xD9, 0x01, 0x1C, 0x01, 0x22, 0x5C, 0x0E, 0xA7, 0x09, 0x33, 0x13, 0x0B, 0x6F, 0x0C, 0xF3, 0xBA, 0x00, 0xD9, 0xF5, 0x03, 0xFF}},
	{0xC9, 37, {0x00, 0x00, 0x0C, 0x17, 0x29, 0x00, 0x4D, 0x04, 0xA2, 0x06, 0x11, 0x0A, 0x09, 0x87, 0x03, 0x21, 0xD9, 0x01, 0x1C, 0x01, 0x22, 0x5C, 0x0E, 0xA7, 0x09, 0x33, 0x13, 0x0B, 0x6F, 0x0C, 0xF3, 0xBA, 0x00, 0xD9, 0xF5, 0x03, 0xFF}},
	{0xCF, 01, {0x18}},
	{0xD7, 06, {0x14, 0x08, 0x5B, 0x33, 0x20, 0x20}},
	{0xE7, 36, {0x3B, 0x00, 0x00, 0x7C, 0xA1, 0x8C, 0x20, 0x1A, 0xF0, 0xB1, 0x50, 0x00, 0x50, 0xB1, 0x50, 0xB1, 0x50, 0xD8, 0x00, 0x55, 0x00, 0xB1, 0x00, 0x45, 0xC9, 0x6A, 0xFF, 0x5A, 0xD8, 0x38, 0x88, 0x15, 0xB1, 0x01, 0x01, 0x71}},    //for 85M OSC
	{0xE8, 05, {0x20, 0x6A, 0x04, 0x78, 0x78}},
	{0xEA, 10, {0x16, 0x00, 0xFF, 0x01, 0x01, 0x86, 0x01, 0x1E, 0x80, 0x40}},

	{0x60, 03, {0x71, 0x23, 0x00}},
	{0x11, 00, {}},
	{REGFLAG_DELAY, 120, {} },
	{0x29, 00, {}},
	//{0x35, 01, {0x00}},
	{REGFLAG_DELAY, 20, {} },
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
	params->dsi.vertical_backporch                  = 6;
	params->dsi.vertical_frontporch                 = 260;
	params->dsi.vertical_active_line                = FRAME_HEIGHT;
	params->dsi.horizontal_sync_active              = 10;
	params->dsi.horizontal_backporch                = 32;
	params->dsi.horizontal_frontporch               = 32;
	
	params->dsi.horizontal_active_pixel  = FRAME_WIDTH;
	/* params->dsi.ssc_disable = 1; */

	params->dsi.PLL_CLOCK                = 282;//300;//258;
	//params->dsi.ssc_disable                         = 0;
	//params->dsi.ssc_range                           = 4;
	params->dsi.esd_check_enable = 0;
	params->dsi.customization_esd_check_enable = 0;
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
	dprintf(CRITICAL, "%s, LK debug: st7123 id = 0x%08x\n", __func__, id);
#else
	printk("%s: st7123 id = 0x%08x \n", __func__, id);
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
//	printk("dantangweiist7123\n");
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
	dprintf(CRITICAL, "%s, LK debug: st7123 id = 0x%08x\n", __func__, id);
#else
	printk("%s: st7123 id = 0x%08x \n", __func__, id);
#endif

	return (0x0098820d == id)?1:0;
}
static void lcm_init(void)
{
    printk("st7123----init\n");
	//SET_RESET_PIN(0);
    display_bias_enable();
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
    printk("st7123----lcm_suspend\n");
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
	display_bias_enable();
}

static void lcm_suspend_power(void)
{
	display_bias_disable();
}

static void lcm_resume_power(void)
{
	SET_RESET_PIN(0); 
	display_bias_enable();
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

struct LCM_DRIVER st7123_hdp_dsi_vdo_incell_dzx_lcm_drv =
{
    .name			= "st7123_hdp_dsi_vdo_incell_dzx",
#if defined(CONFIG_PRIZE_HARDWARE_INFO) && !defined (BUILD_LK)
	.lcm_info = {
		.chip	= "ST7123_hdp",
		.vendor	= "DZX",
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
