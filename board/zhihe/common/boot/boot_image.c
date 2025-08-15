// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

//#define DEBUG
#include <image.h>
#include <log.h>

#define MAX_UBOOT_SIZE ((1024 + 512) * 1024)
#define SKIP_FIND_OFFSET (512 * 1024)

/*
 * Find uboot fdt blob
 */
void *find_uboot_fdt_blob(void)
{
    int ret;
    const uint32_t *blob = (int *)ALIGN(CONFIG_TEXT_BASE, 4);

    /* FDT_MAGIC defined with big-endian mode. */
    uint32_t fdt_magic = fdt32_to_cpu(FDT_MAGIC);
    debug("cpu fdt magic 0x%x\n", fdt_magic);

    /* Find magic */
    for (int i = ALIGN(SKIP_FIND_OFFSET, 4) / 4; i < (MAX_UBOOT_SIZE / 4); i++) {
        if (blob[i] == fdt_magic) {
            ret = fdt_check_header(&blob[i]);
            debug("fdt check %d\n", ret);
            if (ret == 0) {
                return (void *)&blob[i];
            }
        }
    }

    return NULL;
}
