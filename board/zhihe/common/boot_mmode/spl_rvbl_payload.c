// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <image.h>
#include <log.h>
#include <mapmem.h>
#include <spl.h>
#include <spl_load.h>
#include "../include/board_boot.h"

#ifndef CONFIG_FIT
/*
 * XuanTie boot mode, M-Mode U-Boot
 * Load U-Boot from RVBL Payload (u-boot-with-spl-rvbl.bin)
 */
static int spl_rvbl_load_image(struct spl_image_info *spl_image, struct spl_boot_device *bootdev)
{
	struct zhihe_image_header *header = (struct zhihe_image_header *)(CONFIG_SPL_TEXT_BASE - ZHIHE_PKG_HEAD_SIZE);

	if (header->magic != RVBL_MAGIC) {
		return -1;
	}

	if (header->rvbl_payload_size == 0) {
		return -1;
	}

	/* Output image info */
	spl_image->entry_point = CONFIG_TEXT_BASE;
	spl_image->os = IH_OS_U_BOOT;

	/* Copy payload to entry address */
	memcpy((void *)spl_image->entry_point, (uint8_t *)header + header->image_size, header->rvbl_payload_size);

	return 0;
}

SPL_LOAD_IMAGE_METHOD("RVBL", 0, BOOT_DEVICE_BOOTROM, spl_rvbl_load_image);
#endif /* CONFIG_FIT */
