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
#include <linux/alarmtimer.h>
/*----------------------------------------------------------------------
static variable defination

eoc ---> endoscope
----------------------------------------------------------------------*/
#define ENDOSCOPE_DEVNAME    "endoscope_dev"
#define EN_DEBUG
#if defined(EN_DEBUG)
#define TRACE_FUNC 	printk("[endoscope_dev] function: %s, line: %d \n", __func__, __LINE__);
#define EOC_DEBUG  printk
#else
#define TRACE_FUNC(x,...)
#define EOC_DEBUG(x,...)
#endif
//drv huangjiwu 20230609 for sm nkj start
#ifdef CONFIG_PRIZE_ENDOSCOPE_SM
#define  DEBUG_KEY_UP 1
#endif
//drv huangjiwu 20230609 for sm nkj end
/****************************************************************/
/*******static function defination                             **/
/****************************************************************/
static struct endoscope_device{
	struct alarm eoc_timer;
	struct platform_device* pla_dev;
	struct pinctrl *eoc_pinctrl;
	struct pinctrl_state *eoc_default;
	struct pinctrl_state *eoc_switch_high;
	struct pinctrl_state *eoc_switch_low;
	struct pinctrl_state *endoscope_irq_eint;
	struct pinctrl_state *endoscope_ptt_irq_eint;
	int  eoc_irq_gpio;
	bool eoc_irq_sta;
	int	 eoc_irq_num;
	struct timespec endtime;
	//drv huangjiwu 20230609 for sm nkj start
#ifdef CONFIG_PRIZE_ENDOSCOPE_SM
	int  eoc_sel_gpio;
	int  eoc_ptt_gpio;
	int  eoc_irq_ptt;
	bool eoc_irq_ptt_sta;
	struct work_struct eint_work;
	struct workqueue_struct *eint_workqueue;
	struct delayed_work ptt_detcable;
	struct workqueue_struct *ptt_eint_workqueue;
#endif
//drv huangjiwu 20230609 for sm nkj end
};

static struct endoscope_device* eoc_dev;
//drv huangjiwu 20230609 for sm nkj start
#ifdef CONFIG_PRIZE_ENDOSCOPE_SM
static struct input_dev *endoscope_input_dev;

#define EINT_PIN_DOWN 1
#define EINT_PIN_UP 0
static int cur_eint_state = 0;

static int sm_state = 0;

static int cur_ptt_eint_state = 0;
int prize_get_endoscope_sel_flag(void)
{
	return gpio_get_value(eoc_dev->eoc_sel_gpio);
}
EXPORT_SYMBOL(prize_get_endoscope_sel_flag);

//extern void prize_extcon_detect_cable(int en);
extern void prize_sm_accdet_eint_func_extern(int state);
#endif
//drv huangjiwu 20230609 for sm nkj end
int prize_get_endoscope_flag(void)
{
	return gpio_get_value(eoc_dev->eoc_irq_gpio);
}
EXPORT_SYMBOL(prize_get_endoscope_flag);

static enum alarmtimer_restart endoscope_alarm_timer_func(struct alarm *alarm, ktime_t now)
{

	struct endoscope_device *dev = container_of(alarm, struct endoscope_device,eoc_timer);
	
	dev->eoc_irq_sta = gpio_get_value(dev->eoc_irq_gpio);
	EOC_DEBUG("[endoscope dev] endoscope device  --- > %s\n", dev->eoc_irq_sta?"disconnect" :"connect" );

	if (dev->eoc_irq_sta)
	{ 
	//drv huangjiwu 20230609 for sm nkj start
	#ifdef CONFIG_PRIZE_ENDOSCOPE_SM
	    if(sm_state == 1)
		{
		sm_state = 0;
		prize_sm_accdet_eint_func_extern(0);
		}
	#else
		pinctrl_select_state(dev->eoc_pinctrl, dev->eoc_switch_low);
	#endif
	//drv huangjiwu 20230609 for sm nkj end
	}else{
	//drv huangjiwu 20230609 for sm nkj start
		#ifdef CONFIG_PRIZE_ENDOSCOPE_SM
		pinctrl_select_state(eoc_dev->eoc_pinctrl, eoc_dev->eoc_switch_low);  //优先尝试 手麦
		prize_sm_accdet_eint_func_extern(1);
		sm_state = 1;
		#else
		pinctrl_select_state(dev->eoc_pinctrl, dev->eoc_switch_high);
		#endif
	//drv huangjiwu 20230609 for sm nkj end
	}


 	return ALARMTIMER_NORESTART;
}


static void endoscope_delay_detect_timer(struct endoscope_device* dev)
{
 	struct timespec time, time_now;
 	ktime_t ktime;
 	int ret = 0;

	ret = alarm_try_to_cancel(&dev->eoc_timer);
	if (ret < 0) {
		EOC_DEBUG("[endoscope dev]    callback was running, skip timer\n");
		return;
	}

	if(ret == 0)
		EOC_DEBUG("[endoscope dev]    the timer was not active\n");
	if(ret == 1)
		EOC_DEBUG("[endoscope dev]    the timer was active,cancel succ, restart new one\n");
	
 	get_monotonic_boottime(&time_now);
 	time.tv_sec =  2;
 	time.tv_nsec = 0;
 	dev->endtime = timespec_add(time_now, time);
 	ktime = ktime_set(dev->endtime.tv_sec, dev->endtime.tv_nsec);

 	EOC_DEBUG("[endoscope dev]    %s: alarm timer start:%d, %ld %ld\n", __func__, ret,dev->endtime.tv_sec, dev->endtime.tv_nsec);
 	alarm_start(&dev->eoc_timer, ktime);
	
}

static irqreturn_t endoscope_int_handler(int irq, void *dev_id)
{
	TRACE_FUNC;
	endoscope_delay_detect_timer(eoc_dev);
	//drv huangjiwu 20230609 for sm nkj start
#ifdef CONFIG_PRIZE_ENDOSCOPE_SM
	if(cur_ptt_eint_state == 1 )//抖动过程中 ptt 按键上报键值
	{
		irq_set_irq_type(eoc_dev->eoc_irq_ptt, IRQF_TRIGGER_NONE);
		cur_ptt_eint_state = 0;
		disable_irq_nosync(eoc_dev->eoc_irq_ptt);   //关闭按键中断
		if(cur_eint_state == EINT_PIN_DOWN)
		{
			cur_eint_state = EINT_PIN_UP;
			input_report_key(endoscope_input_dev, 381, cur_eint_state);
	        input_sync(endoscope_input_dev);
			EOC_DEBUG("[endoscope dev] up accdet PTT %d,irq=%d\n", cur_eint_state,gpio_get_value(eoc_dev->eoc_ptt_gpio));
		}
	}
#endif
//drv huangjiwu 20230609 for sm nkj end
	// disable_irq_nosync(eoc_dev->eoc_irq_num);
	return IRQ_HANDLED;
}
//drv huangjiwu 20230609 for sm nkj start
#ifdef CONFIG_PRIZE_ENDOSCOPE_SM
static void ptt_key_handler(struct work_struct *work)
{
	int ret = 0;
	if ((cur_eint_state == EINT_PIN_UP)&&(gpio_get_value(eoc_dev->eoc_ptt_gpio) == 1)) {
			ret=irq_set_irq_type(eoc_dev->eoc_irq_ptt, IRQ_TYPE_LEVEL_LOW);
			if (ret) {
			pr_err("%s : set_irq_type failed\n", __func__);
			}
		    cur_eint_state = EINT_PIN_DOWN;
	} else if((cur_eint_state == EINT_PIN_DOWN)&&(gpio_get_value(eoc_dev->eoc_ptt_gpio) == 0)){
			ret=irq_set_irq_type(eoc_dev->eoc_irq_ptt, IRQ_TYPE_LEVEL_HIGH);
			if (ret) {
			pr_err("%s : set_irq_type failed\n", __func__);
			}
		    cur_eint_state = EINT_PIN_UP;
	}else
	{
		EOC_DEBUG("[endoscope dev] cur_eint_state %d,eoc_ptt_gpio=%d\n", cur_eint_state,gpio_get_value(eoc_dev->eoc_ptt_gpio));
		return;
	}
	gpio_set_debounce(eoc_dev->eoc_ptt_gpio, 256*1000);
	if(cur_eint_state == gpio_get_value(eoc_dev->eoc_ptt_gpio))
	{
		input_report_key(endoscope_input_dev, 381, cur_eint_state);
		input_sync(endoscope_input_dev);
	}
	EOC_DEBUG("[endoscope dev] accdet PTT %d,irq=%d\n", cur_eint_state,gpio_get_value(eoc_dev->eoc_ptt_gpio));
}


static irqreturn_t endoscope_ptt_int_handler(int irq, void *dev_id)
{
	/* issue detection work */
	queue_delayed_work(eoc_dev->ptt_eint_workqueue, &eoc_dev->ptt_detcable, 100);
	return IRQ_HANDLED;
}
#endif
//drv huangjiwu 20230609 for sm nkj end
static int endoscope_get_dts_fun(struct endoscope_device* dev)
{
	int ret = 0;
	struct platform_device* pdev = dev->pla_dev;
	
	dev->eoc_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(dev->eoc_pinctrl)) {
		EOC_DEBUG("[endoscope dev]    Cannot find endoscope pinctrl!");
		ret = PTR_ERR(dev->eoc_pinctrl);
	}

	dev->eoc_default= pinctrl_lookup_state(dev->eoc_pinctrl, "default");
	if (IS_ERR(dev->eoc_default)) {
		ret = PTR_ERR(dev->eoc_default);
		EOC_DEBUG("[endoscope dev]    %s : init err, endoscope_default\n", __func__);
	}

	dev->eoc_switch_high = pinctrl_lookup_state(dev->eoc_pinctrl, "endoscope_switch_high");
	if (IS_ERR(dev->eoc_switch_high)) {
		ret = PTR_ERR(dev->eoc_switch_high);
		EOC_DEBUG("[endoscope dev]    %s : init err, endoscope_switch_high\n", __func__);
	}

	dev->eoc_switch_low = pinctrl_lookup_state(dev->eoc_pinctrl, "endoscope_switch_low");
	if (IS_ERR(dev->eoc_switch_low)) {
		ret = PTR_ERR(dev->eoc_switch_low);
		EOC_DEBUG("[endoscope dev]    %s : init err, endoscope_switch_low\n", __func__);
	}

	dev->endoscope_irq_eint = pinctrl_lookup_state(dev->eoc_pinctrl, "endoscope_irq_eint");
	if (IS_ERR(dev->endoscope_irq_eint)) {
		ret = PTR_ERR(dev->endoscope_irq_eint);
		EOC_DEBUG("[endoscope dev]    %s : init err, endoscope_irq_eint\n", __func__);
	}
#ifdef CONFIG_PRIZE_ENDOSCOPE_SM
	dev->endoscope_ptt_irq_eint = pinctrl_lookup_state(dev->eoc_pinctrl, "endoscope_ptt_irq_eint");
	if (IS_ERR(dev->endoscope_ptt_irq_eint)) {
		ret = PTR_ERR(dev->endoscope_ptt_irq_eint);
		EOC_DEBUG("[endoscope dev]    %s : init err, endoscope_switch_low\n", __func__);
	}
	pinctrl_select_state(dev->eoc_pinctrl, dev->endoscope_ptt_irq_eint);
#endif
	pinctrl_select_state(dev->eoc_pinctrl, dev->eoc_switch_low);
	pinctrl_select_state(dev->eoc_pinctrl, dev->endoscope_irq_eint);

	return 0;
}

static int endoscope_irq_init(struct endoscope_device* dev)
{
	int irq_flags = 0;
	int ret = 0;
	struct platform_device* pdev = dev->pla_dev;
	//drv huangjiwu 20230609 for sm nkj start
#ifdef CONFIG_PRIZE_ENDOSCOPE_SM
	dev->eoc_sel_gpio = of_get_named_gpio(pdev->dev.of_node, "sel-gpio", 0);
	if (dev->eoc_sel_gpio < 0) {
		EOC_DEBUG("[endoscope dev]    %s: no sel gpio provided.\n", __func__);
		return -1;
	} else {
		EOC_DEBUG("[endoscope dev]    %s: sel gpio provided ok.sy8801_dev->eoc_sel_gpio = %d\n", __func__, dev->eoc_sel_gpio);
	}
	
	dev->eoc_ptt_gpio = of_get_named_gpio(pdev->dev.of_node, "ptt-gpio", 0);
	if (dev->eoc_ptt_gpio < 0) {
		EOC_DEBUG("[endoscope dev]    %s: no ptt gpio provided.\n", __func__);
		return -1;
	} else {
		EOC_DEBUG("[endoscope dev]    %s: ptt gpio provided ok.sy8801_dev->eoc_ptt_gpio = %d\n", __func__, dev->eoc_ptt_gpio);
	}
#endif
	//drv huangjiwu 20230609 for sm nkj end
	dev->eoc_irq_gpio = of_get_named_gpio(pdev->dev.of_node, "irq-gpio", 0);
	if (dev->eoc_irq_gpio < 0) {
		EOC_DEBUG("[endoscope dev]    %s: no irq gpio provided.\n", __func__);
		return -1;
	} else {
		EOC_DEBUG("[endoscope dev]    %s: irq gpio provided ok.sy8801_dev->irq_gpio = %d\n", __func__, dev->eoc_irq_gpio);
	}
	dev->eoc_irq_num =	gpio_to_irq(dev->eoc_irq_gpio);

	if (gpio_is_valid(dev->eoc_irq_gpio)) {
		ret = devm_gpio_request_one(&pdev->dev,dev->eoc_irq_gpio,GPIOF_DIR_IN, "endoscope_int");
		if (ret) {
			EOC_DEBUG("[endoscope dev]    %s: irq_gpio request failed\n", __func__);
			return -1;
		}
		irq_flags = IRQF_TRIGGER_FALLING  | IRQF_ONESHOT | IRQF_TRIGGER_RISING;
		ret = devm_request_threaded_irq(&pdev->dev,dev->eoc_irq_num,NULL,endoscope_int_handler, irq_flags, "endoscope", dev);

		if (ret != 0) {
				EOC_DEBUG("[endoscope dev]    failed to request IRQ %d: %d\n", dev->eoc_irq_num, ret);
				return -1;
		}
		EOC_DEBUG("[endoscope dev]    sucess to request IRQ %d: %d\n", dev->eoc_irq_num, ret);

	}else{
		EOC_DEBUG("[endoscope dev]    %s skipping IRQ registration\n", __func__);
	}
	return 0;
}

#if DEBUG_KEY_UP
static ssize_t endoscope_sel_show(struct device *dev,
								   struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "sel = %x\n", gpio_get_value(eoc_dev->eoc_sel_gpio));
}

static ssize_t endoscope_sel_store(struct device *dev,
									struct device_attribute *attr, const char *buf, size_t size)
{
	int data;
	if(sscanf(buf, "%u", &data) != 1)
	{
		EOC_DEBUG("[endoscope dev]: Invalid values\n");
		return -EINVAL;
	}
	if(data)
	{
		pinctrl_select_state(eoc_dev->eoc_pinctrl, eoc_dev->eoc_switch_high);
		//prize_extcon_detect_cable(1);
	}
	else
	{
		pinctrl_select_state(eoc_dev->eoc_pinctrl, eoc_dev->eoc_switch_low);
		if( prize_get_endoscope_sel_flag() == 0 )
		{
			prize_sm_accdet_eint_func_extern(1);
		}
		//prize_extcon_detect_cable(1);
	}
	return size;
}
static DEVICE_ATTR(endoscope_sel, S_IRUGO | S_IWUSR, endoscope_sel_show, endoscope_sel_store);
#endif
//drv huangjiwu 20230609 for sm nkj start
#ifdef CONFIG_PRIZE_ENDOSCOPE_SM
extern unsigned int accdet_val;

static void endoscope_eint_work_callback(struct work_struct *work)
{
	int ret = 0;
	if((accdet_val  < 1200)&& (accdet_val !=0))   //accdet_val  == 800mv  1200
	{
		//识别为手麦 无需更改
		EOC_DEBUG("[endoscope dev]: shoumai\n");
		if(cur_ptt_eint_state == 0)
		{
		    ret = irq_set_irq_type(eoc_dev->eoc_irq_ptt, IRQ_TYPE_LEVEL_HIGH);
		  if (ret) {
			EOC_DEBUG("[endoscope dev]: irq_set_irq_type failed\n");
			}
			cur_ptt_eint_state = 1;
			enable_irq(eoc_dev->eoc_irq_ptt);   //打开按键中断
		}
	}
	else
	{
		EOC_DEBUG("[endoscope dev]: neikuijing\n");
		pinctrl_select_state(eoc_dev->eoc_pinctrl, eoc_dev->eoc_switch_high);
		//prize_extcon_detect_cable(1);
	}
}

void  endoscope_work_wakeup(void)
{
	int ret = 0;
	if( sm_state )
	{
		ret = queue_work(eoc_dev->eint_workqueue, &eoc_dev->eint_work);
	}
	EOC_DEBUG("[endoscope dev]: endoscope_work_wakeup sm_state =%d\n",sm_state);
}
EXPORT_SYMBOL_GPL(endoscope_work_wakeup);
#endif
//drv huangjiwu 20230609 for sm nkj end
static int endoscope_probe(struct platform_device *pdev)
{
	#if DEBUG_KEY_UP
	struct class *endoscope_class;
	struct device *endoscope_dev;
	#endif
	int ret;
	TRACE_FUNC;

	eoc_dev = devm_kzalloc(&pdev->dev, sizeof(struct endoscope_device), GFP_KERNEL);
	if(IS_ERR_OR_NULL(eoc_dev)) 
    { 
       ret = PTR_ERR(eoc_dev);
       EOC_DEBUG("[endoscope dev]    failed to devm_kzalloc endoscope_dev %d\n", ret);
	   return -1;
    }
	eoc_dev->pla_dev = pdev;
	
	ret = endoscope_irq_init(eoc_dev);
	if(ret < 0){
		EOC_DEBUG("[endoscope dev]    failed to endoscope_irq_init %d\n", ret);
		return ret;
	}

    ret = endoscope_get_dts_fun(eoc_dev);
	if(ret < 0){
		EOC_DEBUG("[endoscope dev]    failed to endoscope_get_dts_fun %d\n", ret);
		return ret;
	}

	alarm_init(&eoc_dev->eoc_timer, ALARM_BOOTTIME,endoscope_alarm_timer_func);
//drv huangjiwu 20230609 for sm nkj start
	#ifdef CONFIG_PRIZE_ENDOSCOPE_SM
	//alarm_init(&eoc_dev->ptt_timer, ALARM_BOOTTIME,endoscope_ptt_alarm_timer_func);
	eoc_dev->eint_workqueue = create_singlethread_workqueue("endoscope_acc_eint");
	INIT_WORK(&eoc_dev->eint_work, endoscope_eint_work_callback);
	if (!eoc_dev->eint_workqueue) {
		EOC_DEBUG("Error: Create eint workqueue failed\n");
		ret = -1;
	}
		//drv  huangjiwu for  start
	#if DEBUG_KEY_UP
	endoscope_class = class_create(THIS_MODULE, "endoscope");
	if (IS_ERR(endoscope_class)) {
		EOC_DEBUG("Failed to create class(endoscope_class)!");
		return PTR_ERR(endoscope_class);
	}
	endoscope_dev = device_create(endoscope_class, NULL, 0, NULL, "endoscope_data");
	if (IS_ERR(endoscope_dev))
	{
		EOC_DEBUG("Failed to create endoscope_dev device");
	}
	if (device_create_file(endoscope_dev, &dev_attr_endoscope_sel) < 0)
	{
		EOC_DEBUG("Failed to create device file(%s)!",dev_attr_endoscope_sel.attr.name);	
	}
	/* Create input device*/
	endoscope_input_dev = input_allocate_device();
	if (!endoscope_input_dev) {
		ret = -ENOMEM;
		EOC_DEBUG("%s input_allocate_device fail.\n", __func__);
	}
	__set_bit(EV_KEY, endoscope_input_dev->evbit);
	__set_bit(381, endoscope_input_dev->keybit);
	//__set_bit(385, endoscope_input_dev->keybit);
	endoscope_input_dev->id.bustype = BUS_HOST;
	endoscope_input_dev->name = "endoscope";
	ret = input_register_device(endoscope_input_dev);
	if (ret) {
		EOC_DEBUG("%s input_register_device fail.ret:%d\n", __func__,ret);
	}
	//drv  huangjiwu for  end
	#endif
	eoc_dev->eoc_irq_ptt =	gpio_to_irq(eoc_dev->eoc_ptt_gpio);
	gpio_set_debounce(eoc_dev->eoc_ptt_gpio, 256*1000);
	
	eoc_dev->ptt_eint_workqueue = create_singlethread_workqueue("ppt_workqueue");
	INIT_DELAYED_WORK(&eoc_dev->ptt_detcable, ptt_key_handler);
	
	ret = request_irq(eoc_dev->eoc_irq_ptt, endoscope_ptt_int_handler, IRQF_TRIGGER_HIGH,
		"endoscope_ptt", NULL);	   //IRQF_TRIGGER_NONE
	disable_irq(eoc_dev->eoc_irq_ptt);   //关闭按键中断
	if (ret) {
		EOC_DEBUG("%s endoscope_ptt irq fail.ret:%d\n", __func__,ret);
	}
	//
	if(!gpio_get_value(eoc_dev->eoc_irq_gpio))
	{
		endoscope_delay_detect_timer(eoc_dev);
	}
	#endif
//drv huangjiwu 20230609 for sm nkj end
	return 0;
}

static int endoscope_remove(struct platform_device *dev)	
{
	EOC_DEBUG("[endoscope dev]    [endoscope_dev]:endoscope_remove begin!\n");
	EOC_DEBUG("[endoscope dev]    [endoscope_dev]:endoscope_remove Done!\n");
	return 0;
}
static const struct of_device_id endoscope_dt_match[] = {
	{.compatible = "prize,endoscope"},
	{},
};

static struct platform_driver endoscope_driver = {
	.probe	= endoscope_probe,
	.remove  = endoscope_remove,
	.driver    = {
		.name       = "endoscope_Driver",
		.of_match_table = of_match_ptr(endoscope_dt_match),
	},
};

static int __init endoscope_init(void)
{
    int retval = 0;
    TRACE_FUNC;

    EOC_DEBUG("[endoscope dev]    [%s]: endoscope_driver, retval=%d \n!", __func__, retval);
	if (retval != 0) {
		  return retval;
	}
    platform_driver_register(&endoscope_driver);
    return 0;
}

static void __exit endoscope_exit(void)
{
    TRACE_FUNC;
    platform_driver_unregister(&endoscope_driver);
}
//drv huangjiwu 20230609 for sm nkj start
#ifdef CONFIG_PRIZE_ENDOSCOPE_SM
late_initcall(endoscope_init);
#else
module_init(endoscope_init);
#endif
//drv huangjiwu 20230609 for sm nkj end
module_exit(endoscope_exit);
MODULE_DESCRIPTION("ENDOSCOPE DEVICE driver");
MODULE_AUTHOR("liaojie <liaojie@cooseagroup.com>");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("ENDOSCOPE device");

