/*
 Spacemit eSPI master driver.
 Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
 SPDX-License-Identifier: BSD-2-Clause-Patent
*/

#ifndef __SPACEMIT_ESPI_CMD_LIB_
#define __SPACEMIT_ESPI_CMD_LIB_
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/types.h>
#include <clk.h>
#include <reset.h>

struct spacemit_espi_priv {
	phys_addr_t cfg_base;
	u32 op_freq;		   /* Operating frequency */
	u32 io_mode;		   /* IO mode: single/dual/quad */
	u32 alert_mode;		   /* Alert mode: pin/io1 */
	u32 alert_type;		   /* Alert type: push-pull/open-drain */
	bool crc_enable;	   /* CRC checking enable */
	bool resp_modifier_enable; /* Response modifier enable */

	/* Channel support flags */
	bool peri_chan_enable;	/* Peripheral channel enable */
	bool vwire_chan_enable; /* Virtual wire channel enable */
	bool oob_chan_enable;	/* OOB channel enable */
	bool flash_chan_enable; /* Flash channel enable */

	/* Timing and control parameters */
	u32 watchdog_timeout; /* Watchdog timeout value */
	u32 pr_mem_base0;     /* PR memory base address 0 */
	u32 pr_mem_base1;     /* PR memory base address 1 */
	u32 pr_io_base;	      /* PR IO base address for SIO operations */

	/* Auto gating control */
	bool auto_gating_enable; /* Auto clock gating enable */

	/* VW GPIO configuration */
	int vw_gpio_count;     /* Number of VW GPIO mappings */
	u16 vw_gpio_map[4][2]; /* VW GPIO mappings: [group][index] */

	/* Clock and reset */
	struct clk clk_sclk_src; /* Serial clock source */
	struct clk clk_sclk;     /* Serial clock */
	struct clk clk_mclk;     /* Master clock */
	struct reset_ctl reset_espi; /* eSPI reset control */

	/* Initialization state - set true only after full successful init */
	bool initialized;

	/* Slave general capabilities read at negotiation (0 = no EC slave) */
	u32 slave_gen_caps;
};

/* Global pointer to current eSPI instance */
extern struct spacemit_espi_priv *g_espi_priv;

/**
 * spacemit_espi_is_ready - Check if eSPI controller is fully initialized
 *
 * Return: true if eSPI is fully initialized and ready, false otherwise
 */
bool spacemit_espi_is_ready(void);

/**************************************************************************************/
#define LOHALF(b) (b & 0x0F)
#define HIHALF(b) (b >> 4)
#ifndef LOBYTE
#define LOBYTE(w) ((uint8_t) w)
#endif
#ifndef HIBYTE
#define HIBYTE(w) ((uint8_t) ((w) >> 8))
#endif
#ifndef LOWORD
#define LOWORD(w) ((uint16_t) w)
#endif
#ifndef HIWORD
#define HIWORD(w) ((uint16_t) ((w) >> 16))
#endif
#ifndef LODWORD
#define LODWORD(w) ((uint32_t) w)
#endif
#ifndef HIDWORD
#define HIDWORD(w) ((uint32_t) ((w) >> 32))
#endif
#define MAKEBYTE(a, b) ((uint8_t) (((a) & 0x0F) | (((b) & 0x0F) << 4)))
#ifndef MAKEWORD
#define MAKEWORD(a, b) ((uint16_t) (((uint16_t) (a)) | ((uint16_t) (b)) << 8))
#endif
#ifndef MAKELONG
#define MAKELONG(a, b) ((uint32_t) (((uint32_t) (a)) | ((uint32_t) (b)) << 16))
#endif
#ifndef MAKELLONG
#define MAKELLONG(a, b) ((uint64_t) (((uint64_t) (a)) | ((uint64_t) (b)) << 32))
#endif
#define REG_1BIT_MASK 0x001
#define REG_2BIT_MASK 0x003
#define REG_3BIT_MASK 0x007
#define REG_4BIT_MASK 0x00F
#define REG_5BIT_MASK 0x01F
#define REG_6BIT_MASK 0x03F
#define REG_7BIT_MASK 0x07F
#define REG_8BIT_MASK 0x0FF
#define REG_9BIT_MASK 0x1FF
#define REG_10BIT_MASK 0x3FF
#define REG_11BIT_MASK 0x7FF
#define REG_12BIT_MASK 0xFFF
#define REG_13BIT_MASK 0x1FFF
#define REG_14BIT_MASK 0x3FFF
#define REG_15BIT_MASK 0x7FFF
#define REG_16BIT_MASK 0xFFFF
#define REG_17BIT_MASK 0x1FFFF
#define REG_18BIT_MASK 0x3FFFF
#define REG_20BIT_MASK 0xFFFFF
#define REG_22BIT_MASK 0x3FFFFF
#define REG_23BIT_MASK 0x7FFFFF
#define REG_24BIT_MASK 0xFFFFFF
#define REG_28BIT_MASK 0xFFFFFFF
#define REG_32BIT_MASK 0xFFFFFFFF
#define REG_36BIT_MASK ULL(0xFFFFFFFFF)
#define REG_40BIT_MASK ULL(0xFFFFFFFFFF)
#define REG_44BIT_MASK ULL(0xFFFFFFFFFFF)
#define REG_48BIT_MASK ULL(0xFFFFFFFFFFFF)
#define REG16_1BIT_INDEX(n) ((((uint16_t) (n)) & (~UL(15))) >> 4)
#define REG16_2BIT_INDEX(n) ((((uint16_t) (n)) & (~UL(7))) >> 3)
#define REG16_4BIT_INDEX(n) ((((uint16_t) (n)) & (~UL(3))) >> 2)
#define REG16_8BIT_INDEX(n) ((((uint16_t) (n)) & (~UL(1))) >> 1)
#define REG32_1BIT_INDEX(n) ((((uint32_t) (n)) & (~UL(31))) >> 5)
#define REG32_2BIT_INDEX(n) ((((uint32_t) (n)) & (~UL(15))) >> 4)
#define REG32_4BIT_INDEX(n) ((((uint32_t) (n)) & (~UL(7))) >> 3)
#define REG32_8BIT_INDEX(n) ((((uint32_t) (n)) & (~UL(3))) >> 2)
#define REG32_16BIT_INDEX(n) ((((uint32_t) (n)) & (~UL(1))) >> 1)
#define REG64_1BIT_INDEX(n) ((((uint64_t) (n)) & (~ULL(63))) >> 6)
#define REG64_2BIT_INDEX(n) ((((uint64_t) (n)) & (~ULL(31))) >> 5)
#define REG64_4BIT_INDEX(n) ((((uint64_t) (n)) & (~ULL(15))) >> 4)
#define REG64_8BIT_INDEX(n) ((((uint64_t) (n)) & (~ULL(7))) >> 3)
#define REG64_16BIT_INDEX(n) ((((uint64_t) (n)) & (~ULL(3))) >> 2)
#define REG64_32BIT_INDEX(n) ((((uint64_t) (n)) & (~ULL(1))) >> 1)
#define REG_1BIT_INDEX(n) REG32_1BIT_INDEX(n)
#define REG_2BIT_INDEX(n) REG32_2BIT_INDEX(n)
#define REG_4BIT_INDEX(n) REG32_4BIT_INDEX(n)
#define REG_8BIT_INDEX(n) REG32_8BIT_INDEX(n)
#define REG_16BIT_INDEX(n) REG32_16BIT_INDEX(n)
#define REG16_1BIT_OFFSET(n) ((((uint16_t) (n)) & UL(15)) << 0)
#define REG16_2BIT_OFFSET(n) ((((uint16_t) (n)) & UL(7)) << 1)
#define REG16_4BIT_OFFSET(n) ((((uint16_t) (n)) & UL(3)) << 2)
#define REG16_8BIT_OFFSET(n) ((((uint16_t) (n)) & UL(1)) << 3)
#define REG32_1BIT_OFFSET(n) ((((uint32_t) (n)) & UL(31)) << 0)
#define REG32_2BIT_OFFSET(n) ((((uint32_t) (n)) & UL(15)) << 1)
#define REG32_4BIT_OFFSET(n) ((((uint32_t) (n)) & UL(7)) << 2)
#define REG32_8BIT_OFFSET(n) ((((uint32_t) (n)) & UL(3)) << 3)
#define REG32_16BIT_OFFSET(n) ((((uint32_t) (n)) & UL(1)) << 4)
#define REG64_1BIT_OFFSET(n) ((((uint64_t) (n)) & ULL(63)) << 0)
#define REG64_2BIT_OFFSET(n) ((((uint64_t) (n)) & ULL(31)) << 1)
#define REG64_4BIT_OFFSET(n) ((((uint64_t) (n)) & ULL(15)) << 2)
#define REG64_8BIT_OFFSET(n) ((((uint64_t) (n)) & ULL(7)) << 3)
#define REG64_16BIT_OFFSET(n) ((((uint64_t) (n)) & ULL(3)) << 4)
#define REG64_32BIT_OFFSET(n) ((((uint64_t) (n)) & ULL(1)) << 5)
#define REG_1BIT_OFFSET(n) REG32_1BIT_OFFSET(n)
#define REG_2BIT_OFFSET(n) REG32_2BIT_OFFSET(n)
#define REG_4BIT_OFFSET(n) REG32_4BIT_OFFSET(n)
#define REG_8BIT_OFFSET(n) REG32_8BIT_OFFSET(n)
#define REG_16BIT_OFFSET(n) REG32_16BIT_OFFSET(n)
#define REG_1BIT_ADDR(base, n) ((base) + (((n) & (~31)) >> 3))
#define REG_2BIT_ADDR(base, n) ((base) + (((n) & (~15)) >> 2))
#define REG_4BIT_ADDR(base, n) ((base) + (((n) & (~7)) >> 1))
#define REG_8BIT_ADDR(base, n) ((base) + (((n) & (~3)) >> 0))
#define REG_16BIT_ADDR(base, n) ((base) + (((n) & (~1)) << 1))
#define _BV_UL(bit) (UL(1) << (bit))
#define _BV_ULL(bit) (ULL(1) << (bit))
#define _BV(bit) _BV_UL(bit)
#define _SET_FV_UL(name, value) ((UL(0) + ((value) & (name##_MASK))) << (name##_OFFSET))
#define _SET_FV_ULL(name, value) ((ULL(0) + ((value) & (name##_MASK))) << (name##_OFFSET))
#define _SET_FV(name, value) _SET_FV_UL(name, value)
#define _GET_FV_UL(name, value) ((UL(0) + ((value) >> (name##_OFFSET))) & (name##_MASK))
#define _GET_FV_ULL(name, value) ((ULL(0) + ((value) >> (name##_OFFSET))) & (name##_MASK))
#define _GET_FV(name, value) _GET_FV_UL(name, value)

/* Default to _SET_FV() as by default _FV() is used to generate field
 * values to be written to the registers.
 */
#define _FV(name, value) _SET_FV(name, value)
#define _GET_FVn(n, name, value) (((value) >> (name##_OFFSET(n))) & (name##_MASK))
#define _SET_FVn(n, name, value) (((value) & (name##_MASK)) << (name##_OFFSET(n)))
#define _GET_FV_ULLn(n, name, value) ((((uint64_t) (value)) >> (name##_OFFSET(n))) & (name##_MASK))
#define _SET_FV_ULLn(n, name, value) ((((uint64_t) (value)) & (name##_MASK)) << (name##_OFFSET(n)))
/**************************************************************************************/

/* DN_TXHDR_0
 * DN_TXHDR_1
 * DN_TXHDR_2
 */
#define ESPI_DNCMD_HDATA_OFFSET(N) REG32_8BIT_OFFSET((N) + 1)
#define ESPI_DNCMD_HDATA_MASK REG_8BIT_MASK
#define ESPI_DNCMD_HDATA(N, VALUE) _SET_FVn(N, ESPI_DNCMD_HDATA, VALUE)
#define ESPI_DNCMD_GET_HDATA(N, VALUE) _GET_FVn(N, ESPI_DNCMD_HDATA, VALUE)
#define ESPI_DNCMD_SLAVE_SEL_OFFSET 4
#define ESPI_DNCMD_SLAVE_SEL_MASK REG_2BIT_MASK
#define ESPI_DNCMD_SLAVE_SEL(VALUE) _SET_FV(ESPI_DNCMD_SLAVE_SEL, VALUE)
#define ESPI_DNCMD_EN BIT(3)
#define ESPI_DNCMD_TYPE_OFFSET 0
#define ESPI_DNCMD_TYPE_MASK REG_3BIT_MASK
#define ESPI_DNCMD_TYPE(VALUE) _SET_FV(ESPI_DNCMD_TYPE, VALUE)
/* Register Offset */
#define ESPI_DN_TXHDR (0x00)
#define ESPI_DN_TXHDRN(N) (0x00 + (REG32_8BIT_INDEX((N) + 1) << 2))
#define ESPI_DN_TXDATA_PORT (0x0C)
#define ESPI_UP_RXHDR (0x10)
#define ESPI_UP_RXHDRN(N) (0x10 + (REG32_8BIT_INDEX((N) + 1) << 2))
#define ESPI_UP_RXDATA_PORT (0x18)
#define ESPI_MASTER_CAP (0x2C)
#define ESPI_GLOBAL_CONTROL_0 (0x30)
#define ESPI_GLOBAL_CONTROL_1 (0x34)
#define ESPI_PR_BASE_ADDR_MEM0 (0x38)
#define ESPI_PR_BASE_ADDR_MEM1 (0x3C)
#define ESPI_SLAVE0_STS_SHADOW (0x44)
#define ESPI_SLAVE0_CONFIG (0x68)
#define ESPI_SLAVE0_INT_EN (0x6C)
#define ESPI_SLAVE0_INT_STS (0x70)
#define ESPI_SLAVE0_RXMSG_HDR0 (0x74)
#define ESPI_SLAVE0_RXMSG_HDR1 (0x78)
#define ESPI_SLAVE0_RXMSG_DATA_PORT (0x7C)
#define ESPI_SLAVE0_RXVW_STS (0x98)
#define ESPI_SLAVE0_RXVW (0x9C)
#define ESPI_SLAVE0_RXVW_DATA (0xA0)
#define ESPI_SLAVE0_RXVW_INDEX (0xA4)
#define ESPI_SLAVE0_VW_CTL (0xA8)
#define ESPI_SLAVE0_VW_POLARITY (0xAC)

/* SLAVE0_RXVW - System event bits */
#define SLAVE0_RXVW_HOST_RST_ACK BIT(19)
#define SLAVE0_RXVW_RCIN BIT(18)
#define SLAVE0_RXVW_SMI BIT(17)
#define SLAVE0_RXVW_SCI BIT(16)
#define SLAVE0_RXVW_SLAVE_BOOT_LOAD_STS BIT(15)
#define SLAVE0_RXVW_SLAVE_ERROR_NONFATAL BIT(14)
#define SLAVE0_RXVW_SLAVE_ERROR_FATAL BIT(13)
#define SLAVE0_RXVW_SLAVE_BOOT_LOAD_DONE BIT(12)
#define SLAVE0_RXVW_PME BIT(11)
#define SLAVE0_RXVW_WAKE BIT(10)
#define SLAVE0_RXVW_OOB_RST_ACK BIT(8)
#define SLAVE0_RXVW_DNX_ACK BIT(1)
#define SLAVE0_RXVW_SUS_ACK_B BIT(0)

/* 4.2 Command phase */
#define ESPI_CMD_PUT_PC 0x00
#define ESPI_CMD_GET_PC 0x01
#define ESPI_CMD_PUT_NP 0x02
#define ESPI_CMD_GET_NP 0x03
#define ESPI_CMD_PUT_IORD_SHORT(C) (0x40 | (C))
#define ESPI_CMD_PUT_IOWR_SHORT(C) (0x44 | (C))
#define ESPI_CMD_PUT_MEMRD32_SHORT(C) (0x48 | (C))
#define ESPI_CMD_PUT_MEMWR32_SHORT(C) (0x4C | (C))
#define ESPI_CMD_PUT_VWIRE 0x04
#define ESPI_CMD_GET_VWIRE 0x05
#define ESPI_CMD_PUT_OOB 0x06
#define ESPI_CMD_GET_OOB 0x07
#define ESPI_CMD_PUT_FLASH_C 0x08
#define ESPI_CMD_GET_FLASH_NP 0x09
#define ESPI_CMD_GET_STATUS 0x25
#define ESPI_CMD_SET_CONFIGURATION 0x22
#define ESPI_CMD_GET_CONFIGURATION 0x21
#define ESPI_CMD_RESET 0xFF

/*Cycle Types */
#define ESPI_TAG_OFFSET 4
#define ESPI_TAG_MASK REG_4BIT_MASK
#define ESPI_TXHDR_TAG(VALUE) _SET_FV(ESPI_TAG, VALUE)
#define ESPI_RXHDR_TAG(VALUE) _GET_FV(ESPI_TAG, VALUE)
#define ESPI_LEN_OFFSET 0
#define ESPI_LEN_MASK REG_4BIT_MASK
#define ESPI_TXHDR_LEN(VALUE) _SET_FV(ESPI_LEN, VALUE)
#define ESPI_RXHDR_LEN(VALUE) _GET_FV(ESPI_LEN, VALUE)
#define ESPI_TXHDR_LENGTH(HEADER, LEN)                                                             \
	do {                                                                                       \
		HEADER[1] &= ~ESPI_TXHDR_LEN(REG_4BIT_MASK);                                       \
		HEADER[1] |= ESPI_TXHDR_LEN((u8) ((LEN) >> 8));                                    \
		HEADER[2] = ((u8) (LEN));                                                          \
	} while (0)
#define ESPI_RXHDR_LENGTH(HEADER) MAKEWORD(HEADER[2], ESPI_RXHDR_LEN(HEADER[1]))

/* SLAVE0_CONFIG */
#define ESPI_CRC_CHECK_EN BIT(31)
#define ESPI_ESPI_RSTN BIT(5)
#define ESPI_SW_RST BIT(0)

/* General Capabilities and Configurations */
#define ESPI_SLAVE_GEN_CFG 0x08
#define ESPI_GEN_CRC_ENABLE BIT(31)
#define ESPI_ALERT_MODE_SEL BIT(30)
#define ESPI_GEN_ALERT_MODE_PIN BIT(28)
#define ESPI_GEN_OPEN_DRAIN_ALERT_SEL BIT(23)
#define ESPI_GEN_OPEN_DRAIN_ALERT_SUP BIT(19)
#define ESPI_GEN_FLASH_CHAN_SUP BIT(3)
#define ESPI_GEN_OOB_CHAN_SUP BIT(2)
#define ESPI_GEN_VWIRE_CHAN_SUP BIT(1)
#define ESPI_GEN_PERI_CHAN_SUP BIT(0)
#define ESPI_SLAVE_PERI_CFG 0x10
#define ESPI_SLAVE_VWIRE_CFG 0x20
#define ESPI_SLAVE_OOB_CFG 0x30
#define ESPI_SLAVE_FLASH_CFG 0x40
#define ESPI_GEN_IO_MODE_SEL_OFFSET 26
#define ESPI_GEN_IO_MODE_SEL_MASK REG_2BIT_MASK
#define ESPI_GEN_IO_MODE_SEL(VALUE) _GET_FV(ESPI_GEN_IO_MODE_SEL, VALUE)
#define ESPI_GEN_IO_MODE_SET(VALUE) _SET_FV(ESPI_GEN_IO_MODE_SEL, VALUE)
#define ESPI_IO_MODE_SEL_OFFSET 28
#define ESPI_IO_MODE_SEL_MASK REG_2BIT_MASK
#define ESPI_IO_MODE_SEL(MODE) _SET_FV(ESPI_IO_MODE_SEL, MODE)
#define ESPI_CLK_FREQ_SEL_OFFSET 25
#define ESPI_CLK_FREQ_SEL_MASK REG_3BIT_MASK
#define ESPI_CLK_FREQ_SEL(FREQ) _SET_FV(ESPI_CLK_FREQ_SEL, FREQ)
#define ESPI_GEN_OP_FREQ_SEL_OFFSET 20
#define ESPI_GEN_OP_FREQ_SEL_MASK REG_3BIT_MASK
#define ESPI_GEN_OP_FREQ_SEL(VALUE) _GET_FV(ESPI_GEN_OP_FREQ_SEL, VALUE)
#define ESPI_GEN_IO_MODE_SUP_OFFSET 24
#define ESPI_GEN_IO_MODE_SUP_MASK REG_2BIT_MASK
#define ESPI_GEN_IO_MODE_SUP(VALUE) _GET_FV(ESPI_GEN_IO_MODE_SUP, VALUE)
#define ESPI_GEN_OP_FREQ_SUP_OFFSET 16
#define ESPI_GEN_OP_FREQ_SUP_MASK REG_3BIT_MASK
#define ESPI_GEN_OP_FREQ_SUP(VALUE) _GET_FV(ESPI_GEN_OP_FREQ_SUP, VALUE)
#define ESPI_GEN_ALERT_MODE_IO1 0
#define ESPI_GEN_ALERT_TYPE_OD ESPI_GEN_OPEN_DRAIN_ALERT_SEL
#define ESPI_GEN_OP_FREQ_20MHZ 0
#define ESPI_GEN_OP_FREQ_25MHZ 1
#define ESPI_GEN_OP_FREQ_33MHZ 2
#define ESPI_GEN_OP_FREQ_50MHZ 3
#define ESPI_GEN_OP_FREQ_66MHZ 4
#define ESPI_GEN_IO_MODE_SINGLE 0x0
#define ESPI_GEN_IO_MODE_DUAL 0x1
#define ESPI_GEN_IO_MODE_QUAD 0x2
#define ESPI_GEN_CAP_MASK                                                                          \
	(ESPI_GEN_IO_MODE_SUP(ESPI_GEN_IO_MODE_SUP_MASK) | ESPI_GEN_OPEN_DRAIN_ALERT_SUP |         \
	 ESPI_GEN_OP_FREQ_SUP(ESPI_GEN_OP_FREQ_SUP_MASK) | ESPI_GEN_FLASH_CHAN_SUP |               \
	 ESPI_GEN_OOB_CHAN_SUP | ESPI_GEN_VWIRE_CHAN_SUP | ESPI_GEN_PERI_CHAN_SUP)
#define ESPI_PR_CHANNEL_ENABLE BIT(3)
#define ESPI_VW_CHANNEL_ENABLE BIT(2)
#define ESPI_OOB_CHANNEL_ENABLE BIT(1)
#define ESPI_FLASH_CHANNEL_ENABLE BIT(0)

/* DN_TXHDR_0 */
#define ESPI_DNCMD_SET_CONFIGURATION 0x00
#define ESPI_DNCMD_GET_CONFIGURATION 0x01
#define ESPI_DNCMD_IN_BAND_RESET 0x02
#define ESPI_DNCMD_PR_MESSAGE 0x04
#define ESPI_DNCMD_PUT_VW 0x05
#define ESPI_DNCMD_PUT_OOB 0x06
#define ESPI_DNCMD_PUT_FLASH_C 0x07
#define ESPI_ENABLE_PR_CHANNEL 0x1115
#define ESPI_ENABLE_VW_CHANNEL 0x70701
#define ESPI_ENABLE_OOB_CHANNEL 0x111
#define ESPI_ENABLE_FLASH_CHANNEL 0x11125
/* UP_RXHDR_0
 * UP_RXHDR_1
 */
#define ESPI_UPCMD_HDATA_OFFSET(N) REG32_8BIT_OFFSET((N) + 1)
#define ESPI_UPCMD_HDATA_MASK REG_8BIT_MASK
#define ESPI_UPCMD_HDATA(N, VALUE) _GET_FVn(N, ESPI_UPCMD_HDATA, VALUE)

/* Configuration defines */
#define ESPI_CRC_CHECKING ESPI_GEN_CRC_ENABLE
#define ESPI_RSP_MODIFIER 0

#define ESPI_SLAVE_IO_MODE_SUP_QUAD(CAPS) (ESPI_GEN_IO_MODE_SUP(CAPS) == ESPI_GEN_IO_MODE_QUAD)
#define ESPI_SLAVE_IO_MODE_SUP_DUAL(CAPS) (ESPI_GEN_IO_MODE_SUP(CAPS) == ESPI_GEN_IO_MODE_DUAL)
#define ESPI_SLAVE_FREQ_SUP_66MHZ(CAPS) (ESPI_GEN_OP_FREQ_SUP(CAPS) == ESPI_GEN_OP_FREQ_66MHZ)
#define ESPI_SLAVE_OP_FREQ_SUP_50MHZ(CAPS) (ESPI_GEN_OP_FREQ_SUP(CAPS) == ESPI_GEN_OP_FREQ_50MHZ)
#define ESPI_SLAVE_OP_FREQ_SUP_33MHZ(CAPS) (ESPI_GEN_OP_FREQ_SUP(CAPS) == ESPI_GEN_OP_FREQ_33MHZ)
#define ESPI_SLAVE_OP_FREQ_SUP_25MHZ(CAPS) (ESPI_GEN_OP_FREQ_SUP(CAPS) == ESPI_GEN_OP_FREQ_25MHZ)
#define ESPI_SLAVE_OP_FREQ_SUP_20MHZ(CAPS) (ESPI_GEN_OP_FREQ_SUP(CAPS) == ESPI_GEN_OP_FREQ_20MHZ)
#define ESPI_SLAVE_OP_FREQ_SUP_MAX(CAPS)                                                           \
	(ESPI_SLAVE_FREQ_SUP_66MHZ(CAPS)      ? 66                                                 \
	 : ESPI_SLAVE_OP_FREQ_SUP_50MHZ(CAPS) ? 50                                                 \
	 : ESPI_SLAVE_OP_FREQ_SUP_33MHZ(CAPS) ? 33                                                 \
	 : ESPI_SLAVE_OP_FREQ_SUP_25MHZ(CAPS) ? 25                                                 \
					      : 20)

/*SLAVE0_INT_STS*/
#define ESPI_FLASH_REQ_INT BIT(31)
#define ESPI_RXOOB_INT BIT(30)
#define ESPI_RXMSG_INT BIT(29)
#define ESPI_DNCMD_INT BIT(28)
#define ESPI_RXVW_GRP3_INT BIT(27)
#define ESPI_RXVW_GRP2_INT BIT(26)
#define ESPI_RXVW_GRP1_INT BIT(25)
#define ESPI_RXVW_GRP0_INT BIT(24)
#define ESPI_CALLBACK_INT                                                                          \
	(ESPI_FLASH_REQ_INT | ESPI_RXOOB_INT | ESPI_RXMSG_INT | ESPI_DNCMD_INT |                   \
	 ESPI_RXVW_GRP3_INT | ESPI_RXVW_GRP2_INT | ESPI_RXVW_GRP1_INT | ESPI_RXVW_GRP0_INT)
#define ESPI_PROTOCOL_ERR_INT BIT(15)
#define ESPI_RXFLASH_OFLOW_INT BIT(14)
#define ESPI_RXMSG_OFLOW_INT BIT(13)
#define ESPI_RXOOB_OFLOW_INT BIT(12)
#define ESPI_ILLEGAL_LEN_INT BIT(11)
#define ESPI_ILLEGAL_TAG_INT BIT(10)
#define ESPI_UNSUCSS_CPL_INT BIT(9)
#define ESPI_INVALID_CT_RSP_INT BIT(8)
#define ESPI_UNKNOWN_RSP_INT BIT(7)
#define ESPI_CRC_ERR_INT BIT(2)
#define ESPI_WAIT_TIMEOUT_INT BIT(1)
#define ESPI_BUS_ERR_INT BIT(0)
#define ESPI_PROTOCOL_INT                                                                          \
	(ESPI_PROTOCOL_ERR_INT | ESPI_RXFLASH_OFLOW_INT | ESPI_RXMSG_OFLOW_INT |                   \
	 ESPI_RXOOB_OFLOW_INT | ESPI_ILLEGAL_LEN_INT | ESPI_ILLEGAL_TAG_INT |                      \
	 ESPI_UNSUCSS_CPL_INT | ESPI_INVALID_CT_RSP_INT | ESPI_UNKNOWN_RSP_INT |                   \
	 ESPI_CRC_ERR_INT | ESPI_WAIT_TIMEOUT_INT | ESPI_BUS_ERR_INT)
#define ESPI_NON_FATAL_INT BIT(6)
#define ESPI_FATAL_ERR_INT BIT(5)
#define ESPI_NO_RSP_INT BIT(4)
#define ESPI_RESPONSE_INT (ESPI_NON_FATAL_INT | ESPI_FATAL_ERR_INT | ESPI_NO_RSP_INT)
#define ESPI_ALL_INT (ESPI_CALLBACK_INT | ESPI_PROTOCOL_INT | ESPI_RESPONSE_INT)
#define ESPI_CYCLE_TYPE_OOB_MESSAGE 0x21
#define ESPI_PERI_SHORT_MEM32_HDR_LEN 4
#define ESPI_PERI_SHORT_IO32_HDR_LEN 2
#define ESPI_PERI_MEM32_HDR_LEN 7
#define ESPI_PERI_MEM64_HDR_LEN 11
#define ESPI_PERI_MESSAGE_HDR_LEN 8
#define ESPI_PERI_COMPLETION_HDR_LEN 3
#define ESPI_OOB_MESSAGE_HDR_LEN 3
#define ESPI_FLASH_ACCESS_REQUEST_HDR_LEN 7
#define ESPI_FLASH_ACCESS_COMPLETION_HDR_LEN 3
#define ESPI_MST_STOP_EN BIT(3)
#define ESPI_CONTROL_WATCHDOG_EN BIT(0)
#define ESPI_CONTROL_WATCHDOG_CNT_OFFSET 8
#define ESPI_CONTROL_WATCHDOG_CNT_MASK REG_16BIT_MASK
#define ESPI_CONTROL_WATCHDOG_CNT(VALUE) _SET_FV(ESPI_CONTROL_WATCHDOG_CNT, VALUE);
#define ESPI_CONTROL_WATCHDOG_IDEL_CNT_OFFSET 4
#define ESPI_CONTROL_WATCHDOG_IDEL_CNT_MASK REG_3BIT_MASK
#define ESPI_CONTROL_WATCHDOG_IDEL_CNT(VALUE) _SET_FV(ESPI_CONTROL_WATCHDOG_IDEL_CNT, VALUE);
#define ESPI_CONTROL_WATCHDOG_WAIT_CNT_OFFSET 24
#define ESPI_CONTROL_WATCHDOG_WAIT_CNT_MASK REG_6BIT_MASK
#define ESPI_CONTROL_WATCHDOG_WAIT_CNT(VALUE) _SET_FV(ESPI_CONTROL_WATCHDOG_WAIT_CNT, VALUE);
#define ESPI_PR_BASE_ADDR_MEM_VALUE 0xB4000000
#define ESPI_SYSTEM_EVENT_STATUS BIT(29)
#define ESPI_SUSPEND_WARN_ACK BIT(0)

/* Capabilities of the channels */
#define SPACEMIT_ESPI_VW_COUNT 16
#define SPACEMIT_ESPI_OOB_SIZE 128
#define SPACEMIT_ESPI_FLASH_SIZE 128
#define SPACEMIT_ESPI_PR_SIZE 64
#define SPACEMIT_ESPI_GPIO_GROUPS 4
#define ESPI_MEM_SIZE SZ_16M
#define ESPI_HW_PERI_SIZE SPACEMIT_ESPI_PR_SIZE
#define ESPI_HW_VWIRE_COUNT SPACEMIT_ESPI_VW_COUNT
#define ESPI_HW_OOB_SIZE SPACEMIT_ESPI_OOB_SIZE
#define ESPI_HW_FLASH_SIZE SPACEMIT_ESPI_FLASH_SIZE
#define ESPI_HW_GPIO_GROUPS SPACEMIT_ESPI_GPIO_GROUPS
#define ESPI_RESPONSE_MODIFIER_OFFSET 6
#define ESPI_RESPONSE_MODIFIER_MASK REG_2BIT_MASK
#define ESPI_RESPONSE_MODIFIER(mod) _SET_FV(ESPI_RESPONSE_MODIFIER, mod)
#define ESPI_RSP_NRESPONSE ESPI_RESPONSE_MODIFIER(0x3)
#define ESPI_RSP_RESPONSE ESPI_RESPONSE_MODIFIER(0x0)
#define ESPI_RSP_ACCEPT(r) (ESPI_RESPONSE_MODIFIER(r) | 0x08)
#define ESPI_RSP_DEFER (ESPI_RSP_RESPONSE | 0x01)
#define ESPI_RSP_NON_FATAL_ERROR (ESPI_RSP_RESPONSE | 0x02)
#define ESPI_RSP_FATAL_ERROR (ESPI_RSP_RESPONSE | 0x03)
#define ESPI_RSP_WAIT_STATE (ESPI_RSP_RESPONSE | 0x0f)
#define ESPI_RSP_NO_RESPONSE (ESPI_RSP_NRESPONSE | 0x0f)
#define ESPI_VWIRE_INTERRUPT 0
#define ESPI_VWIRE_SYSTEM 1
#define ESPI_VWIRE_GPIO 2
#define ESPI_VWIRE_TYPE_OFFSET 11
#define ESPI_VWIRE_TYPE_MASK REG_2BIT_MASK
#define espi_vwire_type(value) _SET_FV(ESPI_VWIRE_TYPE, value)
#define ESPI_VWIRE_TYPE(value) _GET_FV(ESPI_VWIRE_TYPE, value)
#define ESPI_VWIRE_SYSTEM_EVENT_GROUP_MIN 2
#define ESPI_VWIRE_SYSTEM_EVENT_GROUP_MAX 7
#define espi_vwire_system_group(value) _SET_FV(ESPI_VWIRE_SYSTEM_EVENT_GROUP, value)
#define ESPI_VWIRE_SYSTEM_GROUP(value) _GET_FV(ESPI_VWIRE_SYSTEM_EVENT_GROUP, value)

/* System events */
#define ESPI_VWIRE_SYSTEM_LEVEL_HIGH(evt) _BV(evt)
#define ESPI_VWIRE_SYSTEM_VALID(evt) _BV((evt) + 4)
#define ESPI_VWIRE_SYSTEM_EVENT_HIGH(x)                                                            \
	(ESPI_VWIRE_SYSTEM_VALID(x) | ESPI_VWIRE_SYSTEM_LEVEL_HIGH(x))
#define ESPI_VWIRE_SYSTEM_EVENT_LOW(x) (ESPI_VWIRE_SYSTEM_VALID(x))
#define ESPI_VWIRE_SYSTEM_EVENT_GROUP_OFFSET 4
#define ESPI_VWIRE_SYSTEM_EVENT_GROUP_MASK REG_7BIT_MASK
#define ESPI_VWIRE_SYSTEM_EVENT_GROUP_MIN 2
#define ESPI_VWIRE_SYSTEM_EVENT_GROUP_MAX 7
#define espi_vwire_system_group(value) _SET_FV(ESPI_VWIRE_SYSTEM_EVENT_GROUP, value)
#define ESPI_VWIRE_SYSTEM_GROUP(value) _GET_FV(ESPI_VWIRE_SYSTEM_EVENT_GROUP, value)
#define ESPI_VWIRE_SYSTEM_EVENT_VWIRE_OFFSET 0
#define ESPI_VWIRE_SYSTEM_EVENT_VWIRE_MASK REG_4BIT_MASK
#define espi_vwire_system_vwire(value) _SET_FV(ESPI_VWIRE_SYSTEM_EVENT_VWIRE, value)
#define ESPI_VWIRE_SYSTEM_VWIRE(value) _GET_FV(ESPI_VWIRE_SYSTEM_EVENT_VWIRE, value)
#define ESPI_VWIRE_SYSTEM_EVENT(group, vwire)                                                      \
	(espi_vwire_type(ESPI_VWIRE_SYSTEM) | espi_vwire_system_group(group) |                     \
	 espi_vwire_system_vwire(vwire))
#define ESPI_VWIRE_SYSTEM_SLP_S5 ESPI_VWIRE_SYSTEM_EVENT(2, 2)
#define ESPI_VWIRE_SYSTEM_SLP_S4 ESPI_VWIRE_SYSTEM_EVENT(2, 1)
#define ESPI_VWIRE_SYSTEM_SLP_S3 ESPI_VWIRE_SYSTEM_EVENT(2, 0)
#define ESPI_VWIRE_SYSTEM_OOB_RST_WARN ESPI_VWIRE_SYSTEM_EVENT(3, 2)
#define ESPI_VWIRE_SYSTEM_PLTRST ESPI_VWIRE_SYSTEM_EVENT(3, 1)
#define ESPI_VWIRE_SYSTEM_SUS_STAT ESPI_VWIRE_SYSTEM_EVENT(3, 0)
#define ESPI_VWIRE_SYSTEM_PME ESPI_VWIRE_SYSTEM_EVENT(4, 3)
#define ESPI_VWIRE_SYSTEM_WAKE ESPI_VWIRE_SYSTEM_EVENT(4, 2)
#define ESPI_VWIRE_SYSTEM_OOB_RST_ACK ESPI_VWIRE_SYSTEM_EVENT(4, 0)
#define ESPI_VWIRE_SYSTEM_SLV_BOOT_LOAD_STATUS ESPI_VWIRE_SYSTEM_EVENT(5, 3)
#define ESPI_VWIRE_SYSTEM_ERROR_NONFATAL ESPI_VWIRE_SYSTEM_EVENT(5, 2)
#define ESPI_VWIRE_SYSTEM_ERROR_FATAL ESPI_VWIRE_SYSTEM_EVENT(5, 1)
#define ESPI_VWIRE_SYSTEM_SLV_BOOT_LOAD_DONE ESPI_VWIRE_SYSTEM_EVENT(5, 0)
#define ESPI_VWIRE_SYSTEM_HOST_RST_ACK ESPI_VWIRE_SYSTEM_EVENT(6, 3)
#define ESPI_VWIRE_SYSTEM_RCIN ESPI_VWIRE_SYSTEM_EVENT(6, 2)
#define ESPI_VWIRE_SYSTEM_SMI ESPI_VWIRE_SYSTEM_EVENT(6, 1)
#define ESPI_VWIRE_SYSTEM_SCI ESPI_VWIRE_SYSTEM_EVENT(6, 0)
#define ESPI_VWIRE_SYSTEM_NMIOUT ESPI_VWIRE_SYSTEM_EVENT(7, 2)
#define ESPI_VWIRE_SYSTEM_SMIOUT ESPI_VWIRE_SYSTEM_EVENT(7, 1)
#define ESPI_VWIRE_SYSTEM_HOST_RST_WARN ESPI_VWIRE_SYSTEM_EVENT(7, 0)
#define ESPI_VWIRE_SYSTEM_UNKNOWN ESPI_VWIRE_SYSTEM_EVENT(0, 0)
#define ESPI_VWIRE_IS_SYSTEM_EVENT(group)                                                          \
	(((group) <= ESPI_VWIRE_SYSTEM_EVENT_GROUP_MAX) & !ESPI_VWIRE_IS_INTERRUPT_EVENT(group))

/* GPIO expander */
#define ESPI_VWIRE_GPIO_LEVEL_HIGH(evt) _BV(evt)
#define ESPI_VWIRE_GPIO_VALID(evt) _BV((evt) + 4)
#define ESPI_VWIRE_GPIO_EXPANDER_HIGH(x) (ESPI_VWIRE_GPIO_VALID(x) | ESPI_VWIRE_GPIO_LEVEL_HIGH(x))
#define ESPI_VWIRE_GPIO_EXPANDER_LOW(x) (ESPI_VWIRE_GPIO_VALID(x))
#define ESPI_VWIRE_GPIO_EXPANDER_GROUP_OFFSET 4
#define ESPI_VWIRE_GPIO_EXPANDER_GROUP_MASK REG_7BIT_MASK
#define ESPI_VWIRE_GPIO_EXPANDER_GROUP_MIN 128
#define ESPI_VWIRE_GPIO_EXPANDER_GROUP_MAX                                                         \
	(ESPI_VWIRE_GPIO_EXPANDER_GROUP_MIN +                                                      \
	 (ESPI_HW_GPIO_GROUPS - 1)) // 这里数组是从下标0开始，所以需要减1
#define espi_vwire_gpio_group(value) _SET_FV(ESPI_VWIRE_GPIO_EXPANDER_GROUP, (value) - 128)
#define ESPI_VWIRE_GPIO_GROUP(value) (_GET_FV(ESPI_VWIRE_GPIO_EXPANDER_GROUP, value) + 128)
#define ESPI_VWIRE_GPIO_EXPANDER_VWIRE_OFFSET 0
#define ESPI_VWIRE_GPIO_EXPANDER_VWIRE_MASK REG_4BIT_MASK
#define espi_vwire_gpio_vwire(value) _SET_FV(ESPI_VWIRE_GPIO_EXPANDER_VWIRE, value)
#define ESPI_VWIRE_GPIO_VWIRE(value) _GET_FV(ESPI_VWIRE_GPIO_EXPANDER_VWIRE, value)
#define ESPI_VWIRE_GPIO_EXPANDER(group, vwire)                                                     \
	(espi_vwire_type(ESPI_VWIRE_GPIO) | espi_vwire_gpio_group(group) |                         \
	 espi_vwire_gpio_vwire(vwire))

/* SLAVE0_RXVW_STS */
#define SLAVE0_RXVW_STS_SYS_EVT_STS _BV(29)
#define SLAVE0_RXVW_STS_IRQ_STS(n) _BV(n)

/* eSPI Channel Definitions */
#define ESPI_CH_PERIPHERAL BIT(0)
#define ESPI_CH_VWIRE BIT(1)
#define ESPI_CH_OOB BIT(2)
#define ESPI_CH_FLASH BIT(3)
/* eSPI Maximum Sizes */
#define ESPI_PERI_PAYLOAD_SIZE_MAX 1024
#define ESPI_OOB_PAYLOAD_SIZE_MAX 1024
#define ESPI_FLASH_PAYLOAD_SIZE_MAX 1024
#define ESPI_FLASH_READ_REQ_SIZE_MAX 1024
/* eSPI IO Modes */
#define ESPI_IO_MODE_SINGLE 0
#define ESPI_IO_MODE_DUAL 1
#define ESPI_IO_MODE_QUAD 2
/* eSPI Operating Frequencies */
#define ESPI_FREQ_20MHZ 0
#define ESPI_FREQ_25MHZ 1
#define ESPI_FREQ_33MHZ 2
#define ESPI_FREQ_50MHZ 3
#define ESPI_FREQ_66MHZ 4

/* eSPI poll status types */
enum espi_poll_type {
	ESPI_POLL_DNCMD,
	ESPI_POLL_FLASH_REQ,
	ESPI_POLL_RXOOB,
	ESPI_POLL_RXMSG,
};

/* Function prototypes for U-Boot */
int espi_mem_cfg(void);
void espi_config_clk_freq(u32 freq);
void espi_config_gen_io_mode(u32 mode);
void espi_master_init(phys_addr_t espi_cfg_base);
void espi_inband_reset(void);
void espi_get_config(u16 slave_address);
void espi_set_config(u16 slave_address, u32 configs);
void espi_slave_get_config(u16 slave_address);
void espi_controller_config_apply(u32 slave_id, u16 slave_address);
void espi_hw_get_config_rsp(u8 opcode, u8 head_len, u8 *head_buf, u8 data_len, u8 *data_buf);
int espi_poll_status_mask(u32 *status, u32 expected);
int espi_poll_status(u32 *status, enum espi_poll_type type);
int espi_rx_oob(u8 *buffer);
int espi_tx_oob(uint16_t len, uint8_t *buf);
int espi_tx_vw(u16 vwire, bool state);
int espi_rx_vw(u16 vwire, bool *state);
phys_addr_t espi_get_cfg_base(void);
void espi_put_vwire(u16 vwire, bool state);
void set_bit(int nr, volatile unsigned long *addr);
void clear_bit(int nr, volatile unsigned long *addr);
void espi_config_vwgpio(struct spacemit_espi_priv *priv);

/* Library function declarations */
int espi_tx_vwire(u16 vwire, bool state);
int espi_rx_vwire(u16 vwire, bool *state);
int espi_tx_flash_read(u32 addr, void *data, size_t len);
int espi_tx_flash_write(u32 addr, const void *data, size_t len);
int espi_tx_flash_erase(u32 addr, size_t len);

/* Global configuration variables */
extern unsigned long espi_gpios[BITS_TO_LONGS(32)];
extern struct spacemit_espi_priv *g_espi_priv;

#endif /* __SPACEMIT_ESPI_CMD_LIB_ */
