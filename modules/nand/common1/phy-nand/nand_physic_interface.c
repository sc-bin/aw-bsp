/* SPDX-License-Identifier: GPL-2.0 */
/*
************************************************************************************************************************
*                                                      eNand
*                                           Nand flash driver scan module
*
*                             Copyright(C), 2008-2009, SoftWinners Microelectronic Co., Ltd.
*                                                  All Rights Reserved
*
* File Name : nand_chip_interface.c
*
* Author :
*
* Version : v0.1
*
* Date : 2013-11-20
*
* Description :
*
* Others : None at present.
*
*
*
************************************************************************************************************************
*/
#define _NPHYI_COMMON_C_

#include <sunxi-boot.h>
#include "nand_physic_interface.h"
#include "../nfd/nand_osal_for_linux.h"
#include "nand-partition/phy.h"
#include "rawnand/rawnand_chip.h"
#include "rawnand/controller/ndfc_base.h"
#include "rawnand/rawnand.h"
#include "rawnand/rawnand_base.h"
#include "rawnand/rawnand_cfg.h"
#include "nand.h"
/*#include "nand_boot.h"*/
#include "nand-partition3/sunxi_nand_boot.h"
#include "spinand/spinand.h"
#include "nand_weak.h"
#include <mtd/aw-spinand-nftl.h>
//#define POWER_OFF_DBG
__u32 storage_type;

struct _nand_info aw_nand_info = {0};

#ifndef POWER_OFF_DBG
unsigned int power_off_dbg_enable;
#else
unsigned int power_off_dbg_enable = 1;
#endif
/* current logical write block */
unsigned short cur_w_lb_no;
/* current physical write block */
unsigned short cur_w_pb_no;
/* current physical write page */
unsigned short cur_w_p_no;
/* current logical erase block */
unsigned short cur_e_lb_no;

extern unsigned int rawnand_get_super_chip_page_size(struct nand_super_chip_info *schip);
extern unsigned int rawnand_get_super_chip_spare_size(struct nand_super_chip_info *schip);
extern unsigned int rawnand_get_super_chip_block_size(struct nand_super_chip_info *schip);
extern unsigned int rawnand_get_super_chip_size(struct nand_super_chip_info *schip);
extern unsigned int rawnand_get_super_chip_cnt(struct nand_super_chip_info *schip);

__u32 get_storage_type_from_init(void)
{
	return storage_type;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       : 0:ok  -1:fail
*Note         :
*****************************************************************************/
struct _nand_info *nand_hw_init(void)
{
	struct _nand_info *nand_info_temp;

	if (get_storage_type() == 1) {
		nand_info_temp = RawNandHwInit();
	} else if (get_storage_type() == 2) {
		nand_info_temp = spinand_hardware_init();
	} else {
		storage_type = 1;
		nand_info_temp = RawNandHwInit();
		if (nand_info_temp == NULL) {
			storage_type = 2;
			nand_info_temp = spinand_hardware_init();
		}
	}

	return nand_info_temp;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       : 0:ok  -1:fail
*Note         :
*****************************************************************************/
int NandHwExit(void)
{
	int ret = 0;

	if (get_storage_type() == 1) {
		ret = RawNandHwExit();
	} else if (get_storage_type() == 2) {
		ret = spinand_hardware_exit();
	} else {
		RawNandHwExit();
		spinand_hardware_exit();
	}

	return ret;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       : 0:ok  -1:fail
*Note         :
*****************************************************************************/
int nand_hw_super_standby(void)
{
	int ret = 0;

	if (get_storage_type() == 1) {
		ret = rawnand_hw_super_standby();
	} else if (get_storage_type() == 2) {
		ret = 0; //spi host layer achieve
	}

	return ret;
}
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       : 0:ok  -1:fail
*Note         :
*****************************************************************************/
int nand_hw_super_resume(void)
{
	int ret = 0;

	if (get_storage_type() == 1) {
		ret = rawnand_hw_super_resume();
	} else if (get_storage_type() == 2) {
		ret = 0; //spi host layer achieve
	}

	return ret;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       : 0:ok  -1:fail
*Note         :
*****************************************************************************/
int NandHwNormalStandby(void)
{
	int ret = 0;

	if (get_storage_type() == 1) {
		ret = rawnand_hw_normal_standby();
	} else if (get_storage_type() == 2) {
		ret = 0; //spi host layer achieve
	}

	return ret;
}
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       : 0:ok  -1:fail
*Note         :
*****************************************************************************/
int NandHwNormalResume(void)
{
	int ret = 0;

	if (get_storage_type() == 1) {
		ret = rawnand_hw_normal_resume();
	} else if (get_storage_type() == 2) {
		ret = 0; /*spi host layer achieve resume*/
	}

	return ret;
}
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       : 0:ok  -1:fail
*Note         :
*****************************************************************************/
int nand_hw_shutdown(void)
{
	int ret = 0;

	if (get_storage_type() == 1) {
		ret = rawnand_hw_shutdown();
	} else if (get_storage_type() == 2) {
		ret = 0; /*spi host layer achieve suspend*/
	}

	return ret;
}
#if 0
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       : 0:ok  -1:fail
*Note         :
*****************************************************************************/
void *NAND_GetIOBaseAddr(u32 no)
{
	if (get_storage_type() == 1) {
		if (no != 0)
			return (void *)RAWNAND_GetIOBaseAddrCH1();
		else
			return (void *)RAWNAND_GetIOBaseAddrCH0();
	} else if (get_storage_type() == 2) {
		return NULL;
	}
	return NULL;
}
#endif
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       : 0:ok  -1:fail
*Note         :
*****************************************************************************/
u32 nand_get_logic_page_size(void)
{
	//return 16384;
	if (get_storage_type() == 1)
		return 512 * aw_nand_info.SectorNumsPerPage;
	else if (get_storage_type() == 2)
		return spinand_nftl_get_super_page_size(BYTE);

	return 0;
}

u32 nand_get_logic_block_size(void)
{
	if (get_storage_type() == 1)
		return aw_nand_info.PageNumsPerBlk * aw_nand_info.SectorNumsPerPage * 512;
	else if (get_storage_type() == 2)
		return spinand_nftl_get_super_block_size(BYTE);

	return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       : 0:ok  -1:fail
*Note         :
*****************************************************************************/
int nand_physic_erase_block(unsigned int chip, unsigned int block)
{
	if (get_storage_type() == 1)
		return rawnand_physic_erase_block(chip, block);
	else if (get_storage_type() == 2)
		return spinand_nftl_erase_single_block(chip, block);
	return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       : 0:ok  -1:fail
*Note         :
*****************************************************************************/
int nand_physic_read_page(unsigned int chip, unsigned int block, unsigned int page, unsigned int bitmap, unsigned char *mbuf, unsigned char *sbuf)
{
	if (get_storage_type() == 1)
		return rawnand_physic_read_page(chip, block, page, bitmap, mbuf, sbuf);
	else if (get_storage_type() == 2)
		return spinand_nftl_read_single_page(chip, block, page, bitmap, mbuf, sbuf);
	return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       : 0:ok  -1:fail
*Note         :
*****************************************************************************/
int nand_physic_write_page(unsigned int chip, unsigned int block, unsigned int page, unsigned int bitmap, unsigned char *mbuf, unsigned char *sbuf)
{
	if (get_storage_type() == 1)
		return rawnand_physic_write_page(chip, block, page, bitmap, mbuf, sbuf);
	else if (get_storage_type() == 2)
		return spinand_nftl_write_single_page(chip, block, page, bitmap, mbuf, sbuf);
	return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       : 0:ok  -1:fail
*Note         :
*****************************************************************************/
int nand_physic_bad_block_check(unsigned int chip, unsigned int block)
{
	if (get_storage_type() == 1)
		return rawnand_physic_bad_block_check(chip, block);
	else if (get_storage_type() == 2)
		return spinand_nftl_single_badblock_check(chip, block);
	return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       : 0:ok  -1:fail
*Note         :
*****************************************************************************/
int nand_physic_bad_block_mark(unsigned int chip, unsigned int block)
{
	if (get_storage_type() == 1)
		return rawnand_physic_bad_block_mark(chip, block);
	else if (get_storage_type() == 2)
		return spinand_nftl_single_badblock_mark(chip, block);
	return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       : 0:ok  -1:fail
*Note         :
*****************************************************************************/
int nand_physic_block_copy(unsigned int chip_s, unsigned int block_s, unsigned int chip_d, unsigned int block_d)
{
	if (get_storage_type() == 1)
		return rawnand_physic_block_copy(chip_s, block_s, chip_d, block_d);
	else if (get_storage_type() == 2)
		return spinand_nftl_single_block_copy(chip_s, block_s, chip_d, block_d);
	return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       : 0:ok  -1:fail
*Note         :
*****************************************************************************/
int read_virtual_page(unsigned int nDieNum, unsigned int nBlkNum, unsigned int nPage, uint64 SectBitmap, void *pBuf, void *pSpare)
{
	if (get_storage_type() == 1)
		return rawnand_physic_read_super_page(nDieNum, nBlkNum, nPage, SectBitmap, pBuf, pSpare);
	else if (get_storage_type() == 2)
		return spinand_nftl_read_super_page(nDieNum, nBlkNum, nPage, SectBitmap, pBuf, pSpare);
	return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       : 0:ok  -1:fail
*Note         :
*****************************************************************************/
int write_virtual_page(unsigned int nDieNum, unsigned int nBlkNum, unsigned int nPage, uint64 SectBitmap, void *pBuf, void *pSpare)
{
	if (get_storage_type() == 1)
		return rawnand_physic_write_super_page(nDieNum, nBlkNum, nPage, SectBitmap, pBuf, pSpare);
	else if (get_storage_type() == 2)
		return spinand_nftl_write_super_page(nDieNum, nBlkNum, nPage, SectBitmap, pBuf, pSpare);
	return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       : 0:ok  -1:fail
*Note         :
*****************************************************************************/
int erase_virtual_block(unsigned int nDieNum, unsigned int nBlkNum)
{
	if (get_storage_type() == 1)
		return rawnand_physic_erase_super_block(nDieNum, nBlkNum);
	else if (get_storage_type() == 2)
		return spinand_nftl_erase_super_block(nDieNum, nBlkNum);
	return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       : 0:ok  -1:fail
*Note         :
*****************************************************************************/
int virtual_badblock_check(unsigned int nDieNum, unsigned int nBlkNum)
{
	if (get_storage_type() == 1)
		return rawnand_physic_super_bad_block_check(nDieNum, nBlkNum);
	else if (get_storage_type() == 2)
		return spinand_nftl_super_badblock_check(nDieNum, nBlkNum);
	return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       : 0:ok  -1:fail
*Note         :
*****************************************************************************/
int virtual_badblock_mark(unsigned int nDieNum, unsigned int nBlkNum)
{
	if (get_storage_type() == 1)
		return rawnand_physic_super_bad_block_mark(nDieNum, nBlkNum);
	else if (get_storage_type() == 2)
		return spinand_nftl_super_badblock_mark(nDieNum, nBlkNum);
	return 0;
}

__u32 nand_get_lsb_block_size(void)
{
	if (get_storage_type() == 1)
		return rawnand_get_lsb_block_size();
	else if (get_storage_type() == 2)
		return spinand_nftl_get_single_block_size(BYTE);
	return 0;
}

__u32 nand_get_lsb_pages(void)
{
	if (get_storage_type() == 1)
		return rawnand_get_lsb_pages();
	else if (get_storage_type() == 2)
		return spinand_nftl_get_single_block_size(PAGE);
	return 0;
}

__u32 nand_get_phy_block_size(void)
{
	if (get_storage_type() == 1)
		return rawnand_get_phy_block_size();
	else if (get_storage_type() == 2)
		return spinand_nftl_get_single_block_size(BYTE);
	return 0;
}

__u32 nand_used_lsb_pages(void)
{
	if (get_storage_type() == 1)
		return rawnand_used_lsb_pages();
	else if (get_storage_type() == 2)
		return spinand_lsb_page_cnt_per_block();
	return 0;
}

u32 nand_get_pageno(u32 lsb_page_no)
{
	if (get_storage_type() == 1)
		return rawnand_get_pageno(lsb_page_no);
	else if (get_storage_type() == 2)
		return spinand_get_page_num(lsb_page_no);
	return 0;
}

__u32 nand_get_page_cnt_per_block(void)
{
	if (get_storage_type() == 1)
		return rawnand_get_page_cnt_per_block();
	else if (get_storage_type() == 2)
		return spinand_nftl_get_single_block_size(PAGE);
	return 0;
}

__u32 nand_get_page_size(void)
{
	if (get_storage_type() == 1)
		return rawnand_get_page_size();
	else if (get_storage_type() == 2)
		return spinand_nftl_get_single_page_size(BYTE);
	return 0;
}

__u32 nand_get_twoplane_flag(void)
{
	if (get_storage_type() == 1)
		return rawnand_get_twoplane_flag();
	else if (get_storage_type() == 2)
		return spinand_nftl_get_multi_plane_flag();
	return 0;
}

/**
 * nand_get_super_chip_page_size:
 *
 * @return page size in sector
 */
unsigned int nand_get_super_chip_page_size(void)
{
	if (get_storage_type() == 1) {
		return rawnand_get_super_chip_page_size(g_nssi->nsci);
	} else if (get_storage_type() == 2)
		return 0;

	return 0;
}

/**
 * nand_get_super_chip_spare_size:
 *
 * @return the super chip spare size
 */
unsigned int nand_get_super_chip_spare_size(void)
{
	if (get_storage_type() == 1) {
		if (!g_nssi) {
			printk("rawnand hw not init\n");
			return 0;
		}
		return rawnand_get_super_chip_spare_size(g_nssi->nsci);
	} else if (get_storage_type() == 2)
		return 0;

	return 0;
}
/**
 * nand_get_super_chip_block_size:
 *
 * @return block size in page
 */
unsigned int nand_get_super_chip_block_size(void)
{
	if (get_storage_type() == 1) {
		if (!g_nssi) {
			printk("rawnand hw not init\n");
			return 0;
		}
		return rawnand_get_super_chip_block_size(g_nssi->nsci);
	} else if (get_storage_type() == 2)
		return 0;

	return 0;
}

unsigned int nand_get_super_chip_cnt(void)
{
	if (get_storage_type() == 1) {
		if (!g_nssi) {
			printk("rawnand hw not init\n");
			return 0;
		}
		return rawnand_get_super_chip_cnt(g_nssi->nsci);
	} else if (get_storage_type() == 2)
		return 0;

	return 0;
}
/**
 * nand_get_super_chip_size:
 *
 * @return super chip size in block
 */
unsigned int nand_get_super_chip_size(void)
{
	if (get_storage_type() == 1) {
		if (!g_nssi) {
			printk("rawnand hw not init\n");
			return 0;
		}
		return rawnand_get_super_chip_size(g_nssi->nsci);
	} else if (get_storage_type() == 2)
		return 0;

	return 0;
}

/**
 * nand_chips_init: rawnand physic init
 *
 * @chip: chip info
 * @info: nand info
 *
 *
 */
static int nand_chips_init(struct nand_chip_info *chip)
{
	return 0;
}

/**
 * nand_chips_cleanup: rawnand exit
 *
 * @chip: chip info
 *
 *
 */
static void nand_chips_cleanup(struct nand_chip_info *chip)
{
	return;
}

#ifdef SUPPORT_SUPER_STANDBY
/**
 * nand_chips_super_standby: rawnand super standby
 *
 * @chip: chip info
 *
 *
 */
static int nand_chips_super_standby(struct nand_chip_info *chip)
{
	return 0;
}

/**
 * nand_chips_super_resume: rawnand super resume
 *
 * @chip: chip info
 *
 *
 */
static int nand_chips_super_resume(struct nand_chip_info *chip)
{
	return 0;
}

#else
/**
 * nand_chips_normal_standby: rawnand normal standby
 *
 * @chip: chip info
 *
 *
 */
static int nand_chips_normal_standby(struct nand_chip_info *chip)
{
	return 0;
}

/**
 * nand_chips_normal_resume: rawnand normal resume
 *
 * @chip: chip info
 *
 *
 */
static int nand_chips_normal_resume(struct nand_chip_info *chip)
{
	return 0;
}
#endif

struct nand_chips_ops chips_ops = {
    .nand_chips_init = nand_chips_init,
    .nand_chips_cleanup = nand_chips_cleanup,
#ifdef SUPPORT_SUPER_STANDBY
    .nand_chips_standby = nand_chips_super_standby,
    .nand_chips_resume = nand_chips_super_resume,
#else
    .nand_chips_standby = nand_chips_normal_standby,
    .nand_chips_resume = nand_chips_normal_resume,
#endif
};
