// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <image.h>
#include <log.h>
#include <mapmem.h>
#include <spl.h>
#include <spl_load.h>
#include "../include/pkg_header.h"

static ulong spl_ram_load_read(struct spl_load_info *load, ulong sector, ulong count, void *buf)
{
    ulong paylaod_addr = CONFIG_SPL_LOAD_FIT_ADDRESS;

    debug("%s: sector %lx, count %lx, buf %lx\n", __func__, sector, count, (ulong)buf);

    paylaod_addr += sector;
    memcpy(buf, map_sysmem(paylaod_addr, 0), count);

    return count;
}

static int spl_ram_load_image(struct spl_image_info *spl_image,
                                        struct spl_boot_device *bootdev)
{
    struct legacy_img_hdr *header;
    ulong paylaod_addr = CONFIG_SPL_LOAD_FIT_ADDRESS;
    int ret;

    /* Fit Check */
    header = map_sysmem(paylaod_addr, 0);
    if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) && image_get_magic(header) == FDT_MAGIC) {
        struct spl_load_info load;

        debug("Found FIT\n");

        spl_set_bl_len(&load, 1);
        load.read = spl_ram_load_read;
        if (IS_ENABLED(CONFIG_SPL_LOAD_FIT_FULL)) {
            ret = spl_load(spl_image, bootdev, &load, 0, 0);
        } else {
            ret = spl_load_simple_fit(spl_image, &load, 0, header);
        }
    } else {
        return -1;
    }

    return ret;
}

SPL_LOAD_IMAGE_METHOD("RAM_FIT_FULL", 0, BOOT_DEVICE_RAM, spl_ram_load_image);
