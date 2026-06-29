/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Spacemit DesignWare based PCIe host controller driver.
 *
 * Copyright 2025, spacemit Corporation.
 *
 */

#ifndef _PCIE_DW_SPACEMIT_H_
#define _PCIE_DW_SPACEMIT_H_

enum dw_pcie_device_mode {
	DW_PCIE_UNKNOWN_TYPE,
	DW_PCIE_EP_TYPE,
	DW_PCIE_RC_TYPE,
};

#define PCIE_LINK_UP_TIMEOUT_MS		1000

#define PCIE_VENDORID_MASK	GENMASK(15, 0)
#define PCIE_DEVICEID_SHIFT	16
#define SPACEMIT_PCIE_VENDOR_ID	0x201F
#define SPACEMIT_PCIE_DEVICE_ID	0x0002

#define	PCIECTRL_SPACEMIT_CONF_DEVICE_CMD		0x0000
#define	LTSSM_EN					BIT(6)
/* Perst input value in ep mode */
#define	PCIE_PERST_IN					BIT(7)
#define	PCIE_AUX_PWR_DET				BIT(9)
#define	APP_HOLD_PHY_RST				BIT(30)
/* BIT31 0: EP, 1: RC*/
#define	DEVICE_TYPE_RC					BIT(31)

#define	PCIE_CTRL_LOGIC					0x0004
#define	PCIE_IGNORE_PERSTN				BIT(31)
/* Perst GPIO en in RC mode 1: perst# low, 0: perst# high */
#define	PCIE_PERSTN_OE					BIT(24)
#define PCIE_PERSTN_OUT					BIT(25)

#define	SPACEMIT_PHY_AHB_LINK_STS			0x0004
#define	SMLH_LINK_UP					BIT(1)
#define	RDLH_LINK_UP					BIT(12)
#define   PCIE_CLIENT_DEBUG_LTSSM_MASK			GENMASK(11, 6)
#define   PCIE_CLIENT_DEBUG_LTSSM_L1			(BIT(10) | BIT(8))
#define   PCIE_CLIENT_DEBUG_LTSSM_L2			(BIT(10) | BIT(8) | BIT(6))

#define GEN3_EQ_CONTROL_OFF				0x8a8

#define PHY_NUMS	6

/* Port A Modes */
#define PORTA_MODE_MASK		(BIT(4) | BIT(3))
#define PORTA_MODE_X8		(0)				/* [4:3] = 00b */
#define PORTA_MODE_X4		(BIT(4))			/* [4:3] = 10b */
#define PORTA_MODE_X2_PORTB_X2	(BIT(4) | BIT(3))		/* [4:3] = 11b */

/* Port C Modes */
#define PORTC_LANE_MASK		(BIT(4) | BIT(2) | BIT(1))
#define PORTC_MODE_X2		(BIT(4) | (0))			/* [4:1] = 1x00b */
#define PORTC_MODE_X1_PHY2	(BIT(4) | BIT(1))		/* [4:1] = 1x01b */
#define PORTC_MODE_X1_PHY3	(BIT(4) | BIT(2))		/* [4:1] = 1x10b */

/* Port D Modes */
#define PORTD_LANE_MASK		(BIT(4) | BIT(0))
#define PORTD_MODE_PCIE		(BIT(4) | (0))			/* [4:0] = 1xxx0b */

/* Port E Modes */
#define PORTE_LANE_MASK		(BIT(4))
#define PORTE_MODE_PCIE		(BIT(4))			/* [4:0] = 1xxxxb */

#endif
