#ifndef __RESMON_H__
#define __RESMON_H__

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/sched/rt.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>

#define TAG "resmon "

#define resmon_info(fmt, args...)		pr_info(TAG fmt, ##args)
#define resmon_debug(fmt, args...)		pr_debug(TAG fmt, ##args)
#define resmon_warn(fmt, args...)		pr_warn(TAG fmt, ##args)
#define resmon_error(fmt, args...)		pr_err(TAG fmt, ##args)

/* reserved, for high-resolution timers */
#define RESMON_PERIODICAL_BY_HR_TIMER          0

#if RESMON_PERIODICAL_BY_HR_TIMER
static ktime_t ktime;
#endif
#define RESMON_DEV_NAME "resmon"

#define RESMON_TASK_PRIORITY		(MAX_RT_PRIO - 3)
#define RESMON_TIMER_INTERVAL_MS		6000
#define RESMON_CPU_P		85
#define RESMON_IO_P		10

#define RESMON_DEFAULT_ENABLE		1

/* trigger type */
enum resmon_trigger {
	CPU_TRIGGER,
	MEM_TRIGGER,
	IOW_TRIGGER,
	NR_TRIGGER,
};


struct resmon_struct {
	unsigned int cpuload_thresh; /* cpu attr for userspace */
	unsigned int io_thresh;	/* io attr for userspace */
	unsigned int mem_thresh; /* mem attr for userspace */
	unsigned int trigger_time; /* time attr for userspace */
	unsigned int enable; /* enable attr for userspace */

	struct mutex lock; /* Synchronizes accesses to loads statistics */
	struct task_struct *tsk_struct_ptr; /* thread to process monitor */
#if RESMON_PERIODICAL_BY_HR_TIMER
	struct hrtimer hr_timer;
#endif
	struct timer_list tmr_list;

	struct miscdevice mdev; /* misc device */
	struct notifier_block notifier;
	atomic_t in_suspend;
	atomic_t state; /* Synchronizes state between user and kernel */

	/* percent  */
	unsigned long long cpuloads;
	unsigned long long cpuidle;
	unsigned long long iowait;

};
#endif
