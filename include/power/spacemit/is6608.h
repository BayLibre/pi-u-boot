/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2025, Spacemit
 */

#ifndef __IS6608_H__
#define __IS6608_H__

enum IS6608_reg {
	IS6608_ID_DCDC1,
};

#define SPACEMIT_IS6608_MAX_REG	0x2

#define IS6608_BUCK_VSEL_REG		0x21
#define IS6608_BUCK_VSEL_MASK		0xfff

/* No practical significance */
#define IS6608_BUCK_EN_MASK		0x0
/* No practical significance */
#define IS6608_BUCK_CTRL_REG		0x0

#define IS6608_BUCK_LINER_RANGE					\
static const struct pm8xx_linear_range is6608_buck_ranges[] = {	\
        REGULATOR_LINEAR_RANGE(400000, 0xc8, 0x9c4, 2000),		\
};

#define IS6608_REGULATOR_DESC		\
static const struct pm8xx_buck_desc is6608_buck_desc[] = {			\
	/* BUCK */		\
	PM8XX_DESC_COMMON(IS6608_ID_DCDC1, "EDCDC_REG1",			\
			91, IS6608_BUCK_VSEL_REG, IS6608_BUCK_VSEL_MASK,	\
			IS6608_BUCK_CTRL_REG, IS6608_BUCK_EN_MASK,		\
			0, 0,							\
			is6608_buck_ranges),	\
};

#define IS6608_REGULATOR_MATCH_DATA						\
struct regulator_match_data is6608_regulator_match_data = {			\
	.nr_buck_desc = ARRAY_SIZE(is6608_buck_desc),				\
	.buck_desc = is6608_buck_desc,						\
	.nr_ldo_desc = 0,							\
	.ldo_desc = NULL,							\
	.nr_switch_desc = 0,							\
	.switch_desc = NULL,							\
	.name = "is6608",							\
	.max_registers = 0x22,/* SPACEMIT_SY8810L_MAX_REG */			\
};

#define DECLEAR_IS6608_REGULATOR_MATCH_DATA	extern struct regulator_match_data is6608_regulator_match_data;

#endif
