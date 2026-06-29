/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Enhanced SPI (eSPI) support for U-Boot
 *
 * Copyright (c) 2025 SpacemiT Technology Co., Ltd.
 */
#ifndef _ESPI_H_
#define _ESPI_H_
#include <linux/bitops.h>
/* Forward declaration */
struct udevice;

/**
 * struct espi_device - eSPI endpoint device
 * @channels: Supported channels (bitmask of ESPI_CH_*)
 * @max_freq: Maximum frequency in Hz
 */
struct espi_device {
	u32 channels;
	u32 max_freq;
};
/**
 * struct espi_ops - eSPI controller operations
 * @hw_init: Initialize controller hardware
 * @set_configuration: Set eSPI configuration
 * @send_vwire: Send virtual wire message
 * @receive_vwire: Receive virtual wire message
 * @send_oob: Send OOB message
 * @receive_oob: Receive OOB message
 * @flash_read: Read from flash
 * @flash_write: Write to flash
 * @flash_erase: Erase flash region
 */
struct espi_ops {
	int (*hw_init)(struct udevice *dev);
	int (*send_vwire)(struct udevice *dev, u16 vwire, bool state);
	int (*receive_vwire)(struct udevice *dev, u16 vwire, bool *state);
	int (*send_oob)(struct udevice *dev, u8 *buf, size_t len);
	int (*receive_oob)(struct udevice *dev, u8 *buf, size_t len);
	int (*flash_read)(struct udevice *dev, u32 addr, void *data, size_t len);
	int (*flash_write)(struct udevice *dev, u32 addr, const void *data, size_t len);
	int (*flash_erase)(struct udevice *dev, u32 addr, size_t len);
	int (*receive_peripheral)(struct udevice *dev, u8 *buf, size_t len);
};
/* eSPI Core API Functions */
/**
 * espi_hw_init() - Initialize eSPI controller hardware
 * @dev: eSPI controller device
 *
 * Return: 0 if OK, -ve on error
 */
int espi_hw_init(struct udevice *dev);
/**
 * espi_send_vwire() - Send virtual wire message
 * @dev: eSPI controller device
 * @vwire: Virtual wire identifier
 * @state: State to set the virtual wire to
 *
 * Return: 0 if OK, -ve on error
 */
int espi_send_vwire(struct udevice *dev, u16 vwire, bool state);
/**
 * espi_receive_vwire() - Receive virtual wire message
 * @dev: eSPI controller device
 * @vwire: Virtual wire identifier
 * @state: Pointer to store virtual wire state
 *
 * Return: 0 if OK, -ve on error
 */
int espi_receive_vwire(struct udevice *dev, u16 vwire, bool *state);
/**
 * espi_send_oob() - Send OOB message
 * @dev: eSPI controller device
 * @buf: Buffer containing message
 * @len: Length of message
 *
 * Return: 0 if OK, -ve on error
 */
int espi_send_oob(struct udevice *dev, u8 *buf, size_t len);
/**
 * espi_receive_oob() - Receive OOB message
 * @dev: eSPI controller device
 * @buf: Buffer to store message
 * @len: Length of buffer
 *
 * Return: 0 if OK, -ve on error
 */
int espi_receive_oob(struct udevice *dev, u8 *buf, size_t len);
/**
 * espi_flash_read() - Read from flash through eSPI
 * @dev: eSPI controller device
 * @addr: Flash address to read from
 * @data: Buffer to store read data
 * @len: Length of data to read
 *
 * Return: 0 if OK, -ve on error
 */
int espi_flash_read(struct udevice *dev, u32 addr, void *data, size_t len);
/**
 * espi_flash_write() - Write to flash through eSPI
 * @dev: eSPI controller device
 * @addr: Flash address to write to
 * @data: Data to write
 * @len: Length of data to write
 *
 * Return: 0 if OK, -ve on error
 */
int espi_flash_write(struct udevice *dev, u32 addr, const void *data, size_t len);
/**
 * espi_flash_erase() - Erase flash region through eSPI
 * @dev: eSPI controller device
 * @addr: Flash address to erase
 * @len: Length of region to erase
 *
 * Return: 0 if OK, -ve on error
 */
int espi_flash_erase(struct udevice *dev, u32 addr, size_t len);
/**
 * espi_negotiate_channels() - Negotiate eSPI channels
 * @dev: eSPI controller device
 *
 * Return: 0 if OK, -ve on error
 */
int espi_negotiate_channels(struct udevice *dev);
/**
 * espi_get_ops() - Get eSPI operations
 * @dev: eSPI controller device
 *
 * Return: pointer to operations or NULL if not found
 */
const struct espi_ops *espi_get_ops(struct udevice *dev);
#endif /* _ESPI_H_ */