// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 SpacemiT Technology Co. Ltd
 * Copyright (c) 2024-2025 Haylen Chu <heylenay@4d2.org>
 */

#include <asm/io.h>
#include <malloc.h>
#include <clk-uclass.h>
#include <dm/device.h>
#include <dm/devres.h>
#include <linux/bitops.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/iopoll.h>
#include <clk.h>
#include <div64.h>
#include "ccu_plla.h"

#define PLL_MIN_FREQ 600000000
#define PLL_MAX_FREQ 3400000000
#define PLL_DELAYTIME 590 //(590*5)us

#define PLLA_SWCR2_MASK 	GENMASK(15, 8)

#define plla_readl(reg)		readl(reg)
#define plla_readl_pll_swcr1(p)	plla_readl(p.base + p.reg_ctrl)
#define plla_readl_pll_swcr2(p)	plla_readl(p.base + p.reg_sel)
#define plla_readl_pll_swcr3(p)	plla_readl(p.base + p.reg_xtc)

#define plla_writel(val, reg)		writel(val, reg)
#define plla_writel_pll_swcr1(val, p)	plla_writel(val, p.base + p.reg_ctrl)
#define plla_writel_pll_swcr2(val, p)	plla_writel(val, p.base + p.reg_sel)
#define plla_writel_pll_swcr3(val, p)	plla_writel(val, p.base + p.reg_xtc)

/* unified pllax_swcr1 for plla */
union pllax_swcr1 {
	struct {
		unsigned int reg1:8;
		unsigned int reg2:8;
		unsigned int reg3:8;
		unsigned int reg4:8;
	} b;
	unsigned int v;
};

/* unified pllax_swcr2 for plla */
union pllax_swcr2 {
	struct {
		unsigned int div1_en:1;
		unsigned int div2_en:1;
		unsigned int div3_en:1;
		unsigned int div4_en:1;
		unsigned int div5_en:1;
		unsigned int div6_en:1;
		unsigned int div7_en:1;
		unsigned int div8_en:1;
		unsigned int reg0:8;
		unsigned int pll_en:1;
		unsigned int mon_cfg:4;
		unsigned int div10_en:1;
		unsigned int mmd_en:1;
		unsigned int mmd:5;
		unsigned int reserved2:3;
		unsigned int div64_en:1;
	} b;
	unsigned int v;
};

/* unified pllax_swcr3 for plla */
union pllax_swcr3{
	struct {
		unsigned int reg5:8;
		unsigned int reg6:8;
		unsigned int reg7:8;
		unsigned int reg8:8;
	} b;

	unsigned int v;
};

static int ccu_plla_is_enabled(struct clk *clk)
{
	struct ccu_plla *p = clk_to_ccu_plla(clk);
	union pllax_swcr2 swcr2;
	unsigned int enabled;

	swcr2.v = plla_readl_pll_swcr2(p->common);
	enabled = swcr2.b.pll_en;

	return enabled;
}

/* frequency unit Mhz, return pll vco freq */
static ulong __get_vco_freq(struct clk *clk)
{
	unsigned int size, i;
	struct ccu_plla_rate_tbl *freq_pll_regs_table;
	struct ccu_plla *p = clk_to_ccu_plla(clk);
	union pllax_swcr1 swcr1;
	union pllax_swcr2 swcr2;
	union pllax_swcr3 swcr3;

	swcr1.v = plla_readl_pll_swcr1(p->common);
	swcr2.v = plla_readl_pll_swcr2(p->common);
	swcr2.v &= PLLA_SWCR2_MASK;
	swcr3.v = plla_readl_pll_swcr3(p->common);

	freq_pll_regs_table = p->pll.rate_tbl;
	size = p->pll.tbl_size;

	for (i = 0; i < size; i++) {
		if ((freq_pll_regs_table[i].swcr1 == swcr1.v) &&
                    (freq_pll_regs_table[i].swcr2 == swcr2.v) &&
                    (freq_pll_regs_table[i].swcr3 == swcr3.v))
			return freq_pll_regs_table[i].rate;
    }

	pr_info("Unknown rate for clock\n");
	return 0;
}

static int ccu_plla_enable(struct clk *clk)
{
	unsigned int delaytime = PLL_DELAYTIME;
	unsigned long flags;
	struct ccu_plla *p = clk_to_ccu_plla(clk);
	union pllax_swcr2 swcr2;

	if (ccu_plla_is_enabled(clk))
		return 0;

	spin_lock_irqsave(p->common.lock, flags);
	swcr2.v = plla_readl_pll_swcr2(p->common);
	swcr2.b.pll_en = 1;
	plla_writel_pll_swcr2(swcr2.v, p->common);
	spin_unlock_irqrestore(p->common.lock, flags);

	/* check lock status */
	udelay(50);

	while ((!(readl(p->pll.lock_base + p->pll.reg_lock) & p->pll.lock_enable_bit))
	       && delaytime) {
		udelay(5);
		delaytime--;
	}
	if (unlikely(!delaytime))
		pr_err("ccu_pll_enable enabling didn't get stable within 3000us!!!\n" );

	return 0;
}

static int ccu_plla_disable(struct clk *clk)
{
	struct ccu_plla *p = clk_to_ccu_plla(clk);
	union pllax_swcr2 swcr2;

	swcr2.v = plla_readl_pll_swcr2(p->common);
	swcr2.b.pll_en = 0;
	plla_writel_pll_swcr2(swcr2.v, p->common);
	return 0;

}

/*
 * pll rate change requires sequence:
 * clock off -> change rate setting -> clock on
 * This function doesn't really change rate, but cache the config
 */
static ulong ccu_plla_set_rate(struct clk *clk, ulong rate)
{
	unsigned int i, reg0 = 0;
	unsigned long old_rate;
	struct ccu_plla *p = clk_to_ccu_plla(clk);
	struct ccu_plla_config *params = &p->pll;
	union pllax_swcr1 swcr1;
	union pllax_swcr1 swcr2;
	union pllax_swcr3 swcr3;
	bool found = false;
	bool pll_enabled = false;

	if (clk_get_rate(clk) == rate)
		return 0;

	if (ccu_plla_is_enabled(clk)) {
		pll_enabled = true;
		ccu_plla_disable(clk);
	}

	old_rate = __get_vco_freq(clk);
	/* setp 1: calculate fbd frcd kvco and band */
	if (params->rate_tbl) {
		for (i = 0; i < params->tbl_size; i++) {
			if (rate == params->rate_tbl[i].rate) {
				found = true;

				swcr1.v = params->rate_tbl[i].swcr1;
				reg0 = params->rate_tbl[i].swcr2;
				swcr3.v = params->rate_tbl[i].swcr3;
				break;
			}
		}
	} else {
		pr_err("don't find freq table for pll\n");
		if (pll_enabled)
			ccu_plla_enable(clk);
		return -EINVAL;
	}

	/* setp 2: set pll kvco/band and fbd/frcd setting */
	plla_writel_pll_swcr1(swcr1.v, p->common);

	swcr2.v = plla_readl_pll_swcr2(p->common);
	swcr2.v &= ~PLLA_SWCR2_MASK;
	swcr2.v |= reg0;
	plla_writel_pll_swcr2(swcr2.v, p->common);

	plla_writel_pll_swcr3(swcr3.v, p->common);

	if (pll_enabled)
		ccu_plla_enable(clk);
	return 0;
}

static ulong ccu_plla_get_rate(struct clk *clk)
{
	ulong val;
	val = __get_vco_freq(clk);
	return val;
}

static ulong ccu_plla_round_rate(struct clk *clk, ulong rate)
{
	struct ccu_plla *p = clk_to_ccu_plla(clk);
	unsigned long max_rate = 0;
	unsigned int i;
	struct ccu_plla_config *params = &p->pll;

	if (rate > PLL_MAX_FREQ || rate < PLL_MIN_FREQ) {
		pr_err("%lu rate out of range!\n", rate);
		return -EINVAL;
	}

	if (params->rate_tbl) {
		for (i = 0; i < params->tbl_size; i++) {
			if (params->rate_tbl[i].rate <= rate) {
				if (max_rate < params->rate_tbl[i].rate)
					max_rate = params->rate_tbl[i].rate;
			}
		}
	} else {
		pr_info("don't find freq table for pll\n");
	}
	return max_rate;
}

const struct clk_ops ccu_plla_ops = {
	.enable 	= ccu_plla_enable,
	.disable 	= ccu_plla_disable,
	.set_rate 	= ccu_plla_set_rate,
	.get_rate 	= ccu_plla_get_rate,
	.round_rate	= ccu_plla_round_rate,
};

U_BOOT_DRIVER(ccu_clk_plla) = {
	.name	= CCU_CLK_PLLA,
	.id	= UCLASS_CLK,
	.ops	= &ccu_plla_ops,
};
