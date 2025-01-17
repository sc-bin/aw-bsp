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
#include <asm/cacheflush.h>
#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/hdreg.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <sunxi-sid.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#ifdef CONFIG_AW_RAWNAND_CD
#include <linux/irqreturn.h>
#endif

#include "nand_base.h"
#include <linux/dma-mapping.h>
/*#include <linux/dma/sunxi-dma.h>*/
#include <linux/dmaengine.h>
#include <sunxi-panicpart.h>

#define SPI_TX_DATA_REG 0x200
#define SPI_RX_DATA_REG 0x300
#define SPI_BASE_ADDR 0x05010000

#define DMA_CONFIG_SIZE (4096)

extern struct completion spinand_dma_done;

extern struct dma_chan *dma_hdl_tx;
extern struct dma_chan *dma_hdl_rx;

extern void *SPIC0_IO_BASE;
extern void *dma_map_addr;

extern void *NDFC0_BASE_ADDR;
extern void *NDFC1_BASE_ADDR;

extern struct dma_chan *dma_hdl;
__u32 NAND_Print_level(void);
__u32 nand_dma_callback(void *para);
__u32 get_storage_type(void);

int nand_print(const char *fmt, ...);
int nand_print_dbg(const char *fmt, ...);
int nand_set_clk(struct sunxi_ndfc *ndfc, __u32 nand_index, __u32 nand_clk0,
		__u32 nand_clk1);
int nand_get_clk(struct sunxi_ndfc *ndfc, __u32 nand_index, __u32 *pnand_clk0,
		__u32 *pnand_clk1);
void eLIBs_CleanFlushDCacheRegion_nand(void *adr, size_t bytes);
__s32 nand_clean_flush_dcache_region(void *buff_addr, __u32 len);
__s32 nand_invaild_dcache_region(__u32 rw, __u32 buff_addr, __u32 len);
void *nand_dma_map_single(struct sunxi_ndfc *ndfc, __u32 rw, void *buff_addr, __u32 len);
void *nand_dma_unmap_single(struct sunxi_ndfc *ndfc, __u32 rw, void *buff_addr, __u32 len);
void *nand_va_to_pa(void *buff_addr);
__s32 nand_pio_request(struct sunxi_ndfc *ndfc, __u32 nand_index);
__s32 NAND_3DNand_Request(void);
__s32 NAND_Check_3DNand(void);
void nand_pio_release(struct sunxi_ndfc *ndfc, __u32 nand_index);
void nand_memset(void *pAddr, unsigned char value, unsigned int len);
int nand_memcmp(const void *s1, const void *s2, size_t n);
int nand_strcmp(const char *s1, const char *s2);
void nand_memcpy(void *pAddr_dst, void *pAddr_src, unsigned int len);
void *nand_malloc(unsigned int Size);
void nand_free(void *addr);
void *nand_malloc(unsigned int size);
int nand_physic_lock_init(void);
int nand_physic_lock(void);
int nand_physic_unlock(void);
int nand_physic_lock_exit(void);
int spinand_request_tx_dma(void);
int spinand_request_rx_dma(void);
int spinand_releasetxdma(void);
int spinand_releaserxdma(void);
void spinand_dma_callback(void *arg);
int tx_dma_config_start(dma_addr_t addr, __u32 length);
int rx_dma_config_start(dma_addr_t addr, __u32 length);
int spinand_dma_config_start(__u32 rw, __u32 addr, __u32 length);
int nand_wait_dma_finish(__u32 tx_flag, __u32 rx_flag);
int nand_dma_end(__u32 rw, __u32 addr, __u32 length);
int rawnand_dma_config_start(__u32 rw, __u32 addr, __u32 length);
int nand_dma_config_start(__u32 rw, __u32 addr, __u32 length);
__u32 nand_get_ndfc_dma_mode(void);
__u32 nand_get_nand_extern_para(struct sunxi_ndfc *ndfc, __u32 para_num);
__u32 nand_get_nand_id_number_ctrl(struct sunxi_ndfc *ndfc);
__u32 nand_get_max_channel_cnt(void);
int nand_request_dma(struct sunxi_ndfc *ndfc);
int nand_release_dma(struct sunxi_ndfc *ndfc, __u32 nand_index);
__u32 nand_get_ndfc_version(void);
void *RAWNAND_GetIOBaseAddrCH0(void);
void *RAWNAND_GetIOBaseAddrCH1(void);
void *SPINAND_GetIOBaseAddrCH0(void);
void *SPINAND_GetIOBaseAddrCH1(void);
__s32 nand_rb_wait_time_out(__u32 no, __u32 *flag);
__s32 nand_rb_wake_up(__u32 no);
__s32 nand_dma_wait_time_out(__u32 no, __u32 *flag);
__s32 nand_dma_wake_up(__u32 no);
__u32 nand_dma_callback(void *para);
int nand_get_voltage(struct sunxi_ndfc *ndfc);
int nand_release_voltage(struct sunxi_ndfc *ndfc);
int nand_is_secure_sys(void);
__u32 nand_print_level(struct sunxi_ndfc *ndfc);
int nand_get_dragon_board_flag(struct sunxi_ndfc *ndfc);
void nand_print_version(void);
int nand_get_drv_version(int *ver_main, int *ver_sub, int *date, int *time);
void nand_cond_resched(void);
__u32 get_storage_type(void);
int nand_get_bsp_code(char *chip_code);
int nand_clk_request(struct sunxi_ndfc *ndfc, __u32 nand_index);
void nand_clk_release(struct sunxi_ndfc *ndfc, __u32 nand_index);
int nand_vccq_1p8v_enable(void);
int nand_vccq_3p3v_enable(void);
extern int sunxi_sel_pio_mode(struct pinctrl *pinctrl, u32 pm_sel);
#ifdef CONFIG_AW_RAWNAND_CD
int nand_pwroff_pio_request(struct sunxi_ndfc *ndfc);
void nand_gpiod_request_cd_irq(struct sunxi_ndfc *host);
#endif

#endif /*NAND_OSAL_FOR_LINUX_H*/
