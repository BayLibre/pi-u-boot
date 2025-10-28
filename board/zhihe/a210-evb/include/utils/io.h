// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2024 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#ifndef _DDR_UTILS_IO_H
#define _DDR_UTILS_IO_H

#include <asm-generic/int-ll64.h>
#include <asm/io.h>

/*
 * register access tools
 */
#define BIT(nr) (1UL << (nr))

#if defined(CONFIG_SPL_BUILD)
#define CHIP_OFFSET (0x2000000000LL)
/* set base to tp */
static inline void chip_set(int chip_id)
{
	u64 base = (u64)chip_id * CHIP_OFFSET;

	asm volatile("mv tp, %0" :: "r"(base) : "tp");
}

/* return tp + addr */
static inline u64 chip_uniform_addr(long addr)
{
	u64 result;
	asm volatile(
			"add %0, tp, %1"
			: "=r"(result)
			: "r"(addr)
		    );
	return result;
}
#else
#define chip_uniform_addr(addr) (addr)
#define chip_set(chip_id)
#endif

static inline void chip_wr(u64 addr, u32 data)
{
	addr = chip_uniform_addr(addr);
	writel(data, (volatile void __iomem *)addr);
}

static inline void chip_wr16(u64 addr, u16 data)
{
	addr = chip_uniform_addr(addr);
	writew(data, (volatile void __iomem *)addr);
}

static inline void chip_wr8(u64 addr, u8 data)
{
	addr = chip_uniform_addr(addr);
	writeb(data, (volatile void __iomem *)addr);
}

static inline u32 chip_rd(u64 addr)
{
	addr = chip_uniform_addr(addr);
	return readl((volatile void __iomem *)addr);
}

static inline u16 chip_rd16(u64 addr)
{
	addr = chip_uniform_addr(addr);
	return readw((volatile void __iomem *)addr);
}

static inline u8 chip_rd8(u64 addr)
{
	addr = chip_uniform_addr(addr);
	return readb((volatile void __iomem *)addr);
}

static inline u32 chip_wr_rd(u64 addr, u32 data)
{
	chip_wr(addr, data);
	return chip_rd(addr);
}

static inline void reg_bit_set(u64 addr, u32 start_bit, u32 num, u32 value)
{
	u32 data;
	u32 bit_mask = ((1 << num) - 1);
	data = chip_rd(addr);
	data &= ~(bit_mask << start_bit);
	data |= ((value & bit_mask) << start_bit);
	chip_wr(addr, data);
}

#endif /* _DDR_UTILS_IO_H */
