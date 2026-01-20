// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 Alibaba Group Holding Limited.
 */

//#define DEBUG
#include <log.h>
#include <linux/delay.h>
#include <clk.h>
#include <dm.h>
#include <malloc.h>
#include <sdhci.h>
#include <mmc.h>
#include "zhihe_sdhci.h"

#define SDHCI_TUNING_LOOP_COUNT 128
//#define SOFT_TUNING_EN

/* DELAY LANE Config */
#define TXDELAY_DEFAULT 50
static char s_delay_lanes[]= {
#ifdef CONFIG_TARGET_A210_EVB
	TXDELAY_DEFAULT, /* 0: MMC_LEGACY */
	TXDELAY_DEFAULT, /* 1: MMC_HS */
	TXDELAY_DEFAULT, /* 2: SD_HS */
	TXDELAY_DEFAULT, /* 3: MMC_HS_52 */
	TXDELAY_DEFAULT, /* 4: MMC_DDR_52 */
	46,              /* 5: UHS_SDR12 */
	46,              /* 6: UHS_SDR25 */
	46,              /* 7: UHS_SDR50 */
	46,              /* 8: UHS_DDR50 */
	46,              /* 9: UHS_SDR104 */
	TXDELAY_DEFAULT, /* 10: MMC_HS_200 */
	24,              /* 11: MMC_HS_400 */
	24,              /* 12: MMC_HS_400_ES */
#else
	TXDELAY_DEFAULT, /* 0: MMC_LEGACY */
	TXDELAY_DEFAULT, /* 1: MMC_HS */
	TXDELAY_DEFAULT, /* 2: SD_HS */
	TXDELAY_DEFAULT, /* 3: MMC_HS_52 */
	TXDELAY_DEFAULT, /* 4: MMC_DDR_52 */
	TXDELAY_DEFAULT, /* 5: UHS_SDR12 */
	TXDELAY_DEFAULT, /* 6: UHS_SDR25 */
	TXDELAY_DEFAULT, /* 7: UHS_SDR50 */
	TXDELAY_DEFAULT, /* 8: UHS_DDR50 */
	TXDELAY_DEFAULT, /* 9: UHS_SDR104 */
	TXDELAY_DEFAULT, /* 10: MMC_HS_200 */
	TXDELAY_DEFAULT, /* 11: MMC_HS_400 */
	TXDELAY_DEFAULT, /* 12: MMC_HS_400_ES */
#endif
};

#define DELAY_LANE s_delay_lanes[MMC_LEGACY]

static unsigned int s_cur_delay_set_mode = MMC_HS_400_ES + 1;
int zhihe_sdhci_set_delay(unsigned int mode, char delay)
{
	if (mode > MMC_HS_400_ES) {
		return -1;
	}

	s_delay_lanes[mode] = delay;
	s_cur_delay_set_mode = mode;
	return 0;
}

static void sdhci_phy_1_8v_init_no_pull(struct sdhci_host *host)
{
	uint32_t val;
	sdhci_writel(host, 1, DWC_MSHC_PTR_PHY_R);
	sdhci_writeb(host, 1 << 4, PHY_SDCLKDL_CNFG_R);
	sdhci_writeb(host, 0x40, PHY_SDCLKDL_DC_R);
	sdhci_writeb(host, 0xa, PHY_DLL_CNFG2_R);

	val = sdhci_readb(host, PHY_SDCLKDL_CNFG_R);
	val &= ~(1 << 4);
	sdhci_writeb(host, val, PHY_SDCLKDL_CNFG_R);

	val = sdhci_readw(host, PHY_CMDPAD_CNFG_R);
	sdhci_writew(host, val | 1, PHY_CMDPAD_CNFG_R);

	val = sdhci_readw(host, PHY_DATAPAD_CNFG_R);
	sdhci_writew(host, val | 1, PHY_DATAPAD_CNFG_R);

	val = sdhci_readw(host, PHY_RSTNPAD_CNFG_R);
	sdhci_writew(host, val | 1, PHY_RSTNPAD_CNFG_R);

	val = sdhci_readw(host, PHY_STBPAD_CNFG_R);
	sdhci_writew(host, val | 1, PHY_STBPAD_CNFG_R);

	val = sdhci_readb(host, PHY_DLL_CTRL_R);
	sdhci_writeb(host, val | 1, PHY_DLL_CTRL_R);
}

static void sdhci_phy_3_3v_init_no_pull(struct sdhci_host *host)
{
	uint32_t val;
	sdhci_writel(host, 1, DWC_MSHC_PTR_PHY_R);
	sdhci_writeb(host, 1 << 4, PHY_SDCLKDL_CNFG_R);
	sdhci_writeb(host, 0x40, PHY_SDCLKDL_DC_R);
	sdhci_writeb(host, 0xa, PHY_DLL_CNFG2_R);

	val = sdhci_readb(host, PHY_SDCLKDL_CNFG_R);
	val &= ~(1 << 4);
	sdhci_writeb(host, val, PHY_SDCLKDL_CNFG_R);

	val = sdhci_readw(host, PHY_CMDPAD_CNFG_R);
	sdhci_writew(host, val | 2, PHY_CMDPAD_CNFG_R);

	val = sdhci_readw(host, PHY_DATAPAD_CNFG_R);
	sdhci_writew(host, val | 2, PHY_DATAPAD_CNFG_R);

	val = sdhci_readw(host, PHY_RSTNPAD_CNFG_R);
	sdhci_writew(host, val | 2, PHY_RSTNPAD_CNFG_R);

	val = sdhci_readw(host, PHY_STBPAD_CNFG_R);
	sdhci_writew(host, val | 2, PHY_STBPAD_CNFG_R);

	val = sdhci_readb(host, PHY_DLL_CTRL_R);
	sdhci_writeb(host, val | 1, PHY_DLL_CTRL_R);
}

static void sdhci_phy_1_8v_init(struct sdhci_host *host, int delay)
{
	uint32_t val;

	struct snps_sdhci_plat *plat = dev_get_plat(host->mmc->dev);
	if (plat->pull_up_en == false) {
		sdhci_phy_1_8v_init_no_pull(host);
		return;
	}

	debug("    %s: set txdelay %d\n", __func__, delay);

	//set driving force
	sdhci_writel(host, (1 << PHY_RSTN) | (0xc << PAD_SP) | (0xc << PAD_SN), PHY_CNFG_R);

	/* disable SD_CLK_EN */
	val = sdhci_readb(host, SDHCI_CLOCK_CONTROL);
	val &= ~SDHCI_CLOCK_CARD_EN;
	sdhci_writeb(host, val, SDHCI_CLOCK_CONTROL);
	//disable delay lane
	sdhci_writeb(host, 1 << UPDATE_DC, PHY_SDCLKDL_CNFG_R);
	//set delay lane
	sdhci_writeb(host, delay, PHY_SDCLKDL_DC_R);
	sdhci_writeb(host, 0xa, PHY_DLL_CNFG2_R);
	//enable delay lane
	val = sdhci_readb(host, PHY_SDCLKDL_CNFG_R);
	val &= ~(1 << UPDATE_DC);
	sdhci_writeb(host, val, PHY_SDCLKDL_CNFG_R);
	/* enable SD_CLK_EN */
	val = sdhci_readb(host, SDHCI_CLOCK_CONTROL);
	val |= SDHCI_CLOCK_CARD_EN;
	sdhci_writeb(host, val, SDHCI_CLOCK_CONTROL);

	/* configure phy pads */
	val = (1 << RXSEL) | (1 << WEAKPULL_EN) | (3 << TXSLEW_CTRL_P) | (3 << TXSLEW_CTRL_N);
	sdhci_writew(host, val, PHY_CMDPAD_CNFG_R);
	sdhci_writew(host, val, PHY_DATAPAD_CNFG_R);
	sdhci_writew(host, val, PHY_RSTNPAD_CNFG_R);

	val = (3 << TXSLEW_CTRL_P) | (3 << TXSLEW_CTRL_N);
	sdhci_writew(host, val, PHY_CLKPAD_CNFG_R);

	val = (1 << RXSEL) | (2 << WEAKPULL_EN) | (3 << TXSLEW_CTRL_P) | (3 << TXSLEW_CTRL_N);
	sdhci_writew(host, val, PHY_STBPAD_CNFG_R);

	/* enable data strobe mode */
	sdhci_writeb(host, 0, PHY_DLL_CTRL_R);
	sdhci_writew(host, 0, SDHCI_CLOCK_CONTROL);
	sdhci_writew(host, 0x8000, PHY_DLLBT_CNFG_R);
	sdhci_writeb(host, 3 << SLV_INPSEL, PHY_DLLDL_CNFG_R);
	sdhci_writeb(host, 0x25, PHY_DLL_CNFG1_R);
	sdhci_writew(host, 0x7, SDHCI_CLOCK_CONTROL);
	sdhci_writeb(host, (1 << DLL_EN), PHY_DLL_CTRL_R);
}

static void sdhci_phy_3_3v_init(struct sdhci_host *host, int delay)
{
	uint32_t val;
	struct snps_sdhci_plat *plat = dev_get_plat(host->mmc->dev);
	if (plat->pull_up_en == false) {
		sdhci_phy_3_3v_init_no_pull(host);
		return;
	}

	debug("    %s: set txdelay %d\n", __func__, delay);

	//set driving force
	sdhci_writel(host, (1 << PHY_RSTN) | (0xc << PAD_SP) | (0xc << PAD_SN), PHY_CNFG_R);

	/* disable SD_CLK_EN */
	val = sdhci_readb(host, SDHCI_CLOCK_CONTROL);
	val &= ~SDHCI_CLOCK_CARD_EN;
	sdhci_writeb(host, val, SDHCI_CLOCK_CONTROL);
	//disable delay lane
	sdhci_writeb(host, 1 << UPDATE_DC, PHY_SDCLKDL_CNFG_R);
	//set delay lane
	sdhci_writeb(host, delay, PHY_SDCLKDL_DC_R);
	sdhci_writeb(host, 0xa, PHY_DLL_CNFG2_R);
	//enable delay lane
	val = sdhci_readb(host, PHY_SDCLKDL_CNFG_R);
	val &= ~(1 << UPDATE_DC);
	sdhci_writeb(host, val, PHY_SDCLKDL_CNFG_R);
	/* enable SD_CLK_EN */
	val = sdhci_readb(host, SDHCI_CLOCK_CONTROL);
	val |= SDHCI_CLOCK_CARD_EN;
	sdhci_writeb(host, val, SDHCI_CLOCK_CONTROL);

	val = (2 << RXSEL) | (1 << WEAKPULL_EN) | (3 << TXSLEW_CTRL_P) | (3 << TXSLEW_CTRL_N);
	sdhci_writew(host, val, PHY_CMDPAD_CNFG_R);
	sdhci_writew(host, val, PHY_DATAPAD_CNFG_R);
	sdhci_writew(host, val, PHY_RSTNPAD_CNFG_R);

	val = (3 << TXSLEW_CTRL_P) | (3 << TXSLEW_CTRL_N);
	sdhci_writew(host, val, PHY_CLKPAD_CNFG_R);

	val = (2 << RXSEL) | (2 << WEAKPULL_EN) | (3 << TXSLEW_CTRL_P) | (3 << TXSLEW_CTRL_N);
	sdhci_writew(host, val, PHY_STBPAD_CNFG_R);

	sdhci_writeb(host, (1 << DLL_EN), PHY_DLL_CTRL_R);
	/*set i wait*/
	sdhci_writeb(host, 0x5, PHY_DLL_CNFG1_R);
}

static void zhihe_sdhci_set_voltage(struct sdhci_host *host)
{
	struct mmc *mmc = (struct mmc *)host->mmc;
	struct snps_sdhci_plat *plat = dev_get_plat(host->mmc->dev);
	u32 reg;

	if ((mmc->selected_mode > MMC_DDR_52) && (mmc->signal_voltage <= MMC_SIGNAL_VOLTAGE_180)) {
		reg = sdhci_readw(host, SDHCI_HOST_CONTROL2);
		reg |= SDHCI_CTRL_VDD_180;
		sdhci_writew(host, reg, SDHCI_HOST_CONTROL2);
	} else if ((mmc->selected_mode <= MMC_DDR_52) && (mmc->signal_voltage > MMC_SIGNAL_VOLTAGE_180)) {
		reg = sdhci_readw(host, SDHCI_HOST_CONTROL2);
		reg &= ~SDHCI_CTRL_VDD_180;
		if (plat->io_fixed_1v8)
			reg |= SDHCI_CTRL_VDD_180;
		sdhci_writew(host, reg, SDHCI_HOST_CONTROL2);
	} else {
		debug("Warning: mode %d, voltage %d\n", mmc->selected_mode, mmc->signal_voltage);
	}
}

static void zhihe_sdhci_set_uhs_timing(struct sdhci_host *host)
{
	struct mmc *mmc = (struct mmc *)host->mmc;
	u32 reg;

	/*
	 * HOST_CTRL2_R: 2:0 Bit
	 *   0x0 (SDR12):  SDR12/Legacy             2(SD_HS),5/0,
	 *   0x1 (SDR25):  SDR25/High Speed SDR     6/1(MMC_HS),3(MMC_HS_52)
	 *   0x2 (SDR50):  SDR50                    7/-
	 *   0x3 (SDR104): SDR104/HS200             9/10
	 *   0x4 (DDR50):  DDR50/High Speed DDR     8/4(MMC_DDR_52)
	 *   0x5 (RSVD5):  Reserved
	 *   0x6 (RSVD6):  Reserved
	 *   0x7 (UHS2):   HS400                    -/11
	*/
	reg = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	reg &= ~SDHCI_CTRL_UHS_MASK;

	switch (mmc->selected_mode) {
	case UHS_SDR25:
	case MMC_HS:
	case MMC_HS_52:
		reg |= SDHCI_CTRL_UHS_SDR25;
		break;
	case UHS_SDR50:
		reg |= SDHCI_CTRL_UHS_SDR50;
		break;
	case UHS_DDR50:
	case MMC_DDR_52:
		reg |= SDHCI_CTRL_UHS_DDR50;
		break;
	case UHS_SDR104:
	case MMC_HS_200:
		reg |= SDHCI_CTRL_UHS_SDR104;
		break;
	case MMC_HS_400:
	case MMC_HS_400_ES:
		reg |= SNPS_SDHCI_CTRL_HS400;
		break;
	default:
		reg |= SDHCI_CTRL_UHS_SDR12;
	}

	sdhci_writew(host, reg, SDHCI_HOST_CONTROL2);
}

static void zhihe_sdhci_set_control_reg(struct sdhci_host *host)
{
	struct mmc *mmc = (struct mmc *)host->mmc;
	u32 reg;

	int delay = s_delay_lanes[mmc->selected_mode];

	if (s_cur_delay_set_mode == mmc->selected_mode) {
		printf("  Set mmc mode %d delay %d\n", mmc->selected_mode, delay);
	}
	debug("\n%s: mode %d delay %d voltage %d\n", __func__, mmc->selected_mode, delay, mmc->signal_voltage);

	reg = sdhci_readw(host, EMMC_CTRL_R);
	if (IS_SD(host->mmc)) {
		reg &= ~EMMC_CARD;
	} else {
		reg |= EMMC_CARD;
	}

	sdhci_writeb(host, reg, EMMC_CTRL_R);

	/*
	 * sdhci_set_control_reg 
	 * is incompatible with the current peripheral, reimplementing it here.
	 */
	zhihe_sdhci_set_voltage(host);
	zhihe_sdhci_set_uhs_timing(host);

	/* 
	 * Phy config
	 * 3.3v Phy: MMC_LEGACY、MMC_HS、SD_HS、MMC_HS_52、MMC_DDR_52
	 */
	if (mmc->selected_mode <= MMC_DDR_52 ) {
		sdhci_phy_3_3v_init(host, delay);
	} else {
		sdhci_phy_1_8v_init(host, delay);
	}

	if (mmc->selected_mode == MMC_HS_400) {
		//disable auto tuning
		reg = sdhci_readl(host, AT_CTRL_R);
		reg &= ~(1 << AT_EN);
		sdhci_writel(host, reg, AT_CTRL_R);
	} else {
		sdhci_writeb(host, 0, PHY_DLLDL_CNFG_R);
	}
}

#ifndef CONFIG_SPL_BUILD
int zhihe_send_tuning(struct mmc *mmc, u8 opcode)
{
	struct mmc_cmd cmd;
	struct mmc_data data;
	struct sdhci_host *host = dev_get_priv(mmc->dev);

	cmd.cmdidx = opcode;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;

	data.blocksize = 64;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;

	if (cmd.cmdidx == MMC_CMD_SEND_TUNING_BLOCK_HS200 && mmc->bus_width == 8)
		data.blocksize = 128;

	sdhci_writew(host, SDHCI_MAKE_BLKSZ(SDHCI_DEFAULT_BOUNDARY_ARG, data.blocksize),
				SDHCI_BLOCK_SIZE);
	sdhci_writew(host, data.blocks, SDHCI_BLOCK_COUNT);
	sdhci_writew(host, SDHCI_TRNS_READ, SDHCI_TRANSFER_MODE);

	return mmc_send_cmd(mmc, &cmd, NULL);
}

/*
 * Tuning Schema
 * 1. Threshold Based Selection Tuning Schema
 *      This mode allows the tuning engine to select the first complete sampling window
 *      that meets the threshold criteria defined.
 *      AT_CTRL_R.SWIN_TH_EN = 1, AT_CTRL_R.SWIN_TH_VAL = <as required for design>
 * 2. Backward Compatible Tuning Schema
 *      This mode allows the tuning engine to function as it was in 1.50a and earlier releases of the DWC_mshc.
 *      AT_CTRL_R.SWIN_TH_EN = 1, AT_CTRL_R.SWIN_TH_VAL = 0
 * 3. Largest Sampling Window Tuning Schema
 *      This mode allows the tuning algorithm to move through all the taps of delay line
 *      to identify the largest sampling window available.
 *      AT_CTRL_R.SWIN_TH_EN = 1, AT_CTRL_R.SWIN_TH_VAL = <do not care>
 */
#ifdef SOFT_TUNING_EN
static char rx_tuning_wnd[SDHCI_TUNING_LOOP_COUNT];
#endif
static int zhihe_execute_tuning(struct mmc *mmc, u8 opcode)
{
	struct sdhci_host *host = dev_get_priv(mmc->dev);
	uint32_t val;
	int i;

	debug("\nEnter %s opcode %d\n", __func__, opcode);

	/* AT_CTRL_R value base init */
	val = sdhci_readl(host, AT_CTRL_R);
	val &= ~(1 << SW_TUNE_EN);     // Disable software tuning
	val &= ~(0xf << WIN_EDGE_SEL); // Clear =0 User selection disabled. Tuning calculated edges are used.
	val &= ~(1 << RPT_TUNE_ERR);   // Default mode, No errors are reported.
	val &= ~(1 << CI_SEL);         // Clear =0 Driven in block gap interval
	val &= ~((1 << SWIN_TH_EN) | (0xff << SWIN_TH_VAL));   // Largest Sampling Window Tuning

	val |= (1 << PRE_CHANGE_DLY) | (3 << POST_CHANGE_DLY); // Phase switching cycle latency
	val |= (1 << TUNE_CLK_STOP_EN);// Clocks stopped during phase code change

#ifndef SOFT_TUNING_EN
	uint16_t ctrl = 0;
	/* Auto-tuning */
	sdhci_writeb(host, 3 << INPSEL_CNFG, PHY_ATDL_CNFG_R); //ATDL Select drift clk by design

	/* Enable auto-tuning */
	val |= (1 << AT_EN);
	sdhci_writel(host, val, AT_CTRL_R); 

	/* Start tuning */
	ctrl = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	ctrl |= SDHCI_CTRL_EXEC_TUNING;
	sdhci_writew(host, ctrl, SDHCI_HOST_CONTROL2);
#else
	/* Soft tuning */
	val |= (1 << SW_TUNE_EN);
	sdhci_writel(host, val, AT_CTRL_R);
#endif
	mdelay(1);

	for(i = 0; i < SDHCI_TUNING_LOOP_COUNT; i++ ) {
#ifndef SOFT_TUNING_EN
		/* CMD21 or CMD19 */
		zhihe_send_tuning(host->mmc, opcode);
		ctrl = sdhci_readw(host, SDHCI_HOST_CONTROL2);
		debug("  %d HOST_CTRL2_R=0x%x AT_STAT_R=0x08%x\n", i, ctrl, sdhci_readl(host, AT_STAT_R));
		if (!(ctrl & SDHCI_CTRL_EXEC_TUNING)) {
			break;
		}

		if (opcode == MMC_CMD_SEND_TUNING_BLOCK) {
			udelay(1);
		}
#else
		/* Update CENTER_PH_CODE */
		val = sdhci_readl(host, AT_STAT_R);
		val &= ~(0xff << CENTER_PH_CODE);
		val |= (i << CENTER_PH_CODE);
		sdhci_writel(host, val, AT_STAT_R);
		if (mmc_send_tuning(host->mmc, opcode)) {
			rx_tuning_wnd[i] = 0;
		} else {
			rx_tuning_wnd[i] = 1;
		}
#endif
	}

	if (s_cur_delay_set_mode == mmc->selected_mode) {
#ifdef SOFT_TUNING_EN
		printf("  TXDLY %d, RXWND[", s_delay_lanes[mmc->selected_mode]);
		for(i = 0; i < SDHCI_TUNING_LOOP_COUNT; i++) {
			if (rx_tuning_wnd[i])
				printf("%c", '-');
			else
				printf("%02x", i);
		}
		printf("]\n");
#endif
	}

#ifndef SOFT_TUNING_EN
	if (!(ctrl & SDHCI_CTRL_TUNED_CLK)) {
		printf("%s:Tuning failed\n", __func__);
		return -1;
	}
#else
	/* Config best CENTER_PH_CODE
	 * Pass Step and Fail Step Scenarios
	 *   FPF PF FP PFP PFPF P(all pass)
	 */
	/* {{end-idx1 size1}, {end-idx2 size2}, , {end-idx3 size3}} */
	int steps[3][3] = {{0, 0}, {0, 0}, {0, 0}};
	int step = 0;
	for (i = 0; i < SDHCI_TUNING_LOOP_COUNT; i++) {
		if (rx_tuning_wnd[i] == 1) {
			steps[step][1]++; // count size
		} else {
			if (steps[step][1] > 0) {
				//save step, find next step
				steps[step][0] = i;
				step++;
				if (step > 2) {
					break;
				}
			}
		}
	}
	// printf("  Steps: {%d %d}, {%d %d}, {%d %d}, %d\n", 
	// 			steps[0][0], steps[0][1],
	// 			steps[1][0], steps[1][1],
	// 			steps[2][0], steps[2][1], i);
	if(i == SDHCI_TUNING_LOOP_COUNT) {
		if (steps[0][0] == steps[0][1]) {
			if (steps[2][0] == 0 && steps[2][1] > 0) {
				/* pfpfp -> pfpf*/
				steps[0][1] += steps[2][1];
				steps[2][1] = 0;
			} else if (steps[1][0] == 0 && steps[1][1] > 0) {
				/* pfp -> pf*/
				steps[0][1] += steps[1][1];
				steps[1][1] = 0;
			}
		}
		// printf("  Steps: {%d %d}, {%d %d}\n", 
		// 			steps[0][0], steps[0][1],
		// 			steps[1][0], steps[1][1]);

		/* Max valid step count = 2 */
		int valid_step = 0;
		if (steps[1][1] > 0) {
			if (steps[1][1] > steps[0][1]) {
				valid_step = 1;
			}
		} else if (steps[0][1] > 0) {
			valid_step = 0;
		} else {
			printf("  SW TUNING: Unkown Scenarios\n"); // all F
			return -1;
		}

		/* Compute the optimal parameters. */
		int mid = steps[valid_step][0] - (steps[valid_step][1] / 2);
		mid = (mid + SDHCI_TUNING_LOOP_COUNT) % SDHCI_TUNING_LOOP_COUNT;
		// printf("  step %d, 0x%08x(%d)\n",valid_step, mid, mid);

		val = sdhci_readl(host, AT_STAT_R);
		val &= ~(0xff << CENTER_PH_CODE);
		val |= (mid << CENTER_PH_CODE);
		sdhci_writel(host, val, AT_STAT_R);
	} else {
		printf("  SW TUNING: Unkown Scenarios\n");
		return -1;
	}

#endif

	val = sdhci_readl(host, AT_STAT_R);
	printf("  Tuning: %d 0x%08x\n", s_delay_lanes[mmc->selected_mode], val);

	/*
	 * Disable the tuning engine to prevent auto-tuning
	 *
	 * U-Boot is only involved in the brief boot process, disable auto-tuning here.
	 * Implement auto-tuning in the kernel instead to enhance system stability.
	 *
	 * Auto-tuning is a hardware managed re-tuning feature that complies with the SD HCI Mode3 re-tuning procedure.
	 * Auto-tuning removes the need for the host software to re-tune the sampling clock for every 4 MB of data transfer (SD HCI).
	 * A tuning sampling clock is recommended in both SD and eMMC modes to ensure ease in timing closure and
	 *   for robust operation while operating at high SDR speeds, such as SDR104 and HS200.
	 *
	 * Auto-tuning is supported only in HS200 (eMMC mode) and SDR104 (SD mode) modes.
	 */
	val = sdhci_readl(host, AT_CTRL_R);
	val &= ~(1 << AT_EN);
	val |= (1 << SW_TUNE_EN);
	sdhci_writel(host, val, AT_CTRL_R);

	return 0;
}
#endif

static int zhihe_sdhci_set_ios_post(struct sdhci_host *host)
{
	debug("\n%s\n", __func__);
	mdelay(50);
	return 0;
}

const struct sdhci_ops snps_ops = {
#ifndef CONFIG_SPL_BUILD
	.platform_execute_tuning = &zhihe_execute_tuning,
#endif
	.set_control_reg = &zhihe_sdhci_set_control_reg,
	.set_ios_post = zhihe_sdhci_set_ios_post,
};

static int snps_sdhci_probe(struct udevice *dev)
{
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct snps_sdhci_plat *plat = dev_get_plat(dev);
	struct sdhci_host *host = dev_get_priv(dev);

	u32 max_clk, f_max;
	struct clk clk;
	int ret;

	ret = clk_get_by_index(dev, 0, &clk);
	if (ret)
		return ret;

	debug("\n%s: txdelay %d\n", __func__, DELAY_LANE);

	plat->pull_up_en = false;
	plat->io_fixed_1v8 = false;
	host->name = dev->name;
	host->ioaddr = (void *)devfdt_get_addr(dev);

	max_clk = clk_get_rate(&clk);
	if (IS_ERR_VALUE(max_clk)) {
		ret = max_clk;
		goto err;
	}

	//get Maximum Clock
	if (0 == dev_read_u32(dev, "max-frequency", &f_max)) {
		;
	}

	host->max_clk = max_clk;
	host->mmc = &plat->mmc;
	host->mmc->dev = dev;
	host->mmc->priv = host;
	upriv->mmc = host->mmc;
	host->ops = &snps_ops;

	ret = mmc_of_parse(dev, &plat->cfg);
	if (ret)
		goto err;

	ret = sdhci_setup_cfg(&plat->cfg, host, f_max, 400000);
	if (ret)
		goto err;

	host->voltages = MMC_VDD_33_34;
	ret = sdhci_probe(dev);
	if (ret)
		goto err;

	if (dev_read_bool(dev, "pull_up"))
		plat->pull_up_en = true;
	if (dev_read_bool(dev, "io_fixed_1v8"))
		plat->io_fixed_1v8 = true;

	uint16_t val = sdhci_readw(host, SDHCI_HOST_CONTROL2);

	if (host->voltages == MMC_VDD_165_195) {
		val |= SDHCI_CTRL_VDD_180;
		sdhci_writew(host, val, SDHCI_HOST_CONTROL2);
		sdhci_phy_1_8v_init(host, DELAY_LANE);
	} else {
		val &= ~SDHCI_CTRL_VDD_180;
		if (plat->io_fixed_1v8)
			val |= SDHCI_CTRL_VDD_180;
		sdhci_writew(host, val, SDHCI_HOST_CONTROL2);
		sdhci_phy_3_3v_init(host, DELAY_LANE);
	}

	host->voltages = MMC_VDD_32_33 | MMC_VDD_33_34 | MMC_VDD_165_195;

	return 0;

err:
	clk_disable(&clk);
	return ret;
}

static int snps_sdhci_bind(struct udevice *dev)
{
	struct snps_sdhci_plat *plat = dev_get_plat(dev);

	return sdhci_bind(dev, &plat->mmc, &plat->cfg);
}

static const struct udevice_id snps_sdhci_ids[] = { { .compatible = "zhihe,a2xx-dwcmshc" }, {} };

U_BOOT_DRIVER(snps_sdhci_drv) = {
	.name = "snps_sdhci",
	.id = UCLASS_MMC,
	.of_match = snps_sdhci_ids,
	.ops = &sdhci_ops,
	.bind = snps_sdhci_bind,
	.probe = snps_sdhci_probe,
	.priv_auto = sizeof(struct sdhci_host),
	.plat_auto = sizeof(struct snps_sdhci_plat),
};
