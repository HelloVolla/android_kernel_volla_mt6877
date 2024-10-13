/* Copyright Prize
 * Author: Eileen <lifenfen@szprize.com>
 * process monitor in kernel.
 */

#include <linux/compat.h>
#include <linux/eventfd.h>
#include <linux/vhost.h>
#include <linux/virtio_net.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
/*lijimeng*/
#include <linux/sched/cputime.h>
#include <linux/tick.h>
#include <linux/cpufreq.h>
#include <linux/swap.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/kernel_stat.h>
#include <linux/time.h>
#include <linux/task_io_accounting_ops.h>
#include <linux/fb.h>
#include <asm/div64.h>

#include <linux/cpumask.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/irqnr.h>
/*lijimeng*/
#include <linux/sched/cputime.h>
#include <linux/tick.h>
#include <linux/cpufreq.h>
#include <linux/swap.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/pid_namespace.h>
#include <linux/ptrace.h>
#include <linux/task_io_accounting_ops.h>
#include <linux/oom.h>
#include <linux/security.h>


#include "resmon.h"

#define P2M(x) ((((unsigned long)x) << (PAGE_SHIFT - 10)) >> (10))
#define RESMON_FILTER	"app"
#define SYSTEM_FILTER	"system_server"


struct resmon_struct *resmon_obj;

#ifdef arch_idle_time

static cputime64_t get_idle_time(int cpu)
{
	cputime64_t idle;

	idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	if (cpu_online(cpu) && !nr_iowait_cpu(cpu))
		idle += arch_idle_time(cpu);
	return idle;
}

static cputime64_t get_iowait_time(int cpu)
{
	cputime64_t iowait;

	iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	if (cpu_online(cpu) && nr_iowait_cpu(cpu))
		iowait += arch_idle_time(cpu);
	return iowait;
}

#else

static u64 get_idle_time(int cpu)
{
	u64 idle, idle_time = -1ULL;

	/*if (cpu_online(cpu))*/
		idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.idle */
		idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
	else
		idle = idle_time * NSEC_PER_USEC;

	return idle;
}

static u64 get_iowait_time(int cpu)
{
	u64 iowait, iowait_time = -1ULL;

	/*if (cpu_online(cpu))*/
		iowait_time = get_cpu_iowait_time_us(cpu, NULL);

	if (iowait_time == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.iowait */
		iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
	else
		iowait = iowait_time* NSEC_PER_USEC;

	return iowait;
}

#endif

static void cpustat_info(unsigned long long *info, int len)
{
	int i;
	if (len >= NR_STATS)
	{
		for_each_possible_cpu(i) {
			info[CPUTIME_USER] += kcpustat_cpu(i).cpustat[CPUTIME_USER];
			info[CPUTIME_NICE] += kcpustat_cpu(i).cpustat[CPUTIME_NICE];
			info[CPUTIME_SYSTEM] += kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
			info[CPUTIME_IDLE] += get_idle_time(i);
			info[CPUTIME_IOWAIT] += get_iowait_time(i);
			info[CPUTIME_IRQ] += kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
			info[CPUTIME_SOFTIRQ] += kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
			info[CPUTIME_STEAL] += kcpustat_cpu(i).cpustat[CPUTIME_STEAL];
			info[CPUTIME_GUEST] += kcpustat_cpu(i).cpustat[CPUTIME_GUEST];
			info[CPUTIME_GUEST_NICE] += kcpustat_cpu(i).cpustat[CPUTIME_GUEST_NICE];
		}
	}
}

static long meminfo_available(void)
{
	struct sysinfo i;
	long cached;
	long available;

	si_meminfo(&i);
	si_swapinfo(&i);

	cached = global_node_page_state(NR_FILE_PAGES) -
			total_swapcache_pages() - i.bufferram;
	if (cached < 0)
		cached = 0;

	available = i.freeram + cached;

	return P2M(available);
}

static int send_event(enum resmon_trigger trigger, int value)
{
	char name_buf[32];
	char state_buf[32];
	char *envp[3];
	int env_offset = 0;
	int ret = 0;

	snprintf(name_buf, sizeof(name_buf),"TYPE=%d", trigger);
	envp[env_offset++] = name_buf;

	snprintf(state_buf, sizeof(state_buf),"LOAD=%d", value);
	envp[env_offset++] = state_buf;

	envp[env_offset] = NULL;

	//resmon_error("resmon send_event - sending event - %s in %s\n", *envp, kobject_get_path(&resmon_obj->mdev.this_device->kobj, GFP_KERNEL));

	ret = kobject_uevent_env(&resmon_obj->mdev.this_device->kobj, KOBJ_CHANGE, envp);
	if (ret < 0)
		resmon_error("uevent sending failed with ret = %d\n", ret);

	return ret;
}

#ifdef CONFIG_RESMON_KILL_DEBUG
int test_pid[1024] = {0};
static int test_count = 0;
void test_set_pid(int id, int pid)
{
	test_pid[id] = pid;
}
#endif

static void resmon_process(void)
{
	unsigned long long cpustat[NR_STATS] = {0};
	unsigned long long cpuloads, cpuidle, iowait = 0;
	long long cpu_delta, idle_delta, io_delta= 0;
	long long cur_loads, cur_iowait, cur_mem = 0;		/* prizem modify compile error */

	mutex_lock(&resmon_obj->lock);

	cpustat_info(cpustat, NR_STATS);

	cpuloads = cpustat[CPUTIME_USER] + cpustat[CPUTIME_NICE]
		+ cpustat[CPUTIME_SYSTEM] + cpustat[CPUTIME_SOFTIRQ]
		+ cpustat[CPUTIME_IRQ] + cpustat[CPUTIME_IDLE]
		+ cpustat[CPUTIME_IOWAIT];
	cpuidle = cpustat[CPUTIME_IDLE];
	iowait = cpustat[CPUTIME_IOWAIT];

	if (resmon_obj->cpuloads != 0)
	{
		cpu_delta = cpuloads - resmon_obj->cpuloads;
		idle_delta = cpuidle - resmon_obj->cpuidle;
		io_delta = iowait - resmon_obj->iowait;
		cur_mem = meminfo_available();

		resmon_debug("%s cpu_delta:%lld idle_delta:%lld io_delta:%lld\n", __func__, cpu_delta, idle_delta, io_delta);
		#if BITS_PER_LONG == 32
		cur_loads = (cpu_delta - idle_delta) * 100;
		do_div(cur_loads, cpu_delta);

		cur_iowait = io_delta * 100;
		do_div(cur_iowait, cpu_delta);
		#else
		cur_loads = 100 * (cpu_delta - idle_delta) / (cpu_delta);
		cur_iowait = 100 * (io_delta) / (cpu_delta);
		#endif

		resmon_error("resmon %s cur_loads:%d cur_iowait:%d cur_mem:%d\n", __func__, cur_loads, cur_iowait, cur_mem);

		if (cur_loads > resmon_obj->cpuload_thresh)
			send_event(CPU_TRIGGER, cur_loads);
		if (cur_iowait > resmon_obj->io_thresh)
			send_event(IOW_TRIGGER, cur_iowait);
		if (cur_mem < resmon_obj->mem_thresh)
			send_event(MEM_TRIGGER, cur_mem);
	}

	resmon_obj->cpuloads = cpuloads;
	resmon_obj->cpuidle = cpuidle;
	resmon_obj->iowait = iowait;
	mutex_unlock(&resmon_obj->lock);

#ifdef CONFIG_RESMON_KILL_DEBUG
	while(test_pid[test_count] && (test_count < 5))
	{
		resmon_error("kil pid = %d\n", test_pid[test_count]);
		kill_pid(find_vpid(test_pid[test_count]), SIGKILL, 1);
		test_count ++;
	}
	test_count = 0;
#endif
}

static int _resmon_task_main(void *data)
{
	resmon_warn("enter _resmon_task_main\n");

	while (1) {
		resmon_process();
		#if RESMON_PERIODICAL_BY_HR_TIMER
		hrtimer_cancel(&resmon_obj->hr_timer);
		ktime = ktime_set(resmon_obj->trigger_time / 1000, (resmon_obj->trigger_time % 1000) * 1000000);
		hrtimer_start(&resmon_obj->hr_timer, ktime, HRTIMER_MODE_REL);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
		#else
		mod_timer(&resmon_obj->tmr_list,
				(jiffies + msecs_to_jiffies(resmon_obj->trigger_time)));
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
		#endif
		if (kthread_should_stop())
			break;
	}

	resmon_warn("leave _resmon_task_main\n");

	return 0;
}

static void _resmon_timer_callback(struct timer_list *t)
{
    /* resmon_warn("_resmon_timer_callback\n");  */
    if (resmon_obj->tsk_struct_ptr)
        wake_up_process(resmon_obj->tsk_struct_ptr);
    //return HRTIMER_NORESTART;
}

static int resmon_task_start(void)
{
    resmon_warn("resmon resmon_task_start");
	if (!atomic_read(&resmon_obj->state))
	{
		atomic_set(&resmon_obj->state, 1);

	//struct sched_param param = {.sched_priority = RESMON_TASK_PRIORITY };

	if (resmon_obj->tsk_struct_ptr == NULL) {
	    //resmon_obj->cpuloads = 1;
		resmon_obj->tsk_struct_ptr = kthread_create(_resmon_task_main, NULL, "resmon_main");
		if (IS_ERR(resmon_obj->tsk_struct_ptr))
			return PTR_ERR(resmon_obj->tsk_struct_ptr);

		//sched_setscheduler_nocheck(resmon_obj->tsk_struct_ptr, SCHED_FIFO, &param);
		get_task_struct(resmon_obj->tsk_struct_ptr);
		wake_up_process(resmon_obj->tsk_struct_ptr);
		resmon_warn("resmon task start success, ptr: %p, pid: %d\n",
			 resmon_obj->tsk_struct_ptr, resmon_obj->tsk_struct_ptr->pid);
	} else {
		resmon_warn("resmon task already exist, ptr: %p, pid: %d\n",
			 resmon_obj->tsk_struct_ptr, resmon_obj->tsk_struct_ptr->pid);
	}
	}
	return 0;
}

static int resmon_task_stop(void)
{
	int r = 0;
	resmon_warn("resmon_task_stop\n");
	if (atomic_read(&resmon_obj->state))
	{
		atomic_set(&resmon_obj->state, 0);
#if RESMON_PERIODICAL_BY_HR_TIMER
	/*deinit timer */
	r = hrtimer_cancel(&resmon_obj->hr_timer);
	if (r)
		resmon_error("resmon hr timer delete error!\n");
#else
	/*deinit timer */
	del_timer_sync(&resmon_obj->tmr_list);
#endif

	if (resmon_obj->tsk_struct_ptr) {
		kthread_stop(resmon_obj->tsk_struct_ptr);
		put_task_struct(resmon_obj->tsk_struct_ptr);
		resmon_obj->tsk_struct_ptr = NULL;

		resmon_obj->cpuloads = 0;
		resmon_obj->cpuidle = 0;
		resmon_obj->iowait = 0;
	}
	}
	return r;
}


static ssize_t show_system_stat(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
	int i, pos = 0;
	unsigned long long cpustat[NR_STATS] = {0};

	cpustat_info(cpustat, NR_STATS);

	for(i = CPUTIME_USER; i < NR_STATS; i++)
		pos += snprintf(buf + pos, 64, " %llu", nsec_to_clock_t(cpustat[i]));

	for_each_possible_cpu(i)
	{
		pos += snprintf(buf + pos, 64, " %u", cpufreq_quick_get_max(i));
	}

	pos += snprintf(buf + pos, 64, " %lu\n", meminfo_available());

	return pos;
}

static DEVICE_ATTR(system_stat, S_IRUGO, show_system_stat, NULL);


static ssize_t show_system_io_mem_stat(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
#ifdef CONFIG_RESMON_DEBUG
	int i = 0;
#endif
    int pos = 0;
	struct task_struct *task;
	struct task_struct *t;
	int result = 0;
	unsigned long long read_bytes, write_bytes = 0;
	u64 utime, stime = 0;
	unsigned long mm_size = 0;
	int oom_adj = OOM_ADJUST_MIN;
	struct task_io_accounting acct;
	unsigned long flags;
	//char value[64] = {0};
	//char *secctx = value;
	char *secctx  = NULL;
	bool from_system = false;

	rcu_read_lock();
	for_each_process(task) {
		if (task->flags & PF_KTHREAD)
			continue;

		if (lock_task_sighand(task, &flags)) {
			result = security_getprocattr(task, "current", &secctx);
			if (result <= 0)
			{
				kfree(secctx);
				unlock_task_sighand(task, &flags);
				continue;
			}

			if (in_egroup_p(KGIDT_INIT(6666)) && strstr(secctx, SYSTEM_FILTER))
			{
				from_system = true;
			}

			if (!from_system && !strstr(secctx, RESMON_FILTER))
			{
			    kfree(secctx);
				unlock_task_sighand(task, &flags);
				continue;
			}

			from_system = false;

			kfree(secctx);
			thread_group_cputime_adjusted(task, &utime, &stime);

			if (task->mm)
				mm_size =  P2M(get_mm_rss(task->mm));

			acct = task->ioac;
			task_io_accounting_add(&acct, &task->signal->ioac);
			t = task;
			while_each_thread(task, t)
				task_io_accounting_add(&acct, &t->ioac);
#ifdef CONFIG_TASK_IO_ACCOUNTING
			read_bytes = acct.read_bytes;
			write_bytes = acct.write_bytes;
#endif

			//if (task->signal->oom_score_adj == OOM_SCORE_ADJ_MAX)
			        //oom_adj = OOM_ADJUST_MAX;
			//else  /*modify for oom_score_adj->oom_adj round*/
			        //oom_adj = ((task->signal->oom_score_adj * -OOM_DISABLE * 10)/OOM_SCORE_ADJ_MAX+5)/10;
			oom_adj = task->signal->oom_score_adj;
            //resmon_error("resmon pid:%d, utime:%llu, stime:%llu,mm:%llu, read:%llu,write:%llu,oom_adj:%d",task->pid,nsec_to_clock_t(utime),nsec_to_clock_t(stime),mm_size,read_bytes,write_bytes,oom_adj);
			pos += sprintf(buf + pos, "%d ", task->pid);
			pos += sprintf(buf + pos, "%llu ", (long long unsigned int)nsec_to_clock_t(utime) );
			pos += sprintf(buf + pos, "%llu ", (long long unsigned int)nsec_to_clock_t(stime) );
			pos += sprintf(buf + pos, "%llu ", (long long unsigned int)mm_size);
			pos += sprintf(buf + pos, "%llu ", read_bytes);
			pos += sprintf(buf + pos, "%llu ", write_bytes);
			pos += sprintf(buf + pos, "%d\n", oom_adj );

#ifdef CONFIG_RESMON_KILL_DEBUG
			test_set_pid(count ++, task->pid);
#endif
			unlock_task_sighand(task, &flags);
		}
	}
	rcu_read_unlock();

#ifdef CONFIG_RESMON_KILL_DEBUG
	count = 0;
#endif

	return pos;
}

static DEVICE_ATTR(system_io_mem_stat, S_IRUGO, show_system_io_mem_stat, NULL);

static ssize_t
resmon_sysfs_cpuload_thresh_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
        return snprintf(buf, 8, "%d\n", resmon_obj->cpuload_thresh);
}

static ssize_t
resmon_sysfs_cpuload_thresh_store(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t count)
{
        long int val = 0;

        /* We only accept numbers. */
        if (kstrtol(buf, 10, &val) < 0)
                return -EINVAL;

		resmon_obj->cpuload_thresh = val;

        return count;
}
static DEVICE_ATTR(cpuload_thresh, S_IRUGO | S_IWUSR, resmon_sysfs_cpuload_thresh_show, resmon_sysfs_cpuload_thresh_store);

static ssize_t
resmon_sysfs_io_thresh_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
        return snprintf(buf, 8, "%d\n", resmon_obj->io_thresh);
}

static ssize_t
resmon_sysfs_io_thresh_store(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t count)
{
        long int val = 0;

        /* We only accept numbers. */
        if (kstrtol(buf, 10, &val) < 0)
                return -EINVAL;

		resmon_obj->io_thresh = val;

        return count;
}
static DEVICE_ATTR(io_thresh, S_IRUGO | S_IWUSR, resmon_sysfs_io_thresh_show, resmon_sysfs_io_thresh_store);

static ssize_t
resmon_sysfs_mem_thresh_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
        return snprintf(buf, 8, "%d\n", resmon_obj->mem_thresh);
}

static ssize_t
resmon_sysfs_mem_thresh_store(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t count)
{
        long int val = 0;

        /* We only accept numbers. */
        if (kstrtol(buf, 10, &val) < 0)
                return -EINVAL;

		resmon_obj->mem_thresh = val;

        return count;
}
static DEVICE_ATTR(mem_thresh, S_IRUGO | S_IWUSR, resmon_sysfs_mem_thresh_show, resmon_sysfs_mem_thresh_store);

static ssize_t
resmon_sysfs_trigger_time_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
        return snprintf(buf, 8, "%d\n", resmon_obj->trigger_time);
}

static ssize_t
resmon_sysfs_trigger_time_store(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t count)
{
        long int val = 0;

        /* We only accept numbers. */
        if (kstrtol(buf, 10, &val) < 0)
                return -EINVAL;

		resmon_obj->trigger_time = val;

        return count;
}
static DEVICE_ATTR(trigger_time, S_IRUGO | S_IWUSR, resmon_sysfs_trigger_time_show, resmon_sysfs_trigger_time_store);

static ssize_t
resmon_sysfs_enable_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
        return snprintf(buf, 8, "%d\n", resmon_obj->enable);
}

static ssize_t
resmon_sysfs_enable_store(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t count)
{
        long int val = 0;

        /* We only accept numbers. */
        if (kstrtol(buf, 10, &val) < 0)
                return -EINVAL;

		resmon_obj->enable = !!val;


        //resmon_error("resmon resmon_sysfs_enable_store enable=%d, in_suspend=%d",resmon_obj->enable,resmon_obj->in_suspend);

		if(!atomic_read(&resmon_obj->in_suspend)) {
			if (resmon_obj->enable == 1)
				resmon_task_start();
			else
				resmon_task_stop();
		}
        return count;
}
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, resmon_sysfs_enable_show, resmon_sysfs_enable_store);

static struct attribute*
resmon_sysfs_parameters_attrs[] = {
        &dev_attr_cpuload_thresh.attr,
        &dev_attr_io_thresh.attr,
        &dev_attr_mem_thresh.attr,
		&dev_attr_trigger_time.attr,
        &dev_attr_enable.attr,
        NULL,
};

static const struct attribute_group
resmon_sysfs_parameters_grp = {
        .name = "parameters",
        .attrs = resmon_sysfs_parameters_attrs,
};

static int resmon_fb_notifier_callback(struct notifier_block *self,
                        unsigned long event, void *data)
{
        struct fb_event *evdata = data;
        unsigned int blank;
        int retval = 0;

        /* If we aren't interested in this event, skip it immediately ... */
        if (event != FB_EVENT_BLANK /* FB_EARLY_EVENT_BLANK */)
                return 0;

        blank = *(int *)evdata->data;

        switch (blank) {
        case FB_BLANK_UNBLANK:
		atomic_set(&resmon_obj->in_suspend, 0);
/* thread state is controlled by user cmd rather than kernel fb notification */
		if (resmon_obj->enable == 1)
			resmon_task_start();

                break;

        case FB_BLANK_POWERDOWN:
		atomic_set(&resmon_obj->in_suspend, 1);
		//if (resmon_obj->enable == 1)
			resmon_task_stop();
                break;

        default:
                resmon_debug("[%s] : other notifier, ignore\n", __func__);
                break;
        }

        return retval;
}

static struct resmon_struct *resmon_alloc_object(void)
{

        struct resmon_struct *obj = kzalloc(sizeof(*obj), GFP_KERNEL);

        if (!obj) {
                resmon_error("Alloc resmon object error!\n");
                return NULL;
        }
		obj->cpuload_thresh = RESMON_CPU_P;
		obj->io_thresh = RESMON_IO_P;
		obj->mem_thresh = 0;
		obj->trigger_time = RESMON_TIMER_INTERVAL_MS;
		obj->enable = RESMON_DEFAULT_ENABLE;

		obj->tsk_struct_ptr = NULL;
		obj->cpuloads = 0;
		obj->cpuidle = 0;
		obj->iowait = 0;

	    atomic_set(&obj->in_suspend, 0);
	    atomic_set(&obj->state, 0);
		mutex_init(&obj->lock);

#if RESMON_PERIODICAL_BY_HR_TIMER
		/*init Hrtimer */
		hrtimer_init(&obj->hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		obj->hr_timer.function = (void *)&_resmon_timer_callback;
#else
		/*init timer */
		//init_timer(&obj->tmr_list);
		//obj->tmr_list.function = (void *)&_resmon_timer_callback;
		//obj->tmr_list.data = (unsigned long)obj;
		timer_setup(&obj->tmr_list, _resmon_timer_callback, 0);
		obj->tmr_list.expires = jiffies + msecs_to_jiffies(obj->trigger_time);
#endif
        return obj;
}

static int resmon_init(void)
{
	int ret = 0;

	resmon_obj = resmon_alloc_object();
	if (!resmon_obj) {
        ret = -ENOMEM;
        resmon_error("unable to allocate resmon obj!\n");
        return ret;
	}

	resmon_obj->mdev.minor = MISC_DYNAMIC_MINOR;
	resmon_obj->mdev.name = RESMON_DEV_NAME;

	ret = misc_register(&resmon_obj->mdev);
	if (ret) {
        resmon_error("misc_register error:%d\n", ret);
		goto exit_alloc_data_failed;
	}

	/* register screen on/off callback */
	resmon_obj->notifier.notifier_call = resmon_fb_notifier_callback;
	ret = fb_register_client(&resmon_obj->notifier);
	if (ret)
		goto exit_misc_register_failed;

	ret = device_create_file(resmon_obj->mdev.this_device, &dev_attr_system_stat);
	if (ret)
		goto err_create_file_failed;

	ret = device_create_file(resmon_obj->mdev.this_device, &dev_attr_system_io_mem_stat);
	if (ret)
		goto err_create_system_failed;

	ret = sysfs_create_group(&resmon_obj->mdev.this_device->kobj, &resmon_sysfs_parameters_grp);
	if (ret)
		goto err_create_attr_failed;

	if (resmon_obj->enable == 1)
	{
		ret = resmon_task_start();
		if (ret)
			goto err_create_attr_failed;
	}

	return 0;

err_create_attr_failed:
	sysfs_remove_group(&resmon_obj->mdev.this_device->kobj, &resmon_sysfs_parameters_grp);

err_create_system_failed:
    device_remove_file(resmon_obj->mdev.this_device, &dev_attr_system_io_mem_stat);
err_create_file_failed:
	device_remove_file(resmon_obj->mdev.this_device, &dev_attr_system_stat);
exit_misc_register_failed:
	misc_deregister(&resmon_obj->mdev);
exit_alloc_data_failed:
	kfree(resmon_obj);
	return ret;
}
module_init(resmon_init);

static void resmon_exit(void)
{
	resmon_task_stop();
	device_remove_file(resmon_obj->mdev.this_device, &dev_attr_system_io_mem_stat);
	device_remove_file(resmon_obj->mdev.this_device, &dev_attr_system_stat);
	sysfs_remove_group(&resmon_obj->mdev.this_device->kobj, &resmon_sysfs_parameters_grp);
	misc_deregister(&resmon_obj->mdev);
	kfree(resmon_obj);
}
module_exit(resmon_exit);
