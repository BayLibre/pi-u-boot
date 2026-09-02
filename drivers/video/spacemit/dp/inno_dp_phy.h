/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2025 Spacemit Co., Ltd.
 *
 */

#ifndef _INNO_DP_PHY_H_
#define _INNO_DP_PHY_H_

#include <linux/types.h>
#include <linux/stddef.h>

#include <asm/global_data.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <div64.h>

#define USED_ACTIVATE_DO_DIV		1
#define USED_HPD_BYPASS			0

#define DP0_REGISTER_BASE_ADDRESS	0xcac84000
#define DP1_REGISTER_BASE_ADDRESS	0xcac88000
#define DP0_REGISTER_SIZE		0x4000
#define DP1_REGISTER_SIZE		0x4000

enum soc_dp_link_rate {
	SOC_DP_LINK_RATE_1_62 = 1620000,	/* 1.62 Gbps */
	SOC_DP_LINK_RATE_2_70 = 2700000,	/* 2.70 Gbps */
	SOC_DP_LINK_RATE_5_40 = 5400000,	/* 5.40 Gbps */
	SOC_DP_LINK_RATE_8_10 = 8100000,	/* 8.10 Gbps */
};

enum soc_dp_lane_count {
	SOC_DP_LANE_1 = 1,
	SOC_DP_LANE_2 = 2,
	SOC_DP_LANE_4 = 4,
};

/* PHY Configuration Options */
struct soc_dp_phy_configure_opts {
	u32 link_rate;
	u8 lanes;

	u8 voltage[4];
	u8 pre[4];

	bool set_rate;
	bool set_lanes;
	bool set_voltages;
};

struct soc_dp_phy {
	void *dev;
	uintptr_t regs;
	u32 ref_clk_khz;

	u32 link_rate_khz;
	int lane_count;

	int power_count;
};

int soc_dp_phy_init(struct soc_dp_phy *phy, uintptr_t base_addr, u32 ref_clk_khz);
int soc_dp_phy_exit(struct soc_dp_phy *phy);
int soc_dp_phy_power_on(struct soc_dp_phy *phy);
int soc_dp_phy_power_off(struct soc_dp_phy *phy);
int soc_dp_phy_configure(struct soc_dp_phy *phy, struct soc_dp_phy_configure_opts *opts);
int soc_dp_phy_set_pixel_clk(struct soc_dp_phy *phy, u32 pixel_clk_khz);

#endif /* _INNO_DP_PHY_H_ */
