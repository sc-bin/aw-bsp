/* SPDX-License-Identifier: GPL-2.0 */
/*
 ************************************************************************************************************************
 *                                                      eNand
 *                                   Nand flash driver data struct type define
 *
 *                             Copyright(C), 2008-2009, SoftWinners Microelectronic Co., Ltd.
 *											       All Rights Reserved
 *
 * File Name : nand_type.h
 *
 * Author : Kevin.z
 *
 * Version : v0.1
 *
 * Date : 2008.03.19
 *
 * Description : This file defines the data struct type and return value type for nand flash driver.
 *
 * Others : None at present.
 *
 *
 * History :
 *
 *  <Author>        <time>       <version>      <description>
 *
 * Kevin.z         2008.03.19      0.1          build the file
 *
 ************************************************************************************************************************
 */
#ifndef __SPINAND_TYPE_H
#define __SPINAND_TYPE_H

#include "../nand_errno.h"
#include "../../aw_nand_type.h"
//==============================================================================
//  define the data structure for physic layer module
//==============================================================================

typedef struct boot_flash_info {
	__u32 chip_cnt;
	__u32 blk_cnt_per_chip;
	__u32 blocksize;
	__u32 pagesize;
	__u32 pagewithbadflag; /*bad block flag was written at the first byte of spare area of this page*/
} boot_flash_info_t;

struct boot_physical_param {
	__u32 chip;	 /*chip no*/
	__u32 block;	/* block no within chip*/
	__u32 page;	 /* apge no within block*/
	__u32 sectorbitmap; /*done't care*/
	void *mainbuf;      /*data buf*/
	void *oobbuf;       /*oob buf*/
};

struct spi_nand_function {
	__s32 (*spi_nand_reset)(__u32 spi_no, __u32 chip);
	__s32 (*spi_nand_read_status)(__u32 spi_no, __u32 chip, __u8 status, __u32 mode);
	__s32 (*spi_nand_setstatus)(__u32 spi_no, __u32 chip, __u8 reg);
	__s32 (*spi_nand_getblocklock)(__u32 spi_no, __u32 chip, __u8 *reg);
	__s32 (*spi_nand_setblocklock)(__u32 spi_no, __u32 chip, __u8 reg);
	__s32 (*spi_nand_getotp)(__u32 spi_no, __u32 chip, __u8 *reg);
	__s32 (*spi_nand_setotp)(__u32 spi_no, __u32 chip, __u8 reg);
	__s32 (*spi_nand_getoutdriver)(__u32 spi_no, __u32 chip, __u8 *reg);
	__s32 (*spi_nand_setoutdriver)(__u32 spi_no, __u32 chip, __u8 reg);
	__s32 (*erase_single_block)(struct boot_physical_param *eraseop);
	__s32 (*write_single_page)(struct boot_physical_param *writeop);
	__s32 (*read_single_page)(struct boot_physical_param *readop, __u32 spare_only_flag);
};
//define the nand flash storage system information
struct __NandStorageInfo_t {
	__u8 ChipCnt;		     //the count of the total nand flash chips are currently connecting on the CE pin
	__u16 ChipConnectInfo;       //chip connect information, bit == 1 means there is a chip connecting on the CE pin
	__u8 ConnectMode;	    //the rb connect  mode
	__u8 BankCntPerChip;	 //the count of the banks in one nand chip, multiple banks can support Inter-Leave
	__u8 DieCntPerChip;	  //the count of the dies in one nand chip, block management is based on Die
	__u8 PlaneCntPerDie;	 //the count of planes in one die, multiple planes can support multi-plane operation
	__u8 SectorCntPerPage;       //the count of sectors in one single physic page, one sector is 0.5k
	__u16 PageCntPerPhyBlk;      //the count of physic pages in one physic block
	__u32 BlkCntPerDie;	  //the count of the physic blocks in one die, include valid block and invalid block
	__u32 OperationOpt;	  //the mask of the operation types which current nand flash can support support
	__u16 FrequencePar;	  //the parameter of the hardware access clock, based on 'MHz'
	__u32 SpiMode;		     //spi nand mode, 0:mode 0, 3:mode 3
	__u8 NandChipId[8];	  //the nand chip id of current connecting nand chip
	__u32 pagewithbadflag;       //bad block flag was written at the first byte of spare area of this page
	__u32 MultiPlaneBlockOffset; //the value of the block number offset between the two plane block
	__u32 MaxEraseTimes;	 //the max erase times of a physic block
	__u32 MaxEccBits;	    //the max ecc bits that nand support
	__u32 EccLimitBits;	  //the ecc limit flag for tne nand
	__u32 Idnumber;
	__u32 EccType;		// Just use in spinand2, select different ecc status type.
	__u32 EccProtectedType; // just use in spinand2, select different ecc protected type.
	const char *Model;
	struct spi_nand_function *spi_nand_function; //erase,write,read function for spi nand
};

struct _spinand_config_para_info {
	unsigned int super_chip_cnt;
	unsigned int super_block_nums;
	unsigned int support_two_plane;
	unsigned int support_v_interleave;
	unsigned int support_dual_channel;
	unsigned int plane_cnt;
	unsigned int support_dual_read;
	unsigned int support_dual_write;
	unsigned int support_quad_read;
	unsigned int support_quad_write;
	unsigned int frequence;
};

//define the page buffer pool for nand flash driver
struct __NandPageCachePool_t {
	__u8 *PageCache0; //the pointer to the first page size ram buffer
	//    __u8        *PageCache1;                        //the pointer to the second page size ram buffer
	//    __u8        *PageCache2;                        //the pointer to the third page size ram buffer
	__u8 *SpareCache;
	__u8 *TmpPageCache;
	__u8 *SpiPageCache;
	__u8 *SpareCache1; // 64bytes, used by mX_spi_nand_read_xY
};

//define the paramter structure for physic operation function
struct __PhysicOpPara_t {
	__u32 BankNum;    //the number of the bank current accessed, bank NO. is different of chip NO.
	__u32 PageNum;    //the number of the page current accessed, the page is based on single-plane or multi-plane
	__u32 BlkNum;     //the number of the physic block, the block is based on single-plane or multi-plane
	__u64 SectBitmap; //the bitmap of the sector in the page which need access data
	void *MDataPtr;   //the pointer to main data buffer, it is the start address of a page size based buffer
	void *SDataPtr;   //the pointer to spare data buffer, it will be set to NULL if needn't access spare data
};

//define the nand flash physical information parameter type, for id table
struct __NandPhyInfoPar_t {
	__u8 NandID[8];				     //the ID number of the nand flash chip
	__u8 DieCntPerChip;			     //the count of the Die in one nand flash chip
	__u8 SectCntPerPage;			     //the count of the sectors in one single physical page
	__u16 PageCntPerBlk;			     //the count of the pages in one single physical block
	__u16 BlkCntPerDie;			     //the count fo the physical blocks in one nand flash Die
	__u32 OperationOpt;			     //the bitmap that marks which optional operation that the nand flash can support
	__u16 AccessFreq;			     //the highest access frequence of the nand flash chip, based on MHz
	__u32 SpiMode;				     //spi nand mode, 0:mode 0, 3:mode 3
	__u32 pagewithbadflag;			     //bad block flag was written at the first byte of spare area of this page
	struct spi_nand_function *spi_nand_function; //erase,write,read function for spi nand
	__u32 MultiPlaneBlockOffset;		     //the value of the block number offset between the two plane block
	__u32 MaxEraseTimes;			     //the max erase times of a physic block
	__u32 MaxEccBits;			     //the max ecc bits that nand support
	__u32 EccLimitBits;			     //the ecc limit flag for tne nand
	__u32 Idnumber;
	__u32 EccType;		// Just use in spinand2, select different ecc status type.
	__u32 EccProtectedType; // just use in spinand2, select different ecc protected type.
	const char *Model;
	//__u8 reserved[4]; //reserved for 32bit align
};

//==============================================================================
//  define the data structure for logic management module
//==============================================================================

//define the logical architecture parameter structure
struct __LogicArchitecture_t {
#if 0
	//__u16       LogicBlkCntPerZone;                 //the counter that marks how many logic blocks in one zone
	__u16       PageCntPerLogicBlk;                 //the counter that marks how many pages in one logical block
	__u8        SectCntPerLogicPage;                //the counter that marks how many  sectors in one logical page
	//__u8        ZoneCntPerDie;                      //the counter that marks how many zones in one die
	//__u16       Reserved;                           //reserved for 32bit align
	__u8       LogicDieCnt;                           //reserved for 32bit align
#else
	__u32 PageCntPerLogicBlk;     //the number of sectors in one logical page
	__u32 SectCntPerLogicPage;    //the number of logical page in one logical block
	__u32 LogicBlkCntPerLogicDie; //the number of logical block in a logical die
	__u32 LogicDieCnt;	    //the number of logical die
//__u32 		ZoneCntPerDie;
#endif
};

//==============================================================================
//  define some constant variable for the nand flash driver used
//==============================================================================

//define the mask for the nand flash optional operation
/* spi nand flash support dual read operation */
#define SPINAND_DUAL_READ (1 << 0)
/* nand flash support page dual program operation */
#define SPINAND_DUAL_PROGRAM (1 << 1)
/* nand flash support multi-plane page read operation */
#define SPINAND_MULTI_READ (1 << 2)
/* nand flash support multi-plane page program operation */
#define SPINAND_MULTI_PROGRAM (1 << 3)

/* nand flash support external inter-leave operation, it based multi-chip */
#define SPINAND_EXT_INTERLEAVE (1 << 4)
/* nand flash support the maximum block erase cnt */
#define SPINAND_MAX_BLK_ERASE_CNT (1 << 5)
/* nand flash support to read reclaim Operation */
#define SPINAND_READ_RECLAIM (1 << 6)
/* nand flash need plane select for addr */
#define SPINAND_TWO_PLANE_SELECT (1 << 7)

/* nand flash need a dummy Byte after random fast read */
#define SPINAND_ONEDUMMY_AFTER_RANDOMREAD (1 << 8)
/* nand flash support quad read operation */
#define SPINAND_QUAD_READ (1 << 9)
/* nand flash support page quad program operation */
#define SPINAND_QUAD_PROGRAM (1 << 10)
/* nand flash only support 8KB user meta data under ecc protected */
#define SPINAND_8K_METADATA_ECC_PROTECTED (1 << 11)

/* nand flash should not enable QE register bit manually */
#define SPINAND_QUAD_NO_NEED_ENABLE (1 << 12)

#if 0
//define the mask for the nand flash part_type
#define NAND_SECURITY_PARTITION (1 << 0)
#define NAND_FORCE_FLUSH_CACHE (1 << 1)


//define part type of nand
#define NORMAL_TYPE 0
#define LSB_TYPE 1
#endif

//define the mask for the nand flash operation status
//#define NAND_OPERATE_FAIL       (1<<0)              //nand flash program/erase failed mask
//#define NAND_CACHE_READY        (1<<5)              //nand flash cache program true ready mask
//#define NAND_STATUS_READY       (1<<6)              //nand flash ready/busy status mask
//#define NAND_WRITE_PROTECT      (1<<7)              //nand flash write protected mask

#if 0
//define the mark for physical page status
#define NAND_Free_PAGE_MARK 0xff  //the page is storing no data, is not used
#define DATA_PAGE_MARK 0x55  //the physical page is used for storing the update data
#define TABLE_PAGE_MARK 0xaa //the physical page is used for storing page mapping table

#define TABLE_BLK_MARK 0xaa    //the mark for the block mapping table block which is a special type block
#define BOOT_BLK_MARK 0xbb     //the mark for the boot block which is a special type block

//define the count of the physical blocks managed by one zone
#define BLOCK_CNT_OF_ZONE 1024 //one zone is organized based on 1024 blocks
#endif

//define the size of the sector
#ifndef SECTOR_SIZE
#define SECTOR_SIZE 512 //the size of a sector, based on byte
#endif

#define BAD_BLK_FLAG_MARK 0x03
/* the bad block flag is only in the first page */
#define BAD_BLK_FLAG_FRIST_1_PAGE 0x00
/* the bad block flag is in the first page or second page*/
#define BAD_BLK_FLAG_FIRST_2_PAGE 0x01
/* the bad block flag is in the last page */
#define BAD_BLK_FLAG_LAST_1_PAGE 0x02
/* the bad block flag is in the last 2 pages */
#define BAD_BLK_FLAG_LAST_2_PAGE 0x03

#endif //ifndef __NAND_TYPE_H
