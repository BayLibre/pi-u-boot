// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2024-2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <cpu_func.h>
#include <asm/io.h>
#include "include/board.h"

/*
0 x x : Fastboot
1 0 0 : eMMC Boot
1 0 1 : SD Boot,SDIO0
1 1 0 : SPI NAND boot,QSPI0,CS0
1 1 1 : SPI NOR boot,QSPI0,CS0
*/
int loader_get_boot_sel(void)
{
	int boot_sel = readl((void *)SOC_OM_ADDRBASE) & 0x7;
	return boot_sel;
}

/*
 * Check whether the system is in flashing mode.
 */
int uboot_bootrom_fastboot(void)
{
	if (loader_get_boot_sel() == 0) {
		return 1;
	}
	return 0;
}
