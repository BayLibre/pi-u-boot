/*
 * Copyright(C) 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#ifndef __BOOT_IMAGE_H_
#define __BOOT_IMAGE_H_

/* boot_method */
u32 spl_boot_device(void);

/* boot_image */
int spl_load_dtb_from_bootfs(void);
void *spl_find_uboot_fdt_blob(void);

/* boot info */
char *spl_env_get_os_dtb(ulong *paddr);
int spl_env_get_mmc_bootfs_partid(void);

/* board check */
const char *board_get_binfo_from_fdt(void *fdt_uboot);
int board_set_binfo_to_fdt(void *fdt_uboot);
const char *board_multi_fit_check(const char *suffix);

#endif
