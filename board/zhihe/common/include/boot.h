/*
 * Copyright(C) 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#ifndef __BOOT_IMAGE_H_
#define __BOOT_IMAGE_H_

enum ddr_type {
    /* Auto size check */
    DDR_LP4X_3200_1Rank, /* a200-evb */
    DDR_LP4X_3200_2Rank,
    DDR_LP4X_3733_1Rank,  
    DDR_LP4X_3733_2Rank, /* p1 */
    DDR_LP4X_4266_1Rank,
    DDR_LP4X_4266_2Rank,

    /* Fix size */
    DDR_LP4X_4266_1Rank_2GBx2, /* a210-evb */
    DDR_LP4X_4266_1Rank_4GBx2, /* a210-evb */
    DDR_LP4X_4266_2Rank_8GBx2, /* a210-evb */

    DDR_TYPE_UNKNOWN,
};

/* boot_method */
u32 spl_boot_device(void);

/* boot_image */
int spl_load_dtb_from_bootfs(void);
void *spl_find_uboot_fdt_blob(void);

/* boot info */
char *spl_env_get_os_dtb(ulong *paddr);
int spl_env_get_mmc_bootfs_partid(void);

/* board check */
#define MAX_DTB_FILENAME_LEN 64
const char *uboot_get_binfo_from_fdt(void *fdt_uboot);
const char *uboot_sync_fdt_binfo_to_env(void *fdt_uboot);
int spl_set_binfo_to_uboot_fdt(void *fdt_uboot);
const char *spl_multi_fit_check(const char *suffix);

#endif
