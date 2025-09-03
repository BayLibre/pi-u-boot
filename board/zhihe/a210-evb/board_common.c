// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2024-2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <cpu_func.h>
#include <asm/io.h>
#include "include/addr_defines.h"
#include "include/board.h"

int board_get_boot_sel(void)
{
    uint32_t mcm_en = (*(volatile uint32_t *) (BOOTSEL_MCM_REG_ADDR)) & 0x01; // mcm_en:bit 0
    uint32_t boot_sel = (*(volatile uint32_t *) (BOOTSEL_REG_ADDR)) & 0x07; // bool_sel:bit 0~2

    boot_sel = (mcm_en << 3) | boot_sel;
    return boot_sel;
}

int board_bootrom_fastboot(void)
{
	int boot_sel = board_get_boot_sel();

	if (boot_sel == BOOT_SEL_FASTBOOT ||
		boot_sel == BOOT_SEL_MCM_FASTBOOT) {
		return 1;
	}

	return 0;
}

#define MCM_OFFSET	(3)
#define MCM_MASK	(1 << MCM_OFFSET)

#define MDIE_OFFSET	(0)
#define MDIE_MASK	(1 << MDIE_OFFSET)
int board_get_die_count(void)
{
	/* mcm bootsel[2] bootsel[1:0] */
	uint32_t boot_sel_raw = board_get_boot_sel();
	if (!(boot_sel_raw & MCM_MASK))
		return 1;

	/*
	 * 2DIE 1_010 1_110
	 * 4DIE 1_011 1_111 
	 */
	if (boot_sel_raw & MDIE_MASK)
		return 4;
	else
		return 2;
}
