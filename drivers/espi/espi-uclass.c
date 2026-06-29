// SPDX-License-Identifier: GPL-2.0+
/*
 * Enhanced SPI (eSPI) uclass for U-Boot
 *
 * Copyright (c) 2025 SpacemiT Technology Co., Ltd.
 */
#include <dm.h>
#include <errno.h>
#include <log.h>
#include <espi.h>
#include <string.h>
#include <dm/device-internal.h>
#include <dm/lists.h>

DECLARE_GLOBAL_DATA_PTR;

/* eSPI Core API Functions */
/**
 * espi_hw_init() - Initialize eSPI controller hardware
 * @dev: eSPI controller device
 *
 * Return: 0 if OK, -ve on error
 */
int espi_hw_init(struct udevice *dev)
{
	const struct espi_ops *ops = espi_get_ops(dev);
	if (!ops->hw_init)
		return -ENOSYS;
	return ops->hw_init(dev);
}

/**
 * espi_send_vwire() - Send virtual wire message
 * @dev: eSPI controller device
 * @vwire: Virtual wire identifier
 * @state: State to set the virtual wire to
 *
 * Return: 0 if OK, -ve on error
 */
int espi_send_vwire(struct udevice *dev, u16 vwire, bool state)
{
	const struct espi_ops *ops = espi_get_ops(dev);
	if (!ops->send_vwire)
		return -ENOSYS;
	return ops->send_vwire(dev, vwire, state);
}

/**
 * espi_receive_vwire() - Receive virtual wire message
 * @dev: eSPI controller device
 * @vwire: Virtual wire identifier
 * @state: Pointer to store virtual wire state
 *
 * Return: 0 if OK, -ve on error
 */
int espi_receive_vwire(struct udevice *dev, u16 vwire, bool *state)
{
	const struct espi_ops *ops = espi_get_ops(dev);
	if (!ops->receive_vwire)
		return -ENOSYS;
	return ops->receive_vwire(dev, vwire, state);
}

/**
 * espi_send_oob() - Send OOB message
 * @dev: eSPI controller device
 * @buf: Buffer containing message
 * @len: Length of message
 *
 * Return: 0 if OK, -ve on error
 */
int espi_send_oob(struct udevice *dev, u8 *buf, size_t len)
{
	const struct espi_ops *ops = espi_get_ops(dev);
	if (!ops->send_oob)
		return -ENOSYS;
	return ops->send_oob(dev, buf, len);
}

/**
 * espi_receive_oob() - Receive OOB message
 * @dev: eSPI controller device
 * @buf: Buffer to store message
 * @len: Length of buffer
 *
 * Return: number of bytes received if OK, -ve on error
 */

int espi_receive_oob(struct udevice *dev, u8 *buf, size_t len)
{
	const struct espi_ops *ops = espi_get_ops(dev);
	if (!ops->receive_oob)
		return -ENOSYS;
	return ops->receive_oob(dev, buf, len);
}

/**
 * espi_flash_read() - Read from flash through eSPI
 * @dev: eSPI controller device
 * @addr: Flash address to read from
 * @data: Buffer to store read data
 * @len: Length of data to read
 *
 * Return: 0 if OK, -ve on error
 */
int espi_flash_read(struct udevice *dev, u32 addr, void *data, size_t len)
{
	const struct espi_ops *ops = espi_get_ops(dev);
	if (!ops->flash_read)
		return -ENOSYS;
	return ops->flash_read(dev, addr, data, len);
}

/**
 * espi_flash_write() - Write to flash through eSPI
 * @dev: eSPI controller device
 * @addr: Flash address to write to
 * @data: Data to write
 * @len: Length of data to write
 *
 * Return: 0 if OK, -ve on error
 */
int espi_flash_write(struct udevice *dev, u32 addr, const void *data, size_t len)
{
	const struct espi_ops *ops = espi_get_ops(dev);
	if (!ops->flash_write)
		return -ENOSYS;
	return ops->flash_write(dev, addr, data, len);
}

/**
 * espi_flash_erase() - Erase flash region through eSPI
 * @dev: eSPI controller device
 * @addr: Flash address to erase
 * @len: Length of region to erase
 *
 * Return: 0 if OK, -ve on error
 */
int espi_flash_erase(struct udevice *dev, u32 addr, size_t len)
{
	const struct espi_ops *ops = espi_get_ops(dev);
	if (!ops->flash_erase)
		return -ENOSYS;
	return ops->flash_erase(dev, addr, len);
}

/**
 * espi_negotiate_channels() - Negotiate eSPI channels
 * @dev: eSPI controller device
 *
 * Return: 0 if OK, -ve on error
 */
int espi_negotiate_channels(struct udevice *dev)
{
	/* This is typically implemented by the driver during probe */
	return 0;
}

/**
 * espi_get_ops() - Get eSPI operations
 * @dev: eSPI controller device
 *
 * Return: pointer to operations or NULL if not found
 */
const struct espi_ops *espi_get_ops(struct udevice *dev)
{
	return (const struct espi_ops *) dev->driver->ops;
}

/**
 * espi_post_bind() - Post-bind callback for eSPI controller
 * @dev: eSPI controller device
 *
 * This function scans the device tree for child devices and binds them.
 * This enables automatic discovery of eSPI slave devices like EC.
 *
 * Return: 0 if OK, -ve on error
 */
static int espi_post_bind(struct udevice *dev)
{
	int ret = 0;
	debug("%s: %s, seq=%d\n", __func__, dev->name, dev_seq(dev));
#if CONFIG_IS_ENABLED(OF_REAL)
	/* Scan device tree for child devices */
	ret = dm_scan_fdt_dev(dev);
	if (ret)
		printf("%s: dm_scan_fdt_dev() failed: %d\n", __func__, ret);
#endif
	return ret;
}

/**
 * espi_child_post_bind() - Post-bind callback for eSPI child devices
 * @dev: eSPI child device
 *
 * This function handles the binding of child devices on the eSPI bus.
 * It can be used to set up device-specific properties from device tree.
 *
 * Return: 0 if OK, -ve on error
 */
static int espi_child_post_bind(struct udevice *dev)
{
	debug("%s: binding child device %s\n", __func__, dev->name);
	/* Child-specific initialization can be added here if needed */
	return 0;
}

UCLASS_DRIVER(espi) = {
	.id = UCLASS_ESPI,
	.name = "espi",
	.post_bind = espi_post_bind,
	.child_post_bind = espi_child_post_bind,
	.per_device_auto = sizeof(struct espi_device),
	.flags = DM_UC_FLAG_SEQ_ALIAS,
};