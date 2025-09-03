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

static struct payload_info {
    ulong payload_addr;
    u32 payload_size;
} priv_data;

static ulong spl_ram_load_read(struct spl_load_info *load, ulong sector, ulong count, void *buf)
{
    ulong read_addr = priv_data.payload_addr + sector;

    debug("%s: sector %lx, count %lx, buf %lx\n", __func__, sector, count, (ulong)buf);
    memcpy(buf, map_sysmem(read_addr, 0), count);

    return count;
}

static int spl_with_fit_load_image(struct spl_image_info *spl_image,
                                        struct spl_boot_device *bootdev)
{
    struct legacy_img_hdr *fit_header;
    int ret;

    /* payload check */
    struct zhihe_image_header *header = (struct zhihe_image_header *)(CONFIG_SPL_TEXT_BASE - ZHIHE_PKG_HEAD_SIZE);

    if (header->magic == RVBL_MAGIC) {
        /* RVBL Header */
        priv_data.payload_addr = (CONFIG_SPL_TEXT_BASE - ZHIHE_PKG_HEAD_SIZE) + header->image_size;
        if (header->rvbl_payload_size == 0) {
            return -1;
        }
        priv_data.payload_size = header->rvbl_payload_size;
    } else if (header->magic == PKSE_MAGIC) {
        /* PKSE Header */
        priv_data.payload_addr = CONFIG_SPL_TEXT_BASE + header->image_size - PUBKEYC_SIZE + SIGOFCODE_SIZE + ZHIHE_PKG_PUBKEY_HEAD_SIZE;
        /* The size of the payload is not defined in the PKSE format. */
        priv_data.payload_size = 0;
    } else {
        return -1;
    }

    debug("FIT playload addr 0x%lx\n", priv_data.payload_addr);

    /* Fit Check */
    fit_header = map_sysmem(priv_data.payload_addr, 0);
    if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) && image_get_magic(fit_header) == FDT_MAGIC) {
        struct spl_load_info load;

        debug("Found FIT\n");

        spl_set_bl_len(&load, 1);
        load.read = spl_ram_load_read;
        if (IS_ENABLED(CONFIG_SPL_LOAD_FIT_FULL)) {
            ret = spl_load(spl_image, bootdev, &load, priv_data.payload_size, 0);
        } else {
            ret = spl_load_simple_fit(spl_image, &load, 0, fit_header);
        }
    } else {
        return -1;
    }

    return ret;
}

SPL_LOAD_IMAGE_METHOD("SPL_WITH_FIT", 0, BOOT_DEVICE_BOOTROM, spl_with_fit_load_image);
