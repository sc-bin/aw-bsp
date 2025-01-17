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
#define _UBOOTT_RAWNAND_C_

/*#include "../nand_boot.h"*/
#include "../nand_errno.h"
#include "../nand_physic_interface.h"
#include "../nand_secure_storage.h"
#include "rawnand_boot.h"
#include "rawnand_chip.h"
#include "rawnand_readretry.h"
#include "rawnand.h"
#include "rawnand_base.h"
#include "rawnand_cfg.h"
#include "rawnand_debug.h"
#include "../version.h"
#include <sunxi-boot.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include "../nand-partition3/sunxi_nand_boot.h"

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 *****************************************************************************/
int rawnand_write_uboot_one_in_block(unsigned char *buf, unsigned int len, struct _boot_info *info_buf, unsigned int info_len, unsigned int counter)
{
	int ret;
	unsigned int j, boot_len, page_index, t_len;
	unsigned char oob_buf[64];
	struct _nand_physic_op_par lnpo;
	struct nand_chip_info *nci;
	int uboot_flag, info_flag;
	unsigned char *kernel_buf;
	unsigned char *ptr;

	RAWNAND_DBG("burn uboot in one block!\n");

	nci = g_nctri->nci;

	uboot_flag = 0;
	info_flag = 0;
	boot_len = 0;
	t_len = 0;

	kernel_buf = nand_get_temp_buf(nci->sector_cnt_per_page << 9);

	memset(oob_buf, 0xff, 64);

	nand_get_version(oob_buf);
	if ((oob_buf[0] != 0xff) || (oob_buf[1] != 0x00)) {
		RAWNAND_ERR("get flash driver version error!");
		nand_free_temp_buf(kernel_buf);
		return ERR_NO_105;
	}

	if (len % (nci->sector_cnt_per_page << 9)) {
		RAWNAND_ERR("uboot length check error!\n");
		nand_free_temp_buf(kernel_buf);
		return ERR_NO_104;
	}

	lnpo.chip = 0;
	lnpo.block = info_buf->uboot_start_block + counter;
	lnpo.page = 0;

	nand_wait_all_rb_ready();

	ret = nci->nand_physic_erase_block(&lnpo);
	if (ret) {
		RAWNAND_ERR("Fail in erasing block %d!\n", lnpo.block);
		//return ret;
	}
	page_index = 0;
	for (j = 0; j < nci->page_cnt_per_blk; j++) {
		lnpo.chip = 0;
		lnpo.block = info_buf->uboot_start_block + counter;
		if (0 == nci->is_lsb_page(j)) {
			continue;
		}
		lnpo.page = j;
		lnpo.sect_bitmap = nci->sector_cnt_per_page;
		lnpo.sdata = oob_buf;
		lnpo.slen = 64;

		if (uboot_flag == 0) {
			boot_len = page_index * (nci->sector_cnt_per_page << 9);
			memcpy(kernel_buf, buf + boot_len, nci->sector_cnt_per_page << 9);
			ptr = kernel_buf;
			if ((len - boot_len) == (nci->sector_cnt_per_page << 9)) {
				uboot_flag = page_index + 1;
			}
		} else if (info_flag == 0) {
			RAWNAND_DBG("uboot info: page %d in block %d.\n", lnpo.page, lnpo.block);
			t_len = (page_index - uboot_flag) * (nci->sector_cnt_per_page << 9);
			ptr = (unsigned char *)info_buf;
			ptr += t_len;
			if ((info_len - t_len) == (nci->sector_cnt_per_page << 9)) {
				info_flag = page_index;
			}
		} else {
			ptr = kernel_buf;
		}

		lnpo.mdata = ptr;

		nand_wait_all_rb_ready();

		if (nci->nand_physic_write_page(&lnpo) != 0) {
			RAWNAND_ERR("Warning. Fail in writing page %d in block %d.\n", lnpo.page, lnpo.block);
		}
		page_index++;
	}

	nand_wait_all_rb_ready();
	nand_free_temp_buf(kernel_buf);
	return 0;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 *****************************************************************************/
int rawnand_write_uboot_one_in_many_block(unsigned char *buf, unsigned int len, struct _boot_info *info_buf, unsigned int info_len, unsigned int counter)
{
	int ret;
	unsigned int j, boot_len, page_index, total_len, t_len, total_pages, pages_per_block, good_block_offset, blocks_one_uboot, uboot_block_offset, m;
	unsigned int write_blocks;
	unsigned char oob_buf[64];
	struct _nand_physic_op_par lnpo;
	struct nand_chip_info *nci;
	int uboot_flag, info_flag;
	unsigned char *kernel_buf;
	unsigned char *ptr;

	RAWNAND_DBG("burn uboot in many block %d!\n", counter);

	nci = g_nctri->nci;

	uboot_flag = 0;
	info_flag = 0;
	boot_len = 0;
	t_len = 0;

	kernel_buf = nand_get_temp_buf(nci->sector_cnt_per_page << 9);

	memset(oob_buf, 0xff, 64);

	nand_get_version(oob_buf);
	if ((oob_buf[0] != 0xff) || (oob_buf[1] != 0x00)) {
		RAWNAND_ERR("get flash driver version error!");
		nand_free_temp_buf(kernel_buf);
		return ERR_NO_105;
	}

	if (len % (nci->sector_cnt_per_page << 9)) {
		RAWNAND_ERR("uboot length check error!\n");
		nand_free_temp_buf(kernel_buf);
		return ERR_NO_104;
	}

	total_len = len + info_len;
	if (total_len % (nci->sector_cnt_per_page << 9)) {
		RAWNAND_ERR("uboot length check error!\n");
		nand_free_temp_buf(kernel_buf);
		return ERR_NO_104;
	}
	total_pages = total_len / (nci->sector_cnt_per_page << 9);
	pages_per_block = nand_get_lsb_pages();
	blocks_one_uboot = total_pages / pages_per_block;

	if (total_pages % pages_per_block) {
		blocks_one_uboot++;
	}

	good_block_offset = blocks_one_uboot * counter;

	for (m = 0, j = info_buf->uboot_start_block; j < info_buf->uboot_next_block; j++) {
		if (m == good_block_offset) {
			break;
		}
		lnpo.chip = 0;
		lnpo.block = j;
		lnpo.page = 0;
		lnpo.sect_bitmap = 0;
		lnpo.mdata = NULL;
		lnpo.sdata = NULL;
		ret = nci->nand_physic_bad_block_check(&lnpo);
		if (ret == 0) {
			m++;
		}
	}

	uboot_block_offset = j;

	if ((uboot_block_offset + blocks_one_uboot) > info_buf->uboot_next_block) {
		nand_free_temp_buf(kernel_buf);
		return 0;
	}

	/////////////////////////////////////////////////////

	uboot_flag = 0;
	info_flag = 0;
	boot_len = 0;
	page_index = 0;
	write_blocks = 0;

	for (j = uboot_block_offset; j < info_buf->uboot_next_block; j++) {
		lnpo.chip = 0;
		lnpo.block = j;
		lnpo.page = 0;
		nand_wait_all_rb_ready();
		ret = nci->nand_physic_bad_block_check(&lnpo);
		if (ret != 0) {
			continue;
		}

		ret = nci->nand_physic_erase_block(&lnpo);
		if (ret != 0) {
			RAWNAND_ERR("Fail in erasing block %d!\n", lnpo.block);
			//return ret;
		}

		write_blocks++;

		RAWNAND_DBG("write uboot many block %d!\n", lnpo.block);

		for (m = 0; m < nci->page_cnt_per_blk; m++) {
			lnpo.chip = 0;
			lnpo.block = j;
			lnpo.page = m;
			lnpo.sect_bitmap = nci->sector_cnt_per_page;
			lnpo.sdata = oob_buf;
			lnpo.slen = 64;
			if (nci->is_lsb_page(m) != 0) {
				if (uboot_flag == 0) {
					boot_len = page_index * (nci->sector_cnt_per_page << 9);
					memcpy(kernel_buf, buf + boot_len, nci->sector_cnt_per_page << 9);
					ptr = kernel_buf;
					if ((len - boot_len) == (nci->sector_cnt_per_page << 9)) {
						uboot_flag = page_index + 1;
					}
				} else if (info_flag == 0) {
					RAWNAND_DBG("uboot info: page %d in block %d.\n", lnpo.page, lnpo.block);
					t_len = (page_index - uboot_flag) * (nci->sector_cnt_per_page << 9);
					ptr = (unsigned char *)info_buf;
					ptr += t_len;
					if ((info_len - t_len) == (nci->sector_cnt_per_page << 9)) {
						info_flag = page_index;
					}
				} else {
					ptr = kernel_buf;
				}

				lnpo.mdata = ptr;
				nand_wait_all_rb_ready();
				if (nci->nand_physic_write_page(&lnpo) != 0) {
					RAWNAND_ERR("Warning. Fail in writing page %d in block %d.\n", lnpo.page, lnpo.block);
				}
				page_index++;
			}
		}

		if (blocks_one_uboot == write_blocks) {
			break;
		}
	}
	nand_wait_all_rb_ready();
	nand_free_temp_buf(kernel_buf);
	return 0;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 *****************************************************************************/
int rawnand_write_uboot_one(unsigned char *buf, unsigned int len, struct _boot_info *info_buf, unsigned int info_len, unsigned int counter)
{
	int ret;
	struct nand_chip_info *nci;
	int real_len;

	RAWNAND_DBG("burn uboot one!\n");

	nci = g_nctri->nci;

	real_len = rawnand_add_len_to_uboot_tail(len);

	//print_physic_info(phyinfo_buf);

	if (((0 == nci->lsb_page_type) && (real_len <= nand_get_phy_block_size())) || ((0 != nci->lsb_page_type) && (real_len <= nand_get_lsb_block_size()))) {
		ret = rawnand_write_uboot_one_in_block(buf, len, phyinfo_buf, PHY_INFO_SIZE, counter);
	} else {
		ret = rawnand_write_uboot_one_in_many_block(buf, len, phyinfo_buf, PHY_INFO_SIZE, counter);
	}
	return ret;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail 1: ecc limit
 *Note         :
 *****************************************************************************/
int rawnand_read_uboot_one_in_block(unsigned char *buf, unsigned int len, unsigned int counter)
{
	int ret = 0, data_error = 0, ecc_limit = 0;
	unsigned int j, page_index, total_pages;
	unsigned char oob_buf[64];
	struct _nand_physic_op_par lnpo;
	struct nand_chip_info *nci;
	//	int uboot_flag = 0;
	unsigned char *ptr;

	RAWNAND_DBG("read uboot in one block %d!\n", counter);

	nci = g_nctri->nci;

	//	uboot_flag = 0;
	//	info_flag = 0;
	//	boot_len = 0;
	//	t_len = 0;

	ptr = nand_get_temp_buf(nci->sector_cnt_per_page << 9);

	memset(oob_buf, 0xff, 64);

	if (len % (nci->sector_cnt_per_page << 9)) {
		RAWNAND_ERR("uboot length check error!\n");
		nand_free_temp_buf(ptr);
		return 0;
	}

	total_pages = len / (nci->sector_cnt_per_page << 9);

	for (j = 0, page_index = 0; j < nci->page_cnt_per_blk; j++) {
		lnpo.chip = 0;
		lnpo.block = phyinfo_buf->uboot_start_block + counter;
		if (0 == nci->is_lsb_page(j)) {
			continue;
		}
		lnpo.page = j;
		lnpo.sect_bitmap = nci->sector_cnt_per_page;
		lnpo.sdata = oob_buf;
		lnpo.slen = 64;
		lnpo.mdata = ptr;

		nand_wait_all_rb_ready();

		ret = nci->nand_physic_read_page(&lnpo);
		if (ret == 0) {
			;
		} else if (ret == ECC_LIMIT) {
			ecc_limit = 1;
			RAWNAND_ERR("Warning. Fail in read page %d in block %d \n", lnpo.page, lnpo.block);
		} else {
			data_error = 1;
			break;
		}

		if ((oob_buf[0] != 0xff) || (oob_buf[1] != 0x00)) {
			RAWNAND_ERR("get uboot flash driver version error!\n");
			data_error = 1;
			break;
		}

		memcpy(buf + page_index * (nci->sector_cnt_per_page << 9), ptr, nci->sector_cnt_per_page << 9);

		page_index++;

		if (total_pages == page_index) {
			break;
		}
	}

	nand_wait_all_rb_ready();
	nand_free_temp_buf(ptr);

	if (data_error == 1) {
		ret = -1;
	} else if (ecc_limit == 1) {
		ret = 1;
	} else {
		ret = 0;
	}
	return ret;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail  1: ecc limit
 *Note         :
 *****************************************************************************/
int rawnand_read_uboot_one_in_many_block(unsigned char *buf, unsigned int len, unsigned int counter)
{
	int ret = 0, data_error = 0, ecc_limit = 0;
	unsigned int j, page_index, total_len, total_pages, pages_per_block, good_block_offset, blocks_one_uboot, uboot_block_offset, m;
	unsigned char oob_buf[64];
	struct _nand_physic_op_par lnpo;
	struct nand_chip_info *nci;
	//    int uboot_flag = 0;
	unsigned char *kernel_buf;
	//    unsigned char* ptr;

	RAWNAND_DBG("read uboot in many block %d!\n", counter);

	nci = g_nctri->nci;

	//    uboot_flag = 0;
	//    info_flag = 0;
	//    boot_len = 0;
	//    t_len = 0;

	kernel_buf = nand_get_temp_buf(nci->sector_cnt_per_page << 9);

	memset(oob_buf, 0xff, 64);

	nand_get_version(oob_buf);
	if ((oob_buf[0] != 0xff) || (oob_buf[1] != 0x00)) {
		RAWNAND_ERR("get flash driver version error!");
		nand_free_temp_buf(kernel_buf);
		return ERR_NO_105;
	}

	if (len % (nci->sector_cnt_per_page << 9)) {
		RAWNAND_ERR("uboot length check error!\n");
		nand_free_temp_buf(kernel_buf);
		return ERR_NO_104;
	}

	total_len = len;
	total_pages = total_len / (nci->sector_cnt_per_page << 9);
	pages_per_block = nand_get_lsb_pages();
	blocks_one_uboot = total_pages / pages_per_block;

	if (total_pages % pages_per_block) {
		blocks_one_uboot++;
	}

	good_block_offset = blocks_one_uboot * counter;

	for (m = 0, j = phyinfo_buf->uboot_start_block; j < phyinfo_buf->uboot_next_block; j++) {
		if (m == good_block_offset) {
			break;
		}
		lnpo.chip = 0;
		lnpo.block = j;
		lnpo.page = 0;
		lnpo.sect_bitmap = 0;
		lnpo.mdata = NULL;
		lnpo.sdata = NULL;
		ret = nci->nand_physic_bad_block_check(&lnpo);
		if (ret == 0) {
			m++;
		}
	}

	uboot_block_offset = j;

	if ((uboot_block_offset + blocks_one_uboot) > phyinfo_buf->uboot_next_block) {
		nand_free_temp_buf(kernel_buf);
		return 0;
	}

	/////////////////////////////////////////////////////

	//    uboot_flag = 0;
	//    info_flag = 0;
	//    boot_len = 0;
	page_index = 0;

	for (j = uboot_block_offset; j < phyinfo_buf->uboot_next_block; j++) {
		lnpo.chip = 0;
		lnpo.block = j;
		lnpo.page = 0;
		nand_wait_all_rb_ready();
		ret = nci->nand_physic_bad_block_check(&lnpo);
		if (ret != 0) {
			continue;
		}

		RAWNAND_DBG("read uboot many block %d!\n", lnpo.block);

		for (m = 0; m < nci->page_cnt_per_blk; m++) {
			lnpo.chip = 0;
			lnpo.block = j;
			lnpo.page = m;
			lnpo.sect_bitmap = nci->sector_cnt_per_page;
			lnpo.sdata = oob_buf;
			lnpo.slen = 64;
			if (nci->is_lsb_page(m) != 0) {
				lnpo.mdata = kernel_buf;
				nand_wait_all_rb_ready();
				ret = nci->nand_physic_read_page(&lnpo);
				if (ret == 0) {
					;
				} else if (ret == ECC_LIMIT) {
					ecc_limit = 1;
					RAWNAND_ERR("Warning. Fail in read page %d in block %d \n", lnpo.page, lnpo.block);
				} else {
					RAWNAND_ERR("error read page: %d in block %d \n", lnpo.page, lnpo.block);
					data_error = 1;
					break;
				}

				if ((oob_buf[0] != 0xff) || (oob_buf[1] != 0x00)) {
					RAWNAND_ERR("get uboot flash driver version error!\n");
					data_error = 1;
					break;
				}

				memcpy(buf + page_index * (nci->sector_cnt_per_page << 9), lnpo.mdata, nci->sector_cnt_per_page << 9);
				page_index++;
				if (total_pages == page_index) {
					break;
				}
			}
		}

		if (total_pages == page_index) {
			break;
		}
		if (data_error == 1) {
			break;
		}
	}

	nand_wait_all_rb_ready();
	nand_free_temp_buf(kernel_buf);

	if (data_error == 1) {
		ret = -1;
	} else if (ecc_limit == 1) {
		ret = 1;
	} else {
		ret = 0;
	}
	return ret;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  <0:fail 1: ecc limit
 *Note         :
 *****************************************************************************/
int rawnand_read_uboot_one(unsigned char *buf, unsigned int len, unsigned int counter)
{
	int ret = 0;
	struct nand_chip_info *nci;

	nci = g_nctri->nci;

	RAWNAND_DBG("read uboot one %d!\n", counter);

	if (((0 == nci->lsb_page_type) && (len <= nand_get_phy_block_size())) || ((0 != nci->lsb_page_type) && (len <= nand_get_lsb_block_size()))) {
		ret = rawnand_read_uboot_one_in_block(buf, len, counter);
	} else {
		ret = rawnand_read_uboot_one_in_many_block(buf, len, counter);
	}
	return ret;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :NAND_GetParam
 *****************************************************************************/
int rawnand_get_param(void *nand_param)
{
	int i;
	struct nand_chip_info *nci;
	boot_nand_para_t *nand_param_temp;

	nand_param_temp = (boot_nand_para_t *)nand_param;

	nci = g_nctri->nci;

	nand_param_temp->ChannelCnt = g_nand_storage_info->ChannelCnt;
	nand_param_temp->ChipCnt = g_nand_storage_info->ChipCnt;
	nand_param_temp->ChipConnectInfo = g_nand_storage_info->ChipConnectInfo;
	nand_param_temp->RbCnt = g_nand_storage_info->ChipConnectInfo;
	nand_param_temp->RbConnectInfo = g_nand_storage_info->RbConnectInfo;
	nand_param_temp->RbConnectMode = g_nand_storage_info->RbConnectMode;
	nand_param_temp->BankCntPerChip = g_nand_storage_info->BankCntPerChip;
	nand_param_temp->DieCntPerChip = g_nand_storage_info->DieCntPerChip;
	nand_param_temp->PlaneCntPerDie = g_nand_storage_info->PlaneCntPerDie;
	nand_param_temp->SectorCntPerPage = g_nand_storage_info->SectorCntPerPage;
	nand_param_temp->PageCntPerPhyBlk = g_nand_storage_info->PageCntPerPhyBlk;
	nand_param_temp->BlkCntPerDie = g_nand_storage_info->BlkCntPerDie;
	nand_param_temp->OperationOpt = g_nand_storage_info->OperationOpt;
	nand_param_temp->FrequencePar = g_nand_storage_info->FrequencePar;
	nand_param_temp->EccMode = g_nand_storage_info->EccMode;
	nand_param_temp->ValidBlkRatio = g_nand_storage_info->ValidBlkRatio;
	nand_param_temp->good_block_ratio = 0;
	nand_param_temp->ReadRetryType = g_nand_storage_info->ReadRetryType;
	nand_param_temp->DDRType = g_nand_storage_info->DDRType;

	nand_param_temp->random_addr_num = g_nand_storage_info->random_addr_num;
	nand_param_temp->random_cmd2_send_flag = g_nand_storage_info->random_cmd2_send_flag;
	nand_param_temp->nand_real_page_size = g_nand_storage_info->nand_real_page_size;

	//boot0 not support DDR mode, only SDR or toggle!!!!
	if ((nand_param_temp->DDRType == 0x2) || (nand_param_temp->DDRType == 0x12)) {
		RAWNAND_DBG("set ddrtype 0, freq 30Mhz\n");
		nand_param_temp->DDRType = 0;
		nand_param_temp->FrequencePar = 30;
	} else if ((nci->support_toggle_only == 0) && ((nand_param_temp->DDRType == 0x3) || (nand_param_temp->DDRType == 0x13))) {
		RAWNAND_DBG("set ddrtype 0\n");
		nand_param_temp->DDRType = 0;
		nand_param_temp->FrequencePar = 30;
	} else if (nand_param_temp->DDRType == 0x13) {
		RAWNAND_DBG("set ddrtype 3, freq 30Mhz\n");
		nand_param_temp->DDRType = 3;
		nand_param_temp->FrequencePar = 20;
	} else {
		;
	}

	for (i = 0; i < 8; i++)
		nand_param_temp->NandChipId[i] = g_nand_storage_info->NandChipId[i];

	RAWNAND_DBG("nand get param end 0x%x\n", nand_param_temp->DDRType);
	return 0;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :NAND_GetParam
 *****************************************************************************/
int rawnand_get_param_for_uboottail(void *nand_param)
{
	boot_nand_para_t *nand_param_temp;

	nand_param_temp = (boot_nand_para_t *)nand_param;

	nand_param_temp->uboot_start_block = aw_nand_info.boot->uboot_start_block;
	nand_param_temp->uboot_next_block = aw_nand_info.boot->uboot_next_block;
	nand_param_temp->logic_start_block = aw_nand_info.boot->logic_start_block;
	nand_param_temp->nand_specialinfo_page = aw_nand_info.boot->nand_specialinfo_page;
	nand_param_temp->nand_specialinfo_offset = aw_nand_info.boot->nand_specialinfo_offset;
	nand_param_temp->physic_block_reserved = aw_nand_info.boot->physic_block_reserved;

	RAWNAND_DBG("uboot_start_block:    %x\n", nand_param_temp->uboot_start_block);
	RAWNAND_DBG("uboot_next_block:   %x\n", nand_param_temp->uboot_next_block);
	RAWNAND_DBG("logic_start_block:%x\n", nand_param_temp->logic_start_block);
	RAWNAND_DBG("nand_specialinfo_page:%x\n", nand_param_temp->nand_specialinfo_page);
	RAWNAND_DBG("nand_specialinfo_offset:    %x\n", nand_param_temp->nand_specialinfo_offset);
	RAWNAND_DBG("physic_block_reserved:    %x\n", nand_param_temp->physic_block_reserved);

	return 0;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 *****************************************************************************/
__s32 RAWNAND_SetPhyArch_V3(struct _boot_info *ram_arch, void *phy_arch)
{
	struct _nand_super_storage_info *phy_arch_temp;

	phy_arch_temp = (struct _nand_super_storage_info *)phy_arch;

	ram_arch->storage_info.data.support_two_plane = phy_arch_temp->support_two_plane;
	ram_arch->storage_info.data.support_v_interleave = phy_arch_temp->support_v_interleave;
	ram_arch->storage_info.data.support_dual_channel = phy_arch_temp->support_dual_channel;

	if (phy_arch_temp->support_two_plane == 0) {
		ram_arch->storage_info.data.plane_cnt = 1;
	} else {
		ram_arch->storage_info.data.plane_cnt = 2;
	}

	//    RAWNAND_DBG("===========NAND_SetPhyArch_V3================\n");
	//    for(i=-0;i<128;i++)
	//    {
	//        RAWNAND_DBG("%x ",ram_arch[i]);
	//        if(((i+1) % 16) == 0)
	//        {
	//            RAWNAND_DBG("\n");
	//        }
	//    }
	return 0;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 *****************************************************************************/
int rawnand_update_phyarch(void)
{
	int ret;
	unsigned int id_number_ctl, para;

	g_nssi->support_two_plane = 0;
	g_nssi->support_v_interleave = 1;
	g_nssi->support_dual_channel = 1;

	nand_permanent_data.magic_data = MAGIC_DATA_FOR_PERMANENT_DATA;
	nand_permanent_data.support_two_plane = 0;
	nand_permanent_data.support_vertical_interleave = 1;
	nand_permanent_data.support_dual_channel = 1;

	RAWNAND_DBG("NAND UpdatePhyArch\n");

	g_nssi->support_two_plane = g_phy_cfg->phy_support_two_plane;
	nand_permanent_data.support_two_plane = g_nssi->support_two_plane;

	g_nssi->support_v_interleave = g_phy_cfg->phy_nand_support_vertical_interleave;
	nand_permanent_data.support_vertical_interleave = g_nssi->support_v_interleave;

	g_nssi->support_dual_channel = g_phy_cfg->phy_support_dual_channel;
	nand_permanent_data.support_dual_channel = g_nssi->support_dual_channel;

	id_number_ctl = nand_get_nand_id_number_ctrl(&aw_ndfc);
	if ((id_number_ctl & 0x0e) != 0) {
		para = nand_get_nand_extern_para(&aw_ndfc, 1);
		if ((para != 0xffffffff) && (id_number_ctl & 0x02)) {
			/*get script success*/
			if (((para & 0xffffff) == g_nctri->nci->npi->id_number) || ((para & 0xffffff) == 0xeeeeee)) {
				RAWNAND_DBG("script support_two_plane %d\n", para);
				g_nssi->support_two_plane = (para >> 24) & 0xff;
				if (g_nssi->support_two_plane == 1) {
					g_nssi->support_two_plane = 1;
				} else {
					g_nssi->support_two_plane = 0;
				}
				nand_permanent_data.support_two_plane = g_nssi->support_two_plane;
			}
		}

		para = nand_get_nand_extern_para(&aw_ndfc, 2);
		if ((para != 0xffffffff) && (id_number_ctl & 0x04)) {
			/*get script success*/
			if (((para & 0xffffff) == g_nctri->nci->npi->id_number) || ((para & 0xffffff) == 0xeeeeee)) {
				RAWNAND_DBG("script support_v_interleave %d\n", para);
				g_nssi->support_v_interleave = (para >> 24) & 0xff;
				nand_permanent_data.support_vertical_interleave = g_nssi->support_v_interleave;
			}
		}

		para = nand_get_nand_extern_para(&aw_ndfc, 3);
		if ((para != 0xffffffff) && (id_number_ctl & 0x08)) {
			/*get script success*/
			if (((para & 0xffffff) == g_nctri->nci->npi->id_number) || ((para & 0xffffff) == 0xeeeeee)) {
				RAWNAND_DBG("script support_dual_channel %d\n", para);
				g_nssi->support_dual_channel = (para >> 24) & 0xff;
				nand_permanent_data.support_dual_channel = g_nssi->support_dual_channel;
			}
		}
	}

	//ret = set_nand_structure((void*)&nand_permanent_data);
	ret = RAWNAND_SetPhyArch_V3(aw_nand_info.boot, (void *)g_nssi);

	return ret;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
int rawnand_is_blank(void)
{
	int i, ret;
	int block_blank_flag = 1;
	int read_retry_mode;
	int start_blk = 4, blk_cnt = 30;
	unsigned char oob[64];
	struct _nand_physic_op_par npo;
	struct nand_chip_info *nci;

	nci = nci_get_from_nsi(g_nsi, 0);
	read_retry_mode = (nci->npi->read_retry_type >> 16) & 0xff;

	if ((read_retry_mode != 0x32) && (read_retry_mode != 0x33) && (read_retry_mode != 0x34) && (read_retry_mode != 0x35))
		return 0;

	for (i = start_blk; i < (start_blk + blk_cnt); i++) {
		npo.chip = 0;
		npo.block = i;
		npo.page = 0;
		npo.sect_bitmap = nci->sector_cnt_per_page;
		npo.mdata = NULL;
		npo.sdata = oob;
		npo.slen = 64;

		ret = nci->nand_physic_read_page(&npo);
		if (ret >= 0) {
			if ((oob[0] == 0xff) && (oob[1] == 0xaa) && (oob[2] == 0x5c)) {
				block_blank_flag = 0;
				break;
			}

			if ((oob[0] == 0xff) && (oob[1] == 0x00)) {
				block_blank_flag = 0;
				break;
			}
		}
	}

	if (block_blank_flag == 1)
		RAWNAND_DBG("nand is blank!!\n");
	else
		RAWNAND_DBG("nand has valid data!!\n");

	return block_blank_flag;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
int nand_check_bad_block_before_first_erase(struct _nand_physic_op_par *npo)
{
	int ret;
	int read_retry_mode;
	struct nand_chip_info *nci;

	nci = nci_get_from_nsi(g_nsi, npo->chip);
	read_retry_mode = (nci->npi->read_retry_type >> 16) & 0xff;
	if ((read_retry_mode != 0x34) && (read_retry_mode != 0x35)) {
		RAWNAND_DBG("not slc program\n");
		return 1;
	}

	ret = sandisk_A19_check_bad_block_first_burn(npo);

	return ret;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 *****************************************************************************/
int rawnand_erase_chip(unsigned int chip, unsigned int start_block, unsigned int end_block, unsigned int force_flag)
{
	int ret, i;
	int read_retry_mode;
	struct _nand_physic_op_par npo;
	struct nand_chip_info *nci;

	nci = nci_get_from_nsi(g_nsi, chip);
	read_retry_mode = (nci->npi->read_retry_type >> 16) & 0xff;

	if ((end_block >= nci->blk_cnt_per_chip) || (end_block == 0))
		end_block = nci->blk_cnt_per_chip;

	if (start_block > end_block)
		return 0;

	for (i = start_block; i < end_block; i++) {
		npo.chip = nci->chip_no;
		npo.block = i;
		npo.page = 0;
		npo.sect_bitmap = nci->sector_cnt_per_page;
		npo.mdata = NULL;
		npo.sdata = NULL;

		if ((read_retry_mode == 0x32) || (read_retry_mode == 0x33)) {
			ret = 0;
		} else if ((read_retry_mode == 0x34) || (read_retry_mode == 0x35)) {
			ret = nand_check_bad_block_before_first_erase(&npo);
			if (ret != 0)
				RAWNAND_DBG("find a bad block %d %d\n", npo.chip, npo.block);
			if (force_flag == 1)
				ret = 0;
		} else {
			ret = nci->nand_physic_bad_block_check(&npo);
			if (force_flag == 1)
				ret = 0;
		}

		if (ret == 0) {
			ret = nci->nand_physic_erase_block(&npo);
			nand_wait_all_rb_ready();

			if (ret != 0)
				nci->nand_physic_bad_block_mark(&npo);
		}
	}

	return 0;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
void rawnand_erase_special_block(void)
{
	int i, j, flag, ret;
	struct _nand_physic_op_par npo;
	u8 oob[64];
	struct nand_chip_info *nci;

	nci = nci_get_from_nsi(g_nsi, 0);

	for (j = 0; j < g_nsi->chip_cnt; j++) {
		for (i = 7; i < 50; i++) {
			flag = 0;

			npo.chip = j;
			npo.block = i;
			npo.page = 0;
			npo.sect_bitmap = nci->sector_cnt_per_page;
			npo.mdata = NULL;
			npo.sdata = NULL;

			ret = nci->nand_physic_bad_block_check(&npo);
			if (ret != 0) {
				npo.chip = j;
				npo.block = i;
				npo.page = 0;
				npo.sect_bitmap = nci->sector_cnt_per_page;
				npo.mdata = NULL;
				npo.sdata = oob;
				npo.slen = 16;
				ret = nci->nand_physic_read_page(&npo);

				if ((oob[1] == 0x78) && (oob[2] == 0x69) && (oob[3] == 0x87) && (oob[4] == 0x41) && (oob[5] == 0x52) && (oob[6] == 0x43) && (oob[7] == 0x48)) {
					flag = 1;
					RAWNAND_DBG("this is nand struct block %d\n", i);
				}

				if ((oob[1] == 0x50) && (oob[2] == 0x48) && (oob[3] == 0x59) && (oob[4] == 0x41) && (oob[5] == 0x52) && (oob[6] == 0x43) && (oob[7] == 0x48)) {
					flag = 1;
					RAWNAND_DBG("this is old nand struct block %d\n", i);
				}

				if ((oob[0] == 0x00) && (oob[1] == 0x4F) && (oob[2] == 0x4F) && (oob[3] == 0x42)) {
					flag = 1;
					RAWNAND_DBG("this is hynix otp data save block %d\n", i);
				}

				if (flag == 1) {
					ret = nci->nand_physic_erase_block(&npo);
					nand_wait_all_rb_ready();
					if (ret != 0) {
						nci->nand_physic_bad_block_mark(&npo);
					}
				}
			}
		}
	}

	return;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 *****************************************************************************/
int rawnand_uboot_erase_all_chip(UINT32 force_flag)
{
	int i, start, end, secure_block_start;
	int uboot_start_block, uboot_next_block;

	uboot_start_block = get_uboot_start_block();
	uboot_next_block = get_uboot_next_block();
	secure_block_start = uboot_next_block;

	for (i = 0; i < g_nsi->chip_cnt; i++) {
		start = 0;
		end = 0xfffff;

		if (i == 0) {
			if ((force_flag == 1) || (rawnand_is_blank()) == 1)
				start = secure_block_start;
			else
				start = nand_secure_storage_first_build(secure_block_start);
		}

		rawnand_erase_chip(i, start, end, force_flag);
	}

	rawnand_erase_special_block();

	clean_physic_info();

	aw_nand_info.boot->uboot_start_block = uboot_start_block;
	aw_nand_info.boot->uboot_next_block = uboot_next_block;

	return 0;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 *****************************************************************************/
int rawnand_dragonborad_test_one(unsigned char *buf, unsigned char *oob, unsigned int blk_num)
{
	int ret;
	unsigned int j;
	unsigned char oob_buf[64];
	struct _nand_physic_op_par lnpo;
	struct nand_chip_info *nci;

	//	RAWNAND_DBG("dragonborad test in one block!\n");

	nci = g_nctri->nci;

	lnpo.chip = 0;
	lnpo.block = blk_num;
	lnpo.page = 0;

	nand_wait_all_rb_ready();

	ret = nci->nand_physic_erase_block(&lnpo);
	if (ret) {
		RAWNAND_ERR("Fail in erasing block %d!\n", lnpo.block);
		return 0;
	}

	for (j = 0; j < nci->page_cnt_per_blk; j++) {
		lnpo.chip = 0;
		lnpo.block = blk_num;
		lnpo.page = j;
		lnpo.sect_bitmap = nci->sector_cnt_per_page;
		lnpo.sdata = oob;
		lnpo.slen = 64;

		lnpo.mdata = buf;

		nand_wait_all_rb_ready();

		if (nci->nand_physic_write_page(&lnpo) != 0) {
			RAWNAND_ERR("Warning. Fail in writing page %d in block %d.\n", lnpo.page, lnpo.block);
		}
	}
	for (j = 0; j < nci->page_cnt_per_blk; j++) {
		lnpo.chip = 0;
		lnpo.block = blk_num;
		lnpo.page = j;
		lnpo.sdata = (unsigned char *)oob_buf;
		lnpo.sect_bitmap = nci->sector_cnt_per_page;
		lnpo.slen = 64;
		lnpo.mdata = buf;

		memset(oob_buf, 0xff, 64);

		if (nci->nand_physic_read_page(&lnpo) != 0) {
			RAWNAND_ERR("Warning. Fail in read page %d in block %d.\n", j, lnpo.block);
			return -1;
		}
		if ((oob_buf[0] != oob[0]) || (oob_buf[1] != oob[1]) || (oob_buf[2] != oob[2]) || (oob_buf[3] != oob[3])) {
			RAWNAND_ERR("oob data error\n!");
			return -1;
		}
	}

	nand_wait_all_rb_ready();
	return 0;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok   1:ecc limit     -1:fail
 *Note         :
 *****************************************************************************/
int change_uboot_start_block(struct _boot_info *info, unsigned int start_block)
{
	if (nand_secure_storage_block != 0) {
		if (info->storage_info.data.support_two_plane == 1) {
			info->uboot_start_block = NAND_UBOOT_BLK_START;
			info->uboot_next_block = NAND_UBOOT_BLK_START + NAND_UBOOT_BLK_CNT;
			info->logic_start_block = (nand_secure_storage_block_bak / 2) + 1 + PHYSIC_RECV_BLOCK / 2;
			info->no_use_block = info->logic_start_block;
		} else {
			info->uboot_start_block = NAND_UBOOT_BLK_START;
			info->uboot_next_block = NAND_UBOOT_BLK_START + NAND_UBOOT_BLK_CNT;
			info->logic_start_block = nand_secure_storage_block_bak + 1 + PHYSIC_RECV_BLOCK;
			info->no_use_block = info->logic_start_block;
		}
		return 0;
	} else {
		//first build
		RAWNAND_ERR("change_uboot_start_block error!!!! \n");
		return -1;
	}
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
int rawnand_physic_info_get_one_copy(unsigned int start_block, unsigned int pages_offset, unsigned int *block_per_copy, unsigned int *buf)
{
	unsigned int page, block;
	unsigned int flag;
	unsigned int size_per_page, pages_per_block, lsbblock_size;
	unsigned char sdata[64];
	int ret;
	unsigned int phyinfo_page_cnt, page_cnt;
	unsigned int badblk_num = 0;
	unsigned int pages_per_phyinfo;
	void *tempbuf = NULL;
	struct _nand_physic_op_par lnpo;
	struct nand_chip_info *nci = g_nctri->nci;

	RAWNAND_DBG("physic_info_get_one_copy start!!\n");

	size_per_page = nci->sector_cnt_per_page << 9;
	pages_per_block = nci->page_cnt_per_blk;

	pages_per_phyinfo = PHY_INFO_SIZE / size_per_page;
	if (PHY_INFO_SIZE % size_per_page)
		pages_per_phyinfo++;

	//	RAWNAND_DBG("pages_per_phyinfo %d\n",pages_per_phyinfo);

	tempbuf = nand_get_temp_buf(32 * 1024);
	if (tempbuf == NULL) {
		RAWNAND_ERR("tempbuf malloc fail\n");
		return -1;
	}

	page_cnt = 0;
	phyinfo_page_cnt = 0;
	flag = 0;
	for (block = start_block;; block++) {
		lnpo.chip = 0;
		lnpo.block = block;
		lnpo.page = 0;
		lnpo.sdata = sdata;
		lnpo.sect_bitmap = 0;
		lnpo.slen = 64;
		lnpo.mdata = NULL;

		ret = nci->nand_physic_read_page(&lnpo);
		if (sdata[0] == 0x0) {
			badblk_num++;
			RAWNAND_DBG("bad block:chip %d block %d\n", lnpo.chip, lnpo.block);
			continue;
		}
		for (page = 0; page < pages_per_block; page++) {
			if (nci->is_lsb_page(page) == 1) {

				if (page_cnt >= pages_offset) {
					lnpo.chip = 0;
					lnpo.block = block;
					lnpo.page = page;
					lnpo.sdata = sdata;
					lnpo.sect_bitmap = nci->sector_cnt_per_page;
					lnpo.slen = 64;
					lnpo.mdata = (void *)((char *)tempbuf + phyinfo_page_cnt * size_per_page);
					RAWNAND_DBG("block %d page %d\n", lnpo.block, lnpo.page);
					ret = nci->nand_physic_read_page(&lnpo);
					if (ret == ERR_ECC) {
						RAWNAND_ERR("ecc err:chip %d block %d page %d\n", lnpo.chip, lnpo.block, lnpo.page);
						break;
					}

					phyinfo_page_cnt++;
					if (phyinfo_page_cnt == pages_per_phyinfo) {
						flag = 1;
						break;
					}
				}
				page_cnt++;
			}
		}

		if (ret == ERR_ECC)
			break;

		if (flag == 1)
			break;
	}

	memcpy(buf, tempbuf, PHY_INFO_SIZE);

	lsbblock_size = nand_get_lsb_block_size();

	*block_per_copy = (pages_offset + pages_per_phyinfo) * size_per_page / lsbblock_size + badblk_num;
	if (((pages_offset + pages_per_phyinfo) * size_per_page) % lsbblock_size)
		*block_per_copy = (*block_per_copy) + 1;
	//	RAWNAND_DBG("block_per_copy %d pages_offset+pages_per_phyinfo %d\n",*block_per_copy,(pages_offset+pages_per_phyinfo));

	if (tempbuf) {
		nand_free_temp_buf(tempbuf);
	}

	return 0;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
int rawnand_add_len_to_uboot_tail(unsigned int uboot_size)
{
	unsigned int size_per_page, pages_per_uboot, page_no, lsb_pages, page_in_lsb_block;
	unsigned int jump_size;

	size_per_page = g_nctri->nci->sector_cnt_per_page << 9;

	pages_per_uboot = uboot_size / size_per_page;
	if (uboot_size % size_per_page)
		pages_per_uboot++;

	jump_size = size_per_page * pages_per_uboot - uboot_size;

	lsb_pages = nand_get_lsb_pages();

	page_in_lsb_block = pages_per_uboot % lsb_pages;

	if (size_per_page == 8192) {
		page_no = nand_get_pageno(page_in_lsb_block + 1);
		phyinfo_buf->nand_specialinfo_page = page_no;
		phyinfo_buf->nand_specialinfo_offset = 512 * 3;
	} else if (size_per_page == 16384) {
		page_no = nand_get_pageno(page_in_lsb_block);
		phyinfo_buf->nand_specialinfo_page = page_no;
		phyinfo_buf->nand_specialinfo_offset = 512 * 19;
	}

	return (uboot_size + jump_size + PHY_INFO_SIZE);
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :
 *****************************************************************************/
int set_hynix_special_info(void)
{
	u32 i, chip_cnt;
	u8 sum;
	u32 len;
	u8 *srcbuf;
	u8 read_retry_mode;
	struct nand_chip_info *nci = g_nsi->nci;

	read_retry_mode = (nci->npi->read_retry_type >> 16) & 0xff;
	if ((read_retry_mode == 2) || (read_retry_mode == 3)) {
		len = 64;
	} else if (read_retry_mode == 4) {
		len = 32;
	} else {
		return -1;
	}

	chip_cnt = 0;
	sum = 0;
	while (nci != NULL) {
		srcbuf = (u8 *)(nci->readretry_value);
		for (i = 0; i < len; i++) {
			sum += srcbuf[i];
			aw_nand_info.boot->nand_special_info.data[i + 2 + chip_cnt * len] = srcbuf[i];
		}
		chip_cnt++;
		nci = nci->nsi_next;
	}

	aw_nand_info.boot->nand_special_info.data[0] = 0xa5;

	for (i = (chip_cnt * len + 2); i < 1024; i++) {
		aw_nand_info.boot->nand_special_info.data[i] = 0;
	}

	aw_nand_info.boot->nand_special_info.data[1] = sum;

	return 0;
}
