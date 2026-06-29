// SPDX-License-Identifier: GPL-2.0-only
/*
 * Spacemit K3 reset controller driver
 *
 * Copyright (c) 2025, spacemit Corporation.
 *
 */

#include <dm.h>
#include <reset-uclass.h>
#include <asm/io.h>
#include <dm/device_compat.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <linux/bitops.h>
#include <dt-bindings/reset/spacemit-k3-reset.h>
#include "reset-spacemit-k3.h"

/* APBC register offset */
#define APBC_UART0_CLK_RST      0x00
#define APBC_UART2_CLK_RST      0x04
#define APBC_GPIO_CLK_RST       0x08
#define APBC_PWM0_CLK_RST       0x0c
#define APBC_PWM1_CLK_RST       0x10
#define APBC_PWM2_CLK_RST       0x14
#define APBC_PWM3_CLK_RST       0x18
#define APBC_TWSI8_CLK_RST      0x20
#define APBC_UART3_CLK_RST      0x24
#define APBC_RTC_CLK_RST        0x28
#define APBC_TWSI0_CLK_RST      0x2c
#define APBC_TWSI1_CLK_RST      0x30
#define APBC_TIMERS0_CLK_RST    0x34
#define APBC_TWSI2_CLK_RST      0x38
#define APBC_AIB_CLK_RST        0x3c
#define APBC_TWSI4_CLK_RST      0x40
#define APBC_TIMERS1_CLK_RST    0x44
#define APBC_ONEWIRE_CLK_RST    0x48
#define APBC_TWSI5_CLK_RST      0x4c
#define APBC_DRO_CLK_RST        0x58
#define APBC_IR0_CLK_RST        0x5c
#define APBC_IR1_CLK_RST        0x1c
#define APBC_TWSI6_CLK_RST      0x60
#define APBC_TSEN_CLK_RST       0x6c
#define APBC_UART4_CLK_RST      0x70
#define APBC_UART5_CLK_RST      0x74
#define APBC_UART6_CLK_RST      0x78
#define APBC_SSP3_CLK_RST       0x7c
#define APBC_SSPA0_CLK_RST      0x80
#define APBC_SSPA1_CLK_RST      0x84
#define APBC_SSPA2_CLK_RST      0x88
#define APBC_SSPA3_CLK_RST      0x8c
#define APBC_IPC_AP2AUD_CLK_RST 0x90
#define APBC_UART7_CLK_RST      0x94
#define APBC_UART8_CLK_RST      0x98
#define APBC_UART9_CLK_RST      0x9c
#define APBC_CAN0_CLK_RST       0xa0
#define APBC_CAN1_CLK_RST       0xa4
#define APBC_PWM4_CLK_RST       0xa8
#define APBC_PWM5_CLK_RST       0xac
#define APBC_PWM6_CLK_RST       0xb0
#define APBC_PWM7_CLK_RST       0xb4
#define APBC_PWM8_CLK_RST       0xb8
#define APBC_PWM9_CLK_RST       0xbc
#define APBC_PWM10_CLK_RST      0xc0
#define APBC_PWM11_CLK_RST      0xc4
#define APBC_PWM12_CLK_RST      0xc8
#define APBC_PWM13_CLK_RST      0xcc
#define APBC_PWM14_CLK_RST      0xd0
#define APBC_PWM15_CLK_RST      0xd4
#define APBC_PWM16_CLK_RST      0xd8
#define APBC_PWM17_CLK_RST      0xdc
#define APBC_PWM18_CLK_RST      0xe0
#define APBC_PWM19_CLK_RST      0xe4
#define APBC_TIMERS2_CLK_RST    0x11c
#define APBC_TIMERS3_CLK_RST    0x120
#define APBC_TIMERS4_CLK_RST    0x124
#define APBC_TIMERS5_CLK_RST    0x128
#define APBC_TIMERS6_CLK_RST    0x12c
#define APBC_TIMERS7_CLK_RST    0x130
#define APBC_CAN2_CLK_RST       0x148
#define APBC_CAN3_CLK_RST       0x14c
#define APBC_CAN4_CLK_RST       0x150
#define APBC_UART10_CLK_RST     0x154
#define APBC_SSP0_CLK_RST       0x158
#define APBC_SSP1_CLK_RST       0x15c
#define APBC_SSPA4_CLK_RST      0x160
#define APBC_SSPA5_CLK_RST      0x164
/* end of APBC register offset */

/* MPMU register offset */
#define MPMU_WDTPCR             0x200
#define MPMU_RIPCCR             0x210
/* end of MPMU register offset */

/* APMU register offset */
#define APMU_CSI_CCIC2_CLK_RES_CTRL     0x24
#define APMU_ISP_CLK_RES_CTRL           0x38
#define APMU_LCD_CLK_RES_CTRL1          0x44
#define APMU_LCD_CLK_RES_CTRL2          0x4c
#define APMU_CCIC_CLK_RES_CTRL          0x50
#define APMU_SDH0_CLK_RES_CTRL          0x54
#define APMU_SDH1_CLK_RES_CTRL          0x58
#define APMU_USB_CLK_RES_CTRL           0x5c
#define APMU_QSPI_CLK_RES_CTRL          0x60
#define APMU_DMA_CLK_RES_CTRL           0x64
#define APMU_AES_CLK_RES_CTRL           0x68
#define APMU_MCB_CLK_RES_CTRL           0x6c
#define APMU_VPU_CLK_RES_CTRL           0xa4
#define APMU_DTC_CLK_RES_CTRL           0xac
#define APMU_GPU_CLK_RES_CTRL           0xcc
#define APMU_SDH2_CLK_RES_CTRL          0xe0
#define APMU_PMUA_MC_CTRL               0xe8
#define APMU_PMU_CC2_AP                 0x100
#define APMU_UCIE_CTRL                  0x11c
#define APMU_AUDIO_CLK_RES_CTRL         0x14c
#define APMU_LCD_CLK_RES_CTRL3          0x26C
#define APMU_UFS_CLK_RES_CTRL           0x268
#define APMU_LCD_CLK_RES_CTRL4          0x270
#define APMU_LCD_CLK_RES_CTRL5          0x274
#define APMU_LCD_EDP_CTRL               0x23c
#define APMU_PCIE_CLK_RES_CTRL_PORTA    0x1F0
#define APMU_PCIE_CLK_RES_CTRL_PORTB    0x1D0
#define APMU_PCIE_CLK_RES_CTRL_PORTC    0x1C8
#define APMU_PCIE_CLK_RES_CTRL_PORTD    0x1E0
#define APMU_PCIE_CLK_RES_CTRL_PORTE    0x1E8
#define APMU_EMAC0_CLK_RES_CTRL         0x3e4
#define APMU_EMAC1_CLK_RES_CTRL         0x3ec
#define APMU_EMAC2_CLK_RES_CTRL         0x248
#define APMU_ESPI_CLK_RES_CTRL          0x240
/* end of APMU register offset */

/* CIUDRAGON register offset */
#define DCIU_DMASYS_S0_RSTN     0x204
#define DCIU_DMASYS_S1_RSTN     0x208
#define DCIU_DMASYS_A0_RSTN     0x20C
#define DCIU_DMASYS_A1_RSTN     0x210
#define DCIU_DMASYS_A2_RSTN     0x214
#define DCIU_DMASYS_A3_RSTN     0x218
#define DCIU_DMASYS_A4_RSTN     0x21C
#define DCIU_DMASYS_A5_RSTN     0x220
#define DCIU_DMASYS_A6_RSTN     0x224
#define DCIU_DMASYS_A7_RSTN     0x228
#define DCIU_DMASYS_RSTN        0x22C
#define DCIU_DMASYS_SDMA_RSTN   0x230
/* end of CIUDRAGON register offset */

/* APBC2 register offset */
#define APBC2_UART1_CLK_RST     0x00
#define APBC2_SSP2_CLK_RST      0x04
#define APBC2_TWSI3_CLK_RST     0x08
#define APBC2_RTC_CLK_RST       0x0c
#define APBC2_TIMERS_CLK_RST    0x10
#define APBC2_JTAG_SW_CLK_RST   0x18
#define APBC2_GPIO_CLK_RST      0x1c
/* end of APBC2 register offset */

/* RCPU register offset */
//0xc088c000
#define RCPU5_CLK_RST_OFFSET    0xC000
#define RCPU5_RT24_CORE0_SW_RESET    (0xCC + RCPU5_CLK_RST_OFFSET)
#define RCPU5_RT24_CORE1_SW_RESET    (0xD0 + RCPU5_CLK_RST_OFFSET)
/* end of RCPU register offset */

enum spacemit_reset_base_type {
	RST_BASE_TYPE_MPMU       = 0,
	RST_BASE_TYPE_APMU       = 1,
	RST_BASE_TYPE_APBC       = 2,
	RST_BASE_TYPE_APBS       = 3,
	RST_BASE_TYPE_CIU        = 4,
	RST_BASE_TYPE_DCIU       = 5,
	RST_BASE_TYPE_DDRC       = 6,
	RST_BASE_TYPE_APBC2      = 7,
	RST_BASE_TYPE_RCPU       = 8,
};

struct spacemit_reset_signal {
	u32 offset;
	u32 mask;
	u32 deassert_val;
	u32 assert_val;
	enum spacemit_reset_base_type type;
};

struct spacemit_reset {
	void __iomem *mpmu_base;
	void __iomem *apmu_base;
	void __iomem *apbc_base;
	void __iomem *apbs_base;
	void __iomem *ciu_base;
	void __iomem *dciu_base;
	void __iomem *ddrc_base;
	void __iomem *apbc2_base;
	void __iomem *rcpu_base;
	const struct spacemit_reset_signal *signals;
};

struct spacemit_reset k3_reset_controller;

#ifdef CONFIG_SPL_BUILD
static const struct spacemit_reset_signal
	k3_reset_signals[RESET_NUMBER_SPL] = {
	[RESET_SDH_AXI_SPL]     = { APMU_SDH0_CLK_RES_CTRL, BIT(0), BIT(0), 0, RST_BASE_TYPE_APMU },
	[RESET_SDH0_SPL]        = { APMU_SDH0_CLK_RES_CTRL, BIT(1), BIT(1), 0, RST_BASE_TYPE_APMU },
	[RESET_SDH2_SPL]        = { APMU_SDH2_CLK_RES_CTRL, BIT(1), BIT(1), 0, RST_BASE_TYPE_APMU },
	[RESET_QSPI_SPL]        = { APMU_QSPI_CLK_RES_CTRL, BIT(1), BIT(1), 0, RST_BASE_TYPE_APMU },
	[RESET_QSPI_BUS_SPL]    = { APMU_QSPI_CLK_RES_CTRL, BIT(0), BIT(0), 0, RST_BASE_TYPE_APMU },
	[RESET_UFS_ACLK_SPL]    = { APMU_UFS_CLK_RES_CTRL, BIT(0), BIT(0), 0, RST_BASE_TYPE_APMU },
	[RESET_USB3_PORTA_SPL]  = { APMU_USB_CLK_RES_CTRL, BIT(5)|BIT(6)|BIT(7), BIT(5)|BIT(6)|BIT(7), 0, RST_BASE_TYPE_APMU },
	[RESET_ESPI_SPL]        = { APMU_ESPI_CLK_RES_CTRL, BIT(0)|BIT(2), BIT(0)|BIT(2), 0, RST_BASE_TYPE_APMU },
};

static u32 transfer_to_spl_list[][2] = {
	{RESET_SDH_AXI, RESET_SDH_AXI_SPL},
	{RESET_SDH0, RESET_SDH0_SPL},
	{RESET_SDH2, RESET_SDH2_SPL},
	{RESET_QSPI, RESET_QSPI_SPL},
	{RESET_QSPI_BUS, RESET_QSPI_BUS_SPL},
	{RESET_UFS_ACLK, RESET_UFS_ACLK_SPL},
	{RESET_USB3_PORTA, RESET_USB3_PORTA_SPL},
	{RESET_ESPI, RESET_ESPI_SPL},
};

ulong transfer_reset_id_to_spl(ulong id)
{
	u32 listsize = ARRAY_SIZE(transfer_to_spl_list);

	for (int i = 0; i < listsize; i++) {
		if (id == transfer_to_spl_list[i][0]) {
			pr_info("id:%ld, %d,\n", id, transfer_to_spl_list[i][1]);
			return transfer_to_spl_list[i][1];
		}
	}
	return id;
}

static int reset_of_xlate(struct reset_ctl *reset_ctl,
			  struct ofnode_phandle_args *args)
{
	if (args->args_count != 1) {
		debug("Invalid args_count: %d\n", args->args_count);
		return -EINVAL;
	}
	reset_ctl->id = args->args[0];
	reset_ctl->id = transfer_reset_id_to_spl(reset_ctl->id);
	return 0;
}
#else
static const struct spacemit_reset_signal
	k3_reset_signals[RESET_NUMBER] = {
	//APBC
	[RESET_UART0]   = { APBC_UART0_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_UART2]   = { APBC_UART2_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_UART3]   = { APBC_UART3_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_UART4]   = { APBC_UART4_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_UART5]   = { APBC_UART5_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_UART6]   = { APBC_UART6_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_UART7]   = { APBC_UART7_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_UART8]   = { APBC_UART8_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_UART9]   = { APBC_UART9_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_UART10]  = { APBC_UART10_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_GPIO]    = { APBC_GPIO_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_PWM0]    = { APBC_PWM0_CLK_RST, BIT(2)|BIT(0), BIT(0), BIT(2), RST_BASE_TYPE_APBC },
	[RESET_PWM1]    = { APBC_PWM1_CLK_RST, BIT(2)|BIT(0), BIT(0), BIT(2), RST_BASE_TYPE_APBC },
	[RESET_PWM2]    = { APBC_PWM2_CLK_RST, BIT(2)|BIT(0), BIT(0), BIT(2), RST_BASE_TYPE_APBC },
	[RESET_PWM3]    = { APBC_PWM3_CLK_RST, BIT(2)|BIT(0), BIT(0), BIT(2), RST_BASE_TYPE_APBC },
	[RESET_PWM4]    = { APBC_PWM4_CLK_RST, BIT(2)|BIT(0), BIT(0), BIT(2), RST_BASE_TYPE_APBC },
	[RESET_PWM5]    = { APBC_PWM5_CLK_RST, BIT(2)|BIT(0), BIT(0), BIT(2), RST_BASE_TYPE_APBC },
	[RESET_PWM6]    = { APBC_PWM6_CLK_RST, BIT(2)|BIT(0), BIT(0), BIT(2), RST_BASE_TYPE_APBC },
	[RESET_PWM7]    = { APBC_PWM7_CLK_RST, BIT(2)|BIT(0), BIT(0), BIT(2), RST_BASE_TYPE_APBC },
	[RESET_PWM8]    = { APBC_PWM8_CLK_RST, BIT(2)|BIT(0), BIT(0), BIT(2), RST_BASE_TYPE_APBC },
	[RESET_PWM9]    = { APBC_PWM9_CLK_RST, BIT(2)|BIT(0), BIT(0), BIT(2), RST_BASE_TYPE_APBC },
	[RESET_PWM10]   = { APBC_PWM10_CLK_RST, BIT(2)|BIT(0), BIT(0), BIT(2), RST_BASE_TYPE_APBC },
	[RESET_PWM11]   = { APBC_PWM11_CLK_RST, BIT(2)|BIT(0), BIT(0), BIT(2), RST_BASE_TYPE_APBC },
	[RESET_PWM12]   = { APBC_PWM12_CLK_RST, BIT(2)|BIT(0), BIT(0), BIT(2), RST_BASE_TYPE_APBC },
	[RESET_PWM13]   = { APBC_PWM13_CLK_RST, BIT(2)|BIT(0), BIT(0), BIT(2), RST_BASE_TYPE_APBC },
	[RESET_PWM14]   = { APBC_PWM14_CLK_RST, BIT(2)|BIT(0), BIT(0), BIT(2), RST_BASE_TYPE_APBC },
	[RESET_PWM15]   = { APBC_PWM15_CLK_RST, BIT(2)|BIT(0), BIT(0), BIT(2), RST_BASE_TYPE_APBC },
	[RESET_PWM16]   = { APBC_PWM16_CLK_RST, BIT(2)|BIT(0), BIT(0), BIT(2), RST_BASE_TYPE_APBC },
	[RESET_PWM17]   = { APBC_PWM17_CLK_RST, BIT(2)|BIT(0), BIT(0), BIT(2), RST_BASE_TYPE_APBC },
	[RESET_PWM18]   = { APBC_PWM18_CLK_RST, BIT(2)|BIT(0), BIT(0), BIT(2), RST_BASE_TYPE_APBC },
	[RESET_PWM19]   = { APBC_PWM19_CLK_RST, BIT(2)|BIT(0), BIT(0), BIT(2), RST_BASE_TYPE_APBC },
	[RESET_SPI0]    = { APBC_SSP0_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_SPI1]    = { APBC_SSP1_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_SPI3]    = { APBC_SSP3_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_RTC]     = { APBC_RTC_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_TWSI0]   = { APBC_TWSI0_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_TWSI1]   = { APBC_TWSI1_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_TWSI2]   = { APBC_TWSI2_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_TWSI4]   = { APBC_TWSI4_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_TWSI5]   = { APBC_TWSI5_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_TWSI6]   = { APBC_TWSI6_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_TWSI8]   = { APBC_TWSI8_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_TIMERS0] = { APBC_TIMERS0_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_TIMERS1] = { APBC_TIMERS1_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_TIMERS2] = { APBC_TIMERS2_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_TIMERS3] = { APBC_TIMERS3_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_TIMERS4] = { APBC_TIMERS4_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_TIMERS5] = { APBC_TIMERS5_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_TIMERS6] = { APBC_TIMERS6_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_TIMERS7] = { APBC_TIMERS7_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_AIB]     = { APBC_AIB_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_ONEWIRE] = { APBC_ONEWIRE_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_I2S0]    = { APBC_SSPA0_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_I2S1]    = { APBC_SSPA1_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_I2S2]    = { APBC_SSPA2_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_I2S3]    = { APBC_SSPA3_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_I2S4]    = { APBC_SSPA4_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_I2S5]    = { APBC_SSPA5_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_DRO]     = { APBC_DRO_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_IR0]     = { APBC_IR0_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_IR1]     = { APBC_IR1_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_TSEN]    = { APBC_TSEN_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_IPC_AP2AUD]   = { APBC_IPC_AP2AUD_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_CAN0]    = { APBC_CAN0_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_CAN1]    = { APBC_CAN1_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_CAN2]    = { APBC_CAN2_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_CAN3]    = { APBC_CAN3_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	[RESET_CAN4]    = { APBC_CAN4_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC },
	//MPMU
	[RESET_WDT]     = { MPMU_WDTPCR, BIT(2), 0, BIT(2), RST_BASE_TYPE_MPMU },
	[RESET_RIPC]    = { MPMU_RIPCCR, BIT(2), 0, BIT(2), RST_BASE_TYPE_MPMU },
	//APMU
	[RESET_CSI]         = { APMU_CSI_CCIC2_CLK_RES_CTRL, BIT(1), BIT(1), 0, RST_BASE_TYPE_APMU },
	[RESET_CCIC2_PHY]   = { APMU_CSI_CCIC2_CLK_RES_CTRL, BIT(2), BIT(2), 0, RST_BASE_TYPE_APMU },
	[RESET_CCIC3_PHY]   = { APMU_CSI_CCIC2_CLK_RES_CTRL, BIT(29), BIT(29), 0, RST_BASE_TYPE_APMU },
	[RESET_ISP_CIBUS]   = { APMU_ISP_CLK_RES_CTRL, BIT(16), BIT(16), 0, RST_BASE_TYPE_APMU },
	[RESET_DSI_ESC]     = { APMU_LCD_CLK_RES_CTRL1, BIT(3), BIT(3), 0, RST_BASE_TYPE_APMU },
	[RESET_LCD]         = { APMU_LCD_CLK_RES_CTRL1, BIT(4), BIT(4), 0, RST_BASE_TYPE_APMU },
	[RESET_V2D]         = { APMU_LCD_CLK_RES_CTRL1, BIT(27), BIT(27), 0, RST_BASE_TYPE_APMU },
	[RESET_LCD_MCLK]    = { APMU_LCD_CLK_RES_CTRL2, BIT(9), BIT(9), 0, RST_BASE_TYPE_APMU },
	[RESET_LCD_DSCCLK]  = { APMU_LCD_CLK_RES_CTRL2, BIT(15), BIT(15), 0, RST_BASE_TYPE_APMU },
	[RESET_SC2_HCLK]    = { APMU_CCIC_CLK_RES_CTRL, BIT(0), BIT(0), 0, RST_BASE_TYPE_APMU },
	[RESET_CCIC_4X]     = { APMU_CCIC_CLK_RES_CTRL, BIT(1), BIT(1), 0, RST_BASE_TYPE_APMU },
	[RESET_CCIC1_PHY]   = { APMU_CCIC_CLK_RES_CTRL, BIT(2), BIT(2), 0, RST_BASE_TYPE_APMU },
	[RESET_SDH_AXI]     = { APMU_SDH0_CLK_RES_CTRL, BIT(0), BIT(0), 0, RST_BASE_TYPE_APMU },
	[RESET_SDH0]        = { APMU_SDH0_CLK_RES_CTRL, BIT(1), BIT(1), 0, RST_BASE_TYPE_APMU },
	[RESET_SDH1]	    = { APMU_SDH1_CLK_RES_CTRL, BIT(1), BIT(1), 0, RST_BASE_TYPE_APMU },
	[RESET_SDH2]        = { APMU_SDH2_CLK_RES_CTRL, BIT(1), BIT(1), 0, RST_BASE_TYPE_APMU },
	[RESET_USB2]	    = { APMU_USB_CLK_RES_CTRL, BIT(0)|BIT(2)|BIT(3), BIT(0)|BIT(2)|BIT(3), 0, RST_BASE_TYPE_APMU },
	[RESET_USB3_PORTA]  = { APMU_USB_CLK_RES_CTRL, BIT(5)|BIT(6)|BIT(7), BIT(5)|BIT(6)|BIT(7), 0, RST_BASE_TYPE_APMU },
	[RESET_USB3_PORTB]  = { APMU_USB_CLK_RES_CTRL, BIT(9)|BIT(10)|BIT(11), BIT(9)|BIT(10)|BIT(11), 0, RST_BASE_TYPE_APMU },
	[RESET_USB3_PORTC]  = { APMU_USB_CLK_RES_CTRL, BIT(13)|BIT(14)|BIT(15), BIT(13)|BIT(14)|BIT(15), 0, RST_BASE_TYPE_APMU },
	[RESET_USB3_PORTD]  = { APMU_USB_CLK_RES_CTRL, BIT(17)|BIT(18)|BIT(19), BIT(17)|BIT(18)|BIT(19), 0, RST_BASE_TYPE_APMU },
	[RESET_QSPI]        = { APMU_QSPI_CLK_RES_CTRL, BIT(1), BIT(1), 0, RST_BASE_TYPE_APMU },
	[RESET_QSPI_BUS]    = { APMU_QSPI_CLK_RES_CTRL, BIT(0), BIT(0), 0, RST_BASE_TYPE_APMU },
	[RESET_DMA]         = { APMU_DMA_CLK_RES_CTRL, BIT(0), BIT(0), 0, RST_BASE_TYPE_APMU },
	[RESET_AES_WTM]     = { APMU_AES_CLK_RES_CTRL, BIT(4), BIT(4), 0, RST_BASE_TYPE_APMU },
	[RESET_MCB_DCLK]    = { APMU_MCB_CLK_RES_CTRL, BIT(0), BIT(0), 0, RST_BASE_TYPE_APMU },
	[RESET_MCB_ACLK]    = { APMU_MCB_CLK_RES_CTRL, BIT(1), BIT(1), 0, RST_BASE_TYPE_APMU },
	[RESET_VPU]         = { APMU_VPU_CLK_RES_CTRL, BIT(0), BIT(0), 0, RST_BASE_TYPE_APMU },
	[RESET_DTC]         = { APMU_DTC_CLK_RES_CTRL, BIT(0), BIT(0), 0, RST_BASE_TYPE_APMU },
	[RESET_GPU]         = { APMU_GPU_CLK_RES_CTRL, BIT(1), BIT(1), 0, RST_BASE_TYPE_APMU },
	[RESET_MC]          = { APMU_PMUA_MC_CTRL, BIT(0), BIT(0), 0, RST_BASE_TYPE_APMU },
	[RESET_CPU0_POP]    = { APMU_PMU_CC2_AP, BIT(0), 0, BIT(0), RST_BASE_TYPE_APMU },
	[RESET_CPU0_SW]     = { APMU_PMU_CC2_AP, BIT(1), 0, BIT(1), RST_BASE_TYPE_APMU },
	[RESET_CPU1_POP]    = { APMU_PMU_CC2_AP, BIT(3), 0, BIT(3), RST_BASE_TYPE_APMU },
	[RESET_CPU1_SW]     = { APMU_PMU_CC2_AP, BIT(4), 0, BIT(4), RST_BASE_TYPE_APMU },
	[RESET_CPU2_POP]    = { APMU_PMU_CC2_AP, BIT(6), 0, BIT(6), RST_BASE_TYPE_APMU },
	[RESET_CPU2_SW]     = { APMU_PMU_CC2_AP, BIT(7), 0, BIT(7), RST_BASE_TYPE_APMU },
	[RESET_CPU3_POP]    = { APMU_PMU_CC2_AP, BIT(9), 0, BIT(9), RST_BASE_TYPE_APMU },
	[RESET_CPU3_SW]     = { APMU_PMU_CC2_AP, BIT(10), 0, BIT(10), RST_BASE_TYPE_APMU },
	[RESET_C0_MPSUB_SW] = { APMU_PMU_CC2_AP, BIT(12), 0, BIT(12), RST_BASE_TYPE_APMU },
	[RESET_CPU4_POP]    = { APMU_PMU_CC2_AP, BIT(16), 0, BIT(16), RST_BASE_TYPE_APMU },
	[RESET_CPU4_SW]     = { APMU_PMU_CC2_AP, BIT(17), 0, BIT(17), RST_BASE_TYPE_APMU },
	[RESET_CPU5_POP]    = { APMU_PMU_CC2_AP, BIT(19), 0, BIT(19), RST_BASE_TYPE_APMU },
	[RESET_CPU5_SW]     = { APMU_PMU_CC2_AP, BIT(20), 0, BIT(20), RST_BASE_TYPE_APMU },
	[RESET_CPU6_POP]    = { APMU_PMU_CC2_AP, BIT(22), 0, BIT(22), RST_BASE_TYPE_APMU },
	[RESET_CPU6_SW]     = { APMU_PMU_CC2_AP, BIT(23), 0, BIT(23), RST_BASE_TYPE_APMU },
	[RESET_CPU7_POP]    = { APMU_PMU_CC2_AP, BIT(25), 0, BIT(25), RST_BASE_TYPE_APMU },
	[RESET_CPU7_SW]     = { APMU_PMU_CC2_AP, BIT(26), 0, BIT(26), RST_BASE_TYPE_APMU },
	[RESET_C1_MPSUB_SW]      = { APMU_PMU_CC2_AP, BIT(28), 0, BIT(28), RST_BASE_TYPE_APMU },
	[RESET_MPSUB_DBG]        = { APMU_PMU_CC2_AP, BIT(29), 0, BIT(29), RST_BASE_TYPE_APMU },
	[RESET_UCIE]             = { APMU_UCIE_CTRL, BIT(1)|BIT(2)|BIT(3), BIT(1)|BIT(2)|BIT(3), 0, RST_BASE_TYPE_APMU },
	[RESET_RCPU]             = { APMU_AUDIO_CLK_RES_CTRL, BIT(0)|BIT(2)|BIT(3), BIT(0)|BIT(2)|BIT(3), 0, RST_BASE_TYPE_APMU },
	[RESET_DSI4LN2_ESCCLK]   = { APMU_LCD_CLK_RES_CTRL3, BIT(3), BIT(3), 0, RST_BASE_TYPE_APMU },
	[RESET_DSI4LN2_LCD_SW]   = { APMU_LCD_CLK_RES_CTRL3, BIT(4), BIT(4), 0, RST_BASE_TYPE_APMU },
	[RESET_DSI4LN2_LCD_MCLK]   = { APMU_LCD_CLK_RES_CTRL4, BIT(9), BIT(9), 0, RST_BASE_TYPE_APMU },
	[RESET_DSI4LN2_LCD_DSCCLK] = { APMU_LCD_CLK_RES_CTRL4, BIT(15), BIT(15), 0, RST_BASE_TYPE_APMU },
	[RESET_DSI4LN2_DPU_ACLK]   = { APMU_LCD_CLK_RES_CTRL5, BIT(0), BIT(0), 0, RST_BASE_TYPE_APMU },
	[RESET_DPU_ACLK]           = { APMU_LCD_CLK_RES_CTRL5, BIT(15), BIT(15), 0, RST_BASE_TYPE_APMU },
	[RESET_UFS_ACLK]           = { APMU_UFS_CLK_RES_CTRL, BIT(0), BIT(0), 0, RST_BASE_TYPE_APMU },
	[RESET_EDP0]         = { APMU_LCD_EDP_CTRL, BIT(0), BIT(0), 0, RST_BASE_TYPE_APMU },
	[RESET_EDP1]         = { APMU_LCD_EDP_CTRL, BIT(16), BIT(16), 0, RST_BASE_TYPE_APMU },
	[RESET_PCIE_PORTA]   = { APMU_PCIE_CLK_RES_CTRL_PORTA, BIT(3)|BIT(4)|BIT(5), BIT(3)|BIT(4)|BIT(5), 0, RST_BASE_TYPE_APMU },
	[RESET_PCIE_PORTB]   = { APMU_PCIE_CLK_RES_CTRL_PORTB, BIT(3)|BIT(4)|BIT(5), BIT(3)|BIT(4)|BIT(5), 0, RST_BASE_TYPE_APMU },
	[RESET_PCIE_PORTC]   = { APMU_PCIE_CLK_RES_CTRL_PORTC, BIT(3)|BIT(4)|BIT(5), BIT(3)|BIT(4)|BIT(5), 0, RST_BASE_TYPE_APMU },
	[RESET_PCIE_PORTD]   = { APMU_PCIE_CLK_RES_CTRL_PORTD, BIT(3)|BIT(4)|BIT(5), BIT(3)|BIT(4)|BIT(5), 0, RST_BASE_TYPE_APMU },
	[RESET_PCIE_PORTE]   = { APMU_PCIE_CLK_RES_CTRL_PORTE, BIT(3)|BIT(4)|BIT(5), BIT(3)|BIT(4)|BIT(5), 0, RST_BASE_TYPE_APMU },
	[RESET_EMAC0]        = { APMU_EMAC0_CLK_RES_CTRL, BIT(1), BIT(1), 0, RST_BASE_TYPE_APMU },
	[RESET_EMAC1]        = { APMU_EMAC1_CLK_RES_CTRL, BIT(1), BIT(1), 0, RST_BASE_TYPE_APMU },
	[RESET_EMAC2]        = { APMU_EMAC2_CLK_RES_CTRL, BIT(1), BIT(1), 0, RST_BASE_TYPE_APMU },
	[RESET_ESPI]         = { APMU_ESPI_CLK_RES_CTRL, BIT(0)|BIT(2), BIT(0)|BIT(2), 0, RST_BASE_TYPE_APMU },
	//DCIU
	[RESET_HDMA]     = { DCIU_DMASYS_RSTN, BIT(0), BIT(0), 0, RST_BASE_TYPE_DCIU },
	[RESET_DMA350]   = { DCIU_DMASYS_SDMA_RSTN, BIT(0), BIT(0), 0, RST_BASE_TYPE_DCIU },
	[RESET_DMA350_0] = { DCIU_DMASYS_S0_RSTN, BIT(0), BIT(0), 0, RST_BASE_TYPE_DCIU },
	[RESET_DMA350_1] = { DCIU_DMASYS_S1_RSTN, BIT(0), BIT(0), 0, RST_BASE_TYPE_DCIU },
	[RESET_AXIDMA0]  = { DCIU_DMASYS_A0_RSTN, BIT(0), BIT(0), 0, RST_BASE_TYPE_DCIU },
	[RESET_AXIDMA1]  = { DCIU_DMASYS_A1_RSTN, BIT(0), BIT(0), 0, RST_BASE_TYPE_DCIU },
	[RESET_AXIDMA2]  = { DCIU_DMASYS_A2_RSTN, BIT(0), BIT(0), 0, RST_BASE_TYPE_DCIU },
	[RESET_AXIDMA3]  = { DCIU_DMASYS_A3_RSTN, BIT(0), BIT(0), 0, RST_BASE_TYPE_DCIU },
	[RESET_AXIDMA4]  = { DCIU_DMASYS_A4_RSTN, BIT(0), BIT(0), 0, RST_BASE_TYPE_DCIU },
	[RESET_AXIDMA5]  = { DCIU_DMASYS_A5_RSTN, BIT(0), BIT(0), 0, RST_BASE_TYPE_DCIU },
	[RESET_AXIDMA6]  = { DCIU_DMASYS_A6_RSTN, BIT(0), BIT(0), 0, RST_BASE_TYPE_DCIU },
	[RESET_AXIDMA7]  = { DCIU_DMASYS_A7_RSTN, BIT(0), BIT(0), 0, RST_BASE_TYPE_DCIU },
	//APBC2
	[RESET_SEC_UART1]  = { APBC2_UART1_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC2 },
	[RESET_SEC_SSP2]   = { APBC2_SSP2_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC2 },
	[RESET_SEC_TWSI3]  = { APBC2_TWSI3_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC2 },
	[RESET_SEC_RTC]    = { APBC2_RTC_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC2 },
	[RESET_SEC_TIMERS] = { APBC2_TIMERS_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC2 },
	[RESET_SEC_JTAG]   = { APBC2_JTAG_SW_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC2 },
	[RESET_SEC_GPIO]   = { APBC2_GPIO_CLK_RST, BIT(2), 0, BIT(2), RST_BASE_TYPE_APBC2 },
	//RCPU
	//0xc088c000
	[RESET_RCPU5_RT24_CORE0] = { RCPU5_RT24_CORE0_SW_RESET, BIT(0), 0, BIT(0), RST_BASE_TYPE_RCPU },
	[RESET_RCPU5_RT24_CORE1] = { RCPU5_RT24_CORE1_SW_RESET, BIT(0), 0, BIT(0), RST_BASE_TYPE_RCPU },
};
#endif

static u32 spacemit_reset_read(struct spacemit_reset *reset, u32 id)
{
	void __iomem *base;

	switch (reset->signals[id].type) {
	case RST_BASE_TYPE_APMU:
		base = reset->apmu_base;
		break;
	case RST_BASE_TYPE_APBC:
		base = reset->apbc_base;
		break;
	case RST_BASE_TYPE_MPMU:
		base = reset->mpmu_base;
		break;
	case RST_BASE_TYPE_APBS:
		base = reset->apbs_base;
		break;
	case RST_BASE_TYPE_CIU:
		base = reset->ciu_base;
		break;
	case RST_BASE_TYPE_DCIU:
		base = reset->dciu_base;
		break;
	case RST_BASE_TYPE_DDRC:
		base = reset->ddrc_base;
		break;
	case RST_BASE_TYPE_APBC2:
		base = reset->apbc2_base;
		break;
	case RST_BASE_TYPE_RCPU:
		base = reset->rcpu_base;
		break;
	default:
		base = reset->apbc_base;
		break;
	}
	return readl(base + reset->signals[id].offset);
}

static void spacemit_reset_write(struct spacemit_reset *reset, u32 value, u32 id)
{
	void __iomem *base;

	switch (reset->signals[id].type) {
	case RST_BASE_TYPE_APMU:
		base = reset->apmu_base;
		break;
	case RST_BASE_TYPE_APBC:
		base = reset->apbc_base;
		break;
	case RST_BASE_TYPE_MPMU:
		base = reset->mpmu_base;
		break;
	case RST_BASE_TYPE_APBS:
		base = reset->apbs_base;
		break;
	case RST_BASE_TYPE_CIU:
		base = reset->ciu_base;
		break;
	case RST_BASE_TYPE_DCIU:
		base = reset->dciu_base;
		break;
	case RST_BASE_TYPE_DDRC:
		base = reset->ddrc_base;
		break;
	case RST_BASE_TYPE_APBC2:
		base = reset->apbc2_base;
		break;
	case RST_BASE_TYPE_RCPU:
		base = reset->rcpu_base;
		break;
	default:
		base = reset->apbc_base;
		break;
	}

	writel(value, base + reset->signals[id].offset);
}

static void spacemit_reset_set(struct reset_ctl *rst, u32 id, bool assert)
{
	u32 value;
	struct spacemit_reset *reset = dev_get_priv(rst->dev);

	value = spacemit_reset_read(reset, id);
	if (assert == 1) {
		value &= ~reset->signals[id].mask;
		value |= reset->signals[id].assert_val;
	} else {
		value &= ~reset->signals[id].mask;
		value |= reset->signals[id].deassert_val;
	}

	spacemit_reset_write(reset, value, id);
}

static int spacemit_reset_update(struct reset_ctl *rst, bool assert)
{
	if (rst->id < RESET_UART0 || rst->id >= RESET_NUMBER)
		return 0;

	spacemit_reset_set(rst, rst->id, assert);
	return 0;
}

static int spacemit_reset_assert(struct reset_ctl *rst)
{
	return spacemit_reset_update(rst, true);
}

static int spacemit_reset_deassert(struct reset_ctl *rst)
{
	return spacemit_reset_update(rst, false);
}

static int spacemit_k3_reset_probe(struct udevice *dev)
{
	struct spacemit_reset *reset = dev_get_priv(dev);

	reset->mpmu_base = (void __iomem *)dev_remap_addr_index(dev, 0);
	if (!reset->mpmu_base) {
		pr_err("failed to map mpmu registers\n");
		goto out;
	}

	reset->apmu_base = (void __iomem *)dev_remap_addr_index(dev, 1);
	if (!reset->apmu_base) {
		pr_err("failed to map apmu registers\n");
		goto out;
	}

	reset->apbc_base = (void __iomem *)dev_remap_addr_index(dev, 2);
	if (!reset->apbc_base) {
		pr_err("failed to map apbc registers\n");
		goto out;
	}

	reset->apbs_base = (void __iomem *)dev_remap_addr_index(dev, 3);
	if (!reset->apbs_base) {
		pr_err("failed to map apbs registers\n");
		goto out;
	}

	reset->ciu_base = (void __iomem *)dev_remap_addr_index(dev, 4);
	if (!reset->ciu_base) {
		pr_err("failed to map ciu registers\n");
		goto out;
	}

	reset->dciu_base = (void __iomem *)dev_remap_addr_index(dev, 5);
	if (!reset->dciu_base) {
		pr_err("failed to map dragon ciu registers\n");
		goto out;
	}

	reset->ddrc_base = (void __iomem *)dev_remap_addr_index(dev, 6);
	if (!reset->ddrc_base) {
		pr_err("failed to map ddrc registers\n");
		goto out;
	}

	reset->apbc2_base = (void __iomem *)dev_remap_addr_index(dev, 7);
	if (!reset->apbc2_base) {
		pr_err("failed to map apbc2 registers\n");
		goto out;
	}

	reset->rcpu_base = (void __iomem *)dev_remap_addr_index(dev, 8);
	if (!reset->rcpu_base) {
		pr_err("failed to map rcpu registers\n");
		goto out;
	}

	reset->signals = k3_reset_signals;
	pr_info("reset driver probe finish\n");

out:
	return 0;
}

const struct reset_ops k3_reset_ops = {
	.rst_assert = spacemit_reset_assert,
	.rst_deassert = spacemit_reset_deassert,
#ifdef CONFIG_SPL_BUILD
	.of_xlate = reset_of_xlate,
#endif
};

static const struct udevice_id k3_reset_ids[] = {
	{ .compatible = "spacemit,k3-reset", },
	{},
};

U_BOOT_DRIVER(k3_reset) = {
	.name		= "spacemit,k3-reset",
	.id		= UCLASS_RESET,
	.ops		= &k3_reset_ops,
	.of_match	= k3_reset_ids,
	.probe		= spacemit_k3_reset_probe,
	.priv_auto	= sizeof(struct spacemit_reset),
};
