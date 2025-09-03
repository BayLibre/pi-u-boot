/*
 * Copyright(C) 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#ifndef __BAORD_H_
#define __BAORD_H_

/* Boot Sel Register */
#define SOC_OM_ADDRBASE		0xFFEF018010

/* PMP */
#define PMP_BASE_ADDR              0xffdc020000

/* DDR */
#define CFG_SYS_SDRAM_BASE         0x0

/* Rambus efuse security device base addr */
#define TEE_SYS_BASE               0xFFFF200000UL
#define EFUSE_BASE                 0xFFFF210000
#define TEE_SYS_EFUSE_DBG_KEY1_OFF 0x70
#define EIP150B_BASE               0xFFFF300000UL
#define EIP28_BASE                 (EIP150B_BASE + 0x4000)
#define EIP120SI_BASE              0xFFFF310000UL
#define EIP120SII_BASE             0xFFFF320000UL
#define EIP120SIII_BASE            0xFFFF330000UL
#define KEYRAM_BASE                0xFFFF260000UL
#define EFUSE_LC_RMA_UPDATE_OFFSET 0x0

/* WDT RST */
#define AONSYS_RSTGEN_BASE			((void __iomem *)0xFFFFF44000UL)
#define REG_RST_REQ_EN_0			(AONSYS_RSTGEN_BASE + 0x140)
#define WDT0_SYS_RST_REQ			(1 << 8)

/*
 * Board Common interface
 */
int board_get_boot_sel(void);
int board_bootrom_fastboot(void);
#endif
