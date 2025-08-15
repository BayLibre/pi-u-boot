// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2024 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <linux/delay.h>
#include <usb.h>
#include <usb/dwc2_udc.h>
#include <asm/io.h>

/* USB CPR */
#define AP_USB_CPR_BADDR 0x0008000000
#define USB20_PHY_SWRST (AP_USB_CPR_BADDR + 0x118)
#define USB20_0_PHY_RST (1 << 0)
#define USB20_1_PHY_RST (1 << 4)

/* USB20_0 */
#define AP_USB20_SYSREG_BADDR 0x0008300000
#define USB20_0_PHY_CFG (AP_USB20_SYSREG_BADDR + 0x4)
#define USB20_0_PHY_CFG_DM_PULDDOWN (1 << 1)

/* USB20_1 */
#define USB20_1_PHY_CFG (AP_USB20_SYSREG_BADDR + 0x1004)
#define USB20_1_PHY_CFG_DM_PULDDOWN (1 << 3)

#define DWC2_OTG_REG_BADDR 0x0008200000

static struct dwc2_plat_otg_data otg_data = {
	.rx_fifo_sz = 512,
	.np_tx_fifo_sz = 16,
	.tx_fifo_sz = 128,
};

int board_usb_init(int index, enum usb_init_type init)
{
	unsigned int val;

	otg_data.regs_otg = DWC2_OTG_REG_BADDR;

	/* USB 2.0OTG phy reset_n */
	val = readl((void __iomem *)USB20_PHY_SWRST);
	writel(val | USB20_0_PHY_RST | USB20_1_PHY_RST, (void __iomem *)USB20_PHY_SWRST);

	/* USB2_0 PHY CFG, set dm pulldown for HS */
	val = readl((void __iomem *)USB20_0_PHY_CFG);
	writel(val | USB20_0_PHY_CFG_DM_PULDDOWN, (void __iomem *)USB20_0_PHY_CFG);

	/* USB2_1 PHY CFG, set dm pulldown for HS */
	val = readl((void __iomem *)USB20_1_PHY_CFG);
	writel(val | USB20_1_PHY_CFG_DM_PULDDOWN, (void __iomem *)USB20_1_PHY_CFG);

	return dwc2_udc_probe(&otg_data);
}
int board_usb_cleanup(int index, enum usb_init_type init)
{
	return 0;
}
