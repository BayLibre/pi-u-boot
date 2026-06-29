/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024 SpacemiT Technology Co. Ltd
 * Copyright (c) 2024-2025 Haylen Chu <heylenay@4d2.org>
 */

#ifndef _CCU_PLLA_H_
#define _CCU_PLLA_H_

#include "common.h"

#define CCU_CLK_PLLA "ccu_clk_plla"
struct ccu_plla_rate_tbl {
	unsigned long long rate;
	u32 swcr1;
	u32 swcr2;
	u32 swcr3;
};

struct ccu_plla_config {
	struct ccu_plla_rate_tbl *rate_tbl;
	u32 tbl_size;
	void __iomem *lock_base;
	u32 reg_lock;
	u32 lock_enable_bit;
};

#define PLLA_RATE(_rate, _swcr1, _swcr2, _swcr3) \
	{									\
		.rate	= _rate,						\
		.swcr1	= _swcr1,						\
		.swcr2	= _swcr2,						\
		.swcr3	= _swcr3,						\
	}

struct ccu_plla {
	struct ccu_plla_config	pll;
	struct ccu_common	common;
};

#define _SPACEMIT_CCU_PLLA_CONFIG(_table, _size, _reg_lock, _lock_enable_bit)	\
	{									\
		.rate_tbl	= (struct ccu_plla_rate_tbl *)_table,		\
		.tbl_size	= _size,					\
		.reg_lock 	= _reg_lock,					\
		.lock_enable_bit	= _lock_enable_bit,			\
	}

#define SPACEMIT_CCU_PLLA(_struct, _name, _table, _size,		\
			_base_type, _reg_ctrl, _reg_sel, _reg_xtc,	\
			_reg_lock, _lock_enable_bit, _is_pll,		\
			_flags)						\
	struct ccu_plla _struct = {					\
		.pll	= _SPACEMIT_CCU_PLLA_CONFIG(_table, _size, _reg_lock, _lock_enable_bit),	\
		.common = { 						\
			.reg_ctrl		= _reg_ctrl, 		\
			.reg_sel		= _reg_sel, 		\
			.reg_xtc		= _reg_xtc, 		\
			.base_type		= _base_type,		\
			.is_pll 		= _is_pll,		\
			.name			= _name,		\
			.parent_name	= SPACEMIT_CLK_NO_PARENT,	\
			.num_parents	= 1,				\
			.driver_name	= CCU_CLK_PLLA,			\
		}							\
	}


static inline struct ccu_plla *clk_to_ccu_plla(struct clk *clk)
{
	struct ccu_common *common = clk_to_ccu_common(clk);

	return container_of(common, struct ccu_plla, common);
}

#endif
