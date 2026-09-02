// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 Spacemit Co., Ltd.
 *
 */

#include "inno_dp_reg.h"
#include "inno_dp_phy.h"

#define dev_info(dev, fmt, ...) \
	pr_info("[DP PHY INFO] " fmt, ##__VA_ARGS__)
#define dev_err(dev, fmt, ...) \
	pr_info("[DP PHY INFO] " fmt, ##__VA_ARGS__)
#define dev_warn(dev, fmt, ...) \
	pr_info("[DP PHY INFO] " fmt, ##__VA_ARGS__)
#define dev_dbg(dev, fmt, ...) \
	pr_debug("[DP PHY DEBUG] " fmt, ##__VA_ARGS__)

#define EINVAL					-22

#define SOC_DP_VCO_MIN_KHZ			1000000
#define SOC_DP_VCO_MAX_KHZ			3000000
#define SOC_DP_PLL_FRAC_MOD			16777216 /* 2^24 */
#define SOC_DP_PLL_ERR_TOLERANCE		10

/* Mappings for PHY Swing/Emphasis Levels */
static const u32 phy_swing_map[] = { 0x0, 0x1, 0x2, 0x3 };
static const u32 phy_preemp_map[] = { 0x0, 0x1, 0x2, 0x3 };

/* Data structure for Core PLL results */
struct soc_dp_core_pll_cfg {
	u32 target_rate_kbps;
	u32 vco_freq_khz;
	u8 prediv;
	u16 fbdiv;
	u32 frac;
	u8 postdiv_reg;
	u8 frac_pd;
	u8 vcoclk_div8_en;
	u8 postdiv_en;
	u8 clk_16mdiv;
	u32 actual_rate_khz;
	bool valid;
};

/* Data structure for Pixel PLL results */
struct soc_dp_pixel_pll_cfg {
	u32 target_pclk_khz;
	u32 vco_freq_khz;
	u8 prediv;
	u16 fbdiv;
	u32 frac;
	u8 div5_en;
	u8 divm;
	u8 divaux;
	u8 divp;	// pclk_divaux
	u8 frac_pd;
	u32 actual_pclk_khz;
	bool valid;
};

static const struct soc_dp_phy_vol_setting {
	u8 mainsel;	/* 5 bits */
	u8 postsel;	/* 4 bits */
	u8 presel;	/* 3 bits */
	u8 isel;	/* 4 bits */
} vol_cfg_table[4][4][4] = {
	/* --- 1.62 Gbps --- */
	{
		/* Swing 0 */
		{ {0x0a, 0x0, 0x0, 0x5}, {0x0e, 0x2, 0x0, 0x5}, {0x11, 0x4, 0x0, 0x5}, {0, 0, 0, 0} },
		/* Swing 1 */
		{ {0x0c, 0x0, 0x0, 0x5}, {0x0f, 0x2, 0x0, 0x5}, {0x13, 0x5, 0x0, 0x5}, {0, 0, 0, 0} },
		/* Swing 2 */
		{ {0x0f, 0x0, 0x0, 0x5}, {0x12, 0x3, 0x0, 0x5}, {0x18, 0x6, 0x0, 0x5}, {0, 0, 0, 0} },
		/* Swing 3 */
		{ {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0} },
	},
	/* --- 2.7 Gbps --- */
	{
		/* Swing 0 */
		{ {0x07, 0x0, 0x0, 0x5}, {0x0a, 0x2, 0x0, 0x5}, {0x0f, 0x5, 0x0, 0x5}, {0, 0, 0, 0} },
		/* Swing 1 */
		{ {0x0d, 0x0, 0x0, 0x5}, {0x10, 0x2, 0x0, 0x5}, {0x14, 0x5, 0x0, 0x5}, {0, 0, 0, 0} },
		/* Swing 2 */
		{ {0x12, 0x0, 0x0, 0x5}, {0x16, 0x3, 0x0, 0x5}, {0x19, 0x6, 0x0, 0x5}, {0, 0, 0, 0} },
		/* Swing 3 */
		{ {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0} },
	},
	/* --- 5.4 Gbps --- */
	{
		/* Swing 0 */
		{ {0x06, 0x0, 0x0, 0x5}, {0x09, 0x1, 0x0, 0x5}, {0x0d, 0x3, 0x0, 0x5}, {0, 0, 0, 0} },
		/* Swing 1 */
		{ {0x0b, 0x0, 0x0, 0x5}, {0x12, 0x3, 0x0, 0x5}, {0x17, 0x5, 0x0, 0x5}, {0, 0, 0, 0} },
		/* Swing 2 */
		{ {0x10, 0x0, 0x0, 0x5}, {0x1d, 0x4, 0x0, 0x5}, {0x1a, 0x6, 0x0, 0x5}, {0, 0, 0, 0} },
		/* Swing 3 */
		{ {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0} },
	},
	/* --- 8.1 Gbps --- */
	{
		/* Swing 0 */
		{ {0x06, 0x0, 0x0, 0x5}, {0x09, 0x1, 0x0, 0x5}, {0x0d, 0x3, 0x0, 0x5}, {0, 0, 0, 0} },
		/* Swing 1 */
		{ {0x0b, 0x0, 0x0, 0x5}, {0x12, 0x3, 0x0, 0x5}, {0x17, 0x5, 0x0, 0x5}, {0, 0, 0, 0} },
		/* Swing 2 */
		{ {0x10, 0x0, 0x0, 0x5}, {0x1d, 0x4, 0x0, 0x5}, {0x1a, 0x6, 0x0, 0x5}, {0, 0, 0, 0} },
		/* Swing 3 */
		{ {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0} },
	},
};

static int soc_dp_reg_write(struct soc_dp_phy *phy, u32 offset,
				u32 bit_wide, u32 mask, u32 val)
{
	u32 reg_val;

	reg_val = (u32)readl((char *)phy->regs + offset);
	reg_val &= ~mask;
	reg_val |= val & mask;
	// dev_info(phy->dev, "[W] 0x%x 0x%x\n", offset, reg_val);
	writel(reg_val, (char *)phy->regs + offset);

	return 0;
}

static int soc_dp_reg_write_range(struct soc_dp_phy *phy, u32 offset,
				  u32 high, u32 low, u32 val)
{
	u32 mask;

	mask = (u32)(((((u64)1) << (high - low + 1)) - 1) << low);
	return soc_dp_reg_write(phy, offset, 32, mask, (val << low) & mask);
}

static int soc_dp_reg_read(struct soc_dp_phy *phy,
			   u32 offset, u32 bit_wide,
			   u32 mask, u32 *val)
{
	*val = ((u32)readl((char *)phy->regs + offset)) & mask;
	// dev_info(phy->dev, "[R] 0x%x 0x%x\n", offset, *val);
	return 0;
}

static int soc_dp_reg_read_range(struct soc_dp_phy *phy, u32 offset,
				 u32 high, u32 low, u32 *val)
{
	int ret;
	u32 mask;

	mask = (u32)(((((u64)1) << (high - low + 1)) - 1) << low);
	ret = soc_dp_reg_read(phy, offset, 32, mask, val);
	*val = *val >> low;

	return ret;
}

/*
 * soc_dp_div64
 * @n: Pointer to dividend (will be updated to quotient)
 * @base: Divisor
 * Return: Remainder
 */
static u32 soc_dp_div64(u64 *n, u32 base)
{
#if USED_ACTIVATE_DO_DIV
	return do_div(*n, base);
#else
	u32 rem = *n % base;
	*n = *n / base;
	return rem;
#endif
}

static u64 soc_dp_abs_diff(u64 a, u64 b)
{
	return (a > b) ? (a - b) : (b - a);
}

/*
 * soc_dp_is_better_config - Determines if the new configuration is better
 * Priorities:
 * 1. Integer mode (Frac == 0)
 * 2. Lower pre-divider (Higher PFD frequency)
 * 3. Higher VCO frequency
 */
static int soc_dp_is_better_config(bool new_valid, bool new_is_int,
				   u8 new_pre, u32 new_vco,
				   bool best_valid, bool best_is_int,
				   u8 best_pre, u32 best_vco)
{
	if (!new_valid) return 0;
	if (!best_valid) return 1;

	if (new_is_int && !best_is_int) return 1;
	if (!new_is_int && best_is_int) return 0;

	if (new_pre < best_pre) return 1;
	if (new_pre > best_pre) return 0;

	if (new_vco > best_vco) return 1;

	return 0;
}

/*
 * soc_dp_solve_pll_frac - Iterative solver for Fractional PLL
 * Searches for optimal Pre/FB/Frac parameters for a target VCO.
 */
static int soc_dp_solve_pll_frac(u32 target_vco_khz,
				 u32 ref_clk_khz, u8 *best_pre,
				 u16 *best_fb, u32 *best_frac)
{
	u64 min_err = ~0ULL;
	int found = 0;
	bool best_is_int = false;
	int pre;

	/* Iterate pre-divider 1 to 63 to find best PFD frequency */
	for (pre = 1; pre <= 63; pre++) {
		u64 ref_clk_hz = (u64)ref_clk_khz * 1000;
		u64 target_vco_hz = (u64)target_vco_khz * 1000;

		/* Calculate required multiplier: Mult = (TargetVCO * Pre) / Ref */
		u64 num = target_vco_hz * pre;
		u64 den = ref_clk_hz;
		u64 remainder;
		u64 fb_val;
		u64 frac_val;
		u64 actual_vco;
		u64 diff;
		bool current_is_int;

		fb_val = num;
		remainder = soc_dp_div64(&fb_val, (u32)den);

		if (fb_val > 4095) continue;

		/* Frac = (Remainder * 2^24 + Ref/2) / Ref */
		frac_val = remainder * SOC_DP_PLL_FRAC_MOD;
		frac_val += (den / 2);
		soc_dp_div64(&frac_val, (u32)den);

		if (frac_val > 0xFFFFFF)
			frac_val = 0xFFFFFF;

		/*
		 * Calculate actual VCO for error checking.
		 * VCO = (Ref * FB / Pre) + (Ref * Frac / (Pre * 2^24))
		 */
		{
			u64 vco_int, vco_frac;

			// Integer part: (Ref * FB) / Pre
			vco_int = ref_clk_hz * fb_val;
			soc_dp_div64(&vco_int, pre);

			// Fractional part: (Ref * Frac) / (Pre * 2^24)
			vco_frac = ref_clk_hz * frac_val;
			soc_dp_div64(&vco_frac, pre);
			soc_dp_div64(&vco_frac, SOC_DP_PLL_FRAC_MOD);

			actual_vco = vco_int + vco_frac;
		}

		diff = soc_dp_abs_diff(actual_vco, target_vco_hz);
		current_is_int = (frac_val == 0);

		if (diff < min_err) {
			min_err = diff;
			*best_pre = pre;
			*best_fb = (u16)fb_val;
			*best_frac = (u32)frac_val;
			best_is_int = current_is_int;
			found = 1;
		} else if (diff == min_err) {
			if (current_is_int && !best_is_int) {
				*best_pre = pre;
				*best_fb = (u16)fb_val;
				*best_frac = (u32)frac_val;
				best_is_int = true;
				found = 1;
			}
		}
	}

	return found ? 0 : -1;
}

/*
 * soc_dp_get_rate_khz - Reverse calculate frequency from parameters
 * Uses stepwise division to prevent 32-bit overflow in do_div base.
 */
static u32 soc_dp_get_rate_khz(u8 pre, u16 fb, u32 frac,
					u32 ref_clk_khz, u32 total_div)
{
	u64 vco_hz;
	u64 ref_hz = (u64)ref_clk_khz * 1000;
	u64 int_part;
	u64 frac_part;

	/* Integer part: (Ref * FB) / Pre */
	int_part = ref_hz * fb;
	int_part += (pre / 2);
	soc_dp_div64(&int_part, pre);

	/* Fractional part: (Ref * Frac) / (Pre * 2^24) */
	frac_part = ref_hz * frac;

	frac_part += (pre / 2);
	soc_dp_div64(&frac_part, pre);

	frac_part += (SOC_DP_PLL_FRAC_MOD / 2);
	soc_dp_div64(&frac_part, SOC_DP_PLL_FRAC_MOD);

	vco_hz = int_part + frac_part;

	/* Final Rate = VCO / total_div */
	vco_hz += (total_div / 2); // Rounding before final division
	soc_dp_div64(&vco_hz, total_div);

	vco_hz += 500; // Rounding for 1000
	soc_dp_div64(&vco_hz, 1000);

	return (u32)vco_hz;
}

static int soc_dp_calc_core_pll(u32 target_rate_kbps,
				u32 ref_clk_khz,
				struct soc_dp_core_pll_cfg *cfg)
{
	struct soc_dp_core_pll_cfg best = {0};
	u32 target_bitclk_base = target_rate_kbps / 2;
	int i;
	int valid_postdivs[] = {1, 2, 4, 8, 16, 32};

	memset(cfg, 0, sizeof(*cfg));

	for (i = 0; i < 6; i++) {
		struct soc_dp_core_pll_cfg curr = {0};
		int post_div_ratio = valid_postdivs[i];
		u32 target_vco = target_bitclk_base * post_div_ratio;
		u8 pre;
		u16 fb;
		u32 frac;

		if (target_vco < SOC_DP_VCO_MIN_KHZ || target_vco > SOC_DP_VCO_MAX_KHZ)
			continue;

		if (soc_dp_solve_pll_frac(target_vco, ref_clk_khz, &pre, &fb, &frac) == 0) {
			u32 actual_vco = soc_dp_get_rate_khz(pre, fb, frac, ref_clk_khz, 1);
			u32 actual_rate = (actual_vco / post_div_ratio) * 2;

			if (soc_dp_abs_diff(actual_rate, target_rate_kbps) > SOC_DP_PLL_ERR_TOLERANCE)
				continue;

			curr.valid = true;
			curr.vco_freq_khz = target_vco;
			curr.actual_rate_khz = actual_rate;
			curr.prediv = pre;
			curr.fbdiv = fb;
			curr.frac = frac;
			curr.frac_pd = (frac == 0) ? 3 : 0;

			/* Map post_div to register value */
			if (post_div_ratio == 1) {
				curr.postdiv_reg = 0; curr.postdiv_en = 0; curr.vcoclk_div8_en = 0;
			} else {
				if (post_div_ratio == 2) curr.postdiv_reg = 0;
				else if (post_div_ratio == 4) curr.postdiv_reg = 1;
				else if (post_div_ratio == 8) curr.postdiv_reg = 3;
				else if (post_div_ratio == 16) curr.postdiv_reg = 5;
				else if (post_div_ratio == 32) curr.postdiv_reg = 7;
				else curr.postdiv_reg = 0;

				curr.postdiv_en = 1; curr.vcoclk_div8_en = 1;
			}

			curr.clk_16mdiv = (actual_rate / 2) / 64000;

			if (soc_dp_is_better_config(curr.valid, (curr.frac==0), curr.prediv, curr.vco_freq_khz,
						    best.valid, (best.frac==0), best.prediv, best.vco_freq_khz)) {
				best = curr;
			}
		}
	}

	if (!best.valid)
		return -EINVAL;

	*cfg = best;
	return 0;
}

static void soc_dp_calc_core_pll_to_reg(struct soc_dp_phy *phy, struct soc_dp_core_pll_cfg *cfg)
{
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_MPLL_PD, 1);
	mdelay(2);

	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_MPLL_PREDIV, cfg->prediv);

	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_MPLL_FBDIV_LBIT, cfg->fbdiv & 0xFF);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_MPLL_FBDIV_HBIT, (cfg->fbdiv >> 8) & 0xF);

	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_MPLL_DACPD, (cfg->frac_pd >> 1) & 0x1);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_MPLL_DSMPD, cfg->frac_pd & 0x1);

	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_MPLL_FRAC_LBIT, cfg->frac & 0xFF);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_MPLL_FRAC_MBIT, (cfg->frac >> 8) & 0xFF);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_MPLL_FRAC_HBIT, (cfg->frac >> 16) & 0xFF);

	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_MPLL_POSTDIV, cfg->postdiv_reg);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_MPLL_POSTDIVEN, cfg->postdiv_en);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_MPLL_VCOCLK_DIV8_EN, cfg->vcoclk_div8_en);

	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_MPLL_CLKDIV_16M, cfg->clk_16mdiv);

	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_PREPLL_LOCK_BYPEN, 1);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_MPLL_PD, 0);
	mdelay(2);
}

/*
 * Note: Since standard PHY subsystem doesn't handle Pixel Clocks easily,
 * this logic is kept in PHY driver but exposed via custom API
 */
static int soc_dp_calc_pixel_pll(u32 target_pclk_khz,
				 u32 ref_clk_khz,
				 struct soc_dp_pixel_pll_cfg *cfg)
{
	struct soc_dp_pixel_pll_cfg best = {0};
	int pclk_div;

	/* Strategy 1: Div5 Path (VCO = PCLK * 5) */
	{
		struct soc_dp_pixel_pll_cfg curr = {0};
		u32 div_total = 5;
		u32 target_vco = target_pclk_khz * div_total;

		if (target_vco >= SOC_DP_VCO_MIN_KHZ && target_vco <= SOC_DP_VCO_MAX_KHZ) {
			u8 pre;
			u16 fb;
			u32 frac;

			if (soc_dp_solve_pll_frac(target_vco, ref_clk_khz, &pre, &fb, &frac) == 0) {
				u32 actual_pclk = soc_dp_get_rate_khz(pre, fb, frac, ref_clk_khz, div_total);

				if (soc_dp_abs_diff(actual_pclk, target_pclk_khz) <= SOC_DP_PLL_ERR_TOLERANCE) {
					curr.valid = true;
					curr.vco_freq_khz = target_vco;
					curr.actual_pclk_khz = actual_pclk;
					curr.prediv = pre; curr.fbdiv = fb; curr.frac = frac;

					curr.frac_pd = (frac == 0) ? 3 : 0;

					curr.div5_en = 1;
					curr.divaux = 0; curr.divm = 0; curr.divp = 0;

					if (soc_dp_is_better_config(curr.valid, (curr.frac==0), curr.prediv, curr.vco_freq_khz,
									best.valid, (best.frac==0), best.prediv, best.vco_freq_khz)) {
						best = curr;
					}
				}
			}
		}
	}

	/* Strategy 2 & 3: Iterate PclkDiv (1 to 31) */
	for (pclk_div = 1; pclk_div <= 31; pclk_div++) {

		/* Strategy 2: DivM Path (DivAux = 1) */
		int divm_factors[] = {1, 2, 3, 5};
		int divm_regs[]    = {0, 1, 2, 3};
		int i;

		for (i = 0; i < 4; i++) {
			struct soc_dp_pixel_pll_cfg curr = {0};
			int m_val = divm_factors[i];
			u32 div_total = 2 * m_val * pclk_div;
			u32 target_vco = target_pclk_khz * div_total;
			u8 pre;
			u16 fb;
			u32 frac;

			if (target_vco < SOC_DP_VCO_MIN_KHZ || target_vco > SOC_DP_VCO_MAX_KHZ) continue;

			if (soc_dp_solve_pll_frac(target_vco, ref_clk_khz, &pre, &fb, &frac) == 0) {
				u32 actual_pclk = soc_dp_get_rate_khz(pre, fb, frac, ref_clk_khz, div_total);

				if (soc_dp_abs_diff(actual_pclk, target_pclk_khz) > SOC_DP_PLL_ERR_TOLERANCE) continue;

				curr.valid = true;
				curr.vco_freq_khz = target_vco;
				curr.actual_pclk_khz = actual_pclk;
				curr.prediv = pre; curr.fbdiv = fb; curr.frac = frac;

				curr.frac_pd = (frac == 0) ? 3 : 0;

				curr.div5_en = 0;
				curr.divaux = 1;         /* Must be 1 to enable DivM logic */
				curr.divm = divm_regs[i];
				curr.divp = pclk_div;

				if (soc_dp_is_better_config(curr.valid, (curr.frac==0), curr.prediv, curr.vco_freq_khz,
								best.valid, (best.frac==0), best.prediv, best.vco_freq_khz)) {
					best = curr;
				}
			}
		}

		/* Strategy 3: DivAux Path (DivAux > 1) */
		{
			int aux;
			for (aux = 2; aux <= 31; aux++) {
				struct soc_dp_pixel_pll_cfg curr = {0};
				u32 div_total = 2 * aux * pclk_div;
				u32 target_vco = target_pclk_khz * div_total;
				u8 pre;
				u16 fb;
				u32 frac;

				if (target_vco < SOC_DP_VCO_MIN_KHZ || target_vco > SOC_DP_VCO_MAX_KHZ) continue;

				if (soc_dp_solve_pll_frac(target_vco, ref_clk_khz, &pre, &fb, &frac) == 0) {
					u32 actual_pclk = soc_dp_get_rate_khz(pre, fb, frac, ref_clk_khz, div_total);

					if (soc_dp_abs_diff(actual_pclk, target_pclk_khz) > SOC_DP_PLL_ERR_TOLERANCE) continue;

					curr.valid = true;
					curr.vco_freq_khz = target_vco;
					curr.actual_pclk_khz = actual_pclk;
					curr.prediv = pre; curr.fbdiv = fb; curr.frac = frac;

					curr.frac_pd = (frac == 0) ? 3 : 0;

					curr.div5_en = 0;
					curr.divaux = aux;
					curr.divm = 0; /* Ignored when divaux != 1 */
					curr.divp = pclk_div;

					if (soc_dp_is_better_config(curr.valid, (curr.frac==0), curr.prediv, curr.vco_freq_khz,
								    best.valid, (best.frac==0), best.prediv, best.vco_freq_khz)) {
						best = curr;
					}
				}
			}
		}
	}

	if (!best.valid)
		return -EINVAL;

	*cfg = best;
	return 0;
}

static void soc_dp_calc_pixel_pll_to_reg(struct soc_dp_phy *phy, struct soc_dp_pixel_pll_cfg *cfg)
{
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_PREPLL_PD, 1);
	mdelay(2);

	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_PREPLL_PREDIV, cfg->prediv);

	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_PREPLL_FBDIV2_LBIT, cfg->fbdiv & 0xFF);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_PREPLL_FBDIV2_HBIT, (cfg->fbdiv >> 8) & 0xF);

	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_PREPLL_DACPD, (cfg->frac_pd >> 1) & 0x1);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_PREPLL_DSMPD, cfg->frac_pd & 0x1);

	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_PREPLL_FRAC2_LBIT, cfg->frac & 0xFF);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_PREPLL_FRAC2_MBIT, (cfg->frac >> 8) & 0xFF);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_PREPLL_FRAC2_HBIT, (cfg->frac >> 16) & 0xFF);

	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_PREPLL_PRECLK_DIVM, cfg->divm);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_PREPLL_PRECLK_DIVAUX, cfg->divaux);

	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_PREPLL_PCLKDIV5_EN, cfg->div5_en);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_PREPLL_PCLK_DIVAUX, cfg->divp);

	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_PREPLL_PD, 0);
	soc_dp_reg_write_range(phy, SOC_DPTX_REG_PCLK_OUTPUT_NORMAL, 1);
	mdelay(2);
}

static int soc_dp_check_pll_lock(struct soc_dp_phy *phy)
{
	u32 pll_locked;

	soc_dp_reg_read_range(phy, SOC_DPTX_AD_LOCK_PIXELPLL, &pll_locked);

	if (!pll_locked) {
		dev_err(phy->dev, "Pre_pll unlocked\n");
		return -EINVAL;
	}

	soc_dp_reg_read_range(phy, SOC_DPTX_AD_LOCK_COREPLL, &pll_locked);

	if (!pll_locked) {
		dev_err(phy->dev, "Post_pll unlocked\n");
		return -EINVAL;
	}

	return 0;
}

int soc_dp_phy_exit(struct soc_dp_phy *phy)
{
	soc_dp_reg_write_range(phy, SOC_DPTX_XMIT_ENABLE, 0);
	mdelay(2);

	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_MPLL_PD, 1);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_PREPLL_PD, 1);
	mdelay(2);

	return 0;
}

int soc_dp_phy_power_off(struct soc_dp_phy *phy)
{
	soc_dp_reg_write_range(phy, SOC_DPTX_XMIT_ENABLE, 0);
	mdelay(2);

	// soc_dp_reg_write_range(phy, SOC_DPTX_ANA_MPLL_PD, 1);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_PREPLL_PD, 1);
	mdelay(2);

	return 0;
}

int soc_dp_phy_power_on(struct soc_dp_phy *phy)
{
	int ret, retry;
	u32 lane_en;

	switch (phy->lane_count) {
	case SOC_DP_LANE_1:
		lane_en = 0x1; break;
	case SOC_DP_LANE_2:
		lane_en = 0x3; break;
	case SOC_DP_LANE_4:
	default:
		lane_en = 0xF; break;
	}

	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_MPLL_PD, 0);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_PREPLL_PD, 0);
	mdelay(2);

	soc_dp_reg_write_range(phy, SOC_DPTX_XMIT_ENABLE, lane_en);
	mdelay(2);

	for (retry = 0; retry < 3; retry++) {
		ret = soc_dp_check_pll_lock(phy);
		if (!ret)
			break;
		mdelay(2);
	}

	return 0;
}

static void soc_dp_phy_config_lanes(struct soc_dp_phy *phy, int lane_count)
{
	u32 phy_lanes_val;

	pr_debug("%s() lane_count %d \n", __func__, lane_count);

	switch (lane_count) {
	case SOC_DP_LANE_1:
		phy_lanes_val = 0; break;
	case SOC_DP_LANE_2:
		phy_lanes_val = 1; break;
	case SOC_DP_LANE_4:
	default:
		phy_lanes_val = 2; break;
	}

	dev_dbg(phy->dev, "Configuring PHY Lane Count: %d (Reg: %d)\n",
		 lane_count, phy_lanes_val);

	soc_dp_reg_write_range(phy, SOC_DPTX_PHY_NUM_LANES, phy_lanes_val);
	phy->lane_count = lane_count;
}

static int soc_dp_phy_config_rate(struct soc_dp_phy *phy, int rate_khz)
{
	u32 rate_val = 0;
	int ret;
	struct soc_dp_core_pll_cfg core_pll_cfg = {0};

	switch (rate_khz) {
	case SOC_DP_LINK_RATE_1_62:
		rate_val = 0; break;
	case SOC_DP_LINK_RATE_2_70:
		rate_val = 1; break;
	case SOC_DP_LINK_RATE_5_40:
		rate_val = 2; break;
	case SOC_DP_LINK_RATE_8_10:
	default:
		rate_val = 3; break;
	}

	soc_dp_reg_write_range(phy, SOC_DPTX_PHY_RATE, rate_val);

	/* Recalculate and set Core PLL */
	dev_dbg(phy->dev, "Setting Core PLL to Rate %d kHz\n", rate_khz);

	ret = soc_dp_calc_core_pll(rate_khz, phy->ref_clk_khz, &core_pll_cfg);
	if (ret) {
		dev_err(phy->dev, "Failed to calc core PLL\n");
		return ret;
	}

	soc_dp_calc_core_pll_to_reg(phy, &core_pll_cfg);
	phy->link_rate_khz = rate_khz;
	return 0;
}

static void soc_dp_phy_set_voltages(struct soc_dp_phy *phy,
					struct soc_dp_phy_configure_opts *opts)
{
	int i;
	int rate_idx;
	u32 swing, preemp;
	const struct soc_dp_phy_vol_setting *cfg;

	/* Determine Rate Index */
	switch (phy->link_rate_khz) {
	case SOC_DP_LINK_RATE_1_62:
		rate_idx = 0;
		break;
	case SOC_DP_LINK_RATE_2_70:
		rate_idx = 1;
		break;
	case SOC_DP_LINK_RATE_5_40:
		rate_idx = 2;
		break;
	case SOC_DP_LINK_RATE_8_10:
		rate_idx = 3;
		break;
	default:
		rate_idx = 0;
		break;
	}

	for (i = 0; i < phy->lane_count; i++) {
		swing = opts->voltage[i] & 0x3;
		preemp = opts->pre[i] & 0x3;

		/* Remap to our table indices */
		swing = phy_swing_map[swing];
		preemp = phy_preemp_map[preemp];

		/* Get configuration parameters */
		cfg = &vol_cfg_table[rate_idx][swing][preemp];

		/* Write to analog registers based on Lane ID */
		switch (i) {
		case 0:
			soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_ISEL_DRV_D0, cfg->isel);
			soc_dp_reg_write_range(phy, SOC_DPTX_DA_TX_MAINSEL_D0_4_0, cfg->mainsel);
			soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_POSTSEL_D0, cfg->postsel);
			soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_PRESEL_D0, cfg->presel);
			break;
		case 1:
			soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_ISEL_DRV_D1, cfg->isel);
			soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_MAINSEL_D1, cfg->mainsel);
			soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_POSTSEL_D1, cfg->postsel);
			soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_PRESEL_D1, cfg->presel);
			break;
		case 2:
			soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_ISEL_DRV_D2, cfg->isel);
			soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_MAINSEL_D2, cfg->mainsel);
			soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_POSTSEL_D2, cfg->postsel);
			soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_PRESEL_D2, cfg->presel);
			break;
		case 3:
			soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_ISEL_DRV_D3, cfg->isel);
			soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_MAINSEL_D3, cfg->mainsel);
			soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_POSTSEL_D3, cfg->postsel);
			soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_PRESEL_D3, cfg->presel);
			break;
		}
	}
}

int soc_dp_phy_configure(struct soc_dp_phy *phy,
			 struct soc_dp_phy_configure_opts *opts)
{
	int ret;

	if (opts->set_lanes)
		soc_dp_phy_config_lanes(phy, opts->lanes);

	if (opts->set_rate) {
		ret = soc_dp_phy_config_rate(phy, opts->link_rate * 1000);
		if (ret)
			return ret;
	}

	if (opts->set_voltages)
		soc_dp_phy_set_voltages(phy, opts);

	return 0;
}

int soc_dp_phy_set_pixel_clk(struct soc_dp_phy *phy, u32 pixel_clk_khz)
{
	struct soc_dp_pixel_pll_cfg pixel_pll_cfg = {0};
	int ret;

	dev_dbg(phy->dev, "Setting Pixel PLL to %d kHz\n", pixel_clk_khz);

	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_PREPLL_DP_EN, 1);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_PREPLL_HDMI_EN, 0);

	ret = soc_dp_calc_pixel_pll(pixel_clk_khz, phy->ref_clk_khz, &pixel_pll_cfg);
	if (ret)
		return ret;

	soc_dp_calc_pixel_pll_to_reg(phy, &pixel_pll_cfg);

	return 0;
}

int soc_dp_phy_init(struct soc_dp_phy *phy, uintptr_t base_addr,
			u32 ref_clk_khz)
{
	int ret;
	u32 clk_div;
	u32 m_isel = 0x5, m_mainsel = 0x19;
	u32 m_pre = 0x0, m_post = 0x2;
	u32 tx_mode = 0x1, tx_pre = 0x0;
	struct soc_dp_phy_configure_opts phy_opts;

	memset(phy, 0, sizeof(*phy));
	phy->regs = base_addr;
	phy->ref_clk_khz = ref_clk_khz;
	phy->power_count = 0;

	clk_div = ref_clk_khz / 100;

	soc_dp_phy_exit(phy);

	// Reset Controller and PHY (PHY specific parts)
	soc_dp_reg_write_range(phy, SOC_DPTX_PHY_RESET, 0x1);
	mdelay(5);
	soc_dp_reg_write_range(phy, SOC_DPTX_PHY_RESET, 0x0);
	mdelay(2);

	// Disable PHY SSC (Spread Spectrum Clocking)
	// soc_dp_reg_write_range(phy, SOC_DPTX_ANA_MPLL_DISABLE_SSCG, 0x1);

	// Bypass PHY busy state
	soc_dp_reg_write_range(phy, SOC_DPTX_PHY_BUSY_BYP, 0x1);

	// Enable Enhance Framing and Scale Down Mode
	soc_dp_reg_write_range(phy, SOC_DPTX_ENHANCE_FRAMING_EN, 0x1);

	memset(&phy_opts, 0, sizeof(phy_opts));
	phy_opts.lanes = SOC_DP_LANE_2;
	phy_opts.link_rate = SOC_DP_LINK_RATE_2_70 / 1000;
	phy_opts.set_lanes = 1;
	phy_opts.set_rate = 1;

	ret = soc_dp_phy_configure(phy, &phy_opts);
	if (ret)
		return ret;

	ret = soc_dp_phy_set_pixel_clk(phy, 148500);
	if (ret)
		return ret;

	ret = soc_dp_phy_power_on(phy);
	if (ret)
		return ret;

	// Analog initialization
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_MODE_D0, 0);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_MODE_D1, 0);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_MODE_D2, 0);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_MODE_D3, 0);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_RTCAL_FREQDIV_HBIT, (clk_div >> 8) & 0x7f);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_RTCAL_BYPASS, 1);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_RTCAL_FREQDIV_LBIT, clk_div & 0xff);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_BG_RCAL_SEL, 0);

	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_RTM_D3, 0);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_RTM_D2, 0);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_RTM_D1, 0);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_RTM_D0, 0);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_RTCAL_BYPASS, 1);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_RTCAL_BYPASS, 0);
	mdelay(100);

	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_MODE_PRE_D3, 1);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_MODE_PRE_D2, 1);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_MODE_PRE_D1, 1);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_MODE_PRE_D0, 1);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_MODE_DE_D3, 1);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_MODE_DE_D2, 1);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_MODE_DE_D1, 1);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_MODE_DE_D0, 1);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_POSTSEL_PRE_D3, tx_pre);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_POSTSEL_PRE_D2, tx_pre);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_POSTSEL_PRE_D1, tx_pre);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_POSTSEL_PRE_D0, tx_pre);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_ISEL_DRV_D3, m_isel);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_ISEL_DRV_D2, m_isel);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_MAINSEL_D2, m_mainsel);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_MAINSEL_D3, m_mainsel);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_ISEL_DRV_D1, m_isel);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_ISEL_DRV_D0, m_isel);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_POSTSEL_D1, m_post);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_POSTSEL_D0, m_post);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_POSTSEL_D3, m_post);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_POSTSEL_D2, m_post);
	soc_dp_reg_write_range(phy, SOC_DPTX_DA_TX_MAINSEL_D0_4_0, m_mainsel);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_MAINSEL_D1, m_mainsel);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_PRESEL_D1, m_pre);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_PRESEL_D0, m_pre);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_PRESEL_D3, m_pre);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_PRESEL_D2, m_pre);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_MODE_D3, tx_mode);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_MODE_D2, tx_mode);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_MODE_D1, tx_mode);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_MODE_D0, tx_mode);
	soc_dp_reg_write_range(phy, SOC_DPTX_ANA_TX_AUX_RX_VSEL, 0x0);

	return 0;
}
