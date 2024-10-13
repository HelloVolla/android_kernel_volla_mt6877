/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include <linux/string.h>
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
#include <linux/kthread.h>
#include <linux/input.h>

#include <linux/sysfs.h>

#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>


#define SWITCH_BATTERY_TAG "[switch_battery]"
#define SWITCH_BATTERY_DEBUG
#ifdef SWITCH_BATTERY_DEBUG
#define SWITCH_BATTERY_LOG(fmt, arg...)   printk(SWITCH_BATTERY_TAG fmt, ##arg)
#else
#define SWITCH_BATTERY_LOG(fmt, arg...) {;}
#endif

//static struct work_struct switch_battery_work;
//static struct workqueue_struct *switch_battery_workqueue = NULL;

static DECLARE_WAIT_QUEUE_HEAD(switch_battery_thread_wq);
static unsigned int switch_battery_thread_wq_flag = 0;
static ktime_t switch_ktime;
static spinlock_t wake_switch_state_lock;

static struct hrtimer switch_battery_timer;

extern int prize_det_battery_status(char level);

struct swtich_battery{
	struct device *dev;
	int gpio_info;
	int irq;
	int curent_status;
	int last_status;
	int set_debounce;
	int key_eint_type;
};

struct swtich_battery *swtich_battery_info = NULL;
static const struct of_device_id swtich_battery_of_match[] = {
	{.compatible = "mediatek,switch_battery"},
	{},
};
MODULE_DEVICE_TABLE(of, swtich_battery_of_match);

#ifdef CONFIG_PINCTRL
static struct pinctrl *switch_battery_ctrl;
static struct pinctrl_state *switch_battery_as_int; 

#endif

void wake_up_switch_battery_thread_wq(void)
{
		
	spin_lock(&wake_switch_state_lock);
	
	switch_battery_thread_wq_flag = 1;

	wake_up(&switch_battery_thread_wq);
	
	spin_unlock(&wake_switch_state_lock);
}


enum hrtimer_restart switch_battery_hrtimer_func(struct hrtimer *timer)
{
	//wake_up_switch_battery_thread_wq();
	
	hrtimer_start(&switch_battery_timer, switch_ktime, HRTIMER_MODE_REL);
	
	return HRTIMER_NORESTART;
}

void switch_battery_hrtimer_init(void)
{
	ktime_t ktime;
	
	ktime = ktime_set(50, 0); 
	
	SWITCH_BATTERY_LOG("switch_battery_hrtimer_init..\n");

	hrtimer_init(&switch_battery_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	switch_battery_timer.function = switch_battery_hrtimer_func;
	hrtimer_start(&switch_battery_timer, ktime, HRTIMER_MODE_REL);

}

static void switch_battery_update_state(int state)
{    
	/*/sys/devices/platform/switch_battery/switch_state */   // upload the state
	  int err = 0;
	  char temp_event[20];
	  char *envp[2] = { temp_event, NULL };
	
	  snprintf(temp_event, 20, "switch_state=%d", state);
	  err = kobject_uevent_env(&(swtich_battery_info->dev->kobj), KOBJ_CHANGE, envp);
	  SWITCH_BATTERY_LOG("%s(), state = %d, err = %d\n", __func__, state, err);
}

static void switch_battery_handler_logic(void)
{
	
	SWITCH_BATTERY_LOG("%s\n", __func__);
	
	swtich_battery_info->curent_status = __gpio_get_value(swtich_battery_info->gpio_info)?1:0;	
	SWITCH_BATTERY_LOG("%s curent_status=%d,last_status=%d\n", __func__,swtich_battery_info->curent_status,swtich_battery_info->last_status);


	if(swtich_battery_info->curent_status == 0)
	{
		irq_set_irq_type(swtich_battery_info->irq, IRQ_TYPE_LEVEL_HIGH);
	}
	else
	{
		irq_set_irq_type(swtich_battery_info->irq, IRQ_TYPE_LEVEL_LOW);
	}
	
	if(swtich_battery_info->curent_status != swtich_battery_info->last_status)
	{
		switch_battery_update_state(swtich_battery_info->curent_status);
	}
	swtich_battery_info->last_status = swtich_battery_info->curent_status;
	
	gpio_set_debounce(swtich_battery_info->gpio_info, swtich_battery_info->set_debounce);
	enable_irq(swtich_battery_info->irq);
}

static int switch_battery_thread_routine(void *x)
{
	while (1) 
	{
		wait_event(switch_battery_thread_wq, (switch_battery_thread_wq_flag == 1));

		switch_battery_handler_logic();
		switch_battery_thread_wq_flag = 0;

		prize_det_battery_status(swtich_battery_info->curent_status);
		//SWITCH_BATTERY_LOG("switch_battery_thread_routine......wake up...........\n");
	}
}
static ssize_t switch_battery_show_status(struct device* dev, struct device_attribute *attr, char *buf)
{

	if (!dev) 
	{		
		SWITCH_BATTERY_LOG("dev is null!!\n");		
		return 0;	
	}	
	
	SWITCH_BATTERY_LOG("show code = %d  \n", swtich_battery_info->curent_status);	
	
	return scnprintf(buf, PAGE_SIZE, "%d\n", swtich_battery_info->curent_status);
}

static DEVICE_ATTR(switch_state, 0664, switch_battery_show_status, NULL);

static int switch_battery_gpio_init(struct device *dev)
{    
	int ret=0;

	SWITCH_BATTERY_LOG("enter %s, %d\n", __func__, __LINE__);

	switch_battery_ctrl = devm_pinctrl_get(dev);
	if (IS_ERR(switch_battery_ctrl)) 
	{
		ret = PTR_ERR(switch_battery_ctrl);
		SWITCH_BATTERY_LOG(" Cannot find switch_battery_ctrl!\n");
		return ret;
	}

	switch_battery_as_int = pinctrl_lookup_state(switch_battery_ctrl, "switch_battery_gpio_as_int");
	if (IS_ERR(switch_battery_as_int)) 
	{
		ret = PTR_ERR(switch_battery_as_int);
		SWITCH_BATTERY_LOG("%s : pinctrl err, switch_battery_as_int \n", __func__);
	}
	
    ret = of_get_named_gpio(dev->of_node,"switch_battery_gpio",0);
	if(ret < 0)
	{
		SWITCH_BATTERY_LOG("Get switch_battery_detect_as_int error\n");
		return ret;
	}
	swtich_battery_info->gpio_info = ret;	

	SWITCH_BATTERY_LOG(" scankeyctrl_get_info swtich_battery_info->gpio_info=%d end!\n",ret);

    return ret;
}

static irqreturn_t switch_battery_irq_handler(void)
{
	SWITCH_BATTERY_LOG("%s\n", __func__);

	disable_irq_nosync(swtich_battery_info->irq);
	
	wake_up_switch_battery_thread_wq();

	return IRQ_HANDLED;
}

static int switch_battery_register_irq(void)
{
	int ret = 0;
	struct device_node *np = NULL;
	u32 ints[2] = { 0, 0 };
	u32 ints1[2] = { 0, 0 };

	np = of_find_compatible_node(NULL, NULL, "mediatek,switch_battery-eint");
	SWITCH_BATTERY_LOG("%s np:%d\n", __func__,np);

	if (np) 
	{
		of_property_read_u32_array(np, "debounce", ints, ARRAY_SIZE(ints));
		of_property_read_u32_array(np, "interrupts", ints1, ARRAY_SIZE(ints1));
		
		swtich_battery_info->key_eint_type = ints1[1];
		
		SWITCH_BATTERY_LOG("ints1[0]=%d,ints1[1]=%d\n",ints[0],ints[1]);
		gpio_set_debounce(swtich_battery_info->gpio_info, swtich_battery_info->set_debounce);
		swtich_battery_info->irq = irq_of_parse_and_map(np, 0);
	
		if(swtich_battery_info->curent_status == 0)
		{
			ret = request_irq(swtich_battery_info->irq, (irq_handler_t)switch_battery_irq_handler, IRQF_TRIGGER_HIGH, "switch_battery-eint", NULL);

		}
		else
		{
			ret = request_irq(swtich_battery_info->irq, (irq_handler_t)switch_battery_irq_handler, IRQF_TRIGGER_LOW, "switch_battery-eint", NULL);
		}
		if (ret != 0) 
		{
			SWITCH_BATTERY_LOG("[dc]EINT IRQ LINE NOT AVAILABLE\n");
			goto err_request_irq;
		} 
		else 
		{
			SWITCH_BATTERY_LOG("switch_battery set EINT finished, irq=%d, gpio_info=%d\n",swtich_battery_info->irq, swtich_battery_info->gpio_info);
		}
	} 
	else 
	{
		ret = -ENODEV;
		goto err_nodev;
		SWITCH_BATTERY_LOG("%s can't find compatible node\n", __func__);
	}

	return 0;

err_nodev:
err_request_irq:
	return ret;
}


static int switch_battery_probe(struct platform_device *pdev)
{
    int ret = 0;
    const struct of_device_id *id;
	struct device *dev = &pdev->dev;

    SWITCH_BATTERY_LOG("[%s]\n",__func__);
	 
	id = of_match_node(swtich_battery_of_match, pdev->dev.of_node);
	if (!id)
	{
		return -ENODEV;
	}
    swtich_battery_info = devm_kzalloc(&pdev->dev, sizeof(struct swtich_battery), GFP_KERNEL);
    if (!swtich_battery_info)
    {
    	return -ENOMEM;
    }
    platform_set_drvdata(pdev, swtich_battery_info);
	swtich_battery_info->dev = &pdev->dev;

	/*init pinctrl*/
 	ret = switch_battery_gpio_init(dev);
    if(ret < 0){
		SWITCH_BATTERY_LOG("switch_battery_gpio_init fail\n");
		return ret;
    }	
	/*create thread*/
	//switch_battery_workqueue = create_singlethread_workqueue("switch_battery");
	
	//INIT_WORK(&switch_battery_work, switch_battery_handler);

	/*create device file*/
	ret = device_create_file(swtich_battery_info->dev,&dev_attr_switch_state);
	if (ret!= 0)
	{
	    SWITCH_BATTERY_LOG("dev attr switch_state create failed\n");
		return -1;
	}
	else
	{
	    SWITCH_BATTERY_LOG("dev attr switch_state create success\n");
	}
	
	/*create thread*/
	kthread_run(switch_battery_thread_routine, NULL, "switch_battery_thread_routine");

	/*register irq*/
    ret = switch_battery_register_irq();
	if (ret < 0) 
	{
		SWITCH_BATTERY_LOG("%s input dev register failed\r\n", __func__);
		goto irq_register_err;
	}

	switch_battery_hrtimer_init();

	kobject_uevent(&(swtich_battery_info->dev->kobj), KOBJ_ADD);

    SWITCH_BATTERY_LOG("switch_battery_probe success\n");    
    return 0;
	
  irq_register_err:	
  return -1;

}

static int switch_battery_remove(struct platform_device *pdev)
{
	return 0;
}


static struct platform_driver switch_battery_driver = {
	.probe = switch_battery_probe,
	.remove = switch_battery_remove,
	.driver = {
		   .name = "switch_battery",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = swtich_battery_of_match,
#endif
	},
};

static int __init switch_battery_init(void)
{
	int ret = 0;
	
	if (platform_driver_register(&switch_battery_driver)) 
	{
		SWITCH_BATTERY_LOG("%s : failed to register switch battery driver\n", __func__);
		return -ENODEV;
	}

	return ret;
}

static void __exit switch_battery_exit(void)
{
	platform_driver_unregister(&switch_battery_driver);
}

module_init(switch_battery_init);
module_exit(switch_battery_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Switch Battery");
MODULE_AUTHOR("jiang.he <hejiang@chainway.cn>");

