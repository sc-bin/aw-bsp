/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __RAWNAND_BOOT_H__
#define __RAWNAND_BOOT_H__
#include "../nand-partition/phy.h"
#include <sunxi-boot.h>
#include "../../nfd/nand_base.h"

struct boot0_df_cfg_mode {
	s32 (*generic_read_boot0_page_cfg_mode)(struct nand_chip_info *nci, struct _nand_physic_op_par *npo, struct boot_ndfc_cfg cfg);
	s32 (*generic_write_boot0_page_cfg_mode)(struct nand_chip_info *nci, struct _nand_physic_op_par *npo, struct boot_ndfc_cfg cfg);
};

struct boot0_copy_df {
	u32 (*ndfc_blks_per_boot0_copy)(struct nand_chip_info *nci);
};

enum rq_write_boot0_type {
	NAND_WRITE_BOOT0_GENERIC = 0,
	NAND_WRITE_BOOT0_HYNIX_16NM_4G,
	NAND_WRITE_BOOT0_HYNIX_20NM,
	NAND_WRITE_BOOT0_HYNIX_26NM,
};

typedef int (*write_boot0_one_t)(unsigned char *buf, unsigned int len, unsigned int counter);

struct rawnand_boot0_ops_t {
	int (*write_boot0_page)(struct nand_chip_info *nci, struct _nand_physic_op_par *npo);
	int (*read_boot0_page)(struct nand_chip_info *nci, struct _nand_physic_op_par *npo);
	int (*write_boot0_one)(unsigned char *buf, unsigned int len, unsigned int counter);
	int (*read_boot0_one)(unsigned char *buf, unsigned int len, unsigned int counter);
};

void selected_write_boot0_one(enum rq_write_boot0_type rq);

//extern struct boot0_df_cfg_mode boot0_cfg_df;
extern struct boot0_copy_df boot0_copy_df;
extern struct rawnand_boot0_ops_t rawnand_boot0_ops;
extern write_boot0_one_t write_boot0_one[];

extern int nand_secure_storage_block_bak;
extern int nand_secure_storage_block;

int rawnand_write_boot0_one(unsigned char *buf, unsigned int len, unsigned int counter);
int rawnand_read_boot0_one(unsigned char *buf, unsigned int len, unsigned int counter);
int rawnand_write_uboot_one(unsigned char *buf, unsigned int len, struct _boot_info *info_buf, unsigned int info_len, unsigned int counter);
int rawnand_read_uboot_one(unsigned char *buf, unsigned int len, unsigned int counter);
int rawnand_get_param(void *nand_param);
int rawnand_get_param_for_uboottail(void *nand_param);
int rawnand_update_phyarch(void);
int nand_check_bad_block_before_first_erase(struct _nand_physic_op_par *npo);
int rawnand_erase_chip(unsigned int chip, unsigned int start_block, unsigned int end_block, unsigned int force_flag);
void rawnand_erase_special_block(void);
int rawnand_uboot_erase_all_chip(UINT32 force_flag);
int rawnand_dragonborad_test_one(unsigned char *buf, unsigned char *oob, unsigned int blk_num);
int change_uboot_start_block(struct _boot_info *info, unsigned int start_block);
int rawnand_physic_info_get_one_copy(unsigned int start_block, unsigned int pages_offset, unsigned int *block_per_copy, unsigned int *buf);
int rawnand_add_len_to_uboot_tail(unsigned int uboot_size);
int set_hynix_special_info(void);

extern __u32 nand_get_nand_id_number_ctrl(struct sunxi_ndfc *ndfc);
extern __u32 nand_get_nand_extern_para(struct sunxi_ndfc *ndfc, __u32 para_num);
#endif /*RAWNAND_BOOT_H*/
