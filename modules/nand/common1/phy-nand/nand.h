/*
 * rawnand.h
 *
 * Copyright (C) 2019 Allwinner.
 *
 * cuizhikui <cuizhikui@allwinnertech.com>
 *
 * SPDX-License-Identifier: GPL-2.0
 */
#ifndef __NAND_H__
#define __NAND_H__
#include <sunxi-boot.h>
#define SUPPORT_SUPER_STANDBY
#define NAND_VERSION_0 0x03
#define NAND_VERSION_1 0x01

struct nand_chip_info;
struct _nand_info;

struct nand_chips_ops {
	int (*nand_chips_init)(struct nand_chip_info *chip);
	void (*nand_chips_cleanup)(struct nand_chip_info *chip);
	int (*nand_chips_standby)(struct nand_chip_info *chip);
	int (*nand_chips_resume)(struct nand_chip_info *chip);
};

extern char *rawnand_get_chip_name(struct nand_chip_info *chip);
extern void rawnand_get_chip_id(struct nand_chip_info *chip, unsigned char *id, int cnt);
extern unsigned int rawnand_get_chip_die_cnt(struct nand_chip_info *chip);
extern int rawnand_get_chip_page_size(struct nand_chip_info *chip,
		enum size_type type);
extern int rawnand_get_chip_block_size(struct nand_chip_info *chip,
		enum size_type type);
extern int rawnand_get_chip_block_size(struct nand_chip_info *chip,
		enum size_type type);
extern int rawnand_get_chip_die_size(struct nand_chip_info *chip,
		enum size_type type);
extern unsigned long long rawnand_get_chip_opt(struct nand_chip_info *chip);
extern unsigned int rawnand_get_chip_ecc_mode(struct nand_chip_info *chip);
extern unsigned int rawnand_get_chip_freq(struct nand_chip_info *chip);
#endif /*NAND_H*/
