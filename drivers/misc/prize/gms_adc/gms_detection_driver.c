#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
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
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/kthread.h>
#include <linux/input.h>
#if defined(CONFIG_PM_WAKELOCKS)
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif
#include <linux/time.h>

#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/input.h>

#include <linux/of.h>
#include <linux/iio/consumer.h>
#include <linux/iio/iio.h>

/*----------------------------------------------------------------------
static variable defination
----------------------------------------------------------------------*/
//#define gms_detection_DEVNAME    "gms_detection_dev"

#define EN_DEBUG

#if defined(EN_DEBUG)
		
#define TRACE_FUNC 	printk("[gms_detection_dev] function: %s, line: %d \n", __func__, __LINE__);

#define gms_detection_DEBUG  printk
#else

#define TRACE_FUNC(x,...)

#define gms_detection_DEBUG(x,...)
#endif

struct iio_channel *air_channel = NULL;
int gms_detection_getadc_v(void);
int gms_detection_getadc(void);

//prize add by lipengpeng 20210229 start 
int gms_detection_getadc_v(void){
	
	int ret = 0;
	int val = 0;
	
	if (!IS_ERR_OR_NULL(air_channel)){
		ret = iio_read_channel_processed(air_channel, &val);
		if (ret < 0) {
			printk("%s:Busy/Timeout, IIO ch read failed %d\n", __func__, ret);
			return ret;
		}
         printk("lpp-----get gms detect vol xx=%d\n", val);
		/*val * 1500 / 4096*/
		///ret = (val * 1450) >> 12;  //max 1.45V
	}
	return val;
}
EXPORT_SYMBOL_GPL(gms_detection_getadc_v);

int gms_detection_getadc(void){
	
	int ret = 0;
	int val = 0;
	
	if (!IS_ERR_OR_NULL(air_channel)){
		ret = iio_read_channel_processed(air_channel, &val);
		if (ret < 0) {
			printk("%s:Busy/Timeout, IIO ch read failed %d\n", __func__, ret);
			return ret;
		}
		//1764  0.64 
         printk("lpp-----get gms detect vol=%d\n", val);
	//	ret = (val * 1450) >> 12;  //max 1.45V
	}
	return 1800-val;
}
EXPORT_SYMBOL_GPL(gms_detection_getadc);

static int gms_detection_probe(struct platform_device *pdev)
{
   int ret = 0;
   printk("gms_detection_probe start\n");
	   
	air_channel = iio_channel_get(&pdev->dev, "air-ch");
	if (IS_ERR(air_channel)) {
		ret = PTR_ERR(air_channel);
		printk("[%s] lpp fail to get auxadc iio ch5: %d, %p\n", __func__, ret, air_channel);
		return ret;
	}
	printk("gms_detection_probe end\n");
	return 0;
}

static int gms_detection_remove(struct platform_device *dev)	
{
	gms_detection_DEBUG("[gms_detection_dev]:gms_detection_remove start!\n");
	gms_detection_DEBUG("[gms_detection_dev]:gms_detection_remove end!\n");
	return 0;
}
static const struct of_device_id gms_detection_dt_match[] = {
	{.compatible = "prize,gms_detection"},
	{},
};

static struct platform_driver gms_detection_driver = {
	.probe	= gms_detection_probe,
	.remove  = gms_detection_remove,
	.driver    = {
		.name       = "gms_detection_driver",
		.of_match_table = of_match_ptr(gms_detection_dt_match),
	},
};

static int __init gms_detection_init(void)
{
    int retval = 0;
    printk("gms_detection_init, retval=%d \n!",retval);
	  if (retval != 0) {
		  return retval;
	  }
    platform_driver_register(&gms_detection_driver);
    return 0;
}

static void __exit gms_detection_exit(void)
{
    printk("gms_detection_exit start\n");
    platform_driver_unregister(&gms_detection_driver);
}

module_init(gms_detection_init);
module_exit(gms_detection_exit);
MODULE_DESCRIPTION("AIR QUALITY driver");
MODULE_AUTHOR("lipengpeng <lipengpeng@szprize.com>");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("AIRQUALITYDEVICE");

