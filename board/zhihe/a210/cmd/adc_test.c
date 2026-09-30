// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024-2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <command.h>
#include <asm/io.h>
#include <asm/types.h>
#include <vsprintf.h>
#include <linux/delay.h>
#include "../include/addr_defines.h"

#define	__io_address(a)	(void *)(uintptr_t)(a)

#define A210_ADC_PHY_CFG 0x00
#define A210_ADC_PHY_CTRL 0x04
#define A210_ADC_PHY_TEST 0x08
#define A210_ADC_OP_CTRL 0x0C
#define A210_ADC_OP_SINGLE_START 0x10
#define A210_ADC_FCLK_CTRL 0x14
#define A210_ADC_START_TIME 0x18
#define A210_ADC_SAMPLE_TIME 0x1C
#define A210_ADC_SAMPLE_DATA 0x20
#define A210_ADC_INT_CTRL1 0x50
#define A210_ADC_INT_CTRL2 0x54
#define A210_ADC_INT_STATUS 0x58
#define A210_ADC_INT_ACTUAL_VALUE_CH0 0x60
#define A210_ADC_INT_ACTUAL_VALUE_CH1 0x64
#define A210_ADC_INT_DELTA_VALUE_CH0 0x90
#define A210_ADC_INT_DELTA_VALUE_CH1 0x94
#define A210_ADC_DFX_EOC_CNT_CH0 0xc4

#define A210_ADC_PHY_CTRL_RST_EN (0x1 << 4)
#define A210_ADC_PHY_CTRL_ENADC_EN (0x1 << 0)
#define A210_ADC_OP_SINGLE_START_EN BIT(0)

#define A210_ADC_OP_CONTINOUS_MODE (0 << 0)
#define A210_ADC_OP_ONE_SHOT_MODE (1 << 0)

/* ADC sample data */
/*
 * In Single mode:
 * bit[31:16],reserved;
 * bit[15],data valid;
 * bit[14:12],channel number;
 * bit[11:0],adc conversion data
 *
 * In Continous mode:
 * bit[31],data valid;
 * bit[30:28],channel number;
 * bit[27:16],adc conversion data
 * bit[15],data valid;
 * bit[14:12],channel number;
 * bit[11:0],adc conversion data
 */

#define A210_ADC_SAMPLE_DATA_CH1 GENMASK(27, 16)
#define A210_ADC_SAMPLE_DATA_CH1_OFF (16)
#define A210_ADC_SAMPLE_DATA_CH1_VLD BIT(31)
#define A210_ADC_SAMPLE_CH1_NUMBER GENMASK(30, 28)
#define A210_ADC_SAMPLE_CH1_NUMBER_OFF 28
#define A210_ADC_SAMPLE_DATA_CH0 GENMASK(11, 0)
#define A210_ADC_SAMPLE_DATA_CH0_VLD BIT(15)
#define A210_ADC_SAMPLE_DATA_CH0_OFF (0)
#define A210_ADC_SAMPLE_CH0_NUMBER GENMASK(14, 12)
#define A210_ADC_SAMPLE_CH0_NUMBER_OFF 12

static uint32_t g_chan = 0;
static uint32_t g_sample_cnt = 0x100;
static uint32_t g_fclk_ctrl = 0x1000b;
static uint32_t g_sample_time = 0x1e;
static uint32_t g_start_time = 0xa0;
static int32_t g_phy_cfg = 0x3;
static uint32_t g_op_ctrl = 0x1000;

static void a210_adc_reset(void)
{
	u32 tmp;

	tmp = readl((void *)(AP_ADC_BADDR + A210_ADC_PHY_CTRL));
	tmp |= A210_ADC_PHY_CTRL_RST_EN;
	writel(tmp, (void *)(AP_ADC_BADDR + A210_ADC_PHY_CTRL));
	udelay(10);
	tmp &= ~A210_ADC_PHY_CTRL_RST_EN;
	writel(tmp, (void *)(AP_ADC_BADDR + A210_ADC_PHY_CTRL));
}

static void a210_adc_hw_init(void)
{
	/* adc_phy_enctr： 0x8e0 */
	writel(0x8e0, (void *)(AP_ADC_BADDR + A210_ADC_PHY_TEST));

	writel(g_phy_cfg, (void *)(AP_ADC_BADDR + A210_ADC_PHY_CFG));
	writel(g_op_ctrl, (void *)(AP_ADC_BADDR + A210_ADC_OP_CTRL));

	writel(g_fclk_ctrl, (void *)(AP_ADC_BADDR + A210_ADC_FCLK_CTRL));
	writel(g_start_time, (void *)(AP_ADC_BADDR + A210_ADC_START_TIME));
	writel(g_sample_time, (void *)(AP_ADC_BADDR + A210_ADC_SAMPLE_TIME));

	/* disable the irq */
	writel(0x3, (void *)(AP_ADC_BADDR + A210_ADC_INT_CTRL2));
	writel(0, (void *)(AP_ADC_BADDR + A210_ADC_PHY_CTRL));
}

static void a210_adc_start_sampling(void)
{
	int cnt = 0;
	int phy_ctrl;
	int cnt2, cnt1;
	int sample_cnt;
	int continous_mode = 0;
	ushort *cont_data = (ushort *)0xd0000000; /* 96K size: 130k(0x20800 ~ 226k(0x38800)*/

	phy_ctrl = readl((void *)(AP_ADC_BADDR + A210_ADC_PHY_CTRL));
	phy_ctrl |= A210_ADC_PHY_CTRL_ENADC_EN;
	writel(phy_ctrl, (void *)(AP_ADC_BADDR + A210_ADC_PHY_CTRL));

	cnt1 = readl(__io_address(AP_ADC_BADDR + A210_ADC_DFX_EOC_CNT_CH0 + (g_chan * 8)));
	if (g_chan < 0 || g_chan > 3)
		printf("non valid chan set, only support 0~3 chan\n");

	if (!(g_op_ctrl & A210_ADC_OP_ONE_SHOT_MODE))
		continous_mode = 1;

	while (cnt < g_sample_cnt) {
		uint ievent;
		uint val = 0, val1 = 0;
		uint chan_number;
		int timeout = 1000000;

		do {
			ievent = readl((void *)(AP_ADC_BADDR + A210_ADC_SAMPLE_DATA));

			if (ievent & A210_ADC_SAMPLE_DATA_CH0_VLD) {
				chan_number = (ievent & A210_ADC_SAMPLE_CH0_NUMBER) >> A210_ADC_SAMPLE_CH0_NUMBER_OFF;
				if (chan_number == g_chan) {
					val = (ievent & A210_ADC_SAMPLE_DATA_CH0) >> A210_ADC_SAMPLE_DATA_CH0_OFF;
					if (!continous_mode)
						break;
				}
			}

			if (continous_mode && (ievent & A210_ADC_SAMPLE_DATA_CH1_VLD)) {
				chan_number = (ievent & A210_ADC_SAMPLE_CH1_NUMBER) >> A210_ADC_SAMPLE_CH1_NUMBER_OFF;
				if (chan_number == g_chan) {
					val1 = (ievent & A210_ADC_SAMPLE_DATA_CH1) >> A210_ADC_SAMPLE_DATA_CH1_OFF;
					break;
				}
			}
		} while (timeout--);

		if (timeout <= 0) {
			printf("timeout to read chan sample data\n");

			phy_ctrl = readl((void *)(AP_ADC_BADDR + A210_ADC_PHY_CTRL));
			phy_ctrl &= ~A210_ADC_PHY_CTRL_ENADC_EN;
			writel(phy_ctrl, (void *)(AP_ADC_BADDR + A210_ADC_PHY_CTRL));
			return;
		}

		cont_data[cnt] = (ushort)val;
		cnt++;
		if (continous_mode) {
			cont_data[cnt] = (ushort)val1;
			cnt++;
		}
	}

	phy_ctrl = readl((void *)(AP_ADC_BADDR + A210_ADC_PHY_CTRL));
	phy_ctrl &= ~A210_ADC_PHY_CTRL_ENADC_EN;
	writel(phy_ctrl, (void *)(AP_ADC_BADDR + A210_ADC_PHY_CTRL));

	cnt2 = readl(__io_address(AP_ADC_BADDR + A210_ADC_DFX_EOC_CNT_CH0 + (g_chan * 8)));

	sample_cnt = cnt2 > cnt1 ? cnt2 - cnt1 : (1 << 24) - cnt1 + cnt2;
	printf("sample_cnt:%d\n", sample_cnt);

	cnt = 0;
	while (cnt < g_sample_cnt) {
		printf("%d\n", cont_data[cnt]);
		cnt++;
	}
}

static int adc_sampling(struct cmd_tbl *cmdtp, int flag, int argc,
                        char *const argv[])
{
	ulong args[8] = {0};
	uint32_t *globals[] = {
		&g_chan, &g_sample_cnt, &g_fclk_ctrl, &g_sample_time,
		&g_start_time, &g_phy_cfg
	};
	const char *names[] = {
		"chan", "sample_cnt", "fclk_ctrl", "sample_time",
		"start_time", "phy_cfg"
	};
	int i, n = min(argc - 1, 6);

	for (i = 0; i < n; i++) {
		if (strict_strtoul(argv[i + 1], 16, &args[i]) < 0) {
			printf("Invalid %s: %s\n", names[i], argv[i + 1]);
			return CMD_RET_USAGE;
		}
		*globals[i] = (uint32_t)args[i];
	}

	/*
	 * [19:12] adc_ch_en_vect Support for channel(1 2 4 8)
	 * [4]     adc_dma_enable 
	 * [0]     adc_op_mode    1:Single mode 0:Continous mode
	 */
	g_op_ctrl = (1 << (g_chan + 12));

	printf("        adc_sampling %#x %#x %#x %#x %#x %#x (op ctrl %#x)\n",
			g_chan, g_sample_cnt, g_fclk_ctrl, g_sample_time,
			g_start_time, g_phy_cfg, g_op_ctrl);

	a210_adc_reset();
	a210_adc_hw_init();
	a210_adc_start_sampling();

	return 0;
}

U_BOOT_CMD(
	adc_sampling, 9, 1, adc_sampling,
	"Zhihe ADC sampling, example: adc_sampling 0x0 0x100 0x1000b 0x1e 0xa0 0x3",
	"[chan sample_cnt fclk_ctrl sample_time satrt_time phy_cfg]");
