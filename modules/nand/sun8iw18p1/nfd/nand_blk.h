/*
 * nand_blk.h for  SUNXI NAND .
 *
 * Copyright (C) 2016 Allwinner.
 *
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#ifndef __NAND_BLK_H__
#define __NAND_BLK_H__

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/spinlock.h>
#include <linux/hdreg.h>
#include <linux/init.h>
#include <linux/semaphore.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio.h>
#include <asm/processor.h>
#include <linux/spinlock.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <asm/cacheflush.h>
#include <linux/pm.h>
#include <linux/kthread.h>
#include "nand_type.h"
#include "nand_lib.h"
#include <linux/scatterlist.h>
#include <linux/vmalloc.h>
#include <linux/freezer.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/mii.h>
#include <linux/skbuff.h>
#include <linux/irqreturn.h>
#include <linux/device.h>
#include <linux/pagemap.h>
#include <linux/mutex.h>
#include <linux/reboot.h>
#include <linux/kmod.h>
#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/time.h>
#if 0//#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0))

#else
#include <linux/blk-mq.h>
#endif

#if defined CONFIG_ARCH_SUN8IW1P1
#define  SUN8IW1P1
#elif defined CONFIG_ARCH_SUN8IW3P1
#define  SUN8IW3P1
#elif defined CONFIG_ARCH_SUN9IW1P1
#define  SUN9IW1P1
#elif defined CONFIG_ARCH_SUN8IW5P1
#define  SUN8IW5P1
#elif defined CONFIG_ARCH_SUN8IW6P1
#define  SUN8IW6P1
#elif defined CONFIG_ARCH_SUN8IW7P1
#define  SUN8IW7P1
#elif defined CONFIG_ARCH_SUN8IW8P1
#define  SUN8IW8P1
#elif defined CONFIG_ARCH_SUN8IW10P1
#define  SUN8IW10P1
#elif defined CONFIG_ARCH_SUN8IW11P1
#define  SUN8IW11P1
#elif defined CONFIG_ARCH_SUN8IW12P1
#define  SUN8IW12P1
#elif defined CONFIG_ARCH_SUN8IW15P1
#define  SUN8IW15P1
#elif defined CONFIG_ARCH_SUN50I
#define  SUN50IW1P1
#elif defined CONFIG_ARCH_SUN8IW10P1
#define  SUN8IW10P1
#elif defined CONFIG_ARCH_SUN8IW11P1
#define  SUN8IW11P1
#elif defined CONFIG_ARCH_SUN50IW2P1
#define  SUN50IW2P1
#elif defined CONFIG_ARCH_SUN50IW3P1
#define  SUN50IW6P1
#elif defined CONFIG_ARCH_SUN8IW18P1 || defined CONFIG_ARCH_SUN8IW18
#define  SUN8IW18P1
#else
#error "please select a platform\n"
#endif

#define __FPGA_TEST__
#define __LINUX_NAND_SUPPORT_INT__
#define __LINUX_SUPPORT_DMA_INT__
#define __LINUX_SUPPORT_RB_INT__

#define BLK_ERR_MSG_ON
#ifdef BLK_ERR_MSG_ON
#define nand_dbg_err(fmt, args...) printk(KERN_ERR "[NAND]"fmt, ## args)
#else
#define nand_dbg_err(fmt, ...)  ({})
#endif

#define BLK_INFO_MSG_ON
#ifdef BLK_INFO_MSG_ON
#define nand_dbg_inf(fmt, args...) printk(KERN_DEBUG "[NAND]"fmt, ## args)
#else
#define nand_dbg_inf(fmt, ...)  ({})
#endif
extern int nand_gpt_enable;
extern struct device *ndfc_dev;
#define NAND_SUPPORT_GPT (nand_gpt_enable)

struct nand_blk_ops;
struct list_head;
struct semaphore;
struct hd_geometry;

struct nand_blk_dev {
	struct nand_blk_ops *nandr;
	struct list_head list;
	struct device dev;
	struct mutex lock;
	struct gendisk *disk;
	struct kref ref;
	void *priv;
	struct class dev_class;

	unsigned char heads;
	unsigned char sectors;
	unsigned short cylinders;

	int open;
	int devnum;
	unsigned long size;
	unsigned long off_size;
	int readonly;
	int writeonly;
	int disable_access;
	char name[32];
};

struct _nand_dev {
	struct nand_blk_dev nbd;
	char name[24];
	unsigned int offset;
	unsigned int size;
	struct _nftl_blk *nftl_blk;
	struct _nand_dev *nand_dev_next;

	struct mutex dev_lock;

	int (*read_data)(struct _nand_dev *nand_dev, unsigned int sector,
			 unsigned int len, unsigned char *buf);
	int (*write_data)(struct _nand_dev *nand_dev, unsigned int sector,
			 unsigned int len, unsigned char *buf);
	int (*flush_write_cache)(struct _nand_dev *nand_dev,
			 unsigned int num);
	int (*discard)(struct _nand_dev *nand_dev, unsigned int sector,
			 unsigned int len);
	int (*flush_sector_write_cache)(struct _nand_dev *nand_dev,
			 unsigned int num);
};

struct nand_blk_ops {
	/* blk device ID */
	char name[32];
	int major;
	int minorbits;
	int blksize;
	int blkshift;

	struct task_struct *thread;
	struct request_queue *rq;
	spinlock_t queue_lock;

	/* add/remove nandflash devparts,use gendisk */
	int (*add_dev)(struct nand_blk_ops *tr,
			struct _nand_phy_partition *phy_partition);
	int (*add_dev_test)(struct nand_blk_ops *tr);
	int (*remove_dev)(struct nand_blk_ops *tr);

	/* Block layer ioctls */
	int (*getgeo)(struct nand_blk_dev *dev, struct hd_geometry *geo);
	int (*flush)(struct nand_blk_dev *dev);

	/* Called with mtd_table_mutex held; no race with add/remove */
	int (*open)(struct nand_blk_dev *dev);
	int (*release)(struct nand_blk_dev *dev);

	struct _nftl_blk nftl_blk_head;
	struct _nand_dev nand_dev_head;

	/* synchronization variable */
	struct completion thread_exit;
	int quit;
	wait_queue_head_t thread_wq;
	struct semaphore nand_ops_mutex;
	struct list_head devs;
	int bg_stop;
	int rq_null;

	struct module *owner;
#if 0//#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0))

#else
	struct blk_mq_tag_set tag_set;
#endif
};

/*****************************************************************************/
extern int get_nand_secure_storage_max_item(void);
extern int nand_secure_storage_read(int item, unsigned char *buf,
				    unsigned int len);
extern int nand_secure_storage_write(int item, unsigned char *buf,
				     unsigned int len);

extern int NAND_ReadBoot0(unsigned int length, void *buf);
extern int NAND_ReadBoot1(unsigned int length, void *buf);
extern int NAND_BurnBoot0(unsigned int length, void *buf);
extern int NAND_BurnBoot1(unsigned int length, void *buf);

extern struct _nand_info *p_nand_info;

extern int add_nand(struct nand_blk_ops *tr,
		    struct _nand_phy_partition *phy_partition);
extern int add_nand_for_dragonboard_test(struct nand_blk_ops *tr);
extern int remove_nand(struct nand_blk_ops *tr);
extern int nand_flush(struct nand_blk_dev *dev);
extern struct _nand_phy_partition *get_head_phy_partition_from_nand_info(
				struct _nand_info *nand_info);
extern struct _nand_phy_partition *get_next_phy_partition(
				struct _nand_phy_partition *phy_partition);

/*****************************************************************************/

#endif
