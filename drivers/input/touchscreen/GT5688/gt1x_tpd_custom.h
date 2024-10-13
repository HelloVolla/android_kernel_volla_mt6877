/* drivers/input/touchscreen/gt1x_tpd_custom.h
 *
 * 2010 - 2016 Goodix Technology.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * Version: 1.6   
 * Release Date:  2016/07/28
 */

#ifndef GT1X_TPD_CUSTOM_H__
#define GT1X_TPD_CUSTOM_H__

#include <asm/uaccess.h>
#include <linux/hrtimer.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/byteorder/generic.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/interrupt.h>
#include <linux/time.h>

#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#ifdef CONFIG_MTK_BOOT
#include "mtk_boot_common.h"
#endif
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/proc_fs.h>	/*proc*/
#include <linux/atomic.h>

#ifndef MT6589
//#include <mt_gpio.h>
#endif
//#include <cust_eint.h>
#include "tpd.h"
#include "upmu_common.h"//hwPowerOn/hwPowerDown
//#include "cust_gpio_usage.h"
//#include <pmic_drv.h>

#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0))
#define GTP_MTK_LEGACY
#endif

#define PLATFORM_MTK
#define TPD_I2C_NUMBER		        1

#ifdef CONFIG_MTK_I2C_EXTENSION
#define TPD_SUPPORT_I2C_DMA         1	/* if gt9l, better enable it if hardware platform supported*/
#else
#define TPD_SUPPORT_I2C_DMA         0
#endif

#if defined(CONFIG_MTK_LEGACY)
#define TPD_POWER_SOURCE_CUSTOM	MT6328_POWER_LDO_VGP1
#endif

#define GTP_GPIO_AS_INT(pin) tpd_gpio_as_int(pin)
#define GTP_GPIO_OUTPUT(pin, level) tpd_gpio_output(pin, level)

#ifdef MT6589
extern void mt65xx_eint_unmask(unsigned int line);
extern void mt65xx_eint_mask(unsigned int line);
#define mt_eint_mask mt65xx_eint_mask
#define mt_eint_unmask mt65xx_eint_unmask
#endif

#define IIC_MAX_TRANSFER_SIZE         8
#define IIC_DMA_MAX_TRANSFER_SIZE     250
#define I2C_MASTER_CLOCK              300

#define TPD_MAX_RESET_COUNT           3
#define CONFIG_TPD_HAVE_BUTTON 1

#define CONFIG_GTP_REQUEST_FW_UPDATE 0
#define TPD_KEY_COUNT   2
#define key_1           60,2000
#define key_2           300,2000
//#define key_3           300,2000
#define TPD_KEY_MAP_ARRAY {{key_1},{key_2}}
#define TPD_KEYS        {KEY_MENU, KEY_BACK}
#define TPD_KEYS_DIM    {{key_1,50,20},{key_2,50,20}}
#define TPD_HAVE_CALIBRATION
#define TPD_CALIBRATION_MATRIX        {962,0,0,0,1600,0,0,0};

extern void tpd_on(void);
extern void tpd_off(void);

#endif /* GT1X_TPD_CUSTOM_H__ */
