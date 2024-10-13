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

#include "sitronix_ts.h"


struct sitronix_ts_data *gts = {0};
unsigned char wbuf[SITRONIX_RW_BUF_LEN+8] = {0};
unsigned char rbuf[SITRONIX_RW_BUF_LEN+8] = {0};
/* #define SITRONIX_TS_INPUT_PHYS_NAME "sitronix_ts/input0" */


#if defined(CONFIG_FB)
static void sitronix_ts_resume_work(struct work_struct *work);
#ifdef _MSM_DRM_NOTIFY_H_
static int sitronix_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
#else
static int sitronix_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
#endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void sitronix_ts_early_suspend(struct early_suspend *h);
static void sitronix_ts_late_resume(struct early_suspend *h);
#endif

#if defined(CONFIG_FB)
static int sitronix_ts_suspend(struct device *dev);
static int sitronix_ts_resume(struct device *dev);
#endif

bool sitronix_irq_status = true;
int sitronix_ts_irq_enable(struct sitronix_ts_data *ts_data, bool enable)
{
	int ret = 0;

	stdbg("%s:%u, enable=%u\n", __func__, __LINE__, enable);
	if (enable && !sitronix_irq_status)
		enable_irq(ts_data->irq);
	if (!enable && sitronix_irq_status)
		disable_irq(ts_data->irq);

	sitronix_irq_status = enable;	

	return ret;
}


int sitronix_ts_reset_device(struct sitronix_ts_data *ts_data)
{
#if 0
	int ret;
	uint8_t ctrl = 0x01;

	stmsg("%s: Software Reset device.\n", __func__);

	ret = sitronix_ts_reg_write(ts_data, DEVICE_CONTROL_REG, &ctrl, sizeof(ctrl));
	if (ret < 0) {
		sterr("%s: SW Reset failed!(%d)\n", __func__, ret);
		return ret;
	}

	//msleep(100);
#else /* 0 */

#ifdef SITRONIX_DO_TP_HW_RESET
	stmsg("%s: Hardware Reset device.\n", __func__);

	gpio_set_value(ts_data->host_if->rst_gpio, 0);
	msleep(1);
	gpio_set_value(ts_data->host_if->rst_gpio, 1);
#ifdef SITRONIX_TP_WITH_FLASH
	msleep(100);
#endif
#endif /* SITRONIX_DO_TP_HW_RESET */
#endif /* 0 */

	return 0;
}


static inline void sitronix_ts_pen_down(struct input_dev *input_dev, int id, uint16_t x, uint16_t y)
{
#ifdef SITRONIX_TS_MT_SLOT
	input_mt_slot(input_dev, id);
	input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, true);
	input_report_abs(input_dev, ABS_MT_POSITION_X, x);
	input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
	input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, 1);
	input_report_abs(input_dev, ABS_MT_PRESSURE, 255);
	input_report_abs(input_dev, ABS_MT_TRACKING_ID, id);
#else /* SITRONIX_TS_MT_SLOT */
	input_report_abs(input_dev, ABS_MT_TRACKING_ID, id);
	input_report_abs(input_dev, ABS_MT_POSITION_X, x);
	input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
	input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, 10);
	input_report_abs(input_dev, ABS_MT_TOUCH_MINOR, 10);
	input_report_abs(input_dev, ABS_MT_PRESSURE, 10);
	input_mt_sync(input_dev);
#endif /* SITRONIX_TS_MT_SLOT */
}

static inline void sitronix_ts_pen_up(struct input_dev *input_dev, int id)
{
#ifdef SITRONIX_TS_MT_SLOT
	input_mt_slot(input_dev, id);
	input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
	input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, 0);
	input_report_abs(input_dev, ABS_MT_PRESSURE, 0);
#else /* SITRONIX_TS_MT_SLOT */
#endif /* SITRONIX_TS_MT_SLOT */
}

static inline void sitronix_ts_pen_allup(struct sitronix_ts_data *ts_data)
{
	int i;

	for (i = 0; i < ts_data->ts_dev_info.max_touches; i++)
		sitronix_ts_pen_up(ts_data->input_dev, i);

	input_report_key(ts_data->input_dev, BTN_TOUCH, 0);
	input_sync(ts_data->input_dev);
}

void sitronix_ts_report_swu(struct input_dev *input_dev, uint8_t swu_id)
{
	int key = sitronix_ts_get_swu_keycode(swu_id);

	stmsg("sitronix_ts_report_swu , id = %x , keycode = %x\n", swu_id, key);
	if (key != 0) {
		input_report_key(input_dev, key, 1);
		input_sync(input_dev);
		input_report_key(input_dev, key, 0);
		input_sync(input_dev);
	}
}

void sitronix_ts_report_proximity_demo(struct input_dev *input_dev, uint8_t proximity_id)
{
	if (ST_PROXIMITY_FACEIN_LEVEL_3_ENABLE == 1 && proximity_id == ST_PROXIMITY_FACEIN_LEVEL_3_VALUE) {
		//inforce
		stmsg("proximity status 3 , disp off + sleep in\n");
		//input_report_key(input_dev, ST_KEY_GESTURE_POWER, 1);
		//input_sync(input_dev);
		//input_report_key(input_dev, ST_KEY_GESTURE_POWER, 0);
		//input_sync(input_dev);
		sitronix_write_driver_cmd(0x28, wbuf, 0);
		sitronix_write_driver_cmd(0x10, wbuf, 0);
		msleep(20);
		
		stmsg("proximity status 3 , power down\n");
		sitronix_ts_powerdown(gts, true);
		msleep(20);
		stmsg("proximity status 3 , sleep out\n");
		sitronix_ts_proximity_control_sensing(gts, true);
	}
	
	if (ST_PROXIMITY_FACEOUT_LEVEL_2_ENABLE == 1 && proximity_id == ST_PROXIMITY_FACEOUT_LEVEL_2_VALUE) {
		//inforce
		stmsg("proximity status 5 , disp on\n");
		//input_report_key(input_dev, ST_KEY_GESTURE_POWER, 1);
		//input_sync(input_dev);
		//input_report_key(input_dev, ST_KEY_GESTURE_POWER, 0);
		//input_sync(input_dev);
		
		sitronix_write_driver_cmd(0x29, wbuf, 0);
		msleep(120);
		stmsg("proximity status 5 , power up\n");
		sitronix_ts_powerdown(gts,false);	
		gts->proximity_status = 0;
	}
}


void sitronix_ts_report_proximity(struct input_dev *input_dev, uint8_t proximity_id)
{
	if (ST_PROXIMITY_FACEIN_LEVEL_1_ENABLE == 1 && proximity_id == ST_PROXIMITY_FACEIN_LEVEL_1_VALUE) {
		input_report_abs(input_dev, SITRONIX_PROXIMITY_REPORT_ABS, ST_PROXIMITY_FACEIN_LEVEL_1_REPORT);
		input_sync(input_dev);
	}
	
	if (ST_PROXIMITY_FACEIN_LEVEL_2_ENABLE == 1 && proximity_id == ST_PROXIMITY_FACEIN_LEVEL_2_VALUE) {
		input_report_abs(input_dev, SITRONIX_PROXIMITY_REPORT_ABS, ST_PROXIMITY_FACEIN_LEVEL_2_REPORT);
		input_sync(input_dev);
	}
	
	if (ST_PROXIMITY_FACEIN_LEVEL_3_ENABLE == 1 && proximity_id == ST_PROXIMITY_FACEIN_LEVEL_3_VALUE) {
		gts->proximity_is_facein = true;
		input_report_abs(input_dev, SITRONIX_PROXIMITY_REPORT_ABS, ST_PROXIMITY_FACEIN_LEVEL_3_REPORT);
		input_sync(input_dev);		
	}
	
	if (ST_PROXIMITY_FACEOUT_LEVEL_1_ENABLE == 1 && proximity_id == ST_PROXIMITY_FACEOUT_LEVEL_1_VALUE) {
		input_report_abs(input_dev, SITRONIX_PROXIMITY_REPORT_ABS, ST_PROXIMITY_FACEOUT_LEVEL_1_REPORT);
		input_sync(input_dev);
	}
	
	if (ST_PROXIMITY_FACEOUT_LEVEL_2_ENABLE == 1 && proximity_id == ST_PROXIMITY_FACEOUT_LEVEL_2_VALUE) {
		gts->proximity_is_facein = false;
		input_report_abs(input_dev, SITRONIX_PROXIMITY_REPORT_ABS, ST_PROXIMITY_FACEOUT_LEVEL_2_REPORT);
		input_sync(input_dev);
	}
	
	if (ST_PROXIMITY_FACEOUT_LEVEL_3_ENABLE == 1 && proximity_id == ST_PROXIMITY_FACEOUT_LEVEL_3_VALUE) {
		input_report_abs(input_dev, SITRONIX_PROXIMITY_REPORT_ABS, ST_PROXIMITY_FACEOUT_LEVEL_3_REPORT);
		input_sync(input_dev);
	}
}

static irqreturn_t sitronix_ts_irq_handler(int irq, void *data)
{
	struct sitronix_ts_data *ts_data = (struct sitronix_ts_data *)data;
	int i;
	int ret;
	uint16_t x, y;
	uint8_t read_len = (ts_data->ts_dev_info.max_touches * 7) + 4;
	uint8_t touch_count = 0;
	uint8_t *p;

	if (ts_data->irq_wait_mode == STDEV_IRQ_WAITMODE_RAW) {
	
		ts_data->irq_wait_flag = 1;
		wake_up_interruptible(&ts_data->irq_wait_queue);
		return IRQ_HANDLED;
	}
	
	sitronix_mt_pause_one();

	mutex_lock(&ts_data->mutex);
	stmsg("sitronix_ts_irq_handler\n");
		
	if (ts_data->swu_status && !ts_data->proximity_flag) {
		touch_count = ts_data->swu_gesture_id;
		ret = sitronix_ts_get_swu_gesture(ts_data);
		if (ret < 0) {
			sterr("Sitronix read swu error (%d)\n", ret);
			goto exit_invalid_data;
		}
		if ( touch_count != 0 && ts_data->swu_gesture_id == 0) {
			/* stmsg("Report swu id : %x\n", touch_count); */
			sitronix_ts_report_swu(ts_data->input_dev, touch_count);
		}
		goto exit_invalid_data;
	}

	ret = sitronix_ts_reg_read(ts_data, TOUCH_INFO, ts_data->coord_buf, read_len);
	if (ret < 0) {
		sterr("%s: Read finger touch error!(%d)\n", __func__, ret);
		goto exit_invalid_data;
	}

	if (ts_data->coord_buf[0] & 0x80) {
		/* ESD check fail */
		sterr("Firmware RstChip = 1 , reset device\n");
#ifdef SITRONIX_HDL_IN_IRQ
		sitronix_ts_pen_allup(ts_data);
		sitronix_ts_reset_device(ts_data);
		ts_data->is_reset_chip = true;
		goto exit_invalid_data;
#else
		sitronix_ts_pen_allup(ts_data);
		sitronix_ts_reset_device(ts_data);
#endif
	}

	if (ts_data->is_support_proximity && ts_data->proximity_flag ) {
		touch_count = (ts_data->coord_buf[0] >> 4 ) & 0x07;
		
		if ( touch_count != 0 && touch_count != ts_data->proximity_status ) {
			stmsg("proximity status: %d \n", touch_count);
			ts_data->proximity_status = touch_count;
			if(ts_data->proximity_demo_enable)
				sitronix_ts_report_proximity_demo(ts_data->input_dev, touch_count);
			sitronix_ts_report_proximity(ts_data->input_dev_proximity, touch_count);
			//goto exit_invalid_data;
		}
#if 0
		if (!ts_data->proximity_status && ts_data->coord_buf[0] & 0x40) {
			/* proximity face in  */
			input_report_abs(ts_data->input_dev_proximity, SITRONIX_PROXIMITY_REPORT_ABS, SITRONIX_PROXIMITY_REPORT_VALUE_MAX);
			input_sync(ts_data->input_dev_proximity);
			ts_data->proximity_status = true;
			goto exit_invalid_data;
		}
		if (ts_data->proximity_status && !(ts_data->coord_buf[0] & 0x40)) {
			/* proximity face out */
			input_report_abs(ts_data->input_dev_proximity, SITRONIX_PROXIMITY_REPORT_ABS, SITRONIX_PROXIMITY_REPORT_VALUE_MIN);
			input_sync(ts_data->input_dev_proximity);
			ts_data->proximity_status = false;
			goto exit_invalid_data;
		}
#endif
	}

	touch_count = 0;
	p = &ts_data->coord_buf[4];
	for (i = 0; i < ts_data->ts_dev_info.max_touches; i++) {
		if (*p & 0x80) {
			touch_count++;
			x = (uint16_t)(((uint16_t)(*p & 0x3F) << 8) | ((uint16_t)*(p + 1) & 0xFF));
			y = (uint16_t)(((uint16_t)(*(p + 2) & 0x3F) << 8) | ((uint16_t)*(p + 3) & 0xFF));
			sitronix_ts_pen_down(ts_data->input_dev, i, x, y);
		} else {
#ifdef SITRONIX_TS_MT_SLOT
			sitronix_ts_pen_up(ts_data->input_dev, i);
#endif /* SITRONIX_TS_MT_SLOT */
		}
		p += 7;
	}
#ifndef SITRONIX_TS_MT_SLOT
	if (touch_count == 0)
		sitronix_ts_pen_up(ts_data->input_dev, 0);
#endif /* SITRONIX_TS_MT_SLOT */
	input_report_key(ts_data->input_dev, BTN_TOUCH, (touch_count > 0));
	input_sync(ts_data->input_dev);

	if (ts_data->irq_wait_mode == STDEV_IRQ_WAITMODE_COORD) {		
		ts_data->irq_wait_flag = 1;
		wake_up_interruptible(&ts_data->irq_wait_queue);
	}
exit_invalid_data:
	mutex_unlock(&ts_data->mutex);
	return IRQ_HANDLED;
}

static void sitronix_ts_input_set_params(struct sitronix_ts_data *ts_data)
{
	struct sitronix_ts_device_info *ts_dev_info = &ts_data->ts_dev_info;

#ifdef SITRONIX_TS_MT_SLOT
	input_mt_init_slots(ts_data->input_dev, ts_dev_info->max_touches, INPUT_MT_DIRECT);
	set_bit(INPUT_PROP_DIRECT, ts_data->input_dev->propbit);
	input_set_abs_params(ts_data->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts_data->input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(ts_data->input_dev, ABS_MT_TRACKING_ID, 0, ts_dev_info->max_touches, 0, 0);
#else /* SITRONIX_TS_MT_SLOT */
	set_bit(ABS_X, ts_data->input_dev->absbit);
	set_bit(ABS_Y, ts_data->input_dev->absbit);
	set_bit(ABS_MT_TOUCH_MAJOR, ts_data->input_dev->absbit);
	set_bit(ABS_MT_TOUCH_MINOR, ts_data->input_dev->absbit);
	set_bit(ABS_MT_POSITION_X, ts_data->input_dev->absbit);
	set_bit(ABS_MT_POSITION_Y, ts_data->input_dev->absbit);
	set_bit(ABS_MT_TOOL_TYPE, ts_data->input_dev->absbit);
	set_bit(ABS_MT_BLOB_ID, ts_data->input_dev->absbit);
	set_bit(ABS_MT_TRACKING_ID, ts_data->input_dev->absbit);
	set_bit(INPUT_PROP_DIRECT, ts_data->input_dev->propbit);
	input_set_abs_params(ts_data->input_dev, ABS_MT_TRACKING_ID, 0, ts_dev_info->max_touches, 0, 0);
	input_set_abs_params(ts_data->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts_data->input_dev, ABS_MT_TOUCH_MINOR, 0, 255, 0, 0);
	input_set_abs_params(ts_data->input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
#endif /* SITRONIX_TS_MT_SLOT */
	input_set_abs_params(ts_data->input_dev, ABS_MT_POSITION_X, 0, (ts_dev_info->x_res - 1), 0, 0);
	input_set_abs_params(ts_data->input_dev, ABS_MT_POSITION_Y, 0, (ts_dev_info->y_res - 1), 0, 0);

	return;
}
static int sitronix_ts_input_dev_proximity_init(struct sitronix_ts_data *ts_data)
{
	int ret = 0;

	ts_data->input_dev_proximity = input_allocate_device();
	
	if (ts_data->input_dev_proximity == NULL) {
		sterr("%s: Can not allocate input device proximity!\n", __func__);
		return -ENOMEM;
	}
	
	ts_data->input_dev_proximity->name = "sitronix_touch_proximity";
	ts_data->input_dev_proximity->id.bustype = ts_data->host_if->bus_type;
	ts_data->input_dev_proximity->dev.parent = ts_data->pdev->dev.parent;

	set_bit(EV_SYN, ts_data->input_dev_proximity->evbit);
	set_bit(EV_SW, ts_data->input_dev_proximity->evbit);
	set_bit(INPUT_PROP_DIRECT, ts_data->input_dev_proximity->propbit);

	input_set_abs_params(ts_data->input_dev_proximity, SITRONIX_PROXIMITY_REPORT_ABS, SITRONIX_PROXIMITY_REPORT_VALUE_MIN, SITRONIX_PROXIMITY_REPORT_VALUE_MAX, 0, 0);
	input_set_drvdata(ts_data->input_dev_proximity, ts_data);

	ret = input_register_device(ts_data->input_dev_proximity);
	if (ret) {
		sterr("%s: Failed to register input device proximity\n", __func__);
		return ret;
	}
	
	return ret;
}
static int sitronix_ts_input_dev_init(struct sitronix_ts_data *ts_data)
{
	int ret = 0;

	ts_data->input_dev = input_allocate_device();
	if (ts_data->input_dev == NULL) {
		sterr("%s: Can not allocate input device!\n", __func__);
		return -ENOMEM;
	}

	ts_data->input_dev->name = ts_data->name;
	/* ts_data->input_dev->phys = SITRONIX_TS_INPUT_PHYS_NAME; */
	ts_data->input_dev->id.bustype = ts_data->host_if->bus_type;
	ts_data->input_dev->id.product = 0;
	ts_data->input_dev->id.version = 0;
	ts_data->input_dev->dev.parent = ts_data->pdev->dev.parent;
	input_set_drvdata(ts_data->input_dev, ts_data);

	set_bit(EV_SYN, ts_data->input_dev->evbit);
	set_bit(EV_KEY, ts_data->input_dev->evbit);
	set_bit(EV_ABS, ts_data->input_dev->evbit);
	set_bit(BTN_TOUCH, ts_data->input_dev->keybit);
	/* set_bit(KEY_POWER, ts_data->input_dev->keybit); */
	/* set_bit(KEY_MENU, ts_data->input_dev->keybit); */

	/* SWU */
	input_set_capability(ts_data->input_dev, EV_KEY, ST_KEY_GESTURE_POWER);
	input_set_capability(ts_data->input_dev, EV_KEY, ST_KEY_GESTURE_U);
	input_set_capability(ts_data->input_dev, EV_KEY, ST_KEY_GESTURE_UP);
	input_set_capability(ts_data->input_dev, EV_KEY, ST_KEY_GESTURE_DOWN);
	input_set_capability(ts_data->input_dev, EV_KEY, ST_KEY_GESTURE_LEFT);
	input_set_capability(ts_data->input_dev, EV_KEY, ST_KEY_GESTURE_RIGHT);
	input_set_capability(ts_data->input_dev, EV_KEY, ST_KEY_GESTURE_O);
	input_set_capability(ts_data->input_dev, EV_KEY, ST_KEY_GESTURE_E);
	input_set_capability(ts_data->input_dev, EV_KEY, ST_KEY_GESTURE_M);
	input_set_capability(ts_data->input_dev, EV_KEY, ST_KEY_GESTURE_L);
	input_set_capability(ts_data->input_dev, EV_KEY, ST_KEY_GESTURE_W);
	input_set_capability(ts_data->input_dev, EV_KEY, ST_KEY_GESTURE_S);
	input_set_capability(ts_data->input_dev, EV_KEY, ST_KEY_GESTURE_V);
	input_set_capability(ts_data->input_dev, EV_KEY, ST_KEY_GESTURE_Z);
	input_set_capability(ts_data->input_dev, EV_KEY, ST_KEY_GESTURE_C);

	set_bit(ST_KEY_GESTURE_POWER, ts_data->input_dev->keybit);
	set_bit(ST_KEY_GESTURE_RIGHT, ts_data->input_dev->keybit);
	set_bit(ST_KEY_GESTURE_LEFT, ts_data->input_dev->keybit);
	set_bit(ST_KEY_GESTURE_UP, ts_data->input_dev->keybit);
	set_bit(ST_KEY_GESTURE_DOWN, ts_data->input_dev->keybit);
	set_bit(ST_KEY_GESTURE_U, ts_data->input_dev->keybit);
	set_bit(ST_KEY_GESTURE_O, ts_data->input_dev->keybit);
	set_bit(ST_KEY_GESTURE_E, ts_data->input_dev->keybit);
	set_bit(ST_KEY_GESTURE_M, ts_data->input_dev->keybit);
	set_bit(ST_KEY_GESTURE_W, ts_data->input_dev->keybit);
	set_bit(ST_KEY_GESTURE_L, ts_data->input_dev->keybit);
	set_bit(ST_KEY_GESTURE_S, ts_data->input_dev->keybit);
	set_bit(ST_KEY_GESTURE_V, ts_data->input_dev->keybit);
	set_bit(ST_KEY_GESTURE_Z, ts_data->input_dev->keybit);
	set_bit(ST_KEY_GESTURE_C, ts_data->input_dev->keybit);

	sitronix_ts_input_set_params(ts_data);

	ret = input_register_device(ts_data->input_dev);
	if (ret) {
		sterr("%s: Failed to register input device\n", __func__);
		goto err_register_input;
	}

	return 0;

err_register_input:
	input_free_device(ts_data->input_dev);
	ts_data->input_dev = NULL;

	return ret;
}

static void sitronix_ts_check_display_id(void)
{
	unsigned char id[3];
	int ret = 0;
	
	ret = sitronix_get_display_id(id);
	if(ret < 0) {
		sterr("Read Display ID failed , use default FW and cfg buffer\n");
		return ;
	} else {
		sitronix_replace_dump_buf(id);
		sitronix_replace_test_cmd(id);
	}
}

static bool sitronix_ts_check_ic_sfrver(void)
{
#ifndef SITRONIX_VALIDATE_IC_SFRVER
	return true;
#else
	int ret = 0;

	ret = sitronix_get_ic_sfrver();
	stmsg("IC SFR VER = 0x%X\n",ret);
#ifdef SITRONIX_SFRVER_44
	if(ret == 0x44)
		return true;
	else
		sterr("Host define SITRONIX_SFRVER_44\n");
#endif /* SITRONIX_SFRVER_44*/
#ifdef SITRONIX_SFRVER_43
	if(ret == 0x43 || ret == 0x44)
		return true;
	else
		sterr("Host define SITRONIX_SFRVER_43\n");
#endif /* SITRONIX_SFRVER_43*/

#ifdef SITRONIX_SFRVER_33
	if(ret == 0x33 || ret == 0x00)
		return true;
	else
		sterr("Host define SITRONIX_SFRVER_33\n");
#endif /* SITRONIX_SFRVER_33*/

#endif /* SITRONIX_VALIDATE_IC_SFRVER */
	return false;
}

/**
 * sitronix_ts_probe()
 *
 */
static int sitronix_ts_probe(struct platform_device *pdev)
{
	int ret;
	struct sitronix_ts_data *ts_data = NULL;
	const struct sitronix_ts_host_interface *host_if;
	int status;

	ST_START_PRINT;
	host_if = pdev->dev.platform_data;
	if (!host_if) {
		sterr("%s: No hardware interface found!\n", __func__);
		return -EINVAL;
	}

	ts_data = kzalloc(sizeof(*ts_data), GFP_KERNEL);
	if (!ts_data) {
		sterr("%s: Alloc memory for ts_data failed!\n", __func__);
		return -ENOMEM;
	}

	ts_data->skip_first_resume = true;
	ts_data->name = pdev->name;
	ts_data->pdev = pdev;
	ts_data->host_if = host_if;
	ts_data->in_suspend = false;
	ts_data->wr_len = STDEV_WR_DF;
	gts = ts_data;

	platform_set_drvdata(pdev, ts_data);
	sitronix_ts_reset_device(gts);
	if (!sitronix_ts_check_ic_sfrver()) {
		sterr("%s: Failed to sitronix_ts_check_ic_sfrver\n", __func__);
		return -EINVAL;
	}
	sitronix_ts_check_display_id();

#ifdef SITRONIX_HDL_IN_PROBE
	ret = sitronix_do_upgrade();
	if (ret < 0) {
		sterr("%s: Failed to Host Download\n", __func__);
		return -EINVAL;
	}
#else
	sitronix_ts_reset_device(gts);
#endif /* SITRONIX_HDL_IN_PROBE */

	status = sitronix_ts_get_device_status(ts_data);
	ret = sitronix_ts_get_device_info(ts_data);
	if (ret) {
		sterr("%s: Failed to get device info.\n", __func__);
		ret = -EINVAL;
		goto err_return;
	}

	ret = sitronix_ts_input_dev_init(ts_data);
	if (ret) {
		sterr("%s: Failed to set up input device\n", __func__);
		ret = -EINVAL;
		goto err_return;
	}
	
#ifdef SITRONIX_SUPPORT_PROXIMITY
#ifdef SITRONIX_PROXIMITY_DEMO
	ts_data->proximity_demo_enable = true;
#endif
#endif	

	if (ts_data->is_support_proximity) {
		ret = sitronix_ts_input_dev_proximity_init(ts_data);
		if (ret) {
			sterr("%s: Failed to set up input device\n", __func__);
			ret = -EINVAL;
			goto err_return;
		}
	}
	ts_data->irq = gpio_to_irq(host_if->irq_gpio);
	if (ts_data->irq < 0) {
		sterr("%s: gpio_to_irq() failed!\n", __func__);
		ret = -EINVAL;
		goto err_return;
	}

	ts_data->irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND;
	ret = request_threaded_irq(ts_data->irq, NULL, sitronix_ts_irq_handler,
			ts_data->irq_flags, ts_data->name, ts_data);
	if (ret < 0) {
		sterr("%s: request_irq failed!\n", __func__);
		goto err_return;
	}


#if defined(CONFIG_FB)
	ts_data->workqueue = create_singlethread_workqueue("sitronix_ts_workqueue");
	if (!ts_data->workqueue) {
		sterr("create sitronix_ts_workqueue fail");
		ret = -ENOMEM;
		goto err_create_sitronix_ts_workqueue_failed;
	}
	INIT_WORK(&ts_data->resume_work, sitronix_ts_resume_work);


#ifdef _MSM_DRM_NOTIFY_H_
	ts_data->drm_notif.notifier_call = sitronix_drm_notifier_callback;
	ret = msm_drm_register_client(&ts_data->drm_notif);
	if (ret) {
		sterr("register drm_notifier failed. ret=%d\n", ret);
		goto err_register_drm_notif_failed;
	}
#else
	ts_data->fb_notif.notifier_call = sitronix_fb_notifier_callback;
	ret = fb_register_client(&ts_data->fb_notif);
	if (ret) {
		sterr("register fb_notifier failed. ret=%d\n", ret);
		goto err_register_fb_notif_failed;
	}
#endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = sitronix_ts_early_suspend;
	ts->early_suspend.resume = sitronix_ts_late_resume;
	ret = register_early_suspend(&ts_data->early_suspend);
	if (ret) {
		sterr("register early suspend failed. ret=%d\n", ret);
		goto err_register_early_suspend_failed;
	}
#endif

	ret = sitronix_create_sysfs(ts_data);
	if (ret) {
		sterr("Failed to create sysfs attributes, err: %d\n", ret);
		goto err_remove_sysfs;
	}

	mutex_init(&ts_data->mutex);

#ifdef SITRONIX_MONITOR_THREAD

	if (ts_data->ts_dev_info.chip_id > 3) {
		ts_data->enable_monitor_thread = true;
		ts_data->sitronix_mt_fp = sitronix_ts_monitor_thread_v3;
		sitronix_mt_start(DELAY_MONITOR_THREAD_START_PROBE);
	}
#endif /* SITRONIX_MONITOR_THREAD */

	ret = sitronix_create_st_dev(ts_data);
	if (ret) {
		sterr("Failed to create device node, err: %d\n", ret);
		goto err_remove_st_dev;
	}

	ret = sitronix_create_proc();
	if (ret) {
		sterr("Failed to create proc node, err: %d\n", ret);
		goto err_remove_st_proc;
	}
	return ret;

#if defined(CONFIG_FB)
err_create_sitronix_ts_workqueue_failed:
	if (ts_data->workqueue)
		destroy_workqueue(ts_data->workqueue);
#ifdef _MSM_DRM_NOTIFY_H_
	if (msm_drm_unregister_client(&ts->drm_notif))
		sterr("Error occurred while unregistering drm_notifier.\n");
err_register_drm_notif_failed:
#else
	if (fb_unregister_client(&ts_data->fb_notif))
		sterr("Error occurred while unregistering fb_notifier.\n");
err_register_fb_notif_failed:
#endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts_data->early_suspend);
err_register_early_suspend_failed:
#endif

err_remove_st_proc:
	sitronix_remove_proc();

err_remove_st_dev:
	sitronix_remove_st_dev(ts_data);

err_remove_sysfs:
	sitronix_remove_sysfs(ts_data);

err_return:

#ifdef SITRONIX_MONITOR_THREAD
	sitronix_mt_stop();
#endif /* SITRONIX_MONITOR_THREAD */

	if (ts_data->input_dev) {
		input_unregister_device(ts_data->input_dev);
		input_free_device(ts_data->input_dev);
		ts_data->input_dev = NULL;
	}
	
	if (ts_data->input_dev_proximity) {
		input_unregister_device(ts_data->input_dev_proximity);
		input_free_device(ts_data->input_dev_proximity);
		ts_data->input_dev_proximity = NULL;
	}



	if (ts_data)
		kfree(ts_data);

	if (ts_data->irq > 0)
		free_irq(ts_data->irq, ts_data);

	return ret;
}

static int sitronix_ts_remove(struct platform_device *pdev)
{
	struct sitronix_ts_data *ts_data = platform_get_drvdata(pdev);

	sitronix_remove_proc();

	sitronix_remove_st_dev(ts_data);

	sitronix_remove_sysfs(ts_data);

#ifdef SITRONIX_MONITOR_THREAD
	sitronix_mt_stop();
#endif /* SITRONIX_MONITOR_THREAD */


	if (ts_data->irq > 0)
		free_irq(ts_data->irq, ts_data);

#if defined(CONFIG_FB)
	if (ts_data->workqueue)
		destroy_workqueue(ts_data->workqueue);
#ifdef _MSM_DRM_NOTIFY_H_
	if (msm_drm_unregister_client(&ts_data->drm_notif))
		sterr("Error occurred while unregistering drm_notifier.\n");
#else
	if (fb_unregister_client(&ts_data->fb_notif))
		sterr("Error occurred while unregistering fb_notifier.\n");
#endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts_data->early_suspend);
#endif

	if (ts_data->input_dev) {
		input_unregister_device(ts_data->input_dev);
		input_free_device(ts_data->input_dev);
	}
	
	if (ts_data->input_dev_proximity) {
		input_unregister_device(ts_data->input_dev_proximity);
		input_free_device(ts_data->input_dev_proximity);
	}

	if (ts_data)
		kfree(ts_data);

	mutex_destroy(&ts_data->mutex);

	return 0;
}


#if defined(CONFIG_FB)
static int sitronix_ts_suspend(struct device *dev)
{
	/* struct sitronix_ts_data *ts_data = dev_get_drvdata(dev); */

	/* stdbg("%s:%u, ts_data=0x%x\n", __FUNCTION__, __LINE__, (uint32_t)ts_data); */
	stmsg("run sitronix_ts_suspend\n");
	gts->skip_first_resume = false;
	mutex_lock(&gts->mutex);
/*
	if (gts->in_suspend) {
		stdbg("%s:%u, Already in suspend!\n", __func__, __LINE__);
		mutex_unlock(&gts->mutex);
		return 0;
	}
*/	

#ifdef SITRONIX_MONITOR_THREAD
#ifdef MONITOR_THREAD_STOP_IN_SUSPEND
	sitronix_mt_stop();
#else
	if (!gts->swu_flag || gts->proximity_flag) {
		sitronix_mt_suspend();
	}
#endif /* MONITOR_THREAD_STOP_IN_SUSPEND */	
#endif /* SITRONIX_MONITOR_THREAD */	
	if (gts->swu_flag) {
		sitronix_ts_set_smart_wake_up(gts, true);
		enable_irq_wake(gts->irq);
		gts->swu_status = true;
	} else {
		sitronix_ts_set_smart_wake_up(gts, false);
		sitronix_ts_irq_enable(gts, false);
	}

	msleep(20);
	sitronix_ts_powerdown(gts, true);

	sitronix_ts_pen_allup(gts);
	
	gts->in_suspend = true;
	mutex_unlock(&gts->mutex);
	return 0;
}

static int sitronix_ts_resume(struct device *dev)
{
	/* struct sitronix_ts_data *ts_data = dev_get_drvdata(dev); */
	/* stdbg("%s:%u, ts_data=0x%x\n", __FUNCTION__, __LINE__, (uint32_t)ts_data); */
	stmsg("run sitronix_ts_resume\n");
	
	if (gts->skip_first_resume) {
		gts->skip_first_resume = false;
		return 0;
	}	
#ifdef SITRONIX_TP_RESUME_BEFORE_DISPON
	msleep(SITRONIX_RESUME_DELAY);
#endif	/* SITRONIX_TP_RESUME_BEFORE_DISPON */
	mutex_lock(&gts->mutex);
/*	
	if (!gts->in_suspend) {
		stdbg("%s:%u, Already in resume!\n", __func__, __LINE__);
		mutex_unlock(&gts->mutex);
		return 0;
	}
*/

#ifdef SITRONIX_HDL_IN_RESUME
	sitronix_do_upgrade();
#endif /* SITRONIX_HDL_IN_RESUME */
	sitronix_ts_powerdown(gts, false);
	/* kthread_run(sitronix_ts_powerdown_thread, gts, "Sitronix powerdown Thread"); */
	/* msleep(200); */
	/* sitronix_ts_reset_device(gts); */

	if (gts->swu_flag) {
		disable_irq_wake(gts->irq);
		/* sitronix_ts_set_smart_wake_up(ts_data, false); */
	} else {
		sitronix_ts_irq_enable(gts, true);
	}

	gts->swu_status = false;
	gts->in_suspend = false;

	sitronix_mode_restore();
	mutex_unlock(&gts->mutex);

#ifdef SITRONIX_MONITOR_THREAD
#ifdef MONITOR_THREAD_STOP_IN_SUSPEND
	sitronix_mt_start(DELAY_MONITOR_THREAD_START_RESUME);
#else
	if (!gts->swu_flag || gts->proximity_flag) {
		sitronix_mt_resume();
	}
#endif /* MONITOR_THREAD_STOP_IN_SUSPEND */		
#endif /* SITRONIX_MONITOR_THREAD */
	return 0;
}

#endif

#if defined(CONFIG_FB)
static void sitronix_ts_resume_work(struct work_struct *work)
{
	sitronix_ts_resume(&gts->pdev->dev);
}
#ifdef _MSM_DRM_NOTIFY_H_
static int sitronix_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct msm_drm_notifier *evdata = data;
	int *blank;
	struct ts_sitronix *ts =
		container_of(self, struct ts_sitronix, drm_notif);

	if (!evdata || (evdata->id != 0))
		return 0;

	if (evdata->data && ts) {
		blank = evdata->data;
#ifdef SITRONIX_TP_RESUME_BEFORE_DISPON
		if (event == MSM_DRM_EARLY_EVENT_BLANK) {
			if (*blank == MSM_DRM_BLANK_UNBLANK) {
				//stdbg("event=%lu, *blank=%d\n", event, *blank);
				queue_work(ts->workqueue, &ts->resume_work);
				//sitronix_ts_resume(&ts->pdev->dev);
			}
		}
#endif /* SITRONIX_TP_RESUME_BEFORE_DISPON */
		if (event == MSM_DRM_EVENT_BLANK) {
			if (*blank == MSM_DRM_BLANK_POWERDOWN) {
				//stdbg("event=%lu, *blank=%d\n", event, *blank);
				cancel_work_sync(&ts->resume_work);
				sitronix_ts_suspend(&ts->pdev->dev);
			}
#ifndef SITRONIX_TP_RESUME_BEFORE_DISPON
			if (*blank == MSM_DRM_BLANK_UNBLANK) {
				//stdbg("event=%lu, *blank=%d\n", event, *blank);				
				queue_work(ts->workqueue, &ts->resume_work);
			}
#endif /* SITRONIX_TP_RESUME_BEFORE_DISPON */
		}
	}

	return 0;
}
#else
static int sitronix_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{

	struct fb_event *evdata = data;
	int *blank;
	struct sitronix_ts_data *ts =
		container_of(self, struct sitronix_ts_data, fb_notif);
	/* blank = evdata->data; */
	/* stmsg("event = 0x%02X ,blank = 0x%02X\n",(int)event , *blank); */
	/* stmsg("event = 0x%02X \n",(int)event ); */
#ifdef SITRONIX_TP_RESUME_BEFORE_DISPON
	if (evdata && evdata->data && event == FB_EARLY_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK) {
			/* stmsg("event=%lx, *blank=%d\n", event, *blank); */
			queue_work(ts->workqueue, &ts->resume_work);
			//sitronix_ts_resume(&ts->pdev->dev);
		}
	}
#endif /* SITRONIX_TP_RESUME_BEFORE_DISPON */

	if (evdata && evdata->data && event == FB_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_POWERDOWN) {
			/* stmsg("event=%lx, *blank=%d\n", event, *blank); */
			cancel_work_sync(&ts->resume_work);			
			sitronix_ts_suspend(&ts->pdev->dev);			
		}
#ifndef SITRONIX_TP_RESUME_BEFORE_DISPON		
		if (*blank == FB_BLANK_UNBLANK) {
			/* stmsg("event=%lu, *blank=%d\n", event, *blank); */
			/* sitronix_ts_resume(&ts->pdev->dev); */
			queue_work(ts->workqueue, &ts->resume_work);
		}
#endif /* SITRONIX_TP_RESUME_BEFORE_DISPON */
	}
	return 0;
}
#endif
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void sitronix_ts_early_suspend(struct early_suspend *h)
{
	struct sitronix_ts_data *ts_data = container_of(h, struct sitronix_ts_data, early_suspend);
	sitronix_ts_suspend(&ts_data->pdev->dev);
}

static void sitronix_ts_late_resume(struct early_suspend *h)
{
	struct sitronix_ts_data *ts_data = container_of(h, struct sitronix_ts_data, early_suspend);

	sitronix_ts_resume(&ts_data->pdev->dev);
}
#endif /* CONFIG_HAS_EARLYSUSPEND */

//#ifdef CONFIG_PM
#if (!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND))
static int sitronix_pm_suspend(struct device *dev)
{
	stmsg("pm suspend\n");
	return 0;
}

static int sitronix_pm_resume(struct device *dev)
{	
	stmsg("pm resume");
	return 0;
}

static const struct dev_pm_ops sitronix_ts_dev_pm_ops = {
	.suspend = sitronix_pm_suspend, 
	.resume  = sitronix_pm_resume, 
};
#endif /* CONFIG_PM */

#ifdef SITRONIX_INTERFACE_SPI
static struct platform_driver sitronix_ts_spi_driver = {
	.driver = {
		.name = SITRONIX_TS_SPI_DRIVER_NAME,
		.owner = THIS_MODULE,
//#ifdef CONFIG_PM
#if (!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND))
		.pm = &sitronix_ts_dev_pm_ops,
#endif
	},
	.probe = sitronix_ts_probe,
	.remove = sitronix_ts_remove,
};
#endif /* SITRONIX_INTERFACE_SPI */


#ifdef SITRONIX_INTERFACE_I2C
static struct platform_driver sitronix_ts_i2c_driver = {
	.driver = {
		.name = SITRONIX_TS_I2C_DRIVER_NAME,
		.owner = THIS_MODULE,
//#ifdef CONFIG_PM
#if (!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND))
		.pm = &sitronix_ts_dev_pm_ops,
#endif
	},
	.probe = sitronix_ts_probe,
	/*.remove = __exit_p(sitronix_ts_remove),*/
	.remove = sitronix_ts_remove,
};
#endif /* SITRONIX_INTERFACE_I2C */

static int __init sitronix_ts_init(void)
{
	int ret;
	stmsg("%s\n", SITRONIX_TP_DRIVER_VERSION);
#ifdef SITRONIX_INTERFACE_SPI
	ret = sitronix_ts_spi_init();
	if (ret  == 0)
		platform_driver_register(&sitronix_ts_spi_driver);
#endif /* SITRONIX_INTERFACE_SPI */

#ifdef SITRONIX_INTERFACE_I2C
	ret = sitronix_ts_i2c_init();
	if (ret == 0)
		platform_driver_register(&sitronix_ts_i2c_driver);
#endif /* #SITRONIX_INTERFACE_I2C */
	return ret;
}

static void __exit sitronix_ts_exit(void)
{
#ifdef SITRONIX_INTERFACE_SPI
	platform_driver_unregister(&sitronix_ts_spi_driver);
	sitronix_ts_spi_exit();
#endif /* SITRONIX_INTERFACE_SPI */

#ifdef SITRONIX_INTERFACE_I2C
	platform_driver_unregister(&sitronix_ts_i2c_driver);
	sitronix_ts_i2c_exit();
#endif /* #ifdef SITRONIX_INTERFACE_I2C */
	return;
}

module_init(sitronix_ts_init);
module_exit(sitronix_ts_exit);

MODULE_AUTHOR("Sitronix Technology Co., Ltd.");
MODULE_DESCRIPTION("Sitronix Touchscreen Controller Driver");
MODULE_LICENSE("GPL");
