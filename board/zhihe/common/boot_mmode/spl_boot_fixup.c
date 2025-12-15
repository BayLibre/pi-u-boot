// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

//#define DEBUG
#include <errno.h>
#include <image.h>
#include <log.h>
#include <mapmem.h>
#include <spl.h>
#include <sysinfo.h>
#include "../include/board_boot.h"

#ifndef CONFIG_FIT

/*
 * Get ddr start addres & size
 * Please implement this function in the
 * board-level code to override the weak implementation.
 */
__weak int spl_get_ddr_info(u64 *start, u64 *size)
{
    return -1;
}

/*
 * Fix the issue where the full fit mode cannot access the next level of OS entry
 * spl_perform_fixups is weak imp at u-boot/common/spl/spl.c
 */
void spl_perform_fixups(struct spl_image_info *spl_image)
{
    u64 start;
    u64 size;
    const char *board_type;
    int chosen;
    void *fdt_uboot = spl_find_uboot_fdt_blob();
    if (!fdt_uboot) {
        return;
    }
    debug("uboot fdt blob 0x%p\n", fdt_uboot);

    /* 1. Fixup DDR size, write to u-boot fdt */
    if (spl_get_ddr_info(&start, &size) == 0) {
        int ret = fdt_fixup_memory(fdt_uboot, start, size);
        debug("fixup mem ret %d\n", ret);
        if (ret) {
            printf("Warning: failed fixup memeory\n");
        }
    }

    /* 2. Set board type pass to u-boot */
    chosen = fdt_find_or_add_subnode(fdt_uboot, 0, "chosen");
    if (chosen >= 0) {
        board_type = board_get_fit_config();
        fdt_setprop_string(fdt_uboot, chosen, "board", board_type);
    } else {
        pr_err("%s: could not find/create '/chosen'\n", __func__);
    }
}
#endif
