/*
 * Copyright(C) 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#ifndef __BOOT_IMAGE_H_
#define __BOOT_IMAGE_H_

void *find_uboot_fdt_blob(void);
u32 spl_boot_get_device(void);
u32 spl_boot_device(void);
const char * board_get_fit_config(void);

#endif
