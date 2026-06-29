/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2026, Spacemit
 */

#ifndef __IS6615A_H__
#define __IS6615A_H__

enum IS6615A_reg {
	IS6615A_ID_DCDC1,
	IS6615A_ID_DCDC2,
};

#define SPACEMIT_IS6615A_MAX_REG	0x2

#define IS6615A_BUCK_VSEL_REG		0x21
#define IS6615A_BUCK_VSEL_MASK		0xffff

/* No practical significance */
#define IS6615A_BUCK_EN_MASK		0x0
/* No practical significance */
#define IS6615A_BUCK_CTRL_REG		0x0

#define IS6615A_BUCK_LINER_RANGE					\
static const struct pm8xx_linear_range is6615a_buck_ranges[] = {	\
	REGULATOR_LINEAR_RANGE(531216, 0x110, 0x200, 1953),		\
};

#define IS6615A_REGULATOR_DESC		\
static const struct pm8xx_buck_desc is6615a_buck_desc[] = {			\
	/* BUCK */		\
	PM8XX_DESC_COMMON(IS6615A_ID_DCDC1, "IDCDC_REG1",			\
			240, IS6615A_BUCK_VSEL_REG, IS6615A_BUCK_VSEL_MASK,	\
			IS6615A_BUCK_CTRL_REG, IS6615A_BUCK_EN_MASK,		\
			0, 0,							\
			is6615a_buck_ranges),	\
	PM8XX_DESC_COMMON(IS6615A_ID_DCDC2, "IDCDC_REG2",			\
			240, IS6615A_BUCK_VSEL_REG, IS6615A_BUCK_VSEL_MASK,	\
			IS6615A_BUCK_CTRL_REG, IS6615A_BUCK_EN_MASK,		\
			0, 0,							\
			is6615a_buck_ranges),	\
};

#define IS6615A_REGULATOR_MATCH_DATA						\
struct regulator_match_data is6615a_regulator_match_data = {			\
	.nr_buck_desc = ARRAY_SIZE(is6615a_buck_desc),				\
	.buck_desc = is6615a_buck_desc,					\
	.nr_ldo_desc = 0,							\
	.ldo_desc = NULL,							\
	.nr_switch_desc = 0,							\
	.switch_desc = NULL,							\
	.name = "is6615a",							\
	.max_registers = 0x22,							\
};

#define DECLEAR_IS6615A_REGULATOR_MATCH_DATA	extern struct regulator_match_data is6615a_regulator_match_data;

#endif
