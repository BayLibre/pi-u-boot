/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 Spacemit Co., Ltd.
 *
 */

#ifndef _SPACEMIT_DPU_H_
#define _SPACEMIT_DPU_H_
#include <clk.h>
#include <reset.h>
#include <asm/io.h>

#define OUTFMT_RGB121212	0
#define OUTFMT_RGB101010	1
#define OUTFMT_RGB888		2
#define OUTFMT_RGB666		12
#define OUTFMT_RGB565		13

enum dpu_modes {
	DPU_MODE_EDP = 0,
	DPU_MODE_MIPI,
	DPU_MODE_HDMI,
	DPU_MODE_LVDS,
	DPU_MODE_DP,
};

enum dpu_features {
	DPU_FEATURE_OUTPUT_10BIT = (1 << 0),
};

struct spacemit_mode_modeinfo {
	unsigned int xres;
	unsigned int yres;
	unsigned int left_margin;
	unsigned int right_margin;
	unsigned int upper_margin;
	unsigned int lower_margin;
	unsigned int hsync_len;
	unsigned int vsync_len;
	unsigned int hsync_invert;
	unsigned int vsync_invert;
	unsigned int pixclock_freq;
	int pix_fmt_out;
};

struct spacemit_dpu_priv {
	void __iomem *regs_crtc0;
	void __iomem *regs_crtc1;
	struct udevice *conn_dev;
	struct display_timing timing;
};

struct spacemit_dpu_driverdata {
	u32 features;
};

static inline void dpu_writel(void __iomem *addr, uint32_t offset, uint32_t data)
{
	writel(data, (addr + offset));
}

#endif
