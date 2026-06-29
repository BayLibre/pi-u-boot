/*
 Spacemit eSPI master driver.
 Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
 SPDX-License-Identifier: BSD-2-Clause-Patent
*/

#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include "espi-spacemit.h"

/* Global configuration variables */
u32 espi_gen_config = 0;
u32 espi_peri_config = 0;
u32 espi_vw_config = 0;
u32 espi_oob_config = 0;
u32 espi_flash_config = 0;
u8 espi_oob_request[ESPI_HW_OOB_SIZE];
u8 espi_flash_request[ESPI_HW_FLASH_SIZE];
u8 espi_peri_message[ESPI_HW_PERI_SIZE];
u8 espi_cmd_response[ESPI_HW_OOB_SIZE];
u32 espi_sys_evt;
u32 espi_sys_evt_active_lows;
u16 espi_vwire;
bool espi_vwire_assert;
DECLARE_BITMAP(espi_gpios, 32);

/* Helper functions */
/* bit operations */
void set_bit(int nr, volatile unsigned long *addr) { *addr |= (1UL << nr); }
void clear_bit(int nr, volatile unsigned long *addr) { *addr &= ~(1UL << nr); }

/**
 * Get eSPI configuration register base address
 * @return: configuration base address
 */
phys_addr_t espi_get_cfg_base(void)
{
	if (g_espi_priv)
		return g_espi_priv->cfg_base;
	return 0;
}

/**
 * Masked write to 32-bit eSPI register
 * @reg_address: register offset address
 * @mode: mask for bits to modify
 * @data: data to write (already positioned)
 */
void espi_write32_mask(u32 reg_address, u32 mask, u32 value)
{
	u32 reg_value;
	phys_addr_t espi_cfg_base = espi_get_cfg_base();
	reg_value = readl((void *) espi_cfg_base + reg_address);
	reg_value &= ~(mask);
	reg_value |= value;
	debug("espi_write32_mask: reg_address=0x%08x, mask=0x%08x, value=0x%08x\n", reg_address,
	      mask, value);
	writel(reg_value, (void *) espi_cfg_base + reg_address);
}

/**
 * Write to eSPI transmit header
 * @index: header index
 * @data: data byte to write
 */
void espi_write_tx_head(u8 index, u8 data)
{
	espi_write32_mask(ESPI_DN_TXHDRN(index), ESPI_DNCMD_HDATA(index, REG_8BIT_MASK),
			  ESPI_DNCMD_HDATA(index, data));
}

/**
 * Read from eSPI receive header
 * @index: header index
 * @return: data byte read
 */
u8 espi_read_rx_hdr(u32 index)
{
	u32 rx_hdr;
	u8 value;
	phys_addr_t espi_cfg_base = espi_get_cfg_base();
	rx_hdr = readl((void *) espi_cfg_base + ESPI_UP_RXHDRN(index));
	value = (u8) (ESPI_UPCMD_HDATA(index, rx_hdr));
	return value;
}

/**
 * Read transmit header
 * @offset: header offset
 * @return: header data
 */
u8 espi_read_tx_hdr(u8 offset)
{
	u32 value;
	u8 header;
	phys_addr_t espi_cfg_base = espi_get_cfg_base();
	value = readl((void *) espi_cfg_base + ESPI_DN_TXHDRN(offset));
	header = ESPI_DNCMD_GET_HDATA(offset, value);
	return header;
}

/**
 * Write downstream command
 * @dn_cmd: command type
 * @slave_id: slave device ID
 */
void espi_write_dn_cmd(u8 dn_cmd, u8 slave_id)
{
	u32 mode, data;
	mode = (ESPI_DNCMD_TYPE(ESPI_DNCMD_TYPE_MASK) | ESPI_DNCMD_EN |
		ESPI_DNCMD_SLAVE_SEL(ESPI_DNCMD_SLAVE_SEL_MASK));
	data = (ESPI_DNCMD_TYPE(dn_cmd) | ESPI_DNCMD_EN | ESPI_DNCMD_SLAVE_SEL(slave_id));
	espi_write32_mask(ESPI_DN_TXHDR, mode, data);
}

/**
 * Convert eSPI command to downstream command
 * @opcode: eSPI opcode
 * @return: downstream command
 */
u8 espi_cmd_to_dn_cmd(u8 opcode)
{
	u8 dncmd;
	switch (opcode) {
	case ESPI_CMD_SET_CONFIGURATION:
		dncmd = ESPI_DNCMD_SET_CONFIGURATION;
		break;
	case ESPI_CMD_GET_CONFIGURATION:
		dncmd = ESPI_DNCMD_GET_CONFIGURATION;
		break;
	case ESPI_CMD_PUT_PC:
		dncmd = ESPI_DNCMD_PR_MESSAGE;
		break;
	case ESPI_CMD_PUT_VWIRE:
		dncmd = ESPI_DNCMD_PUT_VW;
		break;
	case ESPI_CMD_PUT_OOB:
	case ESPI_CMD_GET_OOB:
		dncmd = ESPI_DNCMD_PUT_OOB;
		break;
	case ESPI_CMD_PUT_FLASH_C:
	case ESPI_CMD_GET_FLASH_NP:
		dncmd = ESPI_DNCMD_PUT_FLASH_C;
		break;
	case ESPI_CMD_RESET:
		dncmd = ESPI_DNCMD_IN_BAND_RESET;
		break;
	default:
		dncmd = ESPI_DNCMD_PR_MESSAGE;
		break;
	}
	return dncmd;
}

/**
 * Write eSPI command with header and data
 * @opcode: command opcode
 * @head_len: header length
 * @head_buf: header buffer
 * @data_len: data length
 * @data_buf: data buffer
 */
void espi_write_cmd(u8 opcode, u8 hlen, u8 *hbuf, u16 dlen, u8 *dbuf)
{
	u8 i;
	u8 dncmd;
	uint32_t txdata;
	u8 db0, db1, db2, db3;
	u16 len = dlen;
	u8 *buf = dbuf;
	dncmd = espi_cmd_to_dn_cmd(opcode);
	debug("opcode=%02x, dncmd=%02x, hdr=%d, dat=%d\n", opcode, dncmd, hlen, dlen);
	{
		/*format header to buffer, print once for debug*/
		char hex_buf[256];
		char *pos;
		pos = hex_buf;
		pos += sprintf(pos, "hdr=%d:", hlen);
		for (i = 0; i < hlen; i++) {
			pos += sprintf(pos, " %02x", ((u8 *) hbuf)[i]);
		}
		debug("%s\n", hex_buf);
	}

	for (i = 0; i < hlen; i++) {
		espi_write_tx_head(i, hbuf[i]);
	}

	switch (dncmd) {
	case ESPI_DNCMD_PUT_OOB:
		if (dlen < 3) {
			debug("OOB illegal %d\n", dlen);
			return;
		}
		debug("OOB write_tx_head(3, %02x), (4, %02x), (5, %02x)\n", dbuf[0], dbuf[1],
		      dbuf[2]);
		espi_write_tx_head(3, dbuf[0]);
		espi_write_tx_head(4, dbuf[1]);
		espi_write_tx_head(5, dbuf[2]);
		buf = dbuf + 3;
		len = dlen - 3;
		debug("OOB payload(len=%d): ", len);
		for (int k = 0; k < len; k++)
			debug("%02x ", buf[k]);
		debug("\n");
		break;
	}

	{
		/*format data to buffer, print once for debug*/
		char hex_buf[256];
		char *pos = hex_buf;
		pos += sprintf(pos, "dat=%d:", dlen);
		for (i = 0; i < dlen; i++) {
			pos += sprintf(pos, " %02x", ((u8 *) dbuf)[i]);
		}
		debug("%s\n", hex_buf);
	}

	for (i = 0; i < ((len + 3) / 4); i++) {
		u8 ilen = i * 4;
		if (ilen < len)
			db0 = buf[ilen];
		else
			db0 = 0x00;
		if ((ilen + 1) < len)
			db1 = buf[ilen + 1];
		else
			db1 = 0x00;
		if ((ilen + 2) < len)
			db2 = buf[ilen + 2];
		else
			db2 = 0x00;
		if ((ilen + 3) < len)
			db3 = buf[ilen + 3];
		else
			db3 = 0x00;
		txdata = MAKELONG(MAKEWORD(db0, db1), MAKEWORD(db2, db3));
		debug("TXDATA[%d]: %02x %02x %02x %02x (0x%08x)\n", i, db0, db1, db2, db3, txdata);
		espi_write32_mask(ESPI_DN_TXDATA_PORT, REG_32BIT_MASK, txdata);
	}

	espi_write_dn_cmd(dncmd, 0);
}

/**
 * Read response from eSPI
 * @opcode: command opcode
 * @hlen: header length
 * @hbuf: header buffer
 * @dlen: data length
 * @dbuf: data buffer
 * @return: response code
 */
u8 espi_read_rsp(u8 opcode, u8 hlen, u8 *hbuf, u16 dlen, u8 *dbuf)
{
	u8 dncmd;
	u32 i;
	u32 len = 0;
	u8 *buf = dbuf;
	u8 return_status = 0;
	phys_addr_t espi_cfg_base = espi_get_cfg_base();
	dncmd = espi_cmd_to_dn_cmd(opcode);
	debug("opcode=%02x, dncmd=%02x, hdr=%d, dat=%d\n", opcode, dncmd, hlen, dlen);
	switch (dncmd) {
	case ESPI_DNCMD_GET_CONFIGURATION:
		hbuf[0] = espi_read_tx_hdr(3);
		hbuf[1] = espi_read_tx_hdr(4);
		hbuf[2] = espi_read_tx_hdr(5);
		hbuf[3] = espi_read_tx_hdr(6);
		return return_status;
	case ESPI_DNCMD_PUT_OOB:
		hbuf[0] = espi_read_rx_hdr(0);
		hbuf[1] = espi_read_rx_hdr(1);
		hbuf[2] = espi_read_rx_hdr(2);
		len = ESPI_RXHDR_LENGTH(hbuf);
		if (len > dlen) {
			debug("OOB oversized %d > %d\n", len, dlen);
			return_status = ESPI_RSP_NON_FATAL_ERROR;
			goto err_exit;
		}
		if (len < 3) {
			debug("OOB illegal %d < 3\n", len);
			return_status = ESPI_RSP_NON_FATAL_ERROR;
			goto err_exit;
		}
		dlen = len;
		dbuf[0] = espi_read_rx_hdr(3);
		dbuf[1] = espi_read_rx_hdr(4);
		dbuf[2] = espi_read_rx_hdr(5);
		len = dlen - 3;
		buf = dbuf + 3;
		break;
	case ESPI_DNCMD_PUT_FLASH_C:
		hbuf[0] = espi_read_rx_hdr(0);
		hbuf[1] = espi_read_rx_hdr(1);
		hbuf[2] = espi_read_rx_hdr(2);
		hbuf[3] = espi_read_rx_hdr(3);
		hbuf[4] = espi_read_rx_hdr(4);
		hbuf[5] = espi_read_rx_hdr(5);
		hbuf[6] = espi_read_rx_hdr(6);
		len = ESPI_RXHDR_LENGTH(hbuf);
		if (len > dlen) {
			debug("flash access oversized %d > %d\n", len, dlen);
			return_status = ESPI_RSP_NON_FATAL_ERROR;
			goto err_exit;
		}
		dlen = len;
		break;
	}

	{
		/*format header to buffer, print once for debug*/
		char hex_buf[256];
		char *pos = hex_buf;
		pos += sprintf(pos, "hdr=%d:", hlen);
		for (i = 0; i < hlen; i++) {
			pos += sprintf(pos, " %02x", ((u8 *) hbuf)[i]);
		}
		debug("%s\n", hex_buf);
	}

	for (i = 0; i < ((len + 3) / 4); i++) {
		u8 ilen = i * 4;
		u32 rxdata;
		if (dncmd == ESPI_DNCMD_PR_MESSAGE)
			rxdata = readl((void *) (espi_cfg_base + ESPI_SLAVE0_RXMSG_DATA_PORT));
		else
			rxdata = readl((void *) (espi_cfg_base + ESPI_UP_RXDATA_PORT));
		/* restore original data read method, handle byte order in subsequent functions */
		if (ilen < dlen)
			buf[ilen] = LOBYTE(LOWORD(rxdata));
		if ((ilen + 1) < dlen)
			buf[ilen + 1] = HIBYTE(LOWORD(rxdata));
		if ((ilen + 2) < dlen)
			buf[ilen + 2] = LOBYTE(HIWORD(rxdata));
		if ((ilen + 3) < dlen)
			buf[ilen + 3] = HIBYTE(HIWORD(rxdata));
	}

	{
		/*format data to buffer, print once for debug*/
		char hex_buf[256];
		char *pos = hex_buf;
		pos += sprintf(pos, "dat=%d:", len);
		for (i = 0; i < len; i++) {
			pos += sprintf(pos, " %02x", ((u8 *) buf)[i]);
		}
		debug("%s\n", hex_buf);
	}

err_exit:
	return return_status;
}

/**
 * Poll eSPI status register for expected interrupt
 * @status: pointer to store status
 * @expected: expected interrupt bit(s) to wait for (e.g., ESPI_DNCMD_INT)
 * @return: 0 on success, -ETIMEDOUT on timeout, -EIO on error interrupt
 */
int espi_poll_status_mask(u32 *status, u32 expected)
{
	int count = 5000;  /* 5000 * 10us = 50ms max timeout */
	phys_addr_t espi_cfg_base = espi_get_cfg_base();
	u32 error_mask = ESPI_PROTOCOL_INT | ESPI_RESPONSE_INT;

	do {
		*status = readl((void *) espi_cfg_base + ESPI_SLAVE0_INT_STS);
		if (*status) {
			/* Clear the interrupt bits */
			writel(*status, (void *) espi_cfg_base + ESPI_SLAVE0_INT_STS);

			/* Check for expected interrupt first */
			if (*status & expected)
				return 0;

			/* Check for error interrupts */
			if (*status & error_mask) {
				debug("eSPI error interrupt: 0x%08x\n", *status);
				return -EIO;
			}
		}
		udelay(10);  /* Small delay to avoid tight loop */
	} while (count--);
	return -ETIMEDOUT;
}

/**
 * Poll eSPI status register for specific event type
 * @status: pointer to store status
 * @type: poll type (ESPI_POLL_DNCMD, ESPI_POLL_FLASH_REQ, etc.)
 * @return: 0 on success, -ETIMEDOUT on timeout, -EIO on error
 */
int espi_poll_status(u32 *status, enum espi_poll_type type)
{
	u32 expected;
	phys_addr_t espi_cfg_base = espi_get_cfg_base();

	switch (type) {
	case ESPI_POLL_DNCMD:
		expected = ESPI_DNCMD_INT;
		break;
	case ESPI_POLL_FLASH_REQ:
		expected = ESPI_FLASH_REQ_INT;
		break;
	case ESPI_POLL_RXOOB:
		expected = ESPI_RXOOB_INT;
		break;
	case ESPI_POLL_RXMSG:
		expected = ESPI_RXMSG_INT;
		break;
	default:
		expected = ESPI_DNCMD_INT;
		break;
	}

	/* Clear any pending expected interrupt before polling */
	writel(expected, (void *)(espi_cfg_base + ESPI_SLAVE0_INT_STS));

	return espi_poll_status_mask(status, expected);
}

/**
 * Check if write is done
 * @return: 1 if done, 0 if not
 */
int espi_write_done(void)
{
	u32 status;
	phys_addr_t espi_cfg_base = espi_get_cfg_base();
	status = (!(readl((void *) espi_cfg_base + ESPI_DN_TXHDR) & ESPI_DNCMD_EN));
	return status;
}

/**
 * Wait for eSPI ready
 * @return: 0 on success, -EBUSY on timeout
 */
int espi_wait_ready(void)
{
	int count = 1000;
	do {
		if (espi_write_done())
			return 0;
		udelay(1);
	} while (count--);
	return -EBUSY;
}

#ifndef CONFIG_XPL_BUILD
/* Advanced eSPI features not needed in SPL - VWire, Flash, OOB operations */

/**
 * Put virtual wire message
 * @vwire: Virtual wire ID
 * @state: State value (true=high level, false=low level)
 */
int espi_tx_vwire(u16 vwire, bool state)
{
	u8 type = ESPI_VWIRE_TYPE(vwire);
	u8 group, line;
	u8 head_buf[1] = { 0x00 };
	u8 data_buf[2];

	if (type == ESPI_VWIRE_SYSTEM) {
		/* System event VWire */
		group = ESPI_VWIRE_SYSTEM_GROUP(vwire);
		line = ESPI_VWIRE_SYSTEM_VWIRE(vwire);

		data_buf[0] = group;
		if (state) {
			data_buf[1] = ESPI_VWIRE_SYSTEM_EVENT_HIGH(line);
		} else {
			data_buf[1] = ESPI_VWIRE_SYSTEM_EVENT_LOW(line);
		}

		debug("eSPI: Put system VWire (%d:%d), state=%s\n", group, line,
		      state ? "high" : "low");

	} else if (type == ESPI_VWIRE_GPIO) {
		/* GPIO expander VWire */
		group = ESPI_VWIRE_GPIO_GROUP(vwire);
		line = ESPI_VWIRE_GPIO_VWIRE(vwire);

		/* Calculate bit index in espi_gpios bitmap */
		u8 bit_index = (group - ESPI_VWIRE_GPIO_EXPANDER_GROUP_MIN) * 4 + line;

		data_buf[0] = group;
		if (state) {
			data_buf[1] = ESPI_VWIRE_GPIO_EXPANDER_HIGH(line);
			/* Update local GPIO bitmap using bit_index */
			if (!test_bit(bit_index, espi_gpios))
				set_bit(bit_index, espi_gpios);
		} else {
			data_buf[1] = ESPI_VWIRE_GPIO_EXPANDER_LOW(line);
			/* Update local GPIO bitmap using bit_index */
			if (test_bit(bit_index, espi_gpios))
				clear_bit(bit_index, espi_gpios);
		}

		debug("eSPI: Put GPIO VWire (%d:%d) bit_index=%d, state=%s\n", group, line,
		      bit_index, state ? "high" : "low");

	} else {
		debug("eSPI: Unsupported VWire type: %u\n", type);
		return -EINVAL;
	}

	/* Send PUT_VWIRE command with proper format */
	espi_write_cmd(ESPI_CMD_PUT_VWIRE, 1, head_buf, 2, data_buf);

	return 0;
}

/**
 * Helper: Set/clear system event bit based on VWire ID
 */
static inline void espi_update_sys_event(u16 vwire, bool state)
{
	u8 grp = ESPI_VWIRE_SYSTEM_GROUP(vwire);
	u8 evt = ESPI_VWIRE_SYSTEM_VWIRE(vwire);
	u32 bit_pos = (grp * 4) + evt;

	if (state)
		espi_sys_evt |= (1 << bit_pos);
	else
		espi_sys_evt &= ~(1 << bit_pos);
}

/**
 * Sync System event VWire states from hardware registers to local variable
 * This function reads SLAVE0_RXVW register and updates espi_sys_evt
 */
static void espi_sync_system_vwires(void)
{
	phys_addr_t espi_cfg_base = espi_get_cfg_base();
	u32 rxvw_sys;

	/* Read SLAVE0_RXVW register containing system events */
	rxvw_sys = readl((void *) (espi_cfg_base + ESPI_SLAVE0_RXVW));

	debug("eSPI: Sync system events from RXVW: 0x%08x\n", rxvw_sys);

	/* Update espi_sys_evt based on register bits using helper function */
	espi_update_sys_event(ESPI_VWIRE_SYSTEM_SCI, !!(rxvw_sys & SLAVE0_RXVW_SCI));
	espi_update_sys_event(ESPI_VWIRE_SYSTEM_SMI, !!(rxvw_sys & SLAVE0_RXVW_SMI));
	espi_update_sys_event(ESPI_VWIRE_SYSTEM_RCIN, !!(rxvw_sys & SLAVE0_RXVW_RCIN));
	espi_update_sys_event(ESPI_VWIRE_SYSTEM_HOST_RST_ACK,
			      !!(rxvw_sys & SLAVE0_RXVW_HOST_RST_ACK));

	espi_update_sys_event(ESPI_VWIRE_SYSTEM_SLV_BOOT_LOAD_DONE,
			      !!(rxvw_sys & SLAVE0_RXVW_SLAVE_BOOT_LOAD_DONE));
	espi_update_sys_event(ESPI_VWIRE_SYSTEM_ERROR_FATAL,
			      !!(rxvw_sys & SLAVE0_RXVW_SLAVE_ERROR_FATAL));
	espi_update_sys_event(ESPI_VWIRE_SYSTEM_ERROR_NONFATAL,
			      !!(rxvw_sys & SLAVE0_RXVW_SLAVE_ERROR_NONFATAL));
	espi_update_sys_event(ESPI_VWIRE_SYSTEM_SLV_BOOT_LOAD_STATUS,
			      !!(rxvw_sys & SLAVE0_RXVW_SLAVE_BOOT_LOAD_STS));

	espi_update_sys_event(ESPI_VWIRE_SYSTEM_OOB_RST_ACK,
			      !!(rxvw_sys & SLAVE0_RXVW_OOB_RST_ACK));
	espi_update_sys_event(ESPI_VWIRE_SYSTEM_WAKE, !!(rxvw_sys & SLAVE0_RXVW_WAKE));
	espi_update_sys_event(ESPI_VWIRE_SYSTEM_PME, !!(rxvw_sys & SLAVE0_RXVW_PME));

	debug("eSPI: Updated espi_sys_evt: 0x%08x\n", espi_sys_evt);
}

/**
 * Sync GPIO VWire states from hardware registers to local bitmap
 * This function reads SLAVE0_RXVW_DATA register and updates espi_gpios
 */
static void espi_sync_gpio_vwires(void)
{
	phys_addr_t espi_cfg_base = espi_get_cfg_base();
	u32 rxvw_data;
	u8 grp_index, vwire;
	u8 dat, valid, level;
	u8 base_group;

	/* Read RXVW_DATA register containing all 4 groups */
	rxvw_data = readl((void *) (espi_cfg_base + ESPI_SLAVE0_RXVW_DATA));

	/* Process each of the 4 groups in RXVW_DATA */
	for (grp_index = 0; grp_index < ESPI_HW_GPIO_GROUPS; grp_index++) {
		/* Extract 8-bit data for this group */
		dat = (rxvw_data >> (grp_index * 8)) & 0xFF;

		/* Skip if no data */
		if (dat == 0)
			continue;

		/* Calculate actual GPIO group number (128, 129, 130, 131) */
		base_group = ESPI_VWIRE_GPIO_EXPANDER_GROUP_MIN + grp_index;

		/* Parse valid mask (high nibble) and level (low nibble) */
		valid = (dat >> 4) & 0x0F; /* bits 7:4 */
		level = dat & 0x0F;	   /* bits 3:0 */

		debug("eSPI: Sync GPIO group %d: data=0x%02x, valid=0x%x, level=0x%x\n", base_group,
		      dat, valid, level);

		/* Update each vwire in this group */
		for (vwire = 0; vwire < 4; vwire++) {
			/* Only update if valid bit is set */
			if (valid & (1 << vwire)) {
				/* Calculate bit index in espi_gpios bitmap */
				u8 bit_index = grp_index * 4 + vwire;
				bool state = !!(level & (1 << vwire));

				/* Update bitmap using bit_index */
				if (state) {
					if (!test_bit(bit_index, espi_gpios))
						set_bit(bit_index, espi_gpios);
				} else {
					if (test_bit(bit_index, espi_gpios))
						clear_bit(bit_index, espi_gpios);
				}

				debug("eSPI: GPIO (%d:%d) bit_index=%d = %s\n", base_group, vwire,
				      bit_index, state ? "high" : "low");
			}
		}
	}
}

/**
 * Receive virtual wire message
 * @vwire: Virtual wire ID
 * @state: Pointer to store state value
 * @return: 0 for success, negative value for error
 */
int espi_rx_vwire(u16 vwire, bool *state)
{
	u8 type;

	if (!state)
		return -EINVAL;

	type = ESPI_VWIRE_TYPE(vwire);

	if (type == ESPI_VWIRE_SYSTEM) {
		/* Sync system events from hardware first */
		espi_sync_system_vwires();

		/* For system events, check the system event status */
		/* Calculate correct bit position: (group * 4) + event_line */
		u8 grp = ESPI_VWIRE_SYSTEM_GROUP(vwire);
		u8 evt = ESPI_VWIRE_SYSTEM_VWIRE(vwire);
		u32 bit_pos = (grp * 4) + evt;

		*state = !!(espi_sys_evt & (1 << bit_pos));
		debug("eSPI: Receive system VWire 0x%04x (%d:%d), state=%s\n", vwire, grp, evt,
		      *state ? "asserted" : "deasserted");
		return 0;
	} else if (type == ESPI_VWIRE_GPIO) {
		/* Sync GPIO states from hardware first */
		espi_sync_gpio_vwires();

		/* For GPIO expander, check the GPIO bitmap */
		u8 group = ESPI_VWIRE_GPIO_GROUP(vwire);
		u8 line = ESPI_VWIRE_GPIO_VWIRE(vwire);
		u8 bit_index = (group - ESPI_VWIRE_GPIO_EXPANDER_GROUP_MIN) * 4 + line;

		*state = test_bit(bit_index, espi_gpios);
		debug("eSPI: Receive GPIO VWire 0x%04x (%d:%d), state=%s\n", vwire, group, line,
		      *state ? "high" : "low");
		return 0;
	}

	debug("eSPI: Unsupported VWire type: %u\n", type);
	return -EINVAL;
}

/**
 * Read data from flash
 * @addr: Flash address
 * @data: Buffer to store read data
 * @len: Length to read
 * @return: 0 for success, negative value for error
 */
int espi_tx_flash_read(u32 addr, void *data, size_t len)
{
	u8 hbuf[ESPI_FLASH_ACCESS_REQUEST_HDR_LEN];
	u8 response_hbuf[ESPI_FLASH_ACCESS_COMPLETION_HDR_LEN];
	u8 *buf = (u8 *) data;
	u16 actual_len;

	if (!data || len == 0)
		return -EINVAL;

	if (len > ESPI_HW_FLASH_SIZE) {
		debug("eSPI: Flash read length %zu exceeds maximum %d\n", len, ESPI_HW_FLASH_SIZE);
		return -EINVAL;
	}

	/* Prepare flash read request header */
	hbuf[0] = 0x00; /* ESPI_CYCLE_FLASH_READ */
	hbuf[1] = HIBYTE(len);
	hbuf[2] = LOBYTE(len);
	hbuf[3] = (addr >> 24) & 0xFF;
	hbuf[4] = (addr >> 16) & 0xFF;
	hbuf[5] = (addr >> 8) & 0xFF;
	hbuf[6] = addr & 0xFF;

	debug("eSPI: Flash read addr=0x%08x, len=%zu\n", addr, len);

	/* Send flash read request */
	espi_write_cmd(ESPI_CMD_PUT_FLASH_C, ESPI_FLASH_ACCESS_REQUEST_HDR_LEN, hbuf, 0, NULL);

	/* Read flash response */
	espi_read_rsp(ESPI_CMD_GET_FLASH_NP, ESPI_FLASH_ACCESS_COMPLETION_HDR_LEN, response_hbuf,
		      len, buf);

	/* Parse response length */
	actual_len = ESPI_RXHDR_LENGTH(response_hbuf);

	debug("eSPI: Flash read completed, actual_len=%d\n", actual_len);

	return (actual_len == len) ? 0 : -EIO;
}

/**
 * Write data to flash
 * @addr: Flash address
 * @data: Data to write
 * @len: Length to write
 * @return: 0 for success, negative value for error
 */
int espi_tx_flash_write(u32 addr, const void *data, size_t len)
{
	u8 hbuf[ESPI_FLASH_ACCESS_REQUEST_HDR_LEN];
	u8 response_hbuf[ESPI_FLASH_ACCESS_COMPLETION_HDR_LEN];
	const u8 *buf = (const u8 *) data;

	if (!data || len == 0)
		return -EINVAL;

	if (len > ESPI_HW_FLASH_SIZE) {
		debug("eSPI: Flash write length %zu exceeds maximum %d\n", len, ESPI_HW_FLASH_SIZE);
		return -EINVAL;
	}

	/* Prepare flash write request header */
	hbuf[0] = 0x01; /* ESPI_CYCLE_FLASH_WRITE */
	hbuf[1] = HIBYTE(len);
	hbuf[2] = LOBYTE(len);
	hbuf[3] = (addr >> 24) & 0xFF;
	hbuf[4] = (addr >> 16) & 0xFF;
	hbuf[5] = (addr >> 8) & 0xFF;
	hbuf[6] = addr & 0xFF;

	debug("eSPI: Flash write addr=0x%08x, len=%zu\n", addr, len);

	/* Send flash write request with data */
	espi_write_cmd(ESPI_CMD_PUT_FLASH_C, ESPI_FLASH_ACCESS_REQUEST_HDR_LEN, hbuf, len,
		       (u8 *) buf);

	/* Read flash write completion response */
	espi_read_rsp(ESPI_CMD_GET_FLASH_NP, ESPI_FLASH_ACCESS_COMPLETION_HDR_LEN, response_hbuf, 0,
		      NULL);

	/* Check if write was successful */
	if (response_hbuf[0] & 0x80) {
		debug("eSPI: Flash write failed, response=0x%02x\n", response_hbuf[0]);
		return -EIO;
	}

	debug("eSPI: Flash write completed successfully\n");

	return 0;
}

/**
 * Erase flash memory
 * @addr: Flash address
 * @len: Length to erase
 * @return: 0 for success, negative value for error
 */
int espi_tx_flash_erase(u32 addr, size_t len)
{
	u8 hbuf[ESPI_FLASH_ACCESS_REQUEST_HDR_LEN];
	u8 response_hbuf[ESPI_FLASH_ACCESS_COMPLETION_HDR_LEN];

	if (len == 0)
		return -EINVAL;

	if (len > ESPI_HW_FLASH_SIZE) {
		debug("eSPI: Flash erase length %zu exceeds maximum %d\n", len, ESPI_HW_FLASH_SIZE);
		return -EINVAL;
	}

	/* Prepare flash erase request header */
	hbuf[0] = 0x02; /* ESPI_CYCLE_FLASH_ERASE */
	hbuf[1] = HIBYTE(len);
	hbuf[2] = LOBYTE(len);
	hbuf[3] = (addr >> 24) & 0xFF;
	hbuf[4] = (addr >> 16) & 0xFF;
	hbuf[5] = (addr >> 8) & 0xFF;
	hbuf[6] = addr & 0xFF;

	debug("eSPI: Flash erase addr=0x%08x, len=%zu\n", addr, len);

	/* Send flash erase request */
	espi_write_cmd(ESPI_CMD_PUT_FLASH_C, ESPI_FLASH_ACCESS_REQUEST_HDR_LEN, hbuf, 0, NULL);

	/* Read flash erase completion response */
	espi_read_rsp(ESPI_CMD_GET_FLASH_NP, ESPI_FLASH_ACCESS_COMPLETION_HDR_LEN, response_hbuf, 0,
		      NULL);

	/* Check if erase was successful */
	if (response_hbuf[0] & 0x80) {
		debug("eSPI: Flash erase failed, response=0x%02x\n", response_hbuf[0]);
		return -EIO;
	}

	debug("eSPI: Flash erase completed successfully\n");

	return 0;
}

/**
 * Transmit OOB message
 * @buffer: message buffer
 * @length: message length
 * @return: 0 on success, negative on error
 */
int espi_tx_oob(u16 len, u8 *buf)
{
	u16 i;
	u8 hbuf[ESPI_OOB_MESSAGE_HDR_LEN] = { ESPI_CYCLE_TYPE_OOB_MESSAGE, HIBYTE(len),
					      LOBYTE(len) };
	{
		char hex_buf[256];
		char *pos = hex_buf;
		pos += sprintf(pos, "espi: put_oob %d -", len);
		for (i = 0; i < len; i++) {
			pos += sprintf(pos, " %02x", buf[i]);
		}
		debug("%s\n", hex_buf);
	}
	espi_write_cmd(ESPI_CMD_PUT_OOB, ESPI_OOB_MESSAGE_HDR_LEN, hbuf, len, buf);
	return 0;
}

int espi_rx_oob(u8 *buffer)
{
	u8 hbuf[ESPI_OOB_MESSAGE_HDR_LEN];
	u16 len, i;
	u8 *buf = espi_oob_request;
	u32 up_status;

	struct spacemit_espi_priv *priv = g_espi_priv;
	if (!priv) {
		return 0;
	}

	/* First check UP_CMD_STATUS bit to confirm if there is a new message */
	up_status = readl((void *) (priv->cfg_base + ESPI_UP_RXHDR));
	if (!(up_status & 0x08)) {
		debug("No new OOB message available\n");
		return 0;
	}
	/* Read OOB data */
	espi_read_rsp(ESPI_CMD_GET_OOB, ESPI_OOB_MESSAGE_HDR_LEN, hbuf, ESPI_HW_OOB_SIZE,
		      espi_oob_request);
	len = ESPI_RXHDR_LENGTH(hbuf);
	/* Copy data to input buffer */
	if (buffer && len > 0) {
		memcpy(buffer, espi_oob_request, len);
	}

	{
		char hex_buf[256];
		char *pos = hex_buf;
		debug("\n");
		pos += sprintf(pos, "espi: get_oob %d -", len);
		for (i = 0; i < len; i++) {
			pos += sprintf(pos, " %02x", buf[i]);
		}
		debug("%s\n", hex_buf);
	}

	/* Clear UP_CMD_STATUS bit, prepare for next OOB request */
	up_status = readl((void *) (priv->cfg_base + ESPI_UP_RXHDR));
	if (up_status & 0x08) {
		writel(0x08, (void *) (priv->cfg_base + ESPI_UP_RXHDR));
		debug("UP_CMD_STATUS cleared (0x%08x)\n", up_status);
	}

	return len;
}

#endif /* !CONFIG_XPL_BUILD */

/**
 * Write to eSPI slave configuration register
 * @reg_address: register offset
 * @data: data to write
 */
void espi_slave_write32(ulong reg_address, u32 data)
{
	u32 value;
	phys_addr_t espi_cfg_base = espi_get_cfg_base();
	value = readl((void *) espi_cfg_base + reg_address);
	value |= data;
	writel(value, (void *) espi_cfg_base + reg_address);
}

/**
 * Set hardware configuration
 * @slave_id: slave device ID
 * @slave_address: configuration register address
 * @config: configuration value
 */
void espi_hw_set_config(u32 slave_id, u16 slave_address, u32 config)
{
	debug("espi_hw_set_config: slave_id=%d, slave_address=0x%04x, config=0x%08x\n", slave_id,
	       slave_address, config);
	switch (slave_address) {
	case ESPI_SLAVE_GEN_CFG:
		if (config & ESPI_GEN_CRC_ENABLE)
			espi_slave_write32(ESPI_SLAVE0_CONFIG, ESPI_CRC_CHECK_EN);
		if (config & ESPI_GEN_IO_MODE_SET(ESPI_GEN_IO_MODE_SEL_MASK)) {
			espi_write32_mask(ESPI_SLAVE0_CONFIG,
					  ESPI_IO_MODE_SEL(ESPI_IO_MODE_SEL_MASK),
					  ESPI_IO_MODE_SEL(ESPI_GEN_IO_MODE_SEL(config)));
		}
		if (config & ESPI_GEN_OP_FREQ_SEL_MASK)
			espi_write32_mask(ESPI_SLAVE0_CONFIG,
					  ESPI_CLK_FREQ_SEL(ESPI_CLK_FREQ_SEL_MASK),
					  ESPI_CLK_FREQ_SEL(ESPI_GEN_OP_FREQ_SEL(config)));
		if (config & ESPI_GEN_ALERT_MODE_PIN)
			espi_slave_write32(ESPI_SLAVE0_CONFIG, ESPI_ALERT_MODE_SEL);
		if (config & ESPI_GEN_FLASH_CHAN_SUP)
			espi_slave_write32(ESPI_SLAVE0_CONFIG, ESPI_FLASH_CHANNEL_ENABLE);
		if (config & ESPI_GEN_OOB_CHAN_SUP)
			espi_slave_write32(ESPI_SLAVE0_CONFIG, ESPI_OOB_CHANNEL_ENABLE);
		if (config & ESPI_GEN_VWIRE_CHAN_SUP)
			espi_slave_write32(ESPI_SLAVE0_CONFIG, ESPI_VW_CHANNEL_ENABLE);
		if (config & ESPI_GEN_PERI_CHAN_SUP)
			espi_slave_write32(ESPI_SLAVE0_CONFIG, ESPI_PR_CHANNEL_ENABLE);
		break;
	}
}
/**
 * Get hardware configuration response
 * @opcode: command opcode
 * @head_len: header length
 * @head_buf: header buffer
 * @data_len: data length
 * @data_buf: data buffer
 */
void espi_hw_get_config_rsp(u8 opcode, u8 head_len, u8 *head_buf, u8 data_len, u8 *data_buf)
{
	u8 dn_cmd;
	dn_cmd = espi_cmd_to_dn_cmd(opcode);
	switch (dn_cmd) {
	case ESPI_DNCMD_GET_CONFIGURATION:
		head_buf[0] = espi_read_tx_hdr(3);
		head_buf[1] = espi_read_tx_hdr(4);
		head_buf[2] = espi_read_tx_hdr(5);
		head_buf[3] = espi_read_tx_hdr(6);
		break;
	}
}

/**
 * Configure eSPI memory mapping for peripheral channel
 * @priv: driver private data
 * @return: 0 on success, negative on error
 */
int espi_mem_cfg(void)
{
	extern struct spacemit_espi_priv *g_espi_priv;
	phys_addr_t espi_cfg_base = espi_get_cfg_base();
	u32 control;
	if (!g_espi_priv) {
		debug("eSPI not initialized\n");
		return -ENODEV;
	}
	/* Basic validation */
	if (!g_espi_priv->pr_mem_base0) {
		debug("Invalid PR memory base0 address\n");
		return -EINVAL;
	}
	/* Ensure valid memory range */
	if (g_espi_priv->pr_mem_base1 <= g_espi_priv->pr_mem_base0) {
		debug("Invalid PR memory range: base1(0x%08x) <= base0(0x%08x), fixing\n",
		      g_espi_priv->pr_mem_base1, g_espi_priv->pr_mem_base0);
		g_espi_priv->pr_mem_base1 = g_espi_priv->pr_mem_base0 + ESPI_MEM_SIZE;
	}
	/* Configure watchdog control */
	control = readl((void *) espi_cfg_base + ESPI_GLOBAL_CONTROL_0);
	control |= ESPI_CONTROL_WATCHDOG_EN;
	control &= ~(ESPI_CONTROL_WATCHDOG_CNT_MASK << ESPI_CONTROL_WATCHDOG_CNT_OFFSET);
	control |= ESPI_CONTROL_WATCHDOG_CNT(g_espi_priv->watchdog_timeout);
	writel(control, (void *) espi_cfg_base + ESPI_GLOBAL_CONTROL_0);
	/* Configure memory base addresses */
	writel(g_espi_priv->pr_mem_base0, (void *) espi_cfg_base + ESPI_PR_BASE_ADDR_MEM0);
	writel(g_espi_priv->pr_mem_base1, (void *) espi_cfg_base + ESPI_PR_BASE_ADDR_MEM1);
	debug("eSPI PR memory configured: base0=0x%08x, base1=0x%08x\n", g_espi_priv->pr_mem_base0,
	      g_espi_priv->pr_mem_base1);
	return 0;
}

/**
 * Set configuration for controller
 * @slave_id: slave device ID (reserved)
 * @slave_address: slave configuration address
 */
void espi_controller_config_apply(u32 slave_id, u16 slave_address)
{
	switch (slave_address) {
	case ESPI_SLAVE_GEN_CFG:
		espi_hw_set_config(slave_id, slave_address, espi_gen_config);
		break;
	case ESPI_SLAVE_PERI_CFG:
		espi_hw_set_config(slave_id, slave_address, espi_peri_config);
		break;
	case ESPI_SLAVE_VWIRE_CFG:
		espi_hw_set_config(slave_id, slave_address, espi_vw_config);
		break;
	case ESPI_SLAVE_OOB_CFG:
		espi_hw_set_config(slave_id, slave_address, espi_oob_config);
		break;
	case ESPI_SLAVE_FLASH_CFG:
		espi_hw_set_config(slave_id, slave_address, espi_flash_config);
		break;
	}
}

/**
 * Get configuration
 * @slave_address: slave configuration address
 */
void espi_get_config(u16 slave_address)
{
	u8 head_buf[3];
	head_buf[0] = 0x00;
	head_buf[1] = (u8) (slave_address >> 8);
	head_buf[2] = (u8) slave_address;
	espi_write_cmd(ESPI_CMD_GET_CONFIGURATION, 3, head_buf, 0, NULL);
}

/**
 * Set configuration
 * @slave_address: slave configuration address
 * @configs: configuration value
 */
void espi_set_config(u16 slave_address, u32 configs)
{
	u8 head_buf[7];
	head_buf[0] = 0x00;
	head_buf[1] = (u8) (slave_address >> 8);
	head_buf[2] = (u8) slave_address;
	head_buf[6] = (u8) (((u16) (configs >> 16)) >> 8);
	head_buf[5] = (u8) ((u16) (configs >> 16));
	head_buf[4] = (u8) (((u16) (configs)) >> 8);
	head_buf[3] = (u8) ((u16) (configs));
	espi_write_cmd(ESPI_CMD_SET_CONFIGURATION, 7, head_buf, 0, NULL);
}

/**
 * Poll IRQ status
 */
void espi_polling_irq_status(void)
{
	u32 status;
	phys_addr_t espi_cfg_base = espi_get_cfg_base();
	status = readl((void *) espi_cfg_base + ESPI_SLAVE0_INT_STS);
	writel(status, (void *) espi_cfg_base + ESPI_SLAVE0_INT_STS);
	/* check each interrupt bit */
	if (status & ESPI_FLASH_REQ_INT) {
		// todo: handle FLASH_INT
		debug("Espi Lib: ESPI_FLASH_REQ_INT\n");
	}
	if (status & ESPI_RXOOB_INT) {
		// todo: handle OOB_INT
		debug("Espi Lib: ESPI_RXOOB_INT\n");
	}
	if (status & ESPI_RXMSG_INT) {
		// todo: handle MSG_INT
		debug("Espi Lib: ESPI_RXMSG_INT\n");
	}
	if (status & ESPI_RXVW_GRP3_INT) {
		// todo: handle GRP3_INT
		debug("Espi Lib: ESPI_RXVW_GRP3_INT\n");
	}
	if (status & ESPI_RXVW_GRP2_INT) {
		// todo: handle GRP2_INT
		debug("Espi Lib: ESPI_RXVW_GRP2_INT\n");
	}
	if (status & ESPI_RXVW_GRP1_INT) {
		// todo: handle GRP1_INT
		debug("Espi Lib: ESPI_RXVW_GRP1_INT\n");
	}
	if (status & ESPI_RXVW_GRP0_INT) {
		// todo: handle GRP0_INT
		debug("Espi Lib: ESPI_RXVW_GRP0_INT\n");
	}
	/* check protocol error interrupt */
	if (status & ESPI_PROTOCOL_ERR_INT)
		debug("Espi Lib: ESPI_PROTOCOL_ERR_INT\n");
	if (status & ESPI_RXFLASH_OFLOW_INT)
		debug("Espi Lib: ESPI_RXFLASH_OFLOW_INT\n");
	if (status & ESPI_RXMSG_OFLOW_INT)
		debug("Espi Lib: ESPI_RXMSG_OFLOW_INT\n");
	if (status & ESPI_RXOOB_OFLOW_INT)
		debug("Espi Lib: ESPI_RXOOB_OFLOW_INT\n");
	if (status & ESPI_ILLEGAL_LEN_INT)
		debug("Espi Lib: ESPI_ILLEGAL_LEN_INT\n");
	if (status & ESPI_ILLEGAL_TAG_INT)
		debug("Espi Lib: ESPI_ILLEGAL_TAG_INT\n");
	if (status & ESPI_UNSUCSS_CPL_INT)
		debug("Espi Lib: ESPI_UNSUCSS_CPL_INT\n");
	if (status & ESPI_INVALID_CT_RSP_INT)
		debug("Espi Lib: ESPI_INVALID_CP_RSP_INT\n");
	if (status & ESPI_UNKNOWN_RSP_INT)
		debug("Espi Lib: ESPI_UNKNOWN_RSP_INT\n");
	if (status & ESPI_CRC_ERR_INT)
		debug("Espi Lib: ESPI_CRC_ERR_INT\n");
	if (status & ESPI_WAIT_TIMEOUT_INT)
		debug("Espi Lib: ESPI_WAIT_TIMEOUT_INT\n");
	if (status & ESPI_BUS_ERR_INT)
		debug("Espi Lib: ESPI_BUS_ERR_INT\n");
	if (status & ESPI_NON_FATAL_INT)
		debug("Espi Lib: ESPI_NON_FATAL_INT\n");
	if (status & ESPI_FATAL_ERR_INT)
		debug("Espi Lib: ESPI_FATAL_ERR_INT\n");
	if (status & ESPI_NO_RSP_INT)
		debug("Espi Lib: ESPI_NO_RSP_INT\n");
	if (status & ESPI_DNCMD_INT)
		debug("Espi Lib: ESPI_DNCMD_INT\n");
}

/**
 * Configure eSPI master auto gating
 * @espi_cfg_base: eSPI configuration base address
 */
void espi_config_auto_gating(phys_addr_t espi_cfg_base)
{
#ifdef CONFIG_SPACEMIT_ESPI_AUTO_GATING
	u32 gating;
	gating = readl((void *) espi_cfg_base + ESPI_GLOBAL_CONTROL_0);
	gating &= ESPI_MST_STOP_EN;
	writel(gating, (void *) espi_cfg_base + ESPI_GLOBAL_CONTROL_0);
#else
	debug("espi auto gating is not enabled\n");
#endif
}

/**
 * Initialize eSPI IRQ
 * @espi_cfg_base: eSPI configuration base address
 */
void espi_irq_enable(phys_addr_t espi_cfg_base)
{
	writel(ESPI_ALL_INT, (void *) espi_cfg_base + ESPI_SLAVE0_INT_EN);
}

/**
 * Send in-band reset
 */
void espi_inband_reset(void) { espi_write_cmd(ESPI_CMD_RESET, 0, NULL, 0, NULL); }

/**
 * Reset eSPI slave hardware
 * @espi_cfg_base: eSPI configuration base address
 */
void espi_slave_hw_reset(phys_addr_t espi_cfg_base)
{
	u32 reset;
	reset = readl((void *) espi_cfg_base + ESPI_GLOBAL_CONTROL_1);
	reset &= ~(ESPI_ESPI_RSTN);
	writel(reset, (void *) espi_cfg_base + ESPI_GLOBAL_CONTROL_1);
	reset = readl((void *) espi_cfg_base + ESPI_GLOBAL_CONTROL_1);
	reset |= ESPI_ESPI_RSTN;
	reset |= ESPI_SW_RST;
	writel(reset, (void *) espi_cfg_base + ESPI_GLOBAL_CONTROL_1);
}

/**
 * Initialize eSPI master
 * @espi_cfg_base: eSPI configuration base address
 */
void espi_master_init(phys_addr_t espi_cfg_base)
{
	espi_config_auto_gating(espi_cfg_base);
	espi_irq_enable(espi_cfg_base);
	espi_slave_hw_reset(espi_cfg_base);
}
