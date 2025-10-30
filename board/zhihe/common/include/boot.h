/*
 * Copyright(C) 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#ifndef __BOOT_IMAGE_H_
#define __BOOT_IMAGE_H_

/* boot_method */
u32 spl_boot_get_device(void);
u32 spl_boot_device(void);

/* boot info */
char *spl_get_osfdt_info(ulong *paddr);
int spl_get_mmc_bootfs_partition(void);
void *spl_find_uboot_fdt_blob(void);

/* weak functions */
const char * board_get_fit_config(void);
#endif
