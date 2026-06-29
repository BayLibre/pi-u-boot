// SPDX-License-Identifier: GPL-2.0
/*
 * phy-k3-usb3.c - Spacemit K3 combo PHY for USB3 and PCIE
 *
 * Copyright (c) 2023 Spacemit Co., Ltd.
 *
 */
#include <dm.h>
#include <dm/device-internal.h>
#include <dm/devres.h>
#include <dm/of_access.h>
#include <reset.h>
#include <clk.h>
#include <generic-phy.h>
#include <dt-bindings/phy/phy.h>
#include <dm/device_compat.h>
#include <dm/lists.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/bitfield.h>
#include <linux/iopoll.h>
#include <linux/err.h>
#include <usb.h>

#define MAX_NUM_PHY 2

#define PLL_TIMEOUT 500000 /* For PHY PLL lock (usec) */
#define PU_CAL_TIMEOUT 2000000
#define POLL_DELAY 500 /* Time between polls (usec) */

/* Selecting the combo PHY operating mode requires APMU regmap access */
#define SYSCON_APMU "spacemit,syscon-apmu"

/*
 * The PCIe/USB Subsystem on SpacemiT K3 have 3 single lane PIPE3 PHYs
 * (PHY2/3/4) shared by PCIE PortC/D and USB3 PortB/C/D.
 *
 * PMUA_PCIE_SUBSYS_MGMT[4:0]
 *
 *   bit4 = 0 : PCIe A X8 mode, all 8 lanes dedicated to PCIe Port A
 *          1 : PHY lanes shared between PCIe or USB according to [3:0]
 *
 * All PHY matrix combinations according to [4:0]:
 * bit[3] - PHY1: 0=PCIe A X4,    1=PCIe A X2 + PCIe B X2
 * bit[2] - PHY2: 0=PCIe C lane0, 1=USB B
 * bit[1] - PHY3: 0=PCIe C lane1, 1=USB C
 * bit[0] - PHY4: 0=PCIe D      , 1=USB D
 *
 * So any USB Port B/C/D operation requires PCIe A X8 mode to be disabled.
 */
#define PMUA_PCIE_SUBSYS_MGMT 0x1d8
#define PU_MATRIX_CONF_X8_DISABLE BIT(4)
#define PU_MATRIX_CONF_USB_MASK GENMASK(2, 0)

#define PMUA_TYPEC_CTRL 0x110
#define TYPEC_ORIENT_FLIP BIT(2)
#define TYPEC_ORIENT_OVRD_EN BIT(3)
#define TYPEC_ORIENT_OVRD BIT(4)
#define TYPEC_ORIENT_OVRD_MASK (TYPEC_ORIENT_OVRD_EN | TYPEC_ORIENT_OVRD)

/* PHY rcal init requires APB_SPARE regmap access */
#define SYSCON_APB_SPARE "spacemit,syscon-apb-spare"

#define APB_SPARE_PU_CAL 0x178
#define PU_CAL BIT(17)

#define APB_SPARE_RCAL_HSIO 0x17c
#define PU_CAL_DONE BIT(8)
#define R_CAL_OVRD_STABLE_EN BIT(31)
#define R_CAL_OVRD_STABLE_VAL BIT(30)
#define R_CAL_OVRD_NTRIM_EN BIT(29)
#define R_CAL_OVRD_PTRIM_EN BIT(28)
#define R_CAL_OVRD_TRIM_EN (R_CAL_OVRD_NTRIM_EN | R_CAL_OVRD_PTRIM_EN)
#define R_CAL_OVRD_NTRIM_MASK GENMASK(27, 24)
#define R_CAL_OVRD_NTRIM_VAL(val) FIELD_PREP(R_CAL_OVRD_NTRIM_MASK, val)
#define NTRIM_DEFAULT 0x6
#define R_CAL_OVRD_PTRIM_MASK GENMASK(23, 20)
#define R_CAL_OVRD_PTRIM_VAL(val) FIELD_PREP(R_CAL_OVRD_PTRIM_MASK, val)
#define PTRIM_DEFAULT 0xa
#define R_CAL_OVRD_TRIM_MASK (R_CAL_OVRD_NTRIM_MASK | R_CAL_OVRD_PTRIM_MASK)

/* PHY Registers */
#define PHY_VERSION 0x0

#define PHY_RESET_CFG 0x04
#define EN_SAMPLE_DATA_AFTER_LOCK BIT(6)

#define PHY_CLK_CFG 0x08
#define PLL_READY BIT(0)
#define CFG_RXCLK_EN BIT(3)
#define CFG_TXCLK_EN BIT(4)
#define CFG_PCLK_EN BIT(5)
#define CFG_PIPE_PCLK_EN BIT(6)
#define CFG_REFCLK_FREQ GENMASK(10, 7)
#define REFCLK_24M 0x2
#define CFG_SW_INIT_DONE BIT(11)
#define CFG_PU_SSC_OUT BIT(23)

#define PHY_MODE_CFG 0x0C
#define CFG_LFPS_TPERIOD GENMASK(9, 8)
#define LFPS_TPERIOD_USB 0x3

#define PHY_PU_SEL 0x40
#define OVRD_STATUS BIT(10)
#define CFG_STATUS BIT(9)

#define PHY_PU_CK_REG 0x54
#define PU_REFCLK_100 BIT(25)

#define PHY_PLL_REG1 0x58
#define FREF_SEL GENMASK(15, 13)
#define FREF_24M 0x1
#define SSC_DEP_SEL GENMASK(27, 24)
#define SSC_5000PPM 0xa
#define SSC_MODE GENMASK(29, 28)
#define SSC_CENTER_SPREAD 0x0
#define SSC_UP_SPREAD 0x1
#define SSC_DOWN_SPREAD 0x2
#define SSC_DOWN_SPREAD1 0x3

#define PHY_PLL_REG2 0x5c
#define SEL_REF100 BIT(21)

/* PHY RX Register Definitions */
#define PHY_RX_REG_A 0x60
#define RX_REG3_MASK GENMASK(31, 24)
#define RX_REG3_RDEG1(n) FIELD_PREP(GENMASK(31, 30), (n))
#define RX_REG3_RDEG1_DEFAULT 0x3
#define RX_REG3_ADJ_BIAS(n) FIELD_PREP(GENMASK(29, 28), (n))
#define RX_REG3_ADJ_BIAS_DEFAULT 0x1
#define RX_REG3_SEL_CBOOST_CODE BIT(27)
#define RX_REG3_I_LOAD_REG(n) FIELD_PREP(GENMASK(26, 24), (n))
#define RX_REG3_I_LOAD_REG_DEFAULT 0x7
#define RX_REG2_MASK GENMASK(23, 16)
#define RX_REG2_PSEL(n) FIELD_PREP(GENMASK(23, 21), (n))
#define RX_REG2_PSEL_DEFAULT 0x4
#define RX_REG2_FORCE_CSEL BIT(20)
#define RX_REG2_CSEL(n) FIELD_PREP(GENMASK(19, 16), (n))
#define RX_REG2_CSEL_DEFAULT 0x8
#define RX_REG1_MASK GENMASK(15, 8)
#define RX_REG1_RC_CALI_REG(n) FIELD_PREP(GENMASK(15, 12), (n))
#define RX_REG1_RC_CALI_REG_DEFAULT 0x7
#define RX_REG1_RTERM_REG(n) FIELD_PREP(GENMASK(11, 8), (n))
#define RX_REG1_RTERM_REG_DEFAULT 0x8
#define RX_REG0_MASK GENMASK(7, 0)
#define RX_REG0_RLOAD BIT(4)

#define PHY_RX_REG_B 0x64
#define RX_REG6_MASK GENMASK(23, 16)
#define RX_REG6_BYPASS_ADPT BIT(22)
#define RX_REG6_ADAPT_GAIN(n) FIELD_PREP(GENMASK(21, 20), (n))
#define RX_REG6_ADAPT_GAIN_DEFAULT 0x2
#define RX_REG6_H1_REG(n) FIELD_PREP(GENMASK(19, 16), (n))
#define RX_REG6_H1_REG_DEFAULT 0x8
#define RX_REG5_MASK GENMASK(15, 8)
#define RX_REG5_RCELL_BIAS(n) FIELD_PREP(GENMASK(15, 12), (n))
#define RX_REG5_RCELL_BIAS_DEFAULT 0x8
#define RX_REG5_RCELL_VCM(n) FIELD_PREP(GENMASK(11, 8), (n))
#define RX_REG5_RCELL_VCM_DEFAULT 0x8
#define RX_REG4_MASK GENMASK(7, 0)
#define RX_REG4_MANUAL_CFG BIT(7)
#define RX_REG4_RTERM_SEL BIT(5)
#define RX_REG4_ENVOS BIT(4)
#define RX_REG4_RDEG2(n) FIELD_PREP(GENMASK(2, 1), (n))
#define RX_REG4_RDEG2_DEFAULT 0x2

#define PHY_RXEQ_TIME 0xb4
#define RXEQ_TIME_OVRD_AMP_SOC BIT(24)
#define RXEQ_TIME_CFG_AMP_SOC_MASK GENMASK(23, 22)
#define RXEQ_TIME_CFG_AMP_SOC(n) FIELD_PREP(GENMASK(23, 22), (n))
#define AMP_SOC_650M 0x0
#define AMP_SOC_800M 0x1
#define AMP_SOC_870M 0x2
#define AMP_SOC_900M 0x3
#define OVRD_POST_C_SOC BIT(21)
#define CFG_POST_C_SOC(n) FIELD_PREP(GENMASK(20, 19), (n))
#define OVRD_PRE_C_SOC BIT(18)
#define CFG_PRE_C_SOC(n) FIELD_PREP(GENMASK(17, 16), (n))
#define CFG_RXEQ_TIMEOUT(n) FIELD_PREP(GENMASK(15, 0), (n))

#define PHY_ADPT_CFG0 0x140
#define AFE_ADPT_RST_OVRD_EN BIT(1)
#define AFE_ADPT_RST_OVRD_VAL BIT(4)

struct k3_usb3phy {
	struct udevice *dev;
	struct phy *phy;
	/* dual phy for orentation switch */
	void __iomem *bases[MAX_NUM_PHY];

	bool is_combo;
	u32 combo_sel_bit;

	/* MMIO regmap (no errors) */
	void __iomem *pmu;
	void __iomem *apb_spare;

	/* For USB only */
	bool nop;
	bool orientation_flip;
	struct udevice *sw;
};

/* We need these helpers so that kernel codes could be used directly */
static inline int k3_test_bits(void __iomem *base, unsigned int reg,
			       unsigned int bits)
{
	unsigned int val;

	val = readl(base + reg);
	return (val & bits) == bits;
}

static inline int k3_read(void __iomem *base, unsigned int reg, u32 *val)
{
	*val = readl(base + reg);
	return 0;
}

static inline int k3_write(void __iomem *base, unsigned int reg, u32 val)
{
	writel(val, base + reg);
	return 0;
}

static inline int k3_update_bits(void __iomem *base, unsigned int reg, u32 mask,
				 u32 val)
{
	u32 temp;

	temp = readl(base + reg);
	temp &= ~(mask);
	temp |= mask & val;
	writel(temp, base + reg);
	return 0;
}

static void k3_usb3phy_combo_set_usb(struct k3_usb3phy *k3_phy, bool usb)
{
	void __iomem *pmu = k3_phy->pmu;
	u32 combo_mode_mask = BIT(k3_phy->combo_sel_bit);
	u32 combo_mode_val = usb << k3_phy->combo_sel_bit;

	combo_mode_mask |= PU_MATRIX_CONF_X8_DISABLE;
	combo_mode_val |= usb ? PU_MATRIX_CONF_X8_DISABLE : 0;

	if (k3_phy->is_combo &&
	    !k3_test_bits(pmu, PMUA_PCIE_SUBSYS_MGMT, combo_mode_val) == usb) {
		k3_update_bits(pmu, PMUA_PCIE_SUBSYS_MGMT, combo_mode_mask,
			       combo_mode_val);
		dev_info(k3_phy->dev, "Update Combo Mode %d to %s Mode\n",
			 combo_mode_val, usb ? "USB" : "PCIE");
	}
}

static void k3_usb3phy_update_status(struct k3_usb3phy *k3_phy)
{
	int ret;
	void __iomem *base;

	for (unsigned int i = 0; i < MAX_NUM_PHY; ++i) {
		base = k3_phy->bases[i];
		if (!base)
			break;
		ret = k3_update_bits(base, PHY_PU_SEL, CFG_STATUS | OVRD_STATUS,
				     OVRD_STATUS);
		if (ret != 0) {
			pr_err("regmap update PHY_PU_SEL failed, ret=%d\n", ret);
			return;
		}
	}
	udelay(200);
}

static int k3_usb3phy_init_single(struct k3_usb3phy *k3_phy, void __iomem *base)
{
	void __iomem *apb_spare = k3_phy->apb_spare;
	int ret;
	u32 version, reg;

	ret = k3_read(base, PHY_VERSION, &version);
	if (ret)
		return ret;

	k3_update_bits(apb_spare, APB_SPARE_PU_CAL, PU_CAL, PU_CAL);

	ret = readl_poll_sleep_timeout(apb_spare + APB_SPARE_RCAL_HSIO, reg,
				       (reg & PU_CAL_DONE),
				       POLL_DELAY, PU_CAL_TIMEOUT);
	if (ret) {
		dev_warn(k3_phy->dev, "PU PHY RCAL timeout, trim override\n");

		k3_update_bits(apb_spare, APB_SPARE_RCAL_HSIO,
			       R_CAL_OVRD_TRIM_EN | R_CAL_OVRD_STABLE_VAL |
			       R_CAL_OVRD_TRIM_MASK,
			       R_CAL_OVRD_TRIM_EN | R_CAL_OVRD_STABLE_VAL |
			       R_CAL_OVRD_NTRIM_VAL(NTRIM_DEFAULT) |
			       R_CAL_OVRD_PTRIM_VAL(PTRIM_DEFAULT));
		k3_update_bits(apb_spare, APB_SPARE_RCAL_HSIO,
			       R_CAL_OVRD_STABLE_EN, R_CAL_OVRD_STABLE_EN);
	}

	/* Do not wait CDR lock before sampling data */
	k3_update_bits(base, PHY_RESET_CFG, EN_SAMPLE_DATA_AFTER_LOCK, 0);

	/* Power down 100MHz refclk buffer */
	k3_update_bits(base, PHY_PU_CK_REG, PU_REFCLK_100, 0);

	/* Program PLL REG1 configure the SSC */
	k3_write(base, PHY_PLL_REG1,
		 FIELD_PREP(SSC_MODE, SSC_DOWN_SPREAD1) |
		 FIELD_PREP(SSC_DEP_SEL, SSC_5000PPM) |
		 FIELD_PREP(FREF_SEL, FREF_24M));

	/* Un-select 100MHz PLL reference */
	k3_update_bits(base, PHY_PLL_REG2, SEL_REF100, 0);

	/* USB LFPS period configuration */
	k3_update_bits(base, PHY_MODE_CFG, CFG_LFPS_TPERIOD,
		       FIELD_PREP(CFG_LFPS_TPERIOD, LFPS_TPERIOD_USB));

	/* Force AFE adaptation reset */
	k3_update_bits(base, PHY_ADPT_CFG0,
		       AFE_ADPT_RST_OVRD_EN | AFE_ADPT_RST_OVRD_VAL,
		       AFE_ADPT_RST_OVRD_EN | AFE_ADPT_RST_OVRD_VAL);

	/* Override driver amplitude value to 900m */
	k3_update_bits(base, PHY_RXEQ_TIME,
		       RXEQ_TIME_OVRD_AMP_SOC | RXEQ_TIME_CFG_AMP_SOC_MASK,
		       RXEQ_TIME_OVRD_AMP_SOC |
		       RXEQ_TIME_CFG_AMP_SOC(AMP_SOC_900M));

	/* Configure RX parameters */
	k3_update_bits(base, PHY_RX_REG_A, RX_REG0_MASK, RX_REG0_RLOAD);
	k3_update_bits(base, PHY_RX_REG_A, RX_REG1_MASK,
		       RX_REG1_RC_CALI_REG(RX_REG1_RC_CALI_REG_DEFAULT) |
		       RX_REG1_RTERM_REG(RX_REG1_RTERM_REG_DEFAULT));
	k3_update_bits(base, PHY_RX_REG_A, RX_REG2_MASK,
		       RX_REG2_PSEL(RX_REG2_PSEL_DEFAULT) | RX_REG2_FORCE_CSEL |
		       RX_REG2_CSEL(RX_REG2_CSEL_DEFAULT));
	k3_update_bits(base, PHY_RX_REG_A, RX_REG3_MASK,
		       RX_REG3_RDEG1(RX_REG3_RDEG1_DEFAULT) |
		       RX_REG3_ADJ_BIAS(RX_REG3_ADJ_BIAS_DEFAULT) |
		       RX_REG3_SEL_CBOOST_CODE |
		       RX_REG3_I_LOAD_REG(RX_REG3_I_LOAD_REG_DEFAULT));
	k3_update_bits(base, PHY_RX_REG_B, RX_REG4_MASK,
		       RX_REG4_MANUAL_CFG | RX_REG4_RTERM_SEL | RX_REG4_ENVOS |
		       RX_REG4_RDEG2(RX_REG4_RDEG2_DEFAULT));
	k3_update_bits(base, PHY_RX_REG_B, RX_REG5_MASK,
		       RX_REG5_RCELL_BIAS(RX_REG5_RCELL_BIAS_DEFAULT) |
		       RX_REG5_RCELL_VCM(RX_REG5_RCELL_VCM_DEFAULT));
	k3_update_bits(base, PHY_RX_REG_B, RX_REG6_MASK,
		       RX_REG6_ADAPT_GAIN(RX_REG6_ADAPT_GAIN_DEFAULT) |
		       RX_REG6_H1_REG(RX_REG6_H1_REG_DEFAULT));
	dev_info(k3_phy->dev, "PUPHY Rx Reg Configured\n");

	/*
	 * Inform PHY that all PLL-related configuration is done.
	 * PLL will not start locking until CFG_SW_INIT_DONE is set.
	 */
	k3_write(base, PHY_CLK_CFG,
		 CFG_SW_INIT_DONE | CFG_PU_SSC_OUT |
			 FIELD_PREP(CFG_REFCLK_FREQ, REFCLK_24M) |
			 CFG_RXCLK_EN | CFG_PCLK_EN | CFG_PIPE_PCLK_EN |
			 CFG_TXCLK_EN);

	ret = readl_poll_sleep_timeout(base + PHY_CLK_CFG, reg,
				 (reg & PLL_READY) == PLL_READY,
				 POLL_DELAY, PLL_TIMEOUT);
	if (ret)
		return -ETIMEDOUT;

	dev_info(k3_phy->dev, "PHY version: 0x%x init as USB3 mode\n", version);

	return 0;
}

static int k3_usb3phy_init(struct phy *phy)
{
	struct k3_usb3phy *k3_phy = dev_get_priv(phy->dev);
	int ret = 0;

	if (k3_phy->nop) {
		dev_info(phy->dev,
			 "maximum high-speed configuration requested\n");
		return 0;
	}

	k3_usb3phy_combo_set_usb(k3_phy, true);

	ret = k3_usb3phy_init_single(k3_phy, k3_phy->bases[0]);
	if (ret)
		return ret;
	if (k3_phy->bases[1])
		ret = k3_usb3phy_init_single(k3_phy, k3_phy->bases[1]);

	return ret;
}

static int k3_usb3phy_exit(struct phy *phy)
{
	return 0;
}

static int k3_usb3phy_set_speed(struct phy *phy, int speed)
{
	struct k3_usb3phy *k3_phy = dev_get_priv(phy->dev);

	switch (speed) {
	case USB_SPEED_HIGH:
		k3_usb3phy_update_status(k3_phy);
		k3_phy->nop = true;
	default:
		break;
	}
	return 0;
}

static int k3_usb3phy_probe(struct udevice *dev)
{
	struct k3_usb3phy *k3_phy = dev_get_priv(dev);
	int num_phy;

	dev_info(dev, "k3 usb3 phy enter...\n");

	k3_phy->is_combo = dev_read_bool(dev, "combo-usb-bit");
	dev_read_u32(dev, "combo-usb-bit", &k3_phy->combo_sel_bit);

	k3_phy->dev = dev;

	k3_phy->pmu = (void *)dev_read_addr_name(dev, SYSCON_APMU);
	if (IS_ERR(k3_phy->pmu)) {
		dev_err(dev, SYSCON_APMU " lookup failed\n");
		return PTR_ERR(k3_phy->pmu);
	}

	k3_phy->apb_spare = (void *)dev_read_addr_name(dev, SYSCON_APB_SPARE);
	if (IS_ERR(k3_phy->apb_spare)) {
		dev_err(dev, SYSCON_APB_SPARE " lookup failed\n");
		return PTR_ERR(k3_phy->apb_spare);
	}
	/* dual phy for orientation switch */
	num_phy = 1;
	dev_read_u32(dev, "phy-nums", &num_phy);

	for (unsigned int i = 0; i < num_phy; ++i) {
		k3_phy->bases[i] = dev_read_addr_index_ptr(dev, i);
		if (IS_ERR(k3_phy->bases[i])) {
			dev_err(dev, "phy addr %d lookup failed\n", i);
			return PTR_ERR(k3_phy->bases[i]);
		}
	}

	return 0;
}

static const struct udevice_id k3_usb3phy_ids[] = {
	{
		.compatible = "spacemit,k3-usb3-phy",
	},
	{ /* sentinel */ }
};

static struct phy_ops k3_usb3phy_ops = {
	.init = k3_usb3phy_init,
	.exit = k3_usb3phy_exit,
	.set_speed = k3_usb3phy_set_speed,
};

U_BOOT_DRIVER(k3_usb3_phy) = {
	.name = "k3-usb3-phy",
	.id = UCLASS_PHY,
	.of_match = k3_usb3phy_ids,
	.ops = &k3_usb3phy_ops,
	.probe = k3_usb3phy_probe,
	.priv_auto = sizeof(struct k3_usb3phy),
};
