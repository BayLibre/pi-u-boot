/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#ifndef _FB_NOR_H_
#define _FB_NOR_H_

/**
 * fastboot_nor_flash_write() - Write image to NOR for fastboot
 *
 * @cmd: Named device to write image to
 * @download_buffer: Pointer to image data
 * @download_bytes: Size of image data
 * @response: Pointer to fastboot response buffer
 */
void fastboot_nor_flash_write(const char *cmd, void *download_buffer, u32 download_bytes,
			      char *response);

/**
 * fastboot_nor_flash_erase() - Erase NOR for fastboot
 *
 * @cmd: Named device to erase
 * @response: Pointer to fastboot response buffer
 */
void fastboot_nor_erase(const char *cmd, char *response);
#endif
