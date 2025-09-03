// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2024 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#ifndef _DDR_UTILS_IO_H
#define _DDR_UTILS_IO_H

#include <asm-generic/int-ll64.h>
#include <linux/delay.h>

/*
 * register access tools
 */
#define BIT(nr) (1UL << (nr))
#define MEM32(addr) *((volatile unsigned int *)(addr))
#define MEM16(addr) *((volatile unsigned short *)(addr))
#define MEM8(addr) *((volatile unsigned char *)(addr))

static inline void wr(u64 addr, u32 data)
{
	MEM32(addr) = data;
}

static inline void wr16(u64 addr, u16 data)
{
	MEM16(addr) = data;
}

static inline void wr8(u64 addr, u8 data)
{
	MEM8(addr) = data;
}

static inline u32 rd(u64 addr)
{
	u32 data;
	data = MEM32(addr);
	return data;
}

static inline u16 rd16(u64 addr)
{
	u32 data;
	data = MEM16(addr);
	return (u16)data;
}

static inline u8 rd8(u64 addr)
{
	u32 data;
	data = MEM8(addr);
	return (u8)data;
}

static inline u32 wr_rd(u64 addr, u32 data)
{
	wr(addr, data);
	return rd(addr);
}

static inline void reg_bit_set(u64 addr, u32 start_bit, u32 num, u32 value)
{
	u32 data;
	u32 bit_mask = ((1 << num) - 1);
	data = rd(addr);
	data &= ~(bit_mask << start_bit);
	data |= ((value & bit_mask) << start_bit);
	wr(addr, data);
}

#endif /* _DDR_UTILS_IO_H */
