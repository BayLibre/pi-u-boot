// SPDX-License-Identifier: GPL-2.0+
#include <dm.h>
#include <errno.h>
#include <fdtdec.h>
#include <log.h>
#include <remoteproc.h>
#include <reset.h>
#include <clk.h>
#include <asm/io.h>
#include <dm/device_compat.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <cpu_func.h>

#define RCPU_CORE0_BOOT_ENTRY_LO	0xc088007c
#define RCPU_CORE0_BOOT_ENTRY_HI	0xc0880080
#define RCPU_CORE0_HART_ID_SET		0xc0880084

#define RCPU_CORE1_BOOT_ENTRY_LO	0xc088008c
#define RCPU_CORE1_BOOT_ENTRY_HI	0xc0880090
#define RCPU_CORE1_HART_ID_SET		0xc0880094

#define RCPU_EXECUTION_CTRL		0xc088c030

#define RCPU_PWR_DOMAIN_STAT_REG	0xd42828f0
#define RCPU_PWR_DOMAIN_REG		0xd4282b78
#define RCPU_PWR_SLEEP1_BITOFF		2
#define RCPU_PWR_SLEEP2_BITOFF		3
#define RCPU_PWR_ISOL_BITOFF		1
#define RCPU_PWR_STAT_BITOFF		3

#define PMU_AUDIO_CLK_CTRL		(0xd428294c)
#define AUDIO_IS_SYS_RST_OFFSET		(0)
#define AUDIO_MCU_CORE_RST_OFFSET	(2)
#define AUDIO_APMU_RST_OFFSET		(3)
#define AUDIO_CLK_EN_OFFSET		(12)
#define AUIO_FORCE_PWR_ON_OFFSET	(13)
#define AUDIO_CTRL_BY_AP_OFFSET		(28)

#define RT24_CORE0_SW_WAKEUP_REG	(0xc088c0d4)
#define RT24_CORE1_SW_WAKEUP_REG	(0xc088c0d8)
#define RT24_CORE0_SW_RESET_REG		(0xc088c0cc)
#define RT24_CORE1_SW_RESET_REG		(0xc088c0d0)

struct k3_rproc_privdata {
	int coreid;
	unsigned long long fw_boot_entry;
	unsigned long long mem_heap_start;
	unsigned int mem_heap_size;
};

static int k3_rproc_load(struct udevice *dev, ulong addr, ulong size)
{
	unsigned int val;
	struct k3_rproc_privdata *priv;

	priv = dev_get_priv(dev);

	/* deassert the rcpu */
	val = readl((void __iomem *)PMU_AUDIO_CLK_CTRL);
	val |= (1 << AUDIO_APMU_RST_OFFSET);
	writel(val, (void __iomem *)PMU_AUDIO_CLK_CTRL);

	while (1) {
		val = readl((void __iomem *)PMU_AUDIO_CLK_CTRL);
		if (val & 0x800000)
			break;
	}

	val = readl((void __iomem *)PMU_AUDIO_CLK_CTRL);
	val |= (1 << AUDIO_IS_SYS_RST_OFFSET);
	writel(val, (void __iomem *)PMU_AUDIO_CLK_CTRL);

	writel(1, (void __iomem *)RCPU_EXECUTION_CTRL);

	switch (priv->coreid) {
	case 0:
		/* reset core0 sw reset */
		writel(0, (void __iomem *)RT24_CORE0_SW_RESET_REG);

		/* keep core0 sleep */
		writel(0, (void __iomem *)RT24_CORE0_SW_WAKEUP_REG);
		break;
	case 1:
		/* reset core0 sw reset */
		writel(0, (void __iomem *)RT24_CORE1_SW_RESET_REG);

		/* keep core0 sleep */
		writel(0, (void __iomem *)RT24_CORE1_SW_WAKEUP_REG);
		break;
	}

	return rproc_elf_load_image(dev, addr, size);
}

static int k3_rproc_start(struct udevice *dev)
{
	int try_count = 6000;
	struct k3_rproc_privdata *priv;

	priv = dev_get_priv(dev);

	switch (priv->coreid) {
	case 0:
		/* set the boot-entry */
		writel(priv->fw_boot_entry & 0xffffffff, (void __iomem *)RCPU_CORE0_BOOT_ENTRY_LO);
		writel((priv->fw_boot_entry >> 32) & 0xffffffff, (void __iomem *)RCPU_CORE0_BOOT_ENTRY_HI);

		/* set the hartid */
		writel(priv->coreid, (void __iomem *)RCPU_CORE0_HART_ID_SET);

		/* sync flag */
		writel(0, (void __iomem *)RCPU_CORE1_BOOT_ENTRY_LO);

		/* reset core0 sw reset */
		writel(1, (void __iomem *)RT24_CORE0_SW_RESET_REG);

		/* sw wakeup core 0 */
		writel(1, (void __iomem *)RT24_CORE0_SW_WAKEUP_REG);

		/* sync flag */
		while (try_count-- >= 0) {
			if (readl((void __iomem *)RCPU_CORE1_BOOT_ENTRY_LO) != 0)
				break;
			mdelay(1);
		}

		if (try_count <= 0)
			dev_err(dev, "try start rproc-0 failed\n");

		break;

	case 1:
		/* set the boot-entry */
		writel(priv->fw_boot_entry & 0xffffffff, (void __iomem *)RCPU_CORE1_BOOT_ENTRY_LO);
		writel((priv->fw_boot_entry >> 32) & 0xffffffff, (void __iomem *)RCPU_CORE1_BOOT_ENTRY_HI);

		/* set the hartid */
		writel(priv->coreid, (void __iomem *)RCPU_CORE1_HART_ID_SET);

		/* sync flag */
		writel(0, (void __iomem *)RCPU_CORE0_BOOT_ENTRY_LO);

		/* reset core1 sw reset */
		writel(1, (void __iomem *)RT24_CORE1_SW_RESET_REG);

		/* sw wakeup core 1 */
		writel(1, (void __iomem *)RT24_CORE1_SW_WAKEUP_REG);

		/* sync flag */
		while (try_count-- >= 0) {
			if (readl((void __iomem *)RCPU_CORE0_BOOT_ENTRY_LO) != 0)
				break;
			mdelay(1);
		}

		if (try_count <= 0)
			dev_err(dev, "try start rproc-1 failed\n");
		break;

	default:
		break;
	}

	return 0;
}

static int k3_rproc_stop(struct udevice *dev)
{
	/* TODO */
	return 0;
}

static int k3_rproc_reset(struct udevice *dev)
{
	/* TODO */
	return 0;
}

static int k3_rproc_is_running(struct udevice *dev)
{
	/* TODO */
	return 0;
}

/**
 * k3_rproc_probe() - Basic probe
 * @dev:	corresponding k3 remote processor device
 * Return: 0 if all went ok, else corresponding -ve error
 */
static int k3_rproc_probe(struct udevice *dev)
{
	unsigned int val[2];
	int ret;
	struct k3_rproc_privdata *priv;

	priv = dev_get_priv(dev);

	ret = dev_read_u32(dev, "coreid", &priv->coreid);
	if (ret) {
		dev_err(dev, "failed to get coreid (%d)\n", ret);
		return ret;
	}

	ret = dev_read_u32_index(dev, "fw_boot_entry", 0, &val[0]);
	if (ret) {
		dev_err(dev, "failed to get firmware boot entry (%d)\n", ret);
		return ret;
	}

	ret = dev_read_u32_index(dev, "fw_boot_entry", 1, &val[1]);
	if (ret) {
		dev_err(dev, "failed to get firmware boot entry (%d)\n", ret);
		return ret;
	}

	priv->fw_boot_entry = (u64)val[0] << 32 | val[1];

	ret = dev_read_u32_index(dev, "mem_heap_start", 0, &val[0]);
	if (ret) {
		dev_err(dev, "failed to get firmware heap start (%d)\n", ret);
		return ret;
	}

	ret = dev_read_u32_index(dev, "mem_heap_start", 1, &val[1]);
	if (ret) {
		dev_err(dev, "failed to get firmware heap start (%d)\n", ret);
		return ret;
	}

	priv->mem_heap_start = (u64)val[0] << 32 | val[1];

	ret = dev_read_u32_index(dev, "mem_heap_size", 0, &priv->mem_heap_size);
	if (ret) {
		dev_err(dev, "failed to get firmware heap size (%d)\n", ret);
		return ret;
	}

	memset((void *)((unsigned long long)priv->mem_heap_start), 0, priv->mem_heap_size);
	flush_cache(priv->mem_heap_start, priv->mem_heap_size);

	dev_dbg(dev, "probed\n");

	return 0;
}

static const struct dm_rproc_ops k3_rproc_ops = {
	.load = k3_rproc_load,
	.start = k3_rproc_start,
	.stop =  k3_rproc_stop,
	.reset = k3_rproc_reset,
	.is_running = k3_rproc_is_running,
};

static const struct udevice_id k3_rproc_ids[] = {
	{.compatible = "spacemit,k3-rproc"},
	{}
};

U_BOOT_DRIVER(rt24_copro) = {
	.name = "k3-rproc",
	.of_match = k3_rproc_ids,
	.id = UCLASS_REMOTEPROC,
	.ops = &k3_rproc_ops,
	.probe = k3_rproc_probe,
	.priv_auto	= sizeof(struct k3_rproc_privdata),
};
