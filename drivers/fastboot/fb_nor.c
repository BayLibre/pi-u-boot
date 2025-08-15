// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <config.h>
#include <blk.h>
#include <mtd.h>

#include <fastboot.h>
#include <fb_nor.h>

static struct mtd_info *get_mtd_by_name(const char *name)
{
	struct mtd_info *mtd;

	mtd_probe_devices();

	mtd = get_mtd_device_nm(name);
	if (IS_ERR_OR_NULL(mtd)) {
		return NULL;
	}

	return mtd;
}

/**
 * fastboot_nor_flash_write() - Write image to NOR for fastboot
 *
 * @cmd: Named device to write image to
 * @download_buffer: Pointer to image data
 * @download_bytes: Size of image data
 * @response: Pointer to fastboot response buffer
 */
void fastboot_nor_flash_write(const char *cmd, void *download_buffer, u32 download_bytes,
			      char *response)
{
	int ret;
	struct mtd_info *mtd = NULL;
	size_t retlen;

	mtd = get_mtd_by_name(cmd);

	if (mtd == NULL) {
		pr_err("mtd device %s not found", cmd);
		return;
	}

	printf("Flashing raw image at offset 0x%llx\n", mtd->offset);
	ret = mtd_write(mtd, 0, download_bytes, &retlen, download_buffer);

	if (ret) {
		fastboot_fail("error writing the image", response);
		return;
	}

	printf("........ wrote %u bytes to '%s'\n",
			download_bytes, mtd->name);

	fastboot_okay(NULL, response);
}

/**
 * fastboot_nor_flash_erase() - Erase NOR for fastboot
 *
 * @cmd: Named device to erase
 * @response: Pointer to fastboot response buffer
 */
void fastboot_nor_erase(const char *cmd, char *response)
{
	int ret;

	struct mtd_info *mtd = NULL;
	struct erase_info erase = { 0 };

	mtd = get_mtd_by_name(cmd);

	if (mtd == NULL) {
		pr_err("mtd device %s not found", cmd);
		return;
	}

	printf("Erasing blocks 0x%llx to 0x%llx\n", mtd->offset, mtd->offset + mtd->size);

	erase.mtd = mtd;
	erase.addr = 0;
	erase.len = mtd->size;

	ret = mtd_erase(mtd, &erase);
	if (ret) {
		fastboot_fail("failed erasing from device", response);
		return;
	}

	printf("........ erased 0x%llx bytes from '%s'\n",
	       mtd->size, mtd->name);

	fastboot_okay(NULL, response);
}
