/*
 * Sitronix Touchscreen Controller Driver
 *
 * Copyright (C) 2018 Sitronix Technology Co., Ltd.
 *	CT Chen <ct_chen@sitronix.com.tw>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#ifndef __SITRONIX_TS_H__
#define __SITRONIX_TS_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
//#include <linux/input/synaptics_dsx.h>
#include <linux/kthread.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/firmware.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/random.h>

#if defined(CONFIG_FB)
#ifdef CONFIG_DRM_MSM
#include <linux/msm_drm_notify.h>
#endif
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

#define SITRONIX_TP_DRIVER_VERSION	"Sitronix ST7123 Touch Driver v43.00.220704"

#define SITRONIX_INTERFACE_I2C
//#define SITRONIX_INTERFACE_SPI
#define SITRONIX_TP_WITH_FLASH

#define SITRONIX_MONITOR_THREAD
#define SITRONIX_TS_MT_SLOT
#define SITRONIX_DO_TP_HW_RESET
#define SITRONIX_PROC_DIR_CREATE
//#define SITRONIX_SUPPORT_PROXIMITY
#define SITRONIX_SUPPORT_SWU
#define SITRONIX_PROXIMITY_DEMO
#define SITRONIX_TP_RESUME_BEFORE_DISPON
//#define SITRONIX_VALIDATE_IC_SFRVER

//#define SITRONIX_SFRVER_44
#define SITRONIX_SFRVER_43
//#define SITRONIX_SFRVER_33

#ifdef SITRONIX_INTERFACE_I2C
//#define SITRONIX_I2C_ADDRESS_DETECT
#define ST_PLATFORM_WRITE_LEN_MAX	0x1000
#define ST_PLATFORM_READ_LEN_MAX	0x100
#define ST_ICP_READ_BLOCK_SIZE		ST_PLATFORM_READ_LEN_MAX
#endif

#ifdef SITRONIX_INTERFACE_SPI
#define ST_PLATFORM_WRITE_LEN_MAX	0x1000
#define ST_PLATFORM_READ_LEN_MAX	0x1000
#define ST_ICP_READ_BLOCK_SIZE		ST_PLATFORM_READ_LEN_MAX
#endif /* SITRONIX_INTERFACE_SPI */

//#define SITRONIX_UPGRADE_TEST

#ifndef SITRONIX_TP_WITH_FLASH
#define SITRONIX_HDL_IN_PROBE
#define SITRONIX_HDL_IN_RESUME
#define SITRONIX_HDL_IN_MT
#define SITRONIX_HDL_IN_IRQ
#define SITRONIX_HDL_VALIDATE_BY_RANDOM
#endif /* SITRONIX_TP_WITH_FLASH */

//#define ST_UPGRADE_USE_REQUESTFW_BUF
#define ST_REQUESTFW_DF_PATH	"7123.dump"


#ifdef SITRONIX_PROC_DIR_CREATE
#define SITRONIX_PROC_DIR_NAME			"sitronix"
#endif /* SITRONIX_PROC_DIR_CREATE */


#define SITRONIX_TS_I2C_DRIVER_NAME		"sitronix_ts_i2c"

struct sitronix_ts_i2c_data {
	struct i2c_client	*client;
};


#define SITRONIX_TS_SPI_DRIVER_NAME		"sitronix_ts_spi"

struct sitronix_ts_spi_data {
	struct spi_device	*spi_dev;
	uint32_t		spi_max_freq;
	uint32_t		byte_delay_us;
	uint8_t			spi_mode;
};



#ifdef SITRONIX_TS_MT_SLOT
#include <linux/input/mt.h>
#endif /* SITRONIX_TS_MT_SLOT */



#define sterr(format, ...)	\
	printk("[ST ERR] " format, ## __VA_ARGS__)

#define stmsg(format, ...)	\
	printk("[ST MSG] " format, ## __VA_ARGS__)

#define stdbg(format, ...)	\
	/* printk("[ST DBG] " format, ## __VA_ARGS__) */

#define ST_START_PRINT stmsg("start of %s\n", __func__);


#define SITRONIX_RW_BUF_LEN	0x1000

extern struct sitronix_ts_data *gts;
extern unsigned char wbuf[SITRONIX_RW_BUF_LEN+8];
extern unsigned char rbuf[SITRONIX_RW_BUF_LEN+8];

#define DELAY_MONITOR_THREAD_START_PROBE 10000
#define DELAY_MONITOR_THREAD_START_RESUME 3000
#define DELAY_MONITOR_THREAD_PEROID_NORMAL 2000
#define DELAY_MONITOR_THREAD_PEROID_ERROR  500
//#define MONITOR_THREAD_STOP_IN_SUSPEND
//#define SITRONIX_MT_CHECK_DIS
#define SITRONIX_MT_DIS_LIMIT	10000


#define SITRONIX_PROXIMITY_REPORT_ABS	ABS_DISTANCE
#define SITRONIX_PROXIMITY_REPORT_VALUE_MIN		0
#define SITRONIX_PROXIMITY_REPORT_VALUE_MAX		0xFF

#define SITRONIX_RESUME_DELAY	200 //MTK 40, Inforce 200

typedef enum {
	ST_PROXIMITY_FACEIN_LEVEL_1_ENABLE		= 0,
	ST_PROXIMITY_FACEIN_LEVEL_1_VALUE		= 1,
	ST_PROXIMITY_FACEIN_LEVEL_1_REPORT		= 1,
	ST_PROXIMITY_FACEIN_LEVEL_2_ENABLE		= 0,
	ST_PROXIMITY_FACEIN_LEVEL_2_VALUE		= 2,
	ST_PROXIMITY_FACEIN_LEVEL_2_REPORT		= 2,
	ST_PROXIMITY_FACEIN_LEVEL_3_ENABLE		= 1,
	ST_PROXIMITY_FACEIN_LEVEL_3_VALUE		= 3,
	ST_PROXIMITY_FACEIN_LEVEL_3_REPORT		= 3,
	ST_PROXIMITY_FACEOUT_LEVEL_1_ENABLE		= 1,
	ST_PROXIMITY_FACEOUT_LEVEL_1_VALUE		= 4,
	ST_PROXIMITY_FACEOUT_LEVEL_1_REPORT		= 4,
	ST_PROXIMITY_FACEOUT_LEVEL_2_ENABLE		= 1,
	ST_PROXIMITY_FACEOUT_LEVEL_2_VALUE		= 5,
	ST_PROXIMITY_FACEOUT_LEVEL_2_REPORT		= 5,
	ST_PROXIMITY_FACEOUT_LEVEL_3_ENABLE		= 0,
	ST_PROXIMITY_FACEOUT_LEVEL_3_VALUE		= 0,
	ST_PROXIMITY_FACEOUT_LEVEL_3_REPORT		= 0,
} PROXIMITY_SETTING;

typedef enum {
	FIRMWARE_VERSION = 0,
	STATUS_REG,
	DEVICE_CONTROL_REG,
	TIMEOUT_TO_IDLE_REG,
	X_RESOLUTION_HIGH = 0x05,
	X_RESOLUTION_LOW = 0x06,
	Y_RESOLUTION_HIGH = 0x07,
	Y_RESOLUTION_LOW = 0x08,
	MAX_NUM_TOUCHES = 0x09,
	SENSING_COUNTER_H = 0x0A,
	SENSING_COUNTER_L = 0x0B,
	DEVICE_CONTROL_REG2 = 0x09,
	FIRMWARE_REVISION_3 = 0x0C,
	FIRMWARE_REVISION_2,
	FIRMWARE_REVISION_1,
	FIRMWARE_REVISION_0,
	TOUCH_INFO = 0x10,
	GESTURES = 0x12,
	MISC_INFO = 0xF0,
	MISC_CONTROL = 0xF1,
	SMART_WAKE_UP_REG = 0xF2,
	CHIP_ID = 0xF4,
	XY_CHS = 0xF5,
	CMDIO_CONTROL = 0xF8,
	PAGE_REG = 0xFF,	
	CMDIO_PORT = 0x110,
	DATA_OUTPUT_BUFFER = 0x140,
} TS_Register;


typedef enum {
	ST_KEY_GESTURE_POWER		= KEY_POWER,
	ST_KEY_GESTURE_LEFT		= KEY_LEFT,
	ST_KEY_GESTURE_RIGHT		= KEY_RIGHT,
	ST_KEY_GESTURE_UP		= KEY_UP,
	ST_KEY_GESTURE_DOWN		= KEY_DOWN,
	ST_KEY_GESTURE_TWO_FINGER_DOWN	= KEY_DOWN,
	ST_KEY_GESTURE_U		= KEY_U,
	ST_KEY_GESTURE_O		= KEY_O,
	ST_KEY_GESTURE_E		= KEY_E,
	ST_KEY_GESTURE_M		= KEY_M,
	ST_KEY_GESTURE_W		= KEY_W,
	ST_KEY_GESTURE_L		= KEY_L,
	ST_KEY_GESTURE_S		= KEY_S,
	ST_KEY_GESTURE_V		= KEY_V,
	ST_KEY_GESTURE_Z		= KEY_Z,
	ST_KEY_GESTURE_C		= KEY_C
} SWU_KEYCODE;

#define STDEV_IOC_MAGIC   0xF1

typedef enum {
	STDEV_WR_DF		=	2,
	STDEV_GET_DRIVER_INFO	=	16,
	STDEV_ADDR_MODE_ON		=	9,
	STDEV_ADDR_MODE_OFF		=	10,
	STDEV_WRITE_DUMP		=	11,
	STDEV_READ_DUMP			=	12,
	STDEV_EXECUTE_UPGRADE	=	13,
	STDEV_RETURN_MODE		=	15,
	STDEV_ENABLE_IRQ		=	17,
	STDEV_DISABLE_IRQ		=	18,
	STDEV_HW_RESET			=	19,
	STDEV_IRQ_WAITMODE_DO	=	14,	
	STDEV_IRQ_WAITMODE_RAW	=	20,
	STDEV_IRQ_WAITMODE_COORD=	21,
	STDEV_IRQ_WAITMODE_OFF	=	22,
	STDEV_CONTINUE_READ		=	23,
	STDEV_PROXIMITY_STATUS	=	24,
	STDEV_PROXIMITY_ON		=	25,
	STDEV_PROXIMITY_OFF		=	26,
	STDEV_GET_COORD_BUF		=	80,	
	SMT_IOC_MAXNR,
} STDEV_FUNCTION;

#define IOCTL_STDEV_GET_DRIVER_INFO		_IOC(_IOC_NONE, STDEV_IOC_MAGIC, STDEV_GET_DRIVER_INFO, 0)
#define IOCTL_STDEV_ADDR_MODE_ON		_IOC(_IOC_NONE, STDEV_IOC_MAGIC, STDEV_ADDR_MODE_ON, 0)
#define IOCTL_STDEV_ADDR_MODE_OFF		_IOC(_IOC_NONE, STDEV_IOC_MAGIC, STDEV_ADDR_MODE_OFF, 0)
#define IOCTL_STDEV_WRITE_DUMP			_IOC(_IOC_NONE, STDEV_IOC_MAGIC, STDEV_WRITE_DUMP, 0)
#define IOCTL_STDEV_READ_DUMP			_IOC(_IOC_NONE, STDEV_IOC_MAGIC, STDEV_READ_DUMP, 0)
#define IOCTL_STDEV_EXECUTE_UPGRADE		_IOC(_IOC_NONE, STDEV_IOC_MAGIC, STDEV_EXECUTE_UPGRADE, 0)
#define IOCTL_STDEV_ENABLE_IRQ			_IOC(_IOC_NONE, STDEV_IOC_MAGIC, STDEV_ENABLE_IRQ, 0)
#define IOCTL_STDEV_DISABLE_IRQ			_IOC(_IOC_NONE, STDEV_IOC_MAGIC, STDEV_DISABLE_IRQ, 0)
#define IOCTL_STDEV_HW_RESET			_IOC(_IOC_NONE, STDEV_IOC_MAGIC, STDEV_HW_RESET, 0)

struct sitronix_ts_device_info {
	uint8_t		chip_id;
	uint8_t		fw_version;
	uint8_t		fw_revision[4];
	uint8_t		customer_info[4];
	uint8_t		max_touches;
	uint16_t	x_res;
	uint16_t	y_res;
	uint8_t		x_chs;
	uint8_t		y_chs;
	uint8_t		misc_info;
};

struct sitronix_ts_data {
	const char *name;
	struct platform_device *pdev;
	struct input_dev *input_dev;
	struct input_dev *input_dev_proximity;
	const struct sitronix_ts_host_interface *host_if;
	struct sitronix_ts_device_info ts_dev_info;
	struct kobject *sitronix_kobj;

#if defined(CONFIG_FB)
	struct workqueue_struct *workqueue;
	struct work_struct	resume_work;
#ifdef _MSM_DRM_NOTIFY_H_
	struct notifier_block	drm_notif;
#else
	struct notifier_block	fb_notif;
#endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
	int irq;
	int irq_flags;
	bool in_suspend;
	struct mutex mutex;
	uint8_t swu_gesture_id;
	bool skip_first_resume;
	/* proximity */
	bool is_support_proximity;
	bool proximity_flag;
	uint8_t proximity_status;
	bool proximity_demo_enable;
	bool proximity_is_facein;

	/* MT */
	bool			is_pause_mt;
	bool			upgrade_doing;
	int			upgrade_result;
	char			upgrade_msg[100];
	bool			is_reset_chip;
	bool			is_suspend_mt;

	bool			enable_monitor_thread;
	int sitronix_ts_delay_monitor_thread_start;
	struct task_struct *SitronixMonitorThread;
	int (*sitronix_mt_fp)(void *);

	/* CMDIO */
	int			cmdio_result;
	char			cmdio_msg[100];
	int			cmdio_rwflag;
	uint16_t		cmdio_addr[8];
	uint32_t		cmdio_len;
	int			cmdio_iotype;

	/* swu */
	bool			swu_flag;
	bool			swu_status;

	/* Mode switch */
	bool			mode_flag[20];
	uint8_t			mode_value[20];

	/* sitronix dev */
	bool			wr_mode;
	uint8_t			wr_len;
	uint8_t			stdev_mode;
	uint8_t			cr_mode;

	/* coord buf */
	uint8_t			coord_buf[80];

	/* IRQ wait mode */
	uint8_t			irq_wait_mode;
	wait_queue_head_t 	irq_wait_queue;
	uint8_t 		irq_wait_flag;
		
	/* self test */
	int self_test_result;
};

struct sitronix_ts_host_interface {
	uint8_t		bus_type;
	bool		is_use_flash;
	int		irq_gpio;
	uint32_t	irq_gpio_flags;
	int		rst_gpio;
	uint32_t	rst_gpio_flags;
	void		*if_data;
	int		(*read)(uint16_t addr, uint8_t *data, uint16_t length, void *if_data);
	int		(*write)(uint16_t addr, uint8_t *data, uint16_t length, void *if_data);
	int		(*dread)(uint8_t *data, uint16_t length, void *if_data);
	int		(*dwrite)(uint8_t *data, uint16_t length, void *if_data);
	int		(*aread)(uint8_t *tx_buf, uint16_t tx_len, uint8_t *rx_buf, uint16_t rx_len, void *if_data);
	int		(*awrite)(uint8_t *tx_buf, uint16_t tx_len, void *if_data);
	int		(*sread)(uint8_t *tx_buf, uint16_t tx_len, uint8_t *rx_buf, uint16_t rx_len, void *if_data);
	int		(*ram_mode_rw)(bool isread, uint16_t cmd, uint16_t addr, uint8_t *txbuf, uint8_t *rxbuf, int len, void *if_data);
	int		(*ram_mode_cmd)(uint16_t cmd, void *if_data);

};

extern int sitronix_ts_bus_init(void);

extern void sitronix_bus_exit(void);

static inline int sitronix_ts_reg_read(struct sitronix_ts_data *ts_data, uint16_t addr, uint8_t *data, uint16_t len)
{
	return ts_data->host_if->read(addr, data, len, ts_data->host_if->if_data);
}

static inline int sitronix_ts_reg_write(struct sitronix_ts_data *ts_data, uint16_t addr, uint8_t *data, uint16_t len)
{
	return ts_data->host_if->write(addr, data, len, ts_data->host_if->if_data);
}

static inline int sitronix_ts_reg_dread(struct sitronix_ts_data *ts_data, uint8_t *data, uint16_t len)
{
	return ts_data->host_if->dread(data, len, ts_data->host_if->if_data);
}

static inline int sitronix_ts_reg_dwrite(struct sitronix_ts_data *ts_data, uint8_t *data, uint16_t len)
{
	return ts_data->host_if->dwrite(data, len, ts_data->host_if->if_data);
}

static inline int sitronix_ts_addrmode_read(struct sitronix_ts_data *ts_data, uint8_t *tx_buf, uint16_t tx_len, uint8_t *rx_buf, uint16_t rx_len)
{
	return ts_data->host_if->aread(tx_buf, tx_len, rx_buf, rx_len, ts_data->host_if->if_data);
}

static inline int sitronix_ts_addrmode_write(struct sitronix_ts_data *ts_data, uint8_t *tx_buf, uint16_t tx_len)
{
	return ts_data->host_if->awrite(tx_buf, tx_len, ts_data->host_if->if_data);
}

static inline int sitronix_ts_addrmode_split_read(struct sitronix_ts_data *ts_data, uint8_t *tx_buf, uint16_t tx_len, uint8_t *rx_buf, uint16_t rx_len)
{
	return ts_data->host_if->sread(tx_buf, tx_len, rx_buf, rx_len, ts_data->host_if->if_data);
}

static inline int sitronix_ts_ram_mode_rw(struct sitronix_ts_data *ts_data, bool isread, uint16_t cmd, uint16_t addr, uint8_t *txbuf, uint8_t *rxbuf, int len)
{
	return ts_data->host_if->ram_mode_rw(isread, cmd, addr, txbuf, rxbuf, len, ts_data->host_if->if_data);
}

static inline int sitronix_ts_ram_mode_cmd(struct sitronix_ts_data *ts_data, uint16_t cmd)
{
	return ts_data->host_if->ram_mode_cmd(cmd, ts_data->host_if->if_data);
}

#define EnableDbgMsg
#ifdef EnableDbgMsg
#define DbgMsg(arg...) printk(arg)
#else
#define DbgMsg(arg...)
#endif

/*sitronix_ts.c*/
int sitronix_ts_reset_device(struct sitronix_ts_data *ts_data);
void sitronix_ts_report_swu(struct input_dev *input_dev, u8 swu_id);
int sitronix_ts_irq_enable(struct sitronix_ts_data *ts_data, bool enable);


/*sitronix_ts_i2c.c*/
extern int sitronix_ts_i2c_init(void);
extern void sitronix_ts_i2c_exit(void);

/*sitronix_ts_spi.c*/
extern int sitronix_ts_spi_init(void);
extern void sitronix_ts_spi_exit(void);

/*sitronix_ts_mt.c*/
int sitronix_ts_monitor_thread_v3(void *data);
void sitronix_mt_pause_one(void);
void sitronix_mt_pause(void);
void sitronix_mt_restore(void);
void sitronix_mt_stop(void);
void sitronix_mt_start(int startDelayMS);
void sitronix_ts_mt_reset_process(void);
void sitronix_mt_suspend(void);
void sitronix_mt_resume(void);


/*sitronix_ts_nodes.c*/
int sitronix_create_sysfs(struct sitronix_ts_data *ts);
void sitronix_remove_sysfs(struct sitronix_ts_data *ts);
int sitronix_create_st_dev(struct sitronix_ts_data *ts);
void sitronix_remove_st_dev(struct sitronix_ts_data *ts);
int sitronix_create_proc(void);
void sitronix_remove_proc(void);

/*sitronix_ts_utility.c*/
int sitronix_ts_get_device_status(struct sitronix_ts_data *ts_data);
int sitronix_ts_get_device_info(struct sitronix_ts_data *ts_data);
int sitronix_ts_set_smart_wake_up(struct sitronix_ts_data *ts_data, bool enable);
int sitronix_ts_get_swu_gesture(struct sitronix_ts_data *ts_data);
int sitronix_ts_get_swu_keycode(uint8_t swu_id);
int sitronix_ts_powerdown(struct sitronix_ts_data *ts_data, bool powerdown);
int sitronix_ts_proximity_enable(struct sitronix_ts_data *ts_data, bool proximity_enable);
int sitronix_ts_proximity_control_sensing(struct sitronix_ts_data *ts_data, bool sensing_enable);
int TDU_CmdioRead(int type, int address, unsigned char *buf, int len);
int TDU_CmdioWrite(int type, int address, unsigned char *buf, int len);
int TDU_FWInfoRead(int type, unsigned char *buf, int len);
int sitronix_get_display_id(unsigned char *buf);
int sitronix_get_ic_sfrver(void);
int sitronix_get_ic_position(unsigned char *buf);
int sitronix_write_driver_cmd(unsigned char cmd, unsigned char *buf, int len);


/*sitronix_ts_upgrade.c*/
int st_check_cfg(const char *data, int *cfgSize);
int st_check_fw(const char *data, int *fwOff, int *fwSize, int *fwInfoOff, int *cfgFlashOff, int *cfgDramOff);
int st_get_dump_size(void);
void st_set_dump(const char *data, int off, int size);
void st_get_dump(unsigned char *data, int off, int size);
int sitronix_do_upgrade(void);
int sitronix_mode_switch(int modeID, bool flag);
void sitronix_mode_restore(void);
int sitronix_spi_pram_rw(bool isread, u32 addr, u8 *txbuf, u8 *rxbuf, int len);
void sitronix_replace_dump_buf(unsigned char *id);

/*sitronix_ts_test.c*/
int st_self_test(void);
void sitronix_replace_test_cmd(unsigned char *id);
#endif /* __SITRONIX_TS_H__ */
