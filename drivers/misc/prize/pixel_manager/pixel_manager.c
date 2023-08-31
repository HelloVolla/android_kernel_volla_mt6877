#define pr_fmt(fmt) "[pix_manager]" fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/poll.h>
#include <linux/bitmap.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <uapi/linux/sched/types.h>
#include <linux/sched_clock.h>
#include <linux/log2.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

struct pixel_data {
	int16_t pixelA;
	int16_t pixelR;
	int16_t pixelG;
	int16_t pixelB;
} g_pix_param;

static struct kobject *pix_manager_kobj;

static int string_is_digital(const char *str)
{
	int i = 0;

	if (str == NULL) {
		return -1;
	}

	while (str[i] != '\0') {
		if (str[i] < '0' || str[i] > '9') {
			return -1;
		}
		i++;
	}

	return 0;
}

static int pixel_atoi(const char *str)
{
	int i = 0;
	int ret_value = 0;

	if (str == NULL) {
		return -1;
	}

	while (str[i] != '\0') {
		ret_value = ret_value * 10 + (str[i] - '0');
		i++;
	}

	return ret_value;
}

static ssize_t sysfs_als_color_write(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	char *find_str1 = NULL;
	char *find_str2 = NULL;
	char *find_str3 = NULL;
	char *find_str4 = NULL;
	char tmp_buf[16] = {0};

	pr_info("pixel_manager_write:%s\n", buf);
	find_str1 = strstr(buf, "A:");
	find_str2 = strstr(buf, "R:");
	find_str3 = strstr(buf, "G:");
	find_str4 = strstr(buf, "B:");
	if (find_str1 != NULL && find_str2 != NULL && find_str2 - find_str1 > 2) {
		strncpy(tmp_buf, find_str1 + 2, find_str2 - find_str1 - 2);
		if (string_is_digital(tmp_buf) == 0) {
			g_pix_param.pixelA = pixel_atoi(tmp_buf);
		}
	}

	if (find_str2 != NULL && find_str3 != NULL && find_str3 - find_str2 > 2) {
		memset(tmp_buf, 0, sizeof(tmp_buf));
		strncpy(tmp_buf, find_str2 + 2, find_str3 - find_str2 - 2);
		if (string_is_digital(tmp_buf) == 0) {
			g_pix_param.pixelR = pixel_atoi(tmp_buf);
		}
	}

	if (find_str3 != NULL && find_str4 != NULL && find_str4 - find_str3 > 2) {
		memset(tmp_buf, 0, sizeof(tmp_buf));
		strncpy(tmp_buf, find_str3 + 2, find_str4 - find_str3 - 2);
		if (string_is_digital(tmp_buf) == 0) {
			g_pix_param.pixelG = pixel_atoi(tmp_buf);
		}
	}

	if (find_str4 != NULL) {
		memset(tmp_buf, 0, sizeof(tmp_buf));
		strcpy(tmp_buf, find_str4 + 2);
		if (string_is_digital(tmp_buf) == 0) {
			g_pix_param.pixelB = pixel_atoi(tmp_buf);
		}
	}
	return count;
}

static ssize_t sysfs_als_color_read(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "A:%dR:%dG:%dB:%d\n",
					g_pix_param.pixelA, g_pix_param.pixelR,
					g_pix_param.pixelG, g_pix_param.pixelB);
}

void get_pix_rgb(int16_t *R, int16_t *G, int16_t *B)
{
	if (R == NULL || G == NULL || B == NULL) {
		return;
	}

	sysfs_notify(pix_manager_kobj, NULL, "als_color");

	if (g_pix_param.pixelR > 255 || g_pix_param.pixelG > 255 || g_pix_param.pixelB > 255) {
		*R = -1;
		*G = -1;
		*B = -1;
		return;
	}

	*R = g_pix_param.pixelR;
	*G = g_pix_param.pixelG;
	*B = g_pix_param.pixelB;
	pr_info("get_pix_rgb %d:%d:%d\n", *R, *G, *B);
}
EXPORT_SYMBOL_GPL(get_pix_rgb);

void reset_pix_rgb(void)
{
	memset(&g_pix_param, -1 , sizeof(g_pix_param));
}
EXPORT_SYMBOL_GPL(reset_pix_rgb);

static struct kobj_attribute kobj_attr_als_color =
	__ATTR(als_color, 0644, sysfs_als_color_read, sysfs_als_color_write);

static struct attribute *pix_manager_fs_attrs[] = {
	&kobj_attr_als_color.attr,
	NULL,
};

static struct attribute_group pix_manager_fs_attrs_group = {
	.attrs = pix_manager_fs_attrs,
};

static int __init pixel_manager_init(void)
{
	int ret;

	pix_manager_kobj = kobject_create_and_add("pix_manager", kernel_kobj);
	if (!pix_manager_kobj)
		return -ENOMEM;

	ret = sysfs_create_group(pix_manager_kobj, &pix_manager_fs_attrs_group);
	if (ret) {
		pr_err("failed to create display device attributes");
		kobject_put(pix_manager_kobj);
	}

	memset(&g_pix_param, -1, sizeof(g_pix_param));

	return ret;
}

static void __exit pixel_manager_exit(void)
{
	kobject_put(pix_manager_kobj);
}

subsys_initcall(pixel_manager_init);
module_exit(pixel_manager_exit);

MODULE_DESCRIPTION("transfer rgb value");
MODULE_AUTHOR("Coosea");
MODULE_LICENSE("GPL v2");
