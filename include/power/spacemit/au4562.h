/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2026, Spacemit
 */

#ifndef __AU4562_H__
#define __AU4562_H__

enum AU4562_reg {
	AU4562_ID_DCDC1,
	AU4562_ID_DCDC2,
};

#define SPACEMIT_AU4562_MAX_REG	0x2

#define AU4562_BUCK_VSEL_REG		0x21
#define AU4562_BUCK_VSEL_MASK		0xffff

/* No practical significance */
#define AU4562_BUCK_EN_MASK		0x0
/* No practical significance */
#define AU4562_BUCK_CTRL_REG		0x0

#define AU4562_BUCK_LINER_RANGE					\
static const struct pm8xx_linear_range au4562_buck_ranges[] = {	\
	REGULATOR_LINEAR_RANGE(540000, 0x6c, 0xc8, 5000),		\
};

#define AU4562_REGULATOR_DESC		\
static const struct pm8xx_buck_desc au4562_buck_desc[] = {			\
	/* BUCK */		\
	PM8XX_DESC_COMMON(AU4562_ID_DCDC1, "ADCDC_REG1",			\
			92, AU4562_BUCK_VSEL_REG, AU4562_BUCK_VSEL_MASK,	\
			AU4562_BUCK_CTRL_REG, AU4562_BUCK_EN_MASK,		\
			0, 0,							\
			au4562_buck_ranges),	\
	PM8XX_DESC_COMMON(AU4562_ID_DCDC2, "ADCDC_REG2",			\
			92, AU4562_BUCK_VSEL_REG, AU4562_BUCK_VSEL_MASK,	\
			AU4562_BUCK_CTRL_REG, AU4562_BUCK_EN_MASK,		\
			0, 0,							\
			au4562_buck_ranges),	\
};

#define AU4562_REGULATOR_MATCH_DATA						\
struct regulator_match_data au4562_regulator_match_data = {			\
	.nr_buck_desc = ARRAY_SIZE(au4562_buck_desc),				\
	.buck_desc = au4562_buck_desc,					\
	.nr_ldo_desc = 0,							\
	.ldo_desc = NULL,							\
	.nr_switch_desc = 0,							\
	.switch_desc = NULL,							\
	.name = "au4562",							\
	.max_registers = 0x22,							\
};

#define DECLEAR_AU4562_REGULATOR_MATCH_DATA	extern struct regulator_match_data au4562_regulator_match_data;

#endif
