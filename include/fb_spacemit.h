/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2023 Spacemit, Inc
 */

#ifndef _FB_SPACEMIT_H_
#define _FB_SPACEMIT_H_

#include <linux/types.h>
#include <linux/usb/ch9.h>

/* FLASH_CONFIG_FILE_NAME is used as the flag to trigger an SD-card flash. */
#define FLASH_CONFIG_FILE_NAME		("partition_universal.json")

/**
 * spacemit_k3_fastboot_speed_flags() - read the K3 fastboot speed scratch reg
 *
 * The K3 passes the fastboot maximum-speed selection from the SPL to U-Boot
 * proper through a CIU debug scratch register (BOOT_CIU_DEBUG_REG0).
 *
 * Return: raw scratch register value
 */
u32 spacemit_k3_fastboot_speed_flags(void);

/**
 * spacemit_k3_fastboot_requested_speed() - decode the requested USB speed
 *
 * Return: %USB_SPEED_SUPER if super-speed was requested, %USB_SPEED_HIGH
 *	   otherwise
 */
enum usb_device_speed spacemit_k3_fastboot_requested_speed(void);

/**
 * spacemit_k3_fastboot_set_superspeed_flag() - set/clear the super-speed flag
 * @enable: %true to request super-speed, %false for high-speed
 */
void spacemit_k3_fastboot_set_superspeed_flag(bool enable);

/**
 * fastboot_oem_flash_ec() - handle "fastboot oem ec:flash" (CrosEC RW update)
 * @cmd_parameter: OEM sub-command (only "flash" is supported)
 * @download_buffer: staged EC RW firmware image
 * @download_bytes: size of the staged image
 * @response: fastboot response buffer
 */
void fastboot_oem_flash_ec(const char *cmd_parameter, void *download_buffer,
			   u32 download_bytes, char *response);

#endif /* _FB_SPACEMIT_H_ */
