/*
 * rawnand_base.c for sunxi rawnand base
 *
 * Copyright (C) 2019 Allwinner.
 * SPDX-License-Identifier: GPL-2.0
 * 2019.9.11 cuizhikui<cuizhikui@allwinnertech.com>
 *
 *                    eNand
 *             Nand flash driver scan module
 *      Copyright(C), 2008-2009, SoftWinners Microelectronic Co., Ltd.
 *              All Rights Reserved
 */

#include "rawnand_base.h"
#include "../../nfd/nand_osal_for_linux.h"
/*#include "../nand_boot.h"*/
#include "../nand-partition3/sunxi_nand_boot.h"
#include "../nand_errno.h"
#include "../nand_physic_interface.h"
#include "../nand_secure_storage.h"
#include "../version.h"
#include "controller/ndfc_base.h"
#include "controller/ndfc_ops.h"
#include "controller/ndfc_timings.h"
#include "rawnand.h"
#include "rawnand_boot.h"
#include "rawnand_cfg.h"
#include "rawnand_chip.h"
#include "rawnand_debug.h"
#include <linux/time.h>

extern void nand_common1_show_version(void);
extern int nand_enable_voltage(struct sunxi_ndfc *ndfc);
extern int nand_disable_voltage(struct sunxi_ndfc *ndfc);


void *g_nreg_base;
struct _nand_temp_buf ntf = {0};
struct _nand_permanent_data nand_permanent_data = {

    MAGIC_DATA_FOR_PERMANENT_DATA,
    0,
};

/*
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok 1:find good block 2:old arch -1:fail
 *Note         :
 */
int read_nand_structure(void *phy_arch, unsigned int *good_blk_no)
{
	int i, retry = 3, ret, ret2 = -1;
	unsigned int b, chip = 0;
	unsigned int start_blk = 12, blk_cnt = 20;
	unsigned char oob[64];
	struct _nand_permanent_data *parch;
	struct _nand_physic_op_par npo;

	parch = (struct _nand_permanent_data *)nand_get_temp_buf(64 * 1024);

	for (b = start_blk; b < start_blk + blk_cnt; b++) {
		for (i = 0; i < retry; i++) {
			npo.chip = chip;
			npo.block = b;
			npo.page = i;
			npo.mdata = (u8 *)parch;
			npo.sect_bitmap = g_nctri->nci->sector_cnt_per_page;
			npo.sdata = oob;
			npo.slen = g_nctri->nci->sdata_bytes_per_page;

			ret = g_nctri->nci->nand_physic_read_page(&npo);
			if (oob[0] == 0x00) {
				if (ret >= 0) {
					if ((ret >= 0) && (oob[1] == 0x78) && (oob[2] == 0x69) && (oob[3] == 0x87) && (oob[4] == 0x41) && (oob[5] == 0x52) && (oob[6] == 0x43) && (oob[7] == 0x48)) {
						memcpy(phy_arch, parch, sizeof(struct _nand_permanent_data));
						RAWNAND_DBG("search nand structure %d  %d: get last physic arch ok 0x%x 0x%x!\n", npo.block, npo.page, parch->support_two_plane, parch->support_vertical_interleave);
						ret2 = 0;
						goto search_end;
					}
					if ((ret >= 0) && (oob[1] == 0x50) && (oob[2] == 0x48) && (oob[3] == 0x59) && (oob[4] == 0x41) && (oob[5] == 0x52) && (oob[6] == 0x43) && (oob[7] == 0x48)) {
						memcpy(phy_arch, parch, sizeof(struct _nand_permanent_data));
						RAWNAND_DBG("search nand structure %d  %d: get old physic arch ok 0x%x 0x%x!\n", npo.block, npo.page, parch->support_two_plane, parch->support_vertical_interleave);
						ret2 = 2;
						goto search_end;
					}
				} else {
					RAWNAND_DBG("search nand structure: bad block no physic arch info!\n");
				}
			} else if (oob[0] == 0xff) {
				if ((ret >= 0) && (i == 0)) {
					RAWNAND_DBG("search nand structure: find a good block: %d, but no physic arch info.\n", npo.block);
					ret2 = 1;
					goto search_end;
				} else {
					RAWNAND_DBG("search nand structure: blank block!\n");
				}
			} else {
				RAWNAND_DBG("search nand structure: unkonwn 0x%x!\n", oob[0]);
			}
		}
	}

search_end:

	if (b == (start_blk + blk_cnt)) {
		ret2 = ERR_NO_58;
		*good_blk_no = 0;
	} else {
		*good_blk_no = b;
	}

	nand_free_temp_buf((u8 *)parch);

	return ret2;
}

/*
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 */
int write_nand_structure(void *phy_arch, unsigned int good_blk_no, unsigned int blk_cnt)
{
	int retry = 3, ret, ret2 = ERR_NO_130;
	unsigned int b, p, ok, chip = 0;
	unsigned char oob[64];
	struct _nand_physic_op_par npo;

	RAWNAND_DBG("write nand structure: write physic arch to blk %d...\n", good_blk_no);

	for (b = good_blk_no; b < good_blk_no + blk_cnt; b++) {
		npo.chip = chip;
		npo.block = b;
		npo.page = 0;
		npo.mdata = (unsigned char *)phy_arch;
		npo.sect_bitmap = g_nctri->nci->sector_cnt_per_page;
		npo.sdata = oob;
		npo.slen = g_nctri->nci->sdata_bytes_per_page;

		ret = g_nctri->nci->nand_physic_erase_block(&npo); //PHY_SimpleErase_CurCH(&nand_op);
		if (ret < 0) {
			RAWNAND_ERR("write nand structure: erase chip %d, block %d error\n", npo.chip, npo.block);
			memset(oob, 0, 64);

			for (p = 0; p < g_nctri->nci->page_cnt_per_blk; p++) {
				npo.page = p;
				ret = g_nctri->nci->nand_physic_write_page(&npo);
				nand_wait_all_rb_ready();
				if (ret < 0) {
					RAWNAND_ERR("write nand structure: mark bad block, write chip %d, block %d, page %d error\n", npo.chip, npo.block, npo.page);
				}
			}
		} else {
			RAWNAND_DBG("write nand structure: erase block %d ok.\n", b);
			memset(oob, 0x88, 64);
			oob[0] = 0x00; //bad block flag
			oob[1] = 0x78; //'N'
			oob[2] = 0x69; //'E'
			oob[3] = 0x87; //'W'
			oob[4] = 0x41; //'A'
			oob[5] = 0x52; //'R'
			oob[6] = 0x43; //'C'
			oob[7] = 0x48; //'H'

			for (ok = 0, p = 0; p < g_nctri->nci->page_cnt_per_blk; p++) {
				npo.page = p;
				ret = g_nctri->nci->nand_physic_write_page(&npo);
				nand_wait_all_rb_ready();
				if (ret != 0) {
					RAWNAND_ERR("write nand structure: write chip %d, block %d, page %d error\n", npo.chip, npo.block, npo.page);
				} else {
					if (p < retry) {
						ok = 1;
					}
				}
			}

			if (ok == 1) {
				ret2 = 0;
				break;
			}
		}
	}

	return ret2;
}

/*
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         : only for erase boot
 */
int set_nand_structure(void *phy_arch)
{
	int ret;
	unsigned good_blk_no = 12;
	void *data;
	//	struct _nand_permanent_data *parch;
	//	struct __RAWNandStorageInfo_t *old_parch;

	data = (void *)nand_get_temp_buf(4096);

	ret = read_nand_structure(data, &good_blk_no);
	if (ret == 1) {
		RAWNAND_DBG("search nand structure: ok arch\n");
	} else if (ret == 2) {
		RAWNAND_DBG("search nand structure: old arch\n");
		//        old_parch =  (struct __NandStorageInfo_t *)data;
		//        parch = (struct _nand_permanent_data *)phy_arch;
		//
		//        parch->magic_data = MAGIC_DATA_FOR_PERMANENT_DATA;
		//        parch->support_two_plane = 0;
		//        parch->support_vertical_interleave = 1;
		//        parch->support_dual_channel = 1;
		//        if(old_parch->PlaneCntPerDie == 2)
		//        {
		//            parch->support_two_plane = 1;
		//        }
	} else if (ret == 0) {
		RAWNAND_DBG("never be here! store nand structure\n");
	} else {
		RAWNAND_ERR("search nand structure: can not find good block: 12~112\n");
		nand_free_temp_buf((u8 *)data);
		return ret;
	}

	ret = write_nand_structure(phy_arch, good_blk_no, 20);
	if (ret != 0) {
		RAWNAND_ERR("write nand structure fail1\n");
	}

	ret = read_nand_structure(data, &good_blk_no);
	if (ret != 0) {
		RAWNAND_ERR("write nand structure: can not find nand structure: 12~112\n");
	}

	nand_free_temp_buf((u8 *)data);

	return 0;
}

__u32 rawnand_get_lsb_block_size(void)
{
	__u32 i, count = 0;
	struct nand_chip_info *nci;

	nci = g_nctri->nci;

	for (i = 0; i < nci->page_cnt_per_blk; i++) {
		if (1 == nci->is_lsb_page(i))
			count++;
	}
	return count * (nci->sector_cnt_per_page << 9);
}

__u32 rawnand_get_lsb_pages(void)
{
	__u32 i, count = 0;
	struct nand_chip_info *nci;

	nci = g_nctri->nci;

	for (i = 0; i < nci->page_cnt_per_blk; i++) {
		if (1 == nci->is_lsb_page(i))
			count++;
	}
	return count;
}

__u32 rawnand_get_phy_block_size(void)
{
	struct nand_chip_info *nci;

	nci = g_nctri->nci;
	return nci->page_cnt_per_blk * (nci->sector_cnt_per_page << 9);
}


__u32 rawnand_used_lsb_pages(void)
{
	__u32 i, count = 0;
	struct nand_chip_info *nci;

	nci = g_nctri->nci;

	for (i = 0; i < nci->page_cnt_per_blk; i++) {
		if (1 == nci->is_lsb_page(i))
			count++;
	}

	if (count == nci->page_cnt_per_blk) {
		return 0;
	}
	return 1;
}

u32 rawnand_get_pageno(u32 lsb_page_no)
{
	u32 i, count = 0;
	struct nand_chip_info *nci;

	nci = g_nctri->nci;

	for (i = 0; i < nci->page_cnt_per_blk; i++) {
		if (1 == nci->is_lsb_page(i))
			count++;
		if (count == (lsb_page_no + 1))
			break;
	}
	return i;
}

__u32 rawnand_get_page_cnt_per_block(void)
{
	return g_nctri->nci->page_cnt_per_blk;
}

__u32 rawnand_get_page_size(void)
{
	return ((g_nctri->nci->sector_cnt_per_page) * 512);
}

__u32 rawnand_get_twoplane_flag(void)
{
	return g_nssi->nsci->two_plane;
}

int nand_read_scan_data(unsigned int chip, unsigned int block, unsigned int page, unsigned int bitmap, unsigned char *mbuf, unsigned char *sbuf)
{
	return nand_physic_read_page(chip, block, page, bitmap, mbuf, sbuf);
}
/*
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 */
int rawnand_physic_erase_block(unsigned int chip, unsigned int block)
{
	int ret = 0;
	struct nand_chip_info *nci;
	struct _nand_physic_op_par npo;

	nci = nci_get_from_nsi(g_nsi, chip);

	npo.chip = chip;
	npo.block = block;
	npo.page = 0;
	npo.sect_bitmap = 0;
	npo.mdata = NULL;
	npo.sdata = NULL;
	npo.slen = 0;
	ret = nci->nand_physic_erase_block(&npo);

	return ret;
}

/*
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 */
int rawnand_physic_read_page(unsigned int chip, unsigned int block, unsigned int page, unsigned int bitmap, unsigned char *mbuf, unsigned char *sbuf)
{
	int ret = 0;
	struct nand_chip_info *nci;
	struct _nand_physic_op_par npo;

	nci = nci_get_from_nsi(g_nsi, chip);

	npo.chip = chip;
	npo.block = block;
	npo.page = page;
	//npo.sect_bitmap = bitmap;
	/*npo.sect_bitmap = g_nsi->nci->sector_cnt_per_page;*/
	npo.sect_bitmap = bitmap;
	npo.mdata = mbuf;
	npo.sdata = sbuf;
	npo.slen = nci->sdata_bytes_per_page;

	ret = nci->nand_physic_read_page(&npo);

	return ret;
}

/*
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 */
int rawnand_physic_write_page(unsigned int chip, unsigned int block, unsigned int page, unsigned int bitmap, unsigned char *mbuf, unsigned char *sbuf)
{
	int ret = 0;
	struct nand_chip_info *nci;
	struct _nand_physic_op_par npo;

	nci = nci_get_from_nsi(g_nsi, chip);

	npo.chip = chip;
	npo.block = block;
	npo.page = page;
	//npo.sect_bitmap = bitmap;
	/*npo.sect_bitmap = g_nsi->nci->sector_cnt_per_page;*/
	npo.sect_bitmap = bitmap;
	npo.mdata = mbuf;
	npo.sdata = sbuf;
	npo.slen = nci->sdata_bytes_per_page;
	ret = nci->nand_physic_write_page(&npo);







	return ret;
}

/*
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 */
int rawnand_physic_bad_block_check(unsigned int chip, unsigned int block)
{
	int ret;
	struct nand_chip_info *nci;
	struct _nand_physic_op_par npo;

	npo.chip = chip;
	npo.block = block;
	npo.page = 0;
	npo.sect_bitmap = 0;
	npo.mdata = NULL;
	npo.sdata = NULL;
	nci = nci_get_from_nsi(g_nsi, chip);
	ret = nci->nand_physic_bad_block_check(&npo);

	return ret;
}

/*
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 */
int rawnand_physic_bad_block_mark(unsigned int chip, unsigned int block)
{
	int ret;
	struct nand_chip_info *nci;
	struct _nand_physic_op_par npo;

	npo.chip = chip;
	npo.block = block;
	npo.page = 0;
	npo.sect_bitmap = 0;
	npo.mdata = NULL;
	npo.sdata = NULL;
	nci = nci_get_from_nsi(g_nsi, chip);
	ret = nci->nand_physic_bad_block_mark(&npo);

	return ret;
}

/*
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 */
int rawnand_physic_erase_super_block(unsigned int chip, unsigned int block)
{
	int ret;
	struct nand_super_chip_info *nsci;
	struct _nand_physic_op_par npo;

	npo.chip = chip;
	npo.block = block;
	npo.page = 0;
	npo.sect_bitmap = 0;
	npo.mdata = NULL;
	npo.sdata = NULL;
	npo.slen = 0;
	nsci = nsci_get_from_nssi(g_nssi, chip);
	ret = nsci->nand_physic_erase_super_block(&npo);

	return ret;
}

/*
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 */
int rawnand_physic_read_super_page(unsigned int chip, unsigned int block, unsigned int page, unsigned int bitmap, unsigned char *mbuf, unsigned char *sbuf)
{
	int ret;
	struct nand_super_chip_info *nsci;
	struct _nand_physic_op_par npo;

	nsci = nsci_get_from_nssi(g_nssi, chip);

	npo.chip = chip;
	npo.block = block;
	npo.page = page;
	npo.sect_bitmap = bitmap;
	npo.mdata = mbuf;
	npo.sdata = sbuf;
	npo.slen = nsci->spare_bytes;
	ret = nsci->nand_physic_read_super_page(&npo);

	return ret;
}

/*
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 */
int rawnand_physic_write_super_page(unsigned int chip, unsigned int block, unsigned int page, unsigned int bitmap, unsigned char *mbuf, unsigned char *sbuf)
{
	int ret;
	struct nand_super_chip_info *nsci;
	struct _nand_physic_op_par npo;

	nsci = nsci_get_from_nssi(g_nssi, chip);

	npo.chip = chip;
	npo.block = block;
	npo.page = page;
	npo.sect_bitmap = bitmap;
	npo.mdata = mbuf;
	npo.sdata = sbuf;
	npo.slen = nsci->spare_bytes;
	ret = nsci->nand_physic_write_super_page(&npo);

	return ret;
}

/*
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 */
int rawnand_physic_super_bad_block_check(unsigned int chip, unsigned int block)
{
	int ret;
	struct nand_super_chip_info *nsci;
	struct _nand_physic_op_par npo;

	nsci = nsci_get_from_nssi(g_nssi, chip);

	npo.chip = chip;
	npo.block = block;
	npo.page = 0;
	npo.sect_bitmap = 0;
	npo.mdata = NULL;
	npo.sdata = NULL;
	npo.slen = 0;
	ret = nsci->nand_physic_super_bad_block_check(&npo);

	return ret;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       : 0:ok  -1:fail
*Note         :
*****************************************************************************/
int rawnand_physic_super_bad_block_mark(unsigned int chip, unsigned int block)
{
	int ret;
	struct nand_super_chip_info *nsci;
	struct _nand_physic_op_par npo;

	nsci = nsci_get_from_nssi(g_nssi, chip);

	npo.chip = chip;
	npo.block = block;
	npo.page = 0;
	npo.sect_bitmap = 0;
	npo.mdata = NULL;
	npo.sdata = NULL;
	npo.slen = 0;
	ret = nsci->nand_physic_super_bad_block_mark(&npo);

	return ret;
}

/*
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 **/
int nand_write_data_in_whole_block(unsigned int chip, unsigned int block, unsigned char *mbuf, unsigned int mlen, unsigned char *sbuf, unsigned int slen)
{
	int ret, i, flag;
	struct nand_chip_info *nci;
	struct _nand_physic_op_par npo;
	unsigned char spare[64];

	nci = nci_get_from_nsi(g_nsi, chip);

	npo.chip = chip;
	npo.block = block;
	npo.page = 0;
	npo.sect_bitmap = nci->sector_cnt_per_page;
	npo.mdata = NULL;
	npo.sdata = NULL;
	npo.slen = 0;

	ret = nci->nand_physic_erase_block(&npo);
	if (ret != 0) {
		RAWNAND_ERR("nand_write_data_in block error1 chip:0x%x block:0x%x \n", chip, block);
		return ret;
	}
	nand_wait_all_rb_ready();

	memcpy(spare, sbuf, nci->sdata_bytes_per_page);

	flag = ERR_NO_132;
	for (i = 0; i < nci->page_cnt_per_blk; i++) {
		npo.chip = chip;
		npo.block = block;
		npo.page = i;
		npo.sect_bitmap = nci->sector_cnt_per_page;
		npo.mdata = mbuf;
		npo.sdata = spare;
		npo.slen = nci->sdata_bytes_per_page;
		ret = nci->nand_physic_write_page(&npo);
		nand_wait_all_rb_ready();
		if (ret == 0) {
			flag = 0;
		} else {
			RAWNAND_ERR("nand_write_data_in block error2 chip:0x%x block:0x%x page:0x%x \n", chip, block, i);
		}
	}

	return flag;
}

/*
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 */
int nand_read_data_in_whole_block(unsigned int chip, unsigned int block, unsigned char *mbuf, unsigned int mlen, unsigned char *sbuf, unsigned int slen)
{
	int ret, i, flag, page_size;
	struct nand_chip_info *nci;
	struct _nand_physic_op_par npo;
	unsigned char spare[64];
	unsigned char *buf;

	page_size = g_nsi->nci->sector_cnt_per_page << 9;

	if (mlen < page_size) {
		buf = nand_get_temp_buf(page_size);
	} else {
		buf = mbuf;
	}

	nci = nci_get_from_nsi(g_nsi, chip);

	flag = ERR_NO_132;
	for (i = 0; i < nci->page_cnt_per_blk; i++) {
		npo.chip = chip;
		npo.block = block;
		npo.page = i;
		npo.sect_bitmap = nci->sector_cnt_per_page;
		npo.mdata = buf;
		npo.sdata = spare;
		npo.slen = nci->sdata_bytes_per_page;
		ret = nci->nand_physic_read_page(&npo);
		if (ret >= 0) {
			flag = 0;
			break;
		} else {
			RAWNAND_ERR("nand_read_data_in block error2 chip:0x%x block:0x%x page:0x%x \n", chip, block, i);
		}
	}

	memcpy(sbuf, spare, slen);
	if (mlen < page_size) {
		memcpy(mbuf, buf, mlen);
		nand_free_temp_buf(buf);
	}

	return flag;
}

/*
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 */
int rawnand_physic_block_copy(unsigned int chip_s, unsigned int block_s, unsigned int chip_d, unsigned int block_d)
{
	int i, ret = 0;
	unsigned char spare[64];
	unsigned char *buf;

	buf = (unsigned char *)nand_get_temp_buf(g_nsi->nci->sector_cnt_per_page << 9);

	for (i = 0; i < g_nsi->nci->page_cnt_per_blk; i++) {
		ret |= nand_physic_read_page(chip_s, block_s, i, g_nsi->nci->sector_cnt_per_page, buf, spare);
		ret |= nand_physic_write_page(chip_d, block_d, i, g_nsi->nci->sector_cnt_per_page, buf, spare);
	}

	nand_free_temp_buf(buf);
	return ret;
}

/*
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 */
int get_nand_structure(struct _nand_super_storage_info *nssi)
{
	nssi->support_two_plane = 1;
	nssi->support_v_interleave = 1;
	nssi->support_dual_channel = 1;
#if 0
	int good_block, ret;

	rawnand_storage_info_t *nand_storage;

	nssi->support_two_plane = 1;
	nssi->support_v_interleave = 1;
	nssi->support_dual_channel = 1;

	if (is_phyinfo_empty(phyinfo_buf) != 1) {
		nssi->support_two_plane = phyinfo_buf->storage_info.data.support_two_plane;
		nssi->support_v_interleave = phyinfo_buf->storage_info.data.support_v_interleave;
		nssi->support_dual_channel = phyinfo_buf->storage_info.data.support_dual_channel;
		return 0;
	}

	ret = read_nand_structure((void *)(&nand_permanent_data), (u32 *)(&good_block));
	if ((ret == 0) || (ret == 2)) {
		RAWNAND_DBG("get nand structure ok!\n");
		if (nand_permanent_data.magic_data == MAGIC_DATA_FOR_PERMANENT_DATA) {
			RAWNAND_DBG("get nand structure 1!\n");
			nssi->support_two_plane = nand_permanent_data.support_two_plane;
			nssi->support_v_interleave = nand_permanent_data.support_vertical_interleave;
			nssi->support_dual_channel = nand_permanent_data.support_dual_channel;
		} else {
			nand_storage = (rawnand_storage_info_t *)(&nand_permanent_data);
			if (nand_storage->PlaneCntPerDie == 2) {
				RAWNAND_DBG("get nand structure 2 %d!\n", nand_storage->PlaneCntPerDie);
				nssi->support_two_plane = 1;
				memset((void *)&nand_permanent_data, 0, sizeof(struct _nand_permanent_data));
				nand_permanent_data.magic_data = MAGIC_DATA_FOR_PERMANENT_DATA;
				nand_permanent_data.support_two_plane = 1;
				nand_permanent_data.support_vertical_interleave = 1;
				nand_permanent_data.support_dual_channel = 1;
			} else {
				RAWNAND_DBG("get nand structure 3 0x%x!\n", nand_storage->PlaneCntPerDie);
				memset((void *)&nand_permanent_data, 0, sizeof(struct _nand_permanent_data));
				nand_permanent_data.magic_data = MAGIC_DATA_FOR_PERMANENT_DATA;
				nand_permanent_data.support_two_plane = 0;
				nand_permanent_data.support_vertical_interleave = 1;
				nand_permanent_data.support_dual_channel = 1;
			}
		}
	}
	//    else
	//    {
	//        RAWNAND_ERR("get nand structure fail!\n");
	//        return ERR_NO_131;
	//    }

	phyinfo_buf->storage_info.data.support_two_plane = nssi->support_two_plane;
	phyinfo_buf->storage_info.data.support_v_interleave = nssi->support_v_interleave;
	phyinfo_buf->storage_info.data.support_dual_channel = nssi->support_dual_channel;

#endif
	return 0;
}
/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :the return pointer cannot be modified! must call by pair!
 *****************************************************************************/
s32 nand_get_dma_desc(struct nand_controller_info *nctri)
{

	if (nctri->ndfc_dma_desc == 0) {
		nctri->ndfc_dma_desc_cpu = nand_malloc(4 * 1024);
		nctri->ndfc_dma_desc = nctri->ndfc_dma_desc_cpu;
		if (nctri->ndfc_dma_desc == NULL) {
			RAWNAND_ERR("get_dma_desc fail!\n");
			return -1;
		}
	}
	return 0;
}
/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 *****************************************************************************/
int init_parameter(void)
{
	int i;
	struct nand_controller_info *nctri;

	g_nsi = NULL;
	g_nssi = NULL;
	g_nctri = NULL;
	g_nand_storage_info = NULL;

	//g_nsi = (struct _nand_storage_info *)nand_malloc(sizeof(struct _nand_storage_info));
	g_nsi = &g_nsi_data;
	if (g_nsi == NULL) {
		RAWNAND_ERR("init_parameter, no memory for g_nsi\n");
		return NAND_OP_FALSE;
	}
	memset(g_nsi, 0, sizeof(struct _nand_storage_info));

	//g_nssi = (struct _nand_super_storage_info *)nand_malloc(sizeof(struct _nand_super_storage_info));
	g_nssi = &g_nssi_data;
	if (g_nssi == NULL) {
		RAWNAND_ERR("init_parameter, no memory for g_nssi\n");
		return NAND_OP_FALSE;
	}
	memset(g_nssi, 0, sizeof(struct _nand_super_storage_info));

	for (i = 0; i < MAX_CHANNEL; i++) {
		//nctri = (struct nand_controller_info *)nand_malloc(sizeof(struct nand_controller_info));
		nctri = &g_nctri_data[i];
		if (nctri == NULL) {
			RAWNAND_ERR("init_parameter, no memory for g_nctri\n");
			return NAND_OP_FALSE;
		}
		/*memset(nctri, 0, sizeof(struct nand_controller_info));*/

		add_to_nctri(nctri);

		if (init_nctri(nctri)) {
			RAWNAND_ERR("nand_physic_init, init nctri error\n");
			return NAND_OP_FALSE;
		}
		nand_get_dma_desc(nctri);
	}

	//g_nand_storage_info = (struct __NandStorageInfo_t *)nand_malloc(sizeof(struct __NandStorageInfo_t));
	g_nand_storage_info = &g_nand_storage_info_data;
	if (g_nand_storage_info == NULL) {
		RAWNAND_ERR("init_parameter, no memory for g_nand_storage_info\n");
		return NAND_OP_FALSE;
	}

	//	function_read_page_end = generic_read_page_end_not_retry;

	nand_init_temp_buf(&ntf);

	return NAND_OP_TRUE;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :the return pointer cannot be modified! must call by pair!
 *****************************************************************************/
s32 nand_free_dma_desc(struct nand_controller_info *nctri)
{
	/*nand_freeMemoryForDMADescs(&nctri->ndfc_dma_desc_cpu,&nctri->ndfc_dma_desc);*/
	nand_free(nctri->ndfc_dma_desc);
	nctri->ndfc_dma_desc = NULL;
	nctri->ndfc_dma_desc_cpu = NULL;

	return 0;
}
/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 *****************************************************************************/
int nand_init_temp_buf(struct _nand_temp_buf *nand_temp_buf)
{
	int i;

	memset(nand_temp_buf, 0, sizeof(struct _nand_temp_buf));

	for (i = 0; i < NUM_16K_BUF; i++) {
		nand_temp_buf->nand_temp_buf16k[i] = (u8 *)nand_malloc(16384);
		if (nand_temp_buf->nand_temp_buf16k[i] == NULL) {
			RAWNAND_ERR("no memory for nand_init_temp_buf 16K\n");
			return NAND_OP_FALSE;
		}
	}

	for (i = 0; i < NUM_32K_BUF; i++) {
		nand_temp_buf->nand_temp_buf32k[i] = (u8 *)nand_malloc(32768);
		if (nand_temp_buf->nand_temp_buf32k[i] == NULL) {
			RAWNAND_ERR("no memory for nand_init_temp_buf 32K\n");
			return NAND_OP_FALSE;
		}
	}

	for (i = 0; i < NUM_64K_BUF; i++) {
		nand_temp_buf->nand_temp_buf64k[i] = (u8 *)nand_malloc(65536);
		if (nand_temp_buf->nand_temp_buf64k[i] == NULL) {
			RAWNAND_ERR("no memory for nand_init_temp_buf 64K\n");
			return NAND_OP_FALSE;
		}
	}
	return 0;
}
/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 *****************************************************************************/
int nand_exit_temp_buf(struct _nand_temp_buf *nand_temp_buf)
{
	int i;

	for (i = 0; i < NUM_16K_BUF; i++) {
		nand_free(nand_temp_buf->nand_temp_buf16k[i]);
	}

	for (i = 0; i < NUM_32K_BUF; i++) {
		nand_free(nand_temp_buf->nand_temp_buf32k[i]);
	}

	for (i = 0; i < NUM_64K_BUF; i++) {
		nand_free(nand_temp_buf->nand_temp_buf64k[i]);
	}
	return 0;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       :
 *Note         :the return pointer cannot be modified! must call by pair!
 *****************************************************************************/
u8 *nand_get_temp_buf(unsigned int size)
{
	unsigned int i;

	if (size <= 16384) {
		for (i = 0; i < NUM_16K_BUF; i++) {
			if (ntf.used_16k[i] == 0) {
				ntf.used_16k[i] = 1;
				return ntf.nand_temp_buf16k[i];
			}
		}
	}

	if (size <= 32768) {
		for (i = 0; i < NUM_32K_BUF; i++) {
			if (ntf.used_32k[i] == 0) {
				ntf.used_32k[i] = 1;
				return ntf.nand_temp_buf32k[i];
			}
		}
	}

	if (size <= 65536) {
		for (i = 0; i < NUM_64K_BUF; i++) {
			if (ntf.used_64k[i] == 0) {
				ntf.used_64k[i] = 1;
				return ntf.nand_temp_buf64k[i];
			}
		}
	}

	for (i = 0; i < NUM_NEW_BUF; i++) {
		if (ntf.used_new[i] == 0) {
			RAWNAND_DBG("get memory :%d. \n", size);
			ntf.used_new[i] = 1;
			ntf.nand_new_buf[i] = (u8 *)nand_malloc(size);
			if (ntf.nand_new_buf[i] == NULL) {
				RAWNAND_ERR("%s:malloc fail\n", __func__);
				ntf.used_new[i] = 0;
				return NULL;
			}
			return ntf.nand_new_buf[i];
		}
	}

	RAWNAND_ERR("get memory fail %d. \n", size);
	//while(1);
	return NULL;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 *****************************************************************************/
int nand_free_temp_buf(unsigned char *buf)
{
	int i;

	for (i = 0; i < NUM_16K_BUF; i++) {
		if (ntf.nand_temp_buf16k[i] == buf) {
			ntf.used_16k[i] = 0;
			return 0;
		}
	}

	for (i = 0; i < NUM_32K_BUF; i++) {
		if (ntf.nand_temp_buf32k[i] == buf) {
			ntf.used_32k[i] = 0;
			return 0;
		}
	}

	for (i = 0; i < NUM_64K_BUF; i++) {
		if (ntf.nand_temp_buf64k[i] == buf) {
			ntf.used_64k[i] = 0;
			return 0;
		}
	}

	for (i = 0; i < NUM_NEW_BUF; i++) {
		if (ntf.nand_new_buf[i] == buf) {
			ntf.used_new[i] = 0;
			nand_free(ntf.nand_new_buf[i]);
			return 0;
		}
	}

	RAWNAND_ERR("%s free memory fail\n", __func__);
	return -1;
}
/**
 *	rawnand_delete storage info: empety g_nand_storage_info and point null
 */
void rawnand_delete_storage_info(void)
{
	memset(g_nand_storage_info, 0, sizeof(*g_nand_storage_info));
	g_nand_storage_info = NULL;
}
/**
 * rawnand_storage_init: build storage info struction
 */
int rawnand_storage_init(void)
{
	g_nand_storage_info = &g_nand_storage_info_data;
	if (g_nand_storage_info == NULL) {
		RAWNAND_ERR("%s no memory to storage nand info\n", __func__);
		return ERR_NO_12;
	}

	g_nand_storage_info->ChannelCnt = g_nssi->nsci->channel_num;
	g_nand_storage_info->ChipCnt = g_nsi->chip_cnt;
	g_nand_storage_info->ChipConnectInfo = g_nctri->chip_connect_info;
	g_nand_storage_info->RbCnt = g_nsi->chip_cnt;
	g_nand_storage_info->RbConnectInfo = g_nctri->rb_connect_info;
	g_nand_storage_info->RbConnectMode = 0;
	g_nand_storage_info->BankCntPerChip = 1;
	g_nand_storage_info->DieCntPerChip = g_nsi->nci->npi->die_cnt_per_chip;
	g_nand_storage_info->PlaneCntPerDie = 1;
	g_nand_storage_info->SectorCntPerPage = g_nsi->nci->npi->sect_cnt_per_page;
	g_nand_storage_info->PageCntPerPhyBlk = g_nsi->nci->npi->page_cnt_per_blk;
	g_nand_storage_info->BlkCntPerDie = g_nsi->nci->npi->blk_cnt_per_die;
	g_nand_storage_info->OperationOpt = g_nsi->nci->npi->operation_opt;
	g_nand_storage_info->FrequencePar = g_nsi->nci->npi->access_freq;
	g_nand_storage_info->EccMode = g_nsi->nci->npi->ecc_mode;
	g_nand_storage_info->ValidBlkRatio = g_nsi->nci->npi->valid_blk_ratio;
	g_nand_storage_info->ReadRetryType = g_nsi->nci->npi->read_retry_type;
	g_nand_storage_info->DDRType = g_nsi->nci->npi->ddr_type;
	g_nand_storage_info->random_addr_num = g_nsi->nci->npi->random_addr_num;
	g_nand_storage_info->random_cmd2_send_flag = g_nsi->nci->npi->random_cmd2_send_flag;
	g_nand_storage_info->nand_real_page_size = g_nsi->nci->npi->nand_real_page_size;

	memcpy(g_nand_storage_info->NandChipId, g_nsi->nci->id, 8);
	memcpy(&g_nand_storage_info->OptPhyOpPar, g_nsi->nci->opt_phy_op_par, sizeof(struct nand_phy_op_par));

	return 0;
}

/*
 * rawnand_special_init: rawnand special init
 * */
int rawnand_special_init(void)
{
	enum nand_readretry_type type;

	struct nand_chip_info *nci = g_nctri->nci;
	if (nci == NULL) {
		RAWNAND_ERR("%s nci is null\n", __func__);
		return ERR_NO_12;
	}

	type = nci->npi->selected_readretry_no;

	rawnand_chip_special_init(type);

	return 0;
}

/*
 * rawnand_special_exit: rawnand special exit
 * */
int rawnand_special_exit(void)
{
	enum nand_readretry_type type;

	struct nand_chip_info *nci = g_nctri->nci;
	if (nci == NULL) {
		RAWNAND_ERR("%s nci is null\n", __func__);
		return ERR_NO_12;
	}

	type = nci->npi->selected_readretry_no;

	rawnand_chip_special_exit(type);

	return 0;
}
/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 *****************************************************************************/
void nand_cfg_setting(void)
{
	g_phy_cfg = &aw_nand_info.nand_cfg;
	aw_nand_info.nand_cfg.phy_interface_cfg = nand_cfg_interface();

	aw_nand_info.nand_cfg.phy_support_two_plane = nand_support_two_plane();
	aw_nand_info.nand_cfg.phy_nand_support_vertical_interleave = nand_support_vertical_interleave();
	aw_nand_info.nand_cfg.phy_support_dual_channel = nand_support_dual_channel();

	aw_nand_info.nand_cfg.phy_wait_rb_before = nand_wait_rb_before();
	aw_nand_info.nand_cfg.phy_wait_rb_mode = nand_wait_rb_mode();
	aw_nand_info.nand_cfg.phy_wait_dma_mode = nand_wait_dma_mode();
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 *****************************************************************************/
int nand_physic_init(void)
{
#if 0
	RAWNAND_DBG("nand_physic_init\n");

	if (init_parameter() != 0) {
		RAWNAND_ERR("nand_physic_init init_parameter error\n");
		return NAND_OP_FALSE;
	}

	if (nand_build_nsi(g_nsi, g_nctri) != 0) {
		RAWNAND_ERR("nand_physic_init nand_build_nsi error\n");
		return NAND_OP_FALSE;
	}

	storage_type = 1;

	if (check_nctri(g_nctri) != 0) {
		RAWNAND_ERR("nand_physic_init check_nctri error\n");
		return NAND_OP_FALSE;
	}

	set_nand_script_frequence();

	if (update_nctri(g_nctri) != 0) {
		RAWNAND_ERR("nand_physic_init update_nctri error\n");
		return NAND_OP_FALSE;
	}

	/*special_ops.nand_physic_special_init();*/
	rawnand_special_init();

	nand_physic_info_read();

	if (nand_build_nssi(g_nssi, g_nctri) != 0) {
		RAWNAND_ERR("nand_physic_init nand_build_nssi error\n");
		return NAND_OP_FALSE;
	}

	nand_build_storage_info();

	show_static_info();

	nand_code_info();

	//nand_phy_test();
	//nand_phy_erase_all();

	RAWNAND_DBG("nand_physic_init end\n");
#endif
	return NAND_OP_TRUE;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 *****************************************************************************/
int nand_physic_exit(void)
{
	struct nand_controller_info *nctri = g_nctri;

	RAWNAND_DBG("nand_physic_exit\n");
	if (g_nctri == NULL) {
		RAWNAND_INFO("%s %d no resource to free\n", __func__, __LINE__);
		return 0;
	}

	/*if (special_ops.nand_physic_special_exit != NULL)*/
	/*special_ops.nand_physic_special_exit();*/
	rawnand_special_exit();

	while (nctri != NULL) {
		nand_clk_release(&aw_ndfc, nctri->channel_id);
		nand_pio_release(&aw_ndfc, nctri->channel_id);
		if (nctri->dma_type == DMA_MODE_GENERAL_DMA) {
			nand_release_dma(&aw_ndfc, nctri->channel_id);
		}
		nand_free_dma_desc(nctri);
		nctri = nctri->next;
	}

	nand_release_voltage(&aw_ndfc);

	nand_permanent_data.magic_data = MAGIC_DATA_FOR_PERMANENT_DATA;
	nand_permanent_data.support_two_plane = 0;
	nand_permanent_data.support_vertical_interleave = 0;
	nand_permanent_data.support_dual_channel = 0;

	nand_exit_temp_buf(&ntf);
	delete_nctri();
	delete_nsi();
	delete_nssi();
	rawnand_delete_storage_info();

	return 0;
}
void init_list_head(void **head)
{
	if (*head != NULL) {
		*head = NULL;
	}
}

/**
 * rawnand_channel_init: init channel
 * @channel : channel num
 */
static int rawnand_channel_init(int channel)
{
	struct nand_controller_info *nctri = NULL;
	int ret = 0;
	void *nctri_base = NULL;

	if (channel == 0) {
		/*the nctri list*/
		init_list_head((void **)&g_nctri);
		g_nsi = &g_nsi_data;
		if (g_nsi == NULL) {
			RAWNAND_ERR("rawnand err: %s g_nsi is null\n", __func__);
			return ERR_NO_12;
		}
		memset(g_nsi, 0, sizeof(struct _nand_storage_info));
	}
	nctri = &g_nctri_data[channel];
	if (nctri == NULL) {
		RAWNAND_ERR("%s channel@%d no memory for nctri\n", __func__,
			    channel);
		return ERR_NO_12; /*-ENOMEM*/
	}

	RAWNAND_DBG("%s %d nctri:%p channel:%d\n", __func__, __LINE__, nctri,
		    channel);

	aw_ndfc.nctri = nctri;
	nctri_base = nctri->nreg_base;
	memset(nctri, 0, sizeof(struct nand_controller_info));
	add_to_nctri(nctri);
	/*fill the channel's nctri with its register address*/
	nctri->nreg_base = nctri_base;
	g_nreg_base = nctri_base;
	fill_nctri(nctri);

	/*init */
	if (init_nctri(nctri)) {
		RAWNAND_ERR("%s channel@%d init nctri fail\n", __func__,
			    channel);
		ret = ERR_NO_17;
		goto err0;
	}
	/*get the channel's cpu dma dest addr*/
	nand_get_dma_desc(nctri);

	/*init the channel's chip*/
	ret = rawnand_chips_init((struct nand_chip_info *)&nctri->nci);
	if (ret != NAND_OP_TRUE) {
		RAWNAND_ERR("%s channel@%d chips init fail\n", __func__,
			    channel);
		ret = ERR_NO_18;
		goto err0;
	}

	return NAND_OP_TRUE;

err0:
	delete_from_nctri_by_channel(channel);
	return NAND_OP_FALSE;
}
/**
 * rawnand_channel_init_tail: channel init tail
 * eg. some channel's chips special init
 * @channel : channel num
 */
static int rawnand_channel_init_tail(void)
{
	struct nand_chip_info *nci = g_nctri->nci;
	if (nci == NULL) {
		RAWNAND_ERR("%s nci is null\n", __func__);
		return ERR_NO_12;
	}

	g_nssi = &g_nssi_data;
	if (g_nssi == NULL) {
		RAWNAND_ERR("rawnand err: %s g_nssi is null\n", __func__);
		return ERR_NO_12;
	}
	memset(g_nssi, 0, sizeof(struct _nand_super_storage_info));
	set_nand_script_frequence();

	/*according to the id table, update the channel*/
	if (update_nctri(g_nctri) != NAND_OP_TRUE) {
		RAWNAND_ERR("rawnand err: %s update nctri fail\n", __func__);
		return NAND_OP_FALSE;
	}

	/*rawnand some special init. eg. chip special request of readretry*/
	rawnand_special_init();

	nand_physic_info_read();

	/*build the super chips*/
	if (rawnand_sp_chips_init((struct nand_super_chip_info *)&g_nssi->nsci) != NAND_OP_TRUE) {
		RAWNAND_ERR("%s %d rawnand super chips init fail\n",
			    __func__, __LINE__);
		return NAND_OP_FALSE;
	}
	return NAND_OP_TRUE;
}
/**
 * rawnand_hw_init: rawnand hardward init
 */
int rawnand_hw_init(void)
{
	int cn = 0;
	int ret = 0;
	struct nand_controller_info *nctri = NULL;
	struct nand_controller_info *nctri_temp = NULL;

	RAWNAND_INFO("enter rawnand hardward init..\n");
	/*request nand temp buffer*/
	nand_init_temp_buf(&ntf);

	/*PC withstand volatage mode switch to 3.3v,consister other module
	 * switch it to 1.8v, if nand need 1.8v, configure id table ddr_opt
	 * bit16(NAND_VCCQ_1P8V), then update the withstand volatage mode*/
	nand_vccq_3p3v_enable();

	/*init channel*/
	RAWNAND_DBG("%s channel max:%d\n", __func__, MAX_CHANNEL);
	for (cn = 0; cn < MAX_CHANNEL; cn++) {

		ret = rawnand_channel_init(cn);
		if (ret != NAND_OP_TRUE) {
			if (cn >= 1)
				goto right;
			else
				goto err0;
		}

		/**
		 * the connect info of different channel should keep the same
		 */
		if (cn >= 1) {
			nctri_temp = nctri_get(g_nctri, cn - 1);
			if (nctri_temp == NULL) {
				RAWNAND_ERR("%s channel@%d is null\n", __func__,
					    cn - 1);
				goto err0;
			}

			nctri = nctri_get(g_nctri, cn);
			if (nctri == NULL) {
				RAWNAND_ERR("%s channel@%d is null\n", __func__,
					    cn);
				goto err0;
			}
			if (nctri_temp->chip_connect_info !=
			    nctri->chip_connect_info) {
				RAWNAND_ERR("%s channel@%d connect info is"
					    "different with channel@%d\n",
					    __func__, cn - 1, cn);
				goto err0;
			}
		}
	} /*MAX_CHANNEL*/

	if (rawnand_channel_init_tail() != NAND_OP_TRUE) {
		RAWNAND_ERR("%s rawnand channel tail init fail\n", __func__);
		goto err0;
	}

	if (rawnand_storage_init() != NAND_OP_TRUE) {
		RAWNAND_ERR("%s rawnand storage init fail\n", __func__);
		goto err0;
	}

	/*print the channel' info*/
	show_static_info();

right:
	RAWNAND_INFO("exit rawnand hardward init\n");
	return NAND_OP_TRUE;

err0:
	RAWNAND_INFO("rawnand hardward init fail\n");
	delete_nctri();
	return NAND_OP_FALSE;
}
static inline void rawnand_set_nand_info_data(struct _nand_info *nand_info)
{
	nand_info->type = 0;
	nand_info->SectorNumsPerPage = g_nssi->nsci->sector_cnt_per_super_page;
	nand_info->BytesUserData = g_nssi->nsci->spare_bytes;
	nand_info->BlkPerChip = g_nssi->nsci->blk_cnt_per_super_chip;

	nand_info->ChipNum = g_nssi->super_chip_cnt;

	nand_info->PageNumsPerBlk = g_nssi->nsci->page_cnt_per_super_blk;

	nand_info->MaxBlkEraseTimes = g_nssi->nsci->nci_first->max_erase_times;

	nand_info->EnableReadReclaim = 1;

	/*
	 *aw_nand_info.MaxBlkEraseTimes = 2000;
	 *aw_nand_info.FullBitmap = FULL_BITMAP_OF_SUPER_PAGE;
	 *aw_nand_info.EnableReadReclaim = (g_nsi->nci->npi->operation_opt & NAND_READ_RECLAIM) ? 1 : 0;
	 */
	nand_info->boot = phyinfo_buf;

	if (aw_nand_info.boot->physic_block_reserved == 0)
		aw_nand_info.boot->physic_block_reserved = PHYSIC_RECV_BLOCK;

	/*set aw_nand_info.boot->uboot_start_block
	 *    aw_nand_info.boot->uboot_next_block*/
	set_uboot_start_and_end_block();
}
/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 *****************************************************************************/
struct _nand_info *RawNandHwInit(void)
{
	int ret;

	nand_common1_show_version();
	RAWNAND_DBG("%s %d\n", __func__, __LINE__);
	nand_cfg_setting();

	/*ret = nand_physic_init();*/
	ret = rawnand_hw_init();
	if (ret != 0) {
		RAWNAND_ERR("nand_physic_init error %d\n", ret);
		return NULL;
	}

	rawnand_set_nand_info_data(&aw_nand_info);

	set_hynix_special_info();

	nand_secure_storage_init(0);

	RAWNAND_DBG("RawNandHwInit end\n");

	return &aw_nand_info;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 *****************************************************************************/
int RawNandHwExit(void)
{
	nand_physic_lock();
	nand_wait_all_rb_ready();
	nand_physic_exit();
	nand_physic_unlock();
	return 0;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 *****************************************************************************/
int rawnand_hw_super_standby(void)
{
	struct nand_controller_info *nctri = g_nctri;

	RAWNAND_DBG("RawNandHwSuperStandby start\n");
	nand_physic_lock();
	nand_wait_all_rb_ready();

	while (nctri != NULL) {
		save_nctri(nctri);
		//show_nctri(nctri);
		//show_nci(nctri->nci);
		switch_ddrtype_from_ddr_to_sdr(nctri);
		nand_clk_release(&aw_ndfc, nctri->channel_id);
		nand_pio_release(&aw_ndfc, nctri->channel_id);
		nctri = nctri->next;
	}

	nand_disable_voltage(&aw_ndfc);

	return 0;
}
/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 *****************************************************************************/
int rawnand_hw_super_resume(void)
{
	struct nand_controller_info *nctri;
	struct nand_chip_info *nci;

	RAWNAND_DBG("RawNandHwSuperResume start\n");
	nand_enable_voltage(&aw_ndfc);
	nctri = g_nctri;
	while (nctri != NULL) {
		nand_pio_request(&aw_ndfc, nctri->channel_id);
		nand_clk_request(&aw_ndfc, nctri->channel_id);
		ndfc_soft_reset(nctri);
		recover_nctri(nctri);

		nci = nctri->nci;
		while (nci != NULL) {
			nand_reset_chip(nci);
			nci = nci->nctri_next;
		}
		nctri = nctri->next;
	}

	nctri = g_nctri;
	while (nctri != NULL) {
		update_nctri(nctri);
		nctri = nctri->next;
	}


	nand_wait_all_rb_ready();
	nand_physic_unlock();

	RAWNAND_DBG("RawNandHwSuperResume end\n");
	return 0;
}

/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 *****************************************************************************/
int rawnand_hw_normal_standby(void)
{
	RAWNAND_DBG("RawNandHwNormalStandby start\n");
	nand_physic_lock();
	nand_wait_all_rb_ready();
	return 0;
}
/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 *****************************************************************************/
int rawnand_hw_normal_resume(void)
{
	RAWNAND_DBG("RawNandHwNormalResume start\n");
	nand_physic_unlock();
	return 0;
}
/*****************************************************************************
 *Name         :
 *Description  :
 *Parameter    :
 *Return       : 0:ok  -1:fail
 *Note         :
 *****************************************************************************/
int rawnand_hw_shutdown(void)
{
	RAWNAND_DBG("RawNandHwShutDown start\n");
	nand_physic_lock();
	nand_physic_exit();
	nand_wait_all_rb_ready();
	return 0;
}
