// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * spacemit_k3 UFS host controller driver
 *
 * Copyright (C) 2025 Spacemit Technology Co., Ltd.
 */

#include <clk.h>
#include <dm.h>
#include <reset.h>
#include <scsi.h>
#include <ufs.h>
#include <asm/io.h>
#include <dm/device_compat.h>
#include <dm/device-internal.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/bug.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <asm/unaligned.h>
#include "ufs.h"

struct spacemit_k3_ufs_priv {
	struct clk aclk;
	struct clk refclk;
	struct reset_ctl reset;
	u32 phy_mng_base;
	u32 atop_base;
	u32 ref_clk_freq;
	u32 clock_freq_hz;
	ulong aclk_rate_hz;
};

static void spacemit_k3_ufs_config_scsi_scan_luns(struct udevice *dev);
static void spacemit_k3_ufs_phy_shutdown(struct ufs_hba *hba,
					 struct spacemit_k3_ufs_priv *priv);

/*UFS HOST PHY REGISTER*/
#define UFS_ARASAN_TOP_BASE 0x1C00
#define UFS_ARASAN_PHY_MNG_BASE 0x1B00

#define UFS_MPHY_RST_CTRL 0x0
#define UFS_MPHY_PU_CTRL 0x4
#define UFS_MPHY_BKDR_CTRL 0x8
#define UFS_DEVICE_IO_CTRL 0xc
/*UFS HOST LOGIC REGISTER*/

/*on arasan*/
#define UFS_SYS1CLK_1US_REG 0xC0
#define UFS_TX_SYMBOL_CLK_NS_US_REG 0xC4
#define UFS_LOCAL_PORT_ID_REG 0xC8
#define UFS_PA_ERR_CODE_REG 0xCC
#define UFS_RETRY_TIMER_REG 0xD0
#define UFS_PA_LINK_STARTUP_TIMER_REG 0xD8
#define UFS_CFG1_REG 0xDC

#define UFS_BOOT_LU_SIZE 32

#define UFSHCD_HCE_UIC_PWR_MASK                                      \
	(UIC_HIBERNATE_ENTER | UIC_HIBERNATE_EXIT | UIC_POWER_MODE | \
	 UIC_COMMAND_COMPL)

#define UFSHCD_LINK_ALL_MASK                                            \
	(UFSHCD_HCE_UIC_PWR_MASK | UTP_TRANSFER_REQ_COMPL | UIC_ERROR | \
	 DEVICE_FATAL_ERROR | CONTROLLER_FATAL_ERROR | SYSTEM_BUS_FATAL_ERROR)

/* PA Layer Gettable and settable M-PHY Specific Attributes */
#define PA_TXHSG1SYNCLENGTH 0x1552
#define PA_TXHSG1PREPARELENGTH 0x1553
#define PA_TXHSG2SYNCLENGTH 0x1554
#define PA_TXHSG2PREPARELENGTH 0x1555
#define PA_TXHSG3SYNCLENGTH 0x1556
#define PA_TXHSG3PREPARELENGTH 0x1557
#define PA_TXMK2EXTENSION 0x155A
#define PA_PEERSCRAMBLING 0x155B
#define PA_TXSKIP 0x155C
#define PA_TXSKIPPERIOD 0x155D
#define PA_PEER_TX_LCC_ENABLE 0x155F

#define PA_MK2EXTENSIONGUARDBAND 0x15AB

/*special TX/RX Configuration Attributes*/
#define RX_LS_PRE_LEN_CAP 0x008D
#define RX_LANE_HB8_BKDOOR_ATTR 0x00F4
#define RX_PWRM_CLOSURE_LEN_CAP 0x008E
#define RX_MIN_STALL_CAP 0x0088
#define RX_LANE_SOF_BKDOOR_ATT 0x00F2
#define RX_LS_PREPARELEN_TIME 0x008D
#define RX_GARBAGE_COUNT_OFFSET 0x00F2
#define VS_TX_BURST_CLOSURE_DELAY 0xD084

#define UFS_GEOMETRY_CAPACITY_UNIT_BYTES 512ULL

/*special analog reg*/
#define ANA_EQ_CTRL_REG_ATTR 0x00CD
#define ANA_HSGEAR_CTRL_ATTR 0x00C1

/* UFS_MPHY_PU_CTRL bit definitions */
#define UFS_MPHY_PU_PLL_LOCK BIT(31)
#define UFS_DL_AFC0REQTIMEOUTVAL_MAX 0xFFFF

/**
 * spacemit_k3_ufs_set_ref_clk - Set UFS device reference clock frequency
 * @hba: UFS host controller handle
 *
 * Read the expected reference clock frequency from DTS and configure
 * the UFS device's bRefClkFreq attribute if it differs from current value.
 *
 * Returns: 0 on success, -EAGAIN if reinit required, negative error code on failure
 */
static int spacemit_k3_ufs_set_ref_clk(struct ufs_hba *hba)
{
	struct udevice *dev = hba->dev;
	struct spacemit_k3_ufs_priv *priv = dev_get_priv(dev);
	u32 ref_clk = priv->ref_clk_freq;
	u32 cur_clk = 0;
	int err;
	bool updated = false;

	err = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
				      QUERY_ATTR_IDN_REF_CLK_FREQ, 0, 0,
				      &cur_clk);
	if (err) {
		dev_warn(hba->dev, "Failed to read bRefClkFreq, err = %d\n",
			 err);
		return err;
	}

	pr_info("ufs: bRefClkFreq current=%u expected=%u\n", cur_clk, ref_clk);

	if (cur_clk != ref_clk) {
		err = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
					      QUERY_ATTR_IDN_REF_CLK_FREQ, 0, 0,
					      &ref_clk);
		if (err) {
			dev_warn(hba->dev,
				 "Failed to set bRefClkFreq to %u, err = %d\n",
				 ref_clk, err);
			return err;
		}
		updated = true;
	}

	if (updated) {
		pr_info("ufs: bRefClkFreq updated, reinit required\n");
		return -EAGAIN;
	}

	return 0;
}

static int spacemit_k3_ufs_parse_ref_clk_freq(u32 raw, u32 *ref_clk_freq)
{
	/* DTS must provide one of the UFS-spec reference clock frequencies in Hz. */
	switch (raw) {
	case 19200000:
		*ref_clk_freq = UFS_REF_CLK_FREQ_19_2_MHZ;
		return 0;
	case 26000000:
		*ref_clk_freq = UFS_REF_CLK_FREQ_26_MHZ;
		return 0;
	case 38400000:
		*ref_clk_freq = UFS_REF_CLK_FREQ_38_4_MHZ;
		return 0;
	case 52000000:
		*ref_clk_freq = UFS_REF_CLK_FREQ_52_MHZ;
		return 0;
	default:
		return -EINVAL;
	}
}

/**
 * spacemit_k3_ufs_set_power_mode - Configure UFS power mode
 * @hba: UFS host controller handle
 *
 * Configure the UFS link to the maximum supported power mode.
 * Uses ufshcd_get_max_pwr_mode() to get the negotiated parameters,
 * then switches to Rate B for better compatibility.
 *
 * Returns: 0 on success, negative error code on failure
 */
static int spacemit_k3_ufs_set_power_mode(struct ufs_hba *hba)
{
	struct ufs_pa_layer_attr final_pwr;
	struct ufs_pa_layer_attr auto_pwr;
	bool need_fastauto;
	int retry;
	int ret;

	ret = ufshcd_get_max_pwr_mode(hba);
	if (ret) {
		dev_err(hba->dev,
			"%s: Failed getting max supported power mode\n",
			__func__);
		return ret;
	}

	/*
	 * Switch to Rate B for better compatibility with K3 platform.
	 * ufshcd_get_max_pwr_mode() sets Rate A by default.
	 */
	hba->max_pwr_info.info.hs_rate = PA_HS_MODE_B;
	memcpy(&final_pwr, &hba->max_pwr_info.info, sizeof(final_pwr));

	/*
	 * Some devices need a short settle time after the capability queries
	 * before accepting the HS PA_PWRMODE transition reliably.
	 */
	mdelay(1);

	need_fastauto = final_pwr.pwr_rx == FAST_MODE &&
			final_pwr.pwr_tx == FAST_MODE;
	if (need_fastauto) {
		auto_pwr = final_pwr;
		auto_pwr.pwr_rx = FASTAUTO_MODE;
		auto_pwr.pwr_tx = FASTAUTO_MODE;
	}

	for (retry = 0; retry < 3; retry++) {
		/*
		 * Enter FASTAUTO first, let the link settle, then request FAST
		 * mode. Some devices complete direct FAST transition but stop
		 * responding to UTP queries immediately after it.
		 */
		if (need_fastauto) {
			ret = ufshcd_change_power_mode(hba, &auto_pwr);
			if (ret) {
				dev_err(hba->dev,
					"%s: Failed setting FASTAUTO mode, retry %d, err = %d\n",
					__func__, retry + 1, ret);
				mdelay(10);
				continue;
			}

			mdelay(1);
		}

		ret = ufshcd_change_power_mode(hba, &final_pwr);
		if (!ret)
			break;

		dev_err(hba->dev,
			"%s: Failed setting power mode, retry %d, err = %d\n",
			__func__, retry + 1, ret);
		mdelay(10);
	}

	if (ret) {
		dev_err(hba->dev, "%s: Failed setting power mode, err = %d\n",
			__func__, ret);
		return ret;
	}

	ufshcd_print_pwr_info(hba);
	return 0;
}

static int spacemit_k3_ufs_init_aclk(struct udevice *dev)
{
	struct spacemit_k3_ufs_priv *priv = dev_get_priv(dev);
	ulong rate_hz = 0;

	if (priv->clock_freq_hz) {
		rate_hz = clk_set_rate(&priv->aclk, priv->clock_freq_hz);
		if (IS_ERR_VALUE(rate_hz)) {
			dev_warn(dev,
				 "ufs: failed to set aclk to %uHz: %ld, using current rate\n",
				 priv->clock_freq_hz, (long)rate_hz);
			rate_hz = 0;
		}
	}

	if (!rate_hz)
		rate_hz = clk_get_rate(&priv->aclk);

	if (IS_ERR_VALUE(rate_hz) || !rate_hz) {
		long err = IS_ERR_VALUE(rate_hz) ? (long)rate_hz : -EINVAL;

		dev_err(dev, "ufs: failed to get aclk rate: %ld\n", err);
		return err;
	}

	priv->aclk_rate_hz = rate_hz;
	return 0;
}

static int spacemit_k3_ufs_get_aclk_rate(struct ufs_hba *hba, ulong *rate_hz)
{
	struct spacemit_k3_ufs_priv *priv = dev_get_priv(hba->dev);
	ulong rate;

	rate = clk_get_rate(&priv->aclk);
	if (IS_ERR_VALUE(rate) || !rate)
		rate = priv->aclk_rate_hz;

	if (!rate) {
		dev_err(hba->dev, "ufs: invalid aclk rate\n");
		return -EINVAL;
	}

	priv->aclk_rate_hz = rate;
	*rate_hz = rate;
	return 0;
}

static int spacemit_k3_ufs_get_sys1clk_1us(struct ufs_hba *hba,
					   u32 *sys1clk_1us)
{
	ulong rate_hz;
	u32 cycles_per_us;
	int ret;

	ret = spacemit_k3_ufs_get_aclk_rate(hba, &rate_hz);
	if (ret)
		return ret;

	/*
	 * Bias upward so controller timing derived from SYS1CLK is never
	 * programmed shorter than the real ACLK period.
	 */
	cycles_per_us = DIV_ROUND_UP(rate_hz, 1000000UL);
	if (!cycles_per_us)
		return -ERANGE;

	*sys1clk_1us = cycles_per_us;
	return 0;
}

static int spacemit_k3_ufs_clk_enable(struct udevice *dev)
{
	struct spacemit_k3_ufs_priv *priv = dev_get_priv(dev);
	int ret;

	/* First deassert reset */
	ret = reset_deassert(&priv->reset);
	if (ret) {
		dev_err(dev, "ufs: fail to deassert reset, ret=%d\n", ret);
		return ret;
	}

	ret = spacemit_k3_ufs_init_aclk(dev);
	if (ret)
		goto err_assert_reset;

	/* Then enable clock */
	ret = clk_enable(&priv->aclk);
	if (ret) {
		dev_err(dev, "ufs: fail to enable ufs aclk, ret=%d\n", ret);
		goto err_assert_reset;
	}

	if (priv->refclk.dev) {
		ret = clk_enable(&priv->refclk);
		if (ret) {
			dev_err(dev, "ufs: fail to enable ufs refclk, ret=%d\n",
				ret);
			goto err_disable_aclk;
		}
	}

	/* HYNIX1 phone need delay */
	mdelay(5);
	return 0;

err_disable_aclk:
	clk_disable(&priv->aclk);
err_assert_reset:
	reset_assert(&priv->reset);
	return ret;
}

static void spacemit_k3_ufs_clk_disable(struct spacemit_k3_ufs_priv *priv)
{
	if (priv->refclk.dev)
		clk_disable(&priv->refclk);

	/* Disable clock first */
	clk_disable(&priv->aclk);

	/* Then assert reset */
	reset_assert(&priv->reset);
}

static int __maybe_unused debug_print_desc(struct udevice *dev,
					   enum desc_idn idn)
{
	u8 *desc_buf;

	int ret = 0;
	struct ufs_hba *hba = dev_get_uclass_priv(dev);
	int desc_size;

	switch (idn) {
	case QUERY_DESC_IDN_CONFIGURATION:
		desc_size = hba->desc_size.conf_desc;
		break;
	case QUERY_DESC_IDN_DEVICE:
		desc_size = hba->desc_size.dev_desc;
		break;
	case QUERY_DESC_IDN_UNIT:
		desc_size = hba->desc_size.unit_desc;
		break;
	case QUERY_DESC_IDN_GEOMETRY:
		desc_size = hba->desc_size.geom_desc;
		break;
	default:
		dev_err(hba->dev, "Invalid descriptor ID\n");
		return -EINVAL;
	}

	desc_buf = kmalloc(desc_size, GFP_KERNEL);
	if (!desc_buf) {
		ret = -ENOMEM;
		goto out;
	}

	if (idn != QUERY_DESC_IDN_UNIT) {
		ret = ufshcd_query_descriptor_retry(hba,
						    UPIU_QUERY_OPCODE_READ_DESC,
						    idn, 0, 0, desc_buf,
						    &desc_size);
		if (ret) {
			dev_err(hba->dev, "%s:FAILed read descriptor%d\n",
				__func__, ret);
			return ret;
		}

		debug("ufs: debug print descriptor for idn %d\n", idn);

		for (int i = 0; i < hba->desc_size.conf_desc; i++) {
			debug("[%x]:%x  ", i, desc_buf[i]);
			if ((i + 1) % 8 == 0) {
				debug("\n");
			}
		}
	} else {
		debug("ufs: debug print descriptor for idn %d\n", idn);
		for (int i = 0; i < 8; i++) {
			ret = ufshcd_query_descriptor_retry(
				hba, UPIU_QUERY_OPCODE_READ_DESC, idn, i, 0,
				desc_buf, &desc_size);
			if (ret) {
				dev_err(hba->dev,
					"%s:FAILed read descriptor%d\n",
					__func__, ret);

				return ret;
			}

			debug("ufs: unit %d descriptor\n", i);
			for (int i = 0; i < hba->desc_size.conf_desc; i++) {
				debug("[%x]:%x  ", i, desc_buf[i]);
				if ((i + 1) % 8 == 0) {
					debug("\n");
				}
			}
		}
	}
out:
	kfree(desc_buf);
	return ret;
}

#define SPACEMIT_UFS_CONFIG_LUN_SLOTS 8
#define SPACEMIT_UFS_UNIT_DESC_PARAM_LU_ENABLE 0x03
#define SPACEMIT_UFS_UNIT_DESC_PARAM_BOOT_LU_ID 0x04
#define SPACEMIT_UFS_UNIT_DESC_PARAM_LU_WRI_PRO 0x05
#define SPACEMIT_UFS_UNIT_DESC_PARAM_MEM_TYPE 0x08
#define SPACEMIT_UFS_UNIT_DESC_PARAM_DATA_RELY 0x09
#define SPACEMIT_UFS_UNIT_DESC_PARAM_LOGIC_BLK_SIZE 0x0A
#define SPACEMIT_UFS_UNIT_DESC_PARAM_PROVIS_TYPE 0x17
#define SPACEMIT_UFS_UNIT_DESC_PARAM_CON_CAP 0x20
#define SPACEMIT_UFS_UNIT_DESC_PARAM_LUN_WB_BUF_ALLOC_UNIT 0x29
#define SPACEMIT_K3_UFS_RECONF_SETTLE_MS 300
#define SPACEMIT_UFS_DEFAULT_LOGICAL_BLK_SIZE 12

static __maybe_unused int
spacemit_k3_ufs_get_conf_desc_layout(struct ufs_hba *hba, int *head_desc_size,
				     int *unit_desc_size)
{
	if (hba->desc_size.conf_head_desc > 0 &&
	    hba->desc_size.conf_unit_desc > 0 &&
	    hba->desc_size.conf_head_desc +
			    hba->desc_size.conf_unit_desc *
				    SPACEMIT_UFS_CONFIG_LUN_SLOTS <=
		    hba->desc_size.conf_desc) {
		*head_desc_size = hba->desc_size.conf_head_desc;
		*unit_desc_size = hba->desc_size.conf_unit_desc;
		return 0;
	}

	/* Try known descriptor layouts first. */
	if (QUERY_DESC_CONFIGURATION_DEF_SIZE_NEW_HEAD +
		    QUERY_DESC_CONFIGURATION_DEF_SIZE_NEW_UNIT *
			    SPACEMIT_UFS_CONFIG_LUN_SLOTS <=
	    hba->desc_size.conf_desc) {
		*head_desc_size = QUERY_DESC_CONFIGURATION_DEF_SIZE_NEW_HEAD;
		*unit_desc_size = QUERY_DESC_CONFIGURATION_DEF_SIZE_NEW_UNIT;
		return 0;
	}

	if (QUERY_DESC_CONFIGURATION_DEF_SIZE_HEAD +
		    QUERY_DESC_CONFIGURATION_DEF_SIZE_UNIT *
			    SPACEMIT_UFS_CONFIG_LUN_SLOTS <=
	    hba->desc_size.conf_desc) {
		*head_desc_size = QUERY_DESC_CONFIGURATION_DEF_SIZE_HEAD;
		*unit_desc_size = QUERY_DESC_CONFIGURATION_DEF_SIZE_UNIT;
		return 0;
	}

	return -EINVAL;
}

static bool spacemit_k3_ufs_is_conf_desc_layout_valid(struct ufs_hba *hba,
						      int head_desc_size,
						      int unit_desc_size)
{
	if (head_desc_size <= 0 || unit_desc_size <= 0)
		return false;

	return head_desc_size +
		       unit_desc_size * SPACEMIT_UFS_CONFIG_LUN_SLOTS <=
	       hba->desc_size.conf_desc;
}

static int spacemit_k3_ufs_get_conf_desc_layout_from_dev_desc(
	struct ufs_hba *hba, int *head_desc_size, int *unit_desc_size)
{
	u8 desc_buf[QUERY_DESC_MAX_SIZE];
	int desc_size = hba->desc_size.dev_desc;
	int ret;

	if (desc_size <= DEVICE_DESC_PARAM_UD_LEN ||
	    desc_size > QUERY_DESC_MAX_SIZE)
		desc_size = QUERY_DESC_DEVICE_DEF_SIZE;

	ret = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					    QUERY_DESC_IDN_DEVICE, 0, 0,
					    desc_buf, &desc_size);
	if (ret)
		return ret;

	if (desc_size <= DEVICE_DESC_PARAM_UD_LEN)
		return -EINVAL;

	*head_desc_size = desc_buf[DEVICE_DESC_PARAM_UD_OFFSET];
	*unit_desc_size = desc_buf[DEVICE_DESC_PARAM_UD_LEN];

	if (!spacemit_k3_ufs_is_conf_desc_layout_valid(
		    hba, *head_desc_size, *unit_desc_size))
		return -EINVAL;

	return 0;
}

static int spacemit_k3_ufs_read_unit_lu_state(struct ufs_hba *hba,
					      u8 *unit_lu_enabled,
					      int *enabled_lun_count)
{
	u8 desc_buf[QUERY_DESC_MAX_SIZE];
	int desc_size;
	bool any_read_ok = false;
	int i, ret;

	*enabled_lun_count = 0;
	memset(unit_lu_enabled, 0, SPACEMIT_UFS_CONFIG_LUN_SLOTS);

	for (i = 0; i < SPACEMIT_UFS_CONFIG_LUN_SLOTS; i++) {
		desc_size = hba->desc_size.unit_desc;
		if (desc_size <= SPACEMIT_UFS_UNIT_DESC_PARAM_LU_ENABLE ||
		    desc_size > QUERY_DESC_MAX_SIZE)
			desc_size = QUERY_DESC_UNIT_DEF_SIZE;

		ret = ufshcd_query_descriptor_retry(hba,
						    UPIU_QUERY_OPCODE_READ_DESC,
						    QUERY_DESC_IDN_UNIT, i, 0,
						    desc_buf, &desc_size);
		if (ret) {
			dev_dbg(hba->dev,
				"%s: read unit descriptor[%d] failed: %d\n",
				__func__, i, ret);
			continue;
		}

		any_read_ok = true;
		if (desc_size <= SPACEMIT_UFS_UNIT_DESC_PARAM_LU_ENABLE)
			continue;

		if (desc_buf[SPACEMIT_UFS_UNIT_DESC_PARAM_LU_ENABLE] == 0x1) {
			unit_lu_enabled[i] = 1;
			(*enabled_lun_count)++;
		}
	}

	return any_read_ok ? 0 : -EIO;
}

static int spacemit_k3_ufs_map_query_error(int ret)
{
	if (ret <= 0)
		return ret;

	switch (ret) {
	case QUERY_RESULT_INVALID_LENGTH:
	case QUERY_RESULT_INVALID_VALUE:
	case QUERY_RESULT_INVALID_SELECTOR:
	case QUERY_RESULT_INVALID_INDEX:
	case QUERY_RESULT_INVALID_IDN:
	case QUERY_RESULT_INVALID_OPCODE:
		return -EINVAL;
	case QUERY_RESULT_NOT_WRITEABLE:
		return -EROFS;
	case QUERY_RESULT_ALREADY_WRITTEN:
		return -EALREADY;
	case QUERY_RESULT_NOT_READABLE:
		return -EACCES;
	default:
		return -EIO;
	}
}

static void spacemit_k3_ufs_log_unrecoverable_query_error(struct ufs_hba *hba,
							  int query_ret)
{
	switch (query_ret) {
	case QUERY_RESULT_NOT_WRITEABLE:
		dev_err(hba->dev,
			"ufs: configuration descriptor is not writable; cannot move capacity to LU0 (unrecoverable)\n");
		break;
	case QUERY_RESULT_ALREADY_WRITTEN:
		dev_err(hba->dev,
			"ufs: configuration descriptor was already written by the device and cannot be changed again (unrecoverable)\n");
		break;
	default:
		break;
	}
}

static bool spacemit_k3_ufs_conf_unit_has_template(const u8 *desc_buf,
						   int head_desc_size,
						   int unit_desc_size, int lun)
{
	int offset;

	if (lun < 0 || lun >= SPACEMIT_UFS_CONFIG_LUN_SLOTS)
		return false;

	if (unit_desc_size <= CONFIG_DESC_UNIT_PARAM_LOGIC_BLK_SIZE)
		return false;

	offset = head_desc_size + unit_desc_size * lun;
	return desc_buf[offset + CONFIG_DESC_UNIT_PARAM_LOGIC_BLK_SIZE] != 0;
}

static void spacemit_k3_ufs_copy_conf_unit(struct ufs_hba *hba, u8 *desc_buf,
					   int head_desc_size, int unit_desc_size,
					   int src_lun, int dst_lun)
{
	int src_offset;
	int dst_offset;

	if (src_lun == dst_lun ||
	    src_lun < 0 || src_lun >= SPACEMIT_UFS_CONFIG_LUN_SLOTS ||
	    dst_lun < 0 || dst_lun >= SPACEMIT_UFS_CONFIG_LUN_SLOTS)
		return;

	src_offset = head_desc_size + unit_desc_size * src_lun;
	dst_offset = head_desc_size + unit_desc_size * dst_lun;
	memcpy(&desc_buf[dst_offset], &desc_buf[src_offset], unit_desc_size);
	dev_dbg(hba->dev, "ufs: copied config template LU%d -> LU%d\n",
		src_lun, dst_lun);
}

static bool spacemit_k3_ufs_no_template_error(int ret)
{
	return ret == -ENODATA || ret == -EINVAL || ret == -EACCES;
}

static int spacemit_k3_ufs_read_unit_desc(struct ufs_hba *hba, int lun,
					  u8 *unit_desc, int *desc_size)
{
	int size = hba->desc_size.unit_desc;
	int ret;

	if (size <= SPACEMIT_UFS_UNIT_DESC_PARAM_LOGIC_BLK_SIZE ||
	    size > QUERY_DESC_MAX_SIZE)
		size = QUERY_DESC_UNIT_DEF_SIZE;

	ret = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					    QUERY_DESC_IDN_UNIT, lun, 0,
					    unit_desc, &size);
	if (ret)
		return spacemit_k3_ufs_map_query_error(ret);

	*desc_size = size;
	return 0;
}

static int spacemit_k3_ufs_apply_unit_desc_to_conf_unit(struct ufs_hba *hba,
							 u8 *desc_buf,
							 int unit_offset,
							 int conf_unit_desc,
							 int lun,
							 bool require_enabled)
{
	u8 unit_desc[QUERY_DESC_MAX_SIZE];
	int desc_size;
	int ret;

	ret = spacemit_k3_ufs_read_unit_desc(hba, lun, unit_desc, &desc_size);
	if (ret)
		return ret;

	if (desc_size <= SPACEMIT_UFS_UNIT_DESC_PARAM_LU_ENABLE)
		return -ENODATA;

	if (require_enabled &&
	    unit_desc[SPACEMIT_UFS_UNIT_DESC_PARAM_LU_ENABLE] != 0x1)
		return -ENODATA;

	if (desc_size <= SPACEMIT_UFS_UNIT_DESC_PARAM_LOGIC_BLK_SIZE ||
	    !unit_desc[SPACEMIT_UFS_UNIT_DESC_PARAM_LOGIC_BLK_SIZE])
		return -ENODATA;

	if (conf_unit_desc > CONFIG_DESC_UNIT_PARAM_BOOT_LU_ID &&
	    desc_size > SPACEMIT_UFS_UNIT_DESC_PARAM_BOOT_LU_ID)
		desc_buf[unit_offset + CONFIG_DESC_UNIT_PARAM_BOOT_LU_ID] =
			unit_desc[SPACEMIT_UFS_UNIT_DESC_PARAM_BOOT_LU_ID];

	if (conf_unit_desc > CONFIG_DESC_UNIT_PARAM_LU_WRI_PRO &&
	    desc_size > SPACEMIT_UFS_UNIT_DESC_PARAM_LU_WRI_PRO)
		desc_buf[unit_offset + CONFIG_DESC_UNIT_PARAM_LU_WRI_PRO] =
			unit_desc[SPACEMIT_UFS_UNIT_DESC_PARAM_LU_WRI_PRO];

	if (conf_unit_desc > CONFIG_DESC_UNIT_PARAM_MEM_TYPE &&
	    desc_size > SPACEMIT_UFS_UNIT_DESC_PARAM_MEM_TYPE)
		desc_buf[unit_offset + CONFIG_DESC_UNIT_PARAM_MEM_TYPE] =
			unit_desc[SPACEMIT_UFS_UNIT_DESC_PARAM_MEM_TYPE];

	if (conf_unit_desc > CONFIG_DESC_UNIT_PARAM_DATA_RELY &&
	    desc_size > SPACEMIT_UFS_UNIT_DESC_PARAM_DATA_RELY)
		desc_buf[unit_offset + CONFIG_DESC_UNIT_PARAM_DATA_RELY] =
			unit_desc[SPACEMIT_UFS_UNIT_DESC_PARAM_DATA_RELY];

	if (conf_unit_desc > CONFIG_DESC_UNIT_PARAM_LOGIC_BLK_SIZE &&
	    desc_size > SPACEMIT_UFS_UNIT_DESC_PARAM_LOGIC_BLK_SIZE)
		desc_buf[unit_offset + CONFIG_DESC_UNIT_PARAM_LOGIC_BLK_SIZE] =
			unit_desc[SPACEMIT_UFS_UNIT_DESC_PARAM_LOGIC_BLK_SIZE];

	if (conf_unit_desc > CONFIG_DESC_UNIT_PARAM_PROVIS_TYPE &&
	    desc_size > SPACEMIT_UFS_UNIT_DESC_PARAM_PROVIS_TYPE)
		desc_buf[unit_offset + CONFIG_DESC_UNIT_PARAM_PROVIS_TYPE] =
			unit_desc[SPACEMIT_UFS_UNIT_DESC_PARAM_PROVIS_TYPE];

	if (conf_unit_desc >= CONFIG_DESC_UNIT_PARAM_CON_CAP + 2 &&
	    desc_size >= SPACEMIT_UFS_UNIT_DESC_PARAM_CON_CAP + 2)
		put_unaligned_be16(
			get_unaligned_be16(
				&unit_desc[SPACEMIT_UFS_UNIT_DESC_PARAM_CON_CAP]),
			&desc_buf[unit_offset + CONFIG_DESC_UNIT_PARAM_CON_CAP]);

	if (conf_unit_desc >= CONFIG_DESC_UNIT_PARAM_LUN_WB_BUF_ALLOC_UNIT + 4 &&
	    desc_size >= SPACEMIT_UFS_UNIT_DESC_PARAM_LUN_WB_BUF_ALLOC_UNIT + 4)
		put_unaligned_be32(
			get_unaligned_be32(
				&unit_desc[SPACEMIT_UFS_UNIT_DESC_PARAM_LUN_WB_BUF_ALLOC_UNIT]),
			&desc_buf[unit_offset +
				  CONFIG_DESC_UNIT_PARAM_LUN_WB_BUF_ALLOC_UNIT]);

	dev_dbg(hba->dev,
		"ufs: populated LU0 config template from UNIT descriptor LU%d\n",
		lun);
	return 0;
}

static int spacemit_k3_ufs_find_conf_template_lun(const u8 *desc_buf,
						  int head_desc_size,
						  int unit_desc_size,
						  const u8 *unit_lu_enabled,
						  bool have_unit_state,
						  int preferred_lun)
{
	int i;

	if (preferred_lun >= 0 &&
	    preferred_lun < SPACEMIT_UFS_CONFIG_LUN_SLOTS &&
	    spacemit_k3_ufs_conf_unit_has_template(desc_buf, head_desc_size,
						   unit_desc_size,
						   preferred_lun))
		return preferred_lun;

	if (have_unit_state) {
		for (i = 0; i < SPACEMIT_UFS_CONFIG_LUN_SLOTS; i++) {
			if (!unit_lu_enabled[i])
				continue;
			if (spacemit_k3_ufs_conf_unit_has_template(
				    desc_buf, head_desc_size, unit_desc_size, i))
				return i;
		}
	}

	for (i = 0; i < SPACEMIT_UFS_CONFIG_LUN_SLOTS; i++) {
		if (spacemit_k3_ufs_conf_unit_has_template(desc_buf,
							    head_desc_size,
							    unit_desc_size, i))
			return i;
	}

	return -1;
}

static int
spacemit_k3_ufs_apply_any_unit_desc_template(struct ufs_hba *hba, u8 *desc_buf,
					     int head_desc_size, int conf_unit_desc,
					     const u8 *unit_lu_enabled,
					     bool have_unit_state,
					     int preferred_lun, int *template_lun)
{
	int i;
	int ret;

	if (preferred_lun >= 0 && preferred_lun < SPACEMIT_UFS_CONFIG_LUN_SLOTS) {
		ret = spacemit_k3_ufs_apply_unit_desc_to_conf_unit(
			hba, desc_buf, head_desc_size, conf_unit_desc,
			preferred_lun, false);
		if (!ret) {
			*template_lun = preferred_lun;
			return 0;
		}
		if (!spacemit_k3_ufs_no_template_error(ret))
			dev_warn(hba->dev,
				 "ufs: failed to use UNIT descriptor LU%d as LU0 template: %d\n",
				 preferred_lun, ret);
	}

	if (have_unit_state) {
		for (i = 0; i < SPACEMIT_UFS_CONFIG_LUN_SLOTS; i++) {
			if (i == preferred_lun || !unit_lu_enabled[i])
				continue;

			ret = spacemit_k3_ufs_apply_unit_desc_to_conf_unit(
				hba, desc_buf, head_desc_size, conf_unit_desc, i,
				false);
			if (!ret) {
				*template_lun = i;
				return 0;
			}
			if (!spacemit_k3_ufs_no_template_error(ret))
				dev_warn(hba->dev,
					 "ufs: failed to use enabled UNIT descriptor LU%d as LU0 template: %d\n",
					 i, ret);
		}
	}

	for (i = 0; i < SPACEMIT_UFS_CONFIG_LUN_SLOTS; i++) {
		if (i == preferred_lun)
			continue;
		if (have_unit_state && unit_lu_enabled[i])
			continue;

		ret = spacemit_k3_ufs_apply_unit_desc_to_conf_unit(
			hba, desc_buf, head_desc_size, conf_unit_desc, i, false);
		if (!ret) {
			*template_lun = i;
			return 0;
		}
		if (!spacemit_k3_ufs_no_template_error(ret))
			dev_warn(hba->dev,
				 "ufs: failed to use UNIT descriptor LU%d as fallback LU0 template: %d\n",
				 i, ret);
	}

	return -ENODATA;
}

static int spacemit_k3_ufs_score_conf_layout(const u8 *desc_buf,
					     int head_desc_size,
					     int unit_desc_size,
					     const u8 *unit_lu_enabled)
{
	int i;
	int score = 0;

	for (i = 0; i < SPACEMIT_UFS_CONFIG_LUN_SLOTS; i++) {
		int offset = head_desc_size + unit_desc_size * i;
		bool conf_lu_enabled =
			desc_buf[offset + CONFIG_DESC_UNIT_PARAM_LU_EN] == 0x1;

		if (conf_lu_enabled == !!unit_lu_enabled[i])
			score++;
	}

	return score;
}

static void spacemit_k3_ufs_add_conf_layout_candidate(
	struct ufs_hba *hba, int *candidate_head, int *candidate_unit,
	int *candidate_count, int head_desc_size, int unit_desc_size)
{
	int i;

	if (!spacemit_k3_ufs_is_conf_desc_layout_valid(hba, head_desc_size,
						       unit_desc_size))
		return;

	for (i = 0; i < *candidate_count; i++) {
		if (candidate_head[i] == head_desc_size &&
		    candidate_unit[i] == unit_desc_size)
			return;
	}

	candidate_head[*candidate_count] = head_desc_size;
	candidate_unit[*candidate_count] = unit_desc_size;
	(*candidate_count)++;
}

static int spacemit_k3_ufs_select_conf_desc_layout(struct ufs_hba *hba,
						   const u8 *desc_buf,
						   const u8 *unit_lu_enabled,
						   int *head_desc_size,
						   int *unit_desc_size)
{
	int candidate_head[4];
	int candidate_unit[4];
	int candidate_count = 0;
	int dev_head_desc;
	int dev_unit_desc;
	int i;
	int best = -1;
	int best_score = -1;
	int ret;

	ret = spacemit_k3_ufs_get_conf_desc_layout_from_dev_desc(
		hba, &dev_head_desc, &dev_unit_desc);
	if (!ret)
		spacemit_k3_ufs_add_conf_layout_candidate(
			hba, candidate_head, candidate_unit, &candidate_count,
			dev_head_desc, dev_unit_desc);

	spacemit_k3_ufs_add_conf_layout_candidate(
		hba, candidate_head, candidate_unit, &candidate_count,
		hba->desc_size.conf_head_desc, hba->desc_size.conf_unit_desc);

	spacemit_k3_ufs_add_conf_layout_candidate(
		hba, candidate_head, candidate_unit, &candidate_count,
		QUERY_DESC_CONFIGURATION_DEF_SIZE_NEW_HEAD,
		QUERY_DESC_CONFIGURATION_DEF_SIZE_NEW_UNIT);

	spacemit_k3_ufs_add_conf_layout_candidate(
		hba, candidate_head, candidate_unit, &candidate_count,
		QUERY_DESC_CONFIGURATION_DEF_SIZE_HEAD,
		QUERY_DESC_CONFIGURATION_DEF_SIZE_UNIT);

	for (i = 0; i < candidate_count; i++) {
		int score;

		if (!spacemit_k3_ufs_is_conf_desc_layout_valid(
			    hba, candidate_head[i], candidate_unit[i]))
			continue;

		score = spacemit_k3_ufs_score_conf_layout(desc_buf,
							  candidate_head[i],
							  candidate_unit[i],
							  unit_lu_enabled);
		if (score > best_score) {
			best = i;
			best_score = score;
		}
	}

	if (best < 0)
		return -EINVAL;

	*head_desc_size = candidate_head[best];
	*unit_desc_size = candidate_unit[best];

	debug("ufs: selected config layout head=0x%x unit=0x%x score=%d\n",
	      *head_desc_size, *unit_desc_size, best_score);

	return 0;
}

static __maybe_unused void
spacemit_k3_ufs_dump_conf_header(struct ufs_hba *hba, const char *tag,
				 const u8 *desc_buf, int head_desc_size)
{
	u32 shared_wb_alloc = 0;

	if (head_desc_size <= CONFIG_DESC_HEADER_PARAM_INIT_ACT_ICC_LEV)
		return;

	if (head_desc_size >= CONFIG_DESC_HEADER_PARAM_NUM_SHA_WB + 4)
		shared_wb_alloc = get_unaligned_be32(
			&desc_buf[CONFIG_DESC_HEADER_PARAM_NUM_SHA_WB]);

	dev_dbg(hba->dev,
		"ufs: %s cfg len=0x%x id=0x%x cont=%u boot_en=%u desc_acc=%u init_pwr=%u high_lun=%u sec_rm=%u icc=%u rpmb_en=%u wb_pres=%u wb_type=%u shared_wb=%u\n",
		tag, desc_buf[CONFIG_DESC_HEADER_PARAM_LEN],
		desc_buf[CONFIG_DESC_HEADER_PARAM_DES_IDN],
		desc_buf[CONFIG_DESC_HEADER_PARAM_CONF_DESC_CONT],
		desc_buf[CONFIG_DESC_HEADER_PARAM_BOOT_EN],
		desc_buf[CONFIG_DESC_HEADER_PARAM_DES_ACC_EN],
		desc_buf[CONFIG_DESC_HEADER_PARAM_INIT_POWER_MODE],
		desc_buf[CONFIG_DESC_HEADER_PARAM_HIGH_PRI_LUN],
		desc_buf[CONFIG_DESC_HEADER_PARAM_SEC_REM_TYPE],
		desc_buf[CONFIG_DESC_HEADER_PARAM_INIT_ACT_ICC_LEV],
		head_desc_size > CONFIG_DESC_HEADER_PARAM_RPMB_REG_EN ?
			desc_buf[CONFIG_DESC_HEADER_PARAM_RPMB_REG_EN] :
			0,
		head_desc_size > CONFIG_DESC_HEADER_PARAM_WB_BUF_PRE ?
			desc_buf[CONFIG_DESC_HEADER_PARAM_WB_BUF_PRE] :
			0,
		head_desc_size > CONFIG_DESC_HEADER_PARAM_WB_BUF_TYP ?
			desc_buf[CONFIG_DESC_HEADER_PARAM_WB_BUF_TYP] :
			0,
		shared_wb_alloc);
}

static __maybe_unused void
spacemit_k3_ufs_dump_conf_unit(struct ufs_hba *hba, const char *tag,
			       const u8 *desc_buf, int head_desc_size,
			       int unit_desc_size, int lun)
{
	int offset = head_desc_size + unit_desc_size * lun;
	u32 alloc_units = 0;
	u16 context_cap = 0;
	u32 wb_alloc_units = 0;

	if (unit_desc_size <= CONFIG_DESC_UNIT_PARAM_LOGIC_BLK_SIZE)
		return;

	if (unit_desc_size >= CONFIG_DESC_UNIT_PARAM_NUM_ALLOC_UNIT + 4)
		alloc_units = get_unaligned_be32(
			&desc_buf[offset + CONFIG_DESC_UNIT_PARAM_NUM_ALLOC_UNIT]);

	if (unit_desc_size >= CONFIG_DESC_UNIT_PARAM_CON_CAP + 2)
		context_cap = get_unaligned_be16(
			&desc_buf[offset + CONFIG_DESC_UNIT_PARAM_CON_CAP]);

	if (unit_desc_size >= CONFIG_DESC_UNIT_PARAM_LUN_WB_BUF_ALLOC_UNIT + 4)
		wb_alloc_units = get_unaligned_be32(
			&desc_buf[offset +
				  CONFIG_DESC_UNIT_PARAM_LUN_WB_BUF_ALLOC_UNIT]);

	dev_dbg(hba->dev,
		"ufs: %s LU%d en=%u boot=%u wp=%u mem=%u alloc=%u data_rel=%u blk_size=%u provis=%u ctx=0x%04x wb_alloc=%u\n",
		tag, lun, desc_buf[offset + CONFIG_DESC_UNIT_PARAM_LU_EN],
		desc_buf[offset + CONFIG_DESC_UNIT_PARAM_BOOT_LU_ID],
		desc_buf[offset + CONFIG_DESC_UNIT_PARAM_LU_WRI_PRO],
		desc_buf[offset + CONFIG_DESC_UNIT_PARAM_MEM_TYPE], alloc_units,
		desc_buf[offset + CONFIG_DESC_UNIT_PARAM_DATA_RELY],
		desc_buf[offset + CONFIG_DESC_UNIT_PARAM_LOGIC_BLK_SIZE],
		unit_desc_size > CONFIG_DESC_UNIT_PARAM_PROVIS_TYPE ?
			desc_buf[offset + CONFIG_DESC_UNIT_PARAM_PROVIS_TYPE] :
			0,
		context_cap, wb_alloc_units);
}

static void spacemit_k3_ufs_dump_conf_state_err(struct ufs_hba *hba,
						const char *tag,
						const u8 *desc_buf,
						int head_desc_size,
						int unit_desc_size,
						int source_lun)
{
	int offset0 = head_desc_size;
	u32 alloc0 = 0;
	u32 src_alloc = 0;

	if (head_desc_size >= CONFIG_DESC_HEADER_PARAM_NUM_SHA_WB + 4) {
		/* Keep output compact but include the header fields that can block writes. */
		dev_err(hba->dev,
			"ufs: %s cfg boot_en=%u desc_acc=%u init_pwr=%u high_lun=%u rpmb_en=%u wb_type=%u shared_wb=%u\n",
			tag, desc_buf[CONFIG_DESC_HEADER_PARAM_BOOT_EN],
			desc_buf[CONFIG_DESC_HEADER_PARAM_DES_ACC_EN],
			desc_buf[CONFIG_DESC_HEADER_PARAM_INIT_POWER_MODE],
			desc_buf[CONFIG_DESC_HEADER_PARAM_HIGH_PRI_LUN],
			desc_buf[CONFIG_DESC_HEADER_PARAM_RPMB_REG_EN],
			desc_buf[CONFIG_DESC_HEADER_PARAM_WB_BUF_TYP],
			get_unaligned_be32(&desc_buf[CONFIG_DESC_HEADER_PARAM_NUM_SHA_WB]));
	}

	if (unit_desc_size >= CONFIG_DESC_UNIT_PARAM_NUM_ALLOC_UNIT + 4)
		alloc0 = get_unaligned_be32(&desc_buf[offset0 +
						    CONFIG_DESC_UNIT_PARAM_NUM_ALLOC_UNIT]);

	dev_err(hba->dev,
		"ufs: %s LU0 en=%u boot=%u wp=%u mem=%u alloc=%u blk_size=%u provis=%u ctx=0x%04x wb_alloc=%u\n",
		tag, desc_buf[offset0 + CONFIG_DESC_UNIT_PARAM_LU_EN],
		desc_buf[offset0 + CONFIG_DESC_UNIT_PARAM_BOOT_LU_ID],
		desc_buf[offset0 + CONFIG_DESC_UNIT_PARAM_LU_WRI_PRO],
		desc_buf[offset0 + CONFIG_DESC_UNIT_PARAM_MEM_TYPE], alloc0,
		desc_buf[offset0 + CONFIG_DESC_UNIT_PARAM_LOGIC_BLK_SIZE],
		desc_buf[offset0 + CONFIG_DESC_UNIT_PARAM_PROVIS_TYPE],
		get_unaligned_be16(&desc_buf[offset0 +
					     CONFIG_DESC_UNIT_PARAM_CON_CAP]),
		unit_desc_size >= CONFIG_DESC_UNIT_PARAM_LUN_WB_BUF_ALLOC_UNIT + 4 ?
			get_unaligned_be32(&desc_buf[offset0 +
						    CONFIG_DESC_UNIT_PARAM_LUN_WB_BUF_ALLOC_UNIT]) :
			0);

	if (source_lun > 0) {
		int src_offset = head_desc_size + unit_desc_size * source_lun;

		if (unit_desc_size >= CONFIG_DESC_UNIT_PARAM_NUM_ALLOC_UNIT + 4)
			src_alloc = get_unaligned_be32(
				&desc_buf[src_offset +
					  CONFIG_DESC_UNIT_PARAM_NUM_ALLOC_UNIT]);

		dev_err(hba->dev,
			"ufs: %s LU%d en=%u boot=%u wp=%u mem=%u alloc=%u blk_size=%u provis=%u ctx=0x%04x wb_alloc=%u\n",
			tag, source_lun,
			desc_buf[src_offset + CONFIG_DESC_UNIT_PARAM_LU_EN],
			desc_buf[src_offset + CONFIG_DESC_UNIT_PARAM_BOOT_LU_ID],
			desc_buf[src_offset + CONFIG_DESC_UNIT_PARAM_LU_WRI_PRO],
			desc_buf[src_offset + CONFIG_DESC_UNIT_PARAM_MEM_TYPE],
			src_alloc,
			desc_buf[src_offset + CONFIG_DESC_UNIT_PARAM_LOGIC_BLK_SIZE],
			desc_buf[src_offset + CONFIG_DESC_UNIT_PARAM_PROVIS_TYPE],
			get_unaligned_be16(&desc_buf[src_offset +
						     CONFIG_DESC_UNIT_PARAM_CON_CAP]),
			unit_desc_size >= CONFIG_DESC_UNIT_PARAM_LUN_WB_BUF_ALLOC_UNIT + 4 ?
				get_unaligned_be32(&desc_buf[src_offset +
							    CONFIG_DESC_UNIT_PARAM_LUN_WB_BUF_ALLOC_UNIT]) :
				0);
	}
}

static int
spacemit_k3_ufs_get_geometry_capacity(struct ufs_hba *hba,
				      u64 *total_alloc_units,
				      u8 *logical_blk_size)
{
	u8 *desc_buf;
	u64 total_raw_device_capacity;
	u64 alloc_unit_bytes;
	u32 segment_size;
	u8 alloc_unit_size;
	int desc_size = hba->desc_size.geom_desc;
	int ret;

	if (!total_alloc_units && !logical_blk_size)
		return -EINVAL;

	desc_buf = kmalloc(hba->desc_size.geom_desc, GFP_KERNEL);
	if (!desc_buf)
		return -ENOMEM;

	ret = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					    QUERY_DESC_IDN_GEOMETRY, 0, 0,
					    desc_buf, &desc_size);
	if (ret)
		goto out;

	if (logical_blk_size) {
		*logical_blk_size = desc_buf[GEO_DESC_PARAM_OPT_LOGIC_BLK_SIZE];
		if (!*logical_blk_size)
			*logical_blk_size = desc_buf[GEO_DESC_PARAM_MIN_ADDR_BLK_SIZE];
		if (!*logical_blk_size)
			*logical_blk_size = SPACEMIT_UFS_DEFAULT_LOGICAL_BLK_SIZE;
	}

	if (total_alloc_units) {
		total_raw_device_capacity = get_unaligned_be64(
			&desc_buf[GEO_DESC_PARAM_TOTAL_RAW_DEV_CAP]);
		segment_size = get_unaligned_be32(&desc_buf[GEO_DESC_PARAM_SEG_SIZE]);
		alloc_unit_size = desc_buf[GEO_DESC_PARAM_ALLOC_UNIT_SIZE];
		if (!segment_size || !alloc_unit_size) {
			ret = -EINVAL;
			goto out;
		}

		alloc_unit_bytes = (u64)segment_size * alloc_unit_size *
				   UFS_GEOMETRY_CAPACITY_UNIT_BYTES;
		if (!alloc_unit_bytes) {
			ret = -EINVAL;
			goto out;
		}

		*total_alloc_units =
			(total_raw_device_capacity *
			 UFS_GEOMETRY_CAPACITY_UNIT_BYTES) /
			alloc_unit_bytes;
		if (!*total_alloc_units || *total_alloc_units > 0xFFFFFFFFULL) {
			ret = -ERANGE;
			goto out;
		}

		dev_dbg(hba->dev,
			"ufs: geometry raw_cap=%llu seg_size=%u alloc_unit_size=%u total_alloc_units=%llu\n",
			total_raw_device_capacity, segment_size, alloc_unit_size,
			*total_alloc_units);
	}

out:
	kfree(desc_buf);
	return ret;
}

static int
spacemit_k3_ufs_complete_synth_lu0_template(struct ufs_hba *hba, u8 *desc_buf,
					    int head_desc_size, int conf_unit_desc,
					    u8 logical_blk_size)
{
	int offset = head_desc_size;

	if (conf_unit_desc <= CONFIG_DESC_UNIT_PARAM_LOGIC_BLK_SIZE)
		return -EINVAL;

	if (!logical_blk_size)
		logical_blk_size = SPACEMIT_UFS_DEFAULT_LOGICAL_BLK_SIZE;

	if (!desc_buf[offset + CONFIG_DESC_UNIT_PARAM_LOGIC_BLK_SIZE]) {
		desc_buf[offset + CONFIG_DESC_UNIT_PARAM_LOGIC_BLK_SIZE] =
			logical_blk_size;
		dev_warn(hba->dev,
			 "ufs: synthesizing LU0 template with logical block size %u\n",
			 logical_blk_size);
	}

	return desc_buf[offset + CONFIG_DESC_UNIT_PARAM_LOGIC_BLK_SIZE] ?
		0 : -EINVAL;
}

static void
spacemit_k3_ufs_warn_multi_lun_merge_mismatch(struct ufs_hba *hba,
					      const u8 *desc_buf,
					      int head_desc_size,
					      int conf_unit_desc,
					      const u8 *unit_lu_enabled,
					      bool have_unit_state,
					      int template_lun)
{
	u8 unit_desc[QUERY_DESC_MAX_SIZE];
	int desc_size;
	int offset0 = head_desc_size;
	int i;

	for (i = 0; i < SPACEMIT_UFS_CONFIG_LUN_SLOTS; i++) {
		bool enabled;
		int ret;
		bool mismatch = false;

		enabled = have_unit_state ? !!unit_lu_enabled[i] :
			 desc_buf[head_desc_size + conf_unit_desc * i +
				  CONFIG_DESC_UNIT_PARAM_LU_EN] == 0x1;
		if (!enabled || i == template_lun)
			continue;

		ret = spacemit_k3_ufs_read_unit_desc(hba, i, unit_desc, &desc_size);
		if (ret) {
			dev_warn(hba->dev,
				 "ufs: failed to inspect LU%d before merge, keeping LU0 template: %d\n",
				 i, ret);
			continue;
		}

		if (conf_unit_desc > CONFIG_DESC_UNIT_PARAM_MEM_TYPE &&
		    desc_size > SPACEMIT_UFS_UNIT_DESC_PARAM_MEM_TYPE &&
		    unit_desc[SPACEMIT_UFS_UNIT_DESC_PARAM_MEM_TYPE] !=
			    desc_buf[offset0 + CONFIG_DESC_UNIT_PARAM_MEM_TYPE])
			mismatch = true;
		if (conf_unit_desc > CONFIG_DESC_UNIT_PARAM_DATA_RELY &&
		    desc_size > SPACEMIT_UFS_UNIT_DESC_PARAM_DATA_RELY &&
		    unit_desc[SPACEMIT_UFS_UNIT_DESC_PARAM_DATA_RELY] !=
			    desc_buf[offset0 + CONFIG_DESC_UNIT_PARAM_DATA_RELY])
			mismatch = true;
		if (conf_unit_desc > CONFIG_DESC_UNIT_PARAM_LOGIC_BLK_SIZE &&
		    desc_size > SPACEMIT_UFS_UNIT_DESC_PARAM_LOGIC_BLK_SIZE &&
		    unit_desc[SPACEMIT_UFS_UNIT_DESC_PARAM_LOGIC_BLK_SIZE] !=
			    desc_buf[offset0 + CONFIG_DESC_UNIT_PARAM_LOGIC_BLK_SIZE])
			mismatch = true;
		if (conf_unit_desc > CONFIG_DESC_UNIT_PARAM_PROVIS_TYPE &&
		    desc_size > SPACEMIT_UFS_UNIT_DESC_PARAM_PROVIS_TYPE &&
		    unit_desc[SPACEMIT_UFS_UNIT_DESC_PARAM_PROVIS_TYPE] !=
			    desc_buf[offset0 + CONFIG_DESC_UNIT_PARAM_PROVIS_TYPE])
			mismatch = true;
		if (conf_unit_desc >= CONFIG_DESC_UNIT_PARAM_CON_CAP + 2 &&
		    desc_size >= SPACEMIT_UFS_UNIT_DESC_PARAM_CON_CAP + 2 &&
		    get_unaligned_be16(
			    &unit_desc[SPACEMIT_UFS_UNIT_DESC_PARAM_CON_CAP]) !=
			    get_unaligned_be16(
				    &desc_buf[offset0 +
					      CONFIG_DESC_UNIT_PARAM_CON_CAP]))
			mismatch = true;

		if (mismatch)
			dev_warn(hba->dev,
				 "ufs: LU%d attributes differ from LU0 template; merging capacity into LU0 with template values\n",
				 i);
	}
}

/*
 * Return values:
 *   0: already single-LUN and active
 *   1: descriptor changed or re-init still needed
 *  <0: error
 */
static __maybe_unused int
spacemit_k3_ufs_check_and_config_single_lun(struct udevice *dev)
{
	u8 *desc_buf;
	u8 *orig_desc_buf = NULL;
	u8 unit_lu_enabled[SPACEMIT_UFS_CONFIG_LUN_SLOTS];
	u32 conf_desc_lock = 0;
	u64 current_lu0_alloc_units = 0;
	u64 enabled_alloc_units = 0;
	u64 geometry_total_alloc_units = 0;
	u64 target_alloc_units = 0;
	u8 geometry_logical_blk_size = 0;
	int ret;
	int unit_state_ret;
	int unit_enabled_lun_count = 0;
	int conf_enabled_lun_count = 0;
	int conf_head_desc;
	int conf_unit_desc;
	int source_lun = -1;
	int template_lun = -1;
	int conf_template_lun = -1;
	int i;
	bool have_unit_state;
	bool need_reconfigure;
	bool preserve_lu0_template;
	struct ufs_hba *hba = dev_get_uclass_priv(dev);

	desc_buf = kmalloc(hba->desc_size.conf_desc, GFP_KERNEL);
	if (!desc_buf)
		return -ENOMEM;

	ret = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					    QUERY_DESC_IDN_CONFIGURATION, 0, 0,
					    desc_buf,
					    &hba->desc_size.conf_desc);
	if (ret) {
		dev_err(hba->dev, "%s: failed to read config descriptor: %d\n",
			__func__, ret);
		goto out;
	}

	orig_desc_buf = kmalloc(hba->desc_size.conf_desc, GFP_KERNEL);
	if (!orig_desc_buf) {
		ret = -ENOMEM;
		goto out;
	}
	memcpy(orig_desc_buf, desc_buf, hba->desc_size.conf_desc);

	unit_state_ret = spacemit_k3_ufs_read_unit_lu_state(
		hba, unit_lu_enabled, &unit_enabled_lun_count);
	ret = spacemit_k3_ufs_get_conf_desc_layout_from_dev_desc(
		hba, &conf_head_desc, &conf_unit_desc);
	if (ret && unit_state_ret) {
		dev_dbg(hba->dev,
			"%s: device/unit descriptor state unavailable: layout=%d unit=%d\n",
			__func__, ret, unit_state_ret);
		ret = spacemit_k3_ufs_get_conf_desc_layout(hba, &conf_head_desc,
							   &conf_unit_desc);
	} else if (ret) {
		ret = spacemit_k3_ufs_select_conf_desc_layout(hba, desc_buf,
							      unit_lu_enabled,
							      &conf_head_desc,
							      &conf_unit_desc);
	}

	if (ret) {
		dev_err(hba->dev,
			"%s: unsupported config descriptor layout (len=0x%x)\n",
			__func__, hba->desc_size.conf_desc);
		goto out;
	}

	for (i = 0; i < SPACEMIT_UFS_CONFIG_LUN_SLOTS; i++) {
		int offset = conf_head_desc + conf_unit_desc * i;
		bool conf_enabled =
			desc_buf[offset + CONFIG_DESC_UNIT_PARAM_LU_EN] == 0x1;
		bool enabled = unit_state_ret ? conf_enabled : unit_lu_enabled[i];

		if (conf_enabled)
			conf_enabled_lun_count++;

		if (i == 0)
			current_lu0_alloc_units = get_unaligned_be32(
				&desc_buf[offset +
					  CONFIG_DESC_UNIT_PARAM_NUM_ALLOC_UNIT]);

		if (enabled) {
			if (source_lun < 0)
				source_lun = i;
			enabled_alloc_units += get_unaligned_be32(
				&desc_buf[offset +
					  CONFIG_DESC_UNIT_PARAM_NUM_ALLOC_UNIT]);
		}
	}

	dev_dbg(hba->dev,
		"ufs: config layout len=0x%x head=0x%x unit=0x%x conf_luns=%d unit_luns=%d source=%d enabled_alloc_units=%llu\n",
		hba->desc_size.conf_desc, conf_head_desc, conf_unit_desc,
		conf_enabled_lun_count, unit_enabled_lun_count, source_lun,
		enabled_alloc_units);

	have_unit_state = !unit_state_ret;
	if (have_unit_state)
		need_reconfigure =
			(unit_enabled_lun_count != 1 || !unit_lu_enabled[0]);
	else
		need_reconfigure =
			(conf_enabled_lun_count != 1 ||
				 desc_buf[conf_head_desc + CONFIG_DESC_UNIT_PARAM_LU_EN] != 0x1);

	debug("ufs: conf_lun_count=%d unit_lun_count=%d enabled_alloc_units=%llu need_recfg=%d\n",
	      conf_enabled_lun_count, unit_enabled_lun_count,
	      enabled_alloc_units, need_reconfigure);

	if (!need_reconfigure) {
		ret = 0;
		goto out;
	}

	ret = spacemit_k3_ufs_get_geometry_capacity(
		hba, &geometry_total_alloc_units, &geometry_logical_blk_size);
	if (ret) {
		dev_err(hba->dev,
			"%s: failed to get geometry total capacity for LU0 reprovision: %d\n",
			__func__, ret);
		goto out;
	}

	target_alloc_units = geometry_total_alloc_units;
	if (!target_alloc_units || target_alloc_units > 0xFFFFFFFFULL) {
		dev_err(hba->dev,
			"%s: no valid geometry alloc-unit target for LU0 reprovision\n",
			__func__);
		ret = -EINVAL;
		goto out;
	}

	if (enabled_alloc_units && target_alloc_units < enabled_alloc_units) {
		dev_err(hba->dev,
			"%s: geometry target alloc %llu is smaller than enabled user alloc %llu\n",
			__func__, target_alloc_units, enabled_alloc_units);
		ret = -EINVAL;
		goto out;
	}

	ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR,
				      QUERY_ATTR_IDN_CONF_DESC_LOCK, 0, 0,
				      &conf_desc_lock);
	if (ret) {
		dev_warn(hba->dev,
				 "%s: failed to read bConfigDescrLock (%d), continue\n",
				 __func__, ret);
	} else if (conf_desc_lock) {
		dev_err(hba->dev,
			"%s: bConfigDescrLock is set, cannot reconfigure LUNs (unrecoverable)\n",
			__func__);
		ret = -EROFS;
		goto out;
	}

	/*
	 * Keep only LU0 enabled. For devices that are not already single-LUN,
	 * provision LU0 with the full geometry capacity so the flashed image
	 * sees the device's complete user address space. Already-single-LUN
	 * devices return above and keep their current LU0 size unchanged.
	 * Prefer an enabled LU as the template source. If none exists, fall
	 * back to any usable disabled LU template and finally synthesize the
	 * minimal LU0 fields from geometry information.
	 */
	spacemit_k3_ufs_dump_conf_header(hba, "current", desc_buf,
					 conf_head_desc);
	spacemit_k3_ufs_dump_conf_unit(hba, "current", desc_buf,
				       conf_head_desc, conf_unit_desc, 0);
	if (source_lun > 0)
		spacemit_k3_ufs_dump_conf_unit(hba, "current", desc_buf,
					       conf_head_desc, conf_unit_desc,
					       source_lun);

	/*
	 * If LU0 is already active and has a valid configuration template,
	 * keep it as-is and only merge capacity. Reapplying UNIT descriptor
	 * fields can reintroduce vendor provisioning attributes that make
	 * a full-capacity single-LUN CONFIGURATION write invalid.
	 */
	preserve_lu0_template =
		source_lun == 0 &&
		spacemit_k3_ufs_conf_unit_has_template(
			desc_buf, conf_head_desc, conf_unit_desc, 0);
	if (preserve_lu0_template) {
		template_lun = 0;
		dev_dbg(hba->dev,
			"ufs: preserving active LU0 configuration template for single-LUN merge\n");
	} else {
		conf_template_lun = spacemit_k3_ufs_find_conf_template_lun(
			desc_buf, conf_head_desc, conf_unit_desc, unit_lu_enabled,
			have_unit_state, source_lun);
		if (conf_template_lun > 0)
			spacemit_k3_ufs_copy_conf_unit(
				hba, desc_buf, conf_head_desc, conf_unit_desc,
				conf_template_lun, 0);

		ret = spacemit_k3_ufs_apply_any_unit_desc_template(
			hba, desc_buf, conf_head_desc, conf_unit_desc,
			unit_lu_enabled, have_unit_state, source_lun,
			&template_lun);
			if (ret && ret != -ENODATA) {
				dev_warn(hba->dev,
					 "%s: failed to build LU0 template from UNIT descriptors: %d, falling back to existing config fields and geometry block-size defaults\n",
					 __func__, ret);
				ret = 0;
			}
		}

	ret = spacemit_k3_ufs_complete_synth_lu0_template(
		hba, desc_buf, conf_head_desc, conf_unit_desc,
		geometry_logical_blk_size);
	if (ret) {
		dev_err(hba->dev,
			"%s: failed to synthesize a valid LU0 template: %d\n",
			__func__, ret);
		goto out;
	}

	if ((have_unit_state ? unit_enabled_lun_count : conf_enabled_lun_count) > 1)
		spacemit_k3_ufs_warn_multi_lun_merge_mismatch(
			hba, desc_buf, conf_head_desc, conf_unit_desc,
			unit_lu_enabled, have_unit_state, template_lun);

	dev_dbg(hba->dev,
		"ufs: reconfig single-LUN source=%d template=%d alloc(current=%llu target=%llu)\n",
		source_lun, template_lun, current_lu0_alloc_units,
		target_alloc_units);

	for (i = 0; i < SPACEMIT_UFS_CONFIG_LUN_SLOTS; i++) {
		int offset = conf_head_desc + conf_unit_desc * i;

		if (i == 0) {
			desc_buf[offset + CONFIG_DESC_UNIT_PARAM_LU_EN] = 0x1;
			put_unaligned_be32(
				(u32)target_alloc_units,
				&desc_buf[offset +
					  CONFIG_DESC_UNIT_PARAM_NUM_ALLOC_UNIT]);
		} else {
			desc_buf[offset + CONFIG_DESC_UNIT_PARAM_LU_EN] = 0x0;
			desc_buf[offset + CONFIG_DESC_UNIT_PARAM_BOOT_LU_ID] = 0x0;
			desc_buf[offset + CONFIG_DESC_UNIT_PARAM_LU_WRI_PRO] = 0x0;
			put_unaligned_be32(
				0,
				&desc_buf[offset +
					  CONFIG_DESC_UNIT_PARAM_NUM_ALLOC_UNIT]);
			if (conf_unit_desc >=
			    CONFIG_DESC_UNIT_PARAM_LUN_WB_BUF_ALLOC_UNIT + 4)
				put_unaligned_be32(
					0,
					&desc_buf[offset +
						  CONFIG_DESC_UNIT_PARAM_LUN_WB_BUF_ALLOC_UNIT]);
		}
	}

	if (conf_head_desc > CONFIG_DESC_HEADER_PARAM_HIGH_PRI_LUN) {
		u8 high_pri_lun =
			desc_buf[CONFIG_DESC_HEADER_PARAM_HIGH_PRI_LUN];

		if (high_pri_lun < SPACEMIT_UFS_CONFIG_LUN_SLOTS &&
		    high_pri_lun != 0) {
			dev_dbg(hba->dev,
				"ufs: moving HighPriorityLUN %u to LU0\n",
				high_pri_lun);
			desc_buf[CONFIG_DESC_HEADER_PARAM_HIGH_PRI_LUN] = 0;
		}
	}

	if (conf_head_desc > CONFIG_DESC_HEADER_PARAM_BOOT_EN &&
	    desc_buf[CONFIG_DESC_HEADER_PARAM_BOOT_EN] &&
	    !desc_buf[conf_head_desc + CONFIG_DESC_UNIT_PARAM_BOOT_LU_ID]) {
		dev_dbg(hba->dev,
			"ufs: clearing BootEnable because requested LU0 is not a boot LU\n");
		desc_buf[CONFIG_DESC_HEADER_PARAM_BOOT_EN] = 0;
	}

	spacemit_k3_ufs_dump_conf_header(hba, "requested", desc_buf,
					 conf_head_desc);
	spacemit_k3_ufs_dump_conf_unit(hba, "requested", desc_buf,
					       conf_head_desc, conf_unit_desc, 0);
	if (source_lun > 0)
		spacemit_k3_ufs_dump_conf_unit(hba, "requested", desc_buf,
					       conf_head_desc, conf_unit_desc,
					       source_lun);

	if (!desc_buf[conf_head_desc + CONFIG_DESC_UNIT_PARAM_LOGIC_BLK_SIZE]) {
		dev_err(hba->dev,
			"%s: refusing to write invalid LU0 template with logical block size 0\n",
			__func__);
		ret = -EINVAL;
		goto out;
	}

	/*
	 * Some devices need extra settle time after link/power-mode bring-up
	 * before accepting CONFIGURATION descriptor writes. Earlier debug logs
	 * accidentally provided this delay and hid the timing issue.
	 */
	mdelay(SPACEMIT_K3_UFS_RECONF_SETTLE_MS);

	ret = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_WRITE_DESC,
					    QUERY_DESC_IDN_CONFIGURATION, 0, 0,
					    desc_buf,
					    &hba->desc_size.conf_desc);

		if (ret) {
			int mapped_ret = spacemit_k3_ufs_map_query_error(ret);

			spacemit_k3_ufs_log_unrecoverable_query_error(hba, ret);
			if (orig_desc_buf) {
				spacemit_k3_ufs_dump_conf_state_err(
					hba, "current", orig_desc_buf, conf_head_desc,
				conf_unit_desc, source_lun);
			spacemit_k3_ufs_dump_conf_state_err(
				hba, "requested", desc_buf, conf_head_desc,
				conf_unit_desc, source_lun);
		}

		dev_err(hba->dev, "%s: failed to write config descriptor: %d\n",
			__func__, ret);
		ret = mapped_ret;
		goto out;
	}

	dev_info(
		hba->dev,
		"single-LUN config descriptor written, UFS re-init is required\n");
	ret = 1;

out:
	kfree(orig_desc_buf);
	kfree(desc_buf);
	return ret;
}

static int spacemit_k3_ufs_is_single_lun_active(struct udevice *dev)
{
	u8 *desc_buf;
	u8 unit_lu_enabled[SPACEMIT_UFS_CONFIG_LUN_SLOTS];
	int unit_state_ret;
	int unit_enabled_lun_count = 0;
	int conf_enabled_lun_count = 0;
	int conf_head_desc;
	int conf_unit_desc;
	int ret;
	int i;
	bool have_unit_state;
	bool single_lun_active;
	struct ufs_hba *hba = dev_get_uclass_priv(dev);

	desc_buf = kmalloc(hba->desc_size.conf_desc, GFP_KERNEL);
	if (!desc_buf)
		return -ENOMEM;

	ret = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					    QUERY_DESC_IDN_CONFIGURATION, 0, 0,
					    desc_buf, &hba->desc_size.conf_desc);
	if (ret) {
		dev_err(hba->dev,
			"%s: failed to read config descriptor: %d\n",
			__func__, ret);
		goto out;
	}

	unit_state_ret = spacemit_k3_ufs_read_unit_lu_state(
		hba, unit_lu_enabled, &unit_enabled_lun_count);
	ret = spacemit_k3_ufs_get_conf_desc_layout_from_dev_desc(
		hba, &conf_head_desc, &conf_unit_desc);
	if (ret && unit_state_ret) {
		ret = spacemit_k3_ufs_get_conf_desc_layout(hba, &conf_head_desc,
							   &conf_unit_desc);
	} else if (ret) {
		ret = spacemit_k3_ufs_select_conf_desc_layout(hba, desc_buf,
							      unit_lu_enabled,
							      &conf_head_desc,
							      &conf_unit_desc);
	}
	if (ret) {
		dev_err(hba->dev,
			"%s: unsupported config descriptor layout (len=0x%x)\n",
			__func__, hba->desc_size.conf_desc);
		goto out;
	}

	for (i = 0; i < SPACEMIT_UFS_CONFIG_LUN_SLOTS; i++) {
		int offset = conf_head_desc + conf_unit_desc * i;

		if (desc_buf[offset + CONFIG_DESC_UNIT_PARAM_LU_EN] == 0x1)
			conf_enabled_lun_count++;
	}

	have_unit_state = !unit_state_ret;
	if (have_unit_state)
		single_lun_active =
			(unit_enabled_lun_count == 1 && unit_lu_enabled[0]);
	else
		single_lun_active =
			(conf_enabled_lun_count == 1 &&
			 desc_buf[conf_head_desc + CONFIG_DESC_UNIT_PARAM_LU_EN] == 0x1);

	ret = single_lun_active ? 1 : 0;

out:
	kfree(desc_buf);
	return ret;
}

static int spacemit_k3_ufs_reprobe(struct udevice *dev, const char *reason)
{
	struct spacemit_k3_ufs_priv *priv = dev_get_priv(dev);
	struct ufs_hba *hba = dev_get_uclass_priv(dev);
	struct ufs_hba_ops *hba_ops = (struct ufs_hba_ops *)dev->driver_data;
	int ret;

	if (!hba_ops)
		return -ENODEV;

	if (hba_ops->device_reset)
		hba_ops->device_reset(hba);

	ret = ufshcd_probe(dev, hba_ops);
	if (ret) {
		spacemit_k3_ufs_phy_shutdown(hba, priv);
		spacemit_k3_ufs_clk_disable(priv);
		dev_err(hba->dev, "ufs reprobe after %s failed: %d\n", reason, ret);
		return ret;
	}

	spacemit_k3_ufs_config_scsi_scan_luns(dev);
	return 0;
}

int ufs_prepare_dev_for_flash(int index)
{
	struct udevice *dev;
	struct ufs_hba *hba;
	int ret;

	ret = uclass_get_device(UCLASS_UFS, index, &dev);
	if (ret)
		return ret;

	hba = dev_get_uclass_priv(dev);
	ret = spacemit_k3_ufs_check_and_config_single_lun(dev);
	if (ret < 0) {
		dev_err(hba->dev,
			"ufs: failed to enforce single-LUN layout for flashing: %d\n",
			ret);
		return ret;
	}

	if (!ret)
		return 0;

	dev_info(hba->dev,
		 "restarting UFS once to apply single-LUN layout for flashing\n");
	ret = spacemit_k3_ufs_reprobe(dev, "single-LUN provisioning");
	if (ret)
		return ret;

	ret = spacemit_k3_ufs_is_single_lun_active(dev);
	if (ret < 0) {
		dev_err(hba->dev,
			"ufs: failed to verify single-LUN layout after reprobe: %d\n",
			ret);
		return ret;
	}

	if (!ret) {
		dev_err(hba->dev,
			"ufs: single-LUN layout is still inactive after reprobe\n");
		return -EIO;
	}

	return 0;
}

static void spacemit_k3_ufs_config_scsi_scan_luns(struct udevice *dev)
{
	struct udevice *scsi_dev;
	struct scsi_plat *scsi_plat;
	struct ufs_hba *hba = dev_get_uclass_priv(dev);
	u8 unit_lu_enabled[SPACEMIT_UFS_CONFIG_LUN_SLOTS];
	unsigned long scan_luns = SPACEMIT_UFS_CONFIG_LUN_SLOTS;
	int enabled_lun_count = 0;
	int highest_enabled_lun = -1;
	int ret;
	int i;

	device_find_first_child(dev, &scsi_dev);
	if (!scsi_dev) {
		dev_err(hba->dev, "ufs: scsi_dev child not found!\n");
		return;
	}

	scsi_plat = dev_get_uclass_plat(scsi_dev);
	scsi_plat->max_id = 1; /* UFS has a single target */

	ret = spacemit_k3_ufs_read_unit_lu_state(hba, unit_lu_enabled,
						 &enabled_lun_count);
	if (ret) {
		dev_warn(hba->dev,
			 "ufs: failed to read unit LU state (%d), scanning first %lu LUNs\n",
			 ret, scan_luns);
		scsi_plat->max_lun = scan_luns;
		return;
	}

	for (i = 0; i < SPACEMIT_UFS_CONFIG_LUN_SLOTS; i++) {
		if (unit_lu_enabled[i])
			highest_enabled_lun = i;
	}

	/*
	 * The generic SCSI scan walks LUN 0..max_lun-1 sequentially, so scan
	 * up to the highest enabled LUN to cover every active unit.
	 */
	if (highest_enabled_lun >= 0)
		scan_luns = highest_enabled_lun + 1;
	else
		scan_luns = 1;

	scsi_plat->max_lun = scan_luns;
	dev_dbg(hba->dev,
		"ufs: scanning %lu LUN slot(s), enabled user LUN count=%d highest=%d\n",
		scan_luns, enabled_lun_count, highest_enabled_lun);
}

static int spacemit_k3_ufs_wait_mphy_pll_lock(struct ufs_hba *hba,
					      const char *where)
{
	struct spacemit_k3_ufs_priv *priv = dev_get_priv(hba->dev);
	u32 reg_val = 0;
	int timeout = 10000;

	while (timeout-- > 0) {
		reg_val = ufshcd_readl(hba, priv->phy_mng_base + UFS_MPHY_PU_CTRL);
		if (reg_val & UFS_MPHY_PU_PLL_LOCK)
			return 0;
		udelay(1);
	}

	dev_err(hba->dev,
		"%s: M-PHY PLL lock timeout, UFS_MPHY_PU_CTRL=0x%08x\n",
		where, reg_val);
	return -ETIMEDOUT;
}

static int spacemit_k3_ufs_mphy_init(struct ufs_hba *hba)
{
	struct udevice *dev = hba->dev;
	struct spacemit_k3_ufs_priv *priv = dev_get_priv(dev);
	int ret;

	/* reset all mphy logical */
	ufshcd_writel(hba, 0x003, priv->phy_mng_base + UFS_MPHY_RST_CTRL);
	mdelay(1);

	/* power up all */
	ufshcd_writel(hba, 0x87f, priv->phy_mng_base + UFS_MPHY_PU_CTRL);
	mdelay(1);

	/* asserted ana_rx_hb8_reset */
	ufshcd_writel(hba, 0xb7f,
		      priv->phy_mng_base + UFS_MPHY_PU_CTRL);
	mdelay(1);

	/* deasserted ana_rx_hb8_reset */
	ufshcd_writel(hba, 0x87f, priv->phy_mng_base + UFS_MPHY_PU_CTRL);
	mdelay(1);

	/* deasserted ufs device reset & refer clk output enable */
	ufshcd_writel(hba, 0x101,
		      priv->phy_mng_base + UFS_DEVICE_IO_CTRL);
	mdelay(1);

	ret = spacemit_k3_ufs_wait_mphy_pll_lock(hba, __func__);
	if (ret)
		return ret;

	ufshcd_writel(hba, 0x1, priv->phy_mng_base + UFS_MPHY_BKDR_CTRL);
	udelay(20);

	ufshcd_writel(hba, 0x00, priv->atop_base + (ANA_HSGEAR_CTRL_ATTR << 2));
	ufshcd_writel(hba, 0x00, priv->atop_base + (0xC2 << 2));
	udelay(20);

	ufshcd_writel(hba, 0x0, priv->phy_mng_base + UFS_MPHY_BKDR_CTRL);
	udelay(20);

	/* HYNIX1 phone: extra settle time after MPHY tuning */
	mdelay(5);

	pr_debug("ufs: ufs_spacemit_k3_mphy_init done\n");

	return 0;
}

static void spacemit_k3_ufs_phy_shutdown(struct ufs_hba *hba,
					 struct spacemit_k3_ufs_priv *priv)
{
	ufshcd_writel(hba, 0x000, priv->phy_mng_base + UFS_DEVICE_IO_CTRL);
	udelay(20);

	ufshcd_writel(hba, 0x000, priv->phy_mng_base + UFS_MPHY_RST_CTRL);
	udelay(20);

	ufshcd_writel(hba, 0x000, priv->phy_mng_base + UFS_MPHY_PU_CTRL);
	udelay(20);
}

static int spacemit_k3_ufs_unipro_init(struct ufs_hba *hba)
{
	int err;
	u32 real_sysclk, reg_val;
	u32 tx_symbol_clk_ns_us;

	/* PA_TXHSG1SYNCLENGTH */
	err = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXHSG1SYNCLENGTH), 0x4f);
	if (err) {
		pr_err("Writing PA_TXHSG1SYNCLENGTH error \n");
	}
	/* PA_TXHSG1PREPARELENGTH */
	err = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXHSG1PREPARELENGTH), 0xf);
	if (err) {
		pr_err("Writing PA_TXHSG1PREPARELENGTH error \n");
	}

	/* PA_TXHSG2SYNCLENGTH */
	err = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXHSG2SYNCLENGTH), 0x4f);
	if (err) {
		pr_err("Writing PA_TXHSG2SYNCLENGTH error \n");
	}
	/* PA_TXHSG2PREPARELENGTH */
	err = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXHSG2PREPARELENGTH), 0xf);
	if (err) {
		pr_err("Writing PA_TXHSG2PREPARELENGTH error \n");
	}

	/* PA_TXHSG3SYNCLENGTH */
	err = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXHSG3SYNCLENGTH), 0x4f);
	if (err) {
		pr_err("Writing PA_TXHSG3SYNCLENGTH error \n");
	}
	/* PA_TXHSG3PREPARELENGTH */
	err = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXHSG3PREPARELENGTH), 0xf);
	if (err) {
		pr_err("Writing PA_TXHSG3PREPARELENGTH error \n");
	}

	/* PA_TXMK2EXTENSION */
	err = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXMK2EXTENSION), 0x0);
	if (err) {
		pr_err("Writing PA_TXMK2EXTENSION error \n");
	}

	/* PA_PEERSCRAMBLING */
	err = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_PEERSCRAMBLING), 0x1);
	if (err) {
		pr_err("Writing PA_PEERSCRAMBLING error \n");
	}

	/* PA_TXSKIP */
	err = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXSKIP), 0x1);
	if (err) {
		pr_err("Writing PA_TXSKIP error \n");
	}
	/* PA_TXSKIPPERIOD */
	err = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXSKIPPERIOD), 250);
	if (err) {
		pr_err("Writing PA_TXSKIPPERIOD error \n");
	}

	/* PA_LOCAL_TX_LCC_ENABLE */
	err = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_LOCAL_TX_LCC_ENABLE), 0x0);
	if (err) {
		pr_err("Writing PA_LOCAL_TX_LCC_ENABLE error \n");
	}
	/* PA_PEER_TX_LCC_ENABLE */
	err = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_PEER_TX_LCC_ENABLE), 0x0);
	if (err) {
		pr_err("Writing PA_PEER_TX_LCC_ENABLE error \n");
	}

	/* PA_SCRAMBLING */
	err = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_SCRAMBLING), 0x1);
	if (err) {
		pr_err("Writing PA_SCRAMBLING error \n");
	}

	/* PA_GRANULARITY */
	err = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_GRANULARITY), 0x1);
	if (err) {
		pr_err("Writing PA_GRANULARITY error \n");
	}

	/* PA_MK2EXTENSIONGUARDBAND */
	err = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_MK2EXTENSIONGUARDBAND), 0x0);
	if (err) {
		pr_err("Writing PA_MK2EXTENSIONGUARDBAND error \n");
	}

	/* PA_TACTIVATE */
	err = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TACTIVATE), 0x64);
	if (err) {
		pr_err("Writing PA_TACTIVATE error \n");
	}
	/* PA_TXTRAILINGCLOCKS */
	err = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXTRAILINGCLOCKS), 0x64);
	if (err) {
		pr_err("Writing PA_TXTRAILINGCLOCKS error \n");
	}

	{
		/* PA_STALLNOCONFIGTIME */
		err = ufshcd_dme_set(hba, UIC_ARG_MIB(PA_STALLNOCONFIGTIME),
				     15);
		if (err) {
			pr_err("Writing PA_STALLNOCONFIGTIME error \n");
		}

		/* RX_LS_PREPARELEN_TIME RX0 */
		err = ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_LS_PRE_LEN_CAP, 4),
				     0x0B);
		if (err) {
			pr_err("Writing RX_LS_PREPARELEN_TIME RX0 error \n");
		}

		/* RX_LS_PREPARELEN_TIME RX1 */
		err = ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_LS_PRE_LEN_CAP, 5),
				     0X0B);
		if (err) {
			pr_err("Writing RX_LS_PREPARELEN_TIME RX1 error \n");
		}

		/* RX_HIBERNATE_BKEN RX0 */
		err = ufshcd_dme_set(
			hba,
			UIC_ARG_MIB_SEL(RX_LANE_HB8_BKDOOR_ATTR,
					UIC_ARG_MPHY_RX_GEN_SEL_INDEX(0)),
			0x9F);
		if (err) {
			pr_err("Writing RX_HIBERNATE_BKEN RX0 error \n");
		}

		/* RX_HIBERNATE_BKEN RX1 */
		err = ufshcd_dme_set(
			hba,
			UIC_ARG_MIB_SEL(RX_LANE_HB8_BKDOOR_ATTR,
					UIC_ARG_MPHY_RX_GEN_SEL_INDEX(1)),
			0x9F);
		if (err) {
			pr_err("Writing RX_HIBERNATE_BKEN RX1 error \n");
		}

		/* PWM_BURST_closure_length */
		err = ufshcd_dme_set(
			hba,
			UIC_ARG_MIB_SEL(RX_PWRM_CLOSURE_LEN_CAP,
					UIC_ARG_MPHY_RX_GEN_SEL_INDEX(0)),
			15);
		err = ufshcd_dme_set(
			hba,
			UIC_ARG_MIB_SEL(RX_PWRM_CLOSURE_LEN_CAP,
					UIC_ARG_MPHY_RX_GEN_SEL_INDEX(1)),
			15);

		/* min_stall_not_config_time*/
		err = ufshcd_dme_set(
			hba,
			UIC_ARG_MIB_SEL(RX_MIN_STALL_CAP,
					UIC_ARG_MPHY_RX_GEN_SEL_INDEX(0)),
			0xFF);
		err = ufshcd_dme_set(
			hba,
			UIC_ARG_MIB_SEL(RX_MIN_STALL_CAP,
					UIC_ARG_MPHY_RX_GEN_SEL_INDEX(1)),
			0xFF);

		/* TX HB8_TIME CAP */
		err = ufshcd_dme_set(
			hba,
			UIC_ARG_MIB_SEL(TX_HIBERN8TIME_CAPABILITY,
					UIC_ARG_MPHY_TX_GEN_SEL_INDEX(0)),
			0x64);
		err = ufshcd_dme_set(
			hba,
			UIC_ARG_MIB_SEL(TX_HIBERN8TIME_CAPABILITY,
					UIC_ARG_MPHY_TX_GEN_SEL_INDEX(1)),
			0x64);

		/*RX HB8_TIME CAP*/
		err = ufshcd_dme_set(
			hba,
			UIC_ARG_MIB_SEL(RX_HIBERN8TIME_CAPABILITY,
					UIC_ARG_MPHY_RX_GEN_SEL_INDEX(0)),
			0x64);
		err = ufshcd_dme_set(
			hba,
			UIC_ARG_MIB_SEL(RX_HIBERN8TIME_CAPABILITY,
					UIC_ARG_MPHY_RX_GEN_SEL_INDEX(1)),
			0x64);

		/*TX EQ 3DB*/
		err = ufshcd_dme_set(
			hba,
			UIC_ARG_MIB_SEL(ANA_EQ_CTRL_REG_ATTR,
					UIC_ARG_MPHY_TX_GEN_SEL_INDEX(0)),
			0x5);

		/*RX garbage cnt = 32 SI*/
		err = ufshcd_dme_set(
			hba,
			UIC_ARG_MIB_SEL(RX_LANE_SOF_BKDOOR_ATT,
					UIC_ARG_MPHY_RX_GEN_SEL_INDEX(0)),
			0x9F);
		err = ufshcd_dme_set(
			hba,
			UIC_ARG_MIB_SEL(RX_LANE_SOF_BKDOOR_ATT,
					UIC_ARG_MPHY_RX_GEN_SEL_INDEX(1)),
			0x9F);
	}

	/* bypass B0 reduce phy power ECO */
	err = ufshcd_dme_set(hba, UIC_ARG_MIB(0xfc), 0xfc);
	if (err)
		pr_err("Writing 0xfc error \n");

	pr_debug("ufs: ufs_spacemit_k3_uniprov1p6_init done.\n");

	err = spacemit_k3_ufs_get_sys1clk_1us(hba, &real_sysclk);
	if (err) {
		dev_err(hba->dev, "ufs: failed to derive SYS1CLK_1US: %d\n",
			err);
		return err;
	}

	ufshcd_writel(hba, real_sysclk, UFS_SYS1CLK_1US_REG);

	tx_symbol_clk_ns_us = DIV_ROUND_CLOSEST(1000U, real_sysclk);
	if (!tx_symbol_clk_ns_us)
		tx_symbol_clk_ns_us = 1;

	reg_val = tx_symbol_clk_ns_us << 10;
	ufshcd_writel(hba, reg_val, UFS_TX_SYMBOL_CLK_NS_US_REG);

	reg_val = real_sysclk * 100000;
	reg_val &= ~0xfU;
	ufshcd_writel(hba, reg_val, UFS_PA_LINK_STARTUP_TIMER_REG);

	/* HYNIX1 phone need delay*/
	mdelay(5);

	pr_debug("ufs: UFS_PA_LINK_STARTUP_TIMER_REG(0xd8) val: 0x%x\n",
		 ufshcd_readl(hba, UFS_PA_LINK_STARTUP_TIMER_REG));
	pr_debug("ufs: REG_UFS_SYS1CLK_1US(0xc0) val: 0x%x\n",
		 ufshcd_readl(hba, UFS_SYS1CLK_1US_REG));
	pr_debug("ufs: REG_UFS_TX_SYMBOL_CLK_NS_US(0xc4) val: 0x%x\n",
		 ufshcd_readl(hba, UFS_TX_SYMBOL_CLK_NS_US_REG));

	return 0;
}

static int spacemit_k3_ufs_silent_reset(struct ufs_hba *hba)
{
	struct udevice *dev = hba->dev;
	struct spacemit_k3_ufs_priv *priv = dev_get_priv(dev);

	/* stop device ref_clk & asserted ufs device reset */
	spacemit_k3_ufs_phy_shutdown(hba, priv);
	return 0;
}

static int
spacemit_k3_ufs_link_startup_notify(struct ufs_hba *hba,
				    enum ufs_notify_change_status status)
{
	uint32_t reg_val;

	pr_debug("ufs: spacemit_k3_ufs_link_startup_notify, status:%d\n",
		 status);
	if (status == PRE_CHANGE) {
		/* init is done in hce_enable_notify(POST_CHANGE) */
		return 0;
	}

	if (status == POST_CHANGE) {
		/*check status after send dme link_up*/
		reg_val = ufshcd_readl(hba, REG_CONTROLLER_STATUS);
		if (!(reg_val & DEVICE_PRESENT)) {
			pr_err("ufs: DEVICE_PRESENT !!!FAIL! read REG_CONTROLLER_STATUS:0x%x\n",
			       reg_val);
			return -ENODEV;
		}

		reg_val = ufshcd_readl(hba, REG_INTERRUPT_STATUS);
		pr_debug("ufs: REG_INTERRUPT_STATUS before clear (0x%x):0x%x\n",
			 REG_INTERRUPT_STATUS, reg_val);

		if (reg_val & UIC_LINK_STARTUP)
			ufshcd_writel(hba, UIC_LINK_STARTUP,
				      REG_INTERRUPT_STATUS);
		if (reg_val & UIC_ERROR)
			ufshcd_writel(hba, UIC_ERROR, REG_INTERRUPT_STATUS);

		reg_val = ufshcd_readl(hba, REG_INTERRUPT_STATUS);
		pr_debug("ufs: REG_INTERRUPT_STATUS after clear (0x%x):0x%x\n",
			 REG_INTERRUPT_STATUS, reg_val);

		reg_val =
			ufshcd_readl(hba, REG_UIC_ERROR_CODE_PHY_ADAPTER_LAYER);
		pr_debug(
			"ufs: REG_UIC_ERROR_CODE_PHY_ADAPTER_LAYER(0x%x):0x%x\n",
			REG_UIC_ERROR_CODE_PHY_ADAPTER_LAYER, reg_val);

		/* add 0xe8 make UFS2.1 run GEAR3+2Lane@409M*/
		mdelay(5);
		ufshcd_dme_set(
			hba,
			UIC_ARG_MIB_SEL(0xe8, UIC_ARG_MPHY_TX_GEN_SEL_INDEX(0)),
			0x97);
		mdelay(1);
		ufshcd_dme_set(
			hba,
			UIC_ARG_MIB_SEL(0xe8, UIC_ARG_MPHY_TX_GEN_SEL_INDEX(0)),
			0xd7);
		mdelay(1);
		ufshcd_dme_set(
			hba,
			UIC_ARG_MIB_SEL(0xe8, UIC_ARG_MPHY_TX_GEN_SEL_INDEX(0)),
			0x17);

		/* DL_AFC0REQTIMEOUTVAL_MAX */
		ufshcd_dme_set(hba, UIC_ARG_MIB(DL_AFC0REQTIMEOUTVAL),
			       UFS_DL_AFC0REQTIMEOUTVAL_MAX);

		/*LCC_DISABLE*/
		mdelay(5);
		ufshcd_dme_set(
			hba,
			UIC_ARG_MIB_SEL(TX_LCC_ENABLE,
					UIC_ARG_MPHY_TX_GEN_SEL_INDEX(0)),
			0);
		mdelay(1);
		ufshcd_dme_set(
			hba,
			UIC_ARG_MIB_SEL(TX_LCC_ENABLE,
					UIC_ARG_MPHY_TX_GEN_SEL_INDEX(1)),
			0);

		/*TX_Min_ActivateTime*/
		mdelay(1);
		ufshcd_dme_set(
			hba,
			UIC_ARG_MIB_SEL(TX_MIN_ACTIVATETIME,
					UIC_ARG_MPHY_TX_GEN_SEL_INDEX(0)),
			0x0);
		mdelay(1);
		ufshcd_dme_set(
			hba,
			UIC_ARG_MIB_SEL(TX_MIN_ACTIVATETIME,
					UIC_ARG_MPHY_TX_GEN_SEL_INDEX(1)),
			0x0);
		mdelay(10);

		/* use backdoor reg to pre-set TX RATE/GEAR to let PLL lock before set_power_mode
		 * switch */
		ufshcd_dme_set(hba, UIC_ARG_MIB(ANA_HSGEAR_CTRL_ATTR), 0x25);
		mdelay(10);

		if (spacemit_k3_ufs_wait_mphy_pll_lock(hba, __func__))
			return -ETIMEDOUT;
	}

	return 0;
}

static int
spacemit_k3_ufs_hce_enable_notify(struct ufs_hba *hba,
				  enum ufs_notify_change_status status)
{
	int ret;

	pr_debug("ufs: spacemit_k3_ufs_hce_enable_notify, status:%d\n", status);

	if (status == PRE_CHANGE) {
		/*do nothing*/
	}

	if (status == POST_CHANGE) {
		ret = spacemit_k3_ufs_mphy_init(hba);
		if (ret)
			return ret;

		ret = spacemit_k3_ufs_unipro_init(hba);
		if (ret)
			return ret;

		/* Disable auto-hibern8 during bringup */
		ufshcd_writel(hba, 0, REG_AUTO_HIBERNATE_IDLE_TIMER);
	}

	return 0;
}

static int spacemit_k3_ufs_init(struct ufs_hba *hba)
{
	/* Mirror Linux behavior: disable LCC for controller stability */
	hba->quirks |= UFSHCD_QUIRK_BROKEN_LCC;

	return 0;
}

static const struct ufs_hba_ops spacemit_k3_ufs_vops = {
	.init = spacemit_k3_ufs_init,
	.hce_enable_notify = spacemit_k3_ufs_hce_enable_notify,
	.link_startup_notify = spacemit_k3_ufs_link_startup_notify,
	.device_reset = spacemit_k3_ufs_silent_reset,
	.set_ref_clk = spacemit_k3_ufs_set_ref_clk,
	.set_power_mode = spacemit_k3_ufs_set_power_mode,
};

static int spacemit_k3_ufs_pltfm_probe(struct udevice *dev)
{
	struct spacemit_k3_ufs_priv *priv = dev_get_priv(dev);
	struct ufs_hba *hba = dev_get_uclass_priv(dev);
	struct ufs_hba_ops *hba_ops = (struct ufs_hba_ops *)dev->driver_data;
	int ret;
	int retries;

	/* Bring clocks/reset up as early as possible */
	ret = spacemit_k3_ufs_clk_enable(dev);
	if (ret)
		return ret;

#if defined(CONFIG_SPL_BUILD)
	/*
	 * SPL often starts from a warm hardware state. Pre-shutdown once so the
	 * first ufshcd_probe starts from the same clean state as retry path.
	 */
	hba->dev = dev;
	hba->ops = hba_ops;
	hba->mmio_base = (void *)dev_read_addr(dev);
	if (hba->ops && hba->ops->device_reset) {
		hba->ops->device_reset(hba);
	}
#endif

	for (retries = 3; retries > 0; retries--) {
		ret = ufshcd_probe(dev, hba_ops);
		if (!ret) {
			break;
		}
		hba->ops->device_reset(hba);
	}

	if (ret) {
		spacemit_k3_ufs_phy_shutdown(hba, priv);
		spacemit_k3_ufs_clk_disable(priv);
		pr_err("ufs host probe failed:%d\n", ret);
	} else {
		/*
		 * Scan the LUNs that are actually enabled on the device.
		 * Single-LUN provisioning is handled explicitly by flashing paths.
		 */
		spacemit_k3_ufs_config_scsi_scan_luns(dev);
	}

	return ret;
}

static int spacemit_k3_ufs_of_to_plat(struct udevice *dev)
{
	const char *compat;
	const void *prop;
	int compat_length;
	int ret;
	u32 ref_clk_freq;

	struct spacemit_k3_ufs_priv *priv = dev_get_priv(dev);

	compat = ofnode_get_property(dev_ofnode(dev), "compatible", &compat_length);
	if (!compat) {
		return -1;
	}

	if (!strcmp(compat, "spacemit,k3-ufshci")) {
		priv->phy_mng_base = UFS_ARASAN_PHY_MNG_BASE;
		priv->atop_base = UFS_ARASAN_TOP_BASE;
	}

	/*
	 * Read reference clock frequency from DTS. It must be expressed in Hz
	 * and match one of the UFS-spec reference clock frequencies.
	 */
	prop = dev_read_prop(dev, "ref-clk-freq", NULL);
	if (!prop) {
		/* Default to 19.2MHz if not specified in DTS */
		priv->ref_clk_freq = UFS_REF_CLK_FREQ_19_2_MHZ;
		dev_dbg(dev,
			"ufs: ref-clk-freq not found in DTS, using default 19.2MHz\n");
	} else {
		ret = dev_read_u32(dev, "ref-clk-freq", &ref_clk_freq);
		if (ret) {
			dev_err(dev,
				"ufs: malformed ref-clk-freq property, ret=%d\n",
				ret);
			return ret;
		}

		ret = spacemit_k3_ufs_parse_ref_clk_freq(ref_clk_freq,
								&priv->ref_clk_freq);
		if (ret) {
			dev_err(dev,
				"ufs: invalid ref-clk-freq %u, expected "
				"19200000/26000000/38400000/52000000 Hz\n",
				ref_clk_freq);
			return ret;
		}
	}

	ret = clk_get_by_name(dev, "refclk", &priv->refclk);
	if (ret == -ENODATA || ret == -ENOENT || ret == -ENOSYS) {
		memset(&priv->refclk, 0, sizeof(priv->refclk));
	} else if (ret) {
		dev_err(dev, "ufs: failed to get refclk, ret=%d\n", ret);
		return ret;
	}

	ret = dev_read_u32(dev, "clock-freq", &priv->clock_freq_hz);
	if (ret)
		priv->clock_freq_hz = 0;

	ret = clk_get_by_name(dev, "aclk", &priv->aclk);
	if (ret) {
		dev_warn(dev,
			 "ufs: failed to get aclk by name, fallback to index 0, ret=%d\n",
			 ret);
		ret = clk_get_by_index(dev, 0, &priv->aclk);
	}
	if (ret) {
		dev_err(dev, "ufs: failed to get aclk, ret=%d\n", ret);
		return ret;
	}

	ret = reset_get_by_name(dev, "aclk_reset", &priv->reset);
	if (ret) {
		dev_warn(dev,
			 "ufs: failed to get reset by name, fallback to index 0, ret=%d\n",
			 ret);
		ret = reset_get_by_index(dev, 0, &priv->reset);
	}
	if (ret) {
		dev_err(dev, "ufs: failed to get reset, ret=%d\n", ret);
		return ret;
	}

	return 0;
}

static int spacemit_k3_ufs_pltfm_remove(struct udevice *dev)
{
	struct spacemit_k3_ufs_priv *priv = dev_get_priv(dev);

	spacemit_k3_ufs_clk_disable(priv);

	return 0;
}

static const struct udevice_id spacemit_k3_ufs_pltfm_ids[] = {
	{
		.compatible = "spacemit,k3-ufshci",
		.data = (ulong)&spacemit_k3_ufs_vops,
	},
	{ /* sentinel */ }
};

U_BOOT_DRIVER(spacemit_k3_ufs) = {
	.name = "ufs-spacemit_k3",
	.id = UCLASS_UFS,
	.of_match = spacemit_k3_ufs_pltfm_ids,
	.of_to_plat = spacemit_k3_ufs_of_to_plat,
	.probe = spacemit_k3_ufs_pltfm_probe,
	.remove = spacemit_k3_ufs_pltfm_remove,
	.priv_auto = sizeof(struct spacemit_k3_ufs_priv),
};
