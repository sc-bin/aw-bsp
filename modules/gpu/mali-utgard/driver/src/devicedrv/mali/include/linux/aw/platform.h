/**
 * Copyright (C) 2015-2016 Allwinner Technology Limited. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * Author: Albert Yu <yuxyun@allwinnertech.com>
 */

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <linux/dma-mapping.h>
#include <linux/mali/mali_utgard.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/stat.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_MALI_DT
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/of_address.h>
#else /* CONFIG_MALI_DT */
#include <linux/clk/sunxi_name.h>
#include <mach/irqs.h>
#include <mach/sys_config.h>
#include <mach/platform.h>
#endif /* CONFIG_MALI_DT */
#include "mali_kernel_common.h"

typedef struct {
	const char   *clk_name;    /* Clock name */
#ifndef CONFIG_MALI_DT
	const char   *clk_id;      /* Clock ID, which is in the system configuration header file */
#endif /* CONFIG_MALI_DT */

	struct clk   *clk_handle;

	int freq; /* MHz */

	bool         clk_status;
} aw_clk_data;

typedef struct {
	int vol;  /* mV */
	int freq; /* MHz */
} vf_table_data;

typedef struct {
	struct regulator *regulator;
	char   *regulator_id;
	int    current_vol;
	aw_clk_data clk[3];
	struct mutex dvfs_lock;
	bool   dvfs_status;
	vf_table_data vf_table[9];
	struct work_struct dvfs_work;
	int    cool_freq;
	int    begin_level;
	int    current_level;
	int    max_available_level;
	int    max_level;
	int    scene_ctrl_cmd; /* 1 means GPU should provide the highest performance */
	bool   scene_ctrl_status;
	bool   independent_pow;
	bool   dvm;
} aw_pm_data;

typedef struct {
	spinlock_t lock;
	struct mali_gpu_utilization_data data;
} aw_utilization_data;

typedef struct {
	bool temp_ctrl_status;
} aw_tempctrl_data;

typedef struct {
	bool enable;
	bool frequency;
	bool voltage;
	bool tempctrl;
	bool dvfs;
	bool level;
} aw_debug;

typedef struct {
#ifdef CONFIG_MALI_DT
	struct device_node *np_gpu;
#endif /* CONFIG_MALI_DT */

	struct reset_control *reset;

	void __iomem *poweroff_gating_addr;

	aw_tempctrl_data tempctrl;

	aw_pm_data pm;

	aw_utilization_data utilization;

	aw_debug debug;
} aw_private_data;

#endif /* _PLATFORM_H_ */
