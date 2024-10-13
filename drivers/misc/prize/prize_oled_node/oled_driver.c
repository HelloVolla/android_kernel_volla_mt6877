#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>

#define DRIVER_NAME "oled_driver"
#define DEVICE_NAME "oled"
#define DEVICE_TREE_COMP "coosea,oled"

static struct platform_device *oled_device;
//const struct device_attribute dev_attr_oled;

static int oled_open(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "OLED device opened\n");
    return 0;
}

static int oled_release(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "OLED device closed\n");
    return 0;
}

static struct file_operations oled_fops = {
    .owner = THIS_MODULE,
    .open = oled_open,
    .release = oled_release,
};

/* drv-add oled sysfs-pengzhipeng-20230306-start */
static ssize_t oled_show(struct device *dev,
                   struct device_attribute *attr, char *buf)
{
   int count = 0;
   
   count = snprintf(buf, PAGE_SIZE, "oled screens");  
   return count;
}
 
static ssize_t oled_store(struct device *dev,
           struct device_attribute *attr, const char *buf, size_t size)
{
    return size;
}
 
static DEVICE_ATTR(oled, 0664, oled_show, oled_store);

static int oled_probe(struct platform_device *pdev)
{
	int err = 0;
    printk(KERN_INFO "OLED probe function called\n");

    err = device_create_file(&pdev->dev, &dev_attr_oled);
	if (err) {
        pr_err("sys file creation failed\n");
        return -ENODEV;
	}
    return 0;
}

static int oled_remove(struct platform_device *pdev)
{
    printk(KERN_INFO "OLED remove function called\n");

    return 0;
}

static const struct of_device_id oled_of_match[] = {
    { .compatible = DEVICE_TREE_COMP, },
    {},
};

static struct platform_driver oled_platform_driver = {
    .probe = oled_probe,
    .remove = oled_remove,
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(oled_of_match),
    },
};

MODULE_DEVICE_TABLE(of, oled_of_match);

static int __init oled_driver_init(void)
{
    int ret = 0;

    printk(KERN_INFO "OLED driver initializing\n");

    ret = platform_driver_register(&oled_platform_driver);
    if (ret < 0) {
        printk(KERN_ERR "failed to register platform driver\n");
        return ret;
    }

    return 0;
}

static void __exit oled_driver_exit(void)
{
    printk(KERN_INFO "OLED driver exiting\n");

    platform_driver_unregister(&oled_platform_driver);
}

module_init(oled_driver_init);
module_exit(oled_driver_exit);

MODULE_AUTHOR("zhangchao");
MODULE_LICENSE("GPL");

