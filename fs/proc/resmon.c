/* Copyright Prize
 * Author: Eileen <lifenfen@szprize.com>
 * process table in kernel.
 */
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

#ifdef CONFIG_RESMON_DEBUG
char test[3096] = {0};
static int pos = 0;
#endif
#ifdef CONFIG_RESMON_KILL_DEBUG
extern void test_set_pid(int id, int pid);
static int count = 0;
#endif

#define P2M(x) ((((unsigned long)x) << (PAGE_SHIFT - 10)) >> (10))
#define RESMON_FILTER	"app"
#define SYSTEM_FILTER	"system_server"

static int show_resmon_stat(struct seq_file *m, void *v)
{
#ifdef CONFIG_RESMON_DEBUG
	int i = 0;
#endif
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
			seq_printf(m, "%d ", task->pid);
			seq_put_decimal_ull(m, " ", (long long unsigned int)nsec_to_clock_t(utime));
			seq_put_decimal_ull(m, " ", (long long unsigned int)nsec_to_clock_t(stime));
			seq_put_decimal_ull(m, " ", (long long unsigned int)mm_size);
			seq_put_decimal_ull(m, " ", read_bytes);
			seq_put_decimal_ull(m, " ", write_bytes);
			seq_printf(m, " %d\n", oom_adj);

#ifdef CONFIG_RESMON_DEBUG
			pos += sprintf(test + pos, "%d ", task->pid);
			pos += sprintf(test + pos, "%llu ", (long long unsigned int)nsec_to_clock_t(utime) );
			pos += sprintf(test + pos, "%llu ", (long long unsigned int)nsec_to_clock_t(stime) );
			pos += sprintf(test + pos, "%llu ", (long long unsigned int)mm_size);
			pos += sprintf(test + pos, "%llu ", read_bytes);
			pos += sprintf(test + pos, "%llu ", write_bytes);
			pos += sprintf(test + pos, "%d\n", oom_adj );
#endif
#ifdef CONFIG_RESMON_KILL_DEBUG
			test_set_pid(count ++, task->pid);
#endif
			unlock_task_sighand(task, &flags);
		}
	}
	rcu_read_unlock();

#ifdef CONFIG_RESMON_DEBUG
	printk("+ %s \n", __func__);
	for (i = 0; i < pos; i++)
		printk("%c", test[i]);
	printk("- %s \n", __func__);
	pos = 0;
#endif
#ifdef CONFIG_RESMON_KILL_DEBUG
	count = 0;
#endif
	return 0;
}

static int stat_open(struct inode *inode, struct file *file)
{
	size_t size = PAGE_SIZE * 3;

	return single_open_size(file, show_resmon_stat, NULL, size);
}

static const struct file_operations proc_stat_operations = {
	.open		= stat_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_resmon_init(void)
{
	proc_create("resmon_stat", 0, NULL, &proc_stat_operations);
	return 0;
}
fs_initcall(proc_resmon_init);
