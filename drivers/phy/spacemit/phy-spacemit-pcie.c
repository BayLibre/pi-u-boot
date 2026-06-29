// SPDX-License-Identifier: GPL-2.0
/*
 * phy-spacemit-pcie.c - Spacemit k3 PHY for PCIE
 *
 * Copyright (c) 2025 Spacemit Co., Ltd.
 *
 */

#include <clk.h>
#include <dm.h>
#include <asm/global_data.h>
#include <dm/device_compat.h>
#include <generic-phy.h>
#include <reset.h>
#include <syscon.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <linux/iopoll.h>

#define APB_SPARE31_REG_OFF	0
#define APB_SPARE32_REG_OFF	0x4

#define PCIE_REF_CLK_OUTPUT

struct spacemit_pcie_phy {
	struct udevice *dev;
	int	phy_id;
	int	num_lanes;
	void __iomem *phy_base;
	void __iomem *apb_spare;
};

static inline u32 phy_readl(void __iomem *base, u32 offset)
{
	return readl(base + offset);
}

static inline void phy_writel(void __iomem *base, u32 offset, u32 val)
{
	writel(val, base + offset);
}

static inline void phy_mod_bit(void __iomem *base, u32 offset, u32 mask, u32 set)
{
	u32 val = readl(base + offset);
	val &= ~mask;
	if (set)
		val |= mask;
	writel(val, base + offset);
}

static void init_x1_phy(void __iomem *phy_base)
{
	u32 rd_data;
	u32 rx_regs[7] = {0x10, 0x78, 0x98, 0xdf, 0xb4, 0x88, 0x28};
	int id, lsh;

	pr_info("Now int init_x1_puphy...\n");

#ifndef PCIE_100M_REF_CLK
	/* select 24Mhz refclock input pll_reg2[7:4]=2 */
	rd_data = phy_readl(phy_base, (0x16 << 2));
	rd_data &= 0xffff0fff;
	rd_data |= 0x00002000;
	phy_writel(phy_base, (0x16 << 2), rd_data);

	phy_mod_bit(phy_base, (0x17 << 2), (0x1 << 21), 0);
	phy_mod_bit(phy_base, (0x14 << 2), 0x3, 0);

#ifdef PCIE_REF_CLK_OUTPUT
	phy_mod_bit(phy_base, (0x17 << 2), (0x1 << 20), 1);
	phy_writel(phy_base, (0x14 << 2), 0x00006505);
#endif
#endif

	/* pll_reg1 of lane0, disable ssc pll_reg4[3:0]=4'h0 */
	rd_data = phy_readl(phy_base, (0x16 << 2));
	rd_data &= 0xf0ffffff;
	phy_writel(phy_base, (0x16 << 2), rd_data);

	phy_mod_bit(phy_base, (0x10 << 2), (0x1 << 13), 1);

	/* Force RCV Good / Dynamic Lock */
	/* cdr fix bypass */
	phy_mod_bit(phy_base, 0x4, (0x1 << 6), 0);
	/* dynamic lock */
	phy_mod_bit(phy_base, 0xC, (0x1 << 2), 1);

	/* rx_reg 0~3 */
	for (id = 0; id < 4; id++) {
		rd_data = phy_readl(phy_base, 0x60);
		lsh = id * 8;
		rd_data &= ~(0xff << lsh);
		rd_data |= (rx_regs[id] << lsh);
		phy_writel(phy_base, 0x60, rd_data);
	}

	/* rx_reg 4~6 */
	for (id = 4; id < 7; id++) {
		rd_data = phy_readl(phy_base, 0x64);
		lsh = (id - 4) * 8;
		rd_data &= ~(0xff << lsh);
		rd_data |= (rx_regs[id] << lsh);
		phy_writel(phy_base, 0x64, rd_data);
	}

	/* Set init done */
	/* cfg_sw_phy_init_done */
	phy_mod_bit(phy_base, (0x02 << 2), (0x1 << 11), 1);
}

static void init_x2_phy(void __iomem *phy_base)
{
	int i, id, lsh;
	u32 rd_data;
	u32 rx_regs[7] = {0x10, 0x78, 0x98, 0xdf, 0xb4, 0x88, 0x28};
	void __iomem *lane_base;

	pr_info("Now int init_x2_puphy...\n");

#ifndef PCIE_100M_REF_CLK
	/* select 24Mhz refclock input pll_reg2[7:4]=2 */
	rd_data = phy_readl(phy_base, (0x16 << 2));
	rd_data &= 0xffff0fff;
	rd_data |= 0x00002000;
	phy_writel(phy_base, (0x16 << 2), rd_data);

	phy_mod_bit(phy_base, (0x17 << 2), (0x1 << 21), 0);

	for (i = 0; i < 2; i++)
		phy_mod_bit(phy_base + (0x400 * i), (0x14 << 2), 0x3, 0);

#ifdef PCIE_REF_CLK_OUTPUT
	phy_mod_bit(phy_base, (0x17 << 2), (0x1 << 20), 1);
	phy_writel(phy_base, (0x14 << 2), 0x00006505);
#endif
#endif

	/* pll_reg1 of lane0, disable ssc pll_reg4[3:0]=4'h0 */
	rd_data = phy_readl(phy_base, (0x16 << 2));
	rd_data &= 0xf0ffffff;
	phy_writel(phy_base, (0x16 << 2), rd_data);

	for (i = 0; i < 2; i++) {
		lane_base = phy_base + (0x400 * i);
		phy_mod_bit(lane_base, (0x10 << 2), (0x1 << 13), 1);
		/* cdr fix bypass */
		phy_mod_bit(lane_base, 0x4, (0x1 << 6), 0);
		/* dynamic lock */
		phy_mod_bit(lane_base, 0xC, (0x1 << 2), 1);
	}

	for (i = 0; i < 2; i++) {
		lane_base = phy_base + (0x400 * i);

		/* rx_reg 0~3 */
		for (id = 0; id < 4; id++) {
			rd_data = phy_readl(lane_base, 0x60);
			lsh = id * 8;
			rd_data &= ~(0xff << lsh);
			rd_data |= (rx_regs[id] << lsh);
			phy_writel(lane_base, 0x60, rd_data);
		}

		/* rx_reg 4~6 */
		for (id = 4; id < 7; id++) {
			rd_data = phy_readl(lane_base, 0x64);
			lsh = (id - 4) * 8;
			rd_data &= ~(0xff << lsh);
			rd_data |= (rx_regs[id] << lsh);
			phy_writel(lane_base, 0x64, rd_data);
		}
	}

	/* Set init done */
	for (i = 0; i < 2; i++) {
		lane_base = phy_base + (0x400 * i);
		/* cfg_sw_phy_init_done */
		phy_mod_bit(lane_base, (0x02 << 2), (0x1 << 11), 1);
	}
}

static void wait_phy_pll_lock(void __iomem *phy_base)
{
	u32 rd_data = 0;
	pr_info("waiting pll lock...\n");
	if (readl_poll_timeout(phy_base + 0x8, rd_data, (rd_data & 0x1), 1000000))
		pr_debug("PHY PLL Lock Timeout! Status: 0x%x\n", rd_data);
	else
		pr_info("PHY PLL Locked.\n");
}

static int spacemit_pcie_phy_power_on(struct phy *phy)
{
	struct spacemit_pcie_phy *priv = dev_get_priv(phy->dev);
	u32 rd_data = 0;
	int timeout = 100;

	do {
		mdelay(10);
		rd_data = phy_readl(priv->apb_spare, APB_SPARE32_REG_OFF) & (0x1<<8);
		timeout --;
		if(timeout == 0)
			break;
	} while (!rd_data);

	if (!rd_data) {
		printf("rcal timeout, trim override\n");
		rd_data = phy_readl(priv->apb_spare, APB_SPARE32_REG_OFF);
		rd_data |= (0xa<<20 | 0x6<<24 | 0x7<<28);
		phy_writel(priv->apb_spare, APB_SPARE32_REG_OFF, rd_data);

		rd_data = phy_readl(priv->apb_spare, APB_SPARE32_REG_OFF);
		rd_data |= (0x1<<31);
		phy_writel(priv->apb_spare, APB_SPARE32_REG_OFF, rd_data);
	}

	if (priv->num_lanes == 1)
		init_x1_phy(priv->phy_base);
	else
		init_x2_phy(priv->phy_base);

	wait_phy_pll_lock(priv->phy_base);

	return 0;
}

static int spacemit_pcie_phy_init(struct phy *phy)
{
	struct spacemit_pcie_phy *priv = dev_get_priv(phy->dev);

	phy_mod_bit(priv->apb_spare, APB_SPARE31_REG_OFF, (0x1 << 17), 1);
	return 0;
}

static int spacemit_pcie_phy_probe(struct udevice *dev)
{
	struct spacemit_pcie_phy *priv = dev_get_priv(dev);

	priv->dev = dev;
	priv->phy_base = (void __iomem *)devfdt_get_addr_name(dev, "phy");
	priv->apb_spare = (void __iomem *)devfdt_get_addr_name(dev, "apb_spare");
	if (!priv->phy_base || !priv->apb_spare)
		return -EINVAL;

	priv->phy_id = dev_read_u32_default(dev, "spacemit,phy-id", 0);
	priv->num_lanes = dev_read_u32_default(dev, "spacemit,num-lanes", 1);
	if(priv->num_lanes != 1 && priv->num_lanes != 2)
		priv->num_lanes = 1;

	return 0;
}

static struct phy_ops spacemit_pcie_phy_ops = {
	.init = spacemit_pcie_phy_init,
	.power_on = spacemit_pcie_phy_power_on,
};

static const struct udevice_id spacemit_pcie_phy_ids[] = {
	{
		.compatible = "spacemit,k3-pcie-phy",
	},
	{ }
};

U_BOOT_DRIVER(spacemit_pcie_phy) = {
	.name	= "spacemit-pcie-phy",
	.id	= UCLASS_PHY,
	.of_match = spacemit_pcie_phy_ids,
	.ops = &spacemit_pcie_phy_ops,
	.probe = spacemit_pcie_phy_probe,
	.priv_auto	= sizeof(struct spacemit_pcie_phy),
};
