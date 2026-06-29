// SPDX-License-Identifier: GPL-2.0+
/*
 * RT7451 Retimer driver
 *
 * Copyright (C) 2025 SPACEMIT Micro Limited
 */

#include <dm.h>
#include <i2c.h>
#include <errno.h>
#include <log.h>
#include <asm/global_data.h>
#include <linux/delay.h>

DECLARE_GLOBAL_DATA_PTR;

#define RT7451_PRIMARY_ADDR		0x13
#define RT7451_SECONDARY_ADDR		0x29

/* Register definitions */
#define RT7451_REG_CONFIG1		0xf8
#define RT7451_REG_CONFIG2		0xa4

/* Init values */
#define RT7451_CONFIG1_INIT_VAL		0x16
#define RT7451_CONFIG2_INIT_VAL		0x28

struct rt7451_priv {
	struct udevice *i2c_bus;
	u8 primary_addr;
	u8 secondary_addr;
};

static int rt7451_i2c_write_reg(struct udevice *bus, u8 chip_addr, u8 reg, u8 val)
{
	struct dm_i2c_ops *ops = i2c_get_ops(bus);
	struct i2c_msg msg;
	u8 buf[2];
	int ret;

	if (!ops || !ops->xfer)
		return -ENOSYS;

	buf[0] = reg;
	buf[1] = val;

	msg.addr = chip_addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;

	ret = ops->xfer(bus, &msg, 1);
	if (ret) {
		printf("RT7451: Failed to write reg 0x%02x to chip 0x%02x, ret=%d\n",
		       reg, chip_addr, ret);
		return ret;
	}

	return 0;
}

static int rt7451_i2c_read_reg(struct udevice *bus, u8 chip_addr, u8 reg, u8 *val)
{
	struct dm_i2c_ops *ops = i2c_get_ops(bus);
	struct i2c_msg msg[2];
	int ret;

	if (!ops || !ops->xfer)
		return -ENOSYS;

	/* Write register address */
	msg[0].addr = chip_addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &reg;

	/* Read register value */
	msg[1].addr = chip_addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = val;

	ret = ops->xfer(bus, msg, 2);
	if (ret) {
		printf("RT7451: Failed to read reg 0x%02x from chip 0x%02x, ret=%d\n",
		       reg, chip_addr, ret);
		return ret;
	}

	return 0;
}

static int rt7451_write_and_verify(struct udevice *bus, u8 chip_addr, u8 reg, u8 val)
{
	u8 readback;
	int ret;

	/* Write register */
	ret = rt7451_i2c_write_reg(bus, chip_addr, reg, val);
	if (ret)
		return ret;

	/* Small delay for write to complete */
	udelay(100);

	/* Read back and verify */
	ret = rt7451_i2c_read_reg(bus, chip_addr, reg, &readback);
	if (ret)
		return ret;

	if (readback != val) {
		printf("RT7451: Verify failed! chip 0x%02x reg 0x%02x: wrote 0x%02x, read 0x%02x\n",
		       chip_addr, reg, val, readback);
		return -EIO;
	}

	printf("RT7451: chip 0x%02x reg 0x%02x = 0x%02x (verified)\n",
	       chip_addr, reg, val);

	return 0;
}

static int rt7451_ensure_reg(struct udevice *bus, u8 chip_addr, u8 reg, u8 val,
			     bool *already_ok)
{
	u8 readback;
	int ret;

	ret = rt7451_i2c_read_reg(bus, chip_addr, reg, &readback);
	if (ret)
		return ret;

	if (readback == val) {
		if (already_ok)
			*already_ok = true;
		return 0;
	}

	if (already_ok)
		*already_ok = false;

	debug("RT7451: chip 0x%02x reg 0x%02x = 0x%02x, expected 0x%02x\n",
	      chip_addr, reg, readback, val);

	return rt7451_write_and_verify(bus, chip_addr, reg, val);
}

static int rt7451_init_sequence(struct rt7451_priv *priv)
{
	bool primary_ok, secondary_ok;
	int ret;

	ret = rt7451_ensure_reg(priv->i2c_bus, priv->primary_addr,
				RT7451_REG_CONFIG1, RT7451_CONFIG1_INIT_VAL,
				&primary_ok);
	if (ret) {
		printf("RT7451: Failed to init primary addr 0x%02x\n",
		       priv->primary_addr);
		return ret;
	}

	if (!primary_ok)
		udelay(1000);

	ret = rt7451_ensure_reg(priv->i2c_bus, priv->secondary_addr,
				RT7451_REG_CONFIG2, RT7451_CONFIG2_INIT_VAL,
				&secondary_ok);
	if (ret) {
		printf("RT7451: Failed to init secondary addr 0x%02x\n",
		       priv->secondary_addr);
		return ret;
	}

	if (primary_ok && secondary_ok) {
		printf("RT7451: Retimer already initialized, skipping\n");
		return 0;
	}

	printf("RT7451: Retimer initialized successfully\n");
	return 0;
}

static int rt7451_probe(struct udevice *dev)
{
	struct rt7451_priv *priv = dev_get_priv(dev);
	int ret;

	/* Get parent I2C bus */
	priv->i2c_bus = dev_get_parent(dev);
	if (!priv->i2c_bus) {
		printf("RT7451: Failed to get parent I2C bus\n");
		return -ENODEV;
	}

	/* Read addresses from device tree, use defaults if not specified */
	priv->primary_addr = dev_read_u32_default(dev, "primary-addr",
						  RT7451_PRIMARY_ADDR);
	priv->secondary_addr = dev_read_u32_default(dev, "secondary-addr",
						    RT7451_SECONDARY_ADDR);

	debug("RT7451: primary-addr=0x%02x, secondary-addr=0x%02x\n",
	      priv->primary_addr, priv->secondary_addr);

	/* Run initialization sequence */
	ret = rt7451_init_sequence(priv);
	if (ret) {
		printf("RT7451: Initialization failed\n");
		return ret;
	}

	return 0;
}

static const struct udevice_id rt7451_ids[] = {
	{ .compatible = "retimer,rt7451" },
	{ }
};

U_BOOT_DRIVER(rt7451) = {
	.name		= "rt7451",
	.id		= UCLASS_MISC,
	.of_match	= rt7451_ids,
	.probe		= rt7451_probe,
	.priv_auto	= sizeof(struct rt7451_priv),
};
