/*
 * Copyright(C) 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#ifndef _CPU_SS_H_
#define _CPU_SS_H_

// #include <syslog.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

typedef unsigned int u32;

#include "AP_MAP_CPU_SS_BUS_CLK_CCU.h"
#include "AP_MAP_CPU_SS_C908_CPU_CLK_CCU.h"
#include "AP_MAP_CPU_SS_C920_CPU_CLK_CCU.h"
#include "AP_MAP_CPU_SS_SYSREG.h"
#include "AP_MAP_CPU_SS_CPU_APB_CLK_CCU.h"
#include "AP_MAP_CPU_SS_COM_APB_CLK_CCU.h"
#include "AP_MAP_CPU_SS_TOP_TDT_APB_CLK_CCU.h"
#include "AP_MAP_CPU_SS_TOP_NOC_CLK_CCU.h"
#include "AP_MAP_CPU_SS_TOP_GPU_CORE_CLK_CCU.h"
#include "AP_MAP_CPU_SS_TOP_DDR1_AXI_CLK_CCU.h"
#include "AP_MAP_CPU_SS_TOP_DDR0_AXI_CLK_CCU.h"
#include "AP_MAP_CPU_SS_TOP_D2D_AXI_CLK_CCU.h"
#include "AP_MAP_CPU_SS_TOP_CFG_AXI_CLK_CCU.h"
#include "AP_MAP_CPU_SS_PIC_CLK_CCU.h"
#include "AP_MAP_CPU_SS_CFG_AXI_CLK_CCU.h"
#include "AP_MAP_CPU_SS_CLK_SYSREG.h"
#include "AP_MAP_C908_PCTRL_SYSREG.h"
#include "AP_MAP_C920_PCTRL_SYSREG.h"
#include "AP_MAP_CPU_SS_TOP_PPU.h"
#include "AP_MAP_C920_PPU.h"
#include "AP_MAP_C908_PPU.h"
#include "AP_MAP_CPU_SS_TOP_PCU.h"
#include "AP_MAP_C920_PCU.h"
#include "AP_MAP_C908_PCU.h"
#include "AP_MAP_CPU_SS_TOP_BPC.h"
#include "AP_MAP_C920_BPC.h"
#include "AP_MAP_C908_BPC.h"
#include "AP_MAP_CORE_BPC.h"
#include "AP_MAP_CORE_PCU.h"
#include "AP_MAP_CORE_PPU.h"
#include "AP_MAP_CPU_SS_CPU_PLL.h"

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

/* ccu_cfg.c */
void cpu_ss_ccu_init(void);

/* pctrl_cfg.c*/
void cpuss_bpc_init(void);
void cpuss_pcu_init(void);
void cpuss_ppu_init(u32 dyn_mode, u32 pm_en);
void core_bpc_init(int core_id);
void core_pcu_init(int core_id);
void core_ppu_init(int core_id, u32 dyn_mode, u32 pm_en);
void core_ppu_cfg(int core_id, power_mode pm);
void c908_bpc_init(void);
void c908_pcu_init(void);
void c908_ppu_init(u32 dyn_mode, u32 pm_en);
void c908_ppu_cfg(power_mode pm);
void c920_bpc_init(void);
void c920_pcu_init(void);
void c920_ppu_init(u32 dyn_mode, u32 pm_en);
void c920_ppu_cfg(power_mode pm);
void cluster_ppu_init(int cluster_id, u32 dyn_mode, u32 pm_en);

/* pll_cfg.c */
void cpuss_c908_pll_cfg(u32 freq);
void cpuss_c920_pll_cfg(u32 freq);

/*
 *PPU & CCU Mode Config
 */
/* CCU_DLY_EXTREME/CCU_DLY_NORMAL/CCU_OFF */
#define CCU_MODE CCU_DLY_NORMAL
/* PPU_DLY_EXTREME/PPU_DLY_NORMAL */
#define PPU_MODE PPU_DLY_NORMAL

#define CCU_DLY_EXTREME 1
#define CCU_DLY_NORMAL 2
#define CCU_OFF 3
#define PPU_DLY_EXTREME 1
#define PPU_DLY_NORMAL 2

/* SS CCU Mode */
#if CCU_MODE == CCU_DLY_NORMAL
#define NPU_CCU_DLY_TIME 0x5E
#define NPU_CCU_DLY_TIME_STEP 0x3
#define VI_CCU_DLY_TIME 0x5E
#define VI_CCU_DLY_TIME_STEP 0x3
#define VO_CCU_DLY_TIME 0x5E
#define VO_CCU_DLY_TIME_STEP 0x3
#define VP_CCU_DLY_TIME 0x5E
#define VP_CCU_DLY_TIME_STEP 0x3
#define USB_CCU_DLY_TIME 0x5E
#define USB_CCU_DLY_TIME_STEP 0x3
#define PCIE_CCU_DLY_TIME 0x5E
#define PCIE_CCU_DLY_TIME_STEP 0x3
#define DDR_CCU_DLY_TIME 0x5E
#define DDR_CCU_DLY_TIME_STEP 0x3
#define TEE_CCU_DLY_TIME 0x5E
#define TEE_CCU_DLY_TIME_STEP 0x3
#define GPU_CCU_DLY_TIME 0x34
#define GPU_CCU_DLY_TIME_STEP 0x2
#define D2D_CCU_DLY_TIME 0x5E
#define D2D_CCU_DLY_TIME_STEP 0x3
#define D2D_WRAP_CCU_DLY_TIME 0xFF
#define D2D_WRAP_CCU_DLY_TIME_STEP 0x3
#define CPU_CCU_DLY_TIME 0x34
#define CPU_CCU_DLY_TIME_STEP 0x2

#elif CCU_MODE == CCU_DLY_EXTREME
#define NPU_CCU_DLY_TIME 0x1
#define NPU_CCU_DLY_TIME_STEP 0x1
#define VI_CCU_DLY_TIME 0x1
#define VI_CCU_DLY_TIME_STEP 0x1
#define VO_CCU_DLY_TIME 0x1
#define VO_CCU_DLY_TIME_STEP 0x1
#define VP_CCU_DLY_TIME 0x1
#define VP_CCU_DLY_TIME_STEP 0x1
#define USB_CCU_DLY_TIME 0x1
#define USB_CCU_DLY_TIME_STEP 0x1
#define PCIE_CCU_DLY_TIME 0x1
#define PCIE_CCU_DLY_TIME_STEP 0x1
#define DDR_CCU_DLY_TIME 0x1
#define DDR_CCU_DLY_TIME_STEP 0x1
#define TEE_CCU_DLY_TIME 0x40
#define TEE_CCU_DLY_TIME_STEP 0x1
#define GPU_CCU_DLY_TIME 0x1
#define GPU_CCU_DLY_TIME_STEP 0x1
#define D2D_CCU_DLY_TIME 0x1
#define D2D_CCU_DLY_TIME_STEP 0x1
#define D2D_WRAP_CCU_DLY_TIME 0x1
#define D2D_WRAP_CCU_DLY_TIME_STEP 0x1
#define CPU_CCU_DLY_TIME 0xb
#define CPU_CCU_DLY_TIME_STEP 0x1

#elif CCU_MODE == CCU_OFF
#define CCU_BYPASS
#endif

/* SS PPU Mode */
#if PPU_MODE == PPU_DLY_NORMAL
#define DDR_REG_PST_SWITCH_DLY_TIME_2 0x5161
#define C908_CLUSTER_REG_PST_SWITCH_DLY_TIME_2 0xBB6
#define C920_CLUSTER_REG_PST_SWITCH_DLY_TIME_2 0xBB6
#define CPUSS_CLUSTER_REG_PST_SWITCH_DLY_TIME_2 0xBB6
#elif PPU_MODE == PPU_DLY_EXTREME
#define DDR_REG_PST_SWITCH_DLY_TIME_2 0x1B5
#define C908_CLUSTER_REG_PST_SWITCH_DLY_TIME_2 0x40
#define C920_CLUSTER_REG_PST_SWITCH_DLY_TIME_2 0x40
#define CPUSS_CLUSTER_REG_PST_SWITCH_DLY_TIME_2 0x40
#endif

#endif /* _CPU_SS_H_ */
