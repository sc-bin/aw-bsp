/*
 * nand_osal_for_linux.h for  SUNXI NAND .
 *
 * Copyright (C) 2016 Allwinner.
 *
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef _NAND_OSAL_FOR_LINUX_H_
#define _NAND_OSAL_FOR_LINUX_H_
#include <linux/kernel.h>
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
#include <linux/mutex.h>
#include <linux/dma-mapping.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <asm/cacheflush.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio.h>
#include "nand_blk.h"
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <sunxi-sid.h>

#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/version.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0))
#include <linux/dma/sunxi-dma.h>
#else
#include <asm/uaccess.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#endif

#define SPI_TX_DATA_REG			(0x200)
#define SPI_RX_DATA_REG			(0x300)
#define SPI_BASE_ADDR			(0x05010000)

#define DMA_CONFIG_SIZE (4096)


struct clk *pll6;
struct clk *nand0_dclk, *nand0_cclk;
struct clk *ahb_nand0;
struct regulator *regu1;
struct regulator *regu2;

int seq;
int nand_handle;

static __u32 PRINT_LEVEL;

extern struct completion spinand_dma_done;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0))
struct dma_chan *dma_hdl_tx;
struct dma_chan *dma_hdl_rx;
struct dma_chan *dma_hdl;
#else
#define SPINAND_MAX_PAGES	8
enum spinand_dma_dir {
	SPINAND_DMA_RWNULL,
	SPINAND_DMA_WDEV = DMA_TO_DEVICE,
	SPINAND_DMA_RDEV = DMA_FROM_DEVICE,
};
typedef struct {
	enum spinand_dma_dir dir;
	struct dma_chan *chan;
	int nents;
	struct scatterlist sg[SPINAND_MAX_PAGES];
	struct page *pages[SPINAND_MAX_PAGES];
} spinand_dma_info_t;
static spinand_dma_info_t dma_rx;
static spinand_dma_info_t dma_tx;
static spinand_dma_info_t g_dma;
#endif

extern void *SPIC0_IO_BASE;
void *dma_map_addr;


extern void *NDFC0_BASE_ADDR;
extern void *NDFC1_BASE_ADDR;
extern struct device *ndfc_dev;


__u32 NAND_Print_level(void);
__u32 nand_dma_callback(void *para);
__u32 get_storage_type(void);

#endif
