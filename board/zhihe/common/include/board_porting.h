/*
 * Copyright(C) 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#ifndef __BOARD_PORTING_H_
#define __BOARD_PORTING_H_
#include <mmc.h>
#include <spl.h>

/* board porting */
int spl_get_ddr_info(u64 *start, u64 *size);
int spl_fixup_os_fdt(void *fdt);
const char * spl_get_fit_dtb_name(int do_multi_check);

/* uboot weak user-defined */
const char * board_get_fit_config(void);
int board_fit_each_image_post_load(const void *fit, int noffset, ulong loadaddr, ulong len);
int board_spl_fit_is_verify(void);

/* uboot weak standard */
int spl_board_init_f(void);
u32 spl_mmc_boot_mode(struct mmc *mmc, const u32 boot_device);
void spl_perform_fixups(struct spl_image_info *spl_image);

#endif
