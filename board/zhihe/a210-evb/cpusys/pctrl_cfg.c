/*
 * Copyright(C) 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include "include/cpu_ss.h"
#include "../include/addr_defines.h"
#include "../include/utils/io.h"

static void c908_ppu_wait_done(void);
static void c920_ppu_wait_done(void);
static void core_ppu_wait_done(int core_id);
static void cpuss_ppu_wait_done(void);

//bpc
void c908_bpc_init(void)
{
    chip_wr(AP_C908_BPC_BPC_SW_CTR, 0x0);
}

void c920_bpc_init(void)
{
    chip_wr(AP_C920_BPC_BPC_SW_CTR, 0x0);
}

void cpuss_bpc_init(void)
{
    chip_wr(AP_CPU_SS_TOP_BPC_BPC_SW_CTR, 0x0);
}

void core_bpc_init(int core_id)
{
    chip_wr(AP_CORE_BPC_SW_CTR(core_id), 0x0);
}

static void c908_bpc_crg_sw_release(void)
{
    chip_wr(AP_C908_BPC_BPC_SW_CTR_OPT, 0x18);
}

static void c920_bpc_crg_sw_release(void)
{
    chip_wr(AP_C920_BPC_BPC_SW_CTR_OPT, 0x18);
}

static void cpuss_bpc_crg_sw_release(void)
{
    chip_wr(AP_CPU_SS_TOP_BPC_BPC_SW_CTR_OPT, 0x18);
}

void core_bpc_crg_sw_release(int core_id)
{
    chip_wr(AP_CORE_BPC_SW_CTR_OPT(core_id), 0x18);
}

//pcu
void c908_pcu_init(void)
{
    chip_wr(AP_C908_PCU_PCU_DEVICE_ENABLE_LOW2HIGH, 0x1);
}

void c920_pcu_init(void)
{
    chip_wr(AP_C920_PCU_PCU_DEVICE_ENABLE_LOW2HIGH, 0x1);
}

void cpuss_pcu_init(void)
{
    chip_wr(AP_CPU_SS_TOP_PCU_PCU_DEVICE_ENABLE_LOW2HIGH, 0x1);
}

void core_pcu_init(int core_id)
{
    chip_wr(AP_CORE_PCU_DEVICE_ENABLE_LOW2HIGH(core_id), 0x1);
}

//ppu
void c908_ppu_init(u32 dyn_mode, u32 pm_en)
{
    if (dyn_mode == 0) {
        chip_wr(AP_C908_PPU_PPU_CTRL, (chip_rd(AP_C908_PPU_PPU_CTRL) & (~0xFFU)) | dyn_mode);
        chip_wr(AP_C908_PPU_PPU_CTRL, (chip_rd(AP_C908_PPU_PPU_CTRL) & (~0xFF00U)) | (pm_en << 8));
    } else {
        chip_wr(AP_C908_PPU_PPU_DLY_TIME_2, C908_CLUSTER_REG_PST_SWITCH_DLY_TIME_2);
        chip_wr(AP_C908_PPU_PPU_CTRL, (chip_rd(AP_C908_PPU_PPU_CTRL) & (~0xFF00U)) | (pm_en << 8));
        chip_wr(AP_C908_PPU_PPU_CTRL, (chip_rd(AP_C908_PPU_PPU_CTRL) & (~0xFFU)) | dyn_mode);
    }
}

void c920_ppu_init(u32 dyn_mode, u32 pm_en)
{
    if (dyn_mode == 0) {
        chip_wr(AP_C920_PPU_PPU_CTRL, (chip_rd(AP_C920_PPU_PPU_CTRL) & (~0xFFU)) | dyn_mode);
        chip_wr(AP_C920_PPU_PPU_CTRL, (chip_rd(AP_C920_PPU_PPU_CTRL) & (~0xFF00U)) | (pm_en << 8));
    } else {
        chip_wr(AP_C920_PPU_PPU_DLY_TIME_2, C920_CLUSTER_REG_PST_SWITCH_DLY_TIME_2);
        chip_wr(AP_C920_PPU_PPU_CTRL, (chip_rd(AP_C920_PPU_PPU_CTRL) & (~0xFF00U)) | (pm_en << 8));
        chip_wr(AP_C920_PPU_PPU_CTRL, (chip_rd(AP_C920_PPU_PPU_CTRL) & (~0xFFU)) | dyn_mode);
    }
}

void cpuss_ppu_init(u32 dyn_mode, u32 pm_en)
{
    if (dyn_mode == 0) {
        chip_wr(AP_CPU_SS_TOP_PPU_PPU_CTRL, (chip_rd(AP_CPU_SS_TOP_PPU_PPU_CTRL) & (~0xFFU)) | dyn_mode);
        chip_wr(AP_CPU_SS_TOP_PPU_PPU_CTRL,
           (chip_rd(AP_CPU_SS_TOP_PPU_PPU_CTRL) & (~0xFF00U)) | (pm_en << 8));
    } else {
        chip_wr(AP_CPU_SS_TOP_PPU_PPU_DLY_TIME_2, CPUSS_CLUSTER_REG_PST_SWITCH_DLY_TIME_2);
        chip_wr(AP_CPU_SS_TOP_PPU_PPU_CTRL,
           (chip_rd(AP_CPU_SS_TOP_PPU_PPU_CTRL) & (~0xFF00U)) | (pm_en << 8));
        chip_wr(AP_CPU_SS_TOP_PPU_PPU_CTRL, (chip_rd(AP_CPU_SS_TOP_PPU_PPU_CTRL) & (~0xFFU)) | dyn_mode);
    }
}

void core_ppu_init(int core_id, u32 dyn_mode, u32 pm_en)
{
    chip_wr(AP_CORE_PPU_CTRL(core_id),
       (chip_rd(AP_CORE_PPU_CTRL(core_id)) & (~0xFFFFU)) | (pm_en << 8) | dyn_mode);
}

void cluster_ppu_init(int cluster_id, u32 dyn_mode, u32 pm_en)
{
    switch (cluster_id) {
    case 0:
        c908_ppu_init(dyn_mode, pm_en);
        break;
    case 1:
        c920_ppu_init(dyn_mode, pm_en);
        break;
    }
}

void c908_ppu_cfg(power_mode pm)
{
    chip_wr(AP_C908_PPU_PPU_SW_STATE, pm);
    chip_wr(AP_C908_PPU_PPU_SW_REQ, 0x1);
    if ((pm & 0x100) != 0x100)
        c908_ppu_wait_done();
}


void c920_ppu_cfg(power_mode pm)
{
    chip_wr(AP_C920_PPU_PPU_SW_STATE, pm);
    chip_wr(AP_C920_PPU_PPU_SW_REQ, 0x1);
    if ((pm & 0x100) != 0x100)
        c920_ppu_wait_done();
}

void cpuss_ppu_cfg(power_mode pm)
{
    chip_wr(AP_CPU_SS_TOP_PPU_PPU_SW_STATE, pm);
    chip_wr(AP_CPU_SS_TOP_PPU_PPU_SW_REQ, 0x1);
    if ((pm & 0x100) != 0x100)
        cpuss_ppu_wait_done();
}

void core_ppu_cfg(int core_id, power_mode pm)
{
    chip_wr(AP_CORE_PPU_SW_STATE(core_id), pm);
    chip_wr(AP_CORE_PPU_SW_REQ(core_id), 0x1);
    if ((pm & 0x100) != 0x100)
        core_ppu_wait_done(core_id);
}

void cluster_ppu_cfg(int cluster_id, power_mode pm)
{
    switch (cluster_id) {
    case 0:
        c908_ppu_cfg(pm);
        break;
    case 1:
        c920_ppu_cfg(pm);
        break;
    }
}

static void c908_ppu_wait_done(void)
{
    u32 r2p_sts;
    while (1) {
        r2p_sts = chip_rd(AP_C908_PPU_PPU_SW_R2P) & (0x3);
        if (r2p_sts == 1) {
            continue;
        } else if (r2p_sts == 2) {
            break;
        } else if (r2p_sts == 3) {
            printf("ERROR: c908 PPU deny");
            break;
        }
    }
}

static void c920_ppu_wait_done(void)
{
    u32 r2p_sts;
    while (1) {
        r2p_sts = chip_rd(AP_C920_PPU_PPU_SW_R2P) & (0x3);
        if (r2p_sts == 1) {
            continue;
        } else if (r2p_sts == 2) {
            break;
        } else if (r2p_sts == 3) {
            printf("ERROR: c920 PPU deny");
            break;
        }
    }
}

static void cpuss_ppu_wait_done(void)
{
    u32 r2p_sts;
    while (1) {
        r2p_sts = chip_rd(AP_CPU_SS_TOP_PPU_PPU_SW_R2P) & (0x3);
        if (r2p_sts == 1) {
            continue;
        } else if (r2p_sts == 2) {
            break;
        } else if (r2p_sts == 3) {
            printf("ERROR: cpuss PPU deny");
            break;
        }
    }
}

void core_ppu_wait_done(int core_id)
{
    u32 r2p_sts;
    while (1) {
        r2p_sts = chip_rd(AP_CORE_PPU_SW_R2P(core_id)) & (0x3);
        if (r2p_sts == 1) {
            continue;
        } else if (r2p_sts == 2) {
            break;
        } else if (r2p_sts == 3) {
            printf("ERROR: core %d PPU deny", core_id);
            break;
        }
    }
}

void c908_flush_l2cache(void)
{
    chip_wr(AP_C908_PCTRL_SYSREG_PCTRL_MODE, 0x201);
    chip_wr(AP_C908_PCTRL_SYSREG_PCTRL_CTRL, 0x1);
    while (1) {
        if (chip_rd(AP_C908_PCTRL_SYSREG_PCTRL_L2_CACHE_STS) == 1) {
            break;
        }
        printf("wait c908 flush done->1");
    }
    chip_wr(AP_C908_PCTRL_SYSREG_PCTRL_CTRL, 0x0);
    while (1) {
        if (chip_rd(AP_C908_PCTRL_SYSREG_PCTRL_L2_CACHE_STS) == 0) {
            break;
        }
        printf("wait c908 flush done->0");
    }
}

void c920_flush_l2cache(void)
{
    chip_wr(AP_C920_PCTRL_SYSREG_PCTRL_MODE, 0x201);
    chip_wr(AP_C920_PCTRL_SYSREG_PCTRL_CTRL, 0x1);
    while (1) {
        if (chip_rd(AP_C920_PCTRL_SYSREG_PCTRL_L2_CACHE_STS) == 1) {
            break;
        }
        printf("wait c920 flush done->1");
    }
    chip_wr(AP_C920_PCTRL_SYSREG_PCTRL_CTRL, 0x0);
    while (1) {
        if (chip_rd(AP_C920_PCTRL_SYSREG_PCTRL_L2_CACHE_STS) == 0) {
            break;
        }
        printf("wait c920 flush done->0");
    }
}

void cluster_flush_l2cache(int cluster_id)
{
    switch (cluster_id) {
    case 0:
        c908_flush_l2cache();
        break;
    case 1:
        c920_flush_l2cache();
        break;
    }
}
