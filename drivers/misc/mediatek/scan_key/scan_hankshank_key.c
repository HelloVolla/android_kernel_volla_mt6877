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
#include <linux/time.h>

#include <linux/sysfs.h>


#define SCAN_KEY_TAG "[scan_key]"
#define SCAN_KEY_DEBUG
#ifdef SCAN_KEY_DEBUG
#define SCAN_KEY_ERR(fmt, arg...)   printk(SCAN_KEY_TAG fmt, ##arg)
#else
#define SCAN_KEY_ERR(fmt, arg...) {;}
#endif

static struct work_struct scan_key_work;
static struct workqueue_struct *scan_key_workqueue = NULL;

struct scan_key{
	struct device *dev;
	const char *eint_name;
	int key_gpio;
	int irq;
	int key_status;
	int key_debounce;
	int key_eint_type;
	struct input_dev *input;
};

struct input_dev *scan_input_dev;
struct scan_key *scan_key_info = NULL;

static int scan_key_probe(struct platform_device *pdev);
static int scan_key_remove(struct platform_device *pdev);

static const struct of_device_id scan_key_of_match[] = {
	{.compatible = "mediatek,scan_key"},
	{},
};
MODULE_DEVICE_TABLE(of, scan_key_of_match);

static struct platform_driver scan_key_driver = {
	.probe = scan_key_probe,
	.remove = scan_key_remove,
	.driver = {
		   .name = "scan_key",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = scan_key_of_match,
#endif
	},
};

/*----------------------------------------------------------------------------*/

#ifdef CONFIG_PINCTRL
static struct pinctrl *scankeyctrl;
static struct pinctrl_state *scan_key_as_int; 

#endif

static int scan_key_gpio_init(struct device	*dev)
{    
	int ret=0;
//	unsigned int mode;
	//const struct of_device_id *match;

	pr_debug("[scan_key][GPIO] enter %s, %d\n", __func__, __LINE__);

	scankeyctrl = devm_pinctrl_get(dev);
	if (IS_ERR(scankeyctrl)) {
		ret = PTR_ERR(scankeyctrl);
		pr_info("[scan_key][ERROR] Cannot find scankeyctrl!\n");
		return ret;
	}

	scan_key_as_int = pinctrl_lookup_state(scankeyctrl, "scan_key_as_int");
	if (IS_ERR(scan_key_as_int)) {
		ret = PTR_ERR(scan_key_as_int);
		printk("%s : pinctrl err, scan_key_as_int \n", __func__);
	}
	
    ret = of_get_named_gpio(dev->of_node,"scan_key_gpio",0);
	if(ret < 0){
		printk("Get scan_key_detect_as_int error\n");
		return ret;
	}
	scan_key_info->key_gpio = ret;	

	printk("[scan_key][GPIO] scankeyctrl_get_info scan_key_info->key_gpio=%d end!\n",ret);

    return ret;
}

static void scan_key_handler(struct work_struct *work)
{
	SCAN_KEY_ERR("%s\n", __func__);

	//scan_key_info->key_status = __gpio_get_value(scan_key_info->key_gpio)?1:0;
	
	scan_key_info->key_status = __gpio_get_value(scan_key_info->key_gpio)?0:1;
	SCAN_KEY_ERR("===wys000===%s=scan_key_info->key_status=%d=\n", __func__,scan_key_info->key_status);

	//input_event(scan_key_info->input,EV_KEY,KEY_F16,!!scan_key_info->key_status);
	//input_sync(scan_key_info->input);

	input_report_key(scan_key_info->input, KEY_F16, !!scan_key_info->key_status);
	input_sync(scan_key_info->input);

	if(scan_key_info->key_status)
		irq_set_irq_type(scan_key_info->irq, IRQ_TYPE_LEVEL_HIGH);
	else
		irq_set_irq_type(scan_key_info->irq, IRQ_TYPE_LEVEL_LOW);

	gpio_set_debounce(scan_key_info->key_gpio, scan_key_info->key_debounce);	

	enable_irq(scan_key_info->irq);
}


static irqreturn_t scan_key_irq_handler(void)
{
	SCAN_KEY_ERR("%s\n", __func__);

	disable_irq_nosync(scan_key_info->irq);
	queue_work(scan_key_workqueue, &scan_key_work);	

	return IRQ_HANDLED;
}

static int scan_key_register_irq(void)
{
	int ret = 0;
	struct device_node *np = NULL;
	u32 ints[2] = { 0, 0 };
	u32 ints1[2] = { 0, 0 };

	//np = of_find_matching_node(np,scan_key_of_match);
	np = of_find_compatible_node(NULL, NULL, "mediatek,scan_key-eint");

	if (np) {
		of_property_read_u32_array(np, "debounce", ints, ARRAY_SIZE(ints));
		of_property_read_u32_array(np, "interrupts", ints1, ARRAY_SIZE(ints1));
		//scan_key_info->key_gpio = ints[0];
		scan_key_info->key_debounce = ints[1];
		scan_key_info->key_eint_type = ints1[1];
		gpio_set_debounce(scan_key_info->key_gpio, scan_key_info->key_debounce);
		pinctrl_select_state(scankeyctrl, scan_key_as_int);
		scan_key_info->irq = irq_of_parse_and_map(np, 0);
		ret = request_irq(scan_key_info->irq, (irq_handler_t)scan_key_irq_handler, IRQF_TRIGGER_NONE, scan_key_info->eint_name, NULL);
		if (ret != 0) {
			printk("[dc]EINT IRQ LINE NOT AVAILABLE\n");
			goto err_request_irq;
		} else {
			printk("[scankey]scankey set EINT finished, irq=%d, key_gpio=%d, >key_debounce=%d, key_eint_type=%d\n",
				     scan_key_info->irq, scan_key_info->key_gpio, scan_key_info->key_debounce, scan_key_info->key_eint_type);
		}
	} else {
			ret = -ENODEV;
		goto err_nodev;
		printk("[scankey]%s can't find compatible node\n", __func__);
	}

	return 0;

err_nodev:
err_request_irq:
	return ret;
}

static int scan_key_input_dev_register()
{
	int ret = 0;

	scan_key_info->input = input_allocate_device();
	if (scan_key_info->input  == NULL) {
		ret = -ENOMEM;
		printk( "%s: failed to allocate input device\n", __func__);
		return ret;
	}	
	scan_key_info->input->name = "scan-key";
	//scan_key_info->input->phys ="scan_key/input0";
    scan_key_info->input->id.bustype = BUS_HOST;
	scan_key_info->input->dev.parent = scan_key_info->dev;
	input_set_drvdata(scan_key_info->input, scan_key_info);

	__set_bit(EV_KEY, scan_key_info->input->evbit);
	__set_bit(EV_SYN, scan_key_info->input->evbit);
	
	__set_bit(KEY_F16, scan_key_info->input->keybit);
	input_set_capability(scan_key_info->input, EV_KEY, KEY_F16);
	

	
	ret = input_register_device(scan_key_info->input);
	if (ret) {
		input_free_device(scan_key_info->input);
		printk("%s: failed to allocate input device\n", __func__);
		return ret;
	}
	return 0;

}
static int scan_key_probe(struct platform_device *pdev)
{
    int ret = 0;
    const struct of_device_id *id;
	struct device	*dev = &pdev->dev;

     SCAN_KEY_ERR("[%s]\n",__func__);
	 
	id = of_match_node(scan_key_of_match, pdev->dev.of_node);
	if (!id)
		return -ENODEV;

    scan_key_info = devm_kzalloc(&pdev->dev, sizeof(struct scan_key), GFP_KERNEL);
    if (!scan_key_info)
    	return -ENOMEM;
    
    platform_set_drvdata(pdev, scan_key_info);
	scan_key_info->dev = &pdev->dev;
	scan_key_info->eint_name = "scan_key-eint";
    scan_input_dev = scan_key_info->input;
	
    ret = scan_key_gpio_init(dev);
    if(ret < 0){
		printk("scan_key_gpio_init init fail\n");
		return ret;
    }	


	ret = scan_key_input_dev_register();
	if (ret < 0) {
		printk("%s input dev register failed\r\n", __func__);
		goto input_register_err;
	}

	scan_key_workqueue = create_singlethread_workqueue("scan_key");
	INIT_WORK(&scan_key_work, scan_key_handler);
	
    ret = scan_key_register_irq();
	if (ret < 0) {
		printk("%s input dev register failed\r\n", __func__);
		goto irq_register_err;
	}

    SCAN_KEY_ERR("scan_key_probe success\n");    
    return 0;
	
input_register_err:
irq_register_err:	
	return -1;

}

static int scan_key_remove(struct platform_device *pdev)
{
	scankeyctrl = NULL;
	return 0;
}


static int __init scan_key_init(void)
{
	int ret = 0;
	if (platform_driver_register(&scan_key_driver)) {
		SCAN_KEY_ERR("%s : failed to register scan_key driver\n", __func__);
		return -ENODEV;
	}

	return ret;
}

static void __exit scan_key_exit(void)
{
	platform_driver_unregister(&scan_key_driver);
}

module_init(scan_key_init);
module_exit(scan_key_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Scan Key Driver");
MODULE_AUTHOR("yongsheng.wang <wangyongsheng@szprize.com>");

