// SPDX-License-Identifier: GPL-2.0+
/*
 * Chromium OS cros_ec driver - eSPI interface
 *
 * Copyright (c) 2012 The Chromium OS Authors.
 * Modified for eSPI shared memory access
 */
/*
 * The Matrix Keyboard Protocol driver handles talking to the keyboard
 * controller chip. Mostly this is for keyboard functions, but some other
 * things have slipped in, so we provide generic services to talk to the
 * KBC.
 */
#include <dm.h>
#include <command.h>
#include <cros_ec.h>
#include <log.h>
#include <asm/io.h>
#include <linux/delay.h>
#include "../espi/espi-spacemit.h"

/*
 * Override the default packet size limit (0x100) from ec_commands.h
 * The eSPI interface supports larger packets, and Protocol V3 needs more than 256 bytes
 * (e.g. 252 bytes payload + 8 bytes header = 260 bytes).
 */
#undef EC_LPC_HOST_PACKET_SIZE
#define EC_LPC_HOST_PACKET_SIZE 0x200

/*
 * eSPI Shared Memory Address Mapping for EC Communication
 *
 * This driver converts LPC I/O port accesses to eSPI shared memory accesses.
 * The original LPC addresses are mapped to offsets within the eSPI shared memory region.
 *
 * eSPI Shared Memory Access Constraints:
 * - Only 1, 2, and 4-byte accesses are supported
 * - All accesses must be properly aligned to their size boundaries:
 *   * 1-byte: no alignment requirement
 *   * 2-byte: must be 2-byte aligned (address & 1 == 0)
 *   * 4-byte: must be 4-byte aligned (address & 3 == 0)
 * - Unaligned accesses will be rejected with error messages
 *
 * Address Mapping Strategy:
 * - LPC addresses (0x200-0x9FF) are mapped to shared memory offsets (0x000-0x2FF)
 * - The mapping preserves the relative layout and functionality of the original LPC interface
 * - All accesses go through g_espi_priv->pr_mem_base0 + offset
 *
 * Memory Layout in eSPI Shared Memory:
 * 0x000-0x001: Host command/data registers
 * 0x010-0x017: Host arguments (8 bytes)
 * 0x018-0x0FF: Host parameters (up to 232 bytes)
 * 0x100-0x1FF: Packet data area (256 bytes for protocol v3)
 * 0x200-0x2FF: Memory map region (256 bytes)
 */
static struct udevice *g_espi_dev = NULL;
#define ESPI_EC_BASE_OFFSET 0x000 /* Base offset in shared memory */
/* LPC to eSPI address mapping table */
#define ESPI_HOST_CMD_OFFSET 0x000    /* EC_LPC_ADDR_HOST_CMD (0x204) -> 0x000 */
#define ESPI_HOST_DATA_OFFSET 0x001   /* EC_LPC_ADDR_HOST_DATA (0x200) -> 0x001 */
#define ESPI_HOST_ARGS_OFFSET 0x010   /* EC_LPC_ADDR_HOST_ARGS (0x800) -> 0x010 */
#define ESPI_HOST_PARAM_OFFSET 0x018  /* EC_LPC_ADDR_HOST_PARAM (0x804) -> 0x018 */
#define ESPI_HOST_PACKET_OFFSET 0x100 /* EC_LPC_ADDR_HOST_PACKET (0x800) -> 0x100 */
#define ESPI_MEMMAP_OFFSET 0x200      /* EC_LPC_ADDR_MEMMAP (0x900) -> 0x200 */
/* External reference to global eSPI private structure */
extern struct spacemit_espi_priv *g_espi_priv;

/* Convert LPC address to eSPI shared memory offset */
static inline u32 lpc_to_espi_offset(u32 lpc_addr)
{
	u32 offset;

	switch (lpc_addr & 0xFF00) {
	case 0x200: /* HOST_CMD/DATA region */
		if (lpc_addr == EC_LPC_ADDR_HOST_CMD) {
			offset = ESPI_HOST_CMD_OFFSET;
			debug("LPC->eSPI: 0x%x (HOST_CMD) -> 0x%x\n", lpc_addr, offset);
			return offset;
		} else if (lpc_addr == EC_LPC_ADDR_HOST_DATA) {
			offset = ESPI_HOST_DATA_OFFSET;
			debug("LPC->eSPI: 0x%x (HOST_DATA) -> 0x%x\n", lpc_addr, offset);
			return offset;
		}
		break;
	case 0x800: /* HOST_ARGS/PARAM/PACKET region */
		if (lpc_addr >= EC_LPC_ADDR_HOST_PACKET) {
			offset = ESPI_HOST_PACKET_OFFSET + (lpc_addr - EC_LPC_ADDR_HOST_PACKET);
			debug("LPC->eSPI: 0x%x (HOST_PACKET+%d) -> 0x%x\n", lpc_addr,
			      lpc_addr - EC_LPC_ADDR_HOST_PACKET, offset);
			return offset;
		} else if (lpc_addr >= EC_LPC_ADDR_HOST_ARGS && lpc_addr < EC_LPC_ADDR_HOST_PARAM) {
			offset = ESPI_HOST_ARGS_OFFSET + (lpc_addr - EC_LPC_ADDR_HOST_ARGS);
			debug("LPC->eSPI: 0x%x (HOST_ARGS+%d) -> 0x%x\n", lpc_addr,
			      lpc_addr - EC_LPC_ADDR_HOST_ARGS, offset);
			return offset;
		} else if (lpc_addr >= EC_LPC_ADDR_HOST_PARAM) {
			offset = ESPI_HOST_PARAM_OFFSET + (lpc_addr - EC_LPC_ADDR_HOST_PARAM);
			debug("LPC->eSPI: 0x%x (HOST_PARAM+%d) -> 0x%x\n", lpc_addr,
			      lpc_addr - EC_LPC_ADDR_HOST_PARAM, offset);
			return offset;
		}
		break;
	case 0x900: /* MEMMAP region */
		offset = ESPI_MEMMAP_OFFSET + (lpc_addr - EC_LPC_ADDR_MEMMAP);
		debug("LPC->eSPI: 0x%x (MEMMAP+%d) -> 0x%x\n", lpc_addr,
		      lpc_addr - EC_LPC_ADDR_MEMMAP, offset);
		return offset;
	}
	/* Default mapping for unknown addresses */
	debug("Unknown LPC address 0x%x, using direct offset\n", lpc_addr);
	return lpc_addr & 0xFF;
}

/*
 * eSPI shared memory access functions
 *
 * eSPI shared memory supports 1, 2, and 4-byte aligned accesses only.
 * All accesses must be properly aligned to their size boundaries.
 */
static inline u8 espi_inb(u32 lpc_addr)
{
	void *addr;
	u32 offset;

	if (!g_espi_priv || !g_espi_priv->pr_mem_base0) {
		debug("%s: eSPI not initialized or shared memory not available\n", __func__);
		return 0xFF;
	}

	offset = lpc_to_espi_offset(lpc_addr);
	addr = (void *) (uintptr_t) (g_espi_priv->pr_mem_base0 + ESPI_EC_BASE_OFFSET + offset);

	/* eSPI shared memory: 1-byte access */
	return readb(addr);
}

static inline void espi_outb(u8 value, u32 lpc_addr)
{
	void *addr;
	u32 offset;
	if (!g_espi_priv || !g_espi_priv->pr_mem_base0) {
		debug("%s: eSPI not initialized or shared memory not available\n", __func__);
		return;
	}
	offset = lpc_to_espi_offset(lpc_addr);
	addr = (void *) (uintptr_t) (g_espi_priv->pr_mem_base0 + ESPI_EC_BASE_OFFSET + offset);

	/* eSPI shared memory: 1-byte access */
	writeb(value, addr);
}

/* Additional eSPI shared memory access functions for 2 and 4-byte operations */
static inline u16 espi_inw(u32 lpc_addr)
{
	void *addr;
	u32 offset;

	if (!g_espi_priv || !g_espi_priv->pr_mem_base0) {
		debug("%s: eSPI not initialized or shared memory not available\n", __func__);
		return 0xFFFF;
	}

	offset = lpc_to_espi_offset(lpc_addr);

	/* Ensure 2-byte alignment for eSPI shared memory access */
	if (offset & 1) {
		debug("%s: Unaligned 2-byte access at offset 0x%x\n", __func__, offset);
		return 0xFFFF;
	}

	addr = (void *) (uintptr_t) (g_espi_priv->pr_mem_base0 + ESPI_EC_BASE_OFFSET + offset);

	/* eSPI shared memory: 2-byte aligned access */
	return readw(addr);
}

static inline void espi_outw(u16 value, u32 lpc_addr)
{
	void *addr;
	u32 offset;
	if (!g_espi_priv || !g_espi_priv->pr_mem_base0) {
		debug("%s: eSPI not initialized or shared memory not available\n", __func__);
		return;
	}

	offset = lpc_to_espi_offset(lpc_addr);

	/* Ensure 2-byte alignment for eSPI shared memory access */
	if (offset & 1) {
		debug("%s: Unaligned 2-byte access at offset 0x%x\n", __func__, offset);
		return;
	}

	addr = (void *) (uintptr_t) (g_espi_priv->pr_mem_base0 + ESPI_EC_BASE_OFFSET + offset);

	/* eSPI shared memory: 2-byte aligned access */
	writew(value, addr);
}

static inline u32 espi_inl(u32 lpc_addr)
{
	void *addr;
	u32 offset;

	if (!g_espi_priv || !g_espi_priv->pr_mem_base0) {
		debug("%s: eSPI not initialized or shared memory not available\n", __func__);
		return 0xFFFFFFFF;
	}

	offset = lpc_to_espi_offset(lpc_addr);

	/* Ensure 4-byte alignment for eSPI shared memory access */
	if (offset & 3) {
		debug("%s: Unaligned 4-byte access at offset 0x%x\n", __func__, offset);
		return 0xFFFFFFFF;
	}

	addr = (void *) (uintptr_t) (g_espi_priv->pr_mem_base0 + ESPI_EC_BASE_OFFSET + offset);

	/* eSPI shared memory: 4-byte aligned access */
	return readl(addr);
}

static inline void espi_outl(u32 value, u32 lpc_addr)
{
	void *addr;
	u32 offset;
	if (!g_espi_priv || !g_espi_priv->pr_mem_base0) {
		debug("%s: eSPI not initialized or shared memory not available\n", __func__);
		return;
	}

	offset = lpc_to_espi_offset(lpc_addr);

	/* Ensure 4-byte alignment for eSPI shared memory access */
	if (offset & 3) {
		debug("%s: Unaligned 4-byte access at offset 0x%x\n", __func__, offset);
		return;
	}

	addr = (void *) (uintptr_t) (g_espi_priv->pr_mem_base0 + ESPI_EC_BASE_OFFSET + offset);

	/* eSPI shared memory: 4-byte aligned access */
	writel(value, addr);
}

/* Redefine inb and outb for eSPI shared memory access */
#undef inb
#undef outb
#define inb(addr) espi_inb(addr)
#define outb(val, addr) espi_outb((val), (addr))
/* Timeout waiting for a flash erase command to complete */
static const int CROS_EC_CMD_TIMEOUT_MS = 10000;
static int wait_for_sync(struct cros_ec_dev *dev)
{
	unsigned long start;
	start = get_timer(0);
	while (inb(EC_LPC_ADDR_HOST_CMD) & EC_LPC_STATUS_BUSY_MASK) {
		if (get_timer(start) > CROS_EC_CMD_TIMEOUT_MS) {
			debug("%s: Timeout waiting for CROS_EC sync\n", __func__);
			return -1;
		}
	}
	return 0;
}

int cros_ec_lpc_packet(struct udevice *udev, int out_bytes, int in_bytes)
{
	struct cros_ec_dev *dev = dev_get_uclass_priv(udev);
	uint8_t *d;
	int i;
	debug("%s: Called with out_bytes=%d, in_bytes=%d\n", __func__, out_bytes, in_bytes);
	if (out_bytes > EC_LPC_HOST_PACKET_SIZE) {
		debug("%s: ERROR - out_bytes %d > max %d\n", __func__, out_bytes,
		      EC_LPC_HOST_PACKET_SIZE);
		return log_msg_ret("Cannot send that many bytes\n", -E2BIG);
	}
	if (in_bytes > EC_LPC_HOST_PACKET_SIZE) {
		debug("%s: ERROR - in_bytes %d > max %d\n", __func__, in_bytes,
		      EC_LPC_HOST_PACKET_SIZE);
		return log_msg_ret("Cannot receive that many bytes\n", -E2BIG);
	}
	debug("%s: Waiting for EC sync\n", __func__);
	if (wait_for_sync(dev)) {
		debug("%s: ERROR - Timeout waiting for EC ready\n", __func__);
		return log_msg_ret("Timeout waiting ready\n", -ETIMEDOUT);
	}
	/* Write data */

	debug("%s: Writing %d bytes to EC packet buffer\n", __func__, out_bytes);
	debug("%s: Sending data: ", __func__);
	for (i = 0, d = (uint8_t *) dev->dout; i < out_bytes; i++, d++) {
		debug("0x%02x ", *d);
		outb(*d, EC_LPC_ADDR_HOST_PACKET + i);
	}
	debug("\n");
	/* Print detailed packet structure for debugging */
	if (out_bytes >= sizeof(struct ec_host_request)) {
		struct ec_host_request *req = (struct ec_host_request *) dev->dout;
		debug("%s: Packet structure analysis:\n", __func__);
		debug("%s:   struct_version = 0x%02x\n", __func__, req->struct_version);
		debug("%s:   checksum = 0x%02x\n", __func__, req->checksum);
		debug("%s:   command = 0x%04x (little-endian: 0x%02x 0x%02x)\n", __func__,
		      req->command, ((uint8_t *) &req->command)[0], ((uint8_t *) &req->command)[1]);
		debug("%s:   command_version = 0x%02x\n", __func__, req->command_version);
		debug("%s:   reserved = 0x%02x\n", __func__, req->reserved);
		debug("%s:   data_len = 0x%04x (little-endian: 0x%02x 0x%02x)\n", __func__,
		      req->data_len, ((uint8_t *) &req->data_len)[0],
		      ((uint8_t *) &req->data_len)[1]);
	}
	/* Start the command */
	debug("%s: Starting EC command with protocol 3\n", __func__);
	outb(EC_COMMAND_PROTOCOL_3, EC_LPC_ADDR_HOST_CMD);
	debug("%s: Waiting for EC command completion\n", __func__);
	if (wait_for_sync(dev)) {
		debug("%s: ERROR - Timeout waiting for EC command completion\n", __func__);
		return log_msg_ret("Timeout waiting ready\n", -ETIMEDOUT);
	}
	// mdelay(1000);
	/* Read back args */
	debug("%s: Reading %d bytes from EC packet buffer\n", __func__, in_bytes);
	for (i = 0, d = dev->din; i < in_bytes; i++, d++) {
		*d = inb(EC_LPC_ADDR_HOST_PACKET + i);
		if (i < 16) { /* Log first 16 bytes for debugging */
			debug("%s: din[%d] = 0x%02x\n", __func__, i, *d);
		}
	}
	debug("%s: Successfully completed packet transfer, returning %d bytes\n", __func__,
	      in_bytes);
	return in_bytes;
}

int cros_ec_lpc_command(struct udevice *udev, uint8_t cmd, int cmd_version, const uint8_t *dout,
			int dout_len, uint8_t **dinp, int din_len)
{
	struct cros_ec_dev *dev = dev_get_uclass_priv(udev);
	const int cmd_addr = EC_LPC_ADDR_HOST_CMD;
	const int data_addr = EC_LPC_ADDR_HOST_DATA;
	const int args_addr = EC_LPC_ADDR_HOST_ARGS;
	const int param_addr = EC_LPC_ADDR_HOST_PARAM;
	struct ec_lpc_host_args args;
	uint8_t *d;
	int csum;
	int i;
	debug("%s: cmd = %02x, cmd_version = %02x, dout_len = %d\n", __func__, cmd, cmd_version,
	      dout_len);
	if (dout_len > EC_PROTO2_MAX_PARAM_SIZE) {
		debug("%s: Cannot send %d bytes\n", __func__, dout_len);
		return -1;
	}
	/* Fill in args */
	args.flags = EC_HOST_ARGS_FLAG_FROM_HOST;
	args.command_version = cmd_version;
	args.data_size = dout_len;
	/* Calculate checksum */
	csum = cmd + args.flags + args.command_version + args.data_size;
	for (i = 0, d = (uint8_t *) dout; i < dout_len; i++, d++)
		csum += *d;
	args.checksum = (uint8_t) csum;
	if (wait_for_sync(dev)) {
		debug("%s: Timeout waiting ready\n", __func__);
		return -1;
	}
	/* Write args */
	for (i = 0, d = (uint8_t *) &args; i < sizeof(args); i++, d++)
		outb(*d, args_addr + i);
	/* Write data, if any */
	debug("cmd: %02x, ver: %02x", cmd, cmd_version);
	for (i = 0, d = (uint8_t *) dout; i < dout_len; i++, d++) {
		outb(*d, param_addr + i);
		debug("%02x ", *d);
	}
	outb(cmd, cmd_addr);
	debug("\n");
	if (wait_for_sync(dev)) {
		debug("%s: Timeout waiting for response\n", __func__);
		return -1;
	}
	/* Check result */
	i = inb(data_addr);
	if (i) {
		debug("%s: CROS_EC result code %d\n", __func__, i);
		return -i;
	}
	/* Read back args */
	for (i = 0, d = (uint8_t *) &args; i < sizeof(args); i++, d++)
		*d = inb(args_addr + i);
	/*
	 * If EC didn't modify args flags, then somehow we sent a new-style
	 * command to an old EC, which means it would have read its params
	 * from the wrong place.
	 */
	if (!(args.flags & EC_HOST_ARGS_FLAG_TO_HOST)) {
		debug("%s: CROS_EC protocol mismatch\n", __func__);
		return -EC_RES_INVALID_RESPONSE;
	}
	if (args.data_size > din_len) {
		debug("%s: CROS_EC returned too much data %d > %d\n", __func__, args.data_size,
		      din_len);
		return -EC_RES_INVALID_RESPONSE;
	}
	/* Read data, if any */
	for (i = 0, d = (uint8_t *) dev->din; i < args.data_size; i++, d++) {
		*d = inb(param_addr + i);
		debug("%02x ", *d);
	}
	debug("\n");
	/* Verify checksum */
	csum = cmd + args.flags + args.command_version + args.data_size;
	for (i = 0, d = (uint8_t *) dev->din; i < args.data_size; i++, d++)
		csum += *d;
	if (args.checksum != (uint8_t) csum) {
		debug("%s: CROS_EC response has invalid checksum\n", __func__);
		return -EC_RES_INVALID_CHECKSUM;
	}
	*dinp = dev->din;
	/* Return actual amount of data received */
	return args.data_size;
}

/**
 * Initialize eSPI protocol.
 *
 * @param dev		CROS_EC device
 * @param blob		Device tree blob
 * Return: 0 if ok, -1 on error
 */
int cros_ec_lpc_init(struct cros_ec_dev *dev, const void *blob)
{
	int byte, i;
	u8 cmd_byte, data_byte;
	/* Check if eSPI shared memory is available */
	if (!g_espi_priv || !g_espi_priv->pr_mem_base0) {
		debug("%s: eSPI not initialized or shared memory not available\n", __func__);
		return -1;
	}
	debug("%s: eSPI shared memory base: 0x%08x\n", __func__, g_espi_priv->pr_mem_base0);
	/* See if we can find an EC at the other end */
	cmd_byte = inb(EC_LPC_ADDR_HOST_CMD);
	data_byte = inb(EC_LPC_ADDR_HOST_DATA);
	debug("%s: Initial read - CMD: 0x%02x, DATA: 0x%02x\n", __func__, cmd_byte, data_byte);

	byte = 0xff;
	byte &= cmd_byte;
	byte &= data_byte;
	for (i = 0; i < EC_PROTO2_MAX_PARAM_SIZE && (byte == 0xff); i++) {
		u8 param_byte = inb(EC_LPC_ADDR_HOST_PARAM + i);
		byte &= param_byte;
		if (i < 8) { /* Only log first few bytes to avoid spam */
			debug("%s: PARAM[%d]: 0x%02x\n", __func__, i, param_byte);
		}
	}

	debug("%s: Final byte check result: 0x%02x\n", __func__, byte);

	if (byte == 0xff) {
		debug("%s: CROS_EC device not found on eSPI bus (all reads returned 0xFF)\n",
		      __func__);
		return -1;
	}
	debug("%s: CROS_EC device found on eSPI shared memory\n", __func__);
	return 0;
}

int cros_ec_espi_init(struct cros_ec_dev *dev, const void *blob)
{
	int ret;
	struct udevice *espi;
	/* First, find and get the eSPI device */
	ret = uclass_first_device_err(UCLASS_ESPI, &espi);
	if (ret) {
		debug("%s: Failed to find eSPI device: %d\n", __func__, ret);
		return -1;
	}
	/* Store the eSPI device for later use */
	g_espi_dev = espi;
	debug("%s: CROS_EC device found on eSPI bus\n", __func__);
	return 0;
}

static int cros_ec_probe(struct udevice *dev)
{
	struct cros_ec_dev *cdev = dev_get_uclass_priv(dev);
	int ret;
	debug("cros_ec_probe\n");
	/* Initialize eSPI communication first */
	ret = cros_ec_espi_init(cdev, gd->fdt_blob);
	if (ret) {
		debug("%s: Failed to initialize eSPI communication: %d\n", __func__, ret);
		return ret;
	}
	return cros_ec_register(dev);
}

static struct dm_cros_ec_ops cros_ec_ops = {
	.packet = cros_ec_lpc_packet,
	.command = cros_ec_lpc_command,
};

static const struct udevice_id cros_ec_ids[] = {
	{ .compatible = "google,cros-ec-espi" },
	{ .compatible = "google,cros-ec-lpc" }, /* Keep LPC compatibility */
	{}
};

U_BOOT_DRIVER(google_cros_ec_espi) = {
	.name = "google_cros_ec_espi",
	.id = UCLASS_CROS_EC,
	.of_match = cros_ec_ids,
	.probe = cros_ec_probe,
	.ops = &cros_ec_ops,
};
