// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025, Spacemit
 */

#include <dm.h>
#include <errno.h>
#include <log.h>
#include <power/pmic.h>
#include <power/regulator.h>
#include <linux/bug.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include "regulator_common.h"
#include <power/spacemit/spacemit_pmic.h>

SPM8821_BUCK_LINER_RANGE; SPM8821_LDO_LINER_RANGE; SPM8821_SWITCH_LINER_RANGE;
SPM8821_REGULATOR_BUCK_DESC; SPM8821_REGULATOR_LDO_DESC; SPM8821_REGULATOR_SWITCH_DESC;
SPM8821_REGULATOR_MATCH_DATA;

MPQ8655_BUCK_LINER_RANGE; MPQ8655_REGULATOR_DESC; MPQ8655_REGULATOR_MATCH_DATA;
TDA38740_BUCK_LINER_RANGE; TDA38740_REGULATOR_DESC; TDA38740_REGULATOR_MATCH_DATA;
IS6615A_BUCK_LINER_RANGE; IS6615A_REGULATOR_DESC; IS6615A_REGULATOR_MATCH_DATA;
AU4562_BUCK_LINER_RANGE; AU4562_REGULATOR_DESC; AU4562_REGULATOR_MATCH_DATA;

#ifdef CONFIG_TARGET_SPACEMIT_K1X
PM853_BUCK_LINER_RANGE1; PM853_BUCK_LINER_RANGE2; PM853_LDO_LINER_RANGE1; PM853_LDO_LINER_RANGE2;
PM853_LDO_LINER_RANGE3; PM853_LDO_LINER_RANGE4; PM853_SWITCH_LINER_RANGE;
PM853_REGULATOR_BUCK_DESC; PM853_REGULATOR_LDO_DESC; PM853_REGULATOR_SWITCH_DESC;
PM853_REGULATOR_MATCH_DATA;

SY8810L_BUCK_LINER_RANGE; SY8810L_REGULATOR_DESC; SY8810L_REGULATOR_MATCH_DATA;
#endif

/**
 * linear_range_get_value - fetch a value from given range
 * @r:          pointer to linear range where value is looked from
 * @selector:   selector for which the value is searched
 * @val:        address where found value is updated
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
 * linear_range_get_value_array - fetch a value from array of ranges
 * @r:          pointer to array of linear ranges where value is looked from
 * @ranges:     amount of ranges in an array
 * @selector:   selector for which the value is searched
 * @val:        address where found value is updated
 *
 * Search through an array of ranges for value which matches given selector.
 *
 * Return: 0 on success, -EINVAL given selector is not found from any of the
 * ranges.
 */
static int linear_range_get_value_array(const struct pm8xx_linear_range *r, int ranges,
                                 unsigned int selector, unsigned int *val)
{
        int i;

        for (i = 0; i < ranges; i++)
                if (r[i].min_sel <= selector && r[i].max_sel >= selector)
                        return linear_range_get_value(&r[i], selector, val);

        return -EINVAL;
}

/**
 * regulator_desc_list_voltage_linear_range - List voltages for linear ranges
 *
 * @desc: Regulator desc for regulator which volatges are to be listed
 * @selector: Selector to convert into a voltage
 *
 * Regulators with a series of simple linear mappings between voltages
 * and selectors who have set linear_ranges in the regulator descriptor
 * can use this function prior regulator registration to list voltages.
 * This is useful when voltages need to be listed during device-tree
 * parsing.
 */
static int regulator_desc_list_voltage_linear_range(const struct pm8xx_buck_desc *desc,
                                             unsigned int selector)
{
        unsigned int val;
        int ret;

        BUG_ON(!desc->n_linear_ranges);

        ret = linear_range_get_value_array(desc->linear_ranges,
                                           desc->n_linear_ranges, selector,
                                           &val);
        if (ret)
                return ret;

        return val;
}

/**
 * linear_range_get_max_value - return the largest value in a range
 * @r:          pointer to linear range where value is looked from
 *
 * Return: the largest value in the given range
 */
static unsigned int linear_range_get_max_value(const struct pm8xx_linear_range *r)
{
        return r->min + (r->max_sel - r->min_sel) * r->step;
}

static int linear_range_get_selector_low(const struct pm8xx_linear_range *r,
				  unsigned int val, unsigned int *selector)
{
	if (r->min > val)
		return -EINVAL;

	if (linear_range_get_max_value(r) <= val) {
		*selector = r->max_sel;
		return 0;
	}

	if (r->step == 0) {
		*selector = r->min_sel;
		return 0;
	}

	*selector = (val - r->min) / r->step + r->min_sel;

	if (*selector > r->max_sel)
		*selector = r->max_sel;

	return 0;
}

/**
 * linear_range_get_selector_high - return linear range selector for value
 * @r:          pointer to linear range where selector is looked from
 * @val:        value for which the selector is searched
 * @selector:   address where found selector value is updated
 * @found:      flag to indicate that given value was in the range
 *
 * Return selector for which range value is closest match for given
 * input value. Value is matching if it is equal or higher than given
 * value. If given value is in the range, then @found is set true.
 *
 * Return: 0 on success, -EINVAL if range is invalid or does not contain
 * value greater or equal to given value
 */
static int linear_range_get_selector_high(const struct pm8xx_linear_range *r,
                                   unsigned int val, unsigned int *selector,
                                   bool *found)
{
        *found = false;

        if (linear_range_get_max_value(r) < val)
                return -EINVAL;

        if (r->min > val) {
                *selector = r->min_sel;
                return 0;
        }

        *found = true;

        if (r->step == 0)
                *selector = r->max_sel;
        else
                *selector = DIV_ROUND_UP(val - r->min, r->step) + r->min_sel;

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
        const struct pm8xx_linear_range *range;
	int ret = -EINVAL;
	unsigned int sel, low_sel;
	bool found;
	int voltage, i;
	unsigned int best_sel = 0, best_below_sel = 0;
	int best_voltage = 0, best_below_voltage = 0;
	int best_diff = INT_MAX, best_below_diff = INT_MAX;

        if (!desc->n_linear_ranges) {
                BUG_ON(!desc->n_linear_ranges);
                return -EINVAL;
        }

        for (i = 0; i < desc->n_linear_ranges; i++) {
                range = &desc->linear_ranges[i];

                ret = linear_range_get_selector_high(range, min_uV, &sel,
                                                     &found);
			if (ret)
                        continue;

                /*
                 * Map back into a voltage to verify we're still in bounds.
                 * If we are not, then continue checking rest of the ranges.
                 */
		voltage = regulator_desc_list_voltage_linear_range(desc, sel);
			if (voltage >= min_uV && voltage <= max_uV) {
				int diff = voltage - min_uV;

				if (diff < best_diff) {
					best_diff = diff;
					best_sel = sel;
					best_voltage = voltage;
				}
			}

			ret = linear_range_get_selector_low(range, max_uV, &low_sel);
			if (ret)
				continue;

			voltage = regulator_desc_list_voltage_linear_range(desc, low_sel);
			if (voltage <= max_uV) {
				int diff = min_uV - voltage;

				if (diff >= 0 && diff < best_below_diff) {
					best_below_diff = diff;
					best_below_sel = low_sel;
					best_below_voltage = voltage;
				}
			}
        }

	if (best_diff != INT_MAX) {
		ret = best_sel;
		voltage = best_voltage;
	} else if (best_below_diff != INT_MAX) {
		ret = best_below_sel;
		voltage = best_below_voltage;
	} else {
                return -EINVAL;
	}

        return ret;
}

static const struct pm8xx_buck_desc *get_buck_reg(struct udevice *pmic, int num)
{
	struct pm8xx_priv *priv = dev_get_priv(pmic);
	struct regulator_match_data *math = (struct regulator_match_data *)priv->match;

	if (!math || !math->buck_desc || math->nr_buck_desc <= 0)
		return NULL;

	if (num < 0)
		return NULL;

	if (num >= math->nr_buck_desc) {
		num = 0;
	}

	return math->buck_desc + num;

	return NULL;
}

static int get_buck_reg_index(struct udevice *dev)
{
	int buck = dev->driver_data - 1;

	return buck;
}

static int buck_get_value(struct udevice *dev)
{
	int buck = get_buck_reg_index(dev);
	const struct pm8xx_buck_desc *info = get_buck_reg(dev->parent, buck);
	struct pm8xx_priv *priv = dev_get_priv(dev->parent);
	int mask = info->vsel_msk;
	int ret;
	unsigned int val;

	if (info == NULL)
		return -ENOSYS;

	if (strcmp(priv->match->name, "spm8821") != 0) {
		unsigned char vals[2];

		if (strcmp(priv->match->name, "au4562") == 0) {
			if (buck == 0)
				val = 0x1;
			else if (buck == 1)
				val = 0x0;

			vals[0] = val;
			pmic_write(dev->parent, 0x0, vals, 1);
		}

		ret = pmic_read(dev->parent, info->vsel_reg, vals, 2);
		if (ret < 0)
			return ret;

		val = vals[0] | (((unsigned short)vals[1]) << 8);
	} else {
		ret = pmic_reg_read(dev->parent, info->vsel_reg);
		if (ret < 0)
			return ret;
	}

	val = ret & mask;

	val >>= ffs(mask) - 1;

	return regulator_desc_list_voltage_linear_range(info, val);
}

static int buck_set_value(struct udevice *dev, int uvolt)
{
	int sel, ret = -EINVAL;
	int buck = get_buck_reg_index(dev);
	const struct pm8xx_buck_desc *info = get_buck_reg(dev->parent, buck);
	struct pm8xx_priv *priv = dev_get_priv(dev->parent);

	if (info == NULL)
		return -ENOSYS;

	sel = regulator_map_voltage_linear_range(info, uvolt, uvolt);
	if (sel >=0) {
		/* has get the selctor */
		sel <<= ffs(info->vsel_msk) - 1;
		if (strcmp(priv->match->name, "spm8821") != 0) {
			unsigned char vals[2];
			unsigned int val;

			if (strcmp(priv->match->name, "au4562") == 0) {
				if (buck == 0)
					val = 0x1;
				else if (buck == 1)
					val = 0x0;

				vals[0] = val;
				pmic_write(dev->parent, 0x0, vals, 1);
			}

			ret = pmic_read(dev->parent, info->vsel_reg, vals, 2);
			if (ret < 0)
				return ret;

			val = vals[0] | (((unsigned short)vals[1]) << 8);
			val = (val & ~info->vsel_msk) | sel;

			vals[0] = val & 0xff;
			vals[1] = (val >> 8) & 0xff;

			ret = pmic_write(dev->parent, info->vsel_reg, vals, 2);
		 } else {
			ret = pmic_clrsetbits(dev->parent, info->vsel_reg, info->vsel_msk, sel);
		 }
	}

	return ret;
}

static int buck_set_suspend_value(struct udevice *dev, int uvolt)
{
	/* the hardware has already support the function */
	int sel, ret = -EINVAL;
	int buck = get_buck_reg_index(dev);
	const struct pm8xx_buck_desc *info = get_buck_reg(dev->parent, buck);
	struct pm8xx_priv *priv = dev_get_priv(dev->parent);

	if (info == NULL)
		return -ENOSYS;

	if (strcmp(priv->match->name, "spm8821") != 0)
		return -ENOSYS;

	sel = regulator_map_voltage_linear_range(info, uvolt, uvolt);
	if (sel >=0) {
		 // has get the selctor
		 sel <<= ffs(info->vsel_sleep_msk) - 1;
		 ret = pmic_clrsetbits(dev->parent, info->vsel_sleep_reg, info->vsel_sleep_msk, sel);
	}

	return 1;
}

static int buck_get_suspend_value(struct udevice *dev)
{
	/* the hardware has already support the function */
	int buck = get_buck_reg_index(dev);
	const struct pm8xx_buck_desc *info = get_buck_reg(dev->parent, buck);
	int mask = info->vsel_sleep_msk;
	struct pm8xx_priv *priv = dev_get_priv(dev->parent);
	int ret;
	unsigned int val;

	if (info == NULL)
		return -ENOSYS;

	if (strcmp(priv->match->name, "spm8821") != 0)
		return -ENOSYS;

	ret = pmic_reg_read(dev->parent, info->vsel_sleep_reg);
	if (ret < 0)
		return ret;
	val = ret & mask;

	val >>= ffs(mask) - 1;

	return regulator_desc_list_voltage_linear_range(info, val);
}

static int buck_get_enable(struct udevice *dev)
{
	int ret, val;
	int buck = get_buck_reg_index(dev);
	const struct pm8xx_buck_desc *info = get_buck_reg(dev->parent, buck);
	struct pm8xx_priv *priv = dev_get_priv(dev->parent);
	int mask = info->enable_msk;

	if (info == NULL)
		return -ENOSYS;

	/* enabled by default, controled by p1 */
	if (strcmp(priv->match->name, "spm8821") != 0)
		return 1;

	ret = pmic_reg_read(dev->parent, info->enable_reg);
	if (ret < 0)
		return ret;

	val = ret & mask;

	val >>= ffs(mask) - 1;

	return val;
}

static int buck_set_enable(struct udevice *dev, bool enable)
{
	int ret;
	unsigned int val = 0;
	int buck = get_buck_reg_index(dev);
	const struct pm8xx_buck_desc *info = get_buck_reg(dev->parent, buck);
	struct pm8xx_priv *priv = dev_get_priv(dev->parent);
	int mask = info->enable_msk;

	/* uboot can't disable it */
	if (strcmp(priv->match->name, "spm8821") != 0) {
		if (enable == false)
			return -EPERM;
		else
			return 0;
	}

	ret = pmic_reg_read(dev->parent, info->enable_reg);
	if (ret < 0)
		return ret;

	val = (unsigned int)ret;
	val &= mask;
	val >>= ffs(mask) - 1;

	if (enable == val)
		return 0;

	val = enable << (ffs(mask) - 1);

	ret = pmic_clrsetbits(dev->parent, info->enable_reg, info->enable_msk, val);
	if (ret < 0)
		return ret;

	return 0;
}

static int buck_set_suspend_enable(struct udevice *dev, bool enable)
{
	/* TODO */
	return 0;
}

static int buck_get_suspend_enable(struct udevice *dev)
{
	/* TODO */
	return 0;
}

static int pm8xx_buck_probe(struct udevice *dev)
{
	struct dm_regulator_uclass_plat *uc_pdata;

	uc_pdata = dev_get_uclass_plat(dev);

	uc_pdata->type = REGULATOR_TYPE_BUCK;
	uc_pdata->mode_count = 0;

	return 0;
}

static const struct dm_regulator_ops pm8xx_buck_ops = {
	.get_value  = buck_get_value,
	.set_value  = buck_set_value,
	.set_suspend_value = buck_set_suspend_value,
	.get_suspend_value = buck_get_suspend_value,
	.get_enable = buck_get_enable,
	.set_enable = buck_set_enable,
	.set_suspend_enable = buck_set_suspend_enable,
	.get_suspend_enable = buck_get_suspend_enable,
};

U_BOOT_DRIVER(pm8xx_buck) = {
	.name = "pm8xx_buck",
	.id = UCLASS_REGULATOR,
	.ops = &pm8xx_buck_ops,
	.probe = pm8xx_buck_probe,
};

static const struct pm8xx_buck_desc *get_ldo_reg(struct udevice *pmic, int num)
{
	struct pm8xx_priv *priv = dev_get_priv(pmic);
	struct regulator_match_data *math = (struct regulator_match_data *)priv->match;

	return math->ldo_desc + num;

	return NULL;
}

static int ldo_get_value(struct udevice *dev)
{
	int buck = dev->driver_data - 1;
	const struct pm8xx_buck_desc *info = get_ldo_reg(dev->parent, buck);
	int mask = info->vsel_msk;
	int ret;
	unsigned int val;

	if (info == NULL)
		return -ENOSYS;

	ret = pmic_reg_read(dev->parent, info->vsel_reg);
	if (ret < 0)
		return ret;

	val = ret & mask;

	val >>= ffs(mask) - 1;

	return regulator_desc_list_voltage_linear_range(info, val);
}

static int ldo_set_value(struct udevice *dev, int uvolt)
{
	int sel, ret = -EINVAL;
	int buck = dev->driver_data - 1;
	const struct pm8xx_buck_desc *info = get_ldo_reg(dev->parent, buck);

	if (info == NULL)
		return -ENOSYS;

	sel = regulator_map_voltage_linear_range(info, uvolt, uvolt);
	if (sel >=0) {
		/* has get the selctor */
		 sel <<= ffs(info->vsel_msk) - 1;
		 ret = pmic_clrsetbits(dev->parent, info->vsel_reg, info->vsel_msk, sel);
	}

	return ret;
}

static int ldo_set_suspend_value(struct udevice *dev, int uvolt)
{
	int sel, ret = -EINVAL;
	int buck = dev->driver_data - 1;
	const struct pm8xx_buck_desc *info = get_ldo_reg(dev->parent, buck);

	if (info == NULL)
		return -ENOSYS;

	sel = regulator_map_voltage_linear_range(info, uvolt, uvolt);
	if (sel >=0) {
		sel <<= ffs(info->vsel_sleep_msk) - 1;
		ret = pmic_clrsetbits(dev->parent, info->vsel_sleep_reg, info->vsel_sleep_msk, sel);
	}

	return 1;
}

static int ldo_get_suspend_value(struct udevice *dev)
{

	int buck = dev->driver_data - 1;
	const struct pm8xx_buck_desc *info = get_ldo_reg(dev->parent, buck);
	int mask = info->vsel_sleep_msk;
	int ret;
	unsigned int val;

	if (info == NULL)
		return -ENOSYS;

	ret = pmic_reg_read(dev->parent, info->vsel_sleep_reg);
	if (ret < 0)
		return ret;
	val = ret & mask;

	val >>= ffs(mask) - 1;

	return regulator_desc_list_voltage_linear_range(info, val);
}

static int ldo_get_enable(struct udevice *dev)
{
	int ret;
	unsigned int val;
	int buck = dev->driver_data - 1;
	const struct pm8xx_buck_desc *info = get_ldo_reg(dev->parent, buck);
	int mask = info->enable_msk;

	if (info == NULL)
		return -ENOSYS;

	ret = pmic_reg_read(dev->parent, info->enable_reg);
	if (ret < 0)
		return ret;

	val = ret & mask;

	val >>= ffs(mask) - 1;

	return val;
}

static int ldo_set_enable(struct udevice *dev, bool enable)
{
	int ret;
	unsigned int val = 0;
	int buck = dev->driver_data - 1;
	const struct pm8xx_buck_desc *info = get_ldo_reg(dev->parent, buck);
	int mask = info->enable_msk;

	ret = pmic_reg_read(dev->parent, info->enable_reg);
	if (ret < 0)
		return ret;

	val = (unsigned int)ret;
	val &= mask;
	val >>= ffs(mask) - 1;

	if (enable == val)
		return 0;

	val = enable << (ffs(mask) - 1);

	ret = pmic_clrsetbits(dev->parent, info->enable_reg, info->enable_msk, val);
	if (ret < 0)
		return ret;

	return 0;
}

static int ldo_set_suspend_enable(struct udevice *dev, bool enable)
{
	/* TODO */
	return 0;
}

static int ldo_get_suspend_enable(struct udevice *dev)
{
	/* TODO */
	return 0;
}

static const struct dm_regulator_ops pm8xx_ldo_ops = {
	.get_value  = ldo_get_value,
	.set_value  = ldo_set_value,
	.set_suspend_value = ldo_set_suspend_value,
	.get_suspend_value = ldo_get_suspend_value,
	.get_enable = ldo_get_enable,
	.set_enable = ldo_set_enable,
	.set_suspend_enable = ldo_set_suspend_enable,
	.get_suspend_enable = ldo_get_suspend_enable,
};

static int pm8xx_ldo_probe(struct udevice *dev)
{
	struct dm_regulator_uclass_plat *uc_pdata;

	uc_pdata = dev_get_uclass_plat(dev);

	uc_pdata->type = REGULATOR_TYPE_LDO;
	uc_pdata->mode_count = 0;

	return 0;
}

U_BOOT_DRIVER(pm8xx_ldo) = {
	.name = "pm8xx_ldo",
	.id = UCLASS_REGULATOR,
	.ops = &pm8xx_ldo_ops,
	.probe = pm8xx_ldo_probe,
};

static const struct pm8xx_buck_desc *get_switch_reg(struct udevice *pmic, int num)
{
	struct pm8xx_priv *priv = dev_get_priv(pmic);
	struct regulator_match_data *math = (struct regulator_match_data *)priv->match;

	return math->switch_desc + num;

	return NULL;
}

static int switch_get_value(struct udevice *dev)
{
	int ret;
	unsigned int val;
	int buck = dev->driver_data - 1;
	const struct pm8xx_buck_desc *info = get_switch_reg(dev->parent, buck);
	int mask = info->enable_msk;

	if (info == NULL)
		return -ENOSYS;

	ret = pmic_reg_read(dev->parent, info->enable_reg);
	if (ret < 0)
		return ret;

	val = ret & mask;

	val >>= ffs(mask) - 1;

	return val;
}

static int switch_set_value(struct udevice *dev, int uvolt)
{
	int ret;
	unsigned int val = 0;
	int buck = dev->driver_data - 1;
	const struct pm8xx_buck_desc *info = get_switch_reg(dev->parent, buck);
	int mask = info->enable_msk;

	ret = pmic_reg_read(dev->parent, info->enable_reg);
	if (ret < 0)
		return ret;

	val = (unsigned int)ret;
	val &= mask;
	val >>= ffs(mask) - 1;

	if (uvolt == val)
		return 0;

	val = uvolt << (ffs(mask) - 1);

	ret = pmic_clrsetbits(dev->parent, info->enable_reg, info->enable_msk, val);
	if (ret < 0)
		return ret;

	return 0;
}

static int switch_get_enable(struct udevice *dev)
{
	int ret;
	unsigned int val;
	int buck = dev->driver_data - 1;
	const struct pm8xx_buck_desc *info = get_switch_reg(dev->parent, buck);
	int mask = info->enable_msk;

	if (info == NULL)
		return -ENOSYS;

	ret = pmic_reg_read(dev->parent, info->enable_reg);
	if (ret < 0)
		return ret;

	val = ret & mask;

	val >>= ffs(mask) - 1;

	return val;
}

static int switch_set_enable(struct udevice *dev, bool enable)
{
	int ret;
	unsigned int val = 0;
	int buck = dev->driver_data - 1;
	const struct pm8xx_buck_desc *info = get_switch_reg(dev->parent, buck);
	int mask = info->enable_msk;

	ret = pmic_reg_read(dev->parent, info->enable_reg);
	if (ret < 0)
		return ret;

	val = (unsigned int)ret;
	val &= mask;
	val >>= ffs(mask) - 1;

	if (enable == val)
		return 0;

	val = enable << (ffs(mask) - 1);

	ret = pmic_clrsetbits(dev->parent, info->enable_reg, info->enable_msk, val);
	if (ret < 0)
		return ret;

	return 0;

}

static int switch_set_suspend_enable(struct udevice *dev, bool enable)
{
	/* TODO */
	return 0;
}

static int switch_get_suspend_enable(struct udevice *dev)
{
	/* TODO */
	return 0;
}

static int switch_set_suspend_value(struct udevice *dev, int uvolt)
{
	return 0;
}

static int switch_get_suspend_value(struct udevice *dev)
{
	return 0;
}

static const struct dm_regulator_ops pm8xx_switch_ops = {
	.get_value  = switch_get_value,
	.set_value  = switch_set_value,
	.get_enable = switch_get_enable,
	.set_enable = switch_set_enable,
	.set_suspend_enable = switch_set_suspend_enable,
	.get_suspend_enable = switch_get_suspend_enable,
	.set_suspend_value = switch_set_suspend_value,
	.get_suspend_value = switch_get_suspend_value,
};

static int pm8xx_switch_probe(struct udevice *dev)
{
	struct dm_regulator_uclass_plat *uc_pdata;

	uc_pdata = dev_get_uclass_plat(dev);

	uc_pdata->type = REGULATOR_TYPE_FIXED;
	uc_pdata->mode_count = 0;

	return 0;
}

U_BOOT_DRIVER(pm8xx_switch) = {
	.name = "pm8xx_switch",
	.id = UCLASS_REGULATOR,
	.ops = &pm8xx_switch_ops,
	.probe = pm8xx_switch_probe,
};
