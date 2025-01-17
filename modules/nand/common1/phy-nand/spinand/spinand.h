/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __SPINAND_H__
#define __SPINAND_H__

#include "../nand-partition/phy.h"
#include <mtd/aw-spinand-nftl.h>

#define SPINAND_SUPPORT_READ_RECLAIM (1)

#define SPINAND_SECTOR_CNT_OF_SUPER_PAGE (spinand_nftl_get_super_page_size(SECTOR))
#define SPINAND_FULL_BITMAP_OF_SUPER_PAGE ((__u64)(((__u64)1 << (SPINAND_SECTOR_CNT_OF_SUPER_PAGE - 1)) | (((__u64)1 << (SPINAND_SECTOR_CNT_OF_SUPER_PAGE - 1)) - 1)))
#define SECTOR_CNT_OF_SINGLE_PAGE (spinand_nftl_get_single_page_size(SECTOR))
#define FULL_BITMAP_OF_SINGLE_PAGE ((__u32)(((__u32)1 << (SECTOR_CNT_OF_SINGLE_PAGE - 1)) | (((__u32)1 << (SECTOR_CNT_OF_SINGLE_PAGE - 1)) - 1)))


/*define the nand flash storage system information*/
typedef struct spinand_storage_info {
		__u8	ChipCnt;			 /*the count of the total nand flash chips are currently connecting on the CE pin*/
		__u16	ChipConnectInfo;	 /*chip connect information, bit == 1 means there is a chip connecting on the CE pin*/
		__u8	ConnectMode;		 /*the rb connect  mode*/
		__u8	BankCntPerChip; 	 /*the count of the banks in one nand chip, multiple banks can support Inter-Leave*/
		__u8	DieCntPerChip;		 /*the count of the dies in one nand chip, block management is based on Die*/
		__u8	PlaneCntPerDie; 	 /*the count of planes in one die, multiple planes can support multi-plane operation*/
		__u8	SectorCntPerPage;	 /*the count of sectors in one single physic page, one sector is 0.5k*/
		__u16	PageCntPerPhyBlk;	 /*the count of physic pages in one physic block*/
		__u32	BlkCntPerDie;		 /*the count of the physic blocks in one die, include valid block and invalid block*/
		__u32	OperationOpt;		 /*the mask of the operation types which current nand flash can support support*/
		__u8	NandChipId[8];		 /*the nand chip id of current connecting nand chip*/
		__u32	MaxEraseTimes;		 /*the max erase times of a physic block*/
} spinand_storage_info_t;

extern spinand_storage_info_t spinand_storage_info;
extern struct _boot_info *phyinfo_buf;

unsigned int spinand_lsb_page_cnt_per_block(void);
unsigned int spinand_get_page_num(unsigned int lsb_page_no);




void spinand_get_nand_info(struct _nand_info *aw_nand_info);
struct _nand_info *spinand_hardware_init(void);
__s32 spinand_hardware_exit(void);


#endif /*SPINAND_H*/
