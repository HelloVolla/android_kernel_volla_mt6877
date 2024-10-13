/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/backlight.h>
#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <linux/delay.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/fb.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_panel_ext.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#include "../../../misc/prize/lcd_bias/prize_lcd_bias.h"

#if defined(CONFIG_RT4831A_I2C)
#include "../../../misc/mediatek/gate_ic/gate_i2c.h"
#endif

#if defined(CONFIG_PRIZE_HARDWARE_INFO)
#include "../../../misc/mediatek/hardware_info/hardware_info.h"
extern struct hardware_info current_lcm_info;
#endif

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos, *bias_neg;

	bool prepared;
	bool enabled;

	int error;
};

#define lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define lcm_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

static void lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = lcm_dcs_read(ctx,  0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
static struct regulator *disp_bias_pos;
static struct regulator *disp_bias_neg;


static int lcm_panel_bias_regulator_init(void)
{
	static int regulator_inited;
	int ret = 0;

	if (regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_bias_pos = regulator_get(NULL, "dsv_pos");
	if (IS_ERR(disp_bias_pos)) { /* handle return value */
		ret = PTR_ERR(disp_bias_pos);
		pr_err("get dsv_pos fail, error: %d\n", ret);
		return ret;
	}

	disp_bias_neg = regulator_get(NULL, "dsv_neg");
	if (IS_ERR(disp_bias_neg)) { /* handle return value */
		ret = PTR_ERR(disp_bias_neg);
		pr_err("get dsv_neg fail, error: %d\n", ret);
		return ret;
	}

	regulator_inited = 1;
	return ret; /* must be 0 */

}

static int lcm_panel_bias_enable(void)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_bias_regulator_init();

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_bias_pos, 5800000, 5800000);
	if (ret < 0)
		pr_err("set voltage disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;
    mdelay(5);//prize add by lipengpeng 20220223 start 
	ret = regulator_set_voltage(disp_bias_neg, 5800000, 5800000);
	if (ret < 0)
		pr_err("set voltage disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	/* enable regulator */
	ret = regulator_enable(disp_bias_pos);
	if (ret < 0)
		pr_err("enable regulator disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	udelay(2000);

	ret = regulator_enable(disp_bias_neg);
	if (ret < 0)
		pr_err("enable regulator disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}

static int lcm_panel_bias_disable(void)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_bias_regulator_init();

	ret = regulator_disable(disp_bias_neg);
	if (ret < 0)
		pr_err("disable regulator disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_disable(disp_bias_pos);
	if (ret < 0)
		pr_err("disable regulator disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}
#endif

static void lcm_panel_init(struct lcm *ctx)
{

	pr_info("ILI77600a %s\n", __func__);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(50);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
lcm_dcs_write_seq_static(ctx,0xFF,0x5A,0xA5,0x01);
lcm_dcs_write_seq_static(ctx,0x00,0x41);
lcm_dcs_write_seq_static(ctx,0x01,0x50);
lcm_dcs_write_seq_static(ctx,0x02,0x15);     //STV_FTI_R
lcm_dcs_write_seq_static(ctx,0x03,0x34);     //STV_FTI_F

lcm_dcs_write_seq_static(ctx,0x08,0x81);
lcm_dcs_write_seq_static(ctx,0x09,0x03);
lcm_dcs_write_seq_static(ctx,0x0A,0x70);
lcm_dcs_write_seq_static(ctx,0x0B,0x01);     //CLW_MODE
lcm_dcs_write_seq_static(ctx,0x0C,0x08);     //FR1(120Hz)_CLW_R
lcm_dcs_write_seq_static(ctx,0x0E,0x21);     //FR1(120Hz)_CLW_F

//FW_R
lcm_dcs_write_seq_static(ctx,0x31,0x02);     //GOUTR01 TSGSMPL
lcm_dcs_write_seq_static(ctx,0x32,0x07);     //GOUTR02 DUMMY
lcm_dcs_write_seq_static(ctx,0x33,0x07);     //GOUTR03 DUMMY
lcm_dcs_write_seq_static(ctx,0x34,0x07);     //GOUTR04 DUMMY
lcm_dcs_write_seq_static(ctx,0x35,0x07);     //GOUTR05 DUMMY
lcm_dcs_write_seq_static(ctx,0x36,0x07);     //GOUTR06 DUMMY
lcm_dcs_write_seq_static(ctx,0x37,0x07);     //GOUTR07 DUMMY
lcm_dcs_write_seq_static(ctx,0x38,0x37);     //GOUTR08 BSWBL
lcm_dcs_write_seq_static(ctx,0x39,0x36);     //GOUTR09 GSWBL
lcm_dcs_write_seq_static(ctx,0x3A,0x35);     //GOUTR10 RSWBL
lcm_dcs_write_seq_static(ctx,0x3B,0x34);     //GOUTR11 BSWL
lcm_dcs_write_seq_static(ctx,0x3C,0x33);     //GOUTR12 GSWL
lcm_dcs_write_seq_static(ctx,0x3D,0x32);     //GOUTR13 RSWL
lcm_dcs_write_seq_static(ctx,0x3E,0x38);     //GOUTR14 INITL
lcm_dcs_write_seq_static(ctx,0x3F,0x11);     //GOUTR15 GCK8
lcm_dcs_write_seq_static(ctx,0x40,0x17);     //GOUTR16 GCK6
lcm_dcs_write_seq_static(ctx,0x41,0x15);     //GOUTR17 GCK4
lcm_dcs_write_seq_static(ctx,0x42,0x13);     //GOUTR18 GCK2
lcm_dcs_write_seq_static(ctx,0x43,0x09);     //GOUTR19 GSPL
lcm_dcs_write_seq_static(ctx,0x44,0x2A);     //GOUTR20 GVSS_AONL
lcm_dcs_write_seq_static(ctx,0x45,0x2A);     //GOUTR21 GVSS_AONL
lcm_dcs_write_seq_static(ctx,0x46,0x2A);     //GOUTR22 GVSS_AONL
lcm_dcs_write_seq_static(ctx,0x47,0x2A);     //GOUTR23 GVSS_AONL
lcm_dcs_write_seq_static(ctx,0x48,0x2A);     //GOUTR24 GVSS_AONL
//FW_L
lcm_dcs_write_seq_static(ctx,0x49,0x02);     //GOUTL01 TSGSMPR
lcm_dcs_write_seq_static(ctx,0x4A,0x07);     //GOUTL02 DUMMY
lcm_dcs_write_seq_static(ctx,0x4B,0x07);     //GOUTL03 DUMMY
lcm_dcs_write_seq_static(ctx,0x4C,0x07);     //GOUTL04 DUMMY
lcm_dcs_write_seq_static(ctx,0x4D,0x07);     //GOUTL05 DUMMY
lcm_dcs_write_seq_static(ctx,0x4E,0x07);     //GOUTL06 DUMMY
lcm_dcs_write_seq_static(ctx,0x4F,0x07);     //GOUTL07 DUMMY
lcm_dcs_write_seq_static(ctx,0x50,0x37);     //GOUTL08 BSWBR
lcm_dcs_write_seq_static(ctx,0x51,0x36);     //GOUTL09 GSWBR
lcm_dcs_write_seq_static(ctx,0x52,0x35);     //GOUTL10 RSWBR
lcm_dcs_write_seq_static(ctx,0x53,0x34);     //GOUTL11 BSWR
lcm_dcs_write_seq_static(ctx,0x54,0x33);     //GOUTL12 GSWR
lcm_dcs_write_seq_static(ctx,0x55,0x32);     //GOUTL13 RSWR
lcm_dcs_write_seq_static(ctx,0x56,0x38);     //GOUTL14 INITR
lcm_dcs_write_seq_static(ctx,0x57,0x10);     //GOUTL15 GCK7
lcm_dcs_write_seq_static(ctx,0x58,0x16);     //GOUTL16 GCK5
lcm_dcs_write_seq_static(ctx,0x59,0x14);     //GOUTL17 GCK3
lcm_dcs_write_seq_static(ctx,0x5A,0x12);     //GOUTL18 GCK1
lcm_dcs_write_seq_static(ctx,0x5B,0x08);     //GOUTL19 GSPR
lcm_dcs_write_seq_static(ctx,0x5C,0x2A);     //GOUTL20 GVSS_AONR
lcm_dcs_write_seq_static(ctx,0x5D,0x2A);     //GOUTL21 GVSS_AONR
lcm_dcs_write_seq_static(ctx,0x5E,0x2A);     //GOUTL22 GVSS_AONR
lcm_dcs_write_seq_static(ctx,0x5F,0x2A);     //GOUTL23 GVSS_AONR
lcm_dcs_write_seq_static(ctx,0x60,0x2A);     //GOUTL24 GVSS_AONR
//BW_R
lcm_dcs_write_seq_static(ctx,0x61,0x02);     //GOUTR01 TSGSMPL
lcm_dcs_write_seq_static(ctx,0x62,0x07);     //GOUTR02 DUMMY
lcm_dcs_write_seq_static(ctx,0x63,0x07);     //GOUTR03 DUMMY
lcm_dcs_write_seq_static(ctx,0x64,0x07);     //GOUTR04 DUMMY
lcm_dcs_write_seq_static(ctx,0x65,0x07);     //GOUTR05 DUMMY
lcm_dcs_write_seq_static(ctx,0x66,0x07);     //GOUTR06 DUMMY
lcm_dcs_write_seq_static(ctx,0x67,0x07);     //GOUTR07 DUMMY
lcm_dcs_write_seq_static(ctx,0x68,0x37);     //GOUTR08 BSWBL
lcm_dcs_write_seq_static(ctx,0x69,0x36);     //GOUTR09 GSWBL
lcm_dcs_write_seq_static(ctx,0x6A,0x35);     //GOUTR10 RSWBL
lcm_dcs_write_seq_static(ctx,0x6B,0x34);     //GOUTR11 BSWL
lcm_dcs_write_seq_static(ctx,0x6C,0x33);     //GOUTR12 GSWL
lcm_dcs_write_seq_static(ctx,0x6D,0x32);     //GOUTR13 RSWL
lcm_dcs_write_seq_static(ctx,0x6E,0x38);     //GOUTR14 INITL
lcm_dcs_write_seq_static(ctx,0x6F,0x16);     //GOUTR15 GCK8
lcm_dcs_write_seq_static(ctx,0x70,0x10);     //GOUTR16 GCK6
lcm_dcs_write_seq_static(ctx,0x71,0x12);     //GOUTR17 GCK4
lcm_dcs_write_seq_static(ctx,0x72,0x14);     //GOUTR18 GCK2
lcm_dcs_write_seq_static(ctx,0x73,0x08);     //GOUTR19 GSPL
lcm_dcs_write_seq_static(ctx,0x74,0x2A);     //GOUTR20 GVSS_AONL
lcm_dcs_write_seq_static(ctx,0x75,0x2A);     //GOUTR21 GVSS_AONL
lcm_dcs_write_seq_static(ctx,0x76,0x2A);     //GOUTR22 GVSS_AONL
lcm_dcs_write_seq_static(ctx,0x77,0x2A);     //GOUTR23 GVSS_AONL
lcm_dcs_write_seq_static(ctx,0x78,0x2A);     //GOUTR24 GVSS_AONL
//BW_L
lcm_dcs_write_seq_static(ctx,0x79,0x02);     //GOUTL01 TSGSMPR
lcm_dcs_write_seq_static(ctx,0x7A,0x07);     //GOUTL02 DUMMY
lcm_dcs_write_seq_static(ctx,0x7B,0x07);     //GOUTL03 DUMMY
lcm_dcs_write_seq_static(ctx,0x7C,0x07);     //GOUTL04 DUMMY
lcm_dcs_write_seq_static(ctx,0x7D,0x07);     //GOUTL05 DUMMY
lcm_dcs_write_seq_static(ctx,0x7E,0x07);     //GOUTL06 DUMMY
lcm_dcs_write_seq_static(ctx,0x7F,0x07);     //GOUTL07 DUMMY
lcm_dcs_write_seq_static(ctx,0x80,0x37);     //GOUTL08 BSWBR
lcm_dcs_write_seq_static(ctx,0x81,0x36);     //GOUTL09 GSWBR
lcm_dcs_write_seq_static(ctx,0x82,0x35);     //GOUTL10 RSWBR
lcm_dcs_write_seq_static(ctx,0x83,0x34);     //GOUTL11 BSWR
lcm_dcs_write_seq_static(ctx,0x84,0x33);     //GOUTL12 GSWR
lcm_dcs_write_seq_static(ctx,0x85,0x32);     //GOUTL13 RSWR
lcm_dcs_write_seq_static(ctx,0x86,0x38);     //GOUTL14 INITR
lcm_dcs_write_seq_static(ctx,0x87,0x17);     //GOUTL15 GCK7
lcm_dcs_write_seq_static(ctx,0x88,0x11);     //GOUTL16 GCK5
lcm_dcs_write_seq_static(ctx,0x89,0x13);     //GOUTL17 GCK3
lcm_dcs_write_seq_static(ctx,0x8A,0x15);     //GOUTL18 GCK1
lcm_dcs_write_seq_static(ctx,0x8B,0x09);     //GOUTL19 GSPR
lcm_dcs_write_seq_static(ctx,0x8C,0x2A);     //GOUTL20 GVSS_AONR
lcm_dcs_write_seq_static(ctx,0x8D,0x2A);     //GOUTL21 GVSS_AONR
lcm_dcs_write_seq_static(ctx,0x8E,0x2A);     //GOUTL22 GVSS_AONR
lcm_dcs_write_seq_static(ctx,0x8F,0x2A);     //GOUTL23 GVSS_AONR
lcm_dcs_write_seq_static(ctx,0x90,0x2A);     //GOUTL24 GVSS_AONR

//GIP Signal Output
lcm_dcs_write_seq_static(ctx,0xC1,0x2E);     //FR1_REFTP_TR
lcm_dcs_write_seq_static(ctx,0xB0,0x00);     //Gate rescan
lcm_dcs_write_seq_static(ctx,0xB1,0x00);     //Gate rescan
lcm_dcs_write_seq_static(ctx,0xB8,0x40);     //OK VCSW1
lcm_dcs_write_seq_static(ctx,0xB9,0x00);     //OK FWBW
lcm_dcs_write_seq_static(ctx,0xBA,0x0A);     //OK STCH1  //Edward_Burning

//GIP Power On
lcm_dcs_write_seq_static(ctx,0xD1,0x01);     //12region frame
lcm_dcs_write_seq_static(ctx,0xD2,0x80);     //FW_CLKA_STV
lcm_dcs_write_seq_static(ctx,0xD3,0x30);     //STCH1_STCH2_GAS_STV_Burning
lcm_dcs_write_seq_static(ctx,0xD4,0x25);     //VCSW1_CKH
lcm_dcs_write_seq_static(ctx,0xD5,0x82);     //BW_VCSW2
lcm_dcs_write_seq_static(ctx,0xD6,0x91);     //OK CLK_A Edward
lcm_dcs_write_seq_static(ctx,0xD7,0x92);     //OK CLK_A Edward  STCH2_Burning
lcm_dcs_write_seq_static(ctx,0xD9,0x11);     //CKH_VCSW2_VCSW1

//GIP Power Off
lcm_dcs_write_seq_static(ctx,0xDA,0x01);
lcm_dcs_write_seq_static(ctx,0xDB,0x21);
lcm_dcs_write_seq_static(ctx,0xDE,0x80);
lcm_dcs_write_seq_static(ctx,0xDF,0x51);
lcm_dcs_write_seq_static(ctx,0xDC,0x80);
lcm_dcs_write_seq_static(ctx,0xDD,0x06);
lcm_dcs_write_seq_static(ctx,0xE0,0x40);
lcm_dcs_write_seq_static(ctx,0xE1,0x04);
lcm_dcs_write_seq_static(ctx,0xE2,0x10);
lcm_dcs_write_seq_static(ctx,0xF7,0x51);
lcm_dcs_write_seq_static(ctx,0xF8,0x11);

//GIP Abnormal Power Off
lcm_dcs_write_seq_static(ctx,0xE3,0x80);     //FW_CLK_STV
lcm_dcs_write_seq_static(ctx,0xE4,0x40);     //GAS_STCH1_STCH2
lcm_dcs_write_seq_static(ctx,0xE5,0x50);     //BW_CKH_VCSW2_VCSW1
lcm_dcs_write_seq_static(ctx,0xE6,0x23);     //Blank frame

//VCSW1_VCSW2
lcm_dcs_write_seq_static(ctx,0xBB,0x50);

lcm_dcs_write_seq_static(ctx,0xF5,0x80);

lcm_dcs_write_seq_static(ctx,0xFF,0x5A,0xA5,0x11);
lcm_dcs_write_seq_static(ctx,0x00,0x09);     //FR2(90Hz)_CLW_A1_Rise[7:0]
lcm_dcs_write_seq_static(ctx,0x01,0x31);     //FR2(90Hz)_CLW_A1_Fall[7:0]
lcm_dcs_write_seq_static(ctx,0x14,0x4C);     //FR2(90Hz)_REFTP_TR 

lcm_dcs_write_seq_static(ctx,0x30,0x17); 	//FR3(60Hz)_CLW_A1_Rise[7:0]
lcm_dcs_write_seq_static(ctx,0x31,0x45);     //FR3(60Hz)_CLW_A1_Fall[7:0]
lcm_dcs_write_seq_static(ctx,0x44,0x60);     //FR3(60Hz)_REFTP_TR  

lcm_dcs_write_seq_static(ctx,0xFF,0x5A,0xA5,0x02);
lcm_dcs_write_seq_static(ctx,0xBE,0x00);     //FR_sel 00:120/240  10:90/270	20:60/120
lcm_dcs_write_seq_static(ctx,0x46,0x11);     //DUMMY CKH
lcm_dcs_write_seq_static(ctx,0x47,0x03);     //CKH connect
lcm_dcs_write_seq_static(ctx,0x5A,0x00);     //SRC V0@VFP
lcm_dcs_write_seq_static(ctx,0x5B,0x20);     //SRC V255@VFP CKH
lcm_dcs_write_seq_static(ctx,0x4E,0xC4);
lcm_dcs_write_seq_static(ctx,0x4F,0x01);     //CKH connect @ vporch
lcm_dcs_write_seq_static(ctx,0x4C,0x00);     //Advance CKH OFF
lcm_dcs_write_seq_static(ctx,0x7C,0x50);     //SRC_Burning
lcm_dcs_write_seq_static(ctx,0x8B,0x01);     //EQ_R_1step

lcm_dcs_write_seq_static(ctx,0x40,0x0E);     //FR1(120Hz)_T8_DE
lcm_dcs_write_seq_static(ctx,0x41,0x03);     //FR1(120Hz)_prc_sdt
lcm_dcs_write_seq_static(ctx,0x42,0x03);     //FR1(120Hz)_T9_DE
lcm_dcs_write_seq_static(ctx,0x43,0x16);     //FR1(120Hz)_T7_DE R_width
lcm_dcs_write_seq_static(ctx,0x53,0x07);     //FR1(120Hz)_SDT
lcm_dcs_write_seq_static(ctx,0xA1,0x12);     //FR1(120Hz)_T7_DE G_width

lcm_dcs_write_seq_static(ctx,0x07,0x68);     //FR1(120Hz)_Timeout RTN=3.281us
lcm_dcs_write_seq_static(ctx,0x06,0x00);     //FR1(120Hz)_Timeout RTN[10:8]
lcm_dcs_write_seq_static(ctx,0x08,0x2C);     //FR1(120Hz)_Timeout VBP
lcm_dcs_write_seq_static(ctx,0x09,0x2C);     //FR1(120Hz)_Timeout VFP
lcm_dcs_write_seq_static(ctx,0x3C,0x00);     //FR1(120Hz)_BIST VFP[14:8]
lcm_dcs_write_seq_static(ctx,0x39,0x00);     //FR1(120Hz)_RTN_SEL=0(62.5ns)
lcm_dcs_write_seq_static(ctx,0x3B,0x2C);     //FR1(120Hz)_BIST VBP
lcm_dcs_write_seq_static(ctx,0x3D,0x2C);     //FR1(120Hz)_BIST VFP
lcm_dcs_write_seq_static(ctx,0x3A,0x69);     //FR1(120Hz)_BIST RTN=3.276us

lcm_dcs_write_seq_static(ctx,0x0B,0x10);

lcm_dcs_write_seq_static(ctx,0xFF,0x5A,0xA5,0x12);
lcm_dcs_write_seq_static(ctx,0x10,0x21);     //FR2(90Hz)_t8_de
lcm_dcs_write_seq_static(ctx,0x12,0x03);     //FR2(90Hz)_t9_de
lcm_dcs_write_seq_static(ctx,0x13,0x0D);     //FR2(90Hz)_t7_de R_width
lcm_dcs_write_seq_static(ctx,0x15,0x06);     //FR2(90Hz)_sdt_de    source hold time
lcm_dcs_write_seq_static(ctx,0x35,0x1B);     //FR2(90Hz)_t7_de G_width
lcm_dcs_write_seq_static(ctx,0x11,0x01);     //FR2(90HZ)_SDT_DE    source keeping time

lcm_dcs_write_seq_static(ctx,0xC1,0x8B);     //FR2(90Hz)_Timeout RTN=4.375us
lcm_dcs_write_seq_static(ctx,0xC0,0x00);     //FR2(90Hz)_Timeout RTN[10:8]
lcm_dcs_write_seq_static(ctx,0xC2,0x2C);     //FR2(90Hz)_Timeout VBP
lcm_dcs_write_seq_static(ctx,0xC3,0x2C);     //FR2(90Hz)_Timeout VFP
lcm_dcs_write_seq_static(ctx,0x04,0x00);     //FR2(90Hz)_BIST VFP[14:8]
lcm_dcs_write_seq_static(ctx,0x01,0x00);     //FR2(90Hz)_RTN_SEL=0(62.5ns)
lcm_dcs_write_seq_static(ctx,0x03,0x2C);     //FR2(90Hz)_BIST VBP
lcm_dcs_write_seq_static(ctx,0x05,0x2C);     //FR2(90Hz)_BIST VFP
lcm_dcs_write_seq_static(ctx,0x02,0x8C);     //FR2(90Hz)_BIST RTN=4.368us
	
lcm_dcs_write_seq_static(ctx,0x40,0x23);     //FR3(60Hz)_t8_de
lcm_dcs_write_seq_static(ctx,0x42,0x05);     //FR3(60Hz)_t9_de
lcm_dcs_write_seq_static(ctx,0x43,0x32);     //FR3(60Hz)_t7_de R_width
lcm_dcs_write_seq_static(ctx,0x45,0x08);     //FR3(60Hz)_sdt_de
lcm_dcs_write_seq_static(ctx,0x61,0x26);     //FR3(60Hz)_t7_de G_width
lcm_dcs_write_seq_static(ctx,0x48,0x00);     //FR3(60Hz)_prc_de

lcm_dcs_write_seq_static(ctx,0xD1,0xD1);     //FR3(60Hz)_Timeout RTN=6.563us
lcm_dcs_write_seq_static(ctx,0xD0,0x00);     //FR3(60Hz)_Timeout RTN[10:8]
lcm_dcs_write_seq_static(ctx,0xD2,0x2C);     //FR3(60Hz)_Timeout VBP
lcm_dcs_write_seq_static(ctx,0xD3,0x2C);     //FR3(60Hz)_Timeout VFP
lcm_dcs_write_seq_static(ctx,0x0C,0x00);     //FR3(60Hz)_BIST VFP[14:8]
lcm_dcs_write_seq_static(ctx,0x09,0x00);     //FR3(60Hz)_RTN_SEL=0(62.5ns)
lcm_dcs_write_seq_static(ctx,0x0B,0x2C);     //FR3(60Hz)_BIST VBP
lcm_dcs_write_seq_static(ctx,0x0D,0x2C);     //FR3(60Hz)_BIST VFP
lcm_dcs_write_seq_static(ctx,0x0A,0xD2);     //FR3(60Hz)_BIST RTN=6.552us

lcm_dcs_write_seq_static(ctx,0x1E,0x01);     //浣庝?CKH rising EQ,楂樹?CKH falling EQ

lcm_dcs_write_seq_static(ctx,0xFF,0x5A,0xA5,0x05);
lcm_dcs_write_seq_static(ctx,0x05,0x00);
lcm_dcs_write_seq_static(ctx,0x06,0x63);     //FR1_VCOM=0.1V
lcm_dcs_write_seq_static(ctx,0x07,0x00);
lcm_dcs_write_seq_static(ctx,0x08,0x63);     //FR2_VCOM=0.1V
lcm_dcs_write_seq_static(ctx,0x09,0x00);
lcm_dcs_write_seq_static(ctx,0x0A,0x63);     //FR3_VCOM=0.1V

lcm_dcs_write_seq_static(ctx,0x72,0x76);     //VGH=10.6V
lcm_dcs_write_seq_static(ctx,0x74,0x42);     //VGL=-8V
lcm_dcs_write_seq_static(ctx,0x76,0x71);     //VGHO=9.6V
lcm_dcs_write_seq_static(ctx,0x78,0x3D);     //VGLO=-7V
lcm_dcs_write_seq_static(ctx,0x7A,0x86);     //GVDDP=5.16V
lcm_dcs_write_seq_static(ctx,0x7B,0x86);     //GVDDN=-5.16V
lcm_dcs_write_seq_static(ctx,0x5E,0x05);     //LVD IOVCC 1.4
lcm_dcs_write_seq_static(ctx,0x5F,0x33);     //LVD VSP/N +/-4

lcm_dcs_write_seq_static(ctx,0x3E,0x55);     //LVD_Debounce_time
lcm_dcs_write_seq_static(ctx,0xC6,0x80);     //修正sleep in VGH电压掉电台阶异常
lcm_dcs_write_seq_static(ctx,0xC7,0x80);
lcm_dcs_write_seq_static(ctx,0xB1,0x80);
lcm_dcs_write_seq_static(ctx,0xB2,0x80);

lcm_dcs_write_seq_static(ctx,0xFF,0x5A,0xA5,0x06);
lcm_dcs_write_seq_static(ctx,0xCD,0x14);     //Res=1080*2460
lcm_dcs_write_seq_static(ctx,0xCE,0x9C);     //Res=1080*2460
lcm_dcs_write_seq_static(ctx,0xCF,0x09);     //Res=1080*2460
lcm_dcs_write_seq_static(ctx,0xC3,0x06);     //ss_reg

lcm_dcs_write_seq_static(ctx,0x11,0x08);

lcm_dcs_write_seq_static(ctx,0xFF,0x5A,0xA5,0x07);
lcm_dcs_write_seq_static(ctx,0x29,0x01);     //01: DSC on / 00:  DSC off
lcm_dcs_write_seq_static(ctx,0x83,0x20);     //Calibration

lcm_dcs_write_seq_static(ctx,0xFF,0x5A,0xA5,0x17);    //slice 12 normal
lcm_dcs_write_seq_static(ctx,0x20,0x00,0x00,0x00,0x00,0x00,0x11,0x00,0x00,0x89,0x30,0x80,0x09,0x9c,0x04,0x38,0x00,0x0c,0x02,0x1c,0x02,0x1c,0x02,0x00,0x02,0x0e,0x00,0x20,0x01,0x1f,0x00,0x07,0x00,0x0c,0x08,0xbb,0x08,0x7a,0x18,0x00,0x10,0xf0,0x03,0x0c,0x20,0x00,0x06,0x0b,0x0b,0x33,0x0e,0x1c,0x2a,0x38,0x46,0x54,0x62,0x69,0x70,0x77,0x79,0x7b,0x7d,0x7e,0x01,0x02,0x01,0x00,0x09,0x40,0x09,0xbe,0x19,0xfc,0x19,0xfa,0x19,0xf8,0x1a,0x38,0x1a,0x78,0x1a,0xb6,0x2a,0xf6,0x2b,0x34,0x2b,0x74,0x3b,0x74,0x6b,0xf4,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);


lcm_dcs_write_seq_static(ctx,0xFF,0x5A,0xA5,0x08);
lcm_dcs_write_seq_static(ctx,0xE0,0x00,0x00,0x1B,0x46,0x00,0x84,0xB0,0xD4,0x15,0x0C,0x36,0x76,0x25,0xA5,0xED,0x28,0x2A,0x60,0x9F,0xC6,0x3E,0xF9,0x1C,0x46,0x3F,0x62,0x85,0xB0,0x0F,0xCB,0xE1);
lcm_dcs_write_seq_static(ctx,0xE1,0x00,0x00,0x1B,0x46,0x00,0x84,0xB0,0xD4,0x15,0x0C,0x36,0x76,0x25,0xA5,0xED,0x28,0x2A,0x60,0x9F,0xC6,0x3E,0xF9,0x1C,0x46,0x3F,0x62,0x85,0xB0,0x0F,0xCB,0xE1);


lcm_dcs_write_seq_static(ctx,0xFF,0x5A,0xA5,0x0B);
lcm_dcs_write_seq_static(ctx,0xC0,0x84);     //FR1(120Hz)_VDO_DIV_SEL (Frame)
lcm_dcs_write_seq_static(ctx,0xC1,0x12);     //FR1(120Hz)_VDO_CNT_IDEAL[12:0]
lcm_dcs_write_seq_static(ctx,0xC2,0x06);     //FR1(120Hz)_UB [4:0]
lcm_dcs_write_seq_static(ctx,0xC3,0x06);     //FR1(120Hz)_LB [4:0]
lcm_dcs_write_seq_static(ctx,0xC4,0x63);     //FR1(120Hz)_Keep trim code range MAX[7:0]
lcm_dcs_write_seq_static(ctx,0xC5,0x63);     //FR1(120Hz)_Keep trim code range MIN[7:0]
lcm_dcs_write_seq_static(ctx,0xD2,0x43);     //FR1(120Hz)_VDO_LN_DIV_SEL, cnt_line
lcm_dcs_write_seq_static(ctx,0xD3,0x46);     //FR1(120Hz)_VDO_CNT_IDEAL[12:0]
lcm_dcs_write_seq_static(ctx,0xD4,0x03);     //FR1(120Hz)_UB [4:0]
lcm_dcs_write_seq_static(ctx,0xD5,0x03);     //FR1(120Hz)_LB [4:0]
lcm_dcs_write_seq_static(ctx,0xD6,0x52);     //FR1(120Hz)_Keep trim code range MAX[7:0]
lcm_dcs_write_seq_static(ctx,0xD7,0x52);     //FR1(120Hz)_Keep trim code range MIN[7:0]

lcm_dcs_write_seq_static(ctx,0xC6,0x85);     //FR2(90Hz)_VDO_DIV_SEL (Frame)
lcm_dcs_write_seq_static(ctx,0xC7,0x6D);     //FR2(90Hz)_VDO_CNT_IDEAL[12:0]
lcm_dcs_write_seq_static(ctx,0xC8,0x07);     //FR2(90Hz)_UB [4:0]
lcm_dcs_write_seq_static(ctx,0xC9,0x07);     //FR2(90Hz)_LB [4:0]
lcm_dcs_write_seq_static(ctx,0xCA,0x84);     //FR2(90Hz)_Keep trim code range MAX[7:0]
lcm_dcs_write_seq_static(ctx,0xCB,0x84);     //FR2(90Hz)_Keep trim code range MIN[7:0]
lcm_dcs_write_seq_static(ctx,0xD8,0x44);     //FR2(90Hz)_VDO_LN_DIV_SEL, cnt_line
lcm_dcs_write_seq_static(ctx,0xD9,0x5D);     //FR2(90Hz)_VDO_CNT_IDEAL[12:0]
lcm_dcs_write_seq_static(ctx,0xDA,0x03);     //FR2(90Hz)_UB [4:0]
lcm_dcs_write_seq_static(ctx,0xDB,0x03);     //FR2(90Hz)_LB [4:0]
lcm_dcs_write_seq_static(ctx,0xDC,0x6D);     //FR2(90Hz)_Keep trim code range MAX[7:0]
lcm_dcs_write_seq_static(ctx,0xDD,0x6D);     //FR2(90Hz)_Keep trim code range MIN[7:0]

lcm_dcs_write_seq_static(ctx,0x94,0x88);     //FR3(60Hz)_VDO_DIV_SEL (Frame)
lcm_dcs_write_seq_static(ctx,0x95,0x25);     //FR3(60Hz)_VDO_CNT_IDEAL[12:0]
lcm_dcs_write_seq_static(ctx,0x96,0x0B);     //FR3(60Hz)_UB [4:0]
lcm_dcs_write_seq_static(ctx,0x97,0x0B);     //FR3(60Hz)_LB [4:0]
lcm_dcs_write_seq_static(ctx,0x98,0xC6);     //FR3(60Hz)_Keep trim code range MAX[7:0]
lcm_dcs_write_seq_static(ctx,0x99,0xC6);     //FR3(60Hz)_Keep trim code range MIN[7:0]
lcm_dcs_write_seq_static(ctx,0x9A,0x46);     //FR3(60Hz)_VDO_LN_DIV_SEL, cnt_line
lcm_dcs_write_seq_static(ctx,0x9B,0x8B);     //FR3(60Hz)_VDO_CNT_IDEAL[12:0]
lcm_dcs_write_seq_static(ctx,0x9C,0x05);     //FR3(60Hz)_UB [4:0]
lcm_dcs_write_seq_static(ctx,0x9D,0x05);     //FR3(60Hz)_LB [4:0]
lcm_dcs_write_seq_static(ctx,0x9E,0xA3);     //FR3(60Hz)_Keep trim code range MAX[7:0]
lcm_dcs_write_seq_static(ctx,0x9F,0xA3);     //FR3(60Hz)_Keep trim code range MIN[7:0]

lcm_dcs_write_seq_static(ctx,0xAA,0x12);     //D[4]: Trim_ln_num
lcm_dcs_write_seq_static(ctx,0xAB,0xE0);     //OSC auto trim en

lcm_dcs_write_seq_static(ctx,0xFF,0x5A,0xA5,0x0C);     //TP Modulation
lcm_dcs_write_seq_static(ctx,0x00,0x3F);     //FR1(120Hz)
lcm_dcs_write_seq_static(ctx,0x01,0x64);     //FR1(120Hz)
lcm_dcs_write_seq_static(ctx,0x0C,0x3F);     //FR2(90Hz)
lcm_dcs_write_seq_static(ctx,0x0D,0x64);     //FR2(90Hz)
lcm_dcs_write_seq_static(ctx,0x18,0x25);     //FR3(60Hz)
lcm_dcs_write_seq_static(ctx,0x19,0x64);     //FR3(60Hz)

lcm_dcs_write_seq_static(ctx,0xFF,0x5A,0xA5,0x0E);
lcm_dcs_write_seq_static(ctx,0x00,0x83);      //LH mode
lcm_dcs_write_seq_static(ctx,0x02,0x0F);     //vbp_tpv
lcm_dcs_write_seq_static(ctx,0x04,0x8A);     //TSVD/HD setting
lcm_dcs_write_seq_static(ctx,0x13,0x04);     //LV_TSHD_pos
lcm_dcs_write_seq_static(ctx,0xB0,0x01);     //TP1_num
lcm_dcs_write_seq_static(ctx,0xC0,0x12);     //TP3_num
lcm_dcs_write_seq_static(ctx,0xD0,0x15);

lcm_dcs_write_seq_static(ctx,0x24,0x81);     //FR1(120Hz)_TSVD period
lcm_dcs_write_seq_static(ctx,0x2F,0x00);     //FR1(120Hz)_TSHD in V-porch
lcm_dcs_write_seq_static(ctx,0x21,0x36);     //FR1(120Hz)_LH_TSVD1_pos
lcm_dcs_write_seq_static(ctx,0x23,0x44);     //FR1(120Hz)_LH_TSVD1_width
lcm_dcs_write_seq_static(ctx,0x22,0x36);     //FR1(120Hz)_LH_TSVD2_pos
lcm_dcs_write_seq_static(ctx,0x20,0x03);     //FR1(120Hz)_TP term num
lcm_dcs_write_seq_static(ctx,0x25,0x22);     //FR1(120Hz)_TP2_unit0=275.4us
lcm_dcs_write_seq_static(ctx,0x26,0x6D);     //FR1(120Hz)_TP2_unit0=275.4us
lcm_dcs_write_seq_static(ctx,0x27,0x20);     //FR1(120Hz)_unit_line_num
lcm_dcs_write_seq_static(ctx,0x28,0x67);     //FR1(120Hz)_unit_line_num
lcm_dcs_write_seq_static(ctx,0xB1,0x5A);     //FR1(120Hz)_TP1_period
lcm_dcs_write_seq_static(ctx,0xC2,0x5A);     //FR1(120Hz)_TP3-8 period
lcm_dcs_write_seq_static(ctx,0xC3,0x5A);     //FR1(120Hz)_TP3-7 period
lcm_dcs_write_seq_static(ctx,0xC4,0x5A);     //FR1(120Hz)_TP3-6 period
lcm_dcs_write_seq_static(ctx,0xC5,0x5A);     //FR1(120Hz)_TP3-5 period
lcm_dcs_write_seq_static(ctx,0xC6,0x5A);     //FR1(120Hz)_TP3-4 period
lcm_dcs_write_seq_static(ctx,0xC7,0x5A);     //FR1(120Hz)_TP3-3 period
lcm_dcs_write_seq_static(ctx,0xC8,0x5A);     //FR1(120Hz)_TP3-2 period
lcm_dcs_write_seq_static(ctx,0xC9,0x5A);     //FR1(120Hz)_TP3-1 period
lcm_dcs_write_seq_static(ctx,0x2A,0x59);     //FR1(120Hz)_RTN=2.813us
lcm_dcs_write_seq_static(ctx,0x2D,0x00);     //FR1(120Hz)_RTN[9:8]
lcm_dcs_write_seq_static(ctx,0x29,0x00);     //FR1(120Hz)_TPM_step=0
lcm_dcs_write_seq_static(ctx,0x30,0x1F);     //FR1(120Hz)_VFP_1TSHD_RISE
lcm_dcs_write_seq_static(ctx,0x2E,0x0E);     //FR1(120Hz)_pwr_early= 84.4us

lcm_dcs_write_seq_static(ctx,0x31,0x00);     //FR1(120Hz)_Qsync_T1
lcm_dcs_write_seq_static(ctx,0x32,0xC1);     //00);     //FR1(120Hz)_Qsync_T3
lcm_dcs_write_seq_static(ctx,0x35,0x00);     //FR1(120Hz)_Qsync_T4
lcm_dcs_write_seq_static(ctx,0x33,0x22);     //FR1(120Hz)_Qsync_T2=275.53us
lcm_dcs_write_seq_static(ctx,0x34,0x6D);     //FR1(120Hz)_Qsync_T2
lcm_dcs_write_seq_static(ctx,0x36,0x00);     //FR1(120Hz)_Qsync_T5
lcm_dcs_write_seq_static(ctx,0x3A,0xC1);     //00);     //FR1(120Hz)_Qsync_T7
lcm_dcs_write_seq_static(ctx,0x37,0x00);     //FR1(120Hz)_Qsync_T6
lcm_dcs_write_seq_static(ctx,0x3B,0x14);    //00);     //FR1(120Hz)_Qsync_T3、T7
lcm_dcs_write_seq_static(ctx,0x39,0x01);     //FR1(120Hz)_Qsync-TSVD period

lcm_dcs_write_seq_static(ctx,0xE0,0x0E);     //FR1(120Hz)_T8_TP3
lcm_dcs_write_seq_static(ctx,0xE2,0x03);     //FR1(120Hz)_T9_TP3
lcm_dcs_write_seq_static(ctx,0xE3,0x16);     //FR1(120Hz)_T7_TP3 R_width
lcm_dcs_write_seq_static(ctx,0xE5,0x07);     //FR1(120Hz)_SDT_TP3
lcm_dcs_write_seq_static(ctx,0xEE,0x12);     //FR1(120Hz)_T7_TP3 G_width

lcm_dcs_write_seq_static(ctx,0x44,0x83);     //FR2(90Hz)_TSVD period
lcm_dcs_write_seq_static(ctx,0x4F,0x00);     //FR2(90Hz)_TSHD in V-porch
lcm_dcs_write_seq_static(ctx,0x41,0x28);     //FR2(90Hz)_LH_TSVD1_pos
lcm_dcs_write_seq_static(ctx,0x43,0x33);     //FR2(90Hz)_LH_TSVD1_width
lcm_dcs_write_seq_static(ctx,0x42,0x28);     //FR2(90Hz)_LH_TSVD2_pos
lcm_dcs_write_seq_static(ctx,0x40,0x07);     //FR2(90Hz)_TP term num
lcm_dcs_write_seq_static(ctx,0x45,0x16);     //FR2(90Hz)_TP2_unit0=177.9us
lcm_dcs_write_seq_static(ctx,0x46,0x3B);     //FR2(90Hz)_TP2_unit0=177.9us
lcm_dcs_write_seq_static(ctx,0x47,0x10);     //FR2(90Hz)_unit_line_num
lcm_dcs_write_seq_static(ctx,0x48,0x34);     //FR2(90Hz)_unit_line_num
lcm_dcs_write_seq_static(ctx,0x4A,0x77);     //FR2(90Hz)_RTN=3.75us
lcm_dcs_write_seq_static(ctx,0x4D,0x00);     //FR2(90Hz)_RTN[9:8]
lcm_dcs_write_seq_static(ctx,0x49,0x00);     //FR2(90Hz)_TPM_step=0
lcm_dcs_write_seq_static(ctx,0x50,0x1F);     //FR2(90Hz)_VFP_1TSHD_RISE
lcm_dcs_write_seq_static(ctx,0x4E,0x0A);     //FR2(90Hz)_pwr_early= 82.5us

lcm_dcs_write_seq_static(ctx,0x51,0x00);     //FR2(90Hz)_Qsync_T1
lcm_dcs_write_seq_static(ctx,0x52,0x00);     //FR2(90Hz)_Qsync_T3
lcm_dcs_write_seq_static(ctx,0x55,0x00);     //FR2(90Hz)_Qsync_T4
lcm_dcs_write_seq_static(ctx,0x53,0x16);     //FR2(90Hz)_Qsync_T2=177.84us
lcm_dcs_write_seq_static(ctx,0x54,0x3B);     //FR2(90Hz)_Qsync_T2
lcm_dcs_write_seq_static(ctx,0x56,0x00);     //FR2(90Hz)_Qsync_T5
lcm_dcs_write_seq_static(ctx,0x5A,0x00);     //FR2(90Hz)_Qsync_T7
lcm_dcs_write_seq_static(ctx,0x57,0x00);     //FR2(90Hz)_Qsync_T6
lcm_dcs_write_seq_static(ctx,0x5B,0x00);     //FR2(90Hz)_Qsync_T3、T7
lcm_dcs_write_seq_static(ctx,0x59,0x03);     //FR2(90Hz)_Qsync-TSVD period


lcm_dcs_write_seq_static(ctx,0x64,0x83);     //FR3(60Hz)_TSVD period
lcm_dcs_write_seq_static(ctx,0x6F,0x00);     //FR3(60Hz)_TSHD in V-porch
lcm_dcs_write_seq_static(ctx,0x61,0x1A);     //FR3(60Hz)_LH_TSVD1_pos
lcm_dcs_write_seq_static(ctx,0x63,0x22);     //FR3(60Hz)_LH_TSVD1_width
lcm_dcs_write_seq_static(ctx,0x62,0x1A);     //FR3(60Hz)_LH_TSVD2_pos
lcm_dcs_write_seq_static(ctx,0x60,0x07);     //FR3(60Hz)_TP term num
lcm_dcs_write_seq_static(ctx,0x65,0x15);     //FR3(60Hz)_TP2_unit0=169.8us
lcm_dcs_write_seq_static(ctx,0x66,0x37);     //FR3(60Hz)_TP2_unit0=169.8us
lcm_dcs_write_seq_static(ctx,0x67,0x10);     //FR3(60Hz)_unit_line_num
lcm_dcs_write_seq_static(ctx,0x68,0x34);     //FR3(60Hz)_unit_line_num
lcm_dcs_write_seq_static(ctx,0x6A,0xBD);     //FR3(60Hz)_RTN=5.938us
lcm_dcs_write_seq_static(ctx,0x6D,0x00);     //FR3(60Hz)_RTN[9:8]
lcm_dcs_write_seq_static(ctx,0x69,0x00);     //FR3(60Hz)_TPM_step=0
lcm_dcs_write_seq_static(ctx,0x70,0x1F);     //FR3(60Hz)_VFP_1TSHD_RISE
lcm_dcs_write_seq_static(ctx,0x6E,0x06);     //FR3(60Hz)_pwr_early= 83.1us

lcm_dcs_write_seq_static(ctx,0x71,0x00);     //FR3(60Hz)_Qsync_T1
lcm_dcs_write_seq_static(ctx,0x72,0x08);     //FR3(60Hz)_Qsync_T3
lcm_dcs_write_seq_static(ctx,0x75,0x00);     //FR3(60Hz)_Qsync_T4
lcm_dcs_write_seq_static(ctx,0x73,0x15);     //FR3(60Hz)_Qsync_T2=169.72us
lcm_dcs_write_seq_static(ctx,0x74,0x37);     //FR3(60Hz)_Qsync_T2
lcm_dcs_write_seq_static(ctx,0x76,0x00);     //FR3(60Hz)_Qsync_T5
lcm_dcs_write_seq_static(ctx,0x7A,0x08);     //FR3(60Hz)_Qsync_T7
lcm_dcs_write_seq_static(ctx,0x77,0x00);     //FR3(60Hz)_Qsync_T6
lcm_dcs_write_seq_static(ctx,0x7B,0x14);     //FR3(60Hz)_Qsync_T3、T7
lcm_dcs_write_seq_static(ctx,0x79,0x03);     //FR3(60Hz)_Qsync-TSVD period


lcm_dcs_write_seq_static(ctx,0xFF,0x5A,0xA5,0x1E);
lcm_dcs_write_seq_static(ctx,0x0A,0x21);     //FR2(90Hz)_T8_TP3
lcm_dcs_write_seq_static(ctx,0x0C,0x03);     //FR2(90Hz)_T9_TP3
lcm_dcs_write_seq_static(ctx,0x0D,0x0D);     //FR2(90Hz)_T7_TP3 R_width
lcm_dcs_write_seq_static(ctx,0x0E,0x06);     //FR2(90Hz)_SDT_TP3
lcm_dcs_write_seq_static(ctx,0x14,0x1B);     //FR2(90Hz)_T7_TP3 G_width

lcm_dcs_write_seq_static(ctx,0x00,0x78);     //FR2(90Hz)_TP1_period
lcm_dcs_write_seq_static(ctx,0x02,0x78);     //FR2(90Hz)_TP3-8 period
lcm_dcs_write_seq_static(ctx,0x03,0x78);     //FR2(90Hz)_TP3-7 period
lcm_dcs_write_seq_static(ctx,0x04,0x78);     //FR2(90Hz)_TP3-6 period
lcm_dcs_write_seq_static(ctx,0x05,0x78);     //FR2(90Hz)_TP3-5 period
lcm_dcs_write_seq_static(ctx,0x06,0x78);     //FR2(90Hz)_TP3-4 period
lcm_dcs_write_seq_static(ctx,0x07,0x78);     //FR2(90Hz)_TP3-3 period
lcm_dcs_write_seq_static(ctx,0x08,0x78);     //FR2(90Hz)_TP3-2 period
lcm_dcs_write_seq_static(ctx,0x09,0x78);     //FR2(90Hz)_TP3-1 period

lcm_dcs_write_seq_static(ctx,0x20,0x23);     //FR3(60Hz)_T8_TP3
lcm_dcs_write_seq_static(ctx,0x22,0x05);     //FR3(60Hz)_T9_TP3
lcm_dcs_write_seq_static(ctx,0x23,0x32);     //FR3(60Hz)_T7_TP3 R_width
lcm_dcs_write_seq_static(ctx,0x24,0x08);     //FR3(60Hz)_SDT_TP3
lcm_dcs_write_seq_static(ctx,0x2A,0x26);     //FR3(60Hz)_T7_TP3 G_width

lcm_dcs_write_seq_static(ctx,0x16,0xBE);     //FR3(60Hz)_TP1_period
lcm_dcs_write_seq_static(ctx,0x18,0xBE);     //FR3(60Hz)_TP3-8 period
lcm_dcs_write_seq_static(ctx,0x19,0xBE);     //FR3(60Hz)_TP3-7 period
lcm_dcs_write_seq_static(ctx,0x1A,0xBE);     //FR3(60Hz)_TP3-6 period
lcm_dcs_write_seq_static(ctx,0x1B,0xBE);     //FR3(60Hz)_TP3-5 period
lcm_dcs_write_seq_static(ctx,0x1C,0xBE);     //FR3(60Hz)_TP3-4 period
lcm_dcs_write_seq_static(ctx,0x1D,0xBE);     //FR3(60Hz)_TP3-3 period
lcm_dcs_write_seq_static(ctx,0x1E,0xBE);     //FR3(60Hz)_TP3-2 period
lcm_dcs_write_seq_static(ctx,0x1F,0xBE);     //FR3(60Hz)_TP3-1 period

//bist
//MipiWrite(0xFF,0x5A,0xA5,0x02);
//MipiWrite(0x3F,0x01);

lcm_dcs_write_seq_static(ctx,0xFF,0x5A,0xA5,0x00);     //Page0

lcm_dcs_write_seq_static(ctx,0x11);
msleep(130);
lcm_dcs_write_seq_static(ctx,0x29);
msleep(30);
lcm_dcs_write_seq_static(ctx,0x35,0x01);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;
	pr_info("%s\n", __func__);
	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}
	pr_info("%s_end\n", __func__);
	ctx->enabled = false;

	return 0;
}

//extern bool ilitek_is_gesture_wakeup_enabled(void);
static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s\n", __func__);

	if (!ctx->prepared)
		return 0;
    //-------> 1:28 10
	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(50);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(150);

	ctx->error = 0;
	ctx->prepared = false;

	//if (ilitek_is_gesture_wakeup_enabled() == 0) {
//prize add by lipenpeng 20220813 start   //-------> 4: close LCD RESET
		ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
			dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
				__func__, PTR_ERR(ctx->reset_gpio));
			return PTR_ERR(ctx->reset_gpio);
		}
		gpiod_set_value(ctx->reset_gpio, 1);
		msleep(2);
		gpiod_set_value(ctx->reset_gpio, 0);
		msleep(2);
		gpiod_set_value(ctx->reset_gpio, 1);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	//}

//prize add by lipenpeng 20220813 end 
//#else
#if 0
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	if (ilitek_is_gesture_wakeup_enabled() == 0) {
		ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
			"bias", 1, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_neg)) {
			dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
				__func__, PTR_ERR(ctx->bias_neg));
			return PTR_ERR(ctx->bias_neg);
		}
		gpiod_set_value(ctx->bias_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);

		udelay(1000);

		ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
			"bias", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_pos)) {
			dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
				__func__, PTR_ERR(ctx->bias_pos));
			return PTR_ERR(ctx->bias_pos);
		}
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
	}
#endif

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_enable();  //1:?¨¤5V

	
#else
	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	udelay(2000);

	ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
		"bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
#if defined(CONFIG_PRIZE_LCD_BIAS)
	display_bias_enable_v(5600);
#endif
#endif

	lcm_panel_init(ctx);  //3: LCD reset--->4:init lcd

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif
	pr_info("%s-\n", __func__);
	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}
#define HFP (22)
#define HSA (4)
#define HBP (20)
#define VFP (44)
#define VSA (4)
#define VBP (40)
#define VAC (2460)
#define HAC (1080)

static u32 fake_heigh = 2460;
static u32 fake_width = 1080;
static bool need_fake_resolution;

static struct drm_display_mode performance_mode_120 = {
	.clock = 344285,		//htotal*vtotal*vrefresh/1000   1120*2500+40*120/1000
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end = VAC + VFP + VSA,
	.vtotal = VAC + VFP + VSA + VBP,
	.vrefresh = 120,
};

static struct drm_display_mode performance_mode_90 = {
	.clock = 344285,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + 890,
	.vsync_end = VAC + 890 + VSA,
	.vtotal = VAC + 890 + VSA + VBP,
	.vrefresh = 90,
};

static struct drm_display_mode default_mode = {
	.clock = 344285,		//htotal*vtotal*vrefresh/1000   1138*2492*60/1000
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + 2587,
	.vsync_end = VAC + 2587 + VSA,
	.vtotal = VAC + 2587 + VSA + VBP,
	.vrefresh = 60,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params_120 = {
	//.vfp_low_power = 2540,//60hz
	.pll_clk = 325,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x1C,
	},
	.data_rate = 650,
	.is_cphy = 1,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2460,
		.pic_width = 1080,
		.slice_height = 12,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 287,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2235,
		.slice_bpg_offset = 2170,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.dyn = {
		.switch_en = 1,
		.data_rate = 650,
		.hfp = 22,
		.vfp = 44,
	},
	.lfr_enable = 1,
	.lfr_minimum_fps = 60,
};
static struct mtk_panel_params ext_params_90 = {
	//.vfp_low_power = 2540,//60hz
	.pll_clk = 325,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x1C,
	},
	.data_rate = 650,
	.is_cphy = 1,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2460,
		.pic_width = 1080,
		.slice_height = 12,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 287,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2235,
		.slice_bpg_offset = 2170,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,

		},
	.dyn = {
		.switch_en = 1,
		.data_rate = 650,
		.hfp = 22,
		.vfp = 890,
	},
	.lfr_enable = 1,
	.lfr_minimum_fps = 60,
};
static struct mtk_panel_params ext_params_60 = {
	//.vfp_low_power = 2540,//60hz
	.pll_clk = 325,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x1C,
	},
	.data_rate = 650,
	.is_cphy = 1,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2460,
		.pic_width = 1080,
		.slice_height = 12,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 287,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 2235,
		.slice_bpg_offset = 2170,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.dyn = {
		.switch_en = 1,
		.data_rate = 650,
		.hfp = 22,
		.vfp = 2587,
	},
	.lfr_enable = 1,
	.lfr_minimum_fps = 60,
};

static struct drm_display_mode *get_mode_by_id(struct drm_panel *panel,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &panel->connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}
static int current_fps = 60;

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			 unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(panel, mode);


	if (m->vrefresh == 60)
		ext->params = &ext_params_60;
	else if (m->vrefresh == 90)
		ext->params = &ext_params_90;
	else if (m->vrefresh == 120)
		ext->params = &ext_params_120;
	else
		ret = 1;
	if (!ret)
		current_fps = m->vrefresh;
	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	pr_info("%s success\n", __func__);
	return 1;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb0[] = {0x51, 0xFF};

	bl_tb0[1] = level;

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 0;
}

//static int lcm_get_virtual_heigh(void)
//{
//	return VAC;
//}

//static int lcm_get_virtual_width(void)
//{
//	return HAC;
//}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.ata_check = panel_ata_check,
	//.get_virtual_heigh = lcm_get_virtual_heigh,
	//.get_virtual_width = lcm_get_virtual_width,
};
#endif

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static void change_drm_disp_mode_params(struct drm_display_mode *mode)
{
	if (fake_heigh > 0 && fake_heigh < VAC) {
		mode->vsync_start = mode->vsync_start - mode->vdisplay
					+ fake_heigh;
		mode->vsync_end = mode->vsync_end - mode->vdisplay + fake_heigh;
		mode->vtotal = mode->vtotal - mode->vdisplay + fake_heigh;
		mode->vdisplay = fake_heigh;
	}
	if (fake_width > 0 && fake_width < HAC) {
		mode->hsync_start = mode->hsync_start - mode->hdisplay
					+ fake_width;
		mode->hsync_end = mode->hsync_end - mode->hdisplay + fake_width;
		mode->htotal = mode->htotal - mode->hdisplay + fake_width;
		mode->hdisplay = fake_width;
	}
}

static int lcm_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode_60;
	struct drm_display_mode *mode_90;
	struct drm_display_mode *mode_120;
	if (need_fake_resolution)
		change_drm_disp_mode_params(&default_mode);

	mode_60 = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode_60) {
		dev_info(panel->drm->dev, "failed to add mode_60 %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode_60);
	mode_60->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode_60);


	mode_90 = drm_mode_duplicate(panel->drm, &performance_mode_90);
	if (!mode_90) {
		dev_info(panel->drm->dev, "failed to add mode_90 %ux%ux@%u\n",
			performance_mode_90.hdisplay, performance_mode_90.vdisplay,
			performance_mode_90.vrefresh);
		return -ENOMEM;
	}
	drm_mode_set_name(mode_90);
	mode_90->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode_90);
	
	
	mode_120 = drm_mode_duplicate(panel->drm, &performance_mode_120);
	if (!mode_120) {
		dev_info(panel->drm->dev, "failed to add mode_120 %ux%ux@%u\n",
			performance_mode_120.hdisplay, performance_mode_120.vdisplay,
			performance_mode_120.vrefresh);
		return -ENOMEM;
	}
	drm_mode_set_name(mode_120);
	mode_120->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode_120);
	panel->connector->display_info.width_mm = 69;
	panel->connector->display_info.height_mm = 158;//sqrt((size*25.4)^2/(2340^2+1080^2))*2340
//prize add by lipengpeng 20220208 end 
	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static void check_is_need_fake_resolution(struct device *dev)
{
	unsigned int ret = 0;

	ret = of_property_read_u32(dev->of_node, "fake_heigh", &fake_heigh);
	if (ret)
		need_fake_resolution = false;
	ret = of_property_read_u32(dev->of_node, "fake_width", &fake_width);
	if (ret)
		need_fake_resolution = false;
	if (fake_heigh > 0 && fake_heigh < VAC)
		need_fake_resolution = false;
	if (fake_width > 0 && fake_width < HAC)
		need_fake_resolution = false;
}

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	pr_info("%s+\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 3;
	dsi->format = MIPI_DSI_FMT_RGB888;
//prize add by lipengpeng 20220112 start 
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE
			 | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS;
//prize add by lipengpeng 20220112 end 
	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);
#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_enable();
#else
	ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(dev, "%s: cannot get bias-pos 0 %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	devm_gpiod_put(dev, ctx->bias_pos);

	ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(dev, "%s: cannot get bias-neg 1 %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	devm_gpiod_put(dev, ctx->bias_neg);
#endif
	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params_60, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif
	check_is_need_fake_resolution(dev);
	pr_info("%s-\n", __func__);
	
#if defined(CONFIG_PRIZE_HARDWARE_INFO)
    strcpy(current_lcm_info.chip,"ili77600a");
    strcpy(current_lcm_info.vendor,"wanchanglong");
    sprintf(current_lcm_info.id,"0x%02x",0x02);
    strcpy(current_lcm_info.more,"1080x2460");
#endif
//prize add by anhengxuan for lcd hardware info 20220402 end
	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "truly,ili77600a,vdo-wcl,a500", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-truly-ili77600a-vdo-wcl-a500",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("MEDIATEK");
MODULE_DESCRIPTION("sc ili77600a VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");
