/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2026, Spacemit
 */

#ifndef __TDA38740_H__
#define __TDA38740_H__

enum TDA38740_reg {
	TDA38740_ID_DCDC1,
	TDA38740_ID_DCDC2,
};

#define SPACEMIT_TDA38740_MAX_REG	0x2

#define TDA38740_BUCK_VSEL_REG		0x21
#define TDA38740_BUCK_VSEL_MASK		0xffff

/* No practical significance */
#define TDA38740_BUCK_EN_MASK		0x0
/* No practical significance */
#define TDA38740_BUCK_CTRL_REG		0x0

#define TDA38740_BUCK_LINER_RANGE					\
static const struct pm8xx_linear_range tda38740_buck_ranges[] = {	\
	REGULATOR_LINEAR_RANGE(531216, 0x88, 0x100, 3906),		\
};

#define TDA38740_REGULATOR_DESC		\
static const struct pm8xx_buck_desc tda38740_buck_desc[] = {			\
	/* BUCK */		\
	PM8XX_DESC_COMMON(TDA38740_ID_DCDC1, "TDCDC_REG1",			\
			120, TDA38740_BUCK_VSEL_REG, TDA38740_BUCK_VSEL_MASK,	\
			TDA38740_BUCK_CTRL_REG, TDA38740_BUCK_EN_MASK,		\
			0, 0,							\
			tda38740_buck_ranges),	\
	PM8XX_DESC_COMMON(TDA38740_ID_DCDC2, "TDCDC_REG2",			\
			120, TDA38740_BUCK_VSEL_REG, TDA38740_BUCK_VSEL_MASK,	\
			TDA38740_BUCK_CTRL_REG, TDA38740_BUCK_EN_MASK,		\
			0, 0,							\
			tda38740_buck_ranges),	\
};

#define TDA38740_REGULATOR_MATCH_DATA						\
struct regulator_match_data tda38740_regulator_match_data = {			\
	.nr_buck_desc = ARRAY_SIZE(tda38740_buck_desc),				\
	.buck_desc = tda38740_buck_desc,					\
	.nr_ldo_desc = 0,							\
	.ldo_desc = NULL,							\
	.nr_switch_desc = 0,							\
	.switch_desc = NULL,							\
	.name = "tda38740",							\
	.max_registers = 0x22,							\
};

#define DECLEAR_TDA38740_REGULATOR_MATCH_DATA	extern struct regulator_match_data tda38740_regulator_match_data;

#endif
