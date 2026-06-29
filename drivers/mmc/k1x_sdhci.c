// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver for Spacemit K1x/K3 Mobile Storage Host Controller
 *
 * Copyright (C) 2025 Spacemit Inc.
 */
#include <clk.h>
#include <dm.h>
#include <dm/pinctrl.h>
#include <dm/device_compat.h>
#include <fdtdec.h>
#include <reset-uclass.h>
#include <power/regulator.h>
#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <mmc.h>
#include <sdhci.h>

DECLARE_GLOBAL_DATA_PTR;

/* SDH registers define */
#define SPACEMIT_SDHC_OP_EXT_REG	0x108
#define  SDHC_OVRRD_CLK_OEN		BIT(11)
#define  SDHC_FORCE_CLK_ON		BIT(12)

#define SPACEMIT_SDHC_MMC_CTRL_REG	0x114
#define  SDHC_MISC_INT_EN		BIT(1)
#define  SDHC_MISC_INT			BIT(2)
#define  SDHC_ENHANCE_STROBE_EN		BIT(8)
#define  SDHC_MMC_HS400			BIT(9)
#define  SDHC_MMC_HS200			BIT(10)
#define  SDHC_MMC_CARD_MODE		BIT(12)

#define SPACEMIT_SDHC_RX_CFG_REG	0x118
#define  SDHC_RX_SDCLK_SEL0		GENMASK(1, 0)
#define  SDHC_RX_SDCLK_SEL1		GENMASK(3, 2)

#define SPACEMIT_SDHC_TX_CFG_REG	0x11C
#define  SDHC_TX_INT_CLK_SEL		BIT(30)
#define  SDHC_TX_MUX_SEL		BIT(31)

#define SPACEMIT_SDHC_DLINE_CTRL_REG	0x130
#define  SDHC_DLINE_PU			BIT(0)
#define  SDHC_RX_DLINE_CODE		GENMASK(23, 16)
#define  SDHC_TX_DLINE_CODE		GENMASK(31, 24)

#define SPACEMIT_SDHC_DLINE_CFG_REG	0x134
#define  SDHC_RX_DLINE_REG		GENMASK(7, 0)
#define  SDHC_TX_DLINE_REG		GENMASK(23, 16)

#define SPACEMIT_SDHC_PHY_CTRL_REG	0x160
#define  SDHC_PHY_FUNC_EN		BIT(0)
#define  SDHC_PHY_PLL_LOCK		BIT(1)
#define  SDHC_HOST_LEGACY_MODE		BIT(31)

#define SPACEMIT_SDHC_PHY_FUNC_REG	0x164
#define  SDHC_PHY_TEST_EN		BIT(7)
#define  SDHC_HS200_USE_RFIFO		BIT(15)

#define SPACEMIT_SDHC_PHY_DLLCFG	0x168
#define  SDHC_DLL_PREDLY_NUM		GENMASK(3, 2)
#define  SDHC_DLL_FULLDLY_RANGE		GENMASK(5, 4)
#define  SDHC_DLL_VREG_CTRL		GENMASK(7, 6)
#define  SDHC_DLL_ENABLE		BIT(31)

#define SPACEMIT_SDHC_PHY_DLLCFG1	0x16C
#define  SDHC_DLL_REG1_CTRL		GENMASK(7, 0)
#define  SDHC_DLL_REG2_CTRL		GENMASK(15, 8)
#define  SDHC_DLL_REG3_CTRL		GENMASK(23, 16)
#define  SDHC_DLL_REG4_CTRL		GENMASK(31, 24)

#define SPACEMIT_SDHC_PHY_DLLSTS	0x170
#define  SDHC_DLL_LOCK_STATE		BIT(0)

#define SPACEMIT_SDHC_PHY_PADCFG_REG	0x178
#define  SDHC_PHY_DRIVE_SEL		GENMASK(2, 0)
#define  SDHC_RX_BIAS_CTRL		BIT(5)

#define SDHC_RX_TUNE_DELAY_MIN		0x0
#define SDHC_RX_TUNE_DELAY_MAX		0xFF
#define SDHC_RX_TUNE_DELAY_STEP		0x1

#define PHY_DRIVE_SEL_DEFAULT		0x4
#define RX_TUNING_WINDOW_THRESHOLD	50
#define RX_TUNING_DLINE_REG		0x00
#define TX_TUNING_DLINE_REG		0x00
#define TX_TUNING_DELAYCODE		0x7F

enum window_type {
	LEFT_WINDOW = 0,
	MIDDLE_WINDOW = 1,
	RIGHT_WINDOW = 2,
};

struct tuning_window {
	u8 min_delay;
	u8 max_delay;
};

struct rx_tuning {
	u8 tx_delaycode;
	u8 tx_dline_reg;
	u8 rx_dline_reg;

	struct tuning_window windows;
	u8 select_delay;

	u8 window_limit;
	u8 window_type;
};

struct spacemit_sdhci_plat {
	struct mmc_config cfg;
	struct mmc mmc;
};

struct spacemit_sdhci_priv {
	struct sdhci_host host;
	struct reset_ctl_bulk resets;
	struct clk_bulk clks;

	struct rx_tuning rxtuning;
	u8 phy_driver_sel;
	bool phy_module;
};

/*
 * refer to PMU_SDH0_CLK_RES_CTRL<0x054>, SDH0_CLK_SEL:0x0, SDH0_CLK_DIV:0x1
 * the default clock source is 204800000Hz [409.6MHz(pll1_d6_409p6Mhz)/2]
 *
 * in the start-up phase, use the 200KHz frequency
 */
#define SDHC_DEFAULT_MAX_CLOCK (204800000)
#define SDHC_MIN_CLOCK (200 * 1000)

#if CONFIG_IS_ENABLED(MMC_HS400_SUPPORT)
static int spacemit_sdhci_post_select_hs400(struct sdhci_host *host);
static int spacemit_sdhci_pre_hs400_downgrade(struct sdhci_host *host);
#endif

/* All helper functions will update clr/set while preserve rest bits */
static inline void spacemit_sdhci_setbits(struct sdhci_host *host, u32 val, int reg)
{
	sdhci_writel(host, sdhci_readl(host, reg) | val, reg);
}

static inline void spacemit_sdhci_clrbits(struct sdhci_host *host, u32 val, int reg)
{
	sdhci_writel(host, sdhci_readl(host, reg) & ~val, reg);
}

static inline void spacemit_sdhci_clrsetbits(struct sdhci_host *host, u32 clr, u32 set, int reg)
{
	u32 val = sdhci_readl(host, reg);

	val = (val & ~clr) | set;
	sdhci_writel(host, val, reg);
}

static void spacemit_mmc_phy_init(struct sdhci_host *host)
{
	struct udevice *dev = host->mmc->dev;
	struct spacemit_sdhci_priv *priv = dev_get_priv(dev);

	if (!priv->phy_module) {
		/* sd/sdio has no phy */
		spacemit_sdhci_setbits(host, SDHC_TX_INT_CLK_SEL, SPACEMIT_SDHC_TX_CFG_REG);
	} else {
#if !defined(CONFIG_K1_X_BOARD_FPGA) && !defined(CONFIG_K3_BOARD_FPGA)
		/* use phy func mode */
		spacemit_sdhci_setbits(host, SDHC_PHY_FUNC_EN | SDHC_PHY_PLL_LOCK,
				       SPACEMIT_SDHC_PHY_CTRL_REG);
		spacemit_sdhci_clrsetbits(host, SDHC_PHY_DRIVE_SEL,
					  SDHC_RX_BIAS_CTRL |
					  FIELD_PREP(SDHC_PHY_DRIVE_SEL, priv->phy_driver_sel),
					  SPACEMIT_SDHC_PHY_PADCFG_REG);
#else
		/* use phy bypass */
		spacemit_sdhci_setbits(host, SDHC_TX_INT_CLK_SEL, SPACEMIT_SDHC_TX_CFG_REG);
		spacemit_sdhci_setbits(host, SDHC_HOST_LEGACY_MODE, SPACEMIT_SDHC_PHY_CTRL_REG);
		spacemit_sdhci_setbits(host, SDHC_PHY_TEST_EN, SPACEMIT_SDHC_PHY_FUNC_REG);
#endif
		/* mmc card mode */
		spacemit_sdhci_setbits(host, SDHC_MMC_CARD_MODE, SPACEMIT_SDHC_MMC_CTRL_REG);
	}
	spacemit_sdhci_clrbits(host, SDHC_ENHANCE_STROBE_EN, SPACEMIT_SDHC_MMC_CTRL_REG);
}

static void spacemit_sdhci_set_voltage(struct sdhci_host *host)
{
	if (IS_ENABLED(CONFIG_MMC_IO_VOLTAGE)) {
		struct mmc *mmc = (struct mmc *)host->mmc;
		u16 ctrl;

		ctrl = sdhci_readw(host, SDHCI_HOST_CONTROL2);

		switch (mmc->signal_voltage) {
		case MMC_SIGNAL_VOLTAGE_330:
#if CONFIG_IS_ENABLED(DM_REGULATOR)
			if (mmc->vqmmc_supply) {
				if (regulator_set_value(mmc->vqmmc_supply, 3300000)) {
					pr_err("failed to set vqmmc-voltage to 3.3V\n");
					return;
				}

				if (regulator_set_enable_if_allowed(mmc->vqmmc_supply, true)) {
					pr_err("failed to enable vqmmc-supply\n");
					return;
				}
			}
#endif
			ctrl &= ~SDHCI_CTRL_VDD_180;
			sdhci_writew(host, ctrl, SDHCI_HOST_CONTROL2);

			/* Wait for 5ms */
			mdelay(5);

			/* 3.3V regulator output should be stable within 5 ms */
			ctrl = sdhci_readw(host, SDHCI_HOST_CONTROL2);
			if (ctrl & SDHCI_CTRL_VDD_180) {
				pr_err("3.3V regulator output did not become stable\n");
				return;
			}

			break;
		case MMC_SIGNAL_VOLTAGE_180:
#if CONFIG_IS_ENABLED(DM_REGULATOR)
			if (mmc->vqmmc_supply) {
				if (regulator_set_value(mmc->vqmmc_supply, 1800000)) {
					pr_err("failed to set vqmmc-voltage to 1.8V\n");
					return;
				}

				if (regulator_set_enable_if_allowed(mmc->vqmmc_supply, true)) {
					pr_err("failed to enable vqmmc-supply\n");
					return;
				}
			}
#endif
			ctrl |= SDHCI_CTRL_VDD_180;
			sdhci_writew(host, ctrl, SDHCI_HOST_CONTROL2);

			/* Wait for 5 ms */
			mdelay(5);

			/* 1.8V regulator output has to be stable within 5 ms */
			ctrl = sdhci_readw(host, SDHCI_HOST_CONTROL2);
			if (!(ctrl & SDHCI_CTRL_VDD_180)) {
				pr_err("1.8V regulator output did not become stable\n");
				return;
			}

			break;
		default:
			/* No signal voltage switch required */
			return;
		}
	}
}

static void spacemit_sdhci_set_clk_gate(struct sdhci_host *host, int auto_gate)
{
	if (auto_gate)
		spacemit_sdhci_clrbits(host, SDHC_OVRRD_CLK_OEN | SDHC_FORCE_CLK_ON,
				       SPACEMIT_SDHC_OP_EXT_REG);
	else
		spacemit_sdhci_setbits(host, SDHC_OVRRD_CLK_OEN | SDHC_FORCE_CLK_ON,
				       SPACEMIT_SDHC_OP_EXT_REG);
}

static int spacemit_sdhci_wait_dat0(struct udevice *dev, int state,
			   int timeout_us)
{
	struct mmc *mmc = mmc_get_mmc_dev(dev);
	struct sdhci_host *host = mmc->priv;
	unsigned long timeout = timer_get_us() + timeout_us;
	u32 tmp;
	u32 cmd;

	/* readx_poll_timeout is unsuitable because sdhci_readl accepts
	 * two arguments
	 */
	do {
		tmp = sdhci_readl(host, SDHCI_PRESENT_STATE);
		if (!!(tmp & SDHCI_DATA_0_LVL_MASK) == !!state) {
			if (IS_SD(mmc)) {
				cmd = SDHCI_GET_CMD(sdhci_readw(host, SDHCI_COMMAND));
				if ((cmd == SD_CMD_SWITCH_UHS18V) && (mmc->signal_voltage == MMC_SIGNAL_VOLTAGE_180)) {
					/* recover the auto clock */
					spacemit_sdhci_set_clk_gate(host, 1);
				}
			}
			return 0;
		}
	} while (!timeout_us || !time_after(timer_get_us(), timeout));

	return -ETIMEDOUT;
}

static void spacemit_sdhci_set_control_reg(struct sdhci_host *host)
{
	struct mmc *mmc = host->mmc;
	u32 cmd;

	dev_dbg(mmc->dev, "select mode: %s, io voltage: %d\n", mmc_mode_name(mmc->selected_mode),
		mmc->signal_voltage);

	spacemit_sdhci_set_voltage(host);

	/* set pinctrl state for SD card only */
#ifdef CONFIG_PINCTRL
	if (IS_SD(mmc)) {
		if (mmc->signal_voltage == MMC_SIGNAL_VOLTAGE_330) {
			if (mmc->bus_width < 4) {
				pinctrl_select_state(mmc->dev, "debug");
			} else {
				pinctrl_select_state(mmc->dev, "default");
			}
		} else if (mmc->signal_voltage == MMC_SIGNAL_VOLTAGE_180) {
			pinctrl_select_state(mmc->dev, "uhs");
		}
	}
#endif

	if (IS_SD(mmc)) {
		cmd = SDHCI_GET_CMD(sdhci_readw(host, SDHCI_COMMAND));
		if ((cmd == SD_CMD_SWITCH_UHS18V) && (mmc->signal_voltage == MMC_SIGNAL_VOLTAGE_180)) {
			/* disable auto clock */
			spacemit_sdhci_set_clk_gate(host, 0);
		}
	}

#if CONFIG_IS_ENABLED(MMC_HS400_SUPPORT)
	if (mmc->selected_mode == MMC_HS)
		spacemit_sdhci_pre_hs400_downgrade(host);
#endif
	/* according to the SDHC_TX_CFG_REG(0x11c<bit>),
	 * set TX_INT_CLK_SEL to gurantee the hold time
	 * at default speed mode or HS/SDR12/SDR25/SDR50 mode.
	 */
	if (mmc->selected_mode <= UHS_SDR50)
		spacemit_sdhci_setbits(host, SDHC_TX_INT_CLK_SEL, SPACEMIT_SDHC_TX_CFG_REG);
	else
		spacemit_sdhci_clrbits(host, SDHC_TX_INT_CLK_SEL, SPACEMIT_SDHC_TX_CFG_REG);

	if (mmc->selected_mode >= MMC_HS_200) {
		spacemit_sdhci_setbits(host, (mmc->selected_mode == MMC_HS_200) ? SDHC_MMC_HS200 : SDHC_MMC_HS400,
				       SPACEMIT_SDHC_MMC_CTRL_REG);
	} else {
		spacemit_sdhci_clrbits(host, SDHC_MMC_HS400 | SDHC_MMC_HS200 | SDHC_ENHANCE_STROBE_EN,
				       SPACEMIT_SDHC_MMC_CTRL_REG);
	}

	sdhci_set_uhs_timing(host);
}

static int spacemit_sdhci_set_ios_post(struct sdhci_host *host)
{
#if CONFIG_IS_ENABLED(MMC_HS400_SUPPORT)
	if (host->mmc->selected_mode == MMC_HS_400)
		return spacemit_sdhci_post_select_hs400(host);
#endif

	return 0;
}

#if CONFIG_IS_ENABLED(MMC_SUPPORTS_TUNING)
static void spacemit_sw_rx_tuning_prepare(struct sdhci_host *host, u8 dline_reg)
{
	struct mmc *mmc = host->mmc;

	spacemit_sdhci_clrsetbits(host, SDHC_RX_DLINE_REG,
				  FIELD_PREP(SDHC_RX_DLINE_REG, dline_reg),
				  SPACEMIT_SDHC_DLINE_CFG_REG);

	spacemit_sdhci_setbits(host, SDHC_DLINE_PU, SPACEMIT_SDHC_DLINE_CTRL_REG);
	udelay(5);
	spacemit_sdhci_clrsetbits(host, SDHC_RX_SDCLK_SEL1,
				  FIELD_PREP(SDHC_RX_SDCLK_SEL1, 1),
				  SPACEMIT_SDHC_RX_CFG_REG);

	if (mmc->selected_mode == MMC_HS_200)
		spacemit_sdhci_setbits(host, SDHC_HS200_USE_RFIFO, SPACEMIT_SDHC_PHY_FUNC_REG);
}

static void spacemit_sw_rx_set_delaycode(struct sdhci_host *host, u32 delay)
{
	spacemit_sdhci_clrsetbits(host, SDHC_RX_DLINE_CODE,
				  FIELD_PREP(SDHC_RX_DLINE_CODE, delay),
				  SPACEMIT_SDHC_DLINE_CTRL_REG);
}

static void spacemit_sw_tx_tuning_prepare(struct sdhci_host *host)
{
	struct spacemit_sdhci_priv *priv = dev_get_priv(host->mmc->dev);
	struct rx_tuning *rxtuning = &priv->rxtuning;

	/* set TX_DLINE_REG */
	spacemit_sdhci_clrsetbits(host, SDHC_TX_DLINE_REG,
				  FIELD_PREP(SDHC_TX_DLINE_REG, rxtuning->tx_dline_reg),
				  SPACEMIT_SDHC_DLINE_CFG_REG);
	/* set TX_DLINE_CODE */
	spacemit_sdhci_clrsetbits(host, SDHC_TX_DLINE_CODE,
				  FIELD_PREP(SDHC_TX_DLINE_CODE, rxtuning->tx_delaycode),
				  SPACEMIT_SDHC_DLINE_CTRL_REG);
	/* set SDHC_TX_INT_CLK_SEL */
	spacemit_sdhci_setbits(host, SDHC_TX_INT_CLK_SEL, SPACEMIT_SDHC_TX_CFG_REG);
	spacemit_sdhci_setbits(host, SDHC_DLINE_PU, SPACEMIT_SDHC_DLINE_CTRL_REG);
}

static int spacemit_sw_rx_select_window(struct sdhci_host *host, u32 opcode)
{
	struct mmc *mmc = host->mmc;
	struct spacemit_sdhci_priv *priv = dev_get_priv(mmc->dev);
	struct rx_tuning *rxtuning = &priv->rxtuning;
	struct tuning_window *window = &rxtuning->windows;
	int min = 0, max = 0, start, ret;
	int cur_windows = 0;
	int max_windows = 0;

	start = SDHC_RX_TUNE_DELAY_MIN;
	while (start <= SDHC_RX_TUNE_DELAY_MAX) {
		spacemit_sw_rx_set_delaycode(host, start);
		ret = mmc_send_tuning(mmc, opcode);
		if (ret) {
			if (cur_windows)
				dev_info(mmc->dev, "pass window [%d %d)\n", min, start);
			cur_windows = 0;
		} else {
			if (!cur_windows)
				min = start;

			cur_windows++;
			if (cur_windows > max_windows) {
				max_windows = cur_windows;
				max = start;
			}
			if (start == SDHC_RX_TUNE_DELAY_MAX)
				dev_info(mmc->dev, "pass window [%d %d]\n", min, start);
		}
		start += SDHC_RX_TUNE_DELAY_STEP;
	}

	if (max_windows < rxtuning->window_limit) {
		dev_warn(mmc->dev, "fail to find valid tuning window, max_windows:%d\n", max_windows);
		return -EIO;
	}

	window->min_delay = max - max_windows + 1;
	window->max_delay = max;

	if (rxtuning->window_type == LEFT_WINDOW)
		rxtuning->select_delay = window->min_delay + max_windows / 3;
	else if (rxtuning->window_type == RIGHT_WINDOW)
		rxtuning->select_delay = window->min_delay + max_windows * 2 / 3;
	else
		rxtuning->select_delay = window->min_delay + max_windows / 2;

	return 0;
}

static int spacemit_sdhci_execute_tuning(struct mmc *mmc, u8 opcode)
{
	struct sdhci_host *host = mmc->priv;
	struct spacemit_sdhci_priv *priv = dev_get_priv(mmc->dev);
	struct rx_tuning *rxtuning = &priv->rxtuning;
	int ret;

	/*
	 * Tuning is required for SDR50/SDR104, HS200/HS400 cards and
	 * if clock frequency is greater than 100MHz in these modes.
	 */
	if (mmc->clock < 100 * 1000 * 1000 ||
	    !((mmc->selected_mode == MMC_HS_200) ||
	      (mmc->selected_mode == UHS_SDR50) ||
	      (mmc->selected_mode == UHS_SDR104)))
		return 0;

	if (IS_SD(mmc) && !mmc_getcd(mmc)) {
		return 0;
	}

	/* TX tuning config */
	if (!priv->phy_module) {
		dev_info(mmc->dev, "set tx_delaycode: %d\n", rxtuning->tx_delaycode);
		spacemit_sw_tx_tuning_prepare(host);
	} else {
		dev_info(mmc->dev, "use tx default timing\n");
	}

	/* step 2: get pass window and calculate the select_delay */
	spacemit_sw_rx_tuning_prepare(host, rxtuning->rx_dline_reg);
	ret = spacemit_sw_rx_select_window(host, opcode);
	if (ret) {
		dev_warn(mmc->dev, "abort tuning, err:%d\n", ret);
		return ret;
	}

	spacemit_sw_rx_set_delaycode(host, rxtuning->select_delay);
	dev_info(mmc->dev, "tuning done, use delay_code:%d\n", rxtuning->select_delay);
	return 0;
}
#endif

#if CONFIG_IS_ENABLED(MMC_HS400_ES_SUPPORT) || CONFIG_IS_ENABLED(MMC_HS400_SUPPORT)
static int spacemit_sdhci_phy_dll_init(struct sdhci_host *host)
{
	u32 state;
	int ret;

	spacemit_sdhci_clrsetbits(host, SDHC_DLL_PREDLY_NUM |
				  SDHC_DLL_FULLDLY_RANGE |
				  SDHC_DLL_VREG_CTRL,
				  FIELD_PREP(SDHC_DLL_PREDLY_NUM, 1) |
				  FIELD_PREP(SDHC_DLL_FULLDLY_RANGE, 1) |
				  FIELD_PREP(SDHC_DLL_VREG_CTRL, 1),
				  SPACEMIT_SDHC_PHY_DLLCFG);

	spacemit_sdhci_clrsetbits(host, SDHC_DLL_REG1_CTRL,
				  FIELD_PREP(SDHC_DLL_REG1_CTRL, 0x92),
				  SPACEMIT_SDHC_PHY_DLLCFG1);

	spacemit_sdhci_setbits(host, SDHC_DLL_ENABLE, SPACEMIT_SDHC_PHY_DLLCFG);

	ret = readl_poll_timeout(host->ioaddr + SPACEMIT_SDHC_PHY_DLLSTS, state,
				 state & SDHC_DLL_LOCK_STATE, 100);
	if (ret == -ETIMEDOUT) {
		dev_err(mmc_dev(host->mmc), "fail to lock phy dll in 100us!\n");
		return ret;
	}

	return 0;
}
#endif

#if CONFIG_IS_ENABLED(MMC_HS400_ES_SUPPORT)
static int spacemit_sdhci_hs400_enhanced_strobe(struct sdhci_host *host)
{
	spacemit_sdhci_setbits(host, SDHC_ENHANCE_STROBE_EN, SPACEMIT_SDHC_MMC_CTRL_REG);
	return spacemit_sdhci_phy_dll_init(host);
}
#endif

#if CONFIG_IS_ENABLED(MMC_HS400_SUPPORT)
static int spacemit_sdhci_pre_hs400_downgrade(struct sdhci_host *host)
{
	spacemit_sdhci_clrbits(host, SDHC_PHY_FUNC_EN | SDHC_PHY_PLL_LOCK,
			       SPACEMIT_SDHC_PHY_CTRL_REG);
	spacemit_sdhci_clrbits(host, SDHC_MMC_HS400 | SDHC_MMC_HS200 | SDHC_ENHANCE_STROBE_EN,
			       SPACEMIT_SDHC_MMC_CTRL_REG);
	spacemit_sdhci_clrbits(host, SDHC_HS200_USE_RFIFO, SPACEMIT_SDHC_PHY_FUNC_REG);

	udelay(5);

	spacemit_sdhci_setbits(host, SDHC_PHY_FUNC_EN | SDHC_PHY_PLL_LOCK,
			       SPACEMIT_SDHC_PHY_CTRL_REG);

	return 0;
}

static int spacemit_sdhci_pre_select_hs400(struct udevice *dev)
{
	struct spacemit_sdhci_priv *priv = dev_get_priv(dev);
	struct sdhci_host *host = &priv->host;

	spacemit_sdhci_setbits(host, SDHC_MMC_HS400, SPACEMIT_SDHC_MMC_CTRL_REG);

	return 0;
}

static int spacemit_sdhci_post_select_hs400(struct sdhci_host *host)
{
	return spacemit_sdhci_phy_dll_init(host);
}
#endif

static inline int spacemit_sdhci_get_clocks(struct udevice *dev)
{
	struct spacemit_sdhci_priv *priv = dev_get_priv(dev);
	struct sdhci_host *host = &priv->host;
#if !defined(CONFIG_K1_X_BOARD_FPGA) && !defined(CONFIG_K3_BOARD_FPGA)
	struct clk clk;
#endif
	int ret;

#if !defined(CONFIG_K1_X_BOARD_FPGA) && !defined(CONFIG_K3_BOARD_FPGA)
	ret = clk_get_bulk(dev, &priv->clks);
	if (ret) {
		dev_err(dev, "Can't get clk: %d\n", ret);
		return ret;
	}

	ret = clk_enable_bulk(&priv->clks);
	if (ret) {
		dev_err(dev, "Failed to enable clk: %d\n", ret);
		return ret;
	}
#endif
	ret = reset_get_bulk(dev, &priv->resets);
	if (ret) {
		dev_err(dev, "Can't get reset: %d\n", ret);
		return ret;
	}

	ret = reset_deassert_bulk(&priv->resets);
	if (ret) {
		dev_err(dev, "Failed to reset: %d\n", ret);
		return ret;
	}
#if !defined(CONFIG_K1_X_BOARD_FPGA) && !defined(CONFIG_K3_BOARD_FPGA)
	ret = clk_get_by_index(dev, 0, &clk);
	if (ret) {
		dev_err(dev, "Can't get io clk: %d\n", ret);
		return ret;
	}

	ret = clk_set_rate(&clk, host->max_clk);
	if (ret) {
		dev_err(dev, "Failed to set io clk: %d\n", ret);
		return ret;
	}
#endif
	return 0;
}

const struct sdhci_ops spacemit_sdhci_ops = {
	.set_control_reg = spacemit_sdhci_set_control_reg,
	.set_ios_post = spacemit_sdhci_set_ios_post,
#if CONFIG_IS_ENABLED(MMC_SUPPORTS_TUNING)
	.platform_execute_tuning = spacemit_sdhci_execute_tuning,
#endif
#if CONFIG_IS_ENABLED(MMC_HS400_ES_SUPPORT)
	.set_enhanced_strobe = spacemit_sdhci_hs400_enhanced_strobe,
#endif
};

static int spacemit_sdhci_probe(struct udevice *dev)
{
	struct spacemit_sdhci_plat *plat = dev_get_plat(dev);
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct spacemit_sdhci_priv *priv = dev_get_priv(dev);
	struct sdhci_host *host = &priv->host;
	struct dm_mmc_ops *mmc_driver_ops = (struct dm_mmc_ops *)dev->driver->ops;
	int ret;

	ret = spacemit_sdhci_get_clocks(dev);
	if (ret)
		return ret;

	/* Set quirks */
#if defined(CONFIG_SPL_BUILD)
	host->quirks = SDHCI_QUIRK_WAIT_SEND_CMD;
#else
	host->quirks = SDHCI_QUIRK_WAIT_SEND_CMD | SDHCI_QUIRK_32BIT_DMA_ADDR;
#endif
	host->host_caps = MMC_MODE_HS | MMC_MODE_HS_52MHz;
	host->ops = &spacemit_sdhci_ops;
	mmc_driver_ops->wait_dat0 = spacemit_sdhci_wait_dat0;
#if CONFIG_IS_ENABLED(MMC_HS400_SUPPORT)
	mmc_driver_ops->hs400_prepare_ddr = spacemit_sdhci_pre_select_hs400;
#endif
	host->mmc = &plat->mmc;
	host->mmc->dev = dev;
	ret = sdhci_setup_cfg(&plat->cfg, host, host->max_clk, SDHC_MIN_CLOCK);
	if (ret)
		return ret;

	host->mmc->priv = host;
	upriv->mmc = host->mmc;

	ret = sdhci_probe(dev);
	if (ret)
		return ret;

	/* emmc phy bypass if need */
	spacemit_mmc_phy_init(host);
#ifdef CONFIG_PINCTRL
	pinctrl_select_state(host->mmc->dev, "debug");
#endif
	dev_info(dev, "probe done.\n");
	return ret;
}

static int spacemit_sdhci_of_to_plat(struct udevice *dev)
{
	struct spacemit_sdhci_plat *plat = dev_get_plat(dev);
	struct spacemit_sdhci_priv *priv = dev_get_priv(dev);
	struct sdhci_host *host = &priv->host;
	int ret;

	host->name = dev->name;
	host->ioaddr = (void *)devfdt_get_addr(dev);
	host->max_clk = dev_read_u32_default(dev, "clock-frequency", SDHC_DEFAULT_MAX_CLOCK);

	priv->phy_module = dev_read_u32_default(dev, "sdh-phy-module", 0);
	priv->phy_driver_sel = dev_read_u32_default(dev, "spacemit,phy_driver_sel", PHY_DRIVE_SEL_DEFAULT);

	/* read rx tuning dline_reg */
	priv->rxtuning.rx_dline_reg = dev_read_u32_default(dev, "spacemit,rx_dline_reg", RX_TUNING_DLINE_REG);
	/* read rx tuning window limit */
	priv->rxtuning.window_limit = dev_read_u32_default(dev, "spacemit,rx_tuning_limit", RX_TUNING_WINDOW_THRESHOLD);
	/* read rx tuning window type */
	priv->rxtuning.window_type = dev_read_u32_default(dev, "spacemit,rx_tuning_type", MIDDLE_WINDOW);

	/* tx tuning dline_reg */
	priv->rxtuning.tx_dline_reg = dev_read_u32_default(dev, "spacemit,tx_dline_reg", TX_TUNING_DLINE_REG);
	/* tx tuning delaycode */
	priv->rxtuning.tx_delaycode = dev_read_u32_default(dev, "spacemit,tx_delaycode", TX_TUNING_DELAYCODE);

	ret = mmc_of_parse(dev, &plat->cfg);
	return ret;
}

static int spacemit_sdhci_bind(struct udevice *dev)
{
	struct spacemit_sdhci_plat *plat = dev_get_plat(dev);

	return sdhci_bind(dev, &plat->mmc, &plat->cfg);
}

static const struct udevice_id spacemit_sdhci_ids[] = {
	{ .compatible = "spacemit,k1-x-sdhci" },
	{ .compatible = "spacemit,k3-sdhci" },
	{ }
};

U_BOOT_DRIVER(spacemit_sdhci_drv) = {
	.name		= "spacemit_sdhci",
	.id		= UCLASS_MMC,
	.of_match	= spacemit_sdhci_ids,
	.of_to_plat	= spacemit_sdhci_of_to_plat,
	.ops		= &sdhci_ops,
	.bind		= spacemit_sdhci_bind,
	.probe		= spacemit_sdhci_probe,
	.priv_auto = sizeof(struct spacemit_sdhci_priv),
	.plat_auto = sizeof(struct spacemit_sdhci_plat),
};
