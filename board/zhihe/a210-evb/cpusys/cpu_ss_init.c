/*
 * Copyright(C) 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <dm/ofnode.h>
#include "include/cpu_ss.h"
#include "../include/addr_defines.h"
#include "../include/utils/io.h"

/*
 * cpu_ss power init
 */
static void pmic_ctrl_bypass(void)
{
	wr(AON_AON_PMIC_CTRL_BADDR + 0x50, 0x300a3197);
}

static void pca_off(void)
{
	wr(AP_C920_PCA_BADDR + 0x20, 0x0);
	wr(AP_CPU_SS_TOP_MEM_PCA_BADDR + 0x20, 0x0);
	wr(AP_CPU_SS_TOP_LOGIC_PCA_BADDR + 0x20, 0x0);
}

static void cpu_ss_power_init(void)
{
	pmic_ctrl_bypass();
	pca_off();
}

/*
 * cpu_ss pll init
 */
static void c908_pll_init(void)
{
	u32 freq = ofnode_conf_read_int("c908-pll-freq", 1200);
	u32 reg;

	if(freq == 1200)
		return;

	reg = rd(AP_TOP_CRG_BADDR + 0x4);
	reg &= ~0x7;
	reg |= 0x4;
	wr(AP_TOP_CRG_BADDR + 0x4, reg);
	wr(AP_CPU_SS_CLK_SYSREG_C908_CLK_CTRL, 1);
	cpuss_c908_pll_cfg(freq);
	wr(AP_CPU_SS_CLK_SYSREG_C908_CLK_CTRL, 0);
}

static void c920_pll_init(void)
{
	u32 freq = ofnode_conf_read_int("c920-pll-freq", 2040);
	u32 reg;

	if(freq == 2040)
		return;

	reg = rd(AP_TOP_CRG_BADDR + 0x8);
	reg &= ~0x7;
	reg |= 0x4;
	wr(AP_TOP_CRG_BADDR + 0x8, reg);
	wr(AP_CPU_SS_CLK_SYSREG_C920_CLK_CTRL, 1);
	cpuss_c920_pll_cfg(freq);
	wr(AP_CPU_SS_CLK_SYSREG_C920_CLK_CTRL, 0);
}

static void cpu_ss_pll_init(void)
{
	c908_pll_init();
	c920_pll_init();
}

/*
 * cpu_ss pctrl init
 */
#define PM_ENABLE 0x79
#define PM_DSLP 0x10
static void cpu_ss_pctrl_init(void)
{
	cpuss_bpc_init();
	cpuss_pcu_init();
	cpuss_ppu_init(0, PM_DSLP);

	c908_bpc_init();
	c908_pcu_init();
	c908_ppu_init(0, PM_DSLP);
	c908_ppu_cfg(ON);
	for (u32 id = 0; id < 4; id++) {
		core_bpc_init(id);
		core_pcu_init(id);
		core_ppu_init(id, 0, PM_DSLP);
		core_ppu_cfg(id, ON);
	}

	c920_bpc_init();
	c920_pcu_init();
	c920_ppu_init(0, PM_DSLP);
	c920_ppu_cfg(ON);

	for (u32 id = 4; id < 8; id++) {
		core_bpc_init(id);
		core_pcu_init(id);
		core_ppu_init(id, 0, PM_DSLP);
		core_ppu_cfg(id, ON);
	}

	if (ofnode_conf_read_int("cpu-ppu-dyn", 0) == 1) {
		printf("908/920 cluster set to dynamic mode\n");
		/* 908/920 cluster set to dynamic mode. dynamic mode only available at cluster level. */
		cluster_ppu_init(0, 1, PM_DSLP);
		cluster_ppu_init(1, 1, PM_DSLP);
		cpuss_ppu_init(1, PM_DSLP);
	}

	/* Enable C920 Jtag */
	wr(AP_C920_BPC_BPC_SW_SLEEP_USER, 0x00);
	wr(AP_CORE_BPC_SW_SLEEP_USER(3), 0x00);
}

void cpu_freq_banner(void)
{
	debug("c908 target freq=%dMhz, c920 targe freq=%dMhz\n",
		ofnode_conf_read_int("c908-pll-freq", 1200) /
		ofnode_conf_read_int("c908-ccu-div", 1),
		ofnode_conf_read_int("c920-pll-freq", 2040) /
		ofnode_conf_read_int("c920-ccu-div", 1));
	wr(AP_CPU_SS_SYSREG_CLK_MON_CTRL, AP_CPU_SS_SYSREG_CLK_MON_C908);
	wr(AP_CPU_SS_SYSREG_CLK_MON_CTRL, AP_CPU_SS_SYSREG_CLK_MON_C908 | AP_CPU_SS_SYSREG_CLK_MON_ENABLE);
	mdelay(2);
	printf("c908 freq=%dMhz, ", rd(AP_CPU_SS_SYSREG_CLK_FREQ_STS) / 1000); 
	wr(AP_CPU_SS_SYSREG_CLK_MON_CTRL, AP_CPU_SS_SYSREG_CLK_MON_C920);
	wr(AP_CPU_SS_SYSREG_CLK_MON_CTRL, AP_CPU_SS_SYSREG_CLK_MON_C920 | AP_CPU_SS_SYSREG_CLK_MON_ENABLE);
	mdelay(2);
	printf("c920 freq=%dMhz\n", rd(AP_CPU_SS_SYSREG_CLK_FREQ_STS) / 1000);
}

/*
 * cpu_ss main init
 */
int cpu_ss_init(void)
{
	cpu_ss_power_init();
	cpu_ss_pctrl_init();
	cpu_ss_pll_init();
	cpu_ss_ccu_init();
	cpu_freq_banner();
	return 0;
}
