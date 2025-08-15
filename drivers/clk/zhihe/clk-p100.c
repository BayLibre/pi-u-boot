// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025 Zhihe Computing Limited.
 * Author: Dong Yan <yand@zhcomputing.com>
 */

#include <clk.h>
#include <clk-uclass.h>
#include <linux/clk-provider.h>
#include <dm.h>
#include <errno.h>
#include <log.h>
#include <reset.h>
#include <asm/io.h>
#include <dm/ofnode.h>
#include <dm/device_compat.h>
#include <dm/device-internal.h>
#include <linux/err.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <dt-bindings/clock/p100-clock.h>

struct p100_clk {
	void __iomem *pll_wrap_base, *top_crg_base, *cpu_ss_clk_sysreg_base, *cpu_ss_cpu_pll_base;
	void __iomem *ddr0_sysreg_base, *ddr1_sysreg_base,*slc_dual_sysreg_base, *top_crg_t_base;
	void __iomem *cpu_ss_c908_cpu_clk_ccu_base, *cpu_ss_c920_cpu_clk_ccu_base;
	void __iomem *peri0_sysreg_base, *peri1_sysreg_base;
	void __iomem *peri2_sysreg_base, *peri3_sysreg_base;
};

struct p100_pll_clk {
	char  *name;
	u32   id;
	unsigned long freq;
	struct clk p_clk;
};

static const char * const noc_cclk_mux_parents[] = {"dpu1_pll_foutvco", "video_pll_foutvco", "gmac_pll_foutvco"};
static const char * const top_cpusys_pic_clk_mux_parents[] = {"gmac_pll_foutvco", "video_pll_foutvco"};
static const char * const top_cpusys_bus_clk_mux_parents[] = {"video_pll_foutvco", "gmac_pll_foutvco",
	"audio0_pll_foutvco", "dpu2_pll_foutvco"};
static const char * const emmc_ref_clk_mux_parents[] = {"audio0_pll_foutvco", "video_pll_foutvco"};
static const char * const peri1_qspi_ssi_clk_parents[] = {"peri1_qspi_ssi_clk_div1", "peri1_qspi_ssi_clk_div0"};
static const char * const peri2_qspi_ssi_clk_parents[] = {"peri2_qspi_ssi_clk_div1", "peri2_qspi_ssi_clk_div0"};
static const char * const uart_sclk_parents[] = {"uart_sclk_100m", "aon_osc_clk_logic"};

struct p100_pll_clk p100_pll_clks[] = {
	{"audio0_pll_foutvco", AUDIO0_PLL_FOUTVCO,  2352000000UL, {0}},
	{"audio1_pll_foutvco", AUDIO1_PLL_FOUTVCO,  2520000000UL, {0}},
	{"video_pll_foutvco",  VIDEO_PLL_FOUTVCO,   2640000000UL, {0}},
	{"gmac_pll_foutvco",   GMAC_PLL_FOUTVCO,    3000000000UL, {0}},
	{"dpu1_pll_foutvco",   DPU1_PLL_FOUTVCO,    2376000000UL, {0}},
	{"dpu2_pll_foutvco",   DPU2_PLL_FOUTVCO,    2376000000UL, {0}},
	{"c908_pll_foutvco",   C908_PLL_FOUTVCO,    1200000000UL, {0}},
	{"c920_pll_foutvco",   C920_PLL_FOUTVCO,    2040000000UL, {0}},
};

static struct clk *p100_clk_fixed_rate(const char *name, ulong rate)
{
	return clk_register_fixed_rate(NULL, name, rate);
}

static struct clk *p100_clk_fixed_factor(const char *name, const char *parent,
				unsigned int div)
{
	return clk_register_fixed_factor(NULL, name, parent, CLK_SET_RATE_PARENT, 1, div);
}

static struct clk *p100_clk_divider(const char *name, const char *parent,
		void __iomem *reg, u8 shift, u8 width)
{
	return clk_register_divider(NULL, name, parent, CLK_SET_RATE_PARENT, reg,
				shift, width, CLK_DIVIDER_ONE_BASED);
}

static struct clk *p100_clk_divider_zero_based(const char *name, const char *parent,
		void __iomem *reg, u8 shift, u8 width)
{
	return clk_register_divider(NULL, name, parent, CLK_SET_RATE_PARENT, reg,
				shift, width, CLK_DIVIDER_ALLOW_ZERO);
}

// static struct clk *p100_clk_divider_closest(const char *name, const char *parent,
// 		void __iomem *reg, u8 shift, u8 width)
// {
// 	return clk_register_divider(NULL, name, parent, CLK_SET_RATE_PARENT, reg,
// 				shift, width, CLK_DIVIDER_ONE_BASED | CLK_DIVIDER_ROUND_CLOSEST);
// }

static struct clk *p100_clk_gate(const char *name, const char *parent,
		void __iomem *reg, u8 shift)
{
	return clk_register_gate(NULL, name, parent, CLK_SET_RATE_PARENT, reg,
				shift, 0, NULL);
}

static struct clk *p100_clk_mux(const char *name, const char * const *parent_names, u8 num_parents,
		void __iomem *reg, u8 shift, u8 width)
{
	return clk_register_mux(NULL, name, parent_names, num_parents, CLK_SET_RATE_PARENT,
				reg, shift, width, 0);
}

static void p100_register_clock(struct udevice *dev)
{
	struct p100_clk *priv = dev_get_priv(dev);
	ofnode node = dev_ofnode(dev);
	unsigned int pll_rate;
	char pll_name[100];
	int i;

	if (!ofnode_valid(node)) {
		return;
	}

	/* Clock source */
	clk_dm(OSC_24M,
		p100_clk_fixed_rate("osc_24m", 24000000UL));

	/* PLL clocks */
	for (i = 0; i < ARRAY_SIZE(p100_pll_clks); i++) {
		sprintf(pll_name, "%s_%s", p100_pll_clks[i].name, "frequency");
		if (!ofnode_read_u32(node, pll_name, &pll_rate) && (pll_rate > 0)) {
			clk_dm(p100_pll_clks[i].id,	p100_clk_fixed_rate(p100_pll_clks[i].name, pll_rate));
		} else {
			// use default pll frequency
			clk_dm(p100_pll_clks[i].id,	p100_clk_fixed_rate(p100_pll_clks[i].name, p100_pll_clks[i].freq));
		}
	}

	/* Fixed clk */
	clk_dm(AON_OSC_CLK_LOGIC,
			p100_clk_fixed_factor("aon_osc_clk_logic", "osc_24m", 1));
	clk_dm(VIDEO_PLL_FOUT1PH0,
			p100_clk_fixed_factor("video_pll_fout1ph0", "video_pll_foutvco", 4));
	clk_dm(GMAC_PLL_FOUTPOSTDIV,
			p100_clk_fixed_factor("gmac_pll_foutpostdiv", "gmac_pll_foutvco", 3));

	/* TOP SS */
	clk_dm(TOP_CFG_ACLK_DIV,
		p100_clk_divider("top_cfg_aclk", "video_pll_fout1ph0", priv->top_crg_base, 8, 8));
	clk_dm(TOP_PCLK_DIV,
		p100_clk_divider("top_pclk", "video_pll_fout1ph0", priv->top_crg_base, 0, 8));
	clk_dm(AMUX_CLK_DIV,
		p100_clk_divider("top_amux_clk_div", "video_pll_foutvco", priv->top_crg_base + 0x18, 8, 4));

	/* TOP SS - IOMMU_PTW_ACLK */
	clk_dm(IOMMU_PTW_ACLK_DIV,
		p100_clk_divider("iommu_ptw_aclk_div", "video_pll_fout1ph0", priv->top_crg_base, 24, 3));

	/* TOP SS - CPU SS */
	clk_dm(TOP_CPUSYS_BUS_CLK_MUX,
		p100_clk_mux("top_cpusys_bus_clk_mux", top_cpusys_bus_clk_mux_parents, ARRAY_SIZE(top_cpusys_bus_clk_mux_parents), priv->top_crg_base + 0x8, 16, 2));
	clk_dm(TOP_CPUSYS_BUS_CLK_DIV,
		p100_clk_divider("top_cpusys_bus_clk", "top_cpusys_bus_clk_mux", priv->top_crg_base + 0x8, 20, 4));
	clk_dm(TOP_CPUSYS_PIC_CLK_MUX,
		p100_clk_mux("top_cpusys_pic_clk_mux", top_cpusys_pic_clk_mux_parents, ARRAY_SIZE(top_cpusys_pic_clk_mux_parents), priv->top_crg_base + 0x4, 3, 1));
	clk_dm(TOP_CPUSYS_PIC_CLK_DIV,
		p100_clk_divider("top_cpusys_pic_clk", "top_cpusys_pic_clk_mux", priv->top_crg_base + 0x4, 20, 4));

	/* TOP SS - NOC_CCLK */
	clk_dm(NOC_CCLK_MUX,
		p100_clk_mux("noc_cclk_mux", noc_cclk_mux_parents, ARRAY_SIZE(noc_cclk_mux_parents), priv->top_crg_base, 16, 2));
	clk_dm(NOC_CCLK_DIV,
		p100_clk_divider("noc_cclk_div", "noc_cclk_mux", priv->top_crg_base, 20, 4));
	/* TOP SS - PERI SS */
	clk_dm(TOP_PERI_SPI_SSI_CLK0_DIV,
		p100_clk_divider("peri1_spi_ssi_clk", "audio1_pll_foutvco", priv->top_crg_base + 0x20, 20, 8));
	clk_dm(TOP_PERI_MST_ACLK0_DIV,
		p100_clk_divider("peri1_mst_aclk", "video_pll_fout1ph0", priv->top_crg_base + 0x20, 16, 3));
	clk_dm(TOP_PERI_HIRES_CLK0_DIV,
		p100_clk_divider("peri1_hires_clk", "video_pll_foutvco", priv->top_crg_base + 0x24, 16, 6));
	clk_dm(TOP_PERI_SPI_SSI_CLK1_DIV,
		p100_clk_divider("peri2_spi_ssi_clk", "audio1_pll_foutvco", priv->top_crg_base + 0x24, 24, 8));
	clk_dm(TOP_PERI_HIRES_CLK1_DIV,
		p100_clk_divider("peri2_hires_clk", "video_pll_foutvco", priv->top_crg_base + 0x24, 0, 6));
	clk_dm(TOP_PERI_MST_CLK1_DIV,
		p100_clk_divider("peri3_mst_aclk", "video_pll_foutvco", priv->top_crg_base + 0x28, 4, 4));
	clk_dm(TOP_PERI_EMMC_REF_CLK_MUX,
		p100_clk_mux("emmc_ref_clk_mux", emmc_ref_clk_mux_parents, ARRAY_SIZE(emmc_ref_clk_mux_parents), priv->top_crg_base + 0x2c, 20, 1));
	clk_dm(TOP_PERI_EMMC_REF_CLK_DIV,
		p100_clk_divider("emmc_ref_clk", "emmc_ref_clk_mux", priv->top_crg_base + 0x28, 0, 4));
	clk_dm(TOP_PERI_QSPI0_SSI_CLK_DIV0,
		p100_clk_divider("peri1_qspi_ssi_clk_div0", "video_pll_foutvco", priv->top_crg_base + 0x20, 8, 4));
	clk_dm(TOP_PERI_QSPI0_SSI_CLK_DIV1,
		p100_clk_divider("peri1_qspi_ssi_clk_div1", "audio1_pll_foutvco", priv->top_crg_base + 0x20, 0, 8));
	clk_dm(TOP_PERI_QSPI_SSI_CLK_MUX0,
		p100_clk_mux("peri1_qspi_ssi_clk", peri1_qspi_ssi_clk_parents, ARRAY_SIZE(peri1_qspi_ssi_clk_parents), priv->top_crg_base + 0x20, 12, 1));
	clk_dm(TOP_PERI_QSPI1_SSI_CLK_DIV0,
		p100_clk_divider("peri2_qspi_ssi_clk_div0", "video_pll_foutvco", priv->top_crg_base + 0x28, 16, 4));
	clk_dm(TOP_PERI_QSPI1_SSI_CLK_DIV1,
		p100_clk_divider("peri2_qspi_ssi_clk_div1", "audio1_pll_foutvco", priv->top_crg_base + 0x28, 8, 8));
	clk_dm(TOP_PERI_QSPI_SSI_CLK_MUX1,
		p100_clk_mux("peri2_qspi_ssi_clk", peri2_qspi_ssi_clk_parents, ARRAY_SIZE(peri2_qspi_ssi_clk_parents), priv->top_crg_base + 0x28, 20, 1));
	clk_dm(UART_SCLK_100M,
		p100_clk_fixed_factor("uart_sclk_100m", "gmac_pll_foutpostdiv", 10));
	clk_dm(TOP_UART_SCLK_MUX,
		p100_clk_mux("uart_sclk", uart_sclk_parents, ARRAY_SIZE(uart_sclk_parents), priv->top_crg_base + 0x28, 24, 1));
	clk_dm(I2C_IC_CLK,
		p100_clk_fixed_factor("i2c_ic_clk", "gmac_pll_foutpostdiv", 10));

	/* CPU SS */
	clk_dm(C908_CPU_CLK_CCU_RATIO_NORMAL,
		p100_clk_divider_zero_based("c908_cpu_clk", "c908_pll_foutvco", priv->cpu_ss_c908_cpu_clk_ccu_base + 0x1c, 0, 16));
	clk_dm(C920_CPU_CLK_CCU_RATIO_NORMAL,
		p100_clk_divider_zero_based("c920_cpu_clk", "c920_pll_foutvco", priv->cpu_ss_c920_cpu_clk_ccu_base + 0x1c, 0, 16));

	/* PERI0 SS */
	clk_dm(PERI0_WDT0_PCLK_EN,
		p100_clk_gate("peri0_wdt0_pclk", "top_cfg_aclk", priv->peri0_sysreg_base + 0x200, 4)),

	/* PERI1 SS */
	clk_dm(PERI1_GMAC0_ACLK_EN,
		p100_clk_gate("peri1_gmac0_aclk", "peri1_mst_aclk", priv->peri1_sysreg_base + 0x200, 0));
	clk_dm(PERI1_GMAC0_HCLK_EN,
		p100_clk_gate("peri1_gmac0_hclk", "top_cfg_aclk", priv->peri1_sysreg_base + 0x200, 1));
	clk_dm(PERI1_GMAC1_ACLK_EN,
		p100_clk_gate("peri1_gmac1_aclk", "peri1_mst_aclk", priv->peri1_sysreg_base + 0x200, 2));
	clk_dm(PERI1_GMAC1_HCLK_EN,
		p100_clk_gate("peri1_gmac1_hclk", "top_cfg_aclk", priv->peri1_sysreg_base + 0x200, 3));
	clk_dm(PERI1_GPIO0_PCLK_EN,
		p100_clk_gate("peri1_clkgen_gpio0_pclk", "top_cfg_aclk", priv->peri1_sysreg_base + 0x200, 5));
	clk_dm(PERI1_GPIO1_PCLK_EN,
		p100_clk_gate("peri1_clkgen_gpio1_pclk", "top_cfg_aclk", priv->peri1_sysreg_base + 0x200, 7));
	clk_dm(PERI1_I2C0_IC_CLK_EN,
		p100_clk_gate("peri1_clkgen_i2c0_ic_clk", "i2c_ic_clk", priv->peri1_sysreg_base + 0x200, 8));
	clk_dm(PERI1_I2C1_IC_CLK_EN,
		p100_clk_gate("peri1_clkgen_i2c1_ic_clk", "i2c_ic_clk", priv->peri1_sysreg_base + 0x200, 10));
	clk_dm(PERI1_I2C2_IC_CLK_EN,
		p100_clk_gate("peri1_clkgen_i2c2_ic_clk", "i2c_ic_clk", priv->peri1_sysreg_base + 0x200, 12));
	clk_dm(PERI1_SPI0_SSI_CLK_EN,
		p100_clk_gate("peri1_clkgen_spi0_ssi_clk", "peri1_spi_ssi_clk", priv->peri1_sysreg_base + 0x200, 23));
	clk_dm(PERI1_QSPI0_SSI_CLK_EN,
		p100_clk_gate("peri1_clkgen_qspi0_ssi_clk", "peri1_qspi_ssi_clk", priv->peri1_sysreg_base + 0x200, 21));

	/* PERI2 SS */
	clk_dm(PERI2_SPI1_SSI_CLK_EN,
		p100_clk_gate("peri2_clkgen_spi1_ssi_clk", "peri2_spi_ssi_clk", priv->peri2_sysreg_base + 0x200, 3));
	clk_dm(PERI2_UART4_SCLK_EN,
		p100_clk_gate("peri2_clkgen_uart4_sclk", "uart_sclk", priv->peri2_sysreg_base + 0x200, 4));
	clk_dm(PERI2_GPIO2_PCLK_EN,
		p100_clk_gate("peri2_clkgen_gpio2_pclk", "top_cfg_aclk", priv->peri2_sysreg_base + 0x200, 8));
	clk_dm(PERI2_QSPI1_SSI_CLK_EN,
		p100_clk_gate("peri2_clkgen_qspi1_ssi_clk", "peri2_qspi_ssi_clk", priv->peri2_sysreg_base + 0x204, 10));
	clk_dm(PERI2_GPIO3_PCLK_EN,
		p100_clk_gate("peri2_clkgen_gpio3_pclk", "top_cfg_aclk", priv->peri2_sysreg_base + 0x200, 14));
	clk_dm(PERI2_I2C3_IC_CLK_EN,
		p100_clk_gate("peri2_clkgen_i2c3_ic_clk", "i2c_ic_clk", priv->peri2_sysreg_base + 0x200, 17));
	clk_dm(PERI2_I2C4_IC_CLK_EN,
		p100_clk_gate("peri2_clkgen_i2c4_ic_clk", "i2c_ic_clk", priv->peri2_sysreg_base + 0x200, 19));
	clk_dm(PERI2_I2C5_IC_CLK_EN,
		p100_clk_gate("peri2_clkgen_i2c5_ic_clk", "i2c_ic_clk", priv->peri2_sysreg_base + 0x200, 21));
	clk_dm(PERI2_I2C6_IC_CLK_EN,
		p100_clk_gate("peri2_clkgen_i2c6_ic_clk", "i2c_ic_clk", priv->peri2_sysreg_base + 0x200, 23));
	clk_dm(PERI2_I2C7_IC_CLK_EN,
		p100_clk_gate("peri2_clkgen_i2c7_ic_clk", "i2c_ic_clk", priv->peri2_sysreg_base + 0x200, 25));

	/* PERI3 SS */
	clk_dm(PERI3_EMMC_SDIO_REF_CLK,
		p100_clk_fixed_factor("peri3_emmc_sdio_ref_clk", "emmc_ref_clk", 4));	/* Note: base clk is div 4 to 196M*/
	clk_dm(PERI3_EMMC_SDIO_REF_CLK_CG_EN,
		p100_clk_gate("peri3_clkgen_sdio_ref_clk", "peri3_emmc_sdio_ref_clk", priv->peri3_sysreg_base + 0x200, 2));
	clk_dm(PERI3_GPIO4_PCLK_EN,
		p100_clk_gate("peri3_clkgen_gpio4_pclk", "top_cfg_aclk", priv->peri3_sysreg_base + 0x200, 20));
}

static int p100_parse_regbase(struct udevice *dev)
{
	struct p100_clk *priv = dev_get_priv(dev);
	int ret = 0;

	priv->pll_wrap_base = (void __iomem *)dev_read_addr_name_ptr(dev, "PLL_WRAP");
	if (IS_ERR(priv->pll_wrap_base)) {
		ret = PTR_ERR(priv->pll_wrap_base);
		return ret;
	}
	priv->top_crg_base = (void __iomem *)dev_read_addr_name_ptr(dev, "TOP_CRG");
	if (IS_ERR(priv->top_crg_base)) {
		ret = PTR_ERR(priv->top_crg_base);
		return ret;
	}
	priv->top_crg_t_base = (void __iomem *)dev_read_addr_name_ptr(dev, "TOP_CRG_T");
	if (IS_ERR(priv->top_crg_t_base)) {
		ret = PTR_ERR(priv->top_crg_t_base);
		return ret;
	}
	priv->cpu_ss_clk_sysreg_base = (void __iomem *)dev_read_addr_name_ptr(dev, "CPU_SS_CLK_SYSREG");
	if (IS_ERR(priv->cpu_ss_clk_sysreg_base)) {
		ret = PTR_ERR(priv->cpu_ss_clk_sysreg_base);
		return ret;
	}
	priv->cpu_ss_cpu_pll_base = (void __iomem *)dev_read_addr_name_ptr(dev, "CPU_SS_CPU_PLL");
	if (IS_ERR(priv->cpu_ss_cpu_pll_base)) {
		ret = PTR_ERR(priv->cpu_ss_cpu_pll_base);
		return ret;
	}
	priv->ddr0_sysreg_base = (void __iomem *)dev_read_addr_name_ptr(dev, "DDR0_SYSREG");
	if (IS_ERR(priv->ddr0_sysreg_base)) {
		ret = PTR_ERR(priv->ddr0_sysreg_base);
		return ret;
	}
	priv->ddr1_sysreg_base = (void __iomem *)dev_read_addr_name_ptr(dev, "DDR1_SYSREG");
	if (IS_ERR(priv->ddr1_sysreg_base)) {
		ret = PTR_ERR(priv->ddr1_sysreg_base);
		return ret;
	}
	priv->slc_dual_sysreg_base = (void __iomem *)dev_read_addr_name_ptr(dev, "SLC_DUAL_SYSREG");
	if (IS_ERR(priv->slc_dual_sysreg_base)) {
		ret = PTR_ERR(priv->slc_dual_sysreg_base);
		return ret;
	}
	priv->cpu_ss_c908_cpu_clk_ccu_base = (void __iomem *)dev_read_addr_name_ptr(dev, "CPU_SS_C908_CPU_CLK_CCU");
	if (IS_ERR(priv->cpu_ss_c908_cpu_clk_ccu_base)) {
		ret = PTR_ERR(priv->cpu_ss_c908_cpu_clk_ccu_base);
		return ret;
	}
	priv->cpu_ss_c920_cpu_clk_ccu_base = (void __iomem *)dev_read_addr_name_ptr(dev, "CPU_SS_C920_CPU_CLK_CCU");
	if (IS_ERR(priv->cpu_ss_c920_cpu_clk_ccu_base)) {
		ret = PTR_ERR(priv->cpu_ss_c920_cpu_clk_ccu_base);
		return ret;
	}
	priv->peri0_sysreg_base = (void __iomem *)dev_read_addr_name_ptr(dev, "PERI0_SYSREG");
	if (IS_ERR(priv->peri0_sysreg_base)) {
		ret = PTR_ERR(priv->peri0_sysreg_base);
		return ret;
	}
	priv->peri1_sysreg_base = (void __iomem *)dev_read_addr_name_ptr(dev, "PERI1_SYSREG");
	if (IS_ERR(priv->peri1_sysreg_base)) {
		ret = PTR_ERR(priv->peri1_sysreg_base);
		return ret;
	}
	priv->peri2_sysreg_base = (void __iomem *)dev_read_addr_name_ptr(dev, "PERI2_SYSREG");
	if (IS_ERR(priv->peri2_sysreg_base)) {
		ret = PTR_ERR(priv->peri2_sysreg_base);
		return ret;
	}
	priv->peri3_sysreg_base = (void __iomem *)dev_read_addr_name_ptr(dev, "PERI3_SYSREG");
	if (IS_ERR(priv->peri3_sysreg_base)) {
		ret = PTR_ERR(priv->peri3_sysreg_base);
		return ret;
	}

	return ret;
}

static int p100_clk_probe(struct udevice *dev)
{
	int ret;

	ret = p100_parse_regbase(dev);
	if (ret) {
		dev_err(dev, "fail to parse reg base");
		return ret;
	}

	p100_register_clock(dev);

	return 0;
}

static const struct udevice_id p100_clk_ids[] = {
#ifdef CONFIG_CLK_ZHIHE_P100
	{ .compatible = "zhihe,p100-clk"},
#endif
	{ }
};


U_BOOT_DRIVER(zhihe_p100_clk) = {
	.name		= "zhihe_p100_clk",
	.id		= UCLASS_CLK,
	.of_match	= p100_clk_ids,
	.ops		= &ccf_clk_ops,
	.probe		= p100_clk_probe,
	.priv_auto	= sizeof(struct p100_clk),
	.flags = DM_FLAG_PRE_RELOC,
};
