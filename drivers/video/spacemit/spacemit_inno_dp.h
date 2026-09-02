/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 Spacemit Co., Ltd.
 *
 */

#ifndef _SPACEMIT_DP_H_
#define _SPACEMIT_DP_H_

#include <clk.h>
#include <reset.h>
#include "./dp/inno_dp.h"

enum spacemit_inno_dp_types {
	INNO_DP = 0,
	INNO_EDP,
};

struct spacemit_inno_dp_priv {
	void __iomem *base;
	struct soc_dp_dev dp_dev;
	enum spacemit_inno_dp_types dp_type;

	u32 dp_id;
	u32 edp_id;

	bool power_valid;
	bool enable_valid;
	bool bl_valid;

	struct gpio_desc power;
	struct gpio_desc enable;
	struct gpio_desc bl;
	struct udevice *backlight;

	struct clk pxclk;
	struct clk mclk;
	struct clk hclk;
	struct clk escclk;
	struct clk dscclk;
	struct clk aclk;
	struct clk dppxclk;

	struct reset_ctl aclk_reset;
	struct reset_ctl mclk_reset;
	struct reset_ctl esc_reset;
	struct reset_ctl dscclk_reset;
	struct reset_ctl lcd_reset;
	struct reset_ctl dp_reset;
};

#endif /* _SPACEMIT_INNO_DP_H_ */
