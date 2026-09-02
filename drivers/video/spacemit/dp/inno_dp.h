/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 Spacemit Co., Ltd.
 *
 */

#ifndef _INNO_DP_H_
#define _INNO_DP_H_

#include "inno_dp_phy.h"

#define EDID_EXT_LENGTH			256
#define EDID_LENGTH			128
#define EDID_EXTENSION_FLAG		0x7e
#define DP_RECEIVER_CAP_SIZE		0xF

/* Video Mode Flags */
#define SOC_DP_MODE_FLAG_PHSYNC		BIT(0)	/* Horizontal Sync Polarity: Positive */
#define SOC_DP_MODE_FLAG_PVSYNC		BIT(3)	/* Vertical Sync Polarity: Positive */

/*
 * enum soc_video_format - Supported video color formats
 * Used in soc_dp_init() to set the source output format
 */
enum soc_video_format {
	SOC_VIDEO_RGB_6BIT = 0,
	SOC_VIDEO_RGB_8BIT = 1,
	SOC_VIDEO_RGB_10BIT = 2,
	SOC_VIDEO_RGB_12BIT = 3,
	SOC_VIDEO_RGB_16BIT = 4,
	SOC_VIDEO_YUV444_8BIT = 5,
	SOC_VIDEO_YUV444_10BIT = 6,
	SOC_VIDEO_YUV444_12BIT = 7,
	SOC_VIDEO_YUV444_16BIT = 8,
	SOC_VIDEO_YUV422_8BIT = 9,
	SOC_VIDEO_YUV422_10BIT = 10,
	SOC_VIDEO_YUV422_12BIT = 11,
	SOC_VIDEO_YUV422_16BIT = 12,
};

enum soc_dp_ref_clk {
	SOC_DP_REF_CLK_24M = 24000,
	SOC_DP_REF_CLK_50M = 50000,
};

/*
 * enum soc_dp_connector_status - Hot Plug Detect status
 */
enum soc_dp_connector_status {
	connector_status_disconnected = 0,
	connector_status_connected = 1,
	connector_status_unknown = 2,
};

/*
 * struct soc_dp_video_mode - Display timing configuration
 * Equivalent to drm_display_mode for this driver
 */
struct soc_dp_video_mode {
	u32 clock;	/* Pixel clock in kHz */
	u16 hdisplay;	/* Active width */
	u16 htotal;	/* Total width */
	u16 hsync_start;
	u16 hsync_end;
	u16 vdisplay;	/* Active height */
	u16 vtotal;	/* Total height */
	u16 vsync_start;
	u16 vsync_end;

	u32 flags;	/* Polarity flags (SOC_DP_MODE_FLAG_*) */
};

/*
 * struct soc_dp_dev - Main driver context structure
 * Should be allocated by the caller and passed to all API functions
 */
struct soc_dp_dev {
	void *dev;		/* Pointer to device (for logging/callbacks) */
	uintptr_t regs;		/* Base address of memory-mapped registers */

	struct soc_dp_phy phy;	/* PHY instance */

	/* Buffer to store raw DPCD data read from Sink */
	u8 dpcd[DP_RECEIVER_CAP_SIZE];

	u8 edid_data[EDID_EXT_LENGTH];
	struct soc_dp_video_mode video_mode;
	bool edp_mode;

	/* Negotiated link parameters */
	struct {
		u8 revision;
		u8 enhanced_framing;
		u32 max_rate;		/* Negotiated Link Rate (kHz) */
		u32 max_num_lanes;	/* Negotiated Lane Count */
	} link;

	u32 color_format;  /* Configured enum soc_video_format */
	enum soc_dp_connector_status connector_status;
};

/*
 * soc_dp_init - Initialize the DP controller hardware
 * @dp: Driver context
 * @base_addr: Register base address
 * @ref_clk_khz: Reference clock frequency in kHz (e.g., 24000 or 50000)
 * @color_format: Pixel format input (enum soc_video_format)
 * * Performs reset, basic PHY init, and sets up interrupts masks
 */
int soc_dp_init(struct soc_dp_dev *dp, uintptr_t base_addr,
		enum soc_dp_ref_clk ref_clk_khz, enum soc_video_format color_format);

/*
 * soc_dp_hw_detect_hpd - Check Hot Plug Detect status
 * @dp: Driver context
 * * Returns the current physical connection status
 */
enum soc_dp_connector_status soc_dp_hw_detect_hpd(struct soc_dp_dev *dp);

/*
 * soc_dp_hw_clean_hpd - Clear HPD interrupt events
 * @dp: Driver context
 * * Should be called after handling a plug/unplug event
 */
void soc_dp_hw_clean_hpd(struct soc_dp_dev *dp);

/*
 * soc_dp_hw_read_sink_caps - Read DPCD from the connected sink
 * @dp: Driver context
 * * Reads DP capability registers (Rev, Max Rate, Max Lanes) from the sink
 * and stores them in dp->link and dp->dpcd
 */
int soc_dp_hw_read_sink_caps(struct soc_dp_dev *dp);

/*
 * soc_dp_conn_get_edid_block - Read EDID data via AUX channel
 * @dp: Driver context
 * @buf: Buffer to store the read data
 * @block: EDID block number (0 for first 128 bytes)
 * @len: Length to read
 */
int soc_dp_conn_get_edid_block(struct soc_dp_dev *dp, u8 *buf,
			       unsigned int block, size_t len);

/*
 * soc_dp_mode_set - Configure link and setup video timing
 * @dp: Driver context
 * @mode: Video timing parameters
 * * Performs Link Training (Clock Recovery + Channel EQ) and configures MSA
 */
int soc_dp_mode_set(struct soc_dp_dev *dp, const struct soc_dp_video_mode *mode);

/*
 * soc_dp_hw_enable - Enable the video stream
 * @dp: Driver context
 */
void soc_dp_hw_enable(struct soc_dp_dev *dp);

/*
 * soc_dp_hw_disable - Disable the Video Stream
 * @dp: Driver context
 */
void soc_dp_hw_disable(struct soc_dp_dev *dp);

#endif /* _INNO_DP_H_ */
