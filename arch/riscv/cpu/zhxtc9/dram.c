// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright(C) 2024-2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <fdtdec.h>
#include <init.h>
#include <asm/global_data.h>
#include <linux/sizes.h>

DECLARE_GLOBAL_DATA_PTR;

int dram_init(void)
{
	return fdtdec_setup_mem_size_base();
}

int dram_init_banksize(void)
{
	return fdtdec_setup_memory_banksize();
}

/**
 * This function overrides the weak implementation in board_f.c,
 * limiting the system addressing to no more than 32 bits,
 * solving the problem of the inability to use DMA for GMAC
 */
phys_addr_t board_get_usable_ram_top(phys_size_t total_size)
{
	if (gd->ram_top > SZ_4G)
		return SZ_4G;

	return gd->ram_top;
}
