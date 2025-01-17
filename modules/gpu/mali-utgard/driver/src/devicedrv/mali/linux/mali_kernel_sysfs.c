/**
 * Copyright (C) 2011-2016 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


/**
 * @file mali_kernel_sysfs.c
 * Implementation of some sysfs data exports
 */

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/aw/platform.h>
#include "mali_kernel_license.h"
#include "mali_kernel_common.h"
#include "mali_ukk.h"

#if MALI_LICENSE_IS_GPL

#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/mali/mali_utgard.h>
#include "mali_kernel_sysfs.h"
#if defined(CONFIG_MALI400_INTERNAL_PROFILING)
#include <linux/slab.h>
#include "mali_osk_profiling.h"
#endif

#include <linux/mali/mali_utgard.h>
#include "mali_pm.h"
#include "mali_pmu.h"
#include "mali_group.h"
#include "mali_gp.h"
#include "mali_pp.h"
#include "mali_l2_cache.h"
#include "mali_hw_core.h"
#include "mali_kernel_core.h"
#include "mali_user_settings_db.h"
#include "mali_profiling_internal.h"
#include "mali_gp_job.h"
#include "mali_pp_job.h"
#include "mali_executor.h"

#define PRIVATE_DATA_COUNTER_MAKE_GP(src) (src)
#define PRIVATE_DATA_COUNTER_MAKE_PP(src) ((1 << 24) | src)
#define PRIVATE_DATA_COUNTER_MAKE_PP_SUB_JOB(src, sub_job) ((1 << 24) | (1 << 16) | (sub_job << 8) | src)
#define PRIVATE_DATA_COUNTER_IS_PP(a) ((((a) >> 24) & 0xFF) ? MALI_TRUE : MALI_FALSE)
#define PRIVATE_DATA_COUNTER_GET_SRC(a) (a & 0xFF)
#define PRIVATE_DATA_COUNTER_IS_SUB_JOB(a) ((((a) >> 16) & 0xFF) ? MALI_TRUE : MALI_FALSE)
#define PRIVATE_DATA_COUNTER_GET_SUB_JOB(a) (((a) >> 8) & 0xFF)

#define POWER_BUFFER_SIZE 3

extern aw_private_data aw_private;
extern void set_freq_wrap(int freq);
extern void set_voltage(int vol);
extern void dvfs_change(u8 level);
extern void revise_current_level(void);

static struct dentry *mali_debugfs_dir = NULL;

typedef enum {
	_MALI_DEVICE_SUSPEND,
	_MALI_DEVICE_RESUME,
	_MALI_DEVICE_DVFS_PAUSE,
	_MALI_DEVICE_DVFS_RESUME,
	_MALI_MAX_EVENTS
} _mali_device_debug_power_events;

static const char *const mali_power_events[_MALI_MAX_EVENTS] = {
	[_MALI_DEVICE_SUSPEND] = "suspend",
	[_MALI_DEVICE_RESUME] = "resume",
	[_MALI_DEVICE_DVFS_PAUSE] = "dvfs_pause",
	[_MALI_DEVICE_DVFS_RESUME] = "dvfs_resume",
};

static mali_bool power_always_on_enabled = MALI_FALSE;

static int open_copy_private_data(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;
	return 0;
}

static ssize_t group_enabled_read(struct file *filp, char __user *buf, size_t count, loff_t *offp)
{
	int r;
	char buffer[64];
	struct mali_group *group;

	group = (struct mali_group *)filp->private_data;
	MALI_DEBUG_ASSERT_POINTER(group);

	r = snprintf(buffer, 64, "%d\n",
		     mali_executor_group_is_disabled(group) ? 0 : 1);

	return simple_read_from_buffer(buf, count, offp, buffer, r);
}

static ssize_t group_enabled_write(struct file *filp, const char __user *buf, size_t count, loff_t *offp)
{
	int r;
	char buffer[64];
	unsigned long val;
	struct mali_group *group;

	group = (struct mali_group *)filp->private_data;
	MALI_DEBUG_ASSERT_POINTER(group);

	if (count >= sizeof(buffer)) {
		return -ENOMEM;
	}

	if (copy_from_user(&buffer[0], buf, count)) {
		return -EFAULT;
	}
	buffer[count] = '\0';

	r = kstrtoul(&buffer[0], 10, &val);
	if (0 != r) {
		return -EINVAL;
	}

	switch (val) {
	case 1:
		mali_executor_group_enable(group);
		break;
	case 0:
		mali_executor_group_disable(group);
		break;
	default:
		return -EINVAL;
		break;
	}

	*offp += count;
	return count;
}

static const struct file_operations group_enabled_fops = {
	.owner = THIS_MODULE,
	.open  = open_copy_private_data,
	.read = group_enabled_read,
	.write = group_enabled_write,
};

static ssize_t hw_core_base_addr_read(struct file *filp, char __user *buf, size_t count, loff_t *offp)
{
	int r;
	char buffer[64];
	struct mali_hw_core *hw_core;

	hw_core = (struct mali_hw_core *)filp->private_data;
	MALI_DEBUG_ASSERT_POINTER(hw_core);

	r = snprintf(buffer, 64, "0x%lX\n", hw_core->phys_addr);

	return simple_read_from_buffer(buf, count, offp, buffer, r);
}

static const struct file_operations hw_core_base_addr_fops = {
	.owner = THIS_MODULE,
	.open  = open_copy_private_data,
	.read = hw_core_base_addr_read,
};

static ssize_t profiling_counter_src_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *ppos)
{
	u32 is_pp = PRIVATE_DATA_COUNTER_IS_PP((uintptr_t)filp->private_data);
	u32 src_id = PRIVATE_DATA_COUNTER_GET_SRC((uintptr_t)filp->private_data);
	mali_bool is_sub_job = PRIVATE_DATA_COUNTER_IS_SUB_JOB((uintptr_t)filp->private_data);
	u32 sub_job = PRIVATE_DATA_COUNTER_GET_SUB_JOB((uintptr_t)filp->private_data);
	char buf[64];
	int r;
	u32 val;

	if (MALI_TRUE == is_pp) {
		/* PP counter */
		if (MALI_TRUE == is_sub_job) {
			/* Get counter for a particular sub job */
			if (0 == src_id) {
				val = mali_pp_job_get_pp_counter_sub_job_src0(sub_job);
			} else {
				val = mali_pp_job_get_pp_counter_sub_job_src1(sub_job);
			}
		} else {
			/* Get default counter for all PP sub jobs */
			if (0 == src_id) {
				val = mali_pp_job_get_pp_counter_global_src0();
			} else {
				val = mali_pp_job_get_pp_counter_global_src1();
			}
		}
	} else {
		/* GP counter */
		if (0 == src_id) {
			val = mali_gp_job_get_gp_counter_src0();
		} else {
			val = mali_gp_job_get_gp_counter_src1();
		}
	}

	if (MALI_HW_CORE_NO_COUNTER == val) {
		r = snprintf(buf, 64, "-1\n");
	} else {
		r = snprintf(buf, 64, "%u\n", val);
	}

	return simple_read_from_buffer(ubuf, cnt, ppos, buf, r);
}

static ssize_t profiling_counter_src_write(struct file *filp, const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	u32 is_pp = PRIVATE_DATA_COUNTER_IS_PP((uintptr_t)filp->private_data);
	u32 src_id = PRIVATE_DATA_COUNTER_GET_SRC((uintptr_t)filp->private_data);
	mali_bool is_sub_job = PRIVATE_DATA_COUNTER_IS_SUB_JOB((uintptr_t)filp->private_data);
	u32 sub_job = PRIVATE_DATA_COUNTER_GET_SUB_JOB((uintptr_t)filp->private_data);
	char buf[64];
	long val;
	int ret;

	if (cnt >= sizeof(buf)) {
		return -EINVAL;
	}

	if (copy_from_user(&buf, ubuf, cnt)) {
		return -EFAULT;
	}

	buf[cnt] = 0;

	ret = kstrtol(buf, 10, &val);
	if (ret < 0) {
		return ret;
	}

	if (val < 0) {
		/* any negative input will disable counter */
		val = MALI_HW_CORE_NO_COUNTER;
	}

	if (MALI_TRUE == is_pp) {
		/* PP counter */
		if (MALI_TRUE == is_sub_job) {
			/* Set counter for a particular sub job */
			if (0 == src_id) {
				mali_pp_job_set_pp_counter_sub_job_src0(sub_job, (u32)val);
			} else {
				mali_pp_job_set_pp_counter_sub_job_src1(sub_job, (u32)val);
			}
		} else {
			/* Set default counter for all PP sub jobs */
			if (0 == src_id) {
				mali_pp_job_set_pp_counter_global_src0((u32)val);
			} else {
				mali_pp_job_set_pp_counter_global_src1((u32)val);
			}
		}
	} else {
		/* GP counter */
		if (0 == src_id) {
			mali_gp_job_set_gp_counter_src0((u32)val);
		} else {
			mali_gp_job_set_gp_counter_src1((u32)val);
		}
	}

	*ppos += cnt;
	return cnt;
}

static const struct file_operations profiling_counter_src_fops = {
	.owner = THIS_MODULE,
	.open  = open_copy_private_data,
	.read  = profiling_counter_src_read,
	.write = profiling_counter_src_write,
};

static ssize_t l2_l2x_counter_srcx_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *ppos, u32 src_id)
{
	char buf[64];
	int r;
	u32 val;
	struct mali_l2_cache_core *l2_core = (struct mali_l2_cache_core *)filp->private_data;

	if (0 == src_id) {
		val = mali_l2_cache_core_get_counter_src0(l2_core);
	} else {
		val = mali_l2_cache_core_get_counter_src1(l2_core);
	}

	if (MALI_HW_CORE_NO_COUNTER == val) {
		r = snprintf(buf, 64, "-1\n");
	} else {
		r = snprintf(buf, 64, "%u\n", val);
	}
	return simple_read_from_buffer(ubuf, cnt, ppos, buf, r);
}

static ssize_t l2_l2x_counter_srcx_write(struct file *filp, const char __user *ubuf, size_t cnt, loff_t *ppos, u32 src_id)
{
	struct mali_l2_cache_core *l2_core = (struct mali_l2_cache_core *)filp->private_data;
	char buf[64];
	long val;
	int ret;

	if (cnt >= sizeof(buf)) {
		return -EINVAL;
	}

	if (copy_from_user(&buf, ubuf, cnt)) {
		return -EFAULT;
	}

	buf[cnt] = 0;

	ret = kstrtol(buf, 10, &val);
	if (ret < 0) {
		return ret;
	}

	if (val < 0) {
		/* any negative input will disable counter */
		val = MALI_HW_CORE_NO_COUNTER;
	}

	mali_l2_cache_core_set_counter_src(l2_core, src_id, (u32)val);

	*ppos += cnt;
	return cnt;
}

static ssize_t l2_all_counter_srcx_write(struct file *filp, const char __user *ubuf, size_t cnt, loff_t *ppos, u32 src_id)
{
	char buf[64];
	long val;
	int ret;
	u32 l2_id;
	struct mali_l2_cache_core *l2_cache;

	if (cnt >= sizeof(buf)) {
		return -EINVAL;
	}

	if (copy_from_user(&buf, ubuf, cnt)) {
		return -EFAULT;
	}

	buf[cnt] = 0;

	ret = kstrtol(buf, 10, &val);
	if (ret < 0) {
		return ret;
	}

	if (val < 0) {
		/* any negative input will disable counter */
		val = MALI_HW_CORE_NO_COUNTER;
	}

	l2_id = 0;
	l2_cache = mali_l2_cache_core_get_glob_l2_core(l2_id);
	while (NULL != l2_cache) {
		mali_l2_cache_core_set_counter_src(l2_cache, src_id, (u32)val);

		/* try next L2 */
		l2_id++;
		l2_cache = mali_l2_cache_core_get_glob_l2_core(l2_id);
	}

	*ppos += cnt;
	return cnt;
}

static ssize_t l2_l2x_counter_src0_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *ppos)
{
	return l2_l2x_counter_srcx_read(filp, ubuf, cnt, ppos, 0);
}

static ssize_t l2_l2x_counter_src1_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *ppos)
{
	return l2_l2x_counter_srcx_read(filp, ubuf, cnt, ppos, 1);
}

static ssize_t l2_l2x_counter_src0_write(struct file *filp, const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	return l2_l2x_counter_srcx_write(filp, ubuf, cnt, ppos, 0);
}

static ssize_t l2_l2x_counter_src1_write(struct file *filp, const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	return l2_l2x_counter_srcx_write(filp, ubuf, cnt, ppos, 1);
}

static ssize_t l2_all_counter_src0_write(struct file *filp, const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	return l2_all_counter_srcx_write(filp, ubuf, cnt, ppos, 0);
}

static ssize_t l2_all_counter_src1_write(struct file *filp, const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	return l2_all_counter_srcx_write(filp, ubuf, cnt, ppos, 1);
}

static const struct file_operations l2_l2x_counter_src0_fops = {
	.owner = THIS_MODULE,
	.open  = open_copy_private_data,
	.read  = l2_l2x_counter_src0_read,
	.write = l2_l2x_counter_src0_write,
};

static const struct file_operations l2_l2x_counter_src1_fops = {
	.owner = THIS_MODULE,
	.open  = open_copy_private_data,
	.read  = l2_l2x_counter_src1_read,
	.write = l2_l2x_counter_src1_write,
};

static const struct file_operations l2_all_counter_src0_fops = {
	.owner = THIS_MODULE,
	.write = l2_all_counter_src0_write,
};

static const struct file_operations l2_all_counter_src1_fops = {
	.owner = THIS_MODULE,
	.write = l2_all_counter_src1_write,
};

static ssize_t l2_l2x_counter_valx_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *ppos, u32 src_id)
{
	char buf[64];
	int r;
	u32 src0 = 0;
	u32 val0 = 0;
	u32 src1 = 0;
	u32 val1 = 0;
	u32 val = -1;
	struct mali_l2_cache_core *l2_core = (struct mali_l2_cache_core *)filp->private_data;

	mali_l2_cache_core_get_counter_values(l2_core, &src0, &val0, &src1, &val1);

	if (0 == src_id) {
		if (MALI_HW_CORE_NO_COUNTER != val0) {
			val = val0;
		}
	} else {
		if (MALI_HW_CORE_NO_COUNTER != val1) {
			val = val1;
		}
	}

	r = snprintf(buf, 64, "%u\n", val);

	return simple_read_from_buffer(ubuf, cnt, ppos, buf, r);
}

static ssize_t l2_l2x_counter_val0_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *ppos)
{
	return l2_l2x_counter_valx_read(filp, ubuf, cnt, ppos, 0);
}

static ssize_t l2_l2x_counter_val1_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *ppos)
{
	return l2_l2x_counter_valx_read(filp, ubuf, cnt, ppos, 1);
}

static const struct file_operations l2_l2x_counter_val0_fops = {
	.owner = THIS_MODULE,
	.open  = open_copy_private_data,
	.read  = l2_l2x_counter_val0_read,
};

static const struct file_operations l2_l2x_counter_val1_fops = {
	.owner = THIS_MODULE,
	.open  = open_copy_private_data,
	.read  = l2_l2x_counter_val1_read,
};

static ssize_t power_always_on_write(struct file *filp, const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	unsigned long val;
	int ret;
	char buf[32];

	cnt = min(cnt, sizeof(buf) - 1);
	if (copy_from_user(buf, ubuf, cnt)) {
		return -EFAULT;
	}
	buf[cnt] = '\0';

	ret = kstrtoul(buf, 10, &val);
	if (0 != ret) {
		return ret;
	}

	/* Update setting (not exactly thread safe) */
	if (1 == val && MALI_FALSE == power_always_on_enabled) {
		power_always_on_enabled = MALI_TRUE;
		_mali_osk_pm_dev_ref_get_sync();
	} else if (0 == val && MALI_TRUE == power_always_on_enabled) {
		power_always_on_enabled = MALI_FALSE;
		_mali_osk_pm_dev_ref_put();
	}

	*ppos += cnt;
	return cnt;
}

static ssize_t power_always_on_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *ppos)
{
	if (MALI_TRUE == power_always_on_enabled) {
		return simple_read_from_buffer(ubuf, cnt, ppos, "1\n", 2);
	} else {
		return simple_read_from_buffer(ubuf, cnt, ppos, "0\n", 2);
	}
}

static const struct file_operations power_always_on_fops = {
	.owner = THIS_MODULE,
	.read  = power_always_on_read,
	.write = power_always_on_write,
};

static ssize_t power_power_events_write(struct file *filp, const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	if (!strncmp(ubuf, mali_power_events[_MALI_DEVICE_SUSPEND], strlen(mali_power_events[_MALI_DEVICE_SUSPEND]) - 1)) {
		mali_pm_os_suspend(MALI_TRUE);
	} else if (!strncmp(ubuf, mali_power_events[_MALI_DEVICE_RESUME], strlen(mali_power_events[_MALI_DEVICE_RESUME]) - 1)) {
		mali_pm_os_resume();
	} else if (!strncmp(ubuf, mali_power_events[_MALI_DEVICE_DVFS_PAUSE], strlen(mali_power_events[_MALI_DEVICE_DVFS_PAUSE]) - 1)) {
		mali_dev_pause();
	} else if (!strncmp(ubuf, mali_power_events[_MALI_DEVICE_DVFS_RESUME], strlen(mali_power_events[_MALI_DEVICE_DVFS_RESUME]) - 1)) {
		mali_dev_resume();
	}
	*ppos += cnt;
	return cnt;
}

static loff_t power_power_events_seek(struct file *file, loff_t offset, int orig)
{
	file->f_pos = offset;
	return 0;
}

static const struct file_operations power_power_events_fops = {
	.owner = THIS_MODULE,
	.write = power_power_events_write,
	.llseek = power_power_events_seek,
};

#if MALI_STATE_TRACKING
static int mali_seq_internal_state_show(struct seq_file *seq_file, void *v)
{
	u32 len = 0;
	u32 size;
	char *buf;

	size = seq_get_buf(seq_file, &buf);

	if (!size) {
		return -ENOMEM;
	}

	/* Create the internal state dump. */
	len  = snprintf(buf + len, size - len, "Mali device driver %s\n", SVN_REV_STRING);
	len += snprintf(buf + len, size - len, "License: %s\n\n", MALI_KERNEL_LINUX_LICENSE);

	len += _mali_kernel_core_dump_state(buf + len, size - len);

	seq_commit(seq_file, len);

	return 0;
}

static int mali_seq_internal_state_open(struct inode *inode, struct file *file)
{
	return single_open(file, mali_seq_internal_state_show, NULL);
}

static const struct file_operations mali_seq_internal_state_fops = {
	.owner = THIS_MODULE,
	.open = mali_seq_internal_state_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif /* MALI_STATE_TRACKING */

#if defined(CONFIG_MALI400_INTERNAL_PROFILING)
static ssize_t profiling_record_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char buf[64];
	int r;

	r = snprintf(buf, 64, "%d\n", _mali_internal_profiling_is_recording() ? 1 : 0);
	return simple_read_from_buffer(ubuf, cnt, ppos, buf, r);
}

static ssize_t profiling_record_write(struct file *filp, const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char buf[64];
	unsigned long val;
	int ret;

	if (cnt >= sizeof(buf)) {
		return -EINVAL;
	}

	if (copy_from_user(&buf, ubuf, cnt)) {
		return -EFAULT;
	}

	buf[cnt] = 0;

	ret = kstrtoul(buf, 10, &val);
	if (ret < 0) {
		return ret;
	}

	if (val != 0) {
		u32 limit = MALI_PROFILING_MAX_BUFFER_ENTRIES; /* This can be made configurable at a later stage if we need to */

		/* check if we are already recording */
		if (MALI_TRUE == _mali_internal_profiling_is_recording()) {
			MALI_DEBUG_PRINT(3, ("Recording of profiling events already in progress\n"));
			return -EFAULT;
		}

		/* check if we need to clear out an old recording first */
		if (MALI_TRUE == _mali_internal_profiling_have_recording()) {
			if (_MALI_OSK_ERR_OK != _mali_internal_profiling_clear()) {
				MALI_DEBUG_PRINT(3, ("Failed to clear existing recording of profiling events\n"));
				return -EFAULT;
			}
		}

		/* start recording profiling data */
		if (_MALI_OSK_ERR_OK != _mali_internal_profiling_start(&limit)) {
			MALI_DEBUG_PRINT(3, ("Failed to start recording of profiling events\n"));
			return -EFAULT;
		}

		MALI_DEBUG_PRINT(3, ("Profiling recording started (max %u events)\n", limit));
	} else {
		/* stop recording profiling data */
		u32 count = 0;
		if (_MALI_OSK_ERR_OK != _mali_internal_profiling_stop(&count)) {
			MALI_DEBUG_PRINT(2, ("Failed to stop recording of profiling events\n"));
			return -EFAULT;
		}

		MALI_DEBUG_PRINT(2, ("Profiling recording stopped (recorded %u events)\n", count));
	}

	*ppos += cnt;
	return cnt;
}

static const struct file_operations profiling_record_fops = {
	.owner = THIS_MODULE,
	.read  = profiling_record_read,
	.write = profiling_record_write,
};

static void *profiling_events_start(struct seq_file *s, loff_t *pos)
{
	loff_t *spos;

	/* check if we have data avaiable */
	if (MALI_TRUE != _mali_internal_profiling_have_recording()) {
		return NULL;
	}

	spos = kmalloc(sizeof(loff_t), GFP_KERNEL);
	if (NULL == spos) {
		return NULL;
	}

	*spos = *pos;
	return spos;
}

static void *profiling_events_next(struct seq_file *s, void *v, loff_t *pos)
{
	loff_t *spos = v;

	/* check if we have data avaiable */
	if (MALI_TRUE != _mali_internal_profiling_have_recording()) {
		return NULL;
	}

	/* check if the next entry actually is avaiable */
	if (_mali_internal_profiling_get_count() <= (u32)(*spos + 1)) {
		return NULL;
	}

	*pos = ++*spos;
	return spos;
}

static void profiling_events_stop(struct seq_file *s, void *v)
{
	kfree(v);
}

static int profiling_events_show(struct seq_file *seq_file, void *v)
{
	loff_t *spos = v;
	u32 index;
	u64 timestamp;
	u32 event_id;
	u32 data[5];

	index = (u32) * spos;

	/* Retrieve all events */
	if (_MALI_OSK_ERR_OK == _mali_internal_profiling_get_event(index, &timestamp, &event_id, data)) {
		seq_printf(seq_file, "%llu %u %u %u %u %u %u\n", timestamp, event_id, data[0], data[1], data[2], data[3], data[4]);
		return 0;
	}

	return 0;
}

static int profiling_events_show_human_readable(struct seq_file *seq_file, void *v)
{
#define MALI_EVENT_ID_IS_HW(event_id) (((event_id & 0x00FF0000) >= MALI_PROFILING_EVENT_CHANNEL_GP0) && ((event_id & 0x00FF0000) <= MALI_PROFILING_EVENT_CHANNEL_PP7))

	static u64 start_time = 0;
	loff_t *spos = v;
	u32 index;
	u64 timestamp;
	u32 event_id;
	u32 data[5];

	index = (u32) * spos;

	/* Retrieve all events */
	if (_MALI_OSK_ERR_OK == _mali_internal_profiling_get_event(index, &timestamp, &event_id, data)) {
		seq_printf(seq_file, "%llu %u %u %u %u %u %u # ", timestamp, event_id, data[0], data[1], data[2], data[3], data[4]);

		if (0 == index) {
			start_time = timestamp;
		}

		seq_printf(seq_file, "[%06u] ", index);

		switch (event_id & 0x0F000000) {
		case MALI_PROFILING_EVENT_TYPE_SINGLE:
			seq_printf(seq_file, "SINGLE | ");
			break;
		case MALI_PROFILING_EVENT_TYPE_START:
			seq_printf(seq_file, "START | ");
			break;
		case MALI_PROFILING_EVENT_TYPE_STOP:
			seq_printf(seq_file, "STOP | ");
			break;
		case MALI_PROFILING_EVENT_TYPE_SUSPEND:
			seq_printf(seq_file, "SUSPEND | ");
			break;
		case MALI_PROFILING_EVENT_TYPE_RESUME:
			seq_printf(seq_file, "RESUME | ");
			break;
		default:
			seq_printf(seq_file, "0x%01X | ", (event_id & 0x0F000000) >> 24);
			break;
		}

		switch (event_id & 0x00FF0000) {
		case MALI_PROFILING_EVENT_CHANNEL_SOFTWARE:
			seq_printf(seq_file, "SW | ");
			break;
		case MALI_PROFILING_EVENT_CHANNEL_GP0:
			seq_printf(seq_file, "GP0 | ");
			break;
		case MALI_PROFILING_EVENT_CHANNEL_PP0:
			seq_printf(seq_file, "PP0 | ");
			break;
		case MALI_PROFILING_EVENT_CHANNEL_PP1:
			seq_printf(seq_file, "PP1 | ");
			break;
		case MALI_PROFILING_EVENT_CHANNEL_PP2:
			seq_printf(seq_file, "PP2 | ");
			break;
		case MALI_PROFILING_EVENT_CHANNEL_PP3:
			seq_printf(seq_file, "PP3 | ");
			break;
		case MALI_PROFILING_EVENT_CHANNEL_PP4:
			seq_printf(seq_file, "PP4 | ");
			break;
		case MALI_PROFILING_EVENT_CHANNEL_PP5:
			seq_printf(seq_file, "PP5 | ");
			break;
		case MALI_PROFILING_EVENT_CHANNEL_PP6:
			seq_printf(seq_file, "PP6 | ");
			break;
		case MALI_PROFILING_EVENT_CHANNEL_PP7:
			seq_printf(seq_file, "PP7 | ");
			break;
		case MALI_PROFILING_EVENT_CHANNEL_GPU:
			seq_printf(seq_file, "GPU | ");
			break;
		default:
			seq_printf(seq_file, "0x%02X | ", (event_id & 0x00FF0000) >> 16);
			break;
		}

		if (MALI_EVENT_ID_IS_HW(event_id)) {
			if (((event_id & 0x0F000000) == MALI_PROFILING_EVENT_TYPE_START) || ((event_id & 0x0F000000) == MALI_PROFILING_EVENT_TYPE_STOP)) {
				switch (event_id & 0x0000FFFF) {
				case MALI_PROFILING_EVENT_REASON_START_STOP_HW_PHYSICAL:
					seq_printf(seq_file, "PHYSICAL | ");
					break;
				case MALI_PROFILING_EVENT_REASON_START_STOP_HW_VIRTUAL:
					seq_printf(seq_file, "VIRTUAL | ");
					break;
				default:
					seq_printf(seq_file, "0x%04X | ", event_id & 0x0000FFFF);
					break;
				}
			} else {
				seq_printf(seq_file, "0x%04X | ", event_id & 0x0000FFFF);
			}
		} else {
			seq_printf(seq_file, "0x%04X | ", event_id & 0x0000FFFF);
		}

		seq_printf(seq_file, "T0 + 0x%016llX\n", timestamp - start_time);

		return 0;
	}

	return 0;
}

static const struct seq_operations profiling_events_seq_ops = {
	.start = profiling_events_start,
	.next  = profiling_events_next,
	.stop  = profiling_events_stop,
	.show  = profiling_events_show
};

static int profiling_events_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &profiling_events_seq_ops);
}

static const struct file_operations profiling_events_fops = {
	.owner = THIS_MODULE,
	.open = profiling_events_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static const struct seq_operations profiling_events_human_readable_seq_ops = {
	.start = profiling_events_start,
	.next  = profiling_events_next,
	.stop  = profiling_events_stop,
	.show  = profiling_events_show_human_readable
};

static int profiling_events_human_readable_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &profiling_events_human_readable_seq_ops);
}

static const struct file_operations profiling_events_human_readable_fops = {
	.owner = THIS_MODULE,
	.open = profiling_events_human_readable_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

#endif

static int memory_debugfs_show(struct seq_file *s, void *private_data)
{
#ifdef MALI_MEM_SWAP_TRACKING
	seq_printf(s, "  %-25s  %-10s  %-10s  %-15s  %-15s  %-10s  %-10s %-10s \n"\
		   "=================================================================================================================================\n",
		   "Name (:bytes)", "pid", "mali_mem", "max_mali_mem",
		   "external_mem", "ump_mem", "dma_mem", "swap_mem");
#else
	seq_printf(s, "  %-25s  %-10s  %-10s  %-15s  %-15s  %-10s  %-10s \n"\
		   "========================================================================================================================\n",
		   "Name (:bytes)", "pid", "mali_mem", "max_mali_mem",
		   "external_mem", "ump_mem", "dma_mem");
#endif
	mali_session_memory_tracking(s);
	return 0;
}

static int memory_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, memory_debugfs_show, inode->i_private);
}

static const struct file_operations memory_usage_fops = {
	.owner = THIS_MODULE,
	.open = memory_debugfs_open,
	.read  = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static ssize_t utilization_gp_pp_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char buf[64];
	size_t r;
	u32 uval = _mali_ukk_utilization_gp_pp();

	r = snprintf(buf, 64, "%u%%\n", uval*100/256);
	return simple_read_from_buffer(ubuf, cnt, ppos, buf, r);
}

static ssize_t utilization_gp_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char buf[64];
	size_t r;
	u32 uval = _mali_ukk_utilization_gp();

	r = snprintf(buf, 64, "%u%%\n", uval*100/256);
	return simple_read_from_buffer(ubuf, cnt, ppos, buf, r);
}

static ssize_t utilization_pp_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char buf[64];
	size_t r;
	u32 uval = _mali_ukk_utilization_pp();

	r = snprintf(buf, 64, "%u%%\n", uval*100/256);
	return simple_read_from_buffer(ubuf, cnt, ppos, buf, r);
}


static const struct file_operations utilization_gp_pp_fops = {
	.owner = THIS_MODULE,
	.read = utilization_gp_pp_read,
};

static const struct file_operations utilization_gp_fops = {
	.owner = THIS_MODULE,
	.read = utilization_gp_read,
};

static const struct file_operations utilization_pp_fops = {
	.owner = THIS_MODULE,
	.read = utilization_pp_read,
};

static ssize_t user_settings_write(struct file *filp, const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	unsigned long val;
	int ret;
	_mali_uk_user_setting_t setting;
	char buf[32];

	cnt = min(cnt, sizeof(buf) - 1);
	if (copy_from_user(buf, ubuf, cnt)) {
		return -EFAULT;
	}
	buf[cnt] = '\0';

	ret = kstrtoul(buf, 10, &val);
	if (0 != ret) {
		return ret;
	}

	/* Update setting */
	setting = (_mali_uk_user_setting_t)(filp->private_data);
	mali_set_user_setting(setting, val);

	*ppos += cnt;
	return cnt;
}

static ssize_t user_settings_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char buf[64];
	size_t r;
	u32 value;
	_mali_uk_user_setting_t setting;

	setting = (_mali_uk_user_setting_t)(filp->private_data);
	value = mali_get_user_setting(setting);

	r = snprintf(buf, 64, "%u\n", value);
	return simple_read_from_buffer(ubuf, cnt, ppos, buf, r);
}

static const struct file_operations user_settings_fops = {
	.owner = THIS_MODULE,
	.open = open_copy_private_data,
	.read = user_settings_read,
	.write = user_settings_write,
};

static int mali_sysfs_user_settings_register(void)
{
	struct dentry *mali_user_settings_dir = debugfs_create_dir("userspace_settings", mali_debugfs_dir);

	if (mali_user_settings_dir != NULL) {
		long i;
		for (i = 0; i < _MALI_UK_USER_SETTING_MAX; i++) {
			debugfs_create_file(_mali_uk_user_setting_descriptions[i],
					    0600, mali_user_settings_dir, (void *)i,
					    &user_settings_fops);
		}
	}

	return 0;
}

static ssize_t pp_num_cores_enabled_write(struct file *filp, const char __user *buf, size_t count, loff_t *offp)
{
	int ret;
	char buffer[32];
	unsigned long val;

	if (count >= sizeof(buffer)) {
		return -ENOMEM;
	}

	if (copy_from_user(&buffer[0], buf, count)) {
		return -EFAULT;
	}
	buffer[count] = '\0';

	ret = kstrtoul(&buffer[0], 10, &val);
	if (0 != ret) {
		return -EINVAL;
	}

	ret = mali_executor_set_perf_level(val, MALI_TRUE); /* override even if core scaling is disabled */
	if (ret) {
		return ret;
	}

	*offp += count;
	return count;
}

static ssize_t pp_num_cores_enabled_read(struct file *filp, char __user *buf, size_t count, loff_t *offp)
{
	int r;
	char buffer[64];

	r = snprintf(buffer, 64, "%u\n", mali_executor_get_num_cores_enabled());

	return simple_read_from_buffer(buf, count, offp, buffer, r);
}

static const struct file_operations pp_num_cores_enabled_fops = {
	.owner = THIS_MODULE,
	.write = pp_num_cores_enabled_write,
	.read = pp_num_cores_enabled_read,
	.llseek = default_llseek,
};

static ssize_t pp_num_cores_total_read(struct file *filp, char __user *buf, size_t count, loff_t *offp)
{
	int r;
	char buffer[64];

	r = snprintf(buffer, 64, "%u\n", mali_executor_get_num_cores_total());

	return simple_read_from_buffer(buf, count, offp, buffer, r);
}

static const struct file_operations pp_num_cores_total_fops = {
	.owner = THIS_MODULE,
	.read = pp_num_cores_total_read,
};

static ssize_t pp_core_scaling_enabled_write(struct file *filp, const char __user *buf, size_t count, loff_t *offp)
{
	int ret;
	char buffer[32];
	unsigned long val;

	if (count >= sizeof(buffer)) {
		return -ENOMEM;
	}

	if (copy_from_user(&buffer[0], buf, count)) {
		return -EFAULT;
	}
	buffer[count] = '\0';

	ret = kstrtoul(&buffer[0], 10, &val);
	if (0 != ret) {
		return -EINVAL;
	}

	switch (val) {
	case 1:
		mali_executor_core_scaling_enable();
		break;
	case 0:
		mali_executor_core_scaling_disable();
		break;
	default:
		return -EINVAL;
		break;
	}

	*offp += count;
	return count;
}

static ssize_t pp_core_scaling_enabled_read(struct file *filp, char __user *buf, size_t count, loff_t *offp)
{
	return simple_read_from_buffer(buf, count, offp, mali_executor_core_scaling_is_enabled() ? "1\n" : "0\n", 2);
}
static const struct file_operations pp_core_scaling_enabled_fops = {
	.owner = THIS_MODULE,
	.write = pp_core_scaling_enabled_write,
	.read = pp_core_scaling_enabled_read,
	.llseek = default_llseek,
};

static ssize_t version_read(struct file *filp, char __user *buf, size_t count, loff_t *offp)
{
	int r = 0;
	char buffer[64];

	switch (mali_kernel_core_get_product_id()) {
	case _MALI_PRODUCT_ID_MALI200:
		r = snprintf(buffer, 64, "Mali-200\n");
		break;
	case _MALI_PRODUCT_ID_MALI300:
		r = snprintf(buffer, 64, "Mali-300\n");
		break;
	case _MALI_PRODUCT_ID_MALI400:
		r = snprintf(buffer, 64, "Mali-400 MP\n");
		break;
	case _MALI_PRODUCT_ID_MALI450:
		r = snprintf(buffer, 64, "Mali-450 MP\n");
		break;
	case _MALI_PRODUCT_ID_MALI470:
		r = snprintf(buffer, 64, "Mali-470 MP\n");
		break;
	case _MALI_PRODUCT_ID_UNKNOWN:
		return -EINVAL;
		break;
	};

	return simple_read_from_buffer(buf, count, offp, buffer, r);
}

static const struct file_operations version_fops = {
	.owner = THIS_MODULE,
	.read = version_read,
};

#if defined(DEBUG)
static int timeline_debugfs_show(struct seq_file *s, void *private_data)
{
	struct mali_session_data *session, *tmp;
	u32 session_seq = 1;

	seq_printf(s, "timeline system info: \n=================\n\n");

	mali_session_lock();
	MALI_SESSION_FOREACH(session, tmp, link) {
		seq_printf(s, "session %d <%p> start:\n", session_seq, session);
		mali_timeline_debug_print_system(session->timeline_system, s);
		seq_printf(s, "session %d end\n\n\n", session_seq++);
	}
	mali_session_unlock();

	return 0;
}

static int timeline_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, timeline_debugfs_show, inode->i_private);
}

static const struct file_operations timeline_dump_fops = {
	.owner = THIS_MODULE,
	.open = timeline_debugfs_open,
	.read  = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};
#endif

static int get_value(char *cmd, char *buf, int cmd_size, int total_size)
{
	char data[5];
	unsigned long val;
	int i, j;

	if (!strncmp(buf, cmd, cmd_size)) {
		for (i = 0; i < total_size; i++) {
			if (*(buf+i) == ':') {
				for (j = 0; j < total_size - i - 1; j++)
					data[j] = *(buf + i + 1 + j);
				data[j] = '\0';
				break;
			}
		}

		if (kstrtoul(data, 10, &val)) {
			MALI_PRINT_ERROR(("Invalid parameter!\n"));
			return -1;
		} else {
			return val;
		}
	}

	return -1;
}

static ssize_t write_write(struct file *filp, const char __user *buf, size_t count, loff_t *offp)
{
	int val, i, token = 0;
	char buffer[100];

	if (count >= sizeof(buffer))
		return -ENOMEM;

	if (copy_from_user(buffer, buf, count))
		return -EFAULT;

	buffer[count] = '\0';

	for (i = 0; i < count; i++) {
		if (*(buffer+i) == ';') {
			val = get_value("enable", buffer + token, 6, i - token);
			if (val == 0 || val == 1) {
				aw_private.debug.enable = val;
				break;
			}

			val = get_value("frequency", buffer + token, 9, i - token);
			if (val >= 0) {
				if (val == 0 || val == 1) {
					aw_private.debug.frequency = val;
				} else {
					set_freq_wrap(val);

					revise_current_level();
				}
				break;
			}

			val = get_value("voltage", buffer + token, 7, i - token);
			if (val >= 0) {
				if (val == 0 || val == 1)
					aw_private.debug.voltage = val;
				else
					set_voltage(val);
				break;
			}

			val = get_value("tempctrl", buffer + token, 8, i - token);
			if (val >= 0 && val <= 3) {
				if (val == 0 || val == 1)
					aw_private.debug.tempctrl = val;
				else
					aw_private.tempctrl.temp_ctrl_status = val - 2;
				break;
			}

			val = get_value("dvfs", buffer + token, 4, i - token);
			if (val >= 0 && val <= 3) {
				if (val == 0 || val == 1)
					aw_private.debug.dvfs = val;
				else
					aw_private.pm.dvfs_status = val - 2;
				break;
			}

			val = get_value("level", buffer + token, 5, i - token);
			if (val == 0 || val == 1) {
				aw_private.debug.level = val;
				break;
			}

			token = i + 1;
		}
	}

	return count;
}

static const struct file_operations write_fops = {
	.owner = THIS_MODULE,
	.write = write_write,
};

static int dump_debugfs_show(struct seq_file *s, void *private_data)
{
	int i;

	if (aw_private.debug.enable) {
		if (aw_private.debug.frequency)
			seq_printf(s, "frequency: %3dMHz; ", aw_private.pm.clk[2].freq);
		if (aw_private.debug.voltage)
			seq_printf(s, "voltage: %4dmV; ", aw_private.pm.current_vol);
		if (aw_private.debug.tempctrl)
			seq_printf(s, "tempctrl: %s; ", aw_private.tempctrl.temp_ctrl_status ? "on" : "off");
		if (aw_private.debug.dvfs)
			seq_printf(s, "dvfs: %s; ", aw_private.pm.dvfs_status ? "on" : "off");
		if (aw_private.debug.level) {
			seq_printf(s, "\nmax_available_level: %d\n", aw_private.pm.max_available_level);
			seq_printf(s, "current_level: %d\n", aw_private.pm.current_level);
			seq_printf(s, "cool_freq: %dMHz\n", aw_private.pm.cool_freq);
			seq_printf(s, "scene_ctrl_cmd: %d\n", aw_private.pm.scene_ctrl_cmd);
			seq_printf(s, "   level  voltage  frequency\n");
			for (i = 0; i <= aw_private.pm.max_level; i++)
				seq_printf(s, "%s%3d   %4dmV      %3dMHz\n", i == aw_private.pm.current_level ? "-> " : "   ",
							i, aw_private.pm.vf_table[i].vol, aw_private.pm.vf_table[i].freq);
			seq_printf(s, "=========================================");
		}
		seq_printf(s, "\n");
	}

	return 0;
}

static int dump_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_debugfs_show, inode->i_private);
}

static const struct file_operations dump_fops = {
	.owner = THIS_MODULE,
	.open = dump_debugfs_open,
	.read  = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int mali_sysfs_register(const char *mali_dev_name)
{
	mali_debugfs_dir = debugfs_create_dir(mali_dev_name, NULL);
	if (ERR_PTR(-ENODEV) == mali_debugfs_dir) {
		/* Debugfs not supported. */
		mali_debugfs_dir = NULL;
	} else {
		if (NULL != mali_debugfs_dir) {
			/* Debugfs directory created successfully; create files now */
			struct dentry *mali_power_dir;
			struct dentry *mali_gp_dir;
			struct dentry *mali_pp_dir;
			struct dentry *mali_l2_dir;
			struct dentry *mali_profiling_dir;

			debugfs_create_file("version", 0400, mali_debugfs_dir, NULL, &version_fops);

			mali_power_dir = debugfs_create_dir("power", mali_debugfs_dir);
			if (mali_power_dir != NULL) {
				debugfs_create_file("always_on", 0600, mali_power_dir, NULL, &power_always_on_fops);
				debugfs_create_file("power_events", 0200, mali_power_dir, NULL, &power_power_events_fops);
			}

			mali_gp_dir = debugfs_create_dir("gp", mali_debugfs_dir);
			if (mali_gp_dir != NULL) {
				u32 num_groups;
				long i;

				num_groups = mali_group_get_glob_num_groups();
				for (i = 0; i < num_groups; i++) {
					struct mali_group *group = mali_group_get_glob_group(i);

					struct mali_gp_core *gp_core = mali_group_get_gp_core(group);
					if (NULL != gp_core) {
						struct dentry *mali_gp_gpx_dir;
						mali_gp_gpx_dir = debugfs_create_dir("gp0", mali_gp_dir);
						if (NULL != mali_gp_gpx_dir) {
							debugfs_create_file("base_addr", 0400, mali_gp_gpx_dir, &gp_core->hw_core, &hw_core_base_addr_fops);
							debugfs_create_file("enabled", 0600, mali_gp_gpx_dir, group, &group_enabled_fops);
						}
						break; /* no need to look for any other GP cores */
					}

				}
			}

			mali_pp_dir = debugfs_create_dir("pp", mali_debugfs_dir);
			if (mali_pp_dir != NULL) {
				u32 num_groups;
				long i;

				debugfs_create_file("num_cores_total", 0400, mali_pp_dir, NULL, &pp_num_cores_total_fops);
				debugfs_create_file("num_cores_enabled", 0600, mali_pp_dir, NULL, &pp_num_cores_enabled_fops);
				debugfs_create_file("core_scaling_enabled", 0600, mali_pp_dir, NULL, &pp_core_scaling_enabled_fops);

				num_groups = mali_group_get_glob_num_groups();
				for (i = 0; i < num_groups; i++) {
					struct mali_group *group = mali_group_get_glob_group(i);

					struct mali_pp_core *pp_core = mali_group_get_pp_core(group);
					if (NULL != pp_core) {
						char buf[16];
						struct dentry *mali_pp_ppx_dir;
						_mali_osk_snprintf(buf, sizeof(buf), "pp%u", mali_pp_core_get_id(pp_core));
						mali_pp_ppx_dir = debugfs_create_dir(buf, mali_pp_dir);
						if (NULL != mali_pp_ppx_dir) {
							debugfs_create_file("base_addr", 0400, mali_pp_ppx_dir, &pp_core->hw_core, &hw_core_base_addr_fops);
							if (!mali_group_is_virtual(group)) {
								debugfs_create_file("enabled", 0600, mali_pp_ppx_dir, group, &group_enabled_fops);
							}
						}
					}
				}
			}

			mali_l2_dir = debugfs_create_dir("l2", mali_debugfs_dir);
			if (mali_l2_dir != NULL) {
				struct dentry *mali_l2_all_dir;
				u32 l2_id;
				struct mali_l2_cache_core *l2_cache;

				mali_l2_all_dir = debugfs_create_dir("all", mali_l2_dir);
				if (mali_l2_all_dir != NULL) {
					debugfs_create_file("counter_src0", 0200, mali_l2_all_dir, NULL, &l2_all_counter_src0_fops);
					debugfs_create_file("counter_src1", 0200, mali_l2_all_dir, NULL, &l2_all_counter_src1_fops);
				}

				l2_id = 0;
				l2_cache = mali_l2_cache_core_get_glob_l2_core(l2_id);
				while (NULL != l2_cache) {
					char buf[16];
					struct dentry *mali_l2_l2x_dir;
					_mali_osk_snprintf(buf, sizeof(buf), "l2%u", l2_id);
					mali_l2_l2x_dir = debugfs_create_dir(buf, mali_l2_dir);
					if (NULL != mali_l2_l2x_dir) {
						debugfs_create_file("counter_src0", 0600, mali_l2_l2x_dir, l2_cache, &l2_l2x_counter_src0_fops);
						debugfs_create_file("counter_src1", 0600, mali_l2_l2x_dir, l2_cache, &l2_l2x_counter_src1_fops);
						debugfs_create_file("counter_val0", 0600, mali_l2_l2x_dir, l2_cache, &l2_l2x_counter_val0_fops);
						debugfs_create_file("counter_val1", 0600, mali_l2_l2x_dir, l2_cache, &l2_l2x_counter_val1_fops);
						debugfs_create_file("base_addr", 0400, mali_l2_l2x_dir, &l2_cache->hw_core, &hw_core_base_addr_fops);
					}

					/* try next L2 */
					l2_id++;
					l2_cache = mali_l2_cache_core_get_glob_l2_core(l2_id);
				}
			}

			debugfs_create_file("gpu_memory", 0444, mali_debugfs_dir, NULL, &memory_usage_fops);

			debugfs_create_file("utilization_gp_pp", 0400, mali_debugfs_dir, NULL, &utilization_gp_pp_fops);
			debugfs_create_file("utilization_gp", 0400, mali_debugfs_dir, NULL, &utilization_gp_fops);
			debugfs_create_file("utilization_pp", 0400, mali_debugfs_dir, NULL, &utilization_pp_fops);

			mali_profiling_dir = debugfs_create_dir("profiling", mali_debugfs_dir);
			if (mali_profiling_dir != NULL) {
				u32 max_sub_jobs;
				long i;
				struct dentry *mali_profiling_gp_dir;
				struct dentry *mali_profiling_pp_dir;
#if defined(CONFIG_MALI400_INTERNAL_PROFILING)
				struct dentry *mali_profiling_proc_dir;
#endif
				/*
				 * Create directory where we can set GP HW counters.
				 */
				mali_profiling_gp_dir = debugfs_create_dir("gp", mali_profiling_dir);
				if (mali_profiling_gp_dir != NULL) {
					debugfs_create_file("counter_src0", 0600, mali_profiling_gp_dir, (void *)PRIVATE_DATA_COUNTER_MAKE_GP(0), &profiling_counter_src_fops);
					debugfs_create_file("counter_src1", 0600, mali_profiling_gp_dir, (void *)PRIVATE_DATA_COUNTER_MAKE_GP(1), &profiling_counter_src_fops);
				}

				/*
				 * Create directory where we can set PP HW counters.
				 * Possible override with specific HW counters for a particular sub job
				 * (Disable core scaling before using the override!)
				 */
				mali_profiling_pp_dir = debugfs_create_dir("pp", mali_profiling_dir);
				if (mali_profiling_pp_dir != NULL) {
					debugfs_create_file("counter_src0", 0600, mali_profiling_pp_dir, (void *)PRIVATE_DATA_COUNTER_MAKE_PP(0), &profiling_counter_src_fops);
					debugfs_create_file("counter_src1", 0600, mali_profiling_pp_dir, (void *)PRIVATE_DATA_COUNTER_MAKE_PP(1), &profiling_counter_src_fops);
				}

				max_sub_jobs = mali_executor_get_num_cores_total();
				for (i = 0; i < max_sub_jobs; i++) {
					char buf[16];
					struct dentry *mali_profiling_pp_x_dir;
					_mali_osk_snprintf(buf, sizeof(buf), "%u", i);
					mali_profiling_pp_x_dir = debugfs_create_dir(buf, mali_profiling_pp_dir);
					if (NULL != mali_profiling_pp_x_dir) {
						debugfs_create_file("counter_src0",
								    0600, mali_profiling_pp_x_dir,
								    (void *)PRIVATE_DATA_COUNTER_MAKE_PP_SUB_JOB(0, i),
								    &profiling_counter_src_fops);
						debugfs_create_file("counter_src1",
								    0600, mali_profiling_pp_x_dir,
								    (void *)PRIVATE_DATA_COUNTER_MAKE_PP_SUB_JOB(1, i),
								    &profiling_counter_src_fops);
					}
				}

#if defined(CONFIG_MALI400_INTERNAL_PROFILING)
				mali_profiling_proc_dir = debugfs_create_dir("proc", mali_profiling_dir);
				if (mali_profiling_proc_dir != NULL) {
					struct dentry *mali_profiling_proc_default_dir = debugfs_create_dir("default", mali_profiling_proc_dir);
					if (mali_profiling_proc_default_dir != NULL) {
						debugfs_create_file("enable", 0600, mali_profiling_proc_default_dir, (void *)_MALI_UK_USER_SETTING_SW_EVENTS_ENABLE, &user_settings_fops);
					}
				}
				debugfs_create_file("record", 0600, mali_profiling_dir, NULL, &profiling_record_fops);
				debugfs_create_file("events", 0400, mali_profiling_dir, NULL, &profiling_events_fops);
				debugfs_create_file("events_human_readable", 0400, mali_profiling_dir, NULL, &profiling_events_human_readable_fops);
#endif
			}

#if MALI_STATE_TRACKING
			debugfs_create_file("state_dump", 0400, mali_debugfs_dir, NULL, &mali_seq_internal_state_fops);
#endif

#if defined(DEBUG)
			debugfs_create_file("timeline_dump", 0400, mali_debugfs_dir, NULL, &timeline_dump_fops);
#endif
			if (mali_sysfs_user_settings_register()) {
				/* Failed to create the debugfs entries for the user settings DB. */
				MALI_DEBUG_PRINT(2, ("Failed to create user setting debugfs files. Ignoring...\n"));
			}

			debugfs_create_file("dump", 0440, mali_debugfs_dir, NULL, &dump_fops);
			debugfs_create_file("write", 0220, mali_debugfs_dir, NULL, &write_fops);
		}
	}

	/* Success! */
	return 0;
}

int mali_sysfs_unregister(void)
{
	if (NULL != mali_debugfs_dir) {
		debugfs_remove_recursive(mali_debugfs_dir);
	}
	return 0;
}

#else /* MALI_LICENSE_IS_GPL */

/* Dummy implementations for non-GPL */

int mali_sysfs_register(struct mali_dev *device, dev_t dev, const char *mali_dev_name)
{
	return 0;
}

int mali_sysfs_unregister(void)
{
	return 0;
}

#endif /* MALI_LICENSE_IS_GPL */
