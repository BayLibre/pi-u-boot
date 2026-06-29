/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2025, Spacemit
 */

#ifndef __MPQ8655_H__
#define __MPQ8655_H__

enum MPQ8655_reg {
	MPQ8655_ID_DCDC1,
};

#define SPACEMIT_MPQ8655_MAX_REG	0x2

#define MPQ8655_BUCK_VSEL_REG		0x21
#define MPQ8655_BUCK_VSEL_MASK		0xfff

/* No practical significance */
#define MPQ8655_BUCK_EN_MASK		0x0
/* No practical significance */
#define MPQ8655_BUCK_CTRL_REG		0x0

#define MPQ8655_BUCK_LINER_RANGE					\
static const struct pm8xx_linear_range mpq8655_buck_ranges[] = {	\
        REGULATOR_LINEAR_RANGE(534000, 0x10b, 0x1f4, 2000),		\
};

#define MPQ8655_REGULATOR_DESC		\
static const struct pm8xx_buck_desc mpq8655_buck_desc[] = {			\
	/* BUCK */		\
	PM8XX_DESC_COMMON(MPQ8655_ID_DCDC1, "EDCDC_REG1",			\
			337, MPQ8655_BUCK_VSEL_REG, MPQ8655_BUCK_VSEL_MASK,	\
			MPQ8655_BUCK_CTRL_REG, MPQ8655_BUCK_EN_MASK,		\
			0, 0,							\
			mpq8655_buck_ranges),	\
};

#define MPQ8655_REGULATOR_MATCH_DATA						\
struct regulator_match_data mpq8655_regulator_match_data = {			\
	.nr_buck_desc = ARRAY_SIZE(mpq8655_buck_desc),				\
	.buck_desc = mpq8655_buck_desc,						\
	.nr_ldo_desc = 0,							\
	.ldo_desc = NULL,							\
	.nr_switch_desc = 0,							\
	.switch_desc = NULL,							\
	.name = "mpq8655",							\
	.max_registers = 0x22,/* SPACEMIT_SY8810L_MAX_REG */			\
};

#define DECLEAR_MPQ8655_REGULATOR_MATCH_DATA	extern struct regulator_match_data mpq8655_regulator_match_data;

#endif
