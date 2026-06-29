// SPDX-License-Identifier: GPL-2.0
/*
 * spacemit k1x/k3 gpio driver
 *
 * Copyright (C) 2025 Spacemit
 *
 */

#include <asm/io.h>
#include <errno.h>
#include <asm/gpio.h>
#include <linux/bitops.h>
#include <clk.h>
#include "k1x_gpio.h"

#include <dm/read.h>
#include <dm/device.h>
#include <dm/device_compat.h>
#include <dm/devres.h>
#include <dm/pinctrl.h>

static void __iomem *k1x_gpio_base;

#ifndef K1X_MAX_GPIO
#define K1X_MAX_GPIO	128
#endif

#define GPIO_TO_REG(gp)		(gp >> 5)
#define GPIO_TO_BIT(gp)		(1 << (gp & 0x1f))
#define GPIO_VAL(gp, val)	((val >> (gp & 0x1f)) & 0x01)

/* K1 register offsets */
static const struct spacemit_gpio_reg_offsets k1_regs = {
	.gplr    = 0x0,
	.gpdr    = 0xc,
	.gpsr    = 0x18,
	.gpcr    = 0x24,
	.grer    = 0x30,
	.gfer    = 0x3c,
	.gedr    = 0x48,
	.gsdr    = 0x54,
	.gcdr    = 0x60,
	.gsrer   = 0x6c,
	.gcrer   = 0x78,
	.gsfer   = 0x84,
	.gcfer   = 0x90,
	.gapmask = 0x9c,
	.gcpmask = 0xA8,
};

/* K3 register offsets */
static const struct spacemit_gpio_reg_offsets k3_regs = {
	.gplr    = 0x0,
	.gpdr    = 0x4,
	.gpsr    = 0x8,
	.gpcr    = 0xc,
	.grer    = 0x10,
	.gfer    = 0x14,
	.gedr    = 0x18,
	.gsdr    = 0x1c,
	.gcdr    = 0x20,
	.gsrer   = 0x24,
	.gcrer   = 0x28,
	.gsfer   = 0x2c,
	.gcfer   = 0x30,
	.gapmask = 0x34,
	.gcpmask = 0x38,
};

/* K1 bank offsets */
static const unsigned long k1_bank_offsets[] = {0x0, 0x4, 0x8, 0x100};

/* K3 bank offsets */
static const unsigned long k3_bank_offsets[] = {0x0, 0x40, 0x80, 0x100};

static const struct spacemit_gpio_chip_data k1_chip_data = {
	.regs = &k1_regs,
	.bank_offsets = k1_bank_offsets,
	.num_banks = ARRAY_SIZE(k1_bank_offsets),
	.max_gpio = 128,
};

static const struct spacemit_gpio_chip_data k3_chip_data = {
	.regs = &k3_regs,
	.bank_offsets = k3_bank_offsets,
	.num_banks = ARRAY_SIZE(k3_bank_offsets),
	.max_gpio = 128,
};

/**
 * struct spacemit_gpio_pctrl_map - gpio and pinctrl mapping
 * @gpio_pin:	start of gpio number in gpio-ranges
 * @pctrl_pin:	start of pinctrl number in gpio-ranges
 * @npins:	total number of pins in gpio-ranges
 * @node:	list node
 */
struct spacemit_gpio_pctrl_map {
	u32 gpio_pin;
	u32 pctrl_pin;
	u32 npins;
	struct list_head node;
};

/**
 * struct spacemit_gpio_plat - gpio device instance
 * @pinctrl_dev:pointer to gpio device
 * @gpiomap:	list node having mapping between gpio and pinctrl
 * @base:	I/O register base address of gpio device
 * @chip_data:	chip-specific data (K1 or K3)
 * @name:	gpio device name, ex GPIO0, GPIO1
 * @ngpios:	total number of gpios
 */
struct spacemit_gpio_plat {
	struct udevice *pinctrl_dev;
	struct list_head gpiomap;
	void __iomem *base;
	const struct spacemit_gpio_chip_data *chip_data;
};

static inline void *get_gpio_base(int bank, const struct spacemit_gpio_chip_data *chip_data)
{
	if (bank >= chip_data->num_banks) {
		printf("%s: Invalid bank %d\n", __func__, bank);
		return NULL;
	}
	return (void *)(k1x_gpio_base + chip_data->bank_offsets[bank]);
}

static int spacemit_gpio_direction_input(unsigned gpio, const struct spacemit_gpio_chip_data *chip_data)
{
	void __iomem *gpio_base;
	void __iomem *reg_addr;

	if (gpio >= chip_data->max_gpio) {
		printf("%s: Invalid GPIO %d\n", __func__, gpio);
		return -1;
	}

	gpio_base = get_gpio_base(GPIO_TO_REG(gpio), chip_data);
	if (!gpio_base)
		return -1;

	reg_addr = gpio_base + chip_data->regs->gcdr;
	writel(GPIO_TO_BIT(gpio), reg_addr);
	return 0;
}

static int spacemit_gpio_set_value(unsigned gpio, int value, const struct spacemit_gpio_chip_data *chip_data)
{
	void __iomem *gpio_base;
	void __iomem *reg_addr;

	if (gpio >= chip_data->max_gpio) {
		printf("%s: Invalid GPIO %d\n", __func__, gpio);
		return -1;
	}

	gpio_base = get_gpio_base(GPIO_TO_REG(gpio), chip_data);
	if (!gpio_base)
		return -1;

	if (value) {
		reg_addr = gpio_base + chip_data->regs->gpsr;
	} else {
		reg_addr = gpio_base + chip_data->regs->gpcr;
	}

	writel(GPIO_TO_BIT(gpio), reg_addr);
	return 0;
}

static int spacemit_gpio_direction_output(unsigned gpio, int value, const struct spacemit_gpio_chip_data *chip_data)
{
	void __iomem *gpio_base;
	void __iomem *reg_addr;

	if (gpio >= chip_data->max_gpio) {
		printf("%s: Invalid GPIO %d\n", __func__, gpio);
		return -1;
	}

	gpio_base = get_gpio_base(GPIO_TO_REG(gpio), chip_data);
	if (!gpio_base)
		return -1;

	reg_addr = gpio_base + chip_data->regs->gsdr;
	writel(GPIO_TO_BIT(gpio), reg_addr);
	spacemit_gpio_set_value(gpio, value, chip_data);
	return 0;
}

static int spacemit_gpio_get_value(unsigned gpio, const struct spacemit_gpio_chip_data *chip_data)
{
	void __iomem *gpio_base;
	void __iomem *reg_addr;
	u32 gpio_val;

	if (gpio >= chip_data->max_gpio) {
		printf("%s: Invalid GPIO %d\n", __func__, gpio);
		return -1;
	}

	gpio_base = get_gpio_base(GPIO_TO_REG(gpio), chip_data);
	if (!gpio_base)
		return -1;

	reg_addr = gpio_base + chip_data->regs->gplr;
	gpio_val = readl(reg_addr);

	return GPIO_VAL(gpio, gpio_val);
}

/**
 * spacemit_get_pctrl_from_gpio() - get associated pinctrl pin from gpio pin
 */
static int spacemit_get_pctrl_from_gpio(struct spacemit_gpio_plat *plat, u32 gpio, u32 *pctrl_pin)
{
	struct spacemit_gpio_pctrl_map *range = NULL;
	struct list_head *pos, *tmp;
	int ret = -EINVAL;

	list_for_each_safe(pos, tmp, &plat->gpiomap) {
		range = list_entry(pos, struct spacemit_gpio_pctrl_map, node);
		if (gpio >= range->gpio_pin &&
		    gpio < (range->gpio_pin + range->npins)) {
			*pctrl_pin = range->pctrl_pin + (gpio - range->gpio_pin);
			ret = 0;
			break;
		}
	}

	return ret;
}

/**
 * spacemit_get_gpio_pctrl_mapping() - get mapping between gpio and pinctrl
 */
static int spacemit_get_gpio_pctrl_mapping(struct udevice *dev)
{
	struct spacemit_gpio_plat *plat = dev_get_plat(dev);
	struct spacemit_gpio_pctrl_map *range = NULL;
	struct ofnode_phandle_args args;
	int index = 0, ret;

	for (;; index++) {
		ret = dev_read_phandle_with_args(dev, "gpio-ranges",
						 NULL, 3, index, &args);
		if (ret)
			break;

		range = (struct spacemit_gpio_pctrl_map *)devm_kzalloc(dev, sizeof(*range), GFP_KERNEL);
		if (!range)
			return -ENOMEM;

		range->gpio_pin = args.args[0];
		range->pctrl_pin = args.args[1];
		range->npins = args.args[2];
		list_add_tail(&range->node, &plat->gpiomap);
	}

	return 0;
}

static int spacemit_gpio_probe(struct udevice *dev)
{
	struct spacemit_gpio_plat *plat = dev_get_plat(dev);
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct clk gpio_clk;
	int ret = 0;

	/* Get chip-specific data from device tree */
	plat->chip_data = (const struct spacemit_gpio_chip_data *)dev_get_driver_data(dev);
	if (!plat->chip_data) {
		debug("%s: Failed to get chip data\n", __func__);
		return -EINVAL;
	}

	plat->base = dev_remap_addr_index(dev, 0);
	if (!plat->base) {
		debug("%s: Failed to get base address\n", __func__);
		return -EINVAL;
	}
	k1x_gpio_base = plat->base;

	uc_priv->gpio_count = dev_read_u32_default(dev, "gpio-count", 0);
	ret = uclass_get_device_by_phandle(UCLASS_PINCTRL, dev, "gpio-ranges",
				     &plat->pinctrl_dev);
	if (ret == 0) {
		INIT_LIST_HEAD(&plat->gpiomap);
		ret = spacemit_get_gpio_pctrl_mapping(dev);
		if (ret < 0) {
			dev_err(dev, "%s: Failed to get gpio to pctrl map ret(%d)\n",
				__func__, ret);
			return ret;
		}
	} else {
		dev_info(dev, "%s: has no gpio-ranges\n", __func__);
	}

	ret = clk_get_by_index(dev, 0, &gpio_clk);
	if (ret)
		return ret;
	clk_enable(&gpio_clk);

	return 0;
}

static const struct udevice_id spacemit_gpio_ids[] = {
	{ .compatible = "spacemit,k1x-gpio", .data = (ulong)&k1_chip_data },
	{ .compatible = "spacemit,k3-gpio", .data = (ulong)&k3_chip_data },
	{ }
};

static int spacemit_gpio_request(struct udevice *dev, unsigned gpio,
			     const char *label)
{
	struct spacemit_gpio_plat *plat = dev_get_plat(dev);
	u32 pctrl;
	int ret = 0;

	/* nothing to do if there is no corresponding pinctrl device */
	if (!plat->pinctrl_dev)
		return 0;

	ret = spacemit_get_pctrl_from_gpio(plat, gpio, &pctrl);
	if (ret < 0)
		return 0;

	return pinctrl_request(plat->pinctrl_dev, pctrl, 0);
}

static int spacemit_gpio_get_value_dm(struct udevice *dev, unsigned int gpio)
{
	struct spacemit_gpio_plat *plat = dev_get_plat(dev);
	return spacemit_gpio_get_value(gpio, plat->chip_data);
}

static int spacemit_gpio_set_value_dm(struct udevice *dev, unsigned int gpio,
				   int value)
{
	struct spacemit_gpio_plat *plat = dev_get_plat(dev);
	return spacemit_gpio_set_value(gpio, value, plat->chip_data);
}

static int spacemit_gpio_direction_input_dm(struct udevice *dev, unsigned int gpio)
{
	struct spacemit_gpio_plat *plat = dev_get_plat(dev);
	return spacemit_gpio_direction_input(gpio, plat->chip_data);
}

static int spacemit_gpio_direction_output_dm(struct udevice *dev, unsigned int gpio,
					  int value)
{
	struct spacemit_gpio_plat *plat = dev_get_plat(dev);
	return spacemit_gpio_direction_output(gpio, value, plat->chip_data);
}

static const struct dm_gpio_ops spacemit_gpio_ops = {
	.request		= spacemit_gpio_request,
	.direction_input	= spacemit_gpio_direction_input_dm,
	.direction_output	= spacemit_gpio_direction_output_dm,
	.get_value		= spacemit_gpio_get_value_dm,
	.set_value		= spacemit_gpio_set_value_dm,
};
U_BOOT_DRIVER(gpio_spacemit) = {
	.name	= "gpio_spacemit",
	.id	= UCLASS_GPIO,
	.ops	= &spacemit_gpio_ops,
	.of_match = spacemit_gpio_ids,
	.probe	= spacemit_gpio_probe,
	.plat_auto	= sizeof(struct spacemit_gpio_plat),
};
