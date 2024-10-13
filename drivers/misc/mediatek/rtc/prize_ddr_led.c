#include "prize_ddr_led.h"

static struct pinctrl *prize_ddr_led_pinctrl;
static struct pinctrl_state *red_pins_low;
static struct pinctrl_state *red_pins_high;
static struct pinctrl_state *green_pins_low;
static struct pinctrl_state *green_pins_high;
atomic_t    atomic_ddr_sign;

//static struct wake_lock mts_monitor_wake_lock;
static struct mutex mts_monitor_mutex;
static struct task_struct *mts_thread_handle;
static void wake_up_mts_monitor(void)
{
	printk("lsw_ddr_led %s %d\n",__func__,__LINE__);
	if (mts_thread_handle != NULL) {
		//wake_lock(&mts_monitor_wake_lock);
		wake_up_process(mts_thread_handle);
	} else
		printk("lsw_ddr_led %s %d mts_thread_handle not ready\n",__func__,__LINE__);
}
int ddr_led_kthread(void *x)
{
	/* Run on a process content */
	int ddr_led_sign;
	while (1) {
		mutex_lock(&mts_monitor_mutex);

		ddr_led_sign = atomic_read(&atomic_ddr_sign);
		printk("lsw_ddr_led %s %d ddr_led_sign = %d\n",__func__,__LINE__,ddr_led_sign);
		while ( (255 == ddr_led_sign) || (0 == ddr_led_sign) ) {
			ddr_led_sign = atomic_read(&atomic_ddr_sign);
			printk("lsw_ddr_led %s %d ddr_led_sign = %d\n",__func__,__LINE__,ddr_led_sign);
			if( 255 == ddr_led_sign ){
				pinctrl_select_state(prize_ddr_led_pinctrl, green_pins_low);
				pinctrl_select_state(prize_ddr_led_pinctrl, red_pins_low);
				msleep(200);
				pinctrl_select_state(prize_ddr_led_pinctrl, green_pins_high);
				pinctrl_select_state(prize_ddr_led_pinctrl, red_pins_low);
				msleep(500);
				//printk("lsw_ddr_led %s %d ddr_led_sign = %d\n",__func__,__LINE__,ddr_led_sign);
			}else if( 0 == ddr_led_sign ){
				//pinctrl_select_state(prize_ddr_led_pinctrl, green_pins_low);
				//pinctrl_select_state(prize_ddr_led_pinctrl, red_pins_high);
				pinctrl_select_state(prize_ddr_led_pinctrl, green_pins_low);
				pinctrl_select_state(prize_ddr_led_pinctrl, red_pins_low);
				msleep(200);
				pinctrl_select_state(prize_ddr_led_pinctrl, green_pins_low);
				pinctrl_select_state(prize_ddr_led_pinctrl, red_pins_high);
				msleep(500);
				//printk("lsw_ddr_led %s %d ddr_led_sign = %d\n",__func__,__LINE__,ddr_led_sign);
				//break;
			}else{
				pinctrl_select_state(prize_ddr_led_pinctrl, green_pins_high);
				pinctrl_select_state(prize_ddr_led_pinctrl, red_pins_high);
				printk("lsw_ddr_led %s %d ddr_led_sign = %d\n",__func__,__LINE__,ddr_led_sign);
				break;
			}
		}
	
		mutex_unlock(&mts_monitor_mutex);
		//wake_unlock(&mts_monitor_wake_lock);

		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}

	return 0;
}
static void mts_thread_init(void)
{
	mts_thread_handle = kthread_create(ddr_led_kthread, (void *)NULL, "ddr_led_thread");
	if (IS_ERR(mts_thread_handle)) {
		mts_thread_handle = NULL;
		printk("lsw_test_ddr [adc_kthread] creation fails\n");
	} else {
		printk("lsw_test_ddr [adc_kthread] kthread_create Done\n");
	}
}

int prize_ddr_led_pass(void)
{
	int ddr_led_sign;

	ddr_led_sign = atomic_read(&atomic_ddr_sign);
	if(255 == ddr_led_sign){
		printk("lsw_ddr_led %s %d ddr_led_sign = %d thread already run\n",__func__,__LINE__,ddr_led_sign);
		atomic_set(&atomic_ddr_sign, 255);
	}else{
		atomic_set(&atomic_ddr_sign, 255);
		wake_up_mts_monitor();
	}
	return 0;
}

int prize_ddr_led_fail(void)
{
	
	atomic_set(&atomic_ddr_sign, 0);
	pinctrl_select_state(prize_ddr_led_pinctrl, green_pins_low);
	pinctrl_select_state(prize_ddr_led_pinctrl, red_pins_high);
	
	return 0;
}

int prize_ddr_led_high(void)
{
	atomic_set(&atomic_ddr_sign, 1);
	pinctrl_select_state(prize_ddr_led_pinctrl, green_pins_high);
	pinctrl_select_state(prize_ddr_led_pinctrl, red_pins_high);
	return 0;
}

int prize_ddr_led_low(void)
{
	atomic_set(&atomic_ddr_sign, 1);
	pinctrl_select_state(prize_ddr_led_pinctrl, green_pins_low);
	pinctrl_select_state(prize_ddr_led_pinctrl, red_pins_low);
	return 0;
}


extern int rtc_read_ddr_sign(void);

static int prize_ddr_led_probe(struct platform_device *platform_device)
{
	int ret = 0;
	struct pinctrl_state *pins_default = NULL;
	printk("lsw_ddr_led %s %d\n",__func__,__LINE__);
	prize_ddr_led_pinctrl = devm_pinctrl_get(&platform_device->dev);
	if (IS_ERR(prize_ddr_led_pinctrl)) {
		ret = PTR_ERR(prize_ddr_led_pinctrl);
		dev_notice(&platform_device->dev, "get prize_ddr_led_pinctrl fail.\n");
		return ret;
	}
	printk("lsw_ddr_led %s %d\n",__func__,__LINE__);
	pins_default = pinctrl_lookup_state(prize_ddr_led_pinctrl, "default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		dev_notice(&platform_device->dev, "lookup pins_default fail\n");
	}
	printk("lsw_ddr_led %s %d\n",__func__,__LINE__);
	red_pins_low = pinctrl_lookup_state(prize_ddr_led_pinctrl, "prize_ddr_red_pins_low");
	if (IS_ERR(red_pins_low)) {
		ret = PTR_ERR(red_pins_low);
		dev_notice(&platform_device->dev, "lookup red_pins_low fail\n");
		return ret;
	}
	
	red_pins_high = pinctrl_lookup_state(prize_ddr_led_pinctrl, "prize_ddr_red_pins_high");
	if (IS_ERR(red_pins_high)) {
		ret = PTR_ERR(red_pins_high);
		dev_notice(&platform_device->dev, "lookup red_pins_high fail\n");
		return ret;
	}
	
	green_pins_low = pinctrl_lookup_state(prize_ddr_led_pinctrl, "prize_ddr_green_pins_low");
	if (IS_ERR(green_pins_low)) {
		ret = PTR_ERR(green_pins_low);
		dev_notice(&platform_device->dev, "lookup green_pins_low fail\n");
		return ret;
	}
	
	green_pins_high = pinctrl_lookup_state(prize_ddr_led_pinctrl, "prize_ddr_green_pins_high");
	if (IS_ERR(green_pins_high)) {
		ret = PTR_ERR(green_pins_high);
		dev_notice(&platform_device->dev, "lookup green_pins_high fail\n");
		return ret;
	}
	
	if(  rtc_read_ddr_sign() == 15 )
	{
		pinctrl_select_state(prize_ddr_led_pinctrl, red_pins_high);
		pinctrl_select_state(prize_ddr_led_pinctrl, green_pins_high);
	}
	else
	{
		pinctrl_select_state(prize_ddr_led_pinctrl, red_pins_high);
		pinctrl_select_state(prize_ddr_led_pinctrl, green_pins_low);
	}
	//prize_ddr_led_low();
	atomic_set(&atomic_ddr_sign, 1);
	mutex_init(&mts_monitor_mutex);
	mts_thread_init();
	printk("lsw_ddr_led %s %d\n",__func__,__LINE__);
	return 0;
}

static int prize_ddr_led_remove(struct platform_device *pdev)
{
	return 0;
}

static int prize_ddr_led_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int prize_ddr_led_resume(struct platform_device *pdev)
{
	return 0;
}

/*
 * platform driver
 */

#ifdef CONFIG_OF
static const struct of_device_id prize_ddr_led_of_device_id[] = {
	{ .compatible = "mediatek,prize_ddr_led", },
	{}
};
#endif

static struct platform_driver prize_ddr_led_platform_driver = {
	.probe      = prize_ddr_led_probe,
	.remove     = prize_ddr_led_remove,
	.suspend    = prize_ddr_led_suspend,
	.resume     = prize_ddr_led_resume,
	.driver     = {
		.name   = "prize_ddr_led",
		.owner  = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = prize_ddr_led_of_device_id,
#endif
	}
};

/*
 * prize_ddr_led_init()
 */
static int __init prize_ddr_led_init(void)
{
	printk("lsw_ddr_led %s %d\n",__func__,__LINE__);
	if (platform_driver_register(&prize_ddr_led_platform_driver)) {
		pr_err("failed to register lsw_ddr_led driver\n");
		return -ENODEV;
	}
	return 0;
}

/*
 * prize_ddr_led_exit()
 */
static void __exit prize_ddr_led_exit(void)
{
	platform_driver_unregister(&prize_ddr_led_platform_driver);
}


module_init(prize_ddr_led_init);
module_exit(prize_ddr_led_exit);

MODULE_DESCRIPTION("prize ddr led driver");
MODULE_AUTHOR("Prize");
MODULE_LICENSE("GPL v2");