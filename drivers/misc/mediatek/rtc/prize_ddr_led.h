#ifndef _PRIZE_DDR_LED_H_
#define _PRIZE_DDR_LED_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/ctype.h>

#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
//#include <linux/wakelock.h>
#include <linux/input.h>
#include <linux/time.h>

#include <linux/string.h>
#include <asm/atomic.h>
#include <linux/pinctrl/consumer.h>
extern int prize_ddr_led_pass(void);
extern int prize_ddr_led_fail(void);
extern int prize_ddr_led_high(void);
extern int prize_ddr_led_low(void);

#endif