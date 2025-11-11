// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <log.h>
#include <env.h>
#include <string.h>
#include <linux/types.h>
#include <cpu_func.h>
#include "../common/include/board_porting.h"

/* 
 * The weak implementation of this function is in the u-boot/common/spl/spl_opensbi.c file.
 * The implementation uses BRAM as a trampoline to invoke OpenSBI.
 */
extern int spl_call_opensbi(void * entry, ulong hartid, ulong dtb, ulong info, ulong slc_en);
int board_spl_call_opensbi(void * entry, ulong hartid, ulong dtb, ulong info)
{
	ulong slc_en = env_get_ulong("slc_en", 10, 0);
	
	printf("## Load SLC Firmware(%ld)", slc_en);

	spl_call_opensbi(entry, hartid, dtb, info, slc_en);

	/* Never arrive here */
	return 0;
}
