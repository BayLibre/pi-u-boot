// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025, Spacemit
 */

#include <i2c.h>
#include <fdtdec.h>
#include <vsprintf.h>
#include <log.h>
#include <asm/global_data.h>
#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/libfdt.h>
#include <linux/string.h>
#include <linux/types.h>
#include <power/spacemit/spacemit_pmic.h>

DECLARE_GLOBAL_DATA_PTR;

SPM8821_BUCK_LINER_RANGE; SPM8821_LDO_LINER_RANGE /* ; SPM8821_SWITCH_LINER_RANGE */;
SPM8821_REGULATOR_BUCK_DESC; SPM8821_REGULATOR_LDO_DESC/* ; SPM8821_REGULATOR_SWITCH_DESC */;

#ifdef CONFIG_TARGET_SPACEMIT_K1X
PM853_BUCK_LINER_RANGE1; PM853_BUCK_LINER_RANGE2; PM853_LDO_LINER_RANGE1; PM853_LDO_LINER_RANGE2;
PM853_LDO_LINER_RANGE3; PM853_LDO_LINER_RANGE4; /* PM853_SWITCH_LINER_RANGE; */
PM853_REGULATOR_BUCK_DESC; PM853_REGULATOR_LDO_DESC; /* PM853_REGULATOR_SWITCH_DESC; */

SY8810L_BUCK_LINER_RANGE;SY8810L_REGULATOR_DESC;
#endif

MPQ8655_BUCK_LINER_RANGE;MPQ8655_REGULATOR_DESC;
TDA38740_BUCK_LINER_RANGE;TDA38740_REGULATOR_DESC;
IS6615A_BUCK_LINER_RANGE;IS6615A_REGULATOR_DESC;
AU4562_BUCK_LINER_RANGE;AU4562_REGULATOR_DESC;

static const char *global_compatible[] = {
	"spacemit,spm8821",
#ifdef CONFIG_TARGET_SPACEMIT_K1X
	"spacemit,pm853",
	"spacemit,sy8810l",
#endif
};

struct tlv_pmic_info {
	const char *name;
	const char *compatibles[2];
};

static const struct tlv_pmic_info tlv_pmic_infos[] = {
	{
		.name = "mpq8655",
		.compatibles = {
			"spacemit,mpq8655",
		},
	},
	{
		.name = "tda38740",
		.compatibles = {
			"spacemit,tda38740-1",
			"spacemit,tda38740-2",
		},
	},
	{
		.name = "is6615a",
		.compatibles = {
			"spacemit,is6615a-1",
			"spacemit,is6615a-2",
		},
	},
	{
		.name = "au4562",
		.compatibles = {
			"spacemit,au4562",
		},
	},
};

#define TLV_PMIC_TYPE_MAX_LEN	32

static const struct tlv_pmic_info *board_pmic_tlv_info(void)
{
	uint8_t pmic_name[TLV_PMIC_TYPE_MAX_LEN] = { 0 };
	int i;
	int j;
	int ret;

	ret = get_tlvinfo(TLV_CODE_PMIC_TYPE, pmic_name, sizeof(pmic_name) - 1);
	if (ret > 0) {
		pmic_name[sizeof(pmic_name) - 1] = '\0';

		for (i = 0; i < ARRAY_SIZE(tlv_pmic_infos); ++i) {
			if (!strcmp((const char *)pmic_name, tlv_pmic_infos[i].name))
				return &tlv_pmic_infos[i];

			for (j = 0; j < ARRAY_SIZE(tlv_pmic_infos[i].compatibles); ++j) {
				if (!tlv_pmic_infos[i].compatibles[j])
					continue;
				if (!strcmp((const char *)pmic_name, tlv_pmic_infos[i].compatibles[j]))
					return &tlv_pmic_infos[i];
			}
		}
	}

	return &tlv_pmic_infos[0];
}

#define NRESET_BIT	(1 << 6)
#define RTC_ENABLE	(0xf)
#define RTC_IRQ_ENABLE	(1 << 4)
#define INT_STA_EN_BIT	(1 << 2)
#define PWRKEY_IRQ_ENABLE	(0x3)
#define EXT2_SLP_SD	(1 << 2)
#define EXT1_SLP_SD	(1 << 1)
#define EXT3_SLP_SD	(1 << 3)

#define MPQ8655_COMPAT_PREFIX	"spacemit,mpq8655"
#define TDA38740_COMPAT_PREFIX	"spacemit,tda38740"
#define IS6615A_COMPAT_PREFIX	"spacemit,is6615a"
#define AU4562_COMPAT_PREFIX	"spacemit,au4562"

static bool pmic_name_match(const char *name, const char *prefix)
{
	return !strncmp(name, prefix, strlen(prefix));
}

void __regulator_desc_find(const char *name, const struct pm8xx_buck_desc **buck_desc,
		const struct pm8xx_buck_desc **ldo_desc, int *num_buck, int *num_ldo)
{
	if (strcmp(name, global_compatible[0]) == 0) {
		*buck_desc = spm8821_buck_desc;
		*num_buck = sizeof(spm8821_buck_desc) / sizeof(spm8821_buck_desc[0]);
		*ldo_desc = spm8821_ldo_desc;
		*num_ldo = sizeof(spm8821_ldo_desc) / sizeof(spm8821_ldo_desc[0]);
	} else if (pmic_name_match(name, MPQ8655_COMPAT_PREFIX)) {
		*buck_desc = mpq8655_buck_desc;
		*num_buck = sizeof(mpq8655_buck_desc) / sizeof(mpq8655_buck_desc[0]);
		*ldo_desc = NULL;
		*num_ldo = 0;
	} else if (pmic_name_match(name, TDA38740_COMPAT_PREFIX)) {
		*buck_desc = tda38740_buck_desc;
		*num_buck = sizeof(tda38740_buck_desc) / sizeof(tda38740_buck_desc[0]);
		*ldo_desc = NULL;
		*num_ldo = 0;
	} else if (pmic_name_match(name, IS6615A_COMPAT_PREFIX)) {
		*buck_desc = is6615a_buck_desc;
		*num_buck = sizeof(is6615a_buck_desc) / sizeof(is6615a_buck_desc[0]);
		*ldo_desc = NULL;
		*num_ldo = 0;
	} else if (pmic_name_match(name, AU4562_COMPAT_PREFIX)) {
		*buck_desc = au4562_buck_desc;
		*num_buck = sizeof(au4562_buck_desc) / sizeof(au4562_buck_desc[0]);
		*ldo_desc = NULL;
		*num_ldo = 0;
#ifdef CONFIG_TARGET_SPACEMIT_K1X
	} else if (strcmp(name, global_compatible[ARRAY_SIZE(global_compatible) - 2]) == 0) {
		*buck_desc = pm853_buck_desc;
		*num_buck = sizeof(pm853_buck_desc) / sizeof(pm853_buck_desc[0]);
		*ldo_desc = pm853_ldo_desc;
		*num_ldo = sizeof(pm853_ldo_desc) / sizeof(pm853_ldo_desc[0]);
	} else if (strcmp(name, global_compatible[ARRAY_SIZE(global_compatible) - 1]) == 0) {
		*buck_desc = sy8810l_buck_desc;
		*num_buck = sizeof(sy8810l_buck_desc) / sizeof(sy8810l_buck_desc[0]);
		*ldo_desc = NULL;
		*num_ldo = 0;
#endif
 	} else {
		/* TODO */
	}
}

/**
 * linear_range_get_value - fetch a value from given range
 * @r:	  pointer to linear range where value is looked from
 * @selector:   selector for which the value is searched
 * @val:	address where found value is updated
 *
 * Search given ranges for value which matches given selector.
 *
 * Return: 0 on success, -EINVAL given selector is not found from any of the
 * ranges.
 */
static int linear_range_get_value(const struct pm8xx_linear_range *r, unsigned int selector,
			   unsigned int *val)
{
	if (r->min_sel > selector || r->max_sel < selector)
		return -EINVAL;

	*val = r->min + (selector - r->min_sel) * r->step;

	return 0;
}

/**
 * regulator_map_voltage_linear_range - map_voltage() for multiple linear ranges
 *
 * @rdev: Regulator to operate on
 * @min_uV: Lower bound for voltage
 * @max_uV: Upper bound for voltage
 *
 * Drivers providing linear_ranges in their descriptor can use this as
 * their map_voltage() callback.
 */
static int regulator_map_voltage_linear_range(const struct pm8xx_buck_desc *desc,
				       int min_uV, int max_uV)
{
	int best_sel = -EINVAL;
	int best_voltage = 0;
	int best_diff = INT_MAX;
	int best_below_sel = -EINVAL;
	int best_below_voltage = 0;
	int best_below_diff = INT_MAX;
	unsigned int value;
	int target_uV;
	int i;
	bool used_fallback = false;

	if (!desc->n_linear_ranges) {
		BUG_ON(!desc->n_linear_ranges);
		return -EINVAL;
	}

	target_uV = min_uV;

	for (i = 0; i < desc->n_linear_ranges; i++) {
		const struct pm8xx_linear_range *range = &desc->linear_ranges[i];
		unsigned int sel;

		for (sel = range->min_sel; sel <= range->max_sel; sel++) {
			int voltage;
			int diff;

			if (linear_range_get_value(range, sel, &value))
				continue;

			voltage = value;
			diff = abs(voltage - target_uV);

			if (voltage >= min_uV && voltage <= max_uV) {
				if (diff < best_diff ||
				    (diff == best_diff && voltage > best_voltage)) {
					best_diff = diff;
					best_sel = sel;
					best_voltage = voltage;
				}
			} else if (voltage <= max_uV) {
				if (diff < best_below_diff ||
				    (diff == best_below_diff && voltage > best_below_voltage)) {
					best_below_diff = diff;
					best_below_sel = sel;
					best_below_voltage = voltage;
				}
			}
		}
	}

	if (best_sel < 0 && best_below_sel >= 0) {
		used_fallback = true;
		best_sel = best_below_sel;
		best_voltage = best_below_voltage;
	}

	if (best_sel < 0)
		return -EINVAL;

	return best_sel;
}

static int __board_pmic_init(const char *name)
{
	unsigned char regval;
	unsigned char regvals[2];
	const char *s;
	u32 value, min, max, req_value;
	const struct pm8xx_buck_desc *buck_desc, *ldo_desc;
	int offset, ret, sub_offset, len, saddr, i, num_buck, num_ldo, sel;

	offset = fdt_node_offset_by_compatible(gd->fdt_blob, -1, name);
	if (offset < 0)
		return -EINVAL;

	saddr = fdtdec_get_uint(gd->fdt_blob, offset, "reg", 0);
	if (!saddr)
		return -EINVAL;

	int bus;
	/* Legacy I2C mode: use bus property */
	bus = fdtdec_get_uint(gd->fdt_blob, offset, "bus", 0);
	if (!bus)
		return -EINVAL;

	ret = i2c_set_bus_num(bus);
	if (ret < 0)
		return -EINVAL;

#if defined(CONFIG_K3_BOARD_ASIC)
	if (!strncmp(name, "spacemit,spm8821", 16)) {
		/* enable p1 wdt reset */
		i2c_read(saddr, 0x7c, 1, &regval, 1);
		regval |= NRESET_BIT;
		i2c_write(saddr, 0x7c, 1, &regval, 1);

		/* Disable the INT trigger power-on function */
		i2c_read(saddr, 0x7c, 1, &regval, 1);
		regval &= ~INT_STA_EN_BIT;
		i2c_write(saddr, 0x7c, 1, &regval, 1);

		/* enable rtc func in spl */
		i2c_read(saddr, 0x1d, 1, &regval, 1);
		regval |= RTC_ENABLE;
		i2c_write(saddr, 0x1d, 1, &regval, 1);

		i2c_read(saddr, 0x99, 1, &regval, 1);
		regval |= RTC_IRQ_ENABLE;
		i2c_write(saddr, 0x99, 1, &regval, 1);

		i2c_read(saddr, 0x9e, 1, &regval, 1);
		regval |= PWRKEY_IRQ_ENABLE;
		i2c_write(saddr, 0x9e, 1, &regval, 1);

		/*
		 * don't power down x100 & a100 when system standby
		 * */
		i2c_read(saddr, 0x90, 1, &regval, 1);
		regval &= ~(EXT2_SLP_SD | EXT1_SLP_SD | EXT3_SLP_SD);
		i2c_write(saddr, 0x90, 1, &regval, 1);
	}
#endif

	__regulator_desc_find(name, &buck_desc, &ldo_desc, &num_buck, &num_ldo);

	offset = fdt_first_subnode(gd->fdt_blob, offset);

	for (sub_offset = fdt_first_subnode(gd->fdt_blob, offset);
		sub_offset >= 0;
		sub_offset = fdt_next_subnode(gd->fdt_blob, sub_offset)) {

		/* find regulator-boot-on property */
		if (!fdt_getprop(gd->fdt_blob, sub_offset, "regulator-boot-on", &len))
			continue;

		max = fdtdec_get_uint(gd->fdt_blob, sub_offset, "regulator-max-microvolt", 0);
		if (!max)
			continue;

		min = fdtdec_get_uint(gd->fdt_blob, sub_offset, "regulator-min-microvolt", 0);
		if (!min)
			continue;

		value = fdtdec_get_uint(gd->fdt_blob, sub_offset, "regulator-init-microvolt", 0);
		req_value = value;

		/* find wich dcdc or ldo */
		s = fdt_get_name(gd->fdt_blob, sub_offset, &len);
		if (pmic_name_match(name, MPQ8655_COMPAT_PREFIX)) {
			if ((strncmp(s, "EDCDC_REG", 9) == 0)) {
				/* set the regulator */
				if (value) {
					sel = regulator_map_voltage_linear_range(buck_desc, value, value);
					if (sel >= 0) {
						unsigned int rounded_uV;

						linear_range_get_value(&buck_desc[0].linear_ranges[0], sel, &rounded_uV);
					}

					/* default:0.9v */
					regvals[0] = 0x9b;
					regvals[1] = 0x2;
					i2c_write(saddr, 0x29, 1, regvals, 2);

					/* then: set 0x21 */
					if (sel >= 0) {
						sel <<= ffs(buck_desc[0].vsel_msk) - 1;
						i2c_read(saddr, buck_desc[0].vsel_reg, 1, regvals, 2);
						value = regvals[0] | ((unsigned short)regvals[1] << 8);
						value = (value & ~buck_desc[0].vsel_msk) | sel;

						regvals[0] = value & 0xff;
						regvals[1] = (value >> 8) & 0xff;
						i2c_write(saddr, buck_desc[0].vsel_reg, 1, regvals, 2);
					}
				}
			}
		} else if (pmic_name_match(name, IS6615A_COMPAT_PREFIX) || pmic_name_match(name, TDA38740_COMPAT_PREFIX)) {
			if ((strncmp(s, "IDCDC_REG", 9) == 0) || (strncmp(s, "TDCDC_REG", 9) == 0)) {
				for (i = 0; i < num_buck; ++i) {
					if (strcmp(buck_desc[i].name, s) != 0)
						continue;

					/* set the regulator */
					if (value) {
						sel = regulator_map_voltage_linear_range(buck_desc + i, value, max);
						i2c_read(saddr, 0x20, 1, regvals, 1);
						if (sel >= 0) {
							sel <<= ffs(buck_desc[i].vsel_msk) - 1;
							i2c_read(saddr, buck_desc[i].vsel_reg, 1, regvals, 2);
							value = regvals[0] | ((unsigned short)regvals[1] << 8);
							value = (value & ~buck_desc[i].vsel_msk) | sel;

							regvals[0] = value & 0xff;
							regvals[1] = (value >> 8) & 0xff;
							i2c_write(saddr, buck_desc[i].vsel_reg, 1, regvals, 2);
						}
					}
					break;
				}
			}
		} else if (pmic_name_match(name, AU4562_COMPAT_PREFIX)) {
			if (strncmp(s, "ADCDC_REG", 9) == 0) {
				for (i = 0; i < num_buck; ++i) {
					if (strcmp(buck_desc[i].name, s) != 0)
						continue;

					if (strncmp(s, "ADCDC_REG1", 10) == 0)
						regval = 0x1;
					else if (strncmp(s, "ADCDC_REG2", 10) == 0)
						regval = 0x0;

					i2c_write(saddr, 0x00, 1, &regval, 1);

					/* set the regulator */
					if (value) {
						sel = regulator_map_voltage_linear_range(buck_desc + i, value, max);
						i2c_read(saddr, 0x20, 1, regvals, 1);
						if (sel >= 0) {
							sel <<= ffs(buck_desc[i].vsel_msk) - 1;
							i2c_read(saddr, buck_desc[i].vsel_reg, 1, regvals, 2);
							value = regvals[0] | ((unsigned short)regvals[1] << 8);
							value = (value & ~buck_desc[i].vsel_msk) | sel;

							regvals[0] = value & 0xff;
							regvals[1] = (value >> 8) & 0xff;
							i2c_write(saddr, buck_desc[i].vsel_reg, 1, regvals, 2);
						}
					}
					break;
				}
			}
		} else {
			if ((strncmp(s, "DCDC_REG", 8) == 0) || (strncmp(s, "EDCDC_REG", 9) == 0)) {
				for (i = 0; i < num_buck; ++i) {
					if (strcmp(buck_desc[i].name, s) == 0) {
						/* enable the regulator */
						i2c_read(saddr, buck_desc[i].enable_reg, 1, &regval, 1);
						regval |= (1 << (ffs(buck_desc[i].enable_msk) - 1));
						i2c_write(saddr, buck_desc[i].enable_reg, 1, &regval, 1);

						/* set the regulator */
						if (value) {
							sel = regulator_map_voltage_linear_range(buck_desc + i, value, value);
							if (sel >= 0) {
								sel <<= ffs(buck_desc[i].vsel_msk) - 1;
								i2c_read(saddr, buck_desc[i].vsel_reg, 1, &regval, 1);
								regval = (regval & ~buck_desc[i].vsel_msk) | sel;
								i2c_write(saddr, buck_desc[i].vsel_reg, 1, &regval, 1);
							}
						}
						break;
					}
				}
			}
		}

		if (strncmp(s, "LDO_REG", 7) == 0) {
			for (i = 0; i < num_ldo; ++i) {
				if (strcmp(ldo_desc[i].name, s) == 0) {
					/* enable the regulator */
					i2c_read(saddr, ldo_desc[i].enable_reg, 1, &regval, 1);
					regval |= (1 << (ffs(ldo_desc[i].enable_msk) - 1));
					i2c_write(saddr, ldo_desc[i].enable_reg, 1, &regval, 1);

					/* set the regulator */
					if (value) {
						sel = regulator_map_voltage_linear_range(ldo_desc + i, value, value);

						if (sel >= 0) {
							sel <<= ffs(ldo_desc[i].vsel_msk) - 1;

							i2c_read(saddr, ldo_desc[i].vsel_reg, 1, &regval, 1);
							regval = (regval & ~ldo_desc[i].vsel_msk) | sel;
							i2c_write(saddr, ldo_desc[i].vsel_reg, 1, &regval, 1);
						}
					}

					break;
				}
			}
		}
	}

	return 0;
}

int board_pmic_init(void)
{
	const struct tlv_pmic_info *pmic_info;
	int i, j;

	for (i = 0; i < sizeof(global_compatible) / sizeof(global_compatible[0]); ++i)
		__board_pmic_init(global_compatible[i]);

	pmic_info = board_pmic_tlv_info();
	for (j = 0; j < ARRAY_SIZE(pmic_info->compatibles); ++j) {
		if (!pmic_info->compatibles[j])
			continue;
		__board_pmic_init(pmic_info->compatibles[j]);
	}

	return 0;
}
