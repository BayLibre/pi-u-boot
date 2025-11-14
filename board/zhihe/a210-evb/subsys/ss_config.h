// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#ifndef _SS_CONFIG_H_
#define _SS_CONFIG_H_

typedef enum {
    OFF = 0x0,
    MEM_SD = 0x1,
    MEM_RET = 0x2,
    MEM_SD_ONLY = 0x9,
    MEM_DSLP = 0xa,
    MEM_SLP = 0xb,
    CG = 0xf,
    ON = 0x1f,
    WAIT_OFF = 0x100,
    WAIT_MEM_SD = 0x101,
    WAIT_MEM_RET = 0x102,
    WAIT_MEM_SD_ONLY = 0x109,
    WAIT_MEM_DSLP = 0x10a,
    WAIT_MEM_SLP = 0x10b,
    WAIT_CG = 0x10f,
    WAIT_ON = 0x11f
} power_mode;

#define IOPMP_INDICATOR 0x30846110
#define LP3_INDICATOR 0x30846114
#define STR_INDICATOR 0x30846118
#define SAM_INDICATOR 0x3084611c
#define NPU_INDICATOR 0x30846120
#define AON_RESERVED_REG_10 0x30846138
#define AON_RESERVED_REG_11 0x3084613c
#define AON_RESERVED_REG_12 0x30846140

/* CCU DLY TIMING */
#define NPU_CCU_DLY_TIME     0x5E
#define NPU_CCU_DLY_TIME_STEP     0x3
#define VI_CCU_DLY_TIME     0x5E
#define VI_CCU_DLY_TIME_STEP     0x3
#define VO_CCU_DLY_TIME     0x5E
#define VO_CCU_DLY_TIME_STEP     0x3
#define VP_CCU_DLY_TIME     0x5E
#define VP_CCU_DLY_TIME_STEP     0x3
#define USB_CCU_DLY_TIME     0x5E
#define USB_CCU_DLY_TIME_STEP     0x3
#define PCIE_CCU_DLY_TIME     0x5E
#define PCIE_CCU_DLY_TIME_STEP     0x3
#define DDR_CCU_DLY_TIME     0x5E
#define DDR_CCU_DLY_TIME_STEP     0x3
#define TEE_CCU_DLY_TIME     0x5E
#define TEE_CCU_DLY_TIME_STEP     0x3
#define GPU_CCU_DLY_TIME     0x34
#define GPU_CCU_DLY_TIME_STEP     0x2
#define D2D_CCU_DLY_TIME     0x5E
#define D2D_CCU_DLY_TIME_STEP     0x3
#define D2D_WRAP_CCU_DLY_TIME     0xFF
#define D2D_WRAP_CCU_DLY_TIME_STEP     0x3
#define CPU_CCU_DLY_TIME     0x34
#define CPU_CCU_DLY_TIME_STEP     0x2

/* PPU DLY TIMING */
#define DDR_REG_PST_SWITCH_DLY_TIME_2 0x5161
#define C908_CLUSTER_REG_PST_SWITCH_DLY_TIME_2  0xBB6
#define C920_CLUSTER_REG_PST_SWITCH_DLY_TIME_2  0xBB6
#define CPUSS_CLUSTER_REG_PST_SWITCH_DLY_TIME_2 0xBB6

typedef enum {
    VP_CPR,
    VI_CPR,
    NPU_CPR,
    VO_CPR,
    PERI_CPR,
    PCIE_SATA_CPR,
    USB_CPR,
    TEE_CPR,
    GPU_CPR,
    D2D_CPR,
    D2D_CPU_CPR,
    MAX_CPR,
} ss_contrl;

#define SS_CFG_DEFAULT (1 << PERI_CPR) | (1 << USB_CPR) | (1 << PCIE_SATA_CPR) | (1 << VP_CPR) | (1 << VI_CPR) | (1 << NPU_CPR) | (1 << VO_CPR) | (1 << GPU_CPR)| (1 << TEE_CPR)

#endif /*_SS_CONFIG_H_*/
