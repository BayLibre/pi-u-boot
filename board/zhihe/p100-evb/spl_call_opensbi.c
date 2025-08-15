// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <log.h>
#include <env.h>
#include <string.h>
#include <linux/types.h>
#include <cpu_func.h>

#include "slc_firmware.h"

/* 
 * The weak implementation of this function is in the u-boot/common/spl/spl_opensbi.c file.
 * The implementation uses BRAM as a trampoline to invoke OpenSBI.
 */
typedef void (*bram_entry_t)(void);
int board_spl_call_opensbi(void * entry, ulong hartid, ulong dtb, ulong info)
{
	unsigned int slc_fw_len = slc_fw_dis_len;
	unsigned char *slc_fw = slc_fw_dis;

	ulong slc_en = env_get_ulong("slc_en", 10, 0);
	if (slc_en) {
		slc_fw_len = slc_fw_en_len;
		slc_fw = slc_fw_en;
	}

	printf("SLC Firmware(%ld,%d): %s", slc_en, slc_fw_len, SLC_FIRMWARE_BUILD_TIME);

	/* Prepare BRAM OpenSBI Eentry Info */
	struct opensbi_entry_info *entry_info = (struct opensbi_entry_info *)OPENSBI_ENTRY_INFO_ADDR;
	entry_info->entry = entry;
	entry_info->hartid = hartid;
	entry_info->dtb = dtb;
	entry_info->opensbi_info = *((struct fw_dynamic_info *)info);
	
	/* Copy Firmware To BRAM */
	memcpy((void *)SLC_FIRMWARE_LOAD_ADDR, slc_fw, slc_fw_len);
	flush_dcache_range(SLC_FIRMWARE_LOAD_ADDR, SLC_FIRMWARE_LOAD_ADDR + slc_fw_len);
	invalidate_icache_all();

	/* Jump To BRAM Firmware */
	bram_entry_t bram_entry = (bram_entry_t)SLC_FIRMWARE_LOAD_ADDR;
	bram_entry();

	return 0;
}
