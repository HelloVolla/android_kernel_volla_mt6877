/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/backlight.h>
#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_panel_ext.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

//prize add by lvyuanchuan for lcd hardware info 20220331 start
#if defined(CONFIG_PRIZE_HARDWARE_INFO)
#include "../../../misc/mediatek/hardware_info/hardware_info.h"
extern struct hardware_info current_lcm_info;
#endif
//prize add by lvyuanchuan for lcd hardware info 20220331 end
/* prize add by liaoxingen for avdd/avee start */
extern int tps65132_set_vpos_volt(int);
extern int tps65132_set_vneg_volt(int);
/* prize add by liaoxingen for avdd/avee end */

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos, *bias_neg;
	struct gpio_desc *pm_enable_gpio;
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





static void lcm_panel_init(struct lcm *ctx)
{
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(1 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
#if BITS_PER_LONG == 32
	mdelay(10 * 1000); /* udelay not allowed > 2000 in 32 bit */
#else
	udelay(10 * 1000);
#endif
	gpiod_set_value(ctx->reset_gpio, 1);
#if BITS_PER_LONG == 32
	mdelay(10 * 1000);
#else
	udelay(10 * 1000);
#endif
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

		//--------------CMD WR enable--------------//
	lcm_dcs_write_seq_static(ctx,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xFF,0x80,0x57,0x01); 

	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xFF,0x80,0x57);

	//--------------Panel Resolution --------------//
	lcm_dcs_write_seq_static(ctx,0x00, 0x00);
	lcm_dcs_write_seq_static(ctx,0x2A, 0x00, 0x00, 0x02, 0xCF); //720

	lcm_dcs_write_seq_static(ctx,0x00, 0x00);
	lcm_dcs_write_seq_static(ctx,0x2B, 0x00, 0x00, 0x06, 0x4B); //1612

	lcm_dcs_write_seq_static(ctx,0x00,0xA3);  //Y=1612
	lcm_dcs_write_seq_static(ctx,0xB3,0x06,0x4C,0x00,0x18);

	//--------------Voltage set--------------//
	lcm_dcs_write_seq_static(ctx,0x00,0x93);//VGH_N 18V
	lcm_dcs_write_seq_static(ctx,0xC5,0x75);

	lcm_dcs_write_seq_static(ctx,0x00,0x97);//VGH_I 18V
	lcm_dcs_write_seq_static(ctx,0xC5,0x75);

	lcm_dcs_write_seq_static(ctx,0x00,0x9A);//VGL_N -12V 
	lcm_dcs_write_seq_static(ctx,0xC5,0x41);

	lcm_dcs_write_seq_static(ctx,0x00,0x9C);//VGL_I -12V 
	lcm_dcs_write_seq_static(ctx,0xC5,0x41);

	lcm_dcs_write_seq_static(ctx,0x00,0xB6); //VGHO1_N_I=17v
	lcm_dcs_write_seq_static(ctx,0xC5,0x61,0x61);

	lcm_dcs_write_seq_static(ctx,0x00,0xB8); //VGLO1_N_I -11V
	lcm_dcs_write_seq_static(ctx,0xC5,0x37,0x37); 

	lcm_dcs_write_seq_static(ctx,0x00,0x00); 
	lcm_dcs_write_seq_static(ctx,0xD8,0x33,0x33);  //GVDDP/N 5.4V  -5.4V
	//lcm_dcs_write_seq_static(ctx,0x00,0x07); 
	//lcm_dcs_write_seq_static(ctx,0xD9,0x00,0xC0,0xC0);  //VCOM(-1.5V) 

	lcm_dcs_write_seq_static(ctx,0x00,0x82); 
	lcm_dcs_write_seq_static(ctx,0xC5,0x55);  //LVD

	lcm_dcs_write_seq_static(ctx,0x00,0x83); 
	lcm_dcs_write_seq_static(ctx,0xC5,0x07);  //LVD Enable

	//Vgh_s_sel>>vgh 眏
	lcm_dcs_write_seq_static(ctx,0x00,0x96);
	lcm_dcs_write_seq_static(ctx,0xf5,0x0d);
	 
	//Vgl_s_sel>>vgl 眏
	lcm_dcs_write_seq_static(ctx,0x00,0x86);
	lcm_dcs_write_seq_static(ctx,0xf5,0x0d);
	 
	// VGH CLK Line Rate(1 Line)
	lcm_dcs_write_seq_static(ctx,0x00,0x94);
	lcm_dcs_write_seq_static(ctx,0xC5,0x15);
	 
	// VGL CLK Line Rate(1 Line)
	lcm_dcs_write_seq_static(ctx,0x00,0x9B);
	lcm_dcs_write_seq_static(ctx,0xC5,0x51);

	lcm_dcs_write_seq_static(ctx,0x00,0xA3);  //GVDD_EN
	lcm_dcs_write_seq_static(ctx,0xA5,0x04); 

	lcm_dcs_write_seq_static(ctx,0x00,0x99);  
	lcm_dcs_write_seq_static(ctx,0xCF,0x56); 

	lcm_dcs_write_seq_static(ctx,0x00,0x86);  
	lcm_dcs_write_seq_static(ctx,0xB7,0x80); //I2C EN

	lcm_dcs_write_seq_static(ctx,0x00,0xA5);  
	lcm_dcs_write_seq_static(ctx,0xB0,0x1D); //

	//--------------Gamma setting--------------//

	//lcm_dcs_write_seq_static(ctx,0x00,0x00);
	//lcm_dcs_write_seq_static(ctx,0xE1,0x0D,0x11,0x1B,0x26,0x2E,0x36,0x46,0x52,0x53,0x60,0x63,0x79,0x88,0x73,0x71,0x65);
	//lcm_dcs_write_seq_static(ctx,0x00,0x10);
	//lcm_dcs_write_seq_static(ctx,0xE1,0x5C,0x4F,0x3F,0x35,0x2C,0x1D,0x10,0x0F);

	//lcm_dcs_write_seq_static(ctx,0x00,0x00);
	//lcm_dcs_write_seq_static(ctx,0xE2,0x0D,0x11,0x1B,0x26,0x2E,0x36,0x46,0x52,0x53,0x60,0x63,0x79,0x88,0x73,0x71,0x65);
	//lcm_dcs_write_seq_static(ctx,0x00,0x10);
	//lcm_dcs_write_seq_static(ctx,0xE2,0x5C,0x4F,0x3F,0x35,0x2C,0x1D,0x10,0x0F);
	lcm_dcs_write_seq_static(ctx,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xE1,0x0D,0x13,0x24,0x34,0x3D,0x47,0x58,0x65,0x66,0x72,0x74,0x88,0x7B,0x68,0x68,0x5D);
	lcm_dcs_write_seq_static(ctx,0x00,0x10);
	lcm_dcs_write_seq_static(ctx,0xE1,0x55,0x49,0x3A,0x31,0x28,0x1A,0x10,0x0F);

	lcm_dcs_write_seq_static(ctx,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xE2,0x0D,0x13,0x24,0x34,0x3D,0x47,0x58,0x65,0x66,0x72,0x74,0x88,0x7B,0x68,0x68,0x5D);
	lcm_dcs_write_seq_static(ctx,0x00,0x10);
	lcm_dcs_write_seq_static(ctx,0xE2,0x55,0x49,0x3A,0x31,0x28,0x1A,0x10,0x0F);



	//--------------TCON setting--------------//
	//TCON
	lcm_dcs_write_seq_static(ctx,0x00, 0x80);
	lcm_dcs_write_seq_static(ctx,0xC0, 0x00 ,0xD2 ,0x00 ,0x2E ,0x00 ,0x1C);

	lcm_dcs_write_seq_static(ctx,0x00, 0x90);
	lcm_dcs_write_seq_static(ctx,0xC0, 0x00 ,0x7F ,0x00 ,0x2E ,0x00 ,0x1C);

	lcm_dcs_write_seq_static(ctx,0x00, 0xA0);
	lcm_dcs_write_seq_static(ctx,0xC0, 0x00 ,0xD2 ,0x00 ,0x2E ,0x00 ,0x1C);

	lcm_dcs_write_seq_static(ctx,0x00, 0xB0);
	lcm_dcs_write_seq_static(ctx,0xC0, 0x01 ,0x0F ,0x00 ,0x2E ,0x1C);

	lcm_dcs_write_seq_static(ctx,0x00, 0xC1);
	lcm_dcs_write_seq_static(ctx,0xC0, 0x01 ,0x33 ,0x01 ,0x0A ,0x00 ,0xCD ,0x01 ,0x8F);

	lcm_dcs_write_seq_static(ctx,0x00, 0x70);
	lcm_dcs_write_seq_static(ctx,0xC0, 0x00 ,0x7F ,0x00 ,0x2E ,0x00 ,0x1C);

	lcm_dcs_write_seq_static(ctx,0x00, 0xA3);
	lcm_dcs_write_seq_static(ctx,0xC1, 0x00, 0x33, 0x00 ,0x3C ,0x00 ,0x02);

	lcm_dcs_write_seq_static(ctx,0x00, 0xB7);
	lcm_dcs_write_seq_static(ctx,0xC1, 0x00 ,0x33);

	lcm_dcs_write_seq_static(ctx,0x00, 0x73);
	lcm_dcs_write_seq_static(ctx,0xCE, 0x09 ,0x09);

	lcm_dcs_write_seq_static(ctx,0x00, 0x80);
	lcm_dcs_write_seq_static(ctx,0xCE, 0x01, 0x81 ,0x09 ,0x09 ,0x00 ,0x78 ,0x00 ,0x96 ,0x00 ,0x78 ,0x00 ,0x96 ,0x00 ,0x78 ,0x00,0x96);

	lcm_dcs_write_seq_static(ctx,0x00, 0x90);
	lcm_dcs_write_seq_static(ctx,0xCE, 0x00 ,0xA5 ,0x16 ,0x8F ,0x00 ,0xA5 ,0x80 ,0x09 ,0x09 ,0x00 ,0x07 ,0xD0 ,0x16 ,0x16 ,0x17);

	lcm_dcs_write_seq_static(ctx,0x00, 0xA0);
	lcm_dcs_write_seq_static(ctx,0xCE, 0x20 ,0x00 ,0x00);

	lcm_dcs_write_seq_static(ctx,0x00, 0xB0);
	lcm_dcs_write_seq_static(ctx,0xCE, 0x87 ,0x00 ,0x00);

	lcm_dcs_write_seq_static(ctx,0x00, 0xD1);
	lcm_dcs_write_seq_static(ctx,0xCE, 0x00 ,0x00 ,0x01 ,0x00 ,0x00 ,0x00 ,0x00);

	lcm_dcs_write_seq_static(ctx,0x00, 0xE1);
	lcm_dcs_write_seq_static(ctx,0xCE, 0x08 ,0x03 ,0xC3 ,0x03 ,0xC3 ,0x02 ,0xB0 ,0x00 ,0x00 ,0x00 ,0x00);

	lcm_dcs_write_seq_static(ctx,0x00, 0xF1);
	lcm_dcs_write_seq_static(ctx,0xCE, 0x14 ,0x14 ,0x1E ,0x01 ,0x52 ,0x01 ,0x52 ,0x01 ,0x53);

	lcm_dcs_write_seq_static(ctx,0x00, 0xB0);
	lcm_dcs_write_seq_static(ctx,0xCF, 0x00 ,0x00 ,0x6D ,0x71);

	lcm_dcs_write_seq_static(ctx,0x00, 0xB5);
	lcm_dcs_write_seq_static(ctx,0xCF, 0x03 ,0x03 ,0x5B ,0x5F);

	lcm_dcs_write_seq_static(ctx,0x00, 0xC0);
	lcm_dcs_write_seq_static(ctx,0xCF, 0x06 ,0x06 ,0x47 ,0x4B);

	lcm_dcs_write_seq_static(ctx,0x00, 0xC5);
	lcm_dcs_write_seq_static(ctx,0xCF, 0x06 ,0x06 ,0x4B ,0x4F);

	lcm_dcs_write_seq_static(ctx,0x00, 0x60);
	lcm_dcs_write_seq_static(ctx,0xCF, 0x00 ,0x00 ,0x6D ,0x71 ,0x03 ,0x03 ,0x5B ,0x5F);

	lcm_dcs_write_seq_static(ctx,0x00, 0x70);
	lcm_dcs_write_seq_static(ctx,0xCF, 0x00 ,0x00 ,0x65 ,0x69 ,0x03 ,0x03 ,0x53 ,0x57);

	lcm_dcs_write_seq_static(ctx,0x00, 0xAA);
	lcm_dcs_write_seq_static(ctx,0xCF, 0x80 ,0x80 ,0x1C ,0x18);

	//Qsync Detect
	lcm_dcs_write_seq_static(ctx,0x00, 0xD1);
	lcm_dcs_write_seq_static(ctx,0xC1, 0x03 ,0xAA ,0x05 ,0x22 ,0x09 ,0x59 ,0x05 ,0x87 ,0x08 ,0x23 ,0x0F ,0xAC);

	lcm_dcs_write_seq_static(ctx,0x00, 0xE1);
	lcm_dcs_write_seq_static(ctx,0xC1, 0x05 ,0x22);

	lcm_dcs_write_seq_static(ctx,0x00, 0xE2);
	lcm_dcs_write_seq_static(ctx,0xCF, 0x06 ,0xDE ,0x06 ,0xDD ,0x06 ,0xDD ,0x06 ,0xDD ,0x06 ,0xDD ,0x06 ,0xDD);

	//OSC
	lcm_dcs_write_seq_static(ctx,0x00, 0x80);
	lcm_dcs_write_seq_static(ctx,0xC1, 0x00 ,0x00);

	lcm_dcs_write_seq_static(ctx,0x00, 0x90);
	lcm_dcs_write_seq_static(ctx,0xC1, 0x01);

	//Line rate for TP
	lcm_dcs_write_seq_static(ctx,0x00, 0xF5);
	lcm_dcs_write_seq_static(ctx,0xCF, 0x01);

	//TP Frame rate
	lcm_dcs_write_seq_static(ctx,0x00, 0xF6);
	lcm_dcs_write_seq_static(ctx,0xCF, 0x5A);

	//TCON Frame rate
	lcm_dcs_write_seq_static(ctx,0x00, 0xF1);
	lcm_dcs_write_seq_static(ctx,0xCF, 0x5A);

	//Mode switch by CMD2
	lcm_dcs_write_seq_static(ctx,0x00, 0xF7);
	lcm_dcs_write_seq_static(ctx,0xCF, 0x71);

	//switch 90Hz
	lcm_dcs_write_seq_static(ctx,0x00, 0x00);
	lcm_dcs_write_seq_static(ctx,0x1F, 0x5A,0x5A);	//90Hz


	//VB Mode 1.2ms
	lcm_dcs_write_seq_static(ctx,0x00, 0xD1);
	lcm_dcs_write_seq_static(ctx,0xCE, 0x00 ,0x0A ,0x01 ,0x01 ,0x00 ,0xF6 ,0x01);

	lcm_dcs_write_seq_static(ctx,0x00, 0xE8);
	lcm_dcs_write_seq_static(ctx,0xCE, 0x00 ,0xF6 ,0x00 ,0xF6);

	//--------------CGOUT setting--------------//
	//U2D mapping
	//CGOUTL
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xCC,0x00,0x1C,0x26,0x00,0x00,0x1D,0x01,0x00,0x00,0x07,0x09,0x0B,0x0D,0x0F,0x11,0x03);

	lcm_dcs_write_seq_static(ctx,0x00,0x90);
	lcm_dcs_write_seq_static(ctx,0xCC,0x05,0x23,0x00,0x00,0x00,0x00,0x00,0x00);

	//CGOUTR
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xCD,0x00,0x1C,0x26,0x00,0x00,0x1D,0x01,0x00,0x00,0x06,0x08,0x0A,0x0C,0x0E,0x10,0x02);

	lcm_dcs_write_seq_static(ctx,0x00,0x90);
	lcm_dcs_write_seq_static(ctx,0xCD,0x04,0x22,0x00,0x00,0x00,0x00,0x00,0x00);

	//D2U mapping
	//CGOUTL
	lcm_dcs_write_seq_static(ctx,0x00,0xA0);
	lcm_dcs_write_seq_static(ctx,0xCC,0x00,0x26,0x1C,0x00,0x00,0x1D,0x01,0x00,0x00,0x08,0x06,0x10,0x0E,0x0C,0x0A,0x04);

	lcm_dcs_write_seq_static(ctx,0x00,0xB0);
	lcm_dcs_write_seq_static(ctx,0xCC,0x02,0x22,0x00,0x00,0x00,0x00,0x00,0x00);

	//CGOUTR
	lcm_dcs_write_seq_static(ctx,0x00,0xA0);
	lcm_dcs_write_seq_static(ctx,0xCD,0x00,0x26,0x1C,0x00,0x00,0x1D,0x01,0x00,0x00,0x09,0x07,0x11,0x0F,0x0D,0x0B,0x05);

	lcm_dcs_write_seq_static(ctx,0x00,0xB0);
	lcm_dcs_write_seq_static(ctx,0xCD,0x03,0x23,0x00,0x00,0x00,0x00,0x00,0x00);

	//--------------GIP setting--------------//
	//power off enmode setting
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xCB,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1,0xC1);

	lcm_dcs_write_seq_static(ctx,0x00,0xED);
	lcm_dcs_write_seq_static(ctx,0xCB,0xC1);

	//power on enmode setting
	lcm_dcs_write_seq_static(ctx,0x00,0x90);
	lcm_dcs_write_seq_static(ctx,0xCB,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);

	lcm_dcs_write_seq_static(ctx,0x00,0xEE);
	lcm_dcs_write_seq_static(ctx,0xCB,0x00);

	lcm_dcs_write_seq_static(ctx,0x00,0x90);
	lcm_dcs_write_seq_static(ctx,0xC3,0x00);

	//skip & powr on1 enmode setting
	lcm_dcs_write_seq_static(ctx,0x00,0xA0);
	lcm_dcs_write_seq_static(ctx,0xCB,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);

	//power off blank enmode setting
	lcm_dcs_write_seq_static(ctx,0x00,0xB0);
	lcm_dcs_write_seq_static(ctx,0xCB,0x55,0x55,0x55,0x55);
	//power on blank enmode setting
	lcm_dcs_write_seq_static(ctx,0x00,0xC0);
	lcm_dcs_write_seq_static(ctx,0xCB,0x55,0x55,0x55,0x55);

	//power on blank enmode setting
	lcm_dcs_write_seq_static(ctx,0x00,0xD2);
	lcm_dcs_write_seq_static(ctx,0xCB,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00);

	lcm_dcs_write_seq_static(ctx,0x00,0xE0);
	lcm_dcs_write_seq_static(ctx,0xCB,0x83,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00,0x83,0x83,0x00,0x83);

	lcm_dcs_write_seq_static(ctx,0x00,0xFA);
	lcm_dcs_write_seq_static(ctx,0xCB,0x83,0x00);

	lcm_dcs_write_seq_static(ctx,0x00,0xEF);
	lcm_dcs_write_seq_static(ctx,0xCB,0x00);

	//STV1 
	lcm_dcs_write_seq_static(ctx,0x00,0x68);
	lcm_dcs_write_seq_static(ctx,0xC2,0x90,0x05,0x00,0x00);
	//STV2
	lcm_dcs_write_seq_static(ctx,0x00,0x6C);
	lcm_dcs_write_seq_static(ctx,0xC2,0x8F,0x05,0x00,0x00);
	//STV3
	lcm_dcs_write_seq_static(ctx,0x00,0x70);
	lcm_dcs_write_seq_static(ctx,0xC2,0x8E,0x05,0x00,0x00);
	//STV4
	lcm_dcs_write_seq_static(ctx,0x00,0x74);
	lcm_dcs_write_seq_static(ctx,0xC2,0x8D,0x05,0x00,0x00);

	//RST1
	lcm_dcs_write_seq_static(ctx,0x00,0xEA);
	lcm_dcs_write_seq_static(ctx,0xC2,0x12,0x00,0x11,0x08,0x00,0x00,0x00,0x00);

	//CKV1
	lcm_dcs_write_seq_static(ctx,0x00,0x8C);
	lcm_dcs_write_seq_static(ctx,0xC2,0x8A,0x04,0x15,0x9A,0x9F);
	//CKV2
	lcm_dcs_write_seq_static(ctx,0x00,0x91);
	lcm_dcs_write_seq_static(ctx,0xC2,0x89,0x05,0x15,0x9A,0x9F);
	//CKV3
	lcm_dcs_write_seq_static(ctx,0x00,0x96);
	lcm_dcs_write_seq_static(ctx,0xC2,0x88,0x06,0x15,0x9A,0x9F);
	//CKV4
	lcm_dcs_write_seq_static(ctx,0x00,0x9B);
	lcm_dcs_write_seq_static(ctx,0xC2,0x87,0x07,0x15,0x9A,0x9F);

	//CKV5
	lcm_dcs_write_seq_static(ctx,0x00,0xA0);
	lcm_dcs_write_seq_static(ctx,0xC2,0x86,0x00,0x15,0x9A,0x9F);
	//CKV6
	lcm_dcs_write_seq_static(ctx,0x00,0xA5);
	lcm_dcs_write_seq_static(ctx,0xC2,0x85,0x00,0x15,0x9A,0x9F);
	//CKV7
	lcm_dcs_write_seq_static(ctx,0x00,0xAA);
	lcm_dcs_write_seq_static(ctx,0xC2,0x84,0x00,0x15,0x9A,0x9F);
	//CKV8
	lcm_dcs_write_seq_static(ctx,0x00,0xAF);
	lcm_dcs_write_seq_static(ctx,0xC2,0x83,0x00,0x15,0x9A,0x9F);

	//CKV9
	lcm_dcs_write_seq_static(ctx,0x00,0xB4);
	lcm_dcs_write_seq_static(ctx,0xC2,0x82,0x01,0x15,0x9A,0x9F);
	//CKV10
	lcm_dcs_write_seq_static(ctx,0x00,0xB9);
	lcm_dcs_write_seq_static(ctx,0xC2,0x81,0x02,0x15,0x9A,0x9F);
	//CKV11
	lcm_dcs_write_seq_static(ctx,0x00,0xBE);
	lcm_dcs_write_seq_static(ctx,0xC2,0x80,0x03,0x15,0x9A,0x9F);
	//CKV12
	lcm_dcs_write_seq_static(ctx,0x00,0xC3);
	lcm_dcs_write_seq_static(ctx,0xC2,0x01,0x04,0x15,0x9A,0x9F);

	//CKV width setting
	lcm_dcs_write_seq_static(ctx,0x00,0xdc);
	lcm_dcs_write_seq_static(ctx,0xC2,0xbb,0xbb,0xbb,0xbb,0xbb,0xbb,0x00,0x00);

	//GOFF3 
	lcm_dcs_write_seq_static(ctx,0x00,0xE8);
	lcm_dcs_write_seq_static(ctx,0xC3,0x00,0x00,0x00,0x00);

	//GOFF4
	lcm_dcs_write_seq_static(ctx,0x00,0xEC);
	lcm_dcs_write_seq_static(ctx,0xC3,0x00,0x00,0x00,0x00);

	lcm_dcs_write_seq_static(ctx,0x00,0xF9); //GOFF3 add VST4
	lcm_dcs_write_seq_static(ctx,0xCB,0x20);

	lcm_dcs_write_seq_static(ctx,0x00,0xFE); //GOFF4 add VST4
	lcm_dcs_write_seq_static(ctx,0xCB,0x08);

	lcm_dcs_write_seq_static(ctx,0x00,0xF8); //GOFF3/GOFF4 add VST4 shift & width
	lcm_dcs_write_seq_static(ctx,0xCD,0x8C,0x55,0x8B);

	lcm_dcs_write_seq_static(ctx,0x00,0xFC); //GCH/GCL
	lcm_dcs_write_seq_static(ctx,0xCB,0x08,0x40);

	lcm_dcs_write_seq_static(ctx,0x00,0xFB); //GCH/GCL start&end point
	lcm_dcs_write_seq_static(ctx,0xC3,0x93,0x08,0x93,0x08);

	//--------------Source setting--------------//
	lcm_dcs_write_seq_static(ctx,0x00,0x98);
	lcm_dcs_write_seq_static(ctx,0xC4,0x08);

	lcm_dcs_write_seq_static(ctx,0x00,0x91);
	lcm_dcs_write_seq_static(ctx,0xE9,0xFF,0xFF,0xFF,0x00);

	lcm_dcs_write_seq_static(ctx,0x00,0x85);//Posdmy pch
	lcm_dcs_write_seq_static(ctx,0xC4,0x80);

	lcm_dcs_write_seq_static(ctx,0x00,0x86);//4VB pch
	lcm_dcs_write_seq_static(ctx,0xA4,0xB6);

	lcm_dcs_write_seq_static(ctx,0x00,0x95);//VB pch data
	lcm_dcs_write_seq_static(ctx,0xC4,0x80);

	lcm_dcs_write_seq_static(ctx,0x00,0x81);//4VB pch
	lcm_dcs_write_seq_static(ctx,0xA4,0x73);

	//--------------Power on&off--------------//
	lcm_dcs_write_seq_static(ctx,0x00,0xCA);//Power on 3 
	lcm_dcs_write_seq_static(ctx,0xC0,0x90,0x11);

	lcm_dcs_write_seq_static(ctx,0x00,0xB7);//sd_en_sdpl_sel
	lcm_dcs_write_seq_static(ctx,0xF5,0x1D);

	lcm_dcs_write_seq_static(ctx,0x00,0x90);
	lcm_dcs_write_seq_static(ctx,0xC3,0x00);

	lcm_dcs_write_seq_static(ctx,0x00,0xB1);//VCOM
	lcm_dcs_write_seq_static(ctx,0xF5,0x11);

	//--------------sleep in allgateon--------------//
	lcm_dcs_write_seq_static(ctx,0x00,0xB0); 
	lcm_dcs_write_seq_static(ctx,0xC5,0x00);

	lcm_dcs_write_seq_static(ctx,0x00,0xB3);
	lcm_dcs_write_seq_static(ctx,0xC5,0x00);

	lcm_dcs_write_seq_static(ctx,0x00,0xB2);
	lcm_dcs_write_seq_static(ctx,0xC5,0x0D);

	lcm_dcs_write_seq_static(ctx,0x00,0xB5);
	lcm_dcs_write_seq_static(ctx,0xC5,0x02);

	lcm_dcs_write_seq_static(ctx,0x00,0xC2);
	lcm_dcs_write_seq_static(ctx,0xF5,0x42);
	//*********************************************//
	//*********************************************//
	//LongV mode
	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xCE,0x00);

	lcm_dcs_write_seq_static(ctx,0x00,0xD0);
	lcm_dcs_write_seq_static(ctx,0xCE,0x01);

	lcm_dcs_write_seq_static(ctx,0x00,0xE0);
	lcm_dcs_write_seq_static(ctx,0xCE,0x00);

	lcm_dcs_write_seq_static(ctx,0x00,0xA1);
	lcm_dcs_write_seq_static(ctx,0xC1,0xCC);

	lcm_dcs_write_seq_static(ctx,0x00,0xA6);
	lcm_dcs_write_seq_static(ctx,0xC1,0x10);

	lcm_dcs_write_seq_static(ctx,0x00,0x71);//VFP=217-1
	lcm_dcs_write_seq_static(ctx,0xC0,0x85,0x01,0x2B,0x00,0x1C);

	//*********************************************//
	//*********************************************//
	//--------------CMD WR Disable--------------//
	lcm_dcs_write_seq_static(ctx,0x00,0x00);
	lcm_dcs_write_seq_static(ctx,0xFF,0x00,0x00,0x00); 

	lcm_dcs_write_seq_static(ctx,0x00,0x80);
	lcm_dcs_write_seq_static(ctx,0xFF,0x00,0x00);

	//----------------------LCD initial code End----------------------//
	
	lcm_dcs_write_seq_static(ctx,0x11,0x00);
	mdelay(120);
	lcm_dcs_write_seq_static(ctx,0x29,0x00);
	mdelay(10);
	lcm_dcs_write_seq_static(ctx,0x35,0x00);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	mdelay(10);
	lcm_dcs_write_seq_static(ctx, 0x10);
	mdelay(120);

	ctx->error = 0;
	ctx->prepared = false;
#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_disable();
	pr_err("ft8057s----------%s-----%d\n",__func__,__LINE__);
#else
	
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	
	pr_err("ft8057s----------%s-----%d\n",__func__,__LINE__);
	
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
	
	pr_err("ft8057s----------%s-----%d\n",__func__,__LINE__);

	ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
		"bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	udelay(1000);
	ctx->pm_enable_gpio = devm_gpiod_get(ctx->dev,
		"pm-enable", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->pm_enable_gpio)) {
		dev_warn(ctx->dev, "%s: cannot get pm-enable %ld\n",
			__func__, PTR_ERR(ctx->pm_enable_gpio));
		return PTR_ERR(ctx->pm_enable_gpio);
	}
	gpiod_set_value(ctx->pm_enable_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->pm_enable_gpio);
	
	pr_err("ft8057s------exit----%s-----%d\n",__func__,__LINE__);
#endif

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("ft8057s.%s\n", __func__);
//	return 0;
//	pr_err("gezi----------%s-----%d\n",__func__,__LINE__);
	
	if (ctx->prepared)
		return 0;

#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_enable();
//	pr_err("gezi----------%s-----%d\n",__func__,__LINE__);
#else
	ctx->pm_enable_gpio = devm_gpiod_get(ctx->dev,
		"pm-enable", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->pm_enable_gpio)) {
		dev_warn(ctx->dev, "%s: cannot get pm-enable %ld\n",
			__func__, PTR_ERR(ctx->pm_enable_gpio));
		return PTR_ERR(ctx->pm_enable_gpio);
	}
	gpiod_set_value(ctx->pm_enable_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->pm_enable_gpio);
	
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
	pr_err("ft8057s----------%s-----%d\n",__func__,__LINE__);
#endif

	lcm_panel_init(ctx);

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
	pr_info("ft8057s.%s done\n", __func__);

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

#define VAC (1612)
#define HAC (720)
#define HFP (148)
#define HSA (16)
#define HBP (48)
#define VFP (1276)
#define VSA (6)
#define VBP (28)
static u32 fake_heigh = 1612;
static u32 fake_width = 720;
static bool need_fake_resolution;

static struct drm_display_mode default_mode = {
	.clock = 163400,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end = VAC + VFP + VSA,
	.vtotal = VAC + VFP + VSA + VBP,
	.vrefresh = 60,
};

static struct drm_display_mode performance_mode = {
	.clock = 163230,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + 300,				// 150
	.vsync_end = VAC + 300 + VSA,
	.vtotal = VAC + 300 + VSA + VBP,
	.vrefresh = 90,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = 536,
//	.vfp_low_power = 130,//45hz
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.data_rate = 1072,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size = sizeof(top_rc_pattern),
	.corner_pattern_lt_addr = (void *)top_rc_pattern,
#endif	
	.wait_sof_before_dec_vfp = 1,
};

static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = 536,
//	.vfp_low_power = 1294,//60hz
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {

		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.data_rate = 1072,
};

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0x00, 0x00, 0x00};
	unsigned char id[3] = {0x55, 0x00, 0x00};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0xda, data, 1);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	}

	printk("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0])
		return 1;

	printk("ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);
	return 0;
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
static int mtk_panel_ext_param_set(struct drm_panel *panel, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(panel, mode);

	if (m->vrefresh == 60)
		ext->params = &ext_params;
	else if (m->vrefresh == 90)
		ext->params = &ext_params_90hz;
	else
		ret = 1;

	return ret;
}
static int lcm_get_virtual_heigh(void)
{
	return VAC;
}

static int lcm_get_virtual_width(void)
{
	return HAC;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	.ext_param_set = mtk_panel_ext_param_set,
	.get_virtual_heigh = lcm_get_virtual_heigh,
	.get_virtual_width = lcm_get_virtual_width,
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
	struct drm_display_mode *mode;
	struct drm_display_mode *mode2;

	if (need_fake_resolution) {
		change_drm_disp_mode_params(&default_mode);
		change_drm_disp_mode_params(&performance_mode);
	}
	mode = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode) {
		dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode);

	mode2 = drm_mode_duplicate(panel->drm, &performance_mode);
	if (!mode2) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode.hdisplay,
			performance_mode.vdisplay,
			performance_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(panel->connector, mode2);

	panel->connector->display_info.width_mm = 68;
	panel->connector->display_info.height_mm = 152; /* drv modify by shenwenbin for LAE15VGE-172 use product definition height 20230727 */

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
		need_fake_resolution = true;
	if (fake_width > 0 && fake_width < HAC)
		need_fake_resolution = true;
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

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE
			 | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS;

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
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif
	check_is_need_fake_resolution(dev);
	pr_info("ft8057s %s-\n", __func__);
//prize add by anhengxuan for lcd hardware info 20220102 start
#if defined(CONFIG_PRIZE_HARDWARE_INFO)
    strcpy(current_lcm_info.chip,"ft8057s");
    strcpy(current_lcm_info.vendor,"focaltech");
    sprintf(current_lcm_info.id,"0x%02x",0x8057);
    strcpy(current_lcm_info.more,"1612*720");
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
	{ .compatible = "focatech,ft8057s,vdo", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-focatech-ft8057s-hdp-vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Tai-Hua Tseng <tai-hua.tseng@mediatek.com>");
MODULE_DESCRIPTION("chipone icnl9911c VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");
