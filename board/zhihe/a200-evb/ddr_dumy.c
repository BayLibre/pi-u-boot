/*
* Copyright (C) 2024-2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
*
* SPDX-License-Identifier: GPL-2.0+
*/

#include <asm/asm.h>
#include <asm/io.h>

void init_ddr(void)
{
	writel(0x1ff << 4, (void *)0xffff005000);
	//writel(0xffffffff, (void *)0xffff005000);
}

int fixup_ddr_addrmap(unsigned long size)
{
	return 0;
}

int query_ddr_boundary(unsigned long size)
{
	return 0;
}
unsigned long get_ddr_density(void)
{
	return 0x100000000;
}
