// SPDX-License-Identifier: GPL-2.0+

#include <asm/io.h>
#include <dm.h>
#include <errno.h>
#include <mapmem.h>
#include <regmap.h>
#include <power-domain-uclass.h>

#define MAX_REGMAP              5

#define APMU_REGMAP_INDEX       0

#define APMU_POWER_STATUS_REG   0xf0
#define PMUA_PWR_BLK_TMR_REG	(0xd42828dc)

enum pm_domain_id {
	K3_PMU_VPU_PWR_DOMAIN,
	K3_PMU_GPU_PWR_DOMAIN,
	K3_PMU_AUDIO_PWR_DOMAIN,
	K3_PMU_LCD0_PWR_DOMAIN,
	K3_PMU_LCD1_PWR_DOMAIN,
};

struct pm_domain_desc {
	int reg_pwr_ctrl;
	int bit_hw_mode;
	int bit_sleep2;
	int bit_sleep1;
	int bit_isolation;
	int bit_auto_pwr_on;
	int bit_hw_pwr_stat;
	int bit_pwr_stat;
	int use_hw;
	int pm_index;
};

struct spacemit_k3_pd_platdata {
	struct regmap *regmap[MAX_REGMAP];
	struct pm_domain_desc *desc;
};

static struct pm_domain_desc k3_pm_domain_desc[] = {
	[K3_PMU_VPU_PWR_DOMAIN] = {
		.reg_pwr_ctrl = 0xa8,
		.bit_sleep2 = 3,
		.bit_sleep1 = 2,
		.bit_isolation = 1,
		.bit_pwr_stat = 1,
		.bit_hw_pwr_stat = 9,
		.pm_index = K3_PMU_VPU_PWR_DOMAIN,
	},

	[K3_PMU_GPU_PWR_DOMAIN] = {
		.reg_pwr_ctrl = 0xd0,
		.bit_sleep2 = 3,
		.bit_sleep1 = 2,
		.bit_isolation = 1,
		.bit_pwr_stat = 0,
		.pm_index = K3_PMU_GPU_PWR_DOMAIN,
	},

	[K3_PMU_AUDIO_PWR_DOMAIN] = {
		.reg_pwr_ctrl = 0x378,
		.bit_hw_mode = 4,
		.bit_sleep2 = 3,
		.bit_sleep1 = 2,
		.bit_isolation = 1,
		.bit_auto_pwr_on = 0,
		.bit_pwr_stat = 3,
		.pm_index = K3_PMU_AUDIO_PWR_DOMAIN,
	},

	[K3_PMU_LCD0_PWR_DOMAIN] = {
		.reg_pwr_ctrl = 0x380,
		.bit_hw_mode = 4,
		.bit_sleep2 = 3,
		.bit_sleep1 = 2,
		.bit_isolation = 1,
		.bit_auto_pwr_on = 0,
		.bit_pwr_stat = 4,
		.bit_hw_pwr_stat = 12,
		.use_hw = 1,
		.pm_index = K3_PMU_LCD0_PWR_DOMAIN,
	},

	[K3_PMU_LCD1_PWR_DOMAIN] = {
		.reg_pwr_ctrl = 0x3f4,
		.bit_hw_mode = 4,
		.bit_sleep2 = 3,
		.bit_sleep1 = 2,
		.bit_isolation = 1,
		.bit_auto_pwr_on = 0,
		.bit_pwr_stat = 5,
		.bit_hw_pwr_stat = 15,
		.use_hw = 1,
		.pm_index = K3_PMU_LCD1_PWR_DOMAIN,
	},
};

static const struct udevice_id spacemit_power_domain_of_match[] = {
	{ .compatible = "spacemit,k3-pm-domain", .data = (ulong)k3_pm_domain_desc, },
	{ /* sentinel */ }
};

static int spacemit_power_domain_of_xlate(struct power_domain *pd, struct ofnode_phandle_args *args)
{
	struct spacemit_k3_pd_platdata *priv = dev_get_priv(pd->dev);

	debug("%s(power_domain=%p, id=%d)\n", __func__, pd, args->args[0]);

	if (args->args_count < 1) {
		printf("Invalid args_count: %d\n", args->args_count);
		return -EINVAL;
	}

	pd->priv = (void *)(priv->desc + args->args[0]);
	pd->id = args->args[0];

	return 0;
}

static int k3_pd_power_off(struct spacemit_k3_pd_platdata *skp, struct pm_domain_desc *desc)
{
	unsigned int val;
	int loop;

	if (!desc->use_hw) {
		/* this is the sw type */
		regmap_read(skp->regmap[APMU_REGMAP_INDEX], desc->reg_pwr_ctrl, &val);
		val &= ~(1 << desc->bit_isolation);
		regmap_write(skp->regmap[APMU_REGMAP_INDEX], desc->reg_pwr_ctrl, val);

		udelay(15);

		/* mcu power off */
		regmap_read(skp->regmap[APMU_REGMAP_INDEX], desc->reg_pwr_ctrl, &val);
		val &= ~((1 << desc->bit_sleep1) | (1 << desc->bit_sleep2));
		regmap_write(skp->regmap[APMU_REGMAP_INDEX], desc->reg_pwr_ctrl, val);

		udelay(15);

		for (loop = 10000; loop >= 0; --loop) {
			regmap_read(skp->regmap[APMU_REGMAP_INDEX], APMU_POWER_STATUS_REG, &val);
			if ((val & (1 << desc->bit_pwr_stat)) == 0)
				break;
			udelay(5);
		}
	} else {
		/* LCD */
		regmap_read(skp->regmap[APMU_REGMAP_INDEX], desc->reg_pwr_ctrl, &val);
		val &= ~(1 << desc->bit_auto_pwr_on);
		val &= ~(1 << desc->bit_hw_mode);
		regmap_write(skp->regmap[APMU_REGMAP_INDEX], desc->reg_pwr_ctrl, val);

		udelay(30);

		for (loop = 10000; loop >= 0; --loop) {
			regmap_read(skp->regmap[APMU_REGMAP_INDEX], APMU_POWER_STATUS_REG, &val);
			if ((val & (1 << desc->bit_hw_pwr_stat)) == 0)
				break;
			udelay(5);
		}
	}

	if (loop < 0) {
		debug("power-off domain: %d, error\n", desc->pm_index);
		return -EBUSY;
	}

	return 0;
}

static int k3_pd_power_on(struct spacemit_k3_pd_platdata *skp, struct pm_domain_desc *desc)
{
	int loop;
	unsigned int val;

	regmap_read(skp->regmap[APMU_REGMAP_INDEX], APMU_POWER_STATUS_REG, &val);
	if (val & (1 << desc->bit_pwr_stat))
		return 0;

	if (!desc->use_hw) {
		/* mcu power on */
		regmap_read(skp->regmap[APMU_REGMAP_INDEX], desc->reg_pwr_ctrl, &val);
		val |= (1 << desc->bit_sleep1);
		regmap_write(skp->regmap[APMU_REGMAP_INDEX], desc->reg_pwr_ctrl, val);

		udelay(25);

		regmap_read(skp->regmap[APMU_REGMAP_INDEX], desc->reg_pwr_ctrl, &val);
		val |= (1 << desc->bit_sleep2) | (1 << desc->bit_sleep1);
		regmap_write(skp->regmap[APMU_REGMAP_INDEX], desc->reg_pwr_ctrl, val);

		udelay(25);

		/* disable isolation */
		regmap_read(skp->regmap[APMU_REGMAP_INDEX], desc->reg_pwr_ctrl, &val);
		val |= (1 << desc->bit_isolation);
		regmap_write(skp->regmap[APMU_REGMAP_INDEX], desc->reg_pwr_ctrl, val);

		udelay(15);

		for (loop = 10000; loop >= 0; --loop) {
			regmap_read(skp->regmap[APMU_REGMAP_INDEX], APMU_POWER_STATUS_REG, &val);
			if (val & (1 << desc->bit_pwr_stat))
				break;
			udelay(6);
		}
	} else {
		/* LCD */
		regmap_read(skp->regmap[APMU_REGMAP_INDEX], desc->reg_pwr_ctrl, &val);
		val |= (1 << desc->bit_auto_pwr_on);
		val |= (1 << desc->bit_hw_mode);
		regmap_write(skp->regmap[APMU_REGMAP_INDEX], desc->reg_pwr_ctrl, val);

		udelay(310);

		for (loop = 10000; loop >= 0; --loop) {
			regmap_read(skp->regmap[APMU_REGMAP_INDEX], APMU_POWER_STATUS_REG, &val);
			if (val & (1 << desc->bit_hw_pwr_stat))
				break;
			udelay(6);
		}
	}

	if (loop < 0) {
		pr_err("power-off domain: %d, error\n", desc->pm_index);
		return -EBUSY;
	}

	return 0;
}

static int spacemit_power_domain_on(struct power_domain *pd)
{
	struct pm_domain_desc *pd_priv = pd->priv;
	struct spacemit_k3_pd_platdata *priv = dev_get_priv(pd->dev);

	debug("%s(pd=%p, id=%lu)\n", __func__, pd, pd->id);

	/* domain_on */
	k3_pd_power_on(priv, pd_priv);

	return 0;
}

static int spacemit_power_domain_off(struct power_domain *pd)
{
	struct pm_domain_desc *pd_priv = pd->priv;
	struct spacemit_k3_pd_platdata *priv = dev_get_priv(pd->dev);

	debug("%s(pd=%p, id=%lu)\n", __func__, pd, pd->id);
	/* domain_off */
	k3_pd_power_off(priv, pd_priv);

	return 0;
}

static int spacemit_power_domain_probe(struct udevice *dev)
{
	int ret;
	struct spacemit_k3_pd_platdata *priv = dev_get_priv(dev);
	ulong driver_data = dev_get_driver_data(dev);

	priv->desc = (struct pm_domain_desc *)driver_data;
	ret = regmap_init_mem_index(dev_ofnode(dev),
			&priv->regmap[APMU_REGMAP_INDEX], 0);
	if (ret) {
		printf("%s:%d, error\n", __func__, __LINE__);
		return ret;
	}

	/* set GPU/VPU/AUDIO power-on/off time */
	/* power-on time <= 2.73ms */
	writel(0xffffffff, (unsigned int *)PMUA_PWR_BLK_TMR_REG);

	return 0;
}

static struct power_domain_ops spacemit_power_domain_ops = {
	.on = spacemit_power_domain_on,
	.off = spacemit_power_domain_off,
	.of_xlate = spacemit_power_domain_of_xlate,
};

U_BOOT_DRIVER(spacemit_pm_domains) = {
	.name = "spacemit-k3-pm-domains",
	.id = UCLASS_POWER_DOMAIN,
	.of_match = spacemit_power_domain_of_match,
	.probe = spacemit_power_domain_probe,
	.priv_auto = sizeof(struct spacemit_k3_pd_platdata),
	.ops = &spacemit_power_domain_ops,
};
