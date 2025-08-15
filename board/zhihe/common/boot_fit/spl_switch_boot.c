// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <log.h>
#include <env.h>

#ifdef CONFIG_SPL_ENV_SUPPORT
/*
 * The default weak implementation of this function is in common/spl/spl_mmc.c
 * This function is called before loading the FIT file to return the eMMC partition ID
 * For this function to take effect, CONFIG_SYS_MMCSD_FS_BOOT_PARTITION must not be defined as -1
 */
int board_get_mmc_boot_partition(void)
{
    char *act_slot;
    char bootpart_name[]="x_bootpart";
    int bootpart_id = CONFIG_SYS_MMCSD_FS_BOOT_PARTITION;

    act_slot = env_get("active_slot");

    if (act_slot) {
        bootpart_name[0] = act_slot[0];
        bootpart_id = env_get_hex(bootpart_name, CONFIG_SYS_MMCSD_FS_BOOT_PARTITION);
    }
    
    printf("## Boot AB\n");
    printf("  Active slot: %s\n", act_slot);
    printf("  Bootpart: %d\n", bootpart_id);
    return bootpart_id;
}
#endif
