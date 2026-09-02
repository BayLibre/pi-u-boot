// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2025 Spacemit Co., Ltd.
 *
 */

#include "inno_dp.h"
#include "inno_dp_reg.h"

#define dev_info(dev, fmt, ...) \
	pr_info("[DP PHY INFO] " fmt, ##__VA_ARGS__)
#define dev_err(dev, fmt, ...) \
	pr_info("[DP PHY INFO] " fmt, ##__VA_ARGS__)
#define dev_warn(dev, fmt, ...) \
	pr_info("[DP PHY INFO] " fmt, ##__VA_ARGS__)
#define dev_dbg(dev, fmt, ...) \
	pr_debug("[DP PHY DEBUG] " fmt, ##__VA_ARGS__)

/* Error Codes */
#define EINVAL					-22
#define ETIMEDOUT				-110
#define EIO					-5
#define EBUSY					-16
#define EAGAIN					-11

#define DP_AUX_I2C_WRITE			0x0
#define DP_AUX_I2C_READ				0x1
#define DP_AUX_I2C_MOT				0x4
#define DP_AUX_NATIVE_WRITE			0x8
#define DP_AUX_NATIVE_READ			0x9

#define DP_AUX_NATIVE_REPLY_ACK			0x00
#define DP_AUX_NATIVE_REPLY_NACK		0x01
#define DP_AUX_NATIVE_REPLY_DEFER		0x02

#define DP_DPCD_REV				0x000
#define DP_MAX_LINK_RATE			0x001
#define DP_MAX_LANE_COUNT			0x002
#define DP_MAX_LANE_COUNT_MASK			0x1f
#define DP_TPS3_SUPPORTED			BIT(6)
#define DP_ENHANCED_FRAME_CAP			BIT(7)

#define DP_LINK_BW_SET				0x100
#define DP_LINK_BW_1_62				0x06
#define DP_LINK_BW_2_7				0x0a
#define DP_LINK_BW_5_4				0x14
#define DP_LINK_BW_8_1				0x1e

#define DP_LANE_COUNT_SET			0x101
#define DP_LANE_COUNT_ENHANCED_FRAME_EN		BIT(7)

#define DP_TRAINING_PATTERN_SET			0x102
#define DP_TRAINING_PATTERN_DISABLE		0
#define DP_TRAINING_PATTERN_1			1
#define DP_TRAINING_PATTERN_2			2
#define DP_TRAINING_PATTERN_3			3
#define DP_LINK_SCRAMBLING_DISABLE		BIT(5)

#define DP_TRAINING_LANE0_SET			0x103
#define DP_TRAIN_VOLTAGE_SWING_MASK		0x3
#define DP_TRAIN_PRE_EMPHASIS_MASK		(3 << 3)
#define DP_TRAIN_PRE_EMPHASIS_SHIFT		3
#define DP_TRAIN_MAX_SWING_REACHED		BIT(2)
#define DP_TRAIN_MAX_PRE_EMPHASIS_REACHED	BIT(5)

#define DP_DOWNSPREAD_CTRL			0x107

#define DP_MAIN_LINK_CHANNEL_CODING_SET		0x108
#define DP_SET_ANSI_8B10B			BIT(0)

#define DP_TRAINING_AUX_RD_INTERVAL		0x00e
#define DP_TRAINING_AUX_RD_MASK			0x7f

#define DP_LANE0_1_STATUS			0x202
#define DP_LANE_CR_DONE				BIT(0)
#define DP_LANE_CHANNEL_EQ_DONE			BIT(1)
#define DP_LANE_SYMBOL_LOCKED			BIT(2)

#define DP_EDP_CONFIGURATION_SET		0x10a
#define DP_ALTERNATE_SCRAMBLER_RESET_ENABLE	BIT(0)
#define DP_FRAMING_CHANGE_ENABLE		BIT(1)
#define DP_PANEL_SELF_TEST_ENABLE		BIT(7)

#define DP_ADJUST_REQUEST_LANE0_1		0x206

#define DP_SET_POWER				0x600
#define DP_SET_POWER_D0				0x1

#define DP_LINK_STATUS_SIZE			6

#define SOC_DP_SWING_MAX			2
#define SOC_DP_PREEMP_MAX			2

static const struct soc_dp_link_config {
	enum soc_dp_link_rate rate;
	enum soc_dp_lane_count lanes;
} soc_dp_link_priority_table[] = {
	/* --- Tier 1: Low Bandwidth (< 4 Gbps) --- */
	{SOC_DP_LINK_RATE_1_62, SOC_DP_LANE_1},	/* 1.62 Gbps */
	{SOC_DP_LINK_RATE_2_70, SOC_DP_LANE_1},	/* 2.70 Gbps */
	{SOC_DP_LINK_RATE_1_62, SOC_DP_LANE_2},	/* 3.24 Gbps */

	/* --- Tier 2: Medium Bandwidth (~5-6 Gbps) --- */
	{SOC_DP_LINK_RATE_2_70, SOC_DP_LANE_2},	/* 5.40 Gbps */
	{SOC_DP_LINK_RATE_1_62, SOC_DP_LANE_4},	/* 6.48 Gbps */

	/* --- Tier 3: High Bandwidth (~10 Gbps) --- */
	{SOC_DP_LINK_RATE_2_70, SOC_DP_LANE_4},	/* 10.8 Gbps */
	{SOC_DP_LINK_RATE_5_40, SOC_DP_LANE_2},	/* 10.8 Gbps */

	/* --- Tier 4: Ultra High Bandwidth (> 17 Gbps) --- */
	{SOC_DP_LINK_RATE_5_40, SOC_DP_LANE_4},	/* 21.6 Gbps */
};

static const struct soc_format_info {
	u8 bpp; /* Bits Per Pixel */
} format_info_table[] = {
	[SOC_VIDEO_RGB_6BIT]      = { .bpp = 18 },
	[SOC_VIDEO_RGB_8BIT]      = { .bpp = 24 },
	[SOC_VIDEO_RGB_10BIT]     = { .bpp = 30 },
	[SOC_VIDEO_RGB_12BIT]     = { .bpp = 36 },
	[SOC_VIDEO_RGB_16BIT]     = { .bpp = 48 },

	[SOC_VIDEO_YUV444_8BIT]   = { .bpp = 24 },
	[SOC_VIDEO_YUV444_10BIT]  = { .bpp = 30 },
	[SOC_VIDEO_YUV444_12BIT]  = { .bpp = 36 },
	[SOC_VIDEO_YUV444_16BIT]  = { .bpp = 48 },

	[SOC_VIDEO_YUV422_8BIT]   = { .bpp = 16 },
	[SOC_VIDEO_YUV422_10BIT]  = { .bpp = 20 },
	[SOC_VIDEO_YUV422_12BIT]  = { .bpp = 24 },
	[SOC_VIDEO_YUV422_16BIT]  = { .bpp = 32 },
};

static int soc_dp_get_bpp(u32 format)
{
	if (format >= ARRAY_SIZE(format_info_table)) {
		pr_warn("DP: Invalid color format index %d, defaulting to RGB888\n", format);
		return 24;
	}
	return format_info_table[format].bpp;
}

/*
 * soc_dp_div64
 * @n: Pointer to dividend (will be updated to quotient)
 * @base: Divisor
 * Return: Remainder
 */
static u32 soc_dp_div64(u64 *n, u32 base)
{
#if USED_ACTIVATE_DO_DIV
	return do_div(*n, base);
#else
	u32 rem = *n % base;
	*n = *n / base;
	return rem;
#endif
}

static int soc_dp_reg_write(struct soc_dp_dev *dp,
			    u32 offset, u32 bit_wide, u32 mask, u32 val)
{
	u32 reg_val;

	reg_val = (u32)readl((char *)dp->regs + offset);
	reg_val &= ~mask;
	reg_val |= val & mask;
	// pr_info("[W] 0x%x 0x%x\n", offset, reg_val);
	writel(reg_val, (char *)dp->regs + offset);

	return 0;
}

static int soc_dp_reg_write_range(struct soc_dp_dev *dp,
				  u32 offset, u32 high, u32 low, u32 val)
{
	u32 mask;

	mask = (u32)(((((u64)1) << (high - low + 1)) - 1) << low);
	return soc_dp_reg_write(dp, offset, 32, mask, (val << low) & mask);
}

static int soc_dp_reg_only_write_range(struct soc_dp_dev *dp,
				       u32 offset, u32 high, u32 low, u32 val)
{
	u32 mask;

	mask = (u32)(((((u64)1) << (high - low + 1)) - 1) << low);
	writel((val << low) & mask, (char *)dp->regs + offset);
	return 0;
}

static int soc_dp_reg_read(struct soc_dp_dev *dp,
			   u32 offset, u32 bit_wide, u32 mask, u32 *val)
{
	*val = ((u32)readl((char *)dp->regs + offset)) & mask;
	// pr_info("[R] 0x%x 0x%x\n", offset, *val);
	return 0;
}

static int soc_dp_reg_read_range(struct soc_dp_dev *dp,
				 u32 offset, u32 high, u32 low, u32 *val)
{
	int ret;
	u32 mask;

	mask = (u32)(((((u64)1) << (high - low + 1)) - 1) << low);
	ret = soc_dp_reg_read(dp, offset, 32, mask, val);
	*val = *val >> low;

	return ret;
}

static void soc_dp_aux_hw_reset(struct soc_dp_dev *dp)
{
	soc_dp_reg_write_range(dp, SOC_DPTX_AUX_RESET, 0x1);
	mdelay(2);
	soc_dp_reg_write_range(dp, SOC_DPTX_AUX_RESET, 0x0);
	mdelay(2);
	soc_dp_reg_write_range(dp, SOC_DPTX_AUX_REPLY_EVENT_INT_STA, 1);
}

/*
 * Low-level AUX transfer function.
 * Returns bytes transferred on success (>=0), or negative error code on failure.
 */
static int soc_dp_aux_transfer_raw(struct soc_dp_dev *dp, u32 request, u32 address, u8 *buf, int size)
{
	int ret, i;
	unsigned long timeout_cnt = 0;

	u32 cmd, len, val, status;
	u32 data[4] = {0};
	bool is_read = (request & DP_AUX_I2C_READ) || ((request & DP_AUX_NATIVE_READ) == DP_AUX_NATIVE_READ);

	/* 1. Check message validity */
	if (size > 16)
		return -EINVAL;

	cmd = request;

	/* 2. Prepare Data for Write (if applicable) */
	if (!is_read) {
		/* Pack bytes into 32-bit words (Little Endian packing) */
		for (i = 0; i < size; i++) {
			data[i / 4] |= buf[i] << ((i % 4) * 8);
		}

		/* Write data to registers: DATA1(LSB)..DATA4(MSB) */
		soc_dp_reg_write_range(dp, SOC_DPTX_AUX_DATA1, data[0]);
		soc_dp_reg_write_range(dp, SOC_DPTX_AUX_DATA2, data[1]);
		soc_dp_reg_write_range(dp, SOC_DPTX_AUX_DATA3, data[2]);
		soc_dp_reg_write_range(dp, SOC_DPTX_AUX_DATA4, data[3]);
	}

	/* 3. Configure Command, Address, Length */
	/* HW expects Length - 1 */
	len = size > 0 ? size - 1 : 0;

	soc_dp_reg_write_range(dp, SOC_DPTX_AUX_LENGTH, len);
	soc_dp_reg_write_range(dp, SOC_DPTX_AUX_ADDR, address);
	soc_dp_reg_write_range(dp, SOC_DPTX_AUX_CMD_TYPE, cmd);

	/* 4. Trigger Transfer */
	soc_dp_reg_write_range(dp, SOC_DPTX_AUX_START, 0);
	soc_dp_reg_write_range(dp, SOC_DPTX_AUX_START, 1);

	/* 5. Wait for Completion */
	ret = ETIMEDOUT;
	while (1) {
		soc_dp_reg_read_range(dp, SOC_DPTX_AUX_REPLY_EVENT_INT_STA, &val);
		if (val) {
			ret = 0;
			break;
		}

		udelay(100);
		timeout_cnt++;
		if (timeout_cnt > 2000) /* Approx 200ms */
			break;
	}

	if (ret) {
		dev_err(dp->dev, "AUX transfer timeout\n");
		return ret;
	}

	/* 6. Read Status */
	soc_dp_reg_read_range(dp, SOC_DPTX_AUX_STATUS, &status);

	/* 7. Clear Interrupt Status (W1C) */
	soc_dp_reg_write_range(dp, SOC_DPTX_AUX_REPLY_EVENT_INT_STA, 1);

	switch (status) {
	case 0: /* ACK */
		break;
	case 1: /* NACK */
		dev_warn(dp->dev, "AUX NACK: addr 0x%x\n", address);
		return -EIO; /* Return Error for NACK */
	case 2: /* DEFER */
		dev_warn(dp->dev, "AUX DEFER: addr 0x%x\n", address);
		return EBUSY; /* Return Error for DEFER */
	default:
		/* Check error code if status is weird */
		soc_dp_reg_read_range(dp, SOC_DPTX_AUX_REPLY_ERR_CODE, &val);
		dev_dbg(dp->dev, "AUX info, cmd: 0x%x, address: 0x%x, size: %d, status: 0x%x, code: 0x%x\n",
				 cmd, address, size, status, val);
		return EIO;
	}

	/* 8. Read Data (if Read operation and ACK) */
	if (is_read && size > 0) {
		soc_dp_reg_read_range(dp, SOC_DPTX_AUX_DATA1, &data[0]);
		soc_dp_reg_read_range(dp, SOC_DPTX_AUX_DATA2, &data[1]);
		soc_dp_reg_read_range(dp, SOC_DPTX_AUX_DATA3, &data[2]);
		soc_dp_reg_read_range(dp, SOC_DPTX_AUX_DATA4, &data[3]);

		/* Unpack 32-bit words back to bytes */
		for (i = 0; i < size; i++) {
			buf[i] = (data[i / 4] >> ((i % 4) * 8)) & 0xFF;
		}
	}

	return size;
}

/* * Wrapper for AUX transfer with retry mechanism
 */
static int soc_dp_aux_transfer_with_retry(struct soc_dp_dev *dp, u32 cmd, u32 address, u8 *data, int size)
{
	int retries = 0;
	int ret;
	const int max_retries = 5;

	while (retries < max_retries) {
		ret = soc_dp_aux_transfer_raw(dp, cmd, address, data, size);

		if (ret >= 0)
			return ret;

		/* If Timeout, reset aux and retry */
		if (ret == ETIMEDOUT) {
			soc_dp_aux_hw_reset(dp);
			retries++;
			continue;
		}

		/* If DEFER (Sink busy), wait and retry */
		if (ret == EBUSY) {
			udelay(400);
			retries++;
			continue;
		}

		/* If NACK, retry briefly just in case */
		if (ret == EIO) {
			udelay(100);
			retries++;
			continue;
		}

		return ret;
	}

	dev_err(dp->dev, "AUX transfer failed after %d retries (cmd 0x%x, addr 0x%x)\n", max_retries, cmd, address);
	return -ETIMEDOUT;
}

/* Native Write (For DPCD) */
static int soc_dp_aux_native_write(struct soc_dp_dev *dp, u32 address, u8 *data, int size)
{
	return soc_dp_aux_transfer_with_retry(dp, DP_AUX_NATIVE_WRITE, address, data, size);
}

/* Native Read (For DPCD) */
static int soc_dp_aux_native_read(struct soc_dp_dev *dp, u32 address, u8 *data, int size)
{
	return soc_dp_aux_transfer_with_retry(dp, DP_AUX_NATIVE_READ, address, data, size);
}

/* DPCD Write: Updated to support burst writes (buffer + size) */
static int soc_dp_dpcd_write(struct soc_dp_dev *dp, u32 address, u8 *buf, int size)
{
	return soc_dp_aux_native_write(dp, address, buf, size);
}

/* DPCD Write: Updated to support burst writes (u8) */
static int soc_dp_dpcd_writeb(struct soc_dp_dev *dp, u32 address, u8 data)
{
	return soc_dp_dpcd_write(dp, address, &data, 1);
}

/* DPCD Read */
static int soc_dp_dpcd_read(struct soc_dp_dev *dp, u32 address, u8 *buf, int size)
{
	return soc_dp_aux_native_read(dp, address, buf, size);
}

/* I2C Write (For EDID/DDC) */
static int soc_dp_aux_i2c_write(struct soc_dp_dev *dp, u32 address, u8 *data, int size)
{
	return soc_dp_aux_transfer_with_retry(dp, DP_AUX_I2C_WRITE, address, data, size);
}

/* I2C Read (For EDID/DDC) */
static int soc_dp_aux_i2c_read(struct soc_dp_dev *dp, u32 address, u8 *data, int size)
{
	return soc_dp_aux_transfer_with_retry(dp, DP_AUX_I2C_READ, address, data, size);
}

static bool soc_dp_dpcd_caps_valid(const u8 *dpcd)
{
	u8 max_bw = dpcd[DP_MAX_LINK_RATE];
	u8 max_lanes = dpcd[DP_MAX_LANE_COUNT] & DP_MAX_LANE_COUNT_MASK;

	if (!dpcd[DP_DPCD_REV])
		return false;

	switch (max_bw) {
	case DP_LINK_BW_1_62:
	case DP_LINK_BW_2_7:
	case DP_LINK_BW_5_4:
	case DP_LINK_BW_8_1:
		break;
	default:
		return false;
	}

	return max_lanes == 1 || max_lanes == 2 || max_lanes == 4;
}

/*
 * Read the Sink's DPCD capability information.
 * Note: EDID is parsed separately. This function focuses solely on
 * Link Layer capabilities (Rate, Lanes, etc.).
 */
int soc_dp_hw_read_sink_caps(struct soc_dp_dev *dp)
{
	int ret;
	u8 max_bw;
	int retry;

	for (retry = 0; retry < 3; retry++) {
		if (retry) {
			soc_dp_aux_hw_reset(dp);
			mdelay(10);
		}

		/* 1. Read DPCD Receiver Capability fields (0x00000 - 0x0000F) */
		ret = soc_dp_dpcd_read(dp, DP_DPCD_REV, dp->dpcd, DP_RECEIVER_CAP_SIZE);
		if (ret < 0)
			continue;

		if (ret != DP_RECEIVER_CAP_SIZE || !soc_dp_dpcd_caps_valid(dp->dpcd)) {
			dev_dbg(dp->dev,
				"DPCD caps not ready: rev=0x%02x bw=0x%02x lanes=0x%02x\n",
				dp->dpcd[DP_DPCD_REV], dp->dpcd[DP_MAX_LINK_RATE], dp->dpcd[DP_MAX_LANE_COUNT]);
			ret = EAGAIN;
			continue;
		}

		break;
	}

	if (ret < 0) {
		dev_err(dp->dev, "Failed to read DPCD\n");
		dp->link.revision = 0x14;
		dp->link.max_rate = SOC_DP_LINK_RATE_5_40;
		dp->link.max_num_lanes = SOC_DP_LANE_2;
		dp->link.enhanced_framing = 1;
		return ret;
	}

	/* 2. Parse DP Revision */
	dp->link.revision = dp->dpcd[DP_DPCD_REV];

	/*
	 * 3. Parse and determine Link Rate.
	 * Get the maximum link rate supported by the Sink.
	 * Note: During link training, we usually start from min(Sink_Max, Source_Max).
	 */
	max_bw = dp->dpcd[DP_MAX_LINK_RATE];
	switch (max_bw) {
	case DP_LINK_BW_1_62:
		dp->link.max_rate = SOC_DP_LINK_RATE_1_62;
		break;
	case DP_LINK_BW_2_7:
		dp->link.max_rate = SOC_DP_LINK_RATE_2_70;
		break;
	case DP_LINK_BW_5_4:
		dp->link.max_rate = SOC_DP_LINK_RATE_5_40;
		break;
	case DP_LINK_BW_8_1:
		dp->link.max_rate = SOC_DP_LINK_RATE_8_10;
		break;
	default:
		dev_warn(dp->dev, "Unknown DPCD Max Rate: 0x%x, defaulting to 5.40G\n", max_bw);
		dp->link.revision = 0x14;
		dp->link.max_rate = SOC_DP_LINK_RATE_5_40;
		dp->link.max_num_lanes = SOC_DP_LANE_2;
		dp->link.enhanced_framing = 1;
		return 0;
	}

	/* 4. Parse and determine Lane Count */
	dp->link.max_num_lanes = dp->dpcd[DP_MAX_LANE_COUNT] & DP_MAX_LANE_COUNT_MASK;

	/* 5. Check for Enhanced Framing support */
	dp->link.enhanced_framing = (dp->dpcd[DP_MAX_LANE_COUNT] & DP_ENHANCED_FRAME_CAP);

	dev_info(dp->dev, "DPCD: Rev %x.%x, MaxRate %d kHz, MaxLanes %d, EnhFrame %d\n",
			 dp->link.revision >> 4, dp->link.revision & 0xF, dp->link.max_rate,
			 dp->link.max_num_lanes, dp->link.enhanced_framing);

	return 0;
}

/*
 * Check Hot Plug Detect (HPD) Status
 */
enum soc_dp_connector_status soc_dp_hw_detect_hpd(struct soc_dp_dev *dp)
{
	u32 plug_event, unplug_event;
	u32 hpd_status;
	enum soc_dp_connector_status connector_status = dp->connector_status;

	soc_dp_reg_read_range(dp, SOC_DPTX_HOT_PLUG_EVENT, &plug_event);
	soc_dp_reg_read_range(dp, SOC_DPTX_HOT_UNPLUG_EVENT, &unplug_event);

	if (plug_event)
	connector_status = connector_status_connected;

	if (unplug_event)
	connector_status = connector_status_disconnected;

	soc_dp_reg_read_range(dp, SOC_DPTX_HPD_IN_STATUS, &hpd_status);

	pr_debug("%s plug_event %d unplug_event %d hpd_status %d\n", __func__, plug_event, unplug_event, hpd_status);

	if (hpd_status)
		connector_status = connector_status_connected;
	else
		connector_status = connector_status_disconnected;

#if USED_HPD_BYPASS
	connector_status = connector_status_connected;
#endif

	return connector_status;
}

/*
 * Clean Hot Plug Detect (HPD) Status
 */
void soc_dp_hw_clean_hpd(struct soc_dp_dev *dp)
{
	u32 plug_event, unplug_event;

	soc_dp_reg_read_range(dp, SOC_DPTX_HOT_PLUG_EVENT, &plug_event);
	soc_dp_reg_read_range(dp, SOC_DPTX_HOT_UNPLUG_EVENT, &unplug_event);

	if (plug_event)
		soc_dp_reg_only_write_range(dp, SOC_DPTX_HOT_PLUG_EVENT, 0x1);

	if (unplug_event)
		soc_dp_reg_only_write_range(dp, SOC_DPTX_HOT_UNPLUG_EVENT, 0x1);
}

static int soc_dp_set_training_pattern(struct soc_dp_dev *dp, u8 pattern)
{
	u32 tps_sel = 0;
	u8 dpcd_pattern = pattern;
	int ret;

	if (pattern != DP_TRAINING_PATTERN_DISABLE)
	dpcd_pattern |= DP_LINK_SCRAMBLING_DISABLE;

	/* Configure PHY Pattern */
	switch (pattern) {
	case DP_TRAINING_PATTERN_DISABLE:
		tps_sel = 0;
		soc_dp_reg_write_range(dp, SOC_DPTX_SCRAMBLER_DISABLE, 0);
		break;
	case DP_TRAINING_PATTERN_1:
		tps_sel = 1;
		soc_dp_reg_write_range(dp, SOC_DPTX_SCRAMBLER_DISABLE, 1);
		break;
	case DP_TRAINING_PATTERN_2:
		tps_sel = 2;
		soc_dp_reg_write_range(dp, SOC_DPTX_SCRAMBLER_DISABLE, 1);
		break;
	case DP_TRAINING_PATTERN_3:
		tps_sel = 3;
		soc_dp_reg_write_range(dp, SOC_DPTX_SCRAMBLER_DISABLE, 1);
		break;
	default:
		dev_err(dp->dev, "Unsupported training pattern: 0x%x\n", pattern);
		return -EINVAL;
	}

	soc_dp_reg_write_range(dp, SOC_DPTX_TPS_SEL, tps_sel);

	/* Configure DPCD Pattern */
	ret = soc_dp_dpcd_writeb(dp, DP_TRAINING_PATTERN_SET, dpcd_pattern);
	if (ret < 0) {
		dev_err(dp->dev, "Failed to set DPCD training pattern: %d\n", ret);
		return ret;
	}

	return 0;
}

/* Link Training Helper: Check Clock Recovery */
static int soc_dp_link_status_cr_ok(u8 link_status[DP_LINK_STATUS_SIZE], int lane_count)
{
	int lane;
	u8 lane_status;

	for (lane = 0; lane < lane_count; lane++) {
		lane_status = link_status[lane >> 1];
		if (lane & 1) lane_status >>= 4;
		if (!(lane_status & DP_LANE_CR_DONE)) return 0;
	}
	return 1;
}

/* Link Training Helper: Check Channel EQ */
static int soc_dp_link_status_eq_ok(u8 link_status[DP_LINK_STATUS_SIZE], int lane_count)
{
	int lane;
	u8 lane_status;
	u8 align = link_status[2]; // DP_LANE_ALIGN_STATUS_UPDATED

	if (!(align & 1)) return 0; // INTERLANE_ALIGN_DONE

	for (lane = 0; lane < lane_count; lane++) {
		lane_status = link_status[lane >> 1];
		if (lane & 1) lane_status >>= 4;
		if ((lane_status & (DP_LANE_CHANNEL_EQ_DONE | DP_LANE_SYMBOL_LOCKED)) !=
		    (DP_LANE_CHANNEL_EQ_DONE | DP_LANE_SYMBOL_LOCKED))
			return 0;
	}
	return 1;
}

static u8 soc_dp_get_adjust_req_v(u8 link_status[DP_LINK_STATUS_SIZE], int lane)
{
	u8 req = link_status[DP_ADJUST_REQUEST_LANE0_1 - DP_LANE0_1_STATUS + (lane >> 1)];

	if (lane & 1) req >>= 4;
	return req & DP_TRAIN_VOLTAGE_SWING_MASK;
}

static u8 soc_dp_get_adjust_req_p(u8 link_status[DP_LINK_STATUS_SIZE], int lane)
{
	u8 req = link_status[DP_ADJUST_REQUEST_LANE0_1 - DP_LANE0_1_STATUS + (lane >> 1)];

	if (lane & 1) req >>= 4;
	return (req & DP_TRAIN_PRE_EMPHASIS_MASK) >> DP_TRAIN_PRE_EMPHASIS_SHIFT;
}

enum soc_dp_link_train_delay {
	SOC_DP_TRAIN_DELAY_CLOCK_RECOVERY,
	SOC_DP_TRAIN_DELAY_CHANNEL_EQ,
};

static void soc_dp_link_train_delay(struct soc_dp_dev *dp,
				    enum soc_dp_link_train_delay delay_type)
{
	u8 interval = dp->dpcd[DP_TRAINING_AUX_RD_INTERVAL] & DP_TRAINING_AUX_RD_MASK;
	u16 delay_us = delay_type == SOC_DP_TRAIN_DELAY_CLOCK_RECOVERY ? 100 : 400;

	if (!interval) {
		udelay(delay_us);
		return;
	}

	if (interval <= 4) {
		udelay(interval * 4000);
		return;
	}

	dev_warn(dp->dev, "Invalid TRAINING_AUX_RD_INTERVAL 0x%x, fallback to %uus\n",
		 interval, delay_us);
	udelay(delay_us);
}

static int soc_dp_link_train_clock_recovery(struct soc_dp_dev *dp, enum soc_dp_link_rate rate, enum soc_dp_lane_count lanes)
{
	u8 link_status[DP_LINK_STATUS_SIZE];
	u8 training_set[4] = {0};
	int retries = 0;
	int i, ret;
	struct soc_dp_phy_configure_opts phy_opts;

	memset(&phy_opts, 0, sizeof(phy_opts));
	phy_opts.lanes = lanes;

	soc_dp_phy_configure(&dp->phy, &phy_opts);

	ret = soc_dp_dpcd_write(dp, DP_TRAINING_LANE0_SET,
				training_set, lanes);
	if (ret < 0)
		return ret;

	ret = soc_dp_set_training_pattern(dp, DP_TRAINING_PATTERN_1);
	if (ret < 0) {
		soc_dp_set_training_pattern(dp, DP_TRAINING_PATTERN_DISABLE);
		return ret;
	}

	while (retries < 8) {
		soc_dp_link_train_delay(dp, SOC_DP_TRAIN_DELAY_CLOCK_RECOVERY);

		ret = soc_dp_dpcd_read(dp, DP_LANE0_1_STATUS, link_status, DP_LINK_STATUS_SIZE);
		if (ret < 0) {
			soc_dp_set_training_pattern(dp, DP_TRAINING_PATTERN_DISABLE);
			return ret;
		}

		if (soc_dp_link_status_cr_ok(link_status, lanes))
			return 0;

		/* Update settings based on Sink request */
		for (i = 0; i < lanes; i++) {
			u8 v = soc_dp_get_adjust_req_v(link_status, i);
			u8 p = soc_dp_get_adjust_req_p(link_status, i);

			if (v >= SOC_DP_SWING_MAX) {
			v = SOC_DP_SWING_MAX;
			v |= DP_TRAIN_MAX_SWING_REACHED;
			}

			if (p >= SOC_DP_PREEMP_MAX) {
			p = SOC_DP_PREEMP_MAX;
			v |= DP_TRAIN_MAX_PRE_EMPHASIS_REACHED;
			}

			training_set[i] = v | (p << DP_TRAIN_PRE_EMPHASIS_SHIFT);

			// Update PHY Config
			phy_opts.voltage[i] = v & DP_TRAIN_VOLTAGE_SWING_MASK;
			phy_opts.pre[i] = p & DP_TRAIN_PRE_EMPHASIS_MASK;
		}

		ret = soc_dp_phy_configure(&dp->phy, &phy_opts);
		if (ret)
			return ret;


		ret = soc_dp_dpcd_write(dp, DP_TRAINING_LANE0_SET,
					training_set, lanes);
		if (ret < 0) {
			soc_dp_set_training_pattern(dp, DP_TRAINING_PATTERN_DISABLE);
			return ret;
		}

		retries++;
	}

	dev_err(dp->dev, "Link Training Clock Recovery Failed\n");
	soc_dp_set_training_pattern(dp, DP_TRAINING_PATTERN_DISABLE);
	return -ETIMEDOUT;
}

static int soc_dp_link_train_channel_eq(struct soc_dp_dev *dp, enum soc_dp_link_rate rate, enum soc_dp_lane_count lanes)
{
	u8 link_status[DP_LINK_STATUS_SIZE];
	u8 training_set[4] = {0};
	u8 training_pattern = DP_TRAINING_PATTERN_2;
	int retries = 0;
	int i, ret;
	struct soc_dp_phy_configure_opts phy_opts;

	memset(&phy_opts, 0, sizeof(phy_opts));
	phy_opts.lanes = lanes;

	if (dp->dpcd[DP_MAX_LANE_COUNT] & DP_TPS3_SUPPORTED) {
		training_pattern = DP_TRAINING_PATTERN_3;
		dev_info(dp->dev, "Link Training: Using TPS3\n");
	} else {
		dev_info(dp->dev, "Link Training: Using TPS2\n");
	}

	ret = soc_dp_set_training_pattern(dp, training_pattern);
	if (ret < 0) {
		soc_dp_set_training_pattern(dp, DP_TRAINING_PATTERN_DISABLE);
		return ret;
	}

	while (retries < 8) {
		soc_dp_link_train_delay(dp, SOC_DP_TRAIN_DELAY_CHANNEL_EQ);

		ret = soc_dp_dpcd_read(dp, DP_LANE0_1_STATUS, link_status, DP_LINK_STATUS_SIZE);
		if (ret < 0) {
			soc_dp_set_training_pattern(dp, DP_TRAINING_PATTERN_DISABLE);
			return ret;
		}

		if (soc_dp_link_status_eq_ok(link_status, lanes)) {
			soc_dp_set_training_pattern(dp, DP_TRAINING_PATTERN_DISABLE);
			return 0;
		}

		/* Update settings based on Sink request */
		for (i = 0; i < lanes; i++) {
			u8 v = soc_dp_get_adjust_req_v(link_status, i);
			u8 p = soc_dp_get_adjust_req_p(link_status, i);

			if (v >= SOC_DP_SWING_MAX) {
			v = SOC_DP_SWING_MAX;
			v |= DP_TRAIN_MAX_SWING_REACHED;
			}

			if (p >= SOC_DP_PREEMP_MAX) {
			p = SOC_DP_PREEMP_MAX;
			v |= DP_TRAIN_MAX_PRE_EMPHASIS_REACHED;
			}

			training_set[i] = v | (p << DP_TRAIN_PRE_EMPHASIS_SHIFT);

			// Update PHY Config
			phy_opts.voltage[i] = v & DP_TRAIN_VOLTAGE_SWING_MASK;
			phy_opts.pre[i] = p & DP_TRAIN_PRE_EMPHASIS_MASK;
		}

		ret = soc_dp_phy_configure(&dp->phy, &phy_opts);
		if (ret)
			return ret;

		ret = soc_dp_dpcd_write(dp, DP_TRAINING_LANE0_SET,
					training_set, lanes);
		if (ret < 0) {
			soc_dp_set_training_pattern(dp, DP_TRAINING_PATTERN_DISABLE);
			return ret;
		}

		retries++;
	}

	dev_err(dp->dev, "Link Training Channel EQ Failed\n");
	soc_dp_set_training_pattern(dp, DP_TRAINING_PATTERN_DISABLE);
	return -ETIMEDOUT;
}

/*
 * Main Link Training Function
 */
static int soc_dp_link_train(struct soc_dp_dev *dp, enum soc_dp_link_rate rate, enum soc_dp_lane_count lanes)
{
	int ret;
	u8 link_config[2];
	u8 bw_code;

	/* Map Link Rate Enum to DPCD Bandwidth Code */
	switch (rate) {
	case SOC_DP_LINK_RATE_1_62:
		bw_code = DP_LINK_BW_1_62;
		break;
	case SOC_DP_LINK_RATE_2_70:
		bw_code = DP_LINK_BW_2_7;
		break;
	case SOC_DP_LINK_RATE_5_40:
		bw_code = DP_LINK_BW_5_4;
		break;
	case SOC_DP_LINK_RATE_8_10:
		bw_code = DP_LINK_BW_8_1;
		break;
	default:
		bw_code = DP_LINK_BW_1_62;
		break;
	}

	/* Configure DPCD Link Rate and Lane Count */
	link_config[0] = bw_code;
	link_config[1] = lanes;
	if (dp->link.enhanced_framing)
		link_config[1] |= DP_LANE_COUNT_ENHANCED_FRAME_EN;

	ret = soc_dp_dpcd_write(dp, DP_LINK_BW_SET, link_config, 2);
	if (ret < 0) {
		dev_err(dp->dev, "Failed to configure DPCD\n");
		return ret;
	}

	ret = soc_dp_link_train_clock_recovery(dp, rate, lanes);
	if (ret)
		return ret;

	ret = soc_dp_link_train_channel_eq(dp, rate, lanes);
	if (ret)
		return ret;

	mdelay(20);
	return 0;
}

static void soc_dp_hw_set_msa(struct soc_dp_dev *dp, const struct soc_dp_video_mode *mode,
			      enum soc_dp_link_rate rate, enum soc_dp_lane_count lanes)
{
	u64 hb_num;
	u32 link_rate_khz;
	u32 pixel_clk_khz;
	u32 bpp, misc0;
	u32 tu, tu_frac, tu_int, rd_thres;
	u32 hsync_len;

	// 1. Prepare basic parameters
	pixel_clk_khz = mode->clock;
	if (pixel_clk_khz == 0) pixel_clk_khz = 1; // Prevent division by zero

	// Get BPP
	bpp = soc_dp_get_bpp(dp->color_format);

	// Calculate MISC0
	// bit0: 0 (Sync Clock)
	// bits1-7: Color Format (000=RGB, 001=YCbCr422, 010=YCbCr444)
	// bits5-7: BPC (001=8bpc, 010=10bpc, etc)
	switch (dp->color_format) {
	case SOC_VIDEO_RGB_6BIT:      misc0 = 0x00; break;
	case SOC_VIDEO_RGB_8BIT:      misc0 = 0x20; break;
	case SOC_VIDEO_RGB_10BIT:     misc0 = 0x40; break;
	case SOC_VIDEO_RGB_12BIT:     misc0 = 0x60; break;
	case SOC_VIDEO_RGB_16BIT:     misc0 = 0x80; break;
	case SOC_VIDEO_YUV422_8BIT:   misc0 = 0x22; break;
	case SOC_VIDEO_YUV422_10BIT:  misc0 = 0x42; break;
	case SOC_VIDEO_YUV422_12BIT:  misc0 = 0x62; break;
	case SOC_VIDEO_YUV422_16BIT:  misc0 = 0x82; break;
	case SOC_VIDEO_YUV444_8BIT:   misc0 = 0x24; break;
	case SOC_VIDEO_YUV444_10BIT:  misc0 = 0x44; break;
	case SOC_VIDEO_YUV444_12BIT:  misc0 = 0x64; break;
	case SOC_VIDEO_YUV444_16BIT:  misc0 = 0x84; break;
	default:                      misc0 = 0x20; break;
	}

	// 2. Calculate HBlank Interval (hb_num)
	// hb_num = hblank * (LinkRate_kHz / 10000) / 4 / (PixelClock_kHz / 1000)
	// Optimized Formula: hb_num = hblank * LinkRate_kHz / (40 * PixelClock_kHz)
	link_rate_khz = rate; // e.g., 1620000

	hb_num = (u64)(mode->htotal - mode->hdisplay) * link_rate_khz;
	{
		u32 den = 40 * pixel_clk_khz;

		hb_num += (den / 2);
		soc_dp_div64(&hb_num, den);
	}

	// 3. Calculate TU (Transfer Unit)
	// tu = (PixelClock_kHz/1000) * bpp * 640 / (8 * lanes * (LinkRate_kHz/10000))
	// Optimized Formula: tu = PixelClock_kHz * bpp * 800 / (lanes * LinkRate_kHz)
	{
	u64 temp_tu = (u64)pixel_clk_khz * bpp * 800;
		u32 den = lanes * link_rate_khz;

		// Note: TU is typically floored or carefully rounded.
		// Adding rounding here to be safe.
		temp_tu += (den / 2);

		soc_dp_div64(&temp_tu, den);
		tu = temp_tu;
	}
	tu_frac = tu % 10;
	tu_int  = tu / 10;

	// 4. Calculate FIFO read threshold
	if (tu_int < 6) {
		rd_thres = 32;
	} else if ((mode->htotal - mode->hdisplay) < 80) {
		rd_thres = 12;
	} else {
		rd_thres = 16;
	}

	dev_info(dp->dev, "MSA: %dx%d, Rate:%d kHz, Lanes:%d, BPP:%d, TU:%d.%d\n",
		 mode->hdisplay, mode->vdisplay, rate, lanes, bpp, tu_int, tu_frac);

	// 5. Video mapping format
	soc_dp_reg_write_range(dp, SOC_DPTX_VIDEO_MAPPING, dp->color_format);

	// Polarity configuration
	if (mode->flags & SOC_DP_MODE_FLAG_PHSYNC)
		soc_dp_reg_write_range(dp, SOC_DPTX_HSYNC_IN_POLARITY, 1);
	else
		soc_dp_reg_write_range(dp, SOC_DPTX_HSYNC_IN_POLARITY, 0);

	if (mode->flags & SOC_DP_MODE_FLAG_PVSYNC)
		soc_dp_reg_write_range(dp, SOC_DPTX_VSYNC_IN_POLARITY, 1);
	else
		soc_dp_reg_write_range(dp, SOC_DPTX_VSYNC_IN_POLARITY, 0);

	// Basic timing
	soc_dp_reg_write_range(dp, SOC_DPTX_HACTIVE, mode->hdisplay);
	soc_dp_reg_write_range(dp, SOC_DPTX_VACTIVE, mode->vdisplay);
	soc_dp_reg_write_range(dp, SOC_DPTX_HBLANK, mode->htotal - mode->hdisplay);
	soc_dp_reg_write_range(dp, SOC_DPTX_VBLANK, mode->vtotal - mode->vdisplay);

	soc_dp_reg_write_range(dp, SOC_DPTX_HSTART, mode->htotal - mode->hsync_start);
	soc_dp_reg_write_range(dp, SOC_DPTX_VSTART, mode->vtotal - mode->vsync_start);

	hsync_len = mode->hsync_end - mode->hsync_start;

	soc_dp_reg_write_range(dp, SOC_DPTX_H_SYNC_WIDTH, hsync_len);
	soc_dp_reg_write_range(dp, SOC_DPTX_V_SYNC_WIDTH, mode->vsync_end - mode->vsync_start);
	soc_dp_reg_write_range(dp, SOC_DPTX_H_FRONT_PORCH, mode->hsync_start - mode->hdisplay);
	soc_dp_reg_write_range(dp, SOC_DPTX_V_FRONT_PORCH, mode->vsync_start - mode->vdisplay);

	// MSA and MISC
	soc_dp_reg_write_range(dp, SOC_DPTX_MISC0, misc0);
	soc_dp_reg_write_range(dp, SOC_DPTX_MISC1, 0);

	// Link layer parameters
	soc_dp_reg_write_range(dp, SOC_DPTX_HBLANK_INTERVAL, (u32)hb_num);
	soc_dp_reg_write_range(dp, SOC_DPTX_AVERAGE_BYTES_PER_TU, tu_int);
	soc_dp_reg_write_range(dp, SOC_DPTX_AVERAGE_BYTES_PER_TU_FRAC, tu_frac);
	soc_dp_reg_write_range(dp, SOC_DPTX_INIT_THRESHOLD, rd_thres);

	soc_dp_reg_write_range(dp, SOC_DPTX_REG_VID_CLK_SEL, 0);
	soc_dp_reg_write_range(dp, SOC_DPTX_VID_BIST_EN, 0);
}

void soc_dp_hw_enable(struct soc_dp_dev *dp)
{
	dev_dbg(dp->dev, "Enabling Video Stream\n");
	soc_dp_reg_write_range(dp, SOC_DPTX_VIDEO_STREAM_ENABLE, 1);
}

void soc_dp_hw_disable(struct soc_dp_dev *dp)
{
	dev_dbg(dp->dev, "Disabling Video Stream\n");
	soc_dp_reg_write_range(dp, SOC_DPTX_VIDEO_STREAM_ENABLE, 0);
}

/* Calculate required bandwidth in kbps (Pixel Clock * Bits Per Pixel) */
static u32 soc_dp_calc_required_bw(const struct soc_dp_video_mode *mode, int bpp)
{
	return mode->clock * bpp;
}

/* Calculate available link capacity in kbps (taking 8b/10b overhead into account) */
static u32 soc_dp_calc_link_capacity(enum soc_dp_link_rate rate, enum soc_dp_lane_count lanes)
{
	/* Capacity = Rate(kHz) * Lanes * 0.8 */
	return (rate * lanes * 8) / 10;
}


int soc_dp_conn_get_edid_block(struct soc_dp_dev *dp, u8 *buf, unsigned int block, size_t len)
{
	int ret, i;
	int retries;
	u8 offset;

	/*  Wake up Sink */
	for (retries = 0; retries < 5; retries++) {
		ret = soc_dp_dpcd_writeb(dp, DP_SET_POWER, DP_SET_POWER_D0);
		if (ret >= 0) {
			mdelay(2);
			break;
		}
		mdelay(2);
	}

	offset = (block * EDID_LENGTH) & 0xFF;

	ret = soc_dp_aux_i2c_write(dp, 0x50, &offset, 1);

	if (ret < 0) {
		dev_err(dp->dev, "[EDID] AUX write offset failed: %d\n", ret);
		return -EIO;
	}

	for (i = 0; i < len; i += 16) {
		ret = soc_dp_aux_i2c_read(dp, 0x50, buf + i, 16);

		if (ret < 0) {
			dev_err(dp->dev, "[EDID] AUX read data failed at offset %d: %d\n", i, ret);
			return -EIO;
		}
	}

	return 0;
}

static int update_edp_config(struct soc_dp_dev *dp, bool enable)
{
	u8 value;
	int ret;

	ret = soc_dp_dpcd_read(dp, DP_EDP_CONFIGURATION_SET, &value, 1);
	if (ret < 0) {
		dev_err(dp->dev, "Failed to read DP_EDP_CONFIGURATION_SET, ret: %d\n", ret);
		return ret;
	}

	if (enable)
		value |= 0x01;
	else
		value &= ~0x01;

	ret = soc_dp_dpcd_write(dp, DP_EDP_CONFIGURATION_SET, &value, 1);
	if (ret < 0) {
		dev_err(dp->dev, "Failed to write DP_EDP_CONFIGURATION_SET, ret: %d\n", ret);
		return ret;
	}

	return 0;
}

/*
 * soc_dp_mode_set - Configure link and setup video timing
 * Logic:
 * 1. Read Sink Capabilities.
 * 2. Iterate through link configurations (Rate/Lane combinations).
 * 3. Strategy: Ascending Bandwidth Order (Upgrade Logic).
 * - Start with the lowest config that satisfies bandwidth.
 * - Priority: Maximize Lanes first, then increase Rate (Stability over raw speed).
 * 4. Perform Link Training. If failed, upgrade to next config.
 */
int soc_dp_mode_set(struct soc_dp_dev *dp, const struct soc_dp_video_mode *mode)
{
	int i;
	int ret = -ETIMEDOUT;
	u32 req_bw;
	int bpp;
	const struct soc_dp_link_config *cfg;
	struct soc_dp_phy_configure_opts phy_opts;

	dev_info(dp->dev, "DP: Mode Set %dx%d (PCLK: %d kHz)\n",
		 mode->hdisplay, mode->vdisplay, mode->clock);
	memset(&phy_opts, 0, sizeof(phy_opts));

	bpp = soc_dp_get_bpp(dp->color_format);
	req_bw = soc_dp_calc_required_bw(mode, bpp);

	for (i = 0; i < ARRAY_SIZE(soc_dp_link_priority_table); i++) {
		u32 capacity;

		cfg = &soc_dp_link_priority_table[i];

		/* Filter 1: Check HW Capabilities (Source & Sink limits) */
		if (cfg->rate > dp->link.max_rate || cfg->lanes > dp->link.max_num_lanes)
			continue;

		/* Filter 2: Check Bandwidth Requirement */
		capacity = soc_dp_calc_link_capacity(cfg->rate, cfg->lanes);
		if (capacity < req_bw)
			continue;

		dev_dbg(dp->dev, "DP: Attempting Config: R=%d, L=%d (Cap: %d > Req: %d)\n",
			 cfg->rate, cfg->lanes, capacity, req_bw);

		/* Configure PHY Link parameters */
		phy_opts.lanes = cfg->lanes;
		phy_opts.link_rate = cfg->rate / 1000;
		phy_opts.set_lanes = 1;
		phy_opts.set_rate = 1;

		soc_dp_phy_power_off(&dp->phy);

		if (soc_dp_phy_configure(&dp->phy, &phy_opts))
			continue;

		if (soc_dp_phy_set_pixel_clk(&dp->phy, mode->clock))
			continue;

		if (soc_dp_phy_power_on(&dp->phy))
			continue;

		if (dp->edp_mode) {
			pr_info("%s eDP mode\n", __func__);
			soc_dp_reg_write_range(dp, SOC_DPTX_ENABLE_EDP, 0x1);
			soc_dp_reg_write_range(dp, SOC_DPTX_STREAM_ENC_EN, 0x1);
			update_edp_config(dp, true);
		}

		/* Execute Link Training */
		ret = soc_dp_link_train(dp, cfg->rate, cfg->lanes);
		if (ret == 0) {
			dev_info(dp->dev, "DP: Training successful for R:%d L:%d\n",
				 cfg->rate, cfg->lanes);
			break;
		}

		dev_warn(dp->dev, "DP: Training failed for R:%d L:%d. Upgrading...\n",
			 cfg->rate, cfg->lanes);
	}

	if (ret) {
		dev_warn(dp->dev, "DP: No valid link configuration found for %dx%d\n",
			mode->hdisplay, mode->vdisplay);
	}

	soc_dp_hw_set_msa(dp, mode, dp->phy.link_rate_khz, dp->phy.lane_count);

	return 0;
}

int soc_dp_init(struct soc_dp_dev *dp, uintptr_t base_addr,
		enum soc_dp_ref_clk ref_clk_khz, enum soc_video_format color_format)
{
	int ret;

	memset(dp, 0, sizeof(*dp));
	dp->regs = base_addr;
	dp->color_format = color_format;
	dp->dev = NULL;

	// Reset Controller
	soc_dp_reg_write_range(dp, SOC_DPTX_CONTROLLER_RESET, 0x1);
	soc_dp_reg_write_range(dp, SOC_DPTX_HDCP_RESET, 0x1);
	soc_dp_reg_write_range(dp, SOC_DPTX_AUX_RESET, 0x1);
	soc_dp_reg_write_range(dp, SOC_DPTX_VIDEO_RESET, 0x1);
	mdelay(5);

	// Clear Video Reset
	soc_dp_reg_write_range(dp, SOC_DPTX_CONTROLLER_RESET, 0x0);
	soc_dp_reg_write_range(dp, SOC_DPTX_HDCP_RESET, 0x0);
	soc_dp_reg_write_range(dp, SOC_DPTX_AUX_RESET, 0x0);
	soc_dp_reg_write_range(dp, SOC_DPTX_VIDEO_RESET, 0x0);
	mdelay(2);

	soc_dp_reg_write_range(dp, SOC_DPTX_AUX_REPLY_EVENT_INT_STA, 1);

	soc_dp_reg_write_range(dp, SOC_DPTX_DEFAULT_FAST_LINK_TRAIN_EN, 0x0);
	soc_dp_reg_write_range(dp, SOC_DPTX_SCRAMBLER_DISABLE, 0x0);
	soc_dp_reg_write_range(dp, SOC_DPTX_SCALE_DOWN_MODE, 0x0);

	// Unmask Interrupts
	soc_dp_reg_write_range(dp, SOC_DPTX_AUX_REPLY_EVENT_INT_STA_MSK, 0x0);
	soc_dp_reg_write_range(dp, SOC_DPTX_HDCP_INT_STA_MSK, 0x0);
	soc_dp_reg_write_range(dp, SOC_DPTX_ILLEGAL_AUX_CMD_INT_STA_MSK, 0x0);
	soc_dp_reg_write_range(dp, SOC_DPTX_TYPE_C_EVENT_MSK, 0x0);
	soc_dp_reg_write_range(dp, SOC_DPTX_DSC_EVENT_MSK, 0x0);
	soc_dp_reg_write_range(dp, SOC_DPTX_SDP_INT_STA_S3_MSK, 0x0);
	soc_dp_reg_write_range(dp, SOC_DPTX_SDP_INT_STA_S2_MSK, 0x0);
	soc_dp_reg_write_range(dp, SOC_DPTX_SDP_INT_STA_S1_MSK, 0x0);
	soc_dp_reg_write_range(dp, SOC_DPTX_SDP_INT_STA_S0_MSK, 0x0);
	soc_dp_reg_write_range(dp, SOC_DPTX_VIDEO_FIFO_OVERFLOW_INT_STA_S3_MSK, 0x0);
	soc_dp_reg_write_range(dp, SOC_DPTX_VIDEO_FIFO_OVERFLOW_INT_STA_S2_MSK, 0x0);
	soc_dp_reg_write_range(dp, SOC_DPTX_VIDEO_FIFO_OVERFLOW_INT_STA_S1_MSK, 0x0);
	soc_dp_reg_write_range(dp, SOC_DPTX_VIDEO_FIFO_OVERFLOW_INT_STA_S0_MSK, 0x0);
	soc_dp_reg_write_range(dp, SOC_DPTX_SINK_IRQ_EVENT_MSK, 0x0);
	soc_dp_reg_write_range(dp, SOC_DPTX_HPD_INT_STA_MSK, 0x0);
	soc_dp_reg_write_range(dp, SOC_DPTX_HOT_PLUG_EVENT_MSK, 0x0);
	soc_dp_reg_write_range(dp, SOC_DPTX_HOT_UNPLUG_EVENT_MSK, 0x0);
	soc_dp_reg_write_range(dp, SOC_DPTX_SINK_UNPLUG_ERROR_EVENT_MSK, 0x0);
	mdelay(2);

	soc_dp_reg_write_range(dp, SOC_DPTX_VIDEO_STREAM_ENABLE, 0);

	// Initial PHY Config
	ret = soc_dp_phy_init(&dp->phy, base_addr, ref_clk_khz);
	if (ret) {
		dev_err(dp->dev, "Failed to init PHY\n");
		return ret;
	}

	soc_dp_phy_power_off(&dp->phy);

	// Update connector status using hardware detection interface
#if USED_HPD_BYPASS
	soc_dp_reg_write_range(dp, SOC_DPTX_FORCE_HPD, 0x1);
	mdelay(5);
	dp->connector_status = connector_status_connected;
#endif

	return 0;
}
