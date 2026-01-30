/*
 * Copyright(C) 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <dm/ofnode.h>
#include "include/cpu_ss.h"
#include "../include/addr_defines.h"
#include "../include/utils/io.h"

#if 0
static void cpu_ss_ccu_ctrl_bypass(u32 addr, u32 bypass)
{
	chip_wr(addr, (chip_rd(addr) & (~(1 << 0))) | (bypass));
}

static void cpu_ss_ccu_ctrl_muxsel(u32 addr, u32 muxsel)
{
	chip_wr(addr, (chip_rd(addr) & (~(1 << 1))) | (muxsel << 1));
}

static void cpu_ss_ccu_ctrl_cgen(u32 addr, u32 cgen)
{
	chip_wr(addr, (chip_rd(addr) & (~(1 << 2))) | (cgen << 2));
}

static void cpu_ss_ccu_ctrl_mode(u32 addr, u32 mode)
{
	chip_wr(addr, (chip_rd(addr) & (~(1 << 3))) | (mode << 3));
}

static void cpu_ss_ccu_ctrl_auto_gate_func_en(u32 addr, u32 auto_gate_func_en)
{
	chip_wr(addr, (chip_rd(addr) & (~(1 << 4))) | (auto_gate_func_en << 4));
}

static void cpu_ss_sw_clk_gate_cfg(u32 cgen)
{
	cpu_ss_ccu_ctrl_cgen(AP_CPU_SS_BUS_CLK_CCU_CCU_CTRL, cgen);
	cpu_ss_ccu_ctrl_cgen(AP_CPU_SS_C908_CPU_CLK_CCU_CCU_CTRL, cgen);
	cpu_ss_ccu_ctrl_cgen(AP_CPU_SS_C920_CPU_CLK_CCU_CCU_CTRL, cgen);
	cpu_ss_ccu_ctrl_cgen(AP_CPU_SS_CFG_AXI_CLK_CCU_CCU_CTRL, cgen);
	cpu_ss_ccu_ctrl_cgen(AP_CPU_SS_COM_APB_CLK_CCU_CCU_CTRL, cgen);
	cpu_ss_ccu_ctrl_cgen(AP_CPU_SS_CPU_APB_CLK_CCU_CCU_CTRL, cgen);
	cpu_ss_ccu_ctrl_cgen(AP_CPU_SS_PIC_CLK_CCU_CCU_CTRL, cgen);
	cpu_ss_ccu_ctrl_cgen(AP_CPU_SS_TOP_CFG_AXI_CLK_CCU_CCU_CTRL, cgen);
	cpu_ss_ccu_ctrl_cgen(AP_CPU_SS_TOP_D2D_AXI_CLK_CCU_CCU_CTRL, cgen);
	cpu_ss_ccu_ctrl_cgen(AP_CPU_SS_TOP_DDR0_AXI_CLK_CCU_CCU_CTRL, cgen);
	cpu_ss_ccu_ctrl_cgen(AP_CPU_SS_TOP_DDR1_AXI_CLK_CCU_CCU_CTRL, cgen);
	cpu_ss_ccu_ctrl_cgen(AP_CPU_SS_TOP_GPU_CORE_CLK_CCU_CCU_CTRL, cgen);
	cpu_ss_ccu_ctrl_cgen(AP_CPU_SS_TOP_TDT_APB_CLK_CCU_CCU_CTRL, cgen);
}
#endif

static void cpu_ss_auto_clk_gate_cfg(u32 ccu_mode)
{
#if 0
	//(C_RAND_SEED);
	if (ccu_mode == 1) {
		chip_wr(AP_CPU_SS_BUS_CLK_CCU_CCU_RATIO_LP, rand() % 64);
		chip_wr(AP_CPU_SS_C908_CPU_CLK_CCU_CCU_RATIO_LP, rand() % 16);
		chip_wr(AP_CPU_SS_C920_CPU_CLK_CCU_CCU_RATIO_LP, rand() % 16);
		chip_wr(AP_CPU_SS_CFG_AXI_CLK_CCU_CCU_RATIO_LP, rand() % 31 + 1);
		chip_wr(AP_CPU_SS_COM_APB_CLK_CCU_CCU_RATIO_LP, rand() % 13 + 4);
		chip_wr(AP_CPU_SS_CPU_APB_CLK_CCU_CCU_RATIO_LP, rand() % 29 + 4);
		chip_wr(AP_CPU_SS_PIC_CLK_CCU_CCU_RATIO_LP, rand() % 64);
	}
#endif
	chip_wr(AP_CPU_SS_BUS_CLK_CCU_CCU_CTRL, 0x4 | (ccu_mode << 1));
	chip_wr(AP_CPU_SS_C908_CPU_CLK_CCU_CCU_CTRL, 0x4 | (ccu_mode << 1));
	chip_wr(AP_CPU_SS_C920_CPU_CLK_CCU_CCU_CTRL, 0x4 | (ccu_mode << 1));
	chip_wr(AP_CPU_SS_CFG_AXI_CLK_CCU_CCU_CTRL, 0x4 | (ccu_mode << 1));
	chip_wr(AP_CPU_SS_COM_APB_CLK_CCU_CCU_CTRL, 0x4 | (ccu_mode << 1));
	chip_wr(AP_CPU_SS_CPU_APB_CLK_CCU_CCU_CTRL, 0x4 | (ccu_mode << 1));
	ccu_mode = 1; // force auto freq mode for cpu pic bus
	chip_wr(AP_CPU_SS_PIC_CLK_CCU_CCU_RATIO_LP, 63);
	chip_wr(AP_CPU_SS_PIC_CLK_CCU_CCU_CTRL, ccu_mode << 1);
	//only gating
	chip_wr(AP_CPU_SS_TOP_CFG_AXI_CLK_CCU_CCU_CTRL, 0x4);
	chip_wr(AP_CPU_SS_TOP_D2D_AXI_CLK_CCU_CCU_CTRL, 0x4);
	chip_wr(AP_CPU_SS_TOP_DDR0_AXI_CLK_CCU_CCU_CTRL, 0x4);
	chip_wr(AP_CPU_SS_TOP_DDR1_AXI_CLK_CCU_CCU_CTRL, 0x4);
	chip_wr(AP_CPU_SS_TOP_GPU_CORE_CLK_CCU_CCU_CTRL, 0x4);
	chip_wr(AP_CPU_SS_TOP_NOC_CLK_CCU_CCU_CTRL, 0x4);
	chip_wr(AP_CPU_SS_TOP_TDT_APB_CLK_CCU_CCU_CTRL, 0x4);
}

static void cpu_ss_c908_ccu_ratio_normal_cfg(u32 ratio)
{
	chip_wr(AP_CPU_SS_C908_CPU_CLK_CCU_CCU_RATIO_NORMAL, ratio);
}

static void cpu_ss_c920_ccu_ratio_normal_cfg(u32 ratio)
{
	chip_wr(AP_CPU_SS_C920_CPU_CLK_CCU_CCU_RATIO_NORMAL, ratio);
}

static void cpu_ss_ccu_dly_time(u32 dly_time, u32 dly_time_step)
{
	chip_wr(AP_CPU_SS_BUS_CLK_CCU_CCU_TIME, dly_time_step << 8 | dly_time);
	chip_wr(AP_CPU_SS_C908_CPU_CLK_CCU_CCU_TIME, dly_time_step << 8 | dly_time);
	chip_wr(AP_CPU_SS_C920_CPU_CLK_CCU_CCU_TIME, dly_time_step << 8 | dly_time);
	chip_wr(AP_CPU_SS_CFG_AXI_CLK_CCU_CCU_TIME, dly_time_step << 8 | dly_time);
	chip_wr(AP_CPU_SS_COM_APB_CLK_CCU_CCU_TIME, dly_time_step << 8 | dly_time);
	chip_wr(AP_CPU_SS_CPU_APB_CLK_CCU_CCU_TIME, dly_time_step << 8 | dly_time);
	chip_wr(AP_CPU_SS_PIC_CLK_CCU_CCU_TIME, dly_time_step << 8 | dly_time);
	chip_wr(AP_CPU_SS_TOP_CFG_AXI_CLK_CCU_CCU_TIME, dly_time_step << 8 | dly_time);
	chip_wr(AP_CPU_SS_TOP_D2D_AXI_CLK_CCU_CCU_TIME, dly_time_step << 8 | dly_time);
	chip_wr(AP_CPU_SS_TOP_DDR0_AXI_CLK_CCU_CCU_TIME, dly_time_step << 8 | dly_time);
	chip_wr(AP_CPU_SS_TOP_DDR1_AXI_CLK_CCU_CCU_TIME, dly_time_step << 8 | dly_time);
	chip_wr(AP_CPU_SS_TOP_GPU_CORE_CLK_CCU_CCU_TIME, dly_time_step << 8 | dly_time);
	chip_wr(AP_CPU_SS_TOP_NOC_CLK_CCU_CCU_TIME, dly_time_step << 8 | dly_time);
	chip_wr(AP_CPU_SS_TOP_TDT_APB_CLK_CCU_CCU_TIME, dly_time_step << 8 | dly_time);
}

/*
 * cpu_ss ccu init
 */
void cpu_ss_ccu_init(void)
{
	/* increase frequency by config ccu ratio */
	cpu_ss_c908_ccu_ratio_normal_cfg(ofnode_conf_read_int("c908-ccu-div", 1) - 1);
	cpu_ss_c920_ccu_ratio_normal_cfg(ofnode_conf_read_int("c920-ccu-div", 1) - 1);

	if (ofnode_conf_read_int("cpu-ccu-enable", 0) == 0)
		return;

	chip_wr(AP_CPU_SS_COM_APB_CLK_CCU_CCU_RATIO_LP, 0);
	chip_wr(AP_CPU_SS_COM_APB_CLK_CCU_CCU_RATIO_NORMAL, 0);
	chip_wr(AP_CPU_SS_CPU_APB_CLK_CCU_CCU_RATIO_NORMAL, 0);

	cpu_ss_ccu_dly_time(CPU_CCU_DLY_TIME, CPU_CCU_DLY_TIME_STEP);
	chip_wr(AP_CPU_SS_SYSREG_RESERVED_REG0, 0xFFFFFFFF); // 配置成1就是disable 模块内部的自动gating. 配置成0可以优化功耗.
	cpu_ss_auto_clk_gate_cfg(0);
}
