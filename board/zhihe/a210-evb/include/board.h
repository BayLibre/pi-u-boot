/*
 * Copyright(C) 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#ifndef __BAORD_H_
#define __BAORD_H_

#include <linux/types.h>
#include <stdint.h>
#include "addr_defines.h"
#include "../arch_ext/arch_ext.h"

/* Boot Sel Register */
#define BOOTSEL_REG_ADDR (AP_AON_SYSREG_BADDR + 0x104)
#define BOOTSEL_MCM_REG_ADDR AP_CPU_SS_SAM_BADDR

/* Boot select list */
#define BOOT_SEL_FASTBOOT       0x0
#define BOOT_SEL_EMMC           0x1
#define BOOT_SEL_QSPI1_NOR      0x2
#define BOOT_SEL_QSPI1_NAND     0x3
#define BOOT_SEL_CCTBOOT        0x4
#define BOOT_SEL_SDCARD         0x5
#define BOOT_SEL_QSPI0_NOR      0x6
#define BOOT_SEL_QSPI0_NAND     0x7
#define BOOT_SEL_MCM_FASTBOOT   0x8
#define BOOT_SEL_MCM_RES_9      0x9
#define BOOT_SEL_MCM2_QSPI1_NOR 0xA
#define BOOT_SEL_MCM4_QSPI1_NOR 0xB
#define BOOT_SEL_MCM_CCTBOOT    0xC
#define BOOT_SEL_MCM_RES_D      0xD
#define BOOT_SEL_MCM2_QSPI0_NOR 0xE
#define BOOT_SEL_MCM4_QSPI0_NOR 0xF

/* DDR */
#define CFG_SYS_SDRAM_BASE 0x80000000
#if defined(CONFIG_ZHP100EVB_LP4X_4266_1R_2D) || defined(CONFIG_ZHP100EVB_LP4X_3733_1R_2D) || defined(CONFIG_ZHP100EVB_LP4X_3200_1R_2D)
#define CFG_SYS_SDRAM_SIZE 0x100000000
#elif defined(CONFIG_ZHP100EVB_DDR_DUMMY) || defined(CONFIG_ZHP100EMU_LP4X)
#define CFG_SYS_SDRAM_SIZE 0x100000000
#else
#define CFG_SYS_SDRAM_SIZE 0x80000000
#endif

/* Rambus efuse security device base addr */
#define TEE_SYS_BASE               0x27400000UL
#define EFUSE_BASE                 0x27410000UL
#define TEE_SYS_EFUSE_DBG_KEY1_OFF 0x6c
#define EIP150B_BASE               0x27500000UL
#define EIP28_BASE                 (EIP150B_BASE + 0x4000)
#define EIP120SI_BASE              0x27510000UL
#define EIP120SII_BASE             0x27520000UL
#define EIP120SIII_BASE            0x27530000UL
#define KEYRAM_BASE                0x27460000UL
#define EFUSE_LC_RMA_UPDATE_OFFSET 0x10

/* E902 */
#define E902_SYSREG_RST_ADDR           0x30846024
#define E902_SYSREG_RST_VAL_ENTRY      0x308f8000
#define E902_MAX_TEXT_SIZE             0x00008000

#define E902_RSTGEN_RSTCFG_ADDR        0x30842018
#define E902_RSTGEN_RSTCFG_VAL_RESET   0x00000000
#define E902_RSTGEN_RSTCFG_VAL_DERESET 0x00000003

/* Boot Method */
#ifdef CONFIG_FIT
#define BOOT_METHOD_UNKNOWN 0x0
#define BOOT_METHOD_WITH_FIT 0xfa57b007
#define BOOT_METHOD_RAM_FIT  0x2b0deba9
#define BOOT_METHOD_ADDR (CONFIG_SPL_TEXT_BASE - ZHIHE_PKG_HEAD_SIZE + 60)
#endif

/*
 * Board check
 */
enum board_type {
    BOARD_EVB,
    BOARD_DEV,
    BOARD_EVB_D2D,
    BOARD_UNKNOWN,
};

#define STR_BOARD_EVB "a210-evb"
#define STR_BOARD_DEV "a210-dev"
#define STR_BOARD_EVB_D2D "a210-evb-d2d"

enum ddr_type {
    DDR_4266_1Rank_2GB,
    DDR_4266_1Rank_4GB,
    DDR_4266_2Rank_8GB,
    DDR_UNKNOWN,
};

/*
 * Board Common interface
 */
int board_get_boot_sel(void);
int board_get_die_count(void);
void gpio_pin_init(enum board_type board);
void board_type_check(void);
enum board_type board_get_type(void);
enum ddr_type board_get_ddrtype(void);

/*
 * arch ext bram call
 */
void board_spl_prepare_bram_section(void);

/*
 * Bram call user interface
 */
void board_spl_switch_ddrpll(int speed);

#endif
