// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <log.h>
#include <serial.h>
#include <time.h>
#include <spl.h>
#include <spl_load.h>
#include "../include/board_porting.h"
#include "../include/board_boot.h"

#if 0
/*
 * Uart boot select
 * Return
 *   1: get BOOT_DEVICE_BOOTROM Magic
 *   0: get nothing
 */
static int uart_boot_check(void)
{
	unsigned long us_start = timer_get_us();
	int getc_count = 0;

	while (timer_get_us() - us_start < 10000) {
		if (serial_tstc()) {
			char c = serial_getc();
			//printf("%c\n", c);
			if (c == '!') {
				getc_count ++;
				if (getc_count >= 1) {
					printf("Boot SPL u-boot\n");
					return 1;
				}
			}
		}
	}
	return 0;
}
#endif


/*
 * Check the SPL image header to verify whether the payload contains a FIT image.
 * Only the SPL_LOAD_IMAGE_METHOD("SPL_WITH_FIT", ...) uses the payload method to package the image.
 */
static int spl_has_fit_payload(void)
{
	ulong payload_addr = 0;
	struct legacy_img_hdr *fit_header;

    /* payload check */
    struct zhihe_image_header *header = (struct zhihe_image_header *)(CONFIG_SPL_TEXT_BASE - ZHIHE_PKG_HEAD_SIZE);

    if (header->magic == RVBL_MAGIC) {
        /* RVBL Header */
        payload_addr = (CONFIG_SPL_TEXT_BASE - ZHIHE_PKG_HEAD_SIZE) + header->image_size;
        if (header->rvbl_payload_size == 0) {
            return 0;
        }
    } else if (header->magic == PKSE_MAGIC) {
        /* PKSE Header */
        payload_addr = CONFIG_SPL_TEXT_BASE + header->image_size - PUBKEYC_SIZE + SIGOFCODE_SIZE + ZHIHE_PKG_PUBKEY_HEAD_SIZE;
    } else {
        return 0;
    }

    /* Fit Magic Check */
    fit_header = map_sysmem(payload_addr, 0);
    if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) && image_get_magic(fit_header) == FDT_MAGIC) {
		return 1;
	}

	return 0;
}

/*
 * Get first boot device
 */
u32 spl_boot_device(void)
{
	if (spl_has_fit_payload()) {
		/* cct or fastboot send spl-with-fit-rvbl.bin to boot sram */
		return BOOT_DEVICE_BOOTROM;
	} else {
		/* Default boot device */
		return BOOT_DEVICE_MMC1;
	}
}
