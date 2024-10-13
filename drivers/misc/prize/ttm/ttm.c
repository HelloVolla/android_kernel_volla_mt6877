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




#define TTM_DEVNAME "ttm_dev"


static int ttm_remove(struct platform_device *dev);
static int ttm_probe(struct platform_device *pdev);
static void ttm_shutdown(struct platform_device *dev);
//void ttm_rfid_pwr(u8 enable);

static const struct of_device_id ttm_of_match[] = {
	{.compatible = "mediatek,ttm"},
	{},
};
MODULE_DEVICE_TABLE(of, ttm_of_match);

//prize add by lipengpeng 20210330 start 
static int air_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	printk("lpp ttm enter suspend\n");
	//ttm_enter_suspend(0);
        return 0;
}

static int air_resume(struct platform_device *pdev)
{
	printk("lpp ttm enter resume\n");
	//ttm_enter_resume(0);
        return 0;
}
//prize add by lipengpeng 20210330 end

static struct platform_driver ttm_platform_driver = {
	.probe = ttm_probe,
	.remove = ttm_remove,
	.shutdown = ttm_shutdown,
//prize add by lipengpeng 20210330 start 
	.suspend = air_suspend,
    .resume = air_resume,
//prize add by lipengpeng 20210330 end 
	.driver = {
		   .name = TTM_DEVNAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = ttm_of_match,
#endif
	},
};

/*----------------------------------------------------------------------------*/

#ifdef CONFIG_PINCTRL
static struct pinctrl *ttm_gpio;
static struct pinctrl_state *ttm_enable1_gpio0;
static struct pinctrl_state *ttm_enable1_gpio1;
static struct pinctrl_state *ttm_enable2_gpio0;
static struct pinctrl_state *ttm_enable2_gpio1;
static struct pinctrl_state *ttm_enable3_gpio0;
static struct pinctrl_state *ttm_enable3_gpio1;
static struct pinctrl_state *ttm_enable4_gpio0;
static struct pinctrl_state *ttm_enable4_gpio1;
static struct pinctrl_state *ttm_enable5_gpio0;
static struct pinctrl_state *ttm_enable5_gpio1;
//drv add by lipengpeng 20230719 start 
static struct pinctrl_state *ttm_enable6_gpio0;
static struct pinctrl_state *ttm_enable6_gpio1;
static struct pinctrl_state *ttm_enable7_gpio0;
static struct pinctrl_state *ttm_enable7_gpio1;
//drv add by lipengpeng 20230719 end 
static struct pinctrl_state *ttm_rst_gpio0;
static struct pinctrl_state *ttm_rst_gpio1;
#ifdef CONFIG_PRIZE_VDD_FLASH
static struct pinctrl_state *ttm_vdd3_flash_gpio0;
static struct pinctrl_state *ttm_vdd3_flash_gpio1;
#endif
#if 1
static unsigned int gpio_enable1_status;
static unsigned int gpio_enable2_status;
static unsigned int gpio_enable3_status;
static unsigned int gpio_enable4_status;
static unsigned int gpio_enable5_status;
//drv add by lipengpeng 20230719 start 
static unsigned int gpio_enable6_status;
static unsigned int gpio_enable7_status;
//drv add by lipengpeng 20230719 end 
static unsigned int gpio_rst_status;
#ifdef CONFIG_PRIZE_VDD_FLASH
static unsigned int gpio_vdd3_flash_status;
#endif
#endif
#endif

static int ttm_gpio_init(struct device	*dev)
{    
	int ret=0;
//	unsigned int mode;
	//const struct of_device_id *match;

	pr_debug("[ttm][GPIO] enter %s, %d\n", __func__, __LINE__);

	ttm_gpio = devm_pinctrl_get(dev);
	if (IS_ERR(ttm_gpio)) {
		ret = PTR_ERR(ttm_gpio);
		pr_info("[ttm][ERROR] Cannot find ttm_gpio!\n");
		return ret;
	}
//enable1
	ttm_enable1_gpio0 = pinctrl_lookup_state(ttm_gpio, "ttm_enable1_gpio0");
	if (IS_ERR(ttm_enable1_gpio0)) {
		ret = PTR_ERR(ttm_enable1_gpio0);
		pr_info("[ttm][ERROR] Cannot find ttm_enable1_gpio0 %d!\n",
			ret);
	}
	ttm_enable1_gpio1= pinctrl_lookup_state(ttm_gpio, "ttm_enable1_gpio1");
	if (IS_ERR(ttm_enable1_gpio1)) {
		ret = PTR_ERR(ttm_enable1_gpio1);
		pr_info("[ttm][ERROR] Cannot find ttm_enable1_gpio1 %d!\n",
			ret);
	}
//enable2	
	ttm_enable2_gpio0 = pinctrl_lookup_state(ttm_gpio, "ttm_enable2_gpio0");
	if (IS_ERR(ttm_enable2_gpio0)) {
		ret = PTR_ERR(ttm_enable2_gpio0);
		pr_info("[ttm][ERROR] Cannot find ttm_enable2_gpio0 %d!\n",
			ret);
	}
	ttm_enable2_gpio1= pinctrl_lookup_state(ttm_gpio, "ttm_enable2_gpio1");
	if (IS_ERR(ttm_enable2_gpio1)) {
		ret = PTR_ERR(ttm_enable2_gpio1);
		pr_info("[ttm][ERROR] Cannot find ttm_enable2_gpio1 %d!\n",
			ret);
	}
//enable3	
 	ttm_enable3_gpio0 = pinctrl_lookup_state(ttm_gpio, "ttm_enable3_gpio0");
	if (IS_ERR(ttm_enable3_gpio0)) {
		ret = PTR_ERR(ttm_enable3_gpio0);
		pr_info("[ttm][ERROR] Cannot find ttm_enable3_gpio0 %d!\n",
			ret);
	}
	ttm_enable3_gpio1= pinctrl_lookup_state(ttm_gpio, "ttm_enable3_gpio1");
	if (IS_ERR(ttm_enable3_gpio1)) {
		ret = PTR_ERR(ttm_enable3_gpio1);
		pr_info("[ttm][ERROR] Cannot find ttm_enable3_gpio1 %d!\n",
			ret);
	}
//enable4	
	ttm_enable4_gpio0 = pinctrl_lookup_state(ttm_gpio, "ttm_enable4_gpio0");
	if (IS_ERR(ttm_enable4_gpio0)) {
		ret = PTR_ERR(ttm_enable4_gpio0);
		pr_info("[ttm][ERROR] Cannot find ttm_enable4_gpio0 %d!\n",
			ret);
	}
	ttm_enable4_gpio1= pinctrl_lookup_state(ttm_gpio, "ttm_enable4_gpio1");
	if (IS_ERR(ttm_enable4_gpio1)) {
		ret = PTR_ERR(ttm_enable4_gpio1);
		pr_info("[ttm][ERROR] Cannot find ttm_enable4_gpio1 %d!\n",
			ret);
	}
//enable5
	ttm_enable5_gpio0 = pinctrl_lookup_state(ttm_gpio, "ttm_enable5_gpio0");
	if (IS_ERR(ttm_enable5_gpio0)) {
		ret = PTR_ERR(ttm_enable5_gpio0);
		pr_info("[ttm][ERROR] Cannot find ttm_enable5_gpio0 %d!\n",
			ret);
	}
	ttm_enable5_gpio1= pinctrl_lookup_state(ttm_gpio, "ttm_enable5_gpio1");
	if (IS_ERR(ttm_enable5_gpio1)) {
		ret = PTR_ERR(ttm_enable5_gpio1);
		pr_info("[ttm][ERROR] Cannot find ttm_enable5_gpio1 %d!\n",
			ret);
	}
//drv add by lipengpeng 20230719 start 
//enable6
	ttm_enable6_gpio0 = pinctrl_lookup_state(ttm_gpio, "ttm_enable6_gpio0");
	if (IS_ERR(ttm_enable6_gpio0)) {
		ret = PTR_ERR(ttm_enable6_gpio0);
		pr_info("[ttm][ERROR] Cannot find ttm_enable6_gpio0 %d!\n",
			ret);
	}
	ttm_enable6_gpio1= pinctrl_lookup_state(ttm_gpio, "ttm_enable6_gpio1");
	if (IS_ERR(ttm_enable6_gpio1)) {
		ret = PTR_ERR(ttm_enable6_gpio1);
		pr_info("[ttm][ERROR] Cannot find ttm_enable6_gpio1 %d!\n",
			ret);
	}
//enable7
	ttm_enable7_gpio0 = pinctrl_lookup_state(ttm_gpio, "ttm_enable7_gpio0");
	if (IS_ERR(ttm_enable7_gpio0)) {
		ret = PTR_ERR(ttm_enable7_gpio0);
		pr_info("[ttm][ERROR] Cannot find ttm_enable7_gpio0 %d!\n",
			ret);
	}
	ttm_enable7_gpio1= pinctrl_lookup_state(ttm_gpio, "ttm_enable7_gpio1");
	if (IS_ERR(ttm_enable7_gpio1)) {
		ret = PTR_ERR(ttm_enable7_gpio1);
		pr_info("[ttm][ERROR] Cannot find ttm_enable7_gpio1 %d!\n",
			ret);
	}
//drv add by lipengpeng 20230719 end	
//rst	
	ttm_rst_gpio0 = pinctrl_lookup_state(ttm_gpio, "ttm_rst_gpio0");
	if (IS_ERR(ttm_rst_gpio0)) {
		ret = PTR_ERR(ttm_rst_gpio0);
		pr_info("[ttm][ERROR] Cannot find ttm_rst_gpio0 %d!\n",
			ret);
	}
	ttm_rst_gpio1= pinctrl_lookup_state(ttm_gpio, "ttm_rst_gpio1");
	if (IS_ERR(ttm_rst_gpio1)) {
		ret = PTR_ERR(ttm_rst_gpio1);
		pr_info("[ttm][ERROR] Cannot find ttm_rst_gpio1 %d!\n",
			ret);
	}
//vdd3_flash	
#ifdef CONFIG_PRIZE_VDD_FLASH
	ttm_vdd3_flash_gpio0 = pinctrl_lookup_state(ttm_gpio, "ttm_vdd3_flash_gpio0");
	if (IS_ERR(ttm_vdd3_flash_gpio0)) {
		ret = PTR_ERR(ttm_vdd3_flash_gpio0);
		pr_info("[ttm][ERROR] Cannot find ttm_vdd3_flash_gpio0 %d!\n",
			ret);
	}
	ttm_vdd3_flash_gpio1= pinctrl_lookup_state(ttm_gpio, "ttm_vdd3_flash_gpio1");
	if (IS_ERR(ttm_vdd3_flash_gpio1)) {
		ret = PTR_ERR(ttm_vdd3_flash_gpio1);
		pr_info("[ttm][ERROR] Cannot find ttm_vdd3_flash_gpio1 %d!\n",
			ret);
	}
#endif
	
	gpio_enable1_status =of_get_named_gpio(dev->of_node, "gpio_enable1_status", 0);
	gpio_enable2_status =of_get_named_gpio(dev->of_node, "gpio_enable2_status", 0);
	gpio_enable3_status =of_get_named_gpio(dev->of_node, "gpio_enable3_status", 0);
	gpio_enable4_status =of_get_named_gpio(dev->of_node, "gpio_enable4_status", 0);
	gpio_enable5_status =of_get_named_gpio(dev->of_node, "gpio_enable5_status", 0);
//drv add by lipengpeng 20230719 start 
	gpio_enable6_status =of_get_named_gpio(dev->of_node, "gpio_enable6_status", 0);
	gpio_enable7_status =of_get_named_gpio(dev->of_node, "gpio_enable7_status", 0);
//drv add by lipengpeng 20230719 end 	
	gpio_rst_status =of_get_named_gpio(dev->of_node, "gpio_rst_status", 0);
#ifdef CONFIG_PRIZE_VDD_FLASH
	gpio_vdd3_flash_status =of_get_named_gpio(dev->of_node, "gpio_vdd3_flash_status", 0);

#endif

	printk("[ttm][GPIO] ttm_gpio_get_info end!\n");

    return ret;

}


//prize add by lipengpeng 20220302 start 
void ttm_enable7(u8 enable)
{
    printk("%s enable =%d\n", __func__);

    if(enable)
        pinctrl_select_state(ttm_gpio,ttm_enable7_gpio1);
    else
        pinctrl_select_state(ttm_gpio,ttm_enable7_gpio0);
 
}

int get_ttm_enable7_status(void)
{

   int enable7_status = 0;
   return enable7_status = __gpio_get_value(gpio_enable7_status);
   printk("%s enable7_status =%d\n", enable7_status); 
   return enable7_status;
}



void ttm_enable6(u8 enable)
{
    printk("%s enable =%d\n", __func__);

    if(enable)
        pinctrl_select_state(ttm_gpio,ttm_enable6_gpio1);
    else
        pinctrl_select_state(ttm_gpio,ttm_enable6_gpio0);
 
}

int get_ttm_enable6_status(void)
{

   int enable6_status = 0;
   return enable6_status = __gpio_get_value(gpio_enable6_status);
   printk("%s enable6_status =%d\n", enable6_status); 
   return enable6_status;
}
//prize add by lipengpeng 20220302 end 



void ttm_enable5(u8 enable)
{
    printk("%s enable =%d\n", __func__);

    if(enable)
        pinctrl_select_state(ttm_gpio,ttm_enable5_gpio1);
    else
        pinctrl_select_state(ttm_gpio,ttm_enable5_gpio0);
 
}
//prize add by lipengpeng 20220302 start 
int get_ttm_enable5_status(void)
{

   int enable5_status = 0;
   return enable5_status = __gpio_get_value(gpio_enable5_status);
   printk("%s enable5_status =%d\n", enable5_status); 
   return enable5_status;
}
//prize add by lipengpeng 20220302 end 
void ttm_enable4(u8 enable)
{
    printk("%s enable =%d\n", __func__);

    if(enable)
        pinctrl_select_state(ttm_gpio,ttm_enable4_gpio1);
    else
        pinctrl_select_state(ttm_gpio,ttm_enable4_gpio0);
 
}
//prize add by lipengpeng 20220302 start 
int get_ttm_enable4_status(void)
{

   int enable4_status = 0;
   return enable4_status = __gpio_get_value(gpio_enable4_status);
   printk("%s enable4_status =%d\n", enable4_status); 
   return enable4_status;
}
//prize add by lipengpeng 20220302 start 
void ttm_enable3(u8 enable)
{
    pr_debug("%s enable =%d\n", __func__);

    if(enable)
        pinctrl_select_state(ttm_gpio,ttm_enable3_gpio1);
    else
        pinctrl_select_state(ttm_gpio,ttm_enable3_gpio0);
        
}
//prize add by lipengpeng 20220302 start 
int get_ttm_enable3_status(void)
{

   int enable3_status = 0;
   return enable3_status = __gpio_get_value(gpio_enable3_status);
   printk("%s enable3_status =%d\n", enable3_status); 
   return enable3_status;
}
//prize add by lipengpeng 20220302 start 
void ttm_enable2(u8 enable)
{
    pr_debug("%s enable =%d\n", __func__);

    if(enable)
        pinctrl_select_state(ttm_gpio,ttm_enable2_gpio1);
    else
        pinctrl_select_state(ttm_gpio,ttm_enable2_gpio0);
        
}
//prize add by lipengpeng 20220302 start 
int get_ttm_enable2_status(void)
{

   int enable2_status = 0;
   return enable2_status = __gpio_get_value(gpio_enable2_status);
   printk("%s enable2_status =%d\n", enable2_status); 
   return enable2_status;
}
//prize add by lipengpeng 20220302 start 
void ttm_enable1(u8 enable)
{
    pr_debug("%s enable =%d\n", __func__);

    if(enable)
        pinctrl_select_state(ttm_gpio,ttm_enable1_gpio1);
    else
        pinctrl_select_state(ttm_gpio,ttm_enable1_gpio0);

}
//prize add by lipengpeng 20220302 start 
int get_ttm_enable1_status(void)
{

   int enable1_status = 0;
   return enable1_status = __gpio_get_value(gpio_enable1_status);
   printk("%s enable1_status =%d\n", enable1_status); 
   return enable1_status;
}
//prize add by lipengpeng 20220302 start 
void ttm_rst(u8 enable)
{
    pr_debug("%s enable =%d\n", __func__);

    if(enable)
        pinctrl_select_state(ttm_gpio,ttm_rst_gpio1);
    else
        pinctrl_select_state(ttm_gpio,ttm_rst_gpio0);

}
#ifdef CONFIG_PRIZE_VDD_FLASH
void ttm_vdd3_flash(u8 enable)
{
    pr_debug("%s enable =%d\n", __func__);

    if(enable)
        pinctrl_select_state(ttm_gpio,ttm_vdd3_flash_gpio1);
    else
        pinctrl_select_state(ttm_gpio,ttm_vdd3_flash_gpio0);

}
#endif

//drv add by lipengpeng 20230719 start 
/***************************************
node:/sys/bus/platform/drivers/ttm_dev/ttm_enable7
1:enable
0:disable
***********************************/
static ssize_t ttm_enable7_show(struct device_driver *ddri, char *buf)
{
	int ttm_enable7_value = 0;
    ssize_t res;


    ttm_enable7_value = __gpio_get_value(gpio_enable7_status);
    
	res = snprintf(buf, PAGE_SIZE, "ttm_enable7_value = %d\n",ttm_enable7_value);

	return res;
}

static ssize_t ttm_enable7_store(struct device_driver *ddri,
				      const char *buf, size_t tCount)
{

    int ttm_enable7_flag;
	int ret = 0;

	if (strlen(buf) < 1) {
		pr_notice("%s() Invalid input!!\n", __func__);
		return -EINVAL;
	}

    ret = sscanf(buf, "%d", &ttm_enable7_flag);


	if (ttm_enable7_flag == 1){
		ttm_enable7(1);
     }
	else{
		ttm_enable7(0);
    }
   
	return tCount;
} 



/***************************************
node:/sys/bus/platform/drivers/ttm_dev/ttm_enable6
1:enable
0:disable
***********************************/
static ssize_t ttm_enable6_show(struct device_driver *ddri, char *buf)
{
	int ttm_enable6_value = 0;
    ssize_t res;


    ttm_enable6_value = __gpio_get_value(gpio_enable6_status);
    
	res = snprintf(buf, PAGE_SIZE, "ttm_enable6_value = %d\n",ttm_enable6_value);

	return res;
}

static ssize_t ttm_enable6_store(struct device_driver *ddri,
				      const char *buf, size_t tCount)
{

    int ttm_enable6_flag;
	int ret = 0;

	if (strlen(buf) < 1) {
		pr_notice("%s() Invalid input!!\n", __func__);
		return -EINVAL;
	}

    ret = sscanf(buf, "%d", &ttm_enable6_flag);


	if (ttm_enable6_flag == 1){
		ttm_enable6(1);
     }
	else{
		ttm_enable6(0);
    }
   
	return tCount;
} 


//drv add by lipengpeng 20230719 end 
/***************************************
node:/sys/bus/platform/drivers/ttm_dev/ttm_enable5
1:enable
0:disable
***********************************/
static ssize_t ttm_enable5_show(struct device_driver *ddri, char *buf)
{
	int ttm_enable5_value = 0;
    ssize_t res;


    ttm_enable5_value = __gpio_get_value(gpio_enable5_status);
    
	res = snprintf(buf, PAGE_SIZE, "ttm_enable5_value = %d\n",ttm_enable5_value);

	return res;
}

static ssize_t ttm_enable5_store(struct device_driver *ddri,
				      const char *buf, size_t tCount)
{

    int ttm_enable5_flag;
	int ret = 0;

	if (strlen(buf) < 1) {
		pr_notice("%s() Invalid input!!\n", __func__);
		return -EINVAL;
	}

    ret = sscanf(buf, "%d", &ttm_enable5_flag);


	if (ttm_enable5_flag == 1){
		ttm_enable5(1);
     }
	else{
		ttm_enable5(0);
    }
   
	return tCount;
} 
/***************************************
node:/sys/bus/platform/drivers/ttm_dev/ttm_enable4
1:enable
0:disable
***********************************/
static ssize_t ttm_enable4_show(struct device_driver *ddri, char *buf)
{
	int ttm_enable4_value = 0;
    ssize_t res;


    ttm_enable4_value = __gpio_get_value(gpio_enable4_status);
    
	res = snprintf(buf, PAGE_SIZE, "ttm_enable4_value = %d\n",ttm_enable4_value);

	return res;
}

static ssize_t ttm_enable4_store(struct device_driver *ddri,
				      const char *buf, size_t tCount)
{

    int ttm_enable4_flag;
	int ret = 0;

	if (strlen(buf) < 1) {
		pr_notice("%s() Invalid input!!\n", __func__);
		return -EINVAL;
	}

    ret = sscanf(buf, "%d", &ttm_enable4_flag);


	if (ttm_enable4_flag == 1){
		ttm_enable4(1);
     }
	else{
		ttm_enable4(0);
    }
   
	return tCount;
} 

/***************************************
node:/sys/bus/platform/drivers/ttm_dev/ttm_enable3
1:enable
0:disable
***********************************/
static ssize_t ttm_enable3_show(struct device_driver *ddri, char *buf)
{
	int ttm_enable3_value = 0;
    ssize_t res;


    ttm_enable3_value = __gpio_get_value(gpio_enable3_status);
    
	res = snprintf(buf, PAGE_SIZE, "ttm_enable3_value = %d\n",ttm_enable3_value);

	return res;
}

static ssize_t ttm_enable3_store(struct device_driver *ddri,
				      const char *buf, size_t tCount)
{

    int ttm_enable3_flag;
	int ret = 0;

	if (strlen(buf) < 1) {
		pr_notice("%s() Invalid input!!\n", __func__);
		return -EINVAL;
	}

    ret = sscanf(buf, "%d", &ttm_enable3_flag);


	if (ttm_enable3_flag == 1){
		ttm_enable3(1);
     }
	else{
		ttm_enable3(0);
    }
   
	return tCount;
}  
/***************************************
node:/sys/bus/platform/drivers/ttm_dev/ttm_enable2
1:enable
0:disable
***********************************/                      
static ssize_t ttm_enable2_show(struct device_driver *ddri, char *buf)
{
  int ttm_enable2_value = 0;
  ssize_t res;

  ttm_enable2_value = __gpio_get_value(gpio_enable2_status);

  res = snprintf(buf, PAGE_SIZE, "ttm_enable2_value = %d\n",ttm_enable2_value);

  return res;
}

static ssize_t ttm_enable2_store(struct device_driver *ddri,
                    const char *buf, size_t tCount)
{
  int ttm_enable2_flag;
  int ret = 0;

  if (strlen(buf) < 1) {
      pr_notice("%s() Invalid input!!\n", __func__);
      return -EINVAL;
  }

  ret = sscanf(buf, "%d", &ttm_enable2_flag);


  if (ttm_enable2_flag == 1){
      ttm_enable2(1);

   }else{
      ttm_enable2(0);
  }
 
  return tCount;
}   
/***************************************
node:/sys/bus/platform/drivers/ttm_dev/ttm_enable1
1:enable
0:disable
***********************************/ 
static ssize_t ttm_enable1_show(struct device_driver *ddri, char *buf)
{
  int ttm_enable1_value = 0;
  ssize_t res;

  ttm_enable1_value = __gpio_get_value(gpio_enable1_status);

  res = snprintf(buf, PAGE_SIZE, "ttm_enable1_value = %d\n",ttm_enable1_value);

  return res;
}

static ssize_t ttm_enable1_store(struct device_driver *ddri,
                    const char *buf, size_t tCount)
{

  int ttm_enable1_flag;
  int ret = 0;

  if (strlen(buf) < 1) {
      pr_notice("%s() Invalid input!!\n", __func__);
      return -EINVAL;
  }

  ret = sscanf(buf, "%d", &ttm_enable1_flag);


  if (ttm_enable1_flag == 1){
      ttm_enable1(1);

   }else{
      ttm_enable1(0);
  }
 
  return tCount;
}   

/***************************************
node:/sys/bus/platform/drivers/ttm_dev/ttm_rst
1:enable
0:disable
***********************************/ 
static ssize_t ttm_rst_show(struct device_driver *ddri, char *buf)
{
  int ttm_rst_value = 0;
  ssize_t res;

  ttm_rst_value = __gpio_get_value(gpio_rst_status);

  res = snprintf(buf, PAGE_SIZE, "ttm_rst_value = %d\n",ttm_rst_value);

  return res;
}

static ssize_t ttm_rst_store(struct device_driver *ddri,
                    const char *buf, size_t tCount)
{

  int ttm_rst_flag;
  int ret = 0;

  if (strlen(buf) < 1) {
      pr_notice("%s() Invalid input!!\n", __func__);
      return -EINVAL;
  }

  ret = sscanf(buf, "%d", &ttm_rst_flag);


  if (ttm_rst_flag == 1){
      ttm_rst(1);

   }else{
      ttm_rst(0);
  }
 
  return tCount;
} 

/***************************************
node:/sys/bus/platform/drivers/ttm_dev/ttm_vdd3_flash
1:enable
0:disable
***********************************/ 
#ifdef CONFIG_PRIZE_VDD_FLASH
static ssize_t ttm_vdd3_flash_show(struct device_driver *ddri, char *buf)
{
  int ttm_vdd3_flash_value = 0;
  ssize_t res;

  ttm_vdd3_flash_value = __gpio_get_value(gpio_vdd3_flash_status);

  res = snprintf(buf, PAGE_SIZE, "ttm_vdd3_flash_value = %d\n",ttm_vdd3_flash_value);

  return res;
}

static ssize_t ttm_vdd3_flash_store(struct device_driver *ddri,
                    const char *buf, size_t tCount)
{

  int ttm_vdd3_flash_flag;
  int ret = 0;

  if (strlen(buf) < 1) {
      pr_notice("%s() Invalid input!!\n", __func__);
      return -EINVAL;
  }

  ret = sscanf(buf, "%d", &ttm_vdd3_flash_flag);


  if (ttm_vdd3_flash_flag == 1){
      ttm_vdd3_flash(1);

   }else{
      ttm_vdd3_flash(0);
  }
 
  return tCount;
} 

#endif

/*----------------------------------------------------------------------------*/
#if 0                   
static DRIVER_ATTR(ttm_enable1, 0644, ttm_3V_funen_show,ttm_3V_funen_store);
static DRIVER_ATTR(ttm_enable2, 0644, ttm_5V_boost_show,ttm_5V_boost_store);
static DRIVER_ATTR(ttm_enable3, 0644, ttm_rfid_tof_show,ttm_tof_pwr_store);
#else
static DRIVER_ATTR_RW(ttm_enable1);
static DRIVER_ATTR_RW(ttm_enable2);
static DRIVER_ATTR_RW(ttm_enable3);
static DRIVER_ATTR_RW(ttm_enable4);
static DRIVER_ATTR_RW(ttm_enable5);
//drv add by lipengpeng 20230719 start 
static DRIVER_ATTR_RW(ttm_enable6);
static DRIVER_ATTR_RW(ttm_enable7);
//drv add by lipengpeng 20230719 end 
static DRIVER_ATTR_RW(ttm_rst);
#ifdef CONFIG_PRIZE_VDD_FLASH
static DRIVER_ATTR_RW(ttm_vdd3_flash);
#endif

#endif

/*----------------------------------------------------------------------------*/
static struct driver_attribute *ttm_attr_list[] = {    
	&driver_attr_ttm_enable1,
	&driver_attr_ttm_enable2, 
	&driver_attr_ttm_enable3,
	&driver_attr_ttm_enable4,
	&driver_attr_ttm_enable5,
//drv add by lipengpeng 20230719 start
	&driver_attr_ttm_enable6,
	&driver_attr_ttm_enable7,
//drv add by lipengpeng 20230719 end
	&driver_attr_ttm_rst,
#ifdef CONFIG_PRIZE_VDD_FLASH
	&driver_attr_ttm_vdd3_flash,
#endif
};


/*----------------------------------------------------------------------------*/
static int ttm_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)ARRAY_SIZE(ttm_attr_list);

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, ttm_attr_list[idx]);
		if (err) {
			pr_err("driver_create_file (%s) = %d\n",
				   ttm_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int ttm_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)ARRAY_SIZE(ttm_attr_list);

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, ttm_attr_list[idx]);

	return err;
}

static int ttm_probe(struct platform_device *pdev)
{

    const struct of_device_id *id;
	struct device	*dev = &pdev->dev;
    int err =0 ;
  	printk("ttm_probe start\n");
        
	id = of_match_node(ttm_of_match, pdev->dev.of_node);
	if (!id)
		return -ENODEV;
    ttm_gpio_init(dev);

    /* Register sysfs attribute */
	err = ttm_create_attr(&ttm_platform_driver.driver);
	if (err) {
		pr_err("create attribute err = %d\n", err);
		goto exit_sysfs_create_group_failed;
	}
    
    //ttmprt_enable(1);

	printk("ttm_probe done\n");

	return 0;
exit_sysfs_create_group_failed:
    return -1;

}

static int ttm_remove(struct platform_device *dev)
{
    int err = 0;
	err = ttm_delete_attr(&ttm_platform_driver.driver);
	if (err)
		pr_err("ttm_delete_attr fail: %d\n", err);

	return err;
}

static void ttm_shutdown(struct platform_device *dev)
{

}


static int __init ttm_init(void)
{
	int ret;

	pr_debug("Init start\n");    

	ret = platform_driver_register(&ttm_platform_driver);
	if (ret) {
		pr_err("Failed to register platform driver\n");
		return ret;
	}

	pr_debug("Init done\n");

	return 0;
}

static void __exit ttm_exit(void)
{
	pr_debug("Exit start\n");

	platform_driver_unregister(&ttm_platform_driver);

	pr_debug("Exit done\n");
}

module_init(ttm_init);
module_exit(ttm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("pengpeng li <lipengpeng@szprize.com>");
MODULE_DESCRIPTION("MTK ttm Core Driver");

