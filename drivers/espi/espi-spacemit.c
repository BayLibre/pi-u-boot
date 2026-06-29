/*
 Spacemit eSPI master driver.
 Copyright (c) 2025, SpacemiT Co., Ltd. All rights reserved.
 SPDX-License-Identifier: BSD-2-Clause-Patent
*/

#include <dm.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <malloc.h>
#include <dm/device_compat.h>
#include <dm/devres.h>
#include <dm/ofnode.h>
#include <espi.h>
#include <fdt_support.h>
#include <clk.h>
#include "espi-spacemit.h"

/* Global pointer to current eSPI instance */
struct spacemit_espi_priv *g_espi_priv = NULL;
extern u32 espi_gen_config;
extern u32 espi_peri_config;
extern u32 espi_vw_config;
extern u32 espi_oob_config;
extern u32 espi_flash_config;
extern u32 espi_sys_evt;
extern u32 espi_sys_evt_active_lows;

/**
 * spacemit_espi_read32 - Read 32-bit value from eSPI register
 * @priv: Pointer to the SpacemiT eSPI private data
 * @offset: Register offset
 * @return: 32-bit register value
 */
u32 spacemit_espi_read32(struct spacemit_espi_priv *priv, u32 offset)
{
	return readl((void *) (priv->cfg_base + offset));
}

/**
 * spacemit_espi_write32 - Write 32-bit value to eSPI register
 * @priv: Pointer to the SpacemiT eSPI private data
 * @value: Value to write
 * @offset: Register offset
 */
void spacemit_espi_write32(struct spacemit_espi_priv *priv, u32 value, u32 offset)
{
	writel(value, (void *) (priv->cfg_base + offset));
}

/**
 * spacemit_espi_is_ready - Check if eSPI controller is fully initialized
 *
 * This function checks if the eSPI controller has completed all
 * initialization steps successfully. It should be used by other
 * drivers (like cros_ec) to determine if eSPI is available.
 *
 * Return: true if eSPI is fully initialized and ready, false otherwise
 */
bool spacemit_espi_is_ready(void)
{
	if (!g_espi_priv)
		return false;

	return g_espi_priv->initialized && g_espi_priv->slave_gen_caps != 0;
}

/**
 * Initialize eSPI controller from device tree
 * @priv: driver private data
 * @dev: device instance
 * @return: 0 on success, negative on error
 */
static int spacemit_espi_init_from_dt(struct spacemit_espi_priv *priv, struct udevice *dev)
{
	ofnode node = dev_ofnode(dev);
	const char *str_val;
	/* Get base addresses from device tree */
	priv->cfg_base = dev_read_addr_index(dev, 0);
	if (priv->cfg_base == FDT_ADDR_T_NONE) {
		dev_err(dev, "Failed to get cfg base address\n");
		return -EINVAL;
	}

	/* Parse operating frequency */
	if (ofnode_read_u32(node, "operating-frequency", &priv->op_freq)) {
		priv->op_freq = ESPI_GEN_OP_FREQ_20MHZ; /* Default to 20MHz */
		dev_dbg(dev, "Using default operating frequency: 20MHz\n");
	}

	/* Parse IO mode */
	str_val = ofnode_read_string(node, "io-mode");
	if (str_val) {
		if (strcmp(str_val, "single") == 0) {
			priv->io_mode = ESPI_GEN_IO_MODE_SINGLE;
		} else if (strcmp(str_val, "dual") == 0) {
			priv->io_mode = ESPI_GEN_IO_MODE_DUAL;
		} else if (strcmp(str_val, "quad") == 0) {
			priv->io_mode = ESPI_GEN_IO_MODE_QUAD;
		} else {
			dev_warn(dev, "Invalid io-mode '%s', using single\n", str_val);
			priv->io_mode = ESPI_GEN_IO_MODE_SINGLE;
		}
	} else {
		priv->io_mode = ESPI_GEN_IO_MODE_SINGLE; /* Default */
		dev_warn(dev, "io-mode not specified, using single\n");
	}

	/* Parse alert mode */
	str_val = ofnode_read_string(node, "alert-mode");
	if (str_val) {
		if (strcmp(str_val, "pin") == 0) {
			priv->alert_mode = ESPI_GEN_ALERT_MODE_PIN;
		} else if (strcmp(str_val, "io1") == 0) {
			priv->alert_mode = ESPI_GEN_ALERT_MODE_IO1;
		} else {
			dev_warn(dev, "Invalid alert-mode '%s', using pin\n", str_val);
			priv->alert_mode = ESPI_GEN_ALERT_MODE_PIN;
		}
	} else {
		priv->alert_mode = ESPI_GEN_ALERT_MODE_PIN; /* Default */
		dev_warn(dev, "alert-mode not specified, using pin\n");
	}

	/* Parse alert type only when alert-mode is PIN */
	if (priv->alert_mode == ESPI_GEN_ALERT_MODE_PIN) {
		str_val = ofnode_read_string(node, "alert-type");
		if (str_val) {
			if (strcmp(str_val, "open-drain") == 0) {
				priv->alert_type = ESPI_GEN_ALERT_TYPE_OD;
			} else {
				priv->alert_type = 0; /* Push-pull */
			}
		} else {
			priv->alert_type = 0; /* Default to push-pull */
			/* Only warn when PIN mode in use and type missing */
			dev_warn(dev, "alert-type not specified, using push-pull\n");
		}
	} else {
		/* IO1 mode ignores alert-type; default to push-pull for consistency */
		priv->alert_type = 0;
	}

	/* Parse boolean properties */
	priv->crc_enable = ofnode_read_bool(node, "crc-enable");
	priv->resp_modifier_enable = ofnode_read_bool(node, "response-modifier-enable");
	priv->peri_chan_enable = ofnode_read_bool(node, "peripheral-channel-enable");
	priv->vwire_chan_enable = ofnode_read_bool(node, "vwire-channel-enable");
	priv->oob_chan_enable = ofnode_read_bool(node, "oob-channel-enable");
	priv->flash_chan_enable = ofnode_read_bool(node, "flash-channel-enable");
	priv->auto_gating_enable = ofnode_read_bool(node, "auto-gating-enable");

	/* Parse VW GPIO mappings */
	priv->vw_gpio_count = 0;
	if (ofnode_read_bool(node, "vw-gpio-mapping")) {
		int i;
		const u32 *vw_gpio_prop;
		int len;

		vw_gpio_prop = ofnode_read_prop(node, "vw-gpio-mapping", &len);
		if (vw_gpio_prop && len > 0) {
			int num_mappings = len / (2 * sizeof(u32));
			if (num_mappings > 8) {
				dev_warn(dev, "Too many VW GPIO mappings (%d), using first 8\n",
					 num_mappings);
				num_mappings = 8;
			}

			for (i = 0; i < num_mappings; i++) {
				priv->vw_gpio_map[i][0] =
					fdt32_to_cpu(vw_gpio_prop[i * 2]); /* Group */
				priv->vw_gpio_map[i][1] =
					fdt32_to_cpu(vw_gpio_prop[i * 2 + 1]); /* Index */
			}
			priv->vw_gpio_count = num_mappings;

			dev_info(dev, "VW GPIO mappings: %d configured\n", priv->vw_gpio_count);
			for (i = 0; i < priv->vw_gpio_count; i++) {
				dev_info(dev, "  [%d]: Group %d -> VM Index 0x%02x\n", i,
					 priv->vw_gpio_map[i][0], priv->vw_gpio_map[i][1]);
			}
		}
	}

	/* Parse timing parameters */
	if (ofnode_read_u32(node, "watchdog-timeout", &priv->watchdog_timeout)) {
		priv->watchdog_timeout = 0xFFFF; /* Default timeout */
	}

	/* Parse PR memory base addresses from device tree properties */
	if (ofnode_read_u32(node, "pr-mem-base0", &priv->pr_mem_base0)) {
		dev_warn(dev, "pr-mem-base0 not specified, using mem_base: 0x%08x\n",
			 priv->pr_mem_base0);
	}
	if (ofnode_read_u32(node, "pr-mem-base1", &priv->pr_mem_base1)) {
		dev_warn(dev, "pr-mem-base1 not specified, using base0+16MB: 0x%08x\n",
			 priv->pr_mem_base1);
	}

	dev_dbg(dev, "eSPI Configuration:\n");
	dev_dbg(dev, "  Operating frequency: %d\n", priv->op_freq);
	dev_dbg(dev, "  IO mode: %d\n", priv->io_mode);
	dev_dbg(dev, "  Alert mode: %d\n", priv->alert_mode);
	dev_dbg(dev, "  CRC enable: %d\n", priv->crc_enable);
	dev_dbg(dev, "  Channels: PERI=%d VW=%d OOB=%d FLASH=%d\n", priv->peri_chan_enable,
		priv->vwire_chan_enable, priv->oob_chan_enable, priv->flash_chan_enable);
	dev_dbg(dev, "  Memory addresses:\n");
	dev_dbg(dev, "    PR mem base0: 0x%08x (16MB)\n", priv->pr_mem_base0);
	dev_dbg(dev, "    PR mem base1: 0x%08x (16MB)\n", priv->pr_mem_base1);
	dev_dbg(dev, "    PR IO base:   0x%08x\n", priv->pr_io_base);
	dev_dbg(dev, "    Reg cfg base: 0x%08llx\n", (unsigned long long) priv->cfg_base);
	return 0;
}

/**
 * Configure basic eSPI features (CRC and response modifier)
 * @priv: driver private data
 * @negotiated_config: pointer to negotiated configuration to update
 */
static void espi_configure_basic_features(struct spacemit_espi_priv *priv, u32 *negotiated_config)
{
	/* CRC support negotiation */
	if (priv->crc_enable) {
		*negotiated_config |= ESPI_CRC_CHECKING;
		debug("CRC checking enabled\n");
	}
	/* Response modifier support */
	if (priv->resp_modifier_enable) {
		*negotiated_config |= ESPI_RSP_MODIFIER;
		debug("Response modifier enabled\n");
	}
}

/**
 * Configure eSPI alert mode and type
 * @priv: driver private data
 * @slave_gen_caps: slave general capabilities
 * @negotiated_config: pointer to negotiated configuration to update
 */
static void espi_configure_alert_mode(struct spacemit_espi_priv *priv, u32 slave_gen_caps,
				      u32 *negotiated_config)
{
	if (priv->alert_mode == ESPI_GEN_ALERT_MODE_PIN) {
		*negotiated_config |= ESPI_GEN_ALERT_MODE_PIN;
		/* Configure alert type for PIN mode */
		if (priv->alert_type == ESPI_GEN_ALERT_TYPE_OD) {
			if (slave_gen_caps & ESPI_GEN_OPEN_DRAIN_ALERT_SUP) {
				*negotiated_config |= ESPI_GEN_OPEN_DRAIN_ALERT_SEL;
				debug("Negotiated: PIN mode with open-drain alert\n");
			} else {
				debug("Slave doesn't support open-drain, using push-pull alert\n");
			}
		} else {
			debug("Negotiated: PIN mode with push-pull alert\n");
		}
	} else {
		/* IO1 alert mode */
		*negotiated_config |= ESPI_GEN_ALERT_MODE_IO1;
		debug("Negotiated: IO1 alert mode\n");
	}
}

/**
 * Negotiate eSPI operating frequency with slave device
 * Choose the highest common frequency supported by both master and slave
 * @priv: driver private data
 * @slave_gen_caps: slave general capabilities
 * @negotiated_config: pointer to negotiated configuration to update
 */
static void espi_configure_frequency(struct spacemit_espi_priv *priv, u32 slave_gen_caps,
				     u32 *negotiated_config)
{
	u32 slave_max_freq = ESPI_SLAVE_OP_FREQ_SUP_MAX(slave_gen_caps);
	u32 desired_freq = priv->op_freq;
	u32 negotiated_freq = ESPI_GEN_OP_FREQ_20MHZ; /* Default to 20MHz */
	const char *freq_str;

	/* Negotiate from highest to lowest frequency */
	if (desired_freq == ESPI_GEN_OP_FREQ_66MHZ && slave_max_freq >= 66) {
		negotiated_freq = ESPI_GEN_OP_FREQ_66MHZ;
		freq_str = "66MHz";
	} else if (desired_freq >= ESPI_GEN_OP_FREQ_50MHZ && slave_max_freq >= 50) {
		negotiated_freq = ESPI_GEN_OP_FREQ_50MHZ;
		freq_str = "50MHz";
	} else if (desired_freq >= ESPI_GEN_OP_FREQ_33MHZ && slave_max_freq >= 33) {
		negotiated_freq = ESPI_GEN_OP_FREQ_33MHZ;
		freq_str = "33MHz";
	} else if (desired_freq >= ESPI_GEN_OP_FREQ_25MHZ && slave_max_freq >= 25) {
		negotiated_freq = ESPI_GEN_OP_FREQ_25MHZ;
		freq_str = "25MHz";
	} else {
		negotiated_freq = ESPI_GEN_OP_FREQ_20MHZ;
		freq_str = "20MHz";
	}
	*negotiated_config |= ESPI_GEN_OP_FREQ_SEL(negotiated_freq);

	debug("Frequency negotiation: Desired=%d, Slave_max=%d, Negotiated=%s\n", desired_freq,
	      slave_max_freq, freq_str);
}

/**
 * Negotiate eSPI IO mode with slave device
 * Choose the highest common IO mode supported by both master and slave
 * @priv: driver private data
 * @slave_gen_caps: slave general capabilities
 * @negotiated_config: pointer to negotiated configuration to update
 */
static void espi_configure_io_mode(struct spacemit_espi_priv *priv, u32 slave_gen_caps,
				   u32 *negotiated_config)
{
	u32 negotiated_io_mode = ESPI_GEN_IO_MODE_SINGLE; /* Default to single */
	const char *mode_str;

	/* Negotiate from highest to lowest IO mode */
	debug("Slave IO mode support: Quad=%d, Dual=%d\n",
	      ESPI_SLAVE_IO_MODE_SUP_QUAD(slave_gen_caps),
	      ESPI_SLAVE_IO_MODE_SUP_DUAL(slave_gen_caps));
	if (priv->io_mode == ESPI_GEN_IO_MODE_QUAD && ESPI_SLAVE_IO_MODE_SUP_QUAD(slave_gen_caps)) {
		negotiated_io_mode = ESPI_GEN_IO_MODE_QUAD;
		mode_str = "Quad";
	} else if (priv->io_mode >= ESPI_GEN_IO_MODE_DUAL &&
		   ESPI_SLAVE_IO_MODE_SUP_DUAL(slave_gen_caps)) {
		negotiated_io_mode = ESPI_GEN_IO_MODE_DUAL;
		mode_str = "Dual";
	} else {
		negotiated_io_mode = ESPI_GEN_IO_MODE_SINGLE;
		mode_str = "Single";
	}
	*negotiated_config |= ESPI_GEN_IO_MODE_SET(negotiated_io_mode);

	debug("IO mode negotiation: Desired=%d, Negotiated=%s\n", priv->io_mode, mode_str);
}

/**
 * Configure peripheral channel based on capabilities and preferences
 * @priv: driver private data
 * @slave_gen_caps: slave general capabilities
 * @negotiated_config: pointer to negotiated configuration to update
 */
static void espi_configure_peripheral_channel(struct spacemit_espi_priv *priv, u32 slave_gen_caps,
					      u32 *negotiated_config)
{
	if (priv->peri_chan_enable && (slave_gen_caps & ESPI_GEN_PERI_CHAN_SUP)) {
		*negotiated_config |= ESPI_GEN_PERI_CHAN_SUP;
		espi_peri_config = ESPI_ENABLE_PR_CHANNEL;
		debug("Peripheral channel enabled\n");
		/* Send peripheral configuration to Slave immediately */
		espi_set_config(ESPI_SLAVE_PERI_CFG, espi_peri_config);
		debug("  Sent peripheral config to Slave: 0x%08x\n", espi_peri_config);
	} else {
		espi_peri_config = 0;
		if (priv->peri_chan_enable)
			debug("Peripheral channel disabled - slave doesn't support\n");
	}
}

/**
 * Configure virtual wire channel based on capabilities and preferences
 * @priv: driver private data
 * @slave_gen_caps: slave general capabilities
 * @negotiated_config: pointer to negotiated configuration to update
 */
static void espi_configure_vwire_channel(struct spacemit_espi_priv *priv, u32 slave_gen_caps,
					 u32 *negotiated_config)
{
	if (priv->vwire_chan_enable && (slave_gen_caps & ESPI_GEN_VWIRE_CHAN_SUP)) {
		*negotiated_config |= ESPI_GEN_VWIRE_CHAN_SUP;
		espi_vw_config = ESPI_ENABLE_VW_CHANNEL;
		debug("Virtual Wire channel enabled\n");
		/* Send VWire configuration to Slave immediately */
		espi_set_config(ESPI_SLAVE_VWIRE_CFG, espi_vw_config);
		debug("  Sent VWire config to Slave: 0x%08x\n", espi_vw_config);
	} else {
		espi_vw_config = 0;
		if (priv->vwire_chan_enable)
			debug("Virtual Wire channel disabled - slave doesn't support\n");
	}
}

/**
 * Configure OOB (Out-of-Band) channel based on capabilities and preferences
 * @priv: driver private data
 * @slave_gen_caps: slave general capabilities
 * @negotiated_config: pointer to negotiated configuration to update
 */
static void espi_configure_oob_channel(struct spacemit_espi_priv *priv, u32 slave_gen_caps,
				       u32 *negotiated_config)
{
	if (priv->oob_chan_enable && (slave_gen_caps & ESPI_GEN_OOB_CHAN_SUP)) {
		*negotiated_config |= ESPI_GEN_OOB_CHAN_SUP;
		espi_oob_config = ESPI_ENABLE_OOB_CHANNEL;
		debug("OOB channel enabled\n");
		/* Send OOB configuration to Slave immediately */
		espi_set_config(ESPI_SLAVE_OOB_CFG, espi_oob_config);
		debug("  Sent OOB config to Slave: 0x%08x\n", espi_oob_config);
	} else {
		espi_oob_config = 0;
		if (priv->oob_chan_enable)
			debug("OOB channel disabled - slave doesn't support\n");
	}
}

/**
 * Configure flash channel based on capabilities and preferences
 * @priv: driver private data
 * @slave_gen_caps: slave general capabilities
 * @negotiated_config: pointer to negotiated configuration to update
 */
static void espi_configure_flash_channel(struct spacemit_espi_priv *priv, u32 slave_gen_caps,
					 u32 *negotiated_config)
{
	if (priv->flash_chan_enable && (slave_gen_caps & ESPI_GEN_FLASH_CHAN_SUP)) {
		*negotiated_config |= ESPI_GEN_FLASH_CHAN_SUP;
		espi_flash_config = ESPI_ENABLE_FLASH_CHANNEL;
		debug("Flash channel enabled\n");
		/* Send Flash configuration to Slave immediately */
		espi_set_config(ESPI_SLAVE_FLASH_CFG, espi_flash_config);
		debug("  Sent Flash config to Slave: 0x%08x\n", espi_flash_config);
	} else {
		espi_flash_config = 0;
		if (priv->flash_chan_enable)
			debug("Flash channel disabled - slave doesn't support\n");
	}
}

static u32 espi_freq_mhz_to_sel(u32 freq_mhz)
{
	switch (freq_mhz) {
	case ESPI_GEN_OP_FREQ_66MHZ:
	case 66:
		return ESPI_GEN_OP_FREQ_66MHZ;
	case ESPI_GEN_OP_FREQ_50MHZ:
	case 50:
		return ESPI_GEN_OP_FREQ_50MHZ;
	case ESPI_GEN_OP_FREQ_33MHZ:
	case 33:
		return ESPI_GEN_OP_FREQ_33MHZ;
	case ESPI_GEN_OP_FREQ_25MHZ:
	case 25:
		return ESPI_GEN_OP_FREQ_25MHZ;
	case ESPI_GEN_OP_FREQ_20MHZ:
	case 20:
	default:
		return ESPI_GEN_OP_FREQ_20MHZ;
	}
}

static void spacemit_espi_build_master_only_config(struct spacemit_espi_priv *priv)
{
	u32 config = 0;
	u32 freq_sel;

	espi_configure_basic_features(priv, &config);

	if (priv->alert_mode == ESPI_GEN_ALERT_MODE_PIN) {
		config |= ESPI_GEN_ALERT_MODE_PIN;
		if (priv->alert_type == ESPI_GEN_ALERT_TYPE_OD)
			config |= ESPI_GEN_OPEN_DRAIN_ALERT_SEL;
	}

	config |= ESPI_GEN_IO_MODE_SET(priv->io_mode);
	freq_sel = espi_freq_mhz_to_sel(priv->op_freq);
	config |= (freq_sel & ESPI_GEN_OP_FREQ_SEL_MASK) << ESPI_GEN_OP_FREQ_SEL_OFFSET;

	if (priv->peri_chan_enable) {
		config |= ESPI_GEN_PERI_CHAN_SUP;
		espi_peri_config = ESPI_ENABLE_PR_CHANNEL;
	} else {
		espi_peri_config = 0;
	}

	if (priv->vwire_chan_enable) {
		config |= ESPI_GEN_VWIRE_CHAN_SUP;
		espi_vw_config = ESPI_ENABLE_VW_CHANNEL;
	} else {
		espi_vw_config = 0;
	}

	if (priv->oob_chan_enable) {
		config |= ESPI_GEN_OOB_CHAN_SUP;
		espi_oob_config = ESPI_ENABLE_OOB_CHANNEL;
	} else {
		espi_oob_config = 0;
	}

	if (priv->flash_chan_enable) {
		config |= ESPI_GEN_FLASH_CHAN_SUP;
		espi_flash_config = ESPI_ENABLE_FLASH_CHANNEL;
	} else {
		espi_flash_config = 0;
	}

	espi_gen_config = config;
}

/**
 * Negotiate eSPI configuration with slave device
 * @priv: driver private data
 * @return: 0 on success, negative on error
 */
static int spacemit_espi_negotiate_config(struct spacemit_espi_priv *priv)
{
	u32 status;
	u32 slave_gen_caps = 0;
	u32 negotiated_config = 0;
	int retry;
	int ret;

	debug("Starting eSPI configuration negotiation\n");
	/* Step 1: Get slave general capabilities with retry */
	for (retry = 0; retry < 5; retry++) {
		espi_get_config(ESPI_SLAVE_GEN_CFG);
		ret = espi_poll_status(&status, ESPI_POLL_DNCMD);
		if (ret == 0 && (status & ESPI_DNCMD_INT)) {
			u8 header[4];
			espi_hw_get_config_rsp(ESPI_CMD_GET_CONFIGURATION, 4, header, 0, NULL);
			slave_gen_caps = MAKELONG(MAKEWORD(header[0], header[1]),
						  MAKEWORD(header[2], header[3]));
			pr_info("Slave general capabilities: 0x%08x\n", slave_gen_caps);
			break;
		}
		debug("Retry %d: Failed to get slave capabilities, waiting...\n", retry + 1);
		mdelay(10);  /* Brief delay before retry - poll already has 50ms timeout */
	}

	if (retry >= 5) {
		debug("Failed to get slave general capabilities after retries\n");
		return -ETIMEDOUT;
	}

	priv->slave_gen_caps = slave_gen_caps;

	/* Step 2: Negotiate general configuration */
	negotiated_config = 0;
	espi_configure_basic_features(priv, &negotiated_config);
	espi_configure_alert_mode(priv, slave_gen_caps, &negotiated_config);
	espi_configure_frequency(priv, slave_gen_caps, &negotiated_config);
	espi_configure_io_mode(priv, slave_gen_caps, &negotiated_config);
	/* Channel support negotiation - only enable channels supported by both master and slave */
	espi_configure_peripheral_channel(priv, slave_gen_caps, &negotiated_config);
	espi_configure_vwire_channel(priv, slave_gen_caps, &negotiated_config);
	espi_configure_oob_channel(priv, slave_gen_caps, &negotiated_config);
	espi_configure_flash_channel(priv, slave_gen_caps, &negotiated_config);

	/* Has been sent to Slave in step2,need to check and update */
	espi_gen_config = negotiated_config;
	espi_set_config(ESPI_SLAVE_GEN_CFG, espi_gen_config);

	debug("eSPI configuration negotiation and Slave setup completed\n");
	return 0;
}

/**
 * Basic hardware initialization for eSPI controller
 * Only initialize to minimal working state for communication
 * @priv: driver private data
 * @return: 0 on success, negative on error
 */
static int espi_basic_hw_init(struct spacemit_espi_priv *priv)
{
	/* Only initialize master controller to enable communication */
	espi_master_init(priv->cfg_base);
	/* Send initial reset to prepare for negotiation */
	espi_inband_reset();
	/* Brief delay for reset signal to propagate */
	udelay(1000);
	return 0;
}

/**
 * Apply negotiated configuration to hardware
 * @priv: driver private data
 * @return: 0 on success, negative on error
 */
static int espi_apply_config(struct spacemit_espi_priv *priv)
{
	/* Apply general configuration to controller registers */
	espi_controller_config_apply(0, ESPI_SLAVE_GEN_CFG);
	debug("Controller hardware init completed successfully\n");
	return 0;
}

/**
 * Release eSPI controller system reset and configure clock
 * @dev: device instance
 * @return: 0 on success, negative on error
 */
static int espi_release_reset(struct udevice *dev)
{
	struct spacemit_espi_priv *priv = dev_get_priv(dev);
	ulong rate_hz;
	int ret;

	if (!priv->op_freq)
		return 0;

	rate_hz = (ulong)priv->op_freq * 1000000UL;
	ret = clk_set_rate(&priv->clk_sclk, rate_hz);
	if (ret)
		dev_dbg(dev, "Failed to set sclk rate to %lu Hz: %d\n", rate_hz, ret);

	/* Wait for clock stabilization */
	udelay(1000);

	return 0;
}

/* VW GPIO Register Operations */
/**
 * spacemit_espi_enable_vwgpio - Enable virtual wire GPIO group
 * @priv: Pointer to the SpacemiT eSPI private data
 * @group: Group number (0-3)
 */
void spacemit_espi_enable_vwgpio(struct spacemit_espi_priv *priv, int group)
{
	u32 vw_ctl;
	if (group < 0 || group > 3) {
		debug("Invalid VW group %d (valid range: 0-3)\n", group);
		return;
	}
	vw_ctl = spacemit_espi_read32(priv, ESPI_SLAVE0_VW_CTL);
	vw_ctl |= (1 << group);
	spacemit_espi_write32(priv, vw_ctl, ESPI_SLAVE0_VW_CTL);
	debug("Enabled VW group %d\n", group);
}

/**
 * spacemit_espi_config_vwgpio - Configure virtual wire GPIO mapping
 * @priv: Pointer to the SpacemiT eSPI private data
 *
 * This function configures the mapping between virtual wire groups
 * and specific GPIO indices based on the mappings stored in the priv
 * structure, then enables the virtual wire groups.
 */
void espi_config_vwgpio(struct spacemit_espi_priv *priv)
{
	int i;
	uint32_t rxvw_index_val = 0;
	uint32_t vw_grp_enable = 0;
	if (priv->vw_gpio_count <= 0) {
		debug("No virtual wire GPIO mappings to configure\n");
		return;
	}

	/* Print all mapping configurations for debugging */
	debug("VW GPIO mappings from device tree:\n");
	for (i = 0; i < priv->vw_gpio_count; i++) {
		debug("  [%d]: Group %d -> VM Index 0x%02x\n", i, priv->vw_gpio_map[i][0],
		      priv->vw_gpio_map[i][1]);
	}

	/* First read current register value */
	rxvw_index_val = spacemit_espi_read32(priv, ESPI_SLAVE0_RXVW_INDEX);
	debug("Current RXVW_INDEX: 0x%08x\n", rxvw_index_val);
	/* Build new register value */
	for (i = 0; i < priv->vw_gpio_count; i++) {
		uint8_t group = priv->vw_gpio_map[i][0];
		uint16_t index = priv->vw_gpio_map[i][1];
		uint32_t shift_offset;
		uint32_t mask;
		/* Validate group number range */
		if (group > 3) {
			debug("Invalid VW group %d (max 3) for mapping %d\n", group, i);
			continue;
		}
		debug("Processing mapping %d: Group %d -> VM Index 0x%02x\n", i, group, index);
		/* Calculate shift offset and mask */
		shift_offset = group * 8; /* Each group occupies 8 bits */
		mask = 0xFF << shift_offset;
		/* Clear old value and set new value for corresponding group */
		rxvw_index_val &= ~mask;
		rxvw_index_val |= ((uint32_t) index << shift_offset);
		/* Record groups to enable */
		vw_grp_enable |= (1 << group);
		debug("  Shift offset: %d, Mask: 0x%08x, New value: 0x%08x, Enable mask: 0x%08x\n",
		      shift_offset, mask, rxvw_index_val, vw_grp_enable);
	}

	/* Write all VM indexes at once */
	debug("Writing RXVW_INDEX: 0x%08x\n", rxvw_index_val);
	spacemit_espi_write32(priv, rxvw_index_val, ESPI_SLAVE0_RXVW_INDEX);
	/* Verify write operation */
	uint32_t readback = spacemit_espi_read32(priv, ESPI_SLAVE0_RXVW_INDEX);
	debug("RXVW_INDEX readback: 0x%08x\n", readback);
	/* Read current VW control register state */
	uint32_t vw_ctl = spacemit_espi_read32(priv, ESPI_SLAVE0_VW_CTL);
	debug("Current VW_CTL: 0x%08x\n", vw_ctl);

	/* Enable corresponding virtual wire groups */
	debug("Groups to enable: 0x%08x\n", vw_grp_enable);
	for (i = 0; i < 4; i++) {
		if (vw_grp_enable & (1 << i)) {
			debug("Enabling VW group %d\n", i);
			spacemit_espi_enable_vwgpio(priv, i);
			/* Verify if group is enabled successfully */
			uint32_t vw_ctl_after = spacemit_espi_read32(priv, ESPI_SLAVE0_VW_CTL);
			debug("VW_CTL after enabling group %d: 0x%08x\n", i, vw_ctl_after);
		}
	}

	/* Final state check */
	uint32_t final_rxvw_index = spacemit_espi_read32(priv, ESPI_SLAVE0_RXVW_INDEX);
	uint32_t final_vw_ctl = spacemit_espi_read32(priv, ESPI_SLAVE0_VW_CTL);
	uint32_t final_rxvw_data = spacemit_espi_read32(priv, ESPI_SLAVE0_RXVW_DATA);
	debug("Final register status:\n");
	debug("  RXVW_INDEX (0xA4): 0x%08x\n", final_rxvw_index);
	debug("  VW_CTL (0xA8):     0x%08x\n", final_vw_ctl);
	debug("  RXVW_DATA (0xA0):  0x%08x\n", final_rxvw_data);
	debug("VM Index configuration:\n");
	for (i = 0; i < 4; i++) {
		uint8_t vm_index = (final_rxvw_index >> (i * 8)) & 0xFF;
		bool enabled = !!(final_vw_ctl & (1 << i));
		debug("  Group %d: VM Index 0x%02x, %s\n", i, vm_index,
		      enabled ? "ENABLED" : "DISABLED");
	}
	debug("Configured %d virtual wire GPIO mappings successfully\n", priv->vw_gpio_count);
}

/* eSPI Ops Implementation */
static int spacemit_espi_hw_init(struct udevice *dev)
{
	struct spacemit_espi_priv *priv = dev_get_priv(dev);
	return espi_basic_hw_init(priv);
}

#ifndef CONFIG_XPL_BUILD
/* VWire, OOB, and Flash operations are not needed in SPL */

static int spacemit_espi_send_vwire(struct udevice *dev, u16 vwire, bool state)
{
	if (!dev)
		return -EINVAL;

	return espi_tx_vwire(vwire, state);
}

static int spacemit_espi_receive_vwire(struct udevice *dev, u16 vwire, bool *state)
{
	if (!dev || !state)
		return -EINVAL;

	return espi_rx_vwire(vwire, state);
}

static int spacemit_espi_flash_read(struct udevice *dev, u32 addr, void *data, size_t len)
{
	if (!dev || !data || len == 0)
		return -EINVAL;

	return espi_tx_flash_read(addr, data, len);
}

static int spacemit_espi_flash_write(struct udevice *dev, u32 addr, const void *data, size_t len)
{
	if (!dev || !data || len == 0)
		return -EINVAL;

	return espi_tx_flash_write(addr, data, len);
}

static int spacemit_espi_flash_erase(struct udevice *dev, u32 addr, size_t len)
{
	if (!dev || len == 0)
		return -EINVAL;

	return espi_tx_flash_erase(addr, len);
}

static int spacemit_espi_send_oob(struct udevice *dev, u8 *buf, size_t len)
{
	if (len > ESPI_OOB_PAYLOAD_SIZE_MAX) {
		return -EINVAL;
	}
	return espi_tx_oob(len, buf);
}

static int spacemit_espi_receive_oob(struct udevice *dev, u8 *buf, size_t len)
{
	u32 received_len;
	if (len > ESPI_OOB_PAYLOAD_SIZE_MAX) {
		return -EINVAL;
	}
	received_len = espi_rx_oob(buf);
	return (int) received_len;
}
#endif /* !CONFIG_XPL_BUILD */

/**
 * U-Boot driver model probe function
 */
static int spacemit_espi_probe(struct udevice *dev)
{
	struct spacemit_espi_priv *priv = dev_get_priv(dev);
	bool slave_ready = true;
	int ret;

	/* Step 0: Get and enable clocks */
	ret = clk_get_by_name(dev, "sclk_src", &priv->clk_sclk_src);
	if (ret) {
		dev_err(dev, "Failed to get sclk_src: %d\n", ret);
		return ret;
	}

	ret = clk_get_by_name(dev, "sclk", &priv->clk_sclk);
	if (ret) {
		dev_err(dev, "Failed to get sclk: %d\n", ret);
		return ret;
	}

	ret = clk_get_by_name(dev, "mclk", &priv->clk_mclk);
	if (ret) {
		dev_err(dev, "Failed to get mclk: %d\n", ret);
		return ret;
	}

	/* Enable source clock first */
	ret = clk_enable(&priv->clk_sclk_src);
	if (ret) {
		dev_err(dev, "Failed to enable sclk_src: %d\n", ret);
		return ret;
	}

	/* Set sclk parent to sclk_src (select mux) */
	ret = clk_set_parent(&priv->clk_sclk, &priv->clk_sclk_src);
	if (ret)
		dev_dbg(dev, "Failed to set sclk parent: %d (continuing anyway)\n", ret);

	/* Then enable sclk (which muxes from sclk_src) */
	ret = clk_enable(&priv->clk_sclk);
	if (ret) {
		dev_err(dev, "Failed to enable sclk: %d\n", ret);
		goto err_disable_sclk_src;
	}

	ret = clk_enable(&priv->clk_mclk);
	if (ret) {
		dev_err(dev, "Failed to enable mclk: %d\n", ret);
		goto err_disable_sclk;
	}

	/* Step 0.5: Get and deassert reset */
	ret = reset_get_by_name(dev, "espi_reset", &priv->reset_espi);
	if (ret) {
		dev_dbg(dev, "Failed to get espi_reset: %d (continuing anyway)\n", ret);
	} else {
		ret = reset_deassert(&priv->reset_espi);
		if (ret) {
			dev_err(dev, "Failed to deassert eSPI reset: %d\n", ret);
			goto err_disable_clocks;
		}
		udelay(100);
	}

	/* Step 1: Parse device tree configuration (need op_freq before reset config) */
	ret = spacemit_espi_init_from_dt(priv, dev);
	if (ret) {
		dev_err(dev, "Failed to initialize from device tree\n");
		goto err_disable_clocks;
	}

	/* Step 2: Release eSPI controller system reset and configure clock */
	ret = espi_release_reset(dev);
	if (ret) {
		dev_err(dev, "Failed to release eSPI controller reset\n");
		goto err_disable_clocks;
	}

	/* Step 3: Set global pointer for legacy functions */
	g_espi_priv = priv;

	/* Step 4: Basic hardware initialization for communication */
	ret = espi_basic_hw_init(priv);
	if (ret) {
		dev_err(dev, "Basic hardware initialization failed\n");
		goto err_disable_clocks;
	}

	/* Step 5: Negotiate configuration with slave device */
	ret = spacemit_espi_negotiate_config(priv);
	if (ret) {
		dev_warn(dev,
			 "eSPI slave negotiation failed (%d), continue with master-only init\n",
			 ret);
		spacemit_espi_build_master_only_config(priv);
		slave_ready = false;
	}

	/* Step 6: Apply negotiated configuration to master */
	ret = espi_apply_config(priv);
	if (ret) {
		dev_err(dev, "Failed to apply negotiated configuration\n");
		goto err_disable_clocks;
	}

	/* Step 7: Configure peripheral memory mapping */
	ret = espi_mem_cfg();
	if (ret) {
		dev_err(dev, "Failed to configure peripheral memory mapping\n");
		goto err_disable_clocks;
	}

	/* Step 8: Configure VW GPIO mappings if available */
	if (priv->vw_gpio_count > 0) {
		dev_dbg(dev, "Configuring VW GPIO mappings...\n");
		espi_config_vwgpio(priv);
	}

	/* Mark communication readiness (hardware is initialized regardless). */
	priv->initialized = slave_ready;
	if (slave_ready)
		dev_info(dev, "SpacemiT eSPI controller initialized successfully\n");
	else
		dev_warn(dev, "SpacemiT eSPI initialized in master-only mode (no EC slave)\n");

	return 0;

err_disable_clocks:
	g_espi_priv = NULL;
	clk_disable(&priv->clk_mclk);
err_disable_sclk:
	clk_disable(&priv->clk_sclk);
err_disable_sclk_src:
	clk_disable(&priv->clk_sclk_src);
	return ret;
}

static const struct espi_ops spacemit_espi_ops = {
	.hw_init = spacemit_espi_hw_init,
#ifndef CONFIG_XPL_BUILD
	/* Advanced operations not needed in SPL */
	.send_vwire = spacemit_espi_send_vwire,
	.receive_vwire = spacemit_espi_receive_vwire,
	.send_oob = spacemit_espi_send_oob,
	.receive_oob = spacemit_espi_receive_oob,
	.flash_read = spacemit_espi_flash_read,
	.flash_write = spacemit_espi_flash_write,
	.flash_erase = spacemit_espi_flash_erase,
#endif
};

static int spacemit_espi_remove(struct udevice *dev)
{
	struct spacemit_espi_priv *priv = dev_get_priv(dev);

	/* Disable clocks in reverse order */
	clk_disable(&priv->clk_mclk);
	clk_disable(&priv->clk_sclk);
	clk_disable(&priv->clk_sclk_src);

	g_espi_priv = NULL;
	return 0;
}

static const struct udevice_id spacemit_espi_ids[] = {
	{ .compatible = "spacemit,k2-espi" },
	{},
};

U_BOOT_DRIVER(spacemit_espi) = {
	.name = "spacemit_espi",
	.id = UCLASS_ESPI,
	.of_match = spacemit_espi_ids,
	.probe = spacemit_espi_probe,
	.remove = spacemit_espi_remove,
	.priv_auto = sizeof(struct spacemit_espi_priv),
	.ops = &spacemit_espi_ops,
	.flags = DM_FLAG_PRE_RELOC,  /* Enable in SPL/pre-relocation */
};
