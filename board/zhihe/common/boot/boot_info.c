// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <log.h>
#include <env.h>

#ifdef CONFIG_SPL_ENV_SUPPORT
/*
 * This function is called before loading the FIT file to return the eMMC partition ID
 * For this function to take effect, CONFIG_SYS_MMCSD_FS_BOOT_PARTITION must not be defined as -1
 */
int spl_env_get_mmc_bootfs_partid(void)
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

char *spl_env_get_os_dtb(ulong *paddr)
{
    *paddr = env_get_hex("dtb_addr", 0);
    return env_get("dtb_file");
}

#endif
