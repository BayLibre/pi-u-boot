// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2024-2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

//#define DEBUG
#include <asm/cache.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <clk.h>
#include <cpu_func.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <errno.h>
#include <eth_phy.h>
#include <log.h>
#include <malloc.h>
#include <memalign.h>
#include <miiphy.h>
#include <net.h>
#include <netdev.h>
#include <phy.h>
#include <regmap.h>
#include <reset.h>
#include <syscon.h>
#include <wait_bit.h>
#include <linux/bitfield.h>
#include <linux/delay.h>

#include "dwc_eth_qos.h"

/* SYSCFG registers */
#define GMAC_SYSREG_OFS  0xF000
#define GMAC_CLKCTRL     (GMAC_SYSREG_OFS + 0x00)
#define GMAC_CTRL        (GMAC_SYSREG_OFS + 0x04)

/* SYSCFG value */
#define GMAC_CLKTRL_EN_ALL        0x1F
#define GMAC_CLKCTRL_SPEED_MASK  (0x3 << 8)
#define GMAC_CLKCTRL_1G          (0x0 << 8)
#define GMAC_CLKCTRL_10M         (0x2 << 8)
#define GMAC_CLKCTRL_100M        (0x3 << 8)

#define GMAC_CTRL_INTF_MASK 0xF
#define GMAC_CTRL_RGMII     0x1
#define GMAC_CTRL_RMII      0x4

static ulong eqos_get_tick_clk_rate_zhihe(struct udevice *dev)
{
	struct eqos_priv __maybe_unused *eqos = dev_get_priv(dev);

	if (!CONFIG_IS_ENABLED(CLK))
		return 0;

	return clk_get_rate(&eqos->clk_master_bus);
}

static int eqos_start_clks_zhihe(struct udevice *dev)
{
	struct eqos_priv __maybe_unused *eqos = dev_get_priv(dev);
	int ret;

	if (!CONFIG_IS_ENABLED(CLK))
		return 0;

	dev_dbg(dev, "%s:\n", __func__);

	ret = clk_enable(&eqos->clk_master_bus);
	if (ret < 0) {
		dev_err(dev, "clk_enable(clk_master_bus) failed: %d\n", ret);
		goto err;
	}

	ret = clk_enable(&eqos->clk_ck);
	if (ret < 0) {
		dev_err(dev, "clk_enable(clk_ck) failed: %d\n", ret);
		goto err_disable_clk_master_bus;
	}

	// enable gmac clk
	writel(GMAC_CLKTRL_EN_ALL, (void __iomem *)(eqos->regs + GMAC_CLKCTRL));

	dev_dbg(dev, "%s: OK\n", __func__);

	return 0;

err_disable_clk_master_bus:
	clk_disable(&eqos->clk_master_bus);
err:
	dev_dbg(dev, "%s: FAILED: %d\n", __func__, ret);

	return ret;
}

static int eqos_stop_clks_zhihe(struct udevice *dev)
{
	struct eqos_priv __maybe_unused *eqos = dev_get_priv(dev);

	if (!CONFIG_IS_ENABLED(CLK))
		return 0;

	dev_dbg(dev, "%s:\n", __func__);

	clk_disable(&eqos->clk_ck);
	clk_disable(&eqos->clk_master_bus);

	dev_dbg(dev, "%s: OK\n", __func__);

	return 0;
}

static int parse_speed_from_device_tree(const void *fdt, int node_offset)
{
	int speed;
	const __be32 *prop;

	prop = fdt_getprop(fdt, node_offset, "speed", NULL);
	if (!prop) {
		pr_err("Error: 'speed' property not found\n");
		return -1;
	}

	speed = fdt32_to_cpu(*prop);

	return speed;
}

static int eqos_set_tx_clk_speed_zhihe(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);

	int speed = eqos->phy->speed;
	unsigned int reg = 0;
	phy_interface_t interface = eqos->config->interface(dev);

	dev_dbg(dev, "%s: speed %d, phyif %d\n", __func__, speed, interface);

	reg = readl((void __iomem*)(eqos->regs + GMAC_CLKCTRL));
	switch(speed) {
	case SPEED_10:
		reg &= ~GMAC_CLKCTRL_SPEED_MASK;
		reg |= GMAC_CLKCTRL_10M;
		break;
	case SPEED_100:
		reg &= ~GMAC_CLKCTRL_SPEED_MASK;
		reg |= GMAC_CLKCTRL_100M;
		break;
	case SPEED_1000:
		reg &= ~GMAC_CLKCTRL_SPEED_MASK;
		reg |= GMAC_CLKCTRL_1G;
		break;
	default:
		dev_err(dev, "unsupported speed: %d\n", speed);
		return -EINVAL;
	}
	writel(reg, (void __iomem*)(eqos->regs + GMAC_CLKCTRL));

	/* Configure phy interface */
	reg = readl((void __iomem*)(eqos->regs + GMAC_CTRL));
	switch(interface) {
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
		reg &= ~GMAC_CTRL_INTF_MASK;
		reg |= GMAC_CTRL_RGMII;
		break;
	case PHY_INTERFACE_MODE_RMII:
		reg &= ~GMAC_CTRL_INTF_MASK;
		reg |= GMAC_CTRL_RMII;
		break;
	default:
		dev_err(dev, "unsupported phy interface: %d\n", interface);
		return -EINVAL;
	}
	writel(reg, (void __iomem*)(eqos->regs + GMAC_CTRL));

	return 0;
}

static int eqos_probe_resources_zhihe(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	int ret;

	ret = clk_get_by_name(dev, "master_bus", &eqos->clk_master_bus);
	if (ret) {
		dev_err(dev, "clk_get_by_name(master_bus) failed: %d\n", ret);
		goto err_probe;
	}

	ret = clk_get_by_name(dev, "aclk", &eqos->clk_ck);
	if (ret) {
		dev_err(dev, "clk_get_by_name(aclk) failed: %d\n", ret);
		goto err_probe;
	}

	/* Get reset gpio pin (optional) */
	ret = gpio_request_by_name(dev, "phy-reset-gpios", 0,
				   &eqos->phy_reset_gpio, GPIOD_IS_OUT);
	if (ret)
		pr_warn("No phy reset gpio provided: %d\n", ret);

	dev_dbg(dev, "%s: OK\n", __func__);

	return 0;

err_probe:

	dev_dbg(dev, "%s: returns %d\n", __func__, ret);

	return ret;
}

static int eqos_start_resets_zhihe(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);

	debug("%s(dev=%p):\n", __func__, dev);

	if (dm_gpio_is_valid(&eqos->phy_reset_gpio)) {
		dm_gpio_set_value(&eqos->phy_reset_gpio, 1);
		udelay(2);
		dm_gpio_set_value(&eqos->phy_reset_gpio, 0);
	}

	return 0;
}

static int eqos_remove_resources_zhihe(struct udevice *dev)
{
	dev_dbg(dev, "%s:\n", __func__);

	return 0;
}

static struct eqos_ops eqos_zhihe_ops = {
	.eqos_inval_desc = eqos_inval_desc_generic,
	.eqos_flush_desc = eqos_flush_desc_generic,
	.eqos_inval_buffer = eqos_inval_buffer_generic,
	.eqos_flush_buffer = eqos_flush_buffer_generic,
	.eqos_probe_resources = eqos_probe_resources_zhihe,
	.eqos_remove_resources = eqos_remove_resources_zhihe,
	.eqos_stop_resets = eqos_null_ops,
	.eqos_start_resets = eqos_start_resets_zhihe,
	.eqos_stop_clks = eqos_stop_clks_zhihe,
	.eqos_start_clks = eqos_start_clks_zhihe,
	.eqos_calibrate_pads = eqos_null_ops,
	.eqos_disable_calibration = eqos_null_ops,
	.eqos_set_tx_clk_speed = eqos_set_tx_clk_speed_zhihe,
	.eqos_get_enetaddr = eqos_null_ops,
	.eqos_get_tick_clk_rate = eqos_get_tick_clk_rate_zhihe
};

struct eqos_config __maybe_unused eqos_zhihe_a2xx_config = {
	.reg_access_always_ok = false,
	.mdio_wait = 10000,
	.swr_wait = 50,
	.config_mac = EQOS_MAC_RXQ_CTRL0_RXQ0EN_ENABLED_AV,
	.config_mac_mdio = EQOS_MAC_MDIO_ADDRESS_CR_250_300,
	.axi_bus_width = EQOS_AXI_WIDTH_64,
	.interface = dev_read_phy_mode,
	.ops = &eqos_zhihe_ops
};
