// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025, spacemit Corporation.
 *
 */

#ifndef _RESET_SPACEMIT_K3_H_
#define _RESET_SPACEMIT_K3_H_

/* u-boot-spl would used this reset */
enum {
	RESET_SDH_AXI_SPL,
	RESET_SDH0_SPL,
	RESET_SDH2_SPL,
	RESET_QSPI_SPL,
	RESET_QSPI_BUS_SPL,
	RESET_UFS_ACLK_SPL,
	RESET_USB3_PORTA_SPL,
	RESET_ESPI_SPL,
	RESET_NUMBER_SPL,
};

ulong transfer_reset_id_to_spl(ulong id);
#endif /* _RESET_SPACEMIT_K3_H_ */
