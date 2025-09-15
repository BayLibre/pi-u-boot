// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

//#define DEBUG 1

#include <log.h>
#include <dm/ofnode.h>
#include "../include/utils/utils.h"
#include "../include/addr_defines.h"
#include "ss_config.h"
#include "../cmd/ss.h"

#define BPC_SW_MODEL 1
#define BPC_HW_MODEL 0
#define PCU_REG_TRIGGER 1
#define PCU_R2P_TRIGGER 0

#define LP_REG_CCU_TIME_ADDR_OFFSET 0
#define LP_REG_CCU_CTRL_ADDR_OFFSET 0x4
#define LP_REG_CCU_STS_ADDR_OFFSET 0x8
#define LP_REG_CCU_CLR_ADDR_OFFSET 0xc
#define LP_REG_CCU_LP_CTRL_ADDR_OFFSET 0x10
#define LP_REG_CCU_RATIO_LP_ADDR_OFFSET 0x18
#define LP_REG_CCU_RATIO_NORMAL_OFFSET 0x1c
#define LP_REG_CCU_INT_CLR_ADDR_OFFSET 0x24
#define LP_REG_CCU_INT_EN_ADDR_OFFSET 0x28

#define LP_REG_CCU_AUTOGATE_FIELD_OFFSET 0x2
#define LP_REG_CCU_CTRL_MODE_FIELD_OFFSET 0x1
#define LP_REG_CCU_CTRL_BYPASS_FIELD_OFFSET 0x0

//=================================== PPU
#define LP_REG_PPU_SW_REQ_ADDR_OFFSET 0x4
#define LP_REG_PPU_SW_STATE_ADDR_OFFSET 0x8
#define LP_REG_PPU_CTRL_ADDR_OFFSET 0xc
#define LP_REG_PPU_DLY_TIME_CTRL_ADDR_OFFSET 0x14
#define LP_REG_PPU_DLY_TIME_0_ADDR_OFFSET 0x18
#define LP_REG_PPU_DLY_TIME_1_ADDR_OFFSET 0x1c
#define LP_REG_PPU_DLY_TIME_2_ADDR_OFFSET 0x20
#define LP_REG_PPU_INT_CLR_ADDR_OFFSET 0x2c
#define LP_REG_PPU_INT_STS_ADDR_OFFSET 0x30
//=================================== PCU
#define LP_REG_PCU_SW_LPSTATE_ADDR_OFFSET 0xc
#define LP_REG_PCU_SW_LPREQ_ADDR_OFFSET 0x8
#define LP_REG_PCU_IER_ADDR_OFFSET 0x24
#define LP_REG_PCU_ICR_ADDR_OFFSET 0x28
#define LP_REG_PCU_RISR_ADDR_OFFSET 0x2c
#define LP_REG_PCU_ISR_ADDR_OFFSET 0x30
#define LP_REG_PCU_CUR_STATE_OFFSET 0x48
#define LP_REG_PCU_DEVICE_ENABLE_HIGH2LOW_OFFSET 0x6c
#define LP_REG_PCU_DEVICE_ENABLE_LOW2HIGH_OFFSET 0x70

#define PCU_INT_ALL_ON 0b111111
#define PCU_INT_ALL_CLR 0b111111

#define emu_trigger_cmd() printf("%c%c%c\n", 0xDE, 0xAD, 0xBE);

// #define D2D_DEBUG_SS_MT
// #define D2D_DEBUG_SS_REG

void lp_ccu_reg_init_with_gating(u64 base_addr, u32 reg_dly_time, u32 reg_dly_time_step)
{
	u32 data;

	//1.open interrupt
	wr(base_addr + LP_REG_CCU_INT_EN_ADDR_OFFSET, 1);
	//2.open auto clock gating
	data = rd(base_addr + LP_REG_CCU_CTRL_ADDR_OFFSET);
	data = data | (1 << LP_REG_CCU_AUTOGATE_FIELD_OFFSET);
	wr(base_addr + LP_REG_CCU_CTRL_ADDR_OFFSET, data);
	wr(base_addr + LP_REG_CCU_TIME_ADDR_OFFSET,
	   (reg_dly_time << 0 | reg_dly_time_step << 8 | 0x8 << 16));
	//3.mode: 0:gating 1:frequency is reduced
	data = data & 0xfffffffd;
	wr(base_addr + LP_REG_CCU_CTRL_ADDR_OFFSET, data);
	//4.change to hw mode
	data = data & 0xfffffffe;
	wr(base_addr + LP_REG_CCU_CTRL_ADDR_OFFSET, data);
}

void venc_cclk_config(unsigned int freq)
{
	unsigned int tmp;
	switch (freq) {
	case 528:
		tmp = rd(AP_TOP_CRG_BADDR + 0x38);
		tmp &= ~0xF3;
		wr(AP_TOP_CRG_BADDR + 0x38, tmp | 0x51);
		break;
	case 600:
		tmp = rd(AP_TOP_CRG_BADDR + 0x38);
		tmp &= ~0xF3;
		wr(AP_TOP_CRG_BADDR + 0x38, tmp | 0x50);
		break;
	default:
		printf("wrong freq for %s!\n", __func__);
		break;
	}
	debug("%s %dMHz\n", __func__, freq);
	return;
}

void vdec_cclk_config(unsigned int freq)
{
	unsigned int tmp;
	switch (freq) {
	case 660:
		tmp = rd(AP_TOP_CRG_BADDR + 0x38);
		tmp &= ~0xF300;
		wr(AP_TOP_CRG_BADDR + 0x38, tmp | 0x4100);
		break;
	case 786:
		tmp = rd(AP_TOP_CRG_BADDR + 0x38);
		tmp &= ~0xF300;
		wr(AP_TOP_CRG_BADDR + 0x38, tmp | 0x3000);
		break;
	default:
		printf("wrong freq for %s!\n", __func__);
		break;
	}
	debug("%s %dMHz\n", __func__, freq);
	return;
}

void vp_aclk_config(unsigned int freq)
{
	unsigned int tmp;
	switch (freq) {
	case 786:
		tmp = rd(AP_TOP_CRG_BADDR + 0x38);
		tmp &= ~0x1F000000;
		wr(AP_TOP_CRG_BADDR + 0x38, tmp | 0x13000000);
		break;
	case 880:
		tmp = rd(AP_TOP_CRG_BADDR + 0x38);
		tmp &= ~0x1F000000;
		wr(AP_TOP_CRG_BADDR + 0x38, tmp | 0x3000000);
		break;
	default:
		printf("wrong freq for %s!\n", __func__);
		break;
	}
	debug("%s %dMHz\n", __func__, freq);
	return;
}

void g2d_cclk_config(unsigned int freq)
{
	unsigned int tmp;
	switch (freq) {
	case 660:
		tmp = rd(AP_TOP_CRG_BADDR + 0x38);
		tmp &= ~0xF10000;
		wr(AP_TOP_CRG_BADDR + 0x38, tmp | 0x420000);
		break;
	case 786:
		tmp = rd(AP_TOP_CRG_BADDR + 0x38);
		tmp &= ~0xF10000;
		wr(AP_TOP_CRG_BADDR + 0x38, tmp | 0x300000);
		break;
	default:
		printf("wrong freq for %s!\n", __func__);
		break;
	}
	debug("%s %dMHz\n", __func__, freq);
	return;
}

void gpu_cclk_config(unsigned int freq)
{
	unsigned int tmp;
	switch (freq) {
	case 660:
		tmp = rd(AP_TOP_CRG_BADDR + 0x18);
		tmp &= ~0xF3;
		wr(AP_TOP_CRG_BADDR + 0x18, tmp | 0x41);
		break;
	case 792:
		tmp = rd(AP_TOP_CRG_BADDR + 0x18);
		tmp &= ~0xF3;
		wr(AP_TOP_CRG_BADDR + 0x18, tmp | 0x33);
		break;
	default:
		printf("wrong freq for %s!\n", __func__);
		break;
	}
	debug("%s %dMHz\n", __func__, freq);
	return;
}

void npu_cclk_config(unsigned int freq)
{
	unsigned int tmp;
	switch (freq) {
	case 330:
		tmp = rd(AP_TOP_CRG_BADDR + 0x3c);
		tmp &= ~0xF3;
		wr(AP_TOP_CRG_BADDR + 0x3c, tmp | 0x81);
		break;
	case 660:
		tmp = rd(AP_TOP_CRG_BADDR + 0x3c);
		tmp &= ~0xF3;
		wr(AP_TOP_CRG_BADDR + 0x3c, tmp | 0x41);
		break;
	case 786:
		tmp = rd(AP_TOP_CRG_BADDR + 0x3c);
		tmp &= ~0xF3;
		wr(AP_TOP_CRG_BADDR + 0x3c, tmp | 0x32);
		break;
	case 880:
		tmp = rd(AP_TOP_CRG_BADDR + 0x3c);
		tmp &= ~0xF3;
		wr(AP_TOP_CRG_BADDR + 0x3c, tmp | 0x31);
		break;
	case 1000:
		tmp = rd(AP_TOP_CRG_BADDR + 0x3c);
		tmp &= ~0xF3;
		wr(AP_TOP_CRG_BADDR + 0x3c, tmp | 0x30);
		break;
	default:
		printf("wrong freq for %s!\n", __func__);
		break;
	}
	debug("%s %dMHz\n", __func__, freq);
	return;
}

void npu_aclk_config(unsigned int freq)
{
	unsigned int tmp;
	switch (freq) {
	case 330:
		tmp = rd(AP_TOP_CRG_BADDR + 0x3c);
		tmp &= ~0xF300;
		wr(AP_TOP_CRG_BADDR + 0x3c, tmp | 0x8100);
		break;
	case 660:
		tmp = rd(AP_TOP_CRG_BADDR + 0x3c);
		tmp &= ~0xF300;
		wr(AP_TOP_CRG_BADDR + 0x3c, tmp | 0x4100);
		break;
	case 786:
		tmp = rd(AP_TOP_CRG_BADDR + 0x3c);
		tmp &= ~0xF300;
		wr(AP_TOP_CRG_BADDR + 0x3c, tmp | 0x3200);
		break;
	case 880:
		tmp = rd(AP_TOP_CRG_BADDR + 0x3c);
		tmp &= ~0xF300;
		wr(AP_TOP_CRG_BADDR + 0x3c, tmp | 0x3100);
		break;
	case 1000:
		tmp = rd(AP_TOP_CRG_BADDR + 0x3c);
		tmp &= ~0xF300;
		wr(AP_TOP_CRG_BADDR + 0x3c, tmp | 0x3000);
		break;
	default:
		printf("wrong freq for %s!\n", __func__);
		break;
	}
	debug("%s %dMHz\n", __func__, freq);
	return;
}

void top_crg_pll_config(void)
{
	wr(AP_TOP_CRG_BADDR + 0x0, 0x2300202); // TOP_ONLY_CLK_CFG
	wr(AP_TOP_CRG_BADDR + 0x4, rd(AP_TOP_CRG_BADDR + 0x4) | (0x3 << 20)); // CPU_CLK_CFG_0
	wr(AP_TOP_CRG_BADDR + 0x8, rd(AP_TOP_CRG_BADDR + 0x8) | (0x1 << 16)); // CPU_CLK_CFG_1 clkgen_sys_bus_clk=1.32g

	wr(AP_TOP_CRG_BADDR + 0x18, (rd(AP_TOP_CRG_BADDR + 0x18) & 0xfffff0ff) | (0x4 << 8)); // GPU_CLK_CFG
	wr(AP_TOP_CRG_BADDR + 0x14, rd(AP_TOP_CRG_BADDR + 0x14) | (0x3 << 4)); // D2D_CLK_CFG

	wr(AP_TOP_CRG_BADDR + 0x20, (rd(AP_TOP_CRG_BADDR + 0x20) & 0xfff8ffff) | (0x2 << 16)); // PERI_CLK_CFG1
	wr(AP_TOP_CRG_BADDR + 0x2c, rd(AP_TOP_CRG_BADDR + 0x2c) | (0x1 << 20)); // PERI_CLK_CFG4
	wr(AP_TOP_CRG_BADDR + 0x28, (rd(AP_TOP_CRG_BADDR + 0x28) & 0xf7ffff00) | (0x63)); // PERI_CLK_CFG3

	wr(AP_TOP_CRG_BADDR + 0x34, rd(AP_TOP_CRG_BADDR + 0x34) | (0x2 << 8)); // USB_CLK_CFG

	udelay(1);

	wr(AP_PLL_WRAP_BADDR + 0x140, 0x1106e01); // VIDEO_PLL_CFG0
	wr(AP_PLL_WRAP_BADDR + 0x144, 0x1000000); // VIDEO_PLL_CFG1
	wr(AP_PLL_WRAP_BADDR + 0x40, 0x1107d01); // GMAC_PLL_CFG0
	wr(AP_PLL_WRAP_BADDR + 0x44, 0x1000000); // GMAC_PLL_CFG1
	wr(AP_PLL_WRAP_BADDR + 0x0, 0x1106201); // AUDIO0_PLL_CFG0
	wr(AP_PLL_WRAP_BADDR + 0x4, 0x4aaaab); //AUDIO0_PLL_CFG1
	wr(AP_PLL_WRAP_BADDR + 0x20, 0x1106901); //AUDIO1_PLL_CFG0
	wr(AP_PLL_WRAP_BADDR + 0x24, 0x555555); //AUDIO1_PLL_CFG1

	venc_cclk_config(600); // 528 600 MHz
	vdec_cclk_config(786); // 660 786 MHz
	vp_aclk_config(880); // 786 880 MHz
	g2d_cclk_config(786); // 786 660 MHz
	gpu_cclk_config(792); // 792 660 MHz
	npu_cclk_config(1000); // 786 880 1000 MHz
	npu_aclk_config(1000); // 786 880 1000 MHz
}

void bpc_config(char *str, u32 base_addr, u32 bpc_ctrl)
{
	if ((bpc_ctrl & (1 << 0)) != 0) {
		debug("Enter %s bpc_config sw model...\n", str);
		wr(base_addr + 0x000, 0x1); // 0x1 bypass
		wr(base_addr + 0x13c, 0x18); // bpc 9000 ocgen &rset
	} else {
		debug("Enter %s bpc_config hw model...\n", str);
		wr(base_addr + 0x000, 0x0);
	}
	wr(base_addr + 0x004, 0x10101); // pwr venc bpc 3000| fence
}

void pcu_intr(char *str, u32 base_addr)
{
	u32 data;
	udelay(1);
	data = rd(base_addr + 0x2c); // read pcu intr
	while (data == 0) {
		udelay(1);
		data = rd(base_addr + 0x2c); // read pcu intr
	}
	if (((data & (1 << 0)) != 0) || ((data & (1 << 3)) != 0)) {
		debug("%s pcu_intr accept\n", str);
	}
	if (((data & (1 << 1)) != 0) || ((data & (1 << 4)) != 0)) {
		debug("%s pcu_intr deny\n", str);
	}
	if (((data & (1 << 2)) != 0) || ((data & (1 << 5)) != 0)) {
		debug("%s pcu_intr timeout\n", str);
	}
	wr(base_addr + 0x28, data); // clr cpu intr
}

void pcu_config(char *str, u32 base_addr, u32 pcu_ctrl, u32 state)
{
	debug("Enter %s pcu_config intr enable...\n", str);
	wr(base_addr + 0x24, 0x3f); // interrupt enable
	if ((pcu_ctrl & (1 << 0)) != 0) {
		debug("Enter %s pcu_config: pcu reg trigger...\n", str);
		wr(base_addr + 0x0c, (state & 0x1f)); // lpstate = power on
		wr(base_addr + 0x08, 0x1); // lqreq
		pcu_intr(str, base_addr); // wait for accept
	} else {
		debug("Enter %s pcu_config: wait r2p trigger...\n", str);
	}
}

void vpss_r2p_intr(u32 r2p_ctrl)
{
	u32 data;
	if ((r2p_ctrl & (1 << 0)) == 0) {
		udelay(1);
		data = rd(AP_VP_PTRL_R2P_BADDR + 0x1c); // rd r2p intr
		while (data == 0) {
			udelay(1);
			data = rd(AP_VP_PTRL_R2P_BADDR + 0x1c); // rd r2p intr
		}
		if ((data & (1 << 0)) != 0) {
			debug("vpss_r2p_intr accept\n");
		}
		if ((data & (1 << 1)) != 0) {
			debug("vpss_r2p_intr deny\n");
		}
		if ((data & (1 << 2)) != 0) {
			printf("vpss_r2p_intr timeout\n");
		}
		wr(AP_VP_PTRL_R2P_BADDR + 0x14, data); // clr pwr pcu intr
	} else {
		debug("vpss_r2p intr bypass: pcu reg trigger...\n");
	}
}

void vpss_r2p_low_power_config(u32 r2p_ctrl, u32 pctrl_en)
{
	int order;
	//srand((unsigned int)time(NULL));
	//order = rand() % 2;
	order = 0;
	debug("Enter vpss_r2p_config: intr enable...\n");
	wr(AP_VP_PTRL_R2P_BADDR + 0x10, 0x7); // r2p intr en
	if ((r2p_ctrl & (1 << 0)) == 0) {
		wr(AP_VP_PTRL_R2P_BADDR + 0x08,
		   (0x00001f << 4) | ((pctrl_en & 0x7) << 1) | (order & 0x1)); // r2p
		wr(AP_VP_PTRL_R2P_BADDR + 0x04, 0x01);
		if ((pctrl_en & 0x7) == 0x0 || (pctrl_en & 0x7) == 0x2 || (pctrl_en & 0x7) == 0x4 ||
			(pctrl_en & 0x7) == 0x6) {
			debug(
				"Enter vpss_r2p_config: r2p need set: trigger pwr pcu ctrl_en | pctrl_en='h%x, order='h%x\n",
				pctrl_en, order);
		} else if ((pctrl_en & 0x7) == 0x1) {
			debug(
				"Enter vpss_r2p_config: pwr pcu ctrl_en, venc&vdec ctrl_dis | pctrl_en='h%x, order='h%x\n",
				pctrl_en, order);
			pcu_intr("vp_wrap_pctrl", AP_VP_PTRL_VP_WRAP_PTRL_PCU_BADDR);
		} else if ((pctrl_en & 0x7) == 0x3) {
			debug(
				"Enter vpss_r2p_config: pwr&venc pcu ctrl_en,vdec ctrl_dis | pctrl_en='h%x, order='h%x\n",
				pctrl_en, order);
			pcu_intr("vp_venc_pctrl", AP_VP_PTRL_VENC_PTRL_PCU_BADDR);
			pcu_intr("vp_wrap_pctrl", AP_VP_PTRL_VP_WRAP_PTRL_PCU_BADDR);
		} else if ((pctrl_en & 0x7) == 0x5) {
			debug(
				"Enter vpss_r2p_config: pwr&vdec pcu ctrl_en,venc ctrl_dis | pctrl_en='h%x, order='h%x\n",
				pctrl_en, order);
			pcu_intr("vp_vdec_pctrl", AP_VP_PTRL_VDEC_PTRL_PCU_BADDR);
			pcu_intr("vp_wrap_pctrl", AP_VP_PTRL_VP_WRAP_PTRL_PCU_BADDR);
		} else if ((pctrl_en & 0x7) == 0x7) {
			debug("Enter vpss_r2p_config: pwr&venc&vdec pcu ctrl_en ||pctrl_en='h%x, order='h%x\n",
				   pctrl_en, order);
			pcu_intr("vp_vdec_pctrl", AP_VP_PTRL_VDEC_PTRL_PCU_BADDR);
			pcu_intr("vp_venc_pctrl", AP_VP_PTRL_VENC_PTRL_PCU_BADDR);
			pcu_intr("vp_wrap_pctrl", AP_VP_PTRL_VP_WRAP_PTRL_PCU_BADDR);
		}
		debug("Enter vpss_r2p_config: r2p trigger pcu...\n");
	} else {
		debug("vpss_r2p_config bypass: pcu reg trigger...\n");
	}
	vpss_r2p_intr(r2p_ctrl);
}

void vpss_low_power_config(u32 pcu, u32 bpc, power_mode enable)
{
	if (ON == enable) {
		bpc_config("vp_wrap_bpc", AP_VP_PTRL_VP_WRAP_PTRL_BPC_BADDR, bpc);
		pcu_config("vp_wrap_pcu", AP_VP_PTRL_VP_WRAP_PTRL_PCU_BADDR, pcu, enable);
		bpc_config("vp_venc_bpc", AP_VP_PTRL_VENC_PTRL_BPC_BADDR, bpc);
		pcu_config("vp_venc_pcu", AP_VP_PTRL_VENC_PTRL_PCU_BADDR, pcu, enable);
		bpc_config("vp_vdec_bpc", AP_VP_PTRL_VDEC_PTRL_BPC_BADDR, bpc);
		pcu_config("vp_vdec_pcu", AP_VP_PTRL_VDEC_PTRL_PCU_BADDR, pcu, enable);
		vpss_r2p_low_power_config(pcu, 0x7);
	} else {
		bpc_config("vp_venc_bpc", AP_VP_PTRL_VENC_PTRL_BPC_BADDR, bpc);
		pcu_config("vp_venc_pcu", AP_VP_PTRL_VENC_PTRL_PCU_BADDR, pcu, enable);
		bpc_config("vp_vdec_bpc", AP_VP_PTRL_VDEC_PTRL_BPC_BADDR, bpc);
		pcu_config("vp_vdec_pcu", AP_VP_PTRL_VDEC_PTRL_PCU_BADDR, pcu, enable);
		bpc_config("vp_wrap_bpc", AP_VP_PTRL_VP_WRAP_PTRL_BPC_BADDR, bpc);
		pcu_config("vp_wrap_pcu", AP_VP_PTRL_VP_WRAP_PTRL_PCU_BADDR, pcu, enable);
		vpss_r2p_low_power_config(pcu, 0x7);
	}
}

void viss_low_power_config(u32 pcu, u32 bpc, power_mode enable)
{
	if (ON == enable) {
		bpc_config("vi_wrap_bpc", AP_VI_PCTRL_VI_WRAP_PTRL_BPC_BADDR, bpc);
		pcu_config("vi_wrap_pcu", AP_VI_PCTRL_VI_WRAP_PTRL_PCU_BADDR, pcu, enable);
		bpc_config("vi_isp_bpc", AP_VI_PCTRL_ISP_PTRL_BPC_BADDR, bpc);
		pcu_config("vi_isp_pcu", AP_VI_PCTRL_ISP_PTRL_PCU_BADDR, pcu, enable);
		wr(AP_VI_PCTRL_R2P_BADDR + 0x10, 0x7); // r2p intr en
	} else {
		bpc_config("vi_isp_bpc", AP_VI_PCTRL_ISP_PTRL_BPC_BADDR, bpc);
		pcu_config("vi_isp_pcu", AP_VI_PCTRL_ISP_PTRL_PCU_BADDR, pcu, enable);
		bpc_config("vi_wrap_bpc", AP_VI_PCTRL_VI_WRAP_PTRL_BPC_BADDR, bpc);
		pcu_config("vi_wrap_pcu", AP_VI_PCTRL_VI_WRAP_PTRL_PCU_BADDR, pcu, enable);
		wr(AP_VI_PCTRL_R2P_BADDR + 0x10, 0x7); // r2p intr en
	}
}

void voss_low_power_config(u32 pcu, u32 bpc, power_mode enable)
{
	bpc_config("vo_bpc", AP_VO_PCTRL_BPC_BADDR, bpc);
	pcu_config("vo_pcu", AP_VO_PCTRL_PCU_BADDR, pcu, enable);
}

void lp_vp_ss_ccu_init(void)
{
	lp_ccu_reg_init_with_gating(AP_VP_TOP_CCTRL_CFG_CCU_BADDR, VP_CCU_DLY_TIME,
								VP_CCU_DLY_TIME_STEP);
	lp_ccu_reg_init_with_gating(AP_VP_CCTRL_AXI_CCU_BADDR, VP_CCU_DLY_TIME, VP_CCU_DLY_TIME_STEP);
	lp_ccu_reg_init_with_gating(AP_VP_CCTRL_PTW_CCU_BADDR, VP_CCU_DLY_TIME, VP_CCU_DLY_TIME_STEP);
}

void vpss_cpr_init(void)
{
	debug("%s\n", __func__);
	wr(AP_VP_PTRL_PCA_BADDR + 0x20, 0x0); // pca off
	vpss_low_power_config(PCU_REG_TRIGGER, BPC_HW_MODEL, ON);
	wr(AP_VP_SYSREG_BADDR + 0x200, 0xffffffff); // VP_CLK_EN
	wr(AP_VP_SYSREG_BADDR + 0x400, 0xffffffff); // VP_RSTN
	udelay(1);
	//pmic ctrl
	wr(AP_AON_PMIC_CTRL_BADDR + 0x50, rd(AP_AON_PMIC_CTRL_BADDR + 0x50) | 24);
	wr(AP_AON_PMIC_CTRL_BADDR + 0x5c, AON_AON_I2C0_BADDR + 0x04);
	wr(AP_AON_PMIC_CTRL_BADDR + 0x58, AON_AON_I2C0_BADDR + 0x10);
	if (ofnode_conf_read_int("vp-ccu-enable", 0) == 1)
		lp_vp_ss_ccu_init();
}

void vpss_cpr_deinit(void)
{
	debug("%s\n", __func__);
	vpss_low_power_config(PCU_REG_TRIGGER, BPC_HW_MODEL, OFF);
	wr(AP_VP_SYSREG_BADDR + 0x200, 0x0); // VP_CLK_EN
	wr(AP_VP_SYSREG_BADDR + 0x400, 0x0); // VP_RSTN
}

void lp_vi_ss_ccu_init(void)
{
	lp_ccu_reg_init_with_gating(AP_VI_TOP_CCTRL_CFG_CCU_BADDR, VI_CCU_DLY_TIME,
								VI_CCU_DLY_TIME_STEP);
	lp_ccu_reg_init_with_gating(AP_VI_CCTRL_AXI_CCU_BADDR, VI_CCU_DLY_TIME, VI_CCU_DLY_TIME_STEP);
	lp_ccu_reg_init_with_gating(AP_VI_CCTRL_PTW_CCU_BADDR, VI_CCU_DLY_TIME, VI_CCU_DLY_TIME_STEP);
}

void viss_cpr_init(void)
{
	debug("%s\n", __func__);
	viss_low_power_config(PCU_REG_TRIGGER, BPC_HW_MODEL, ON);
	wr(AP_VI_SYSREG_BADDR + 0x200, 0xFFFFFFFF); // VI_CLK_EN
	wr(AP_VI_SYSREG_BADDR + 0x204, 0xFFFFFFFF); // VI_CLK_EN2
	wr(AP_VI_SYSREG_BADDR + 0x50, rd(AP_VI_SYSREG_BADDR + 0x50) | 0x7ff); // isp internal clk
	wr(AP_VI_SYSREG_BADDR + 0x400, 0xFFFFFFFF); // VI_RSTN
	udelay(1);
	if (ofnode_conf_read_int("vi-ccu-enable", 0) == 1)
		lp_vi_ss_ccu_init();
}

void viss_cpr_deinit(void)
{
	debug("%s\n", __func__);
	viss_low_power_config(PCU_REG_TRIGGER, BPC_HW_MODEL, OFF);
	wr(AP_VI_SYSREG_BADDR + 0x200, 0); // VI_CLK_EN
	wr(AP_VI_SYSREG_BADDR + 0x204, 0); // VI_CLK_EN2
	wr(AP_VI_SYSREG_BADDR + 0x400, 0); // VI_RSTN
}

void lp_vo_ss_ccu_init(void)
{
	lp_ccu_reg_init_with_gating(AP_VO_PCTRL_BADDR + 0xf000, VO_CCU_DLY_TIME, VO_CCU_DLY_TIME_STEP);
	lp_ccu_reg_init_with_gating(AP_VO_CCTRL_BADDR + 0x0, VO_CCU_DLY_TIME, VO_CCU_DLY_TIME_STEP);
	lp_ccu_reg_init_with_gating(AP_VO_CCTRL_BADDR + 0x800, VO_CCU_DLY_TIME, VO_CCU_DLY_TIME_STEP);
}

void vo_cpr_init(void)
{
	debug("%s\n", __func__);
	voss_low_power_config(PCU_REG_TRIGGER, BPC_HW_MODEL, ON);
	wr(AP_VO_SYSREG_BADDR + 0x200, 0xFFFFFFFF); // VO_CLK_EN
	wr(AP_VO_SYSREG_BADDR + 0x400, 0xFFFFFFFF); // VO_RSTN
	wr(AP_VO_SYSREG_BADDR + 0x48, 0x4); //dpu port0 link dsi, dpu port1 link hdmi
	udelay(1);
	if (ofnode_conf_read_int("vo-ccu-enable", 0) == 1)
		lp_vo_ss_ccu_init();
}

void vo_cpr_deinit(void)
{
	debug("%s\n", __func__);
	voss_low_power_config(PCU_REG_TRIGGER, BPC_HW_MODEL, OFF);
	wr(AP_VO_SYSREG_BADDR + 0x200, 0x0); // VO_CLK_EN
	wr(AP_VO_SYSREG_BADDR + 0x400, 0x0); // VO_RSTN
}

void npu_low_power_config(u32 pcu, u32 bpc, power_mode enable)
{
	if (rd(NPU_INDICATOR) == 0xdead) {
		debug("npu sramc recovering\n");
		bpc_config("npu_ss_bpc", AP_NPU_PCTRL_BPC_BADDR, BPC_HW_MODEL);
		debug("npu wrapper OFF->ON\n");
		pcu_config("npu_ss_pcu", AP_NPU_PCTRL_PCU_BADDR, pcu, 0x1f);
		debug("npu wrapper ON->ICG\n");
		pcu_config("npu_ss_pcu", AP_NPU_PCTRL_PCU_BADDR, pcu, 0xf);
		wr(0x30846250, 0);
		debug("npu sram iso en = 0\n");
		debug("npu wrapper ICG->ON\n");
		pcu_config("npu_ss_pcu", AP_NPU_PCTRL_PCU_BADDR, pcu, 0x1f);
		debug("npu ip OFF->ON\n");
		bpc_config("npu_ip_bpc", AP_NPU_IP_BPC_BADDR, BPC_HW_MODEL);
		pcu_config("npu_ip_pcu", AP_NPU_IP_PCU_BADDR, pcu, 0x1f);
		wr(0x07112070, 0xffff);
		wr(0x07112074, 0);
		wr(0x0711207c, 1);
		debug("0x30a00000=0x%x\n", rd(0x30a00000));
	} else {
		if (enable == ON) {
			bpc_config("npu_ss_bpc", AP_NPU_PCTRL_BPC_BADDR, bpc);
			pcu_config("npu_ss_pcu", AP_NPU_PCTRL_PCU_BADDR, pcu, enable);
			bpc_config("npu_ip_bpc", AP_NPU_IP_BPC_BADDR, bpc);
			pcu_config("npu_ip_pcu", AP_NPU_IP_PCU_BADDR, pcu, enable);
		} else {
			bpc_config("npu_ip_bpc", AP_NPU_IP_BPC_BADDR, bpc);
			pcu_config("npu_ip_pcu", AP_NPU_IP_PCU_BADDR, pcu, enable);
			bpc_config("npu_ss_bpc", AP_NPU_PCTRL_BPC_BADDR, bpc);
			pcu_config("npu_ss_pcu", AP_NPU_PCTRL_PCU_BADDR, pcu, enable);
		}
	}
}

void lp_npu_ss_ccu_init(void)
{
	lp_ccu_reg_init_with_gating(AP_NPU_PCTRL_CCU_BADDR, NPU_CCU_DLY_TIME, NPU_CCU_DLY_TIME_STEP);
	lp_ccu_reg_init_with_gating(AP_NPU_AXI_CCU_BADDR, NPU_CCU_DLY_TIME, NPU_CCU_DLY_TIME_STEP);
	lp_ccu_reg_init_with_gating(AP_NPU_PTW_CCU_BADDR, NPU_CCU_DLY_TIME, NPU_CCU_DLY_TIME_STEP);
}

void npu_cpr_init(void)
{
	debug("%s\n", __func__);
	wr(AON_NPU_PCTRL_PCA_BADDR + 0x20, 0x0); // pca off
	npu_low_power_config(PCU_REG_TRIGGER, BPC_HW_MODEL, ON);
	//cfg rst/clk register
	wr(AP_NPU_CRG_BADDR + 0x200, 0xffffffff);
	wr(AP_NPU_CRG_BADDR + 0x210, 0xffffffff);
	wr(AP_NPU_CRG_BADDR + 0x074, 0x0000001);
	wr(AP_NPU_CRG_BADDR + 0x07c, 0x0000001);
	if (ofnode_conf_read_int("npu-ccu-enable", 0) == 1)
		lp_npu_ss_ccu_init();
}

void peri_cpr_init(void)
{
	debug("%s\n", __func__);

	wr(AP_PERI0_SYSREG_BADDR + 0x200, 0xffffffff); // PERI0_CLK_EN
	wr(AP_PERI0_SYSREG_BADDR + 0x400, 0xffffffff); // PERI0_RST_N
	wr(AP_PERI1_SYSREG_BADDR + 0x200, 0xffffffff); // PERI1_CLK_EN_0
	wr(AP_PERI1_SYSREG_BADDR + 0x204, 0xffffffff); // PERI1_CLK_EN_1
	wr(AP_PERI1_SYSREG_BADDR + 0x400, 0xffffffff); // PERI1_RST_N_0
	wr(AP_PERI1_SYSREG_BADDR + 0x404, 0xffffffff); // PERI1_RST_N_1
	wr(AP_PERI2_SYSREG_BADDR + 0x200, 0xffffffff); // PERI2_CLK_EN_0
	wr(AP_PERI2_SYSREG_BADDR + 0x204, 0xffffffff); // PERI2_CLK_EN_1
	wr(AP_PERI2_SYSREG_BADDR + 0x400, 0xffffffff); // PERI2_RST_N_0
	wr(AP_PERI2_SYSREG_BADDR + 0x404, 0xffffffff); // PERI2_RST_N_1
	wr(AP_PERI3_SYSREG_BADDR + 0x200, 0xffffffff); // PERI3_CLK_EN
	wr(AP_PERI3_SYSREG_BADDR + 0x400, 0xffffffff); // PERI3_RST_N
	wr(AP_PERI1_SYSREG_BADDR + 0X0, 0x1); // PERI1 I2S0_EN
	wr(AP_PERI2_SYSREG_BADDR + 0X0, 0x15); // PERI2 I2S1/2/3_EN
}

void pcie_low_power_config(u32 pcu, u32 bpc, power_mode enable)
{
	bpc_config("pcie_ctrl0_bpc", AP_PCIE_PCTRL_PCIE_CTRL0_BPC_BADDR, bpc);
	pcu_config("pcie_ctrl0_pcu", AP_PCIE_PCTRL_PCIE_CTRL0_PCU_BADDR, pcu, enable);
	bpc_config("pcie_ctrl1_bpc", AP_PCIE_PCTRL_PCIE_CTRL1_BPC_BADDR, bpc);
	pcu_config("pcie_ctrl1_pcu", AP_PCIE_PCTRL_PCIE_CTRL1_PCU_BADDR, pcu, enable);
	bpc_config("pcie_sata_bpc", AP_PCIE_PCTRL_SATA_CTRL_BPC_BADDR, bpc);
	pcu_config("pcie_sata_pcu", AP_PCIE_PCTRL_SATA_CTRL_PCU_BADDR, pcu, enable);
	if (enable == ON) {
		wr(AP_PERI1_PADCTRL_BADDR + 0x40c, 0x55055555);
		wr(AON_PCIE_CPR_BADDR + 0x0, 0x1111);
		wr(AON_PCIE_CPR_BADDR + 0x4, 0x111);
		wr(AON_PCIE_CPR_BADDR + 0x8, 0x1);
		wr(AON_PCIE_CPR_BADDR + 0x10, 0x110111);
		wr(AON_PCIE_CPR_BADDR + 0x14, 0x110001);
		wr(AON_PCIE_CPR_BADDR + 0x20, 0x111111);
		wr(AON_PCIE_CPR_BADDR + 0x24, 0x111111);
		wr(AON_PCIE_CPR_BADDR + 0x100, 0x1111);
		wr(AON_PCIE_CPR_BADDR + 0x104, 0x11);
		wr(AON_PCIE_CPR_BADDR + 0x108, 0x10);
		wr(AON_PCIE_CPR_BADDR + 0x110, 0x111111);
		wr(AON_PCIE_CPR_BADDR + 0x114, 0x1111);
		wr(AON_PCIE_CPR_BADDR + 0x120, 0x110011);
		wr(AON_PCIE_CPR_BADDR + 0x124, 0x110011);
	}
}

void lp_pcie_ss_ccu_init(void)
{
	lp_ccu_reg_init_with_gating(AP_PCIE_PCTRL_APB_CFG_CCU_BADDR, PCIE_CCU_DLY_TIME,
								PCIE_CCU_DLY_TIME_STEP);
	lp_ccu_reg_init_with_gating(AP_PCIE_CCTRL_AXI_CCU_BADDR, PCIE_CCU_DLY_TIME,
								PCIE_CCU_DLY_TIME_STEP);
	lp_ccu_reg_init_with_gating(AP_PCIE_CCTRL_PTW_CCU_BADDR, PCIE_CCU_DLY_TIME,
								PCIE_CCU_DLY_TIME_STEP);
}

void pcie_cpr_init(void)
{
	debug("%s\n", __func__);

	wr(AP_AON_PADCTRL_BADDR + 0x414, (rd(AP_AON_PADCTRL_BADDR + 0x414) & 0xffffff0f) | (0x1 << 4));
	wr(AP_PERI1_PADCTRL_BADDR + 0x404,
	   (rd(AP_PERI1_PADCTRL_BADDR + 0x404) & 0xfff0000f) | (0x5555 << 4));
	wr(AP_PERI1_PADCTRL_BADDR + 0x40c,
	   (rd(AP_PERI1_PADCTRL_BADDR + 0x40c) & 0xffff) | (0x5555 << 16));

	wr(AP_PCIE_PCTRL_E16PHY0_PCA_BADDR + 0x20, 0x0); // pca off
	wr(AP_PCIE_PCTRL_E16PHY1_PCA_BADDR + 0x20, 0x0); // pca off
	pcie_low_power_config(PCU_REG_TRIGGER, BPC_HW_MODEL, ON);
	if (ofnode_conf_read_int("pcie-ccu-enable", 0) == 1)
		lp_pcie_ss_ccu_init();
}

void usb_low_power_config(u32 pcu, u32 bpc, power_mode enable)
{
	bpc_config("usb_bpc", AP_USB_PCTRL_PCU_BADDR + 0x200, bpc);
	pcu_config("usb_pcu", AP_USB_PCTRL_PCU_BADDR, pcu, enable);
}

void lp_usb_ss_ccu_init(void)
{
	debug("lp_usb_ss_ccu_init\n");
	lp_ccu_reg_init_with_gating(AP_USB_PCTRL_CFG_CCU_BADDR, USB_CCU_DLY_TIME,
								USB_CCU_DLY_TIME_STEP);
	lp_ccu_reg_init_with_gating(AP_USB_CTRL_AXI_CCU_BADDR, USB_CCU_DLY_TIME, USB_CCU_DLY_TIME_STEP);
	lp_ccu_reg_init_with_gating(AP_USB_CCTRL_PTW_CCU_BADDR, USB_CCU_DLY_TIME,
								USB_CCU_DLY_TIME_STEP);
}

void usb_cpr_init(unsigned int ss_cfg)
{
	debug("%s\n", __func__);
	usb_low_power_config(PCU_REG_TRIGGER, BPC_HW_MODEL, ON);
	udelay(1);
	wr(AP_USB_CPR_BADDR + 0x4, 0xffffffff);
	wr(AP_USB_CPR_BADDR + 0x8, 0xffffffff);
	wr(AP_USB_CPR_BADDR + 0xc, 0xffffffff);
	wr(AP_USB_CPR_BADDR + 0x104, 0xffffffff);
	wr(AP_USB_CPR_BADDR + 0x108, 0xffffffff);
	wr(AP_USB_CPR_BADDR + 0x10c, 0xffffffff);
	if (ss_cfg & (1 << PERI_CPR)) {
		wr(AP_USB_CPR_BADDR + 0x20, 0x1); // PERI2 CLK_EN on
		wr(AP_USB_CPR_BADDR + 0x120, 0x1); // PERI2 SWRST on
	}
	if (ofnode_conf_read_int("usb-ccu-enable", 0) == 1)
		lp_usb_ss_ccu_init();
}

void aon_low_power_config(u32 pcu, u32 bpc)
{
	bpc_config("aon_bpc", AP_AON_PCTRL_BPC_BADDR, bpc);
	wr(AP_AON_PCTRL_PCU_BADDR + 0x6c, 0); // LP_REG_PCU_DEVICE_ENABLE_HIGH2LOW_OFFSET on -> off
	wr(AP_AON_PCTRL_PCU_BADDR + 0x70, 0); // LP_REG_PCU_DEVICE_ENABLE_LOW2HIGH_OFFSET
	pcu_config("aon_pcu", AP_AON_PCTRL_PCU_BADDR, pcu, 0x1f);
}

void open_tee_axibus_clk_gate(void)
{
	uint32_t tmp;
	tmp = rd(AP_TEE_SYSREG_BADDR + 0x1c);
	tmp &= (~0x1);
	wr(AP_TEE_SYSREG_BADDR + 0x1c, tmp);
}
void tee_cpr_init(void)
{
	lp_ccu_reg_init_with_gating(AP_TEE_CCTRL_CCU_BADDR, TEE_CCU_DLY_TIME, TEE_CCU_DLY_TIME_STEP);
	open_tee_axibus_clk_gate();
}

int lp_ddr_ss_ppu_init_by_addr(u64 base_addr, u32 reg_pst_switch_dly_time_2)
{
	u32 data;

	//1. wr pm enable
	data = rd(base_addr + LP_REG_PPU_CTRL_ADDR_OFFSET);
	data = (data | 0x1000) & 0xffffb0ff; // DSLP
	wr(base_addr + LP_REG_PPU_CTRL_ADDR_OFFSET, data);
	//2. wr time
	// wr(base_addr+LP_REG_PPU_DLY_TIME_CTRL_ADDR_OFFSET,0x0); // reg_pst_switch_dly_time_step_2=0x1
	// wr(base_addr+LP_REG_PPU_DLY_TIME_0_ADDR_OFFSET,0x55);
	// wr(base_addr+LP_REG_PPU_DLY_TIME_1_ADDR_OFFSET,0x55);
	wr(base_addr + LP_REG_PPU_DLY_TIME_2_ADDR_OFFSET, reg_pst_switch_dly_time_2);
	//3. wr dyn mode
	data = rd(base_addr + LP_REG_PPU_CTRL_ADDR_OFFSET);
	data = data | 0x1;
	wr(base_addr + LP_REG_PPU_CTRL_ADDR_OFFSET, data);

	return 1;
}

int lp_ddr_ss_ccu_init_by_addr(u64 base_addr, u32 reg_dly_time, u32 reg_dly_time_step)
{
	u32 data;

	//	for(int i = 0;i < LP_DDR_SS_CCU_NUM;i++){
	//1.open interrupt
	wr(base_addr + LP_REG_CCU_INT_EN_ADDR_OFFSET, 1);
	//2.open auto clock gating
	data = rd(base_addr + LP_REG_CCU_CTRL_ADDR_OFFSET);
	data = data | (1 << LP_REG_CCU_AUTOGATE_FIELD_OFFSET);
	wr(base_addr + LP_REG_CCU_CTRL_ADDR_OFFSET, data);
	//3. config time
	wr(base_addr + LP_REG_CCU_TIME_ADDR_OFFSET,
	   (reg_dly_time << 0 | reg_dly_time_step << 8 | 0x8 << 16));
	//4.mode: 0:gating 1:frequency is reduced
	data = data & 0xfffffffd; //(0 << LP_REG_CCU_CTRL_MODE_FIELD_OFFSET);
	wr(base_addr + LP_REG_CCU_CTRL_ADDR_OFFSET, data);
	//5.change to hw mode
	data = data & 0xfffffffe; //(0 << LP_REG_CCU_CTRL_BYPASS_FIELD_OFFSET);
	wr(base_addr + LP_REG_CCU_CTRL_ADDR_OFFSET, data);
	//	}

	return 1;
}

void lp_ddr_ss_ccu_init(void)
{
	lp_ddr_ss_ccu_init_by_addr(AP_DDR0_CCU_SLC_BADDR, DDR_CCU_DLY_TIME, DDR_CCU_DLY_TIME_STEP);
	lp_ddr_ss_ccu_init_by_addr(AP_DDR1_CCU_SLC_BADDR, DDR_CCU_DLY_TIME, DDR_CCU_DLY_TIME_STEP);
}

void lp_ddr_ss_pctrl_init(void)
{
	lp_ddr_ss_ppu_init_by_addr(AP_DDR0_PPU_SLC_BADDR,
							   DDR_REG_PST_SWITCH_DLY_TIME_2); // LP_DDR0_SCL_SS
	lp_ddr_ss_ppu_init_by_addr(AP_DDR1_PPU_SLC_BADDR,
							   DDR_REG_PST_SWITCH_DLY_TIME_2); // LP_DDR1_SCL_SS
}

void gpu_low_power_config(u32 pcu, u32 bpc, power_mode enable)
{
	bpc_config("gpu_bpc", AP_GPU_TOP_BPC_BADDR, bpc);
	pcu_config("gpu_pcu", AP_GPU_TOP_PCU_BADDR, pcu, enable);
}

void lp_gpu_ss_ccu_init(void)
{
	lp_ccu_reg_init_with_gating(AP_GPU_TOP_CFG_ACLK_CCU_BADDR, GPU_CCU_DLY_TIME,
								GPU_CCU_DLY_TIME_STEP);
	lp_ccu_reg_init_with_gating(AP_GPU_TOP_ACLK_CCU_BADDR, GPU_CCU_DLY_TIME, GPU_CCU_DLY_TIME_STEP);
	lp_ccu_reg_init_with_gating(AP_GPU_TOP_PCLK_CCU_BADDR, GPU_CCU_DLY_TIME, GPU_CCU_DLY_TIME_STEP);
	lp_ccu_reg_init_with_gating(AP_GPU_ACLK_CCU_BADDR, GPU_CCU_DLY_TIME, GPU_CCU_DLY_TIME_STEP);
	lp_ccu_reg_init_with_gating(AP_GPU_CORE_CLK_CCU_BADDR, GPU_CCU_DLY_TIME, GPU_CCU_DLY_TIME_STEP);
	lp_ccu_reg_init_with_gating(AP_GPU_PCLK_CCU_BADDR, GPU_CCU_DLY_TIME, GPU_CCU_DLY_TIME_STEP);
}

void gpu_cpr_init(void)
{
	u32 data;
	wr(0x06e00620, 0x0);
	data = rd(AP_AON_PMIC_CTRL_BADDR + 0x50);
	data = data | 24;
	wr(AP_AON_PMIC_CTRL_BADDR + 0x50, data);
	wr(AP_AON_PMIC_CTRL_BADDR + 0x5c, 0x3084a000 + 0x4);
	wr(AP_AON_PMIC_CTRL_BADDR + 0x58, 0x3084a000 + 0x10);
	gpu_low_power_config(PCU_REG_TRIGGER, BPC_HW_MODEL, ON);
	lp_gpu_ss_ccu_init();
}

void d2d_low_power_init(void)
{
	if (ofnode_conf_read_int("d2d-ccu-enable", 0) == 1) {
		lp_ccu_reg_init_with_gating(AP_D2D_PCTRL_CFG_CCU_BADDR, D2D_CCU_DLY_TIME,
				D2D_CCU_DLY_TIME_STEP);
		lp_ccu_reg_init_with_gating(AP_D2D_CCTRL_AXI_CCU_BADDR, D2D_WRAP_CCU_DLY_TIME,
				D2D_WRAP_CCU_DLY_TIME_STEP);
	}
}

#define IOPMP_DEVICE_ID_IOMMU_PTW 0x00
//PERI1SS DFMU device id lists
#define IOPMP_DEVICE_ID_PERI1_SS 0x01
#define IOPMP_DEVICE_ID_GMAC_0 0x03
#define IOPMP_DEVICE_ID_GMAC_1 0x04
#define IOPMP_DEVICE_ID_GMAC_2 0x05
#define IOPMP_DEVICE_ID_AON 0x37
#define IOPMP_DEVICE_ID_CHIP_DBG 0x3E
//USB DFMU device id lists
#define IOPMP_DEVICE_ID_USB_SS 0x09
#define IOPMP_DEVICE_ID_USB3_0 0x0A
#define IOPMP_DEVICE_ID_USB2_1 0x0B
#define IOPMP_DEVICE_ID_USB2_2 0x0C
//PCIESS DFMU device id lists
#define IOPMP_DEVICE_ID_PCIE_SS 0x10
#define IOPMP_DEVICE_ID_DMAC_AP 0x02
#define IOPMP_DEVICE_ID_SD 0x06
#define IOPMP_DEVICE_ID_EMMC 0x08
#define IOPMP_DEVICE_ID_PCIE_0 0x11
#define IOPMP_DEVICE_ID_PCIE_1 0x12
#define IOPMP_DEVICE_ID_SATA_0 0x14
#define IOPMP_DEVICE_ID_EIP120_I 0x16
#define IOPMP_DEVICE_ID_EIP120_II 0x17
#define IOPMP_DEVICE_ID_EIP120_III 0x18
#define IOPMP_DEVICE_ID_TEE_DMAC 0x19
//VISS DFMU device id lists
#define IOPMP_DEVICE_ID_VI_SS 0x20
#define IOPMP_DEVICE_ID_ISP 0x21
#define IOPMP_DEVICE_ID_VI_PRE 0x22
#define IOPMP_DEVICE_ID_DW200 0x23
#define IOPMP_DEVICE_ID_VI_COMP_DECOMP 0x24
//VPSS DFMU device id lists
#define IOPMP_DEVICE_ID_VP_SS 0x25
#define IOPMP_DEVICE_ID_VENC 0x26
#define IOPMP_DEVICE_ID_VDEC 0x27
#define IOPMP_DEVICE_ID_G2D 0x28
//VOSS DFMU device id lists
#define IOPMP_DEVICE_ID_VO_SS 0x2B
#define IOPMP_DEVICE_ID_DISPLAY_0 0x2C
#define IOPMP_DEVICE_ID_DISPLAY_1 0x2D
#define IOPMP_DEVICE_ID_AUXDISP 0x2E
//NPUSS DFMU device id lists
#define IOPMP_DEVICE_ID_NPU_SS 0x30
#define IOPMP_DEVICE_ID_NPU 0x31
//GPUSS DFMU device id lists
#define IOPMP_DEVICE_ID_GPU_SS 0x33
#define IOPMP_DEVICE_ID_GPU 0x34
//D2D RX DFMU device id lists
#define IOPMP_DEVICE_ID_D2D_RX 0x1A
//CPU SS DFMU device id lists
#define IOPMP_DEVICE_ID_REMOTE_CPU 0x3A

#define IOPMP_MODE_TOR 0
#define IOPMP_MODE_NAPOT 1

#define TOR_RWX_PERMISSION 0x0F0F
#define TOR_RO_PERMISSION 0x0909
#define TOR_NO_ACCESS_PERMISION 0x0808
#define NAPOT_RWX_PERMISSION 0x1F
#define NAPOT_RO_PERMISSION 0x19
#define NAPOT_NO_ACCESS_PERMISION 0x18

uint32_t iopmp_mode = IOPMP_MODE_TOR;
uint32_t iopmp_permission = TOR_RWX_PERMISSION;

void config_iopmp(u32 chip_id, u64 iopmp_base_addr, u32 mode, u32 devid, u32 permission, u32 index)
{
	u32 global = 0;

	devid |= chip_id << 6; // device struct: chip_id[7~6], device id[5~0]

	if (mode == IOPMP_MODE_TOR) {
		wr(iopmp_base_addr + index * 4, permission); //TOR for entry index*4~index*4+1

		wr(iopmp_base_addr + 0x800 + index * 0x20, 0x0);
		wr(iopmp_base_addr + 0x804 + index * 0x20,
		   (global << 31) | (devid << 14)); //PPN=0x0 for entry index*4

		wr(iopmp_base_addr + 0x808 + index * 0x20, 0x0);
		wr(iopmp_base_addr + 0x80C + index * 0x20,
		   0x20 | (global << 31) | (devid << 14)); //PPN=0x80_0000_0000 for entry index*4+1
	} else {
		wr(iopmp_base_addr + index * 4, permission); //NAPOT for entry index*4

		wr(iopmp_base_addr + 0x800 + index * 0x20, 0xFFFFFFFF);
		wr(iopmp_base_addr + 0x804 + index * 0x20,
		   (global << 31) | (devid << 14) |
			   0xF); //PPN=(0x80_0000_0000 - 1)>>3 global=0 for entry index*4
	}
}

static u32 get_chip_id(void)
{
	ulong hartid = 0;
	u32 chip_id = 0;
#if defined(CONFIG_RISCV_SMODE) && !defined(CONFIG_SPL_BUILD)
	hartid = gd->arch.boot_hart;
#else
	hartid = csr_read(CSR_MHARTID);
#endif

	chip_id = hartid / 8; //0~7:D0, 8~15:D1, 16~23:D2, 24~31:D3

	return chip_id;
}

void aon_cpr_init(void)
{
	debug("%s\n", __func__);
	u32 chip_id = get_chip_id();

	aon_low_power_config(PCU_REG_TRIGGER, BPC_HW_MODEL);
	config_iopmp(chip_id, AP_PERI1_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_AON,
				 iopmp_permission, 0); //AON
}

void ss_cpr_init(unsigned int ss_cfg)
{
	u32 chip_id = get_chip_id();
	debug("%s(%d) ss_cfg:%x chip_id:%d", __func__, __LINE__, ss_cfg, chip_id);

#ifdef D2D_DEBUG_SS_MT
	ss_cfg |= (1 << NPU_CPR) | (1 << VO_CPR) | (1 << PERI_CPR) | (1 << PCIE_SATA_CPR);
#endif

#ifdef D2D_DEBUG_SS_REG
	ss_cfg |= (1 << NPU_CPR) | (1 << VO_CPR);
#endif

	if (ss_cfg & (1 << VP_CPR)) {
		vpss_cpr_init();
		config_iopmp(chip_id, AP_VP_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_VP_SS,
					iopmp_permission, 0); //VP_SS
		config_iopmp(chip_id, AP_VP_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_VENC,
					iopmp_permission, 1); //VENC
		config_iopmp(chip_id, AP_VP_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_VDEC,
					iopmp_permission, 2); //VDEC
		config_iopmp(chip_id, AP_VP_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_VDEC,
					iopmp_permission, 3); //G2D
		config_iopmp(chip_id, AP_VP_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_IOMMU_PTW,
					iopmp_permission, 4); //IOMMU
	}
	if (ss_cfg & (1 << VI_CPR)) {
		viss_cpr_init();
		config_iopmp(chip_id, AP_VI_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_VI_SS,
					iopmp_permission, 0); //VI_SS
		config_iopmp(chip_id, AP_VI_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_ISP, iopmp_permission,
					1); //ISP
		config_iopmp(chip_id, AP_VI_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_VI_PRE,
					iopmp_permission, 2); //VI_PRE
		config_iopmp(chip_id, AP_VI_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_DW200,
					iopmp_permission, 3); //DW200
		config_iopmp(chip_id, AP_VI_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_VI_COMP_DECOMP,
					iopmp_permission, 4); //VI_COMP_EDCOMP
		config_iopmp(chip_id, AP_VI_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_IOMMU_PTW,
					iopmp_permission, 5); //IOMMU
	}
	if (ss_cfg & (1 << NPU_CPR)) {
		npu_cpr_init();
		config_iopmp(chip_id, AP_NPU_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_NPU_SS,
					iopmp_permission, 0); //NPU_SS
		config_iopmp(chip_id, AP_NPU_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_NPU,
					iopmp_permission, 1); //NPU
		config_iopmp(chip_id, AP_NPU_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_IOMMU_PTW,
					iopmp_permission, 2); //IOMMU
	}
	if (ss_cfg & (1 << VO_CPR)) {
		vo_cpr_init();
		config_iopmp(chip_id, AP_VO_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_VO_SS,
					iopmp_permission, 0); //VO_SS
		config_iopmp(chip_id, AP_VO_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_DISPLAY_0,
					iopmp_permission, 1); //DISPLAY_0
		config_iopmp(chip_id, AP_VO_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_DISPLAY_1,
					iopmp_permission, 2); //DISPLAY_1
		config_iopmp(chip_id, AP_VO_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_AUXDISP,
					iopmp_permission, 3); //AUXDISP
		config_iopmp(chip_id, AP_VO_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_IOMMU_PTW,
					iopmp_permission, 4); //IOMMU
	}

	if (ss_cfg & (1 << PERI_CPR)) {
		peri_cpr_init();
		config_iopmp(chip_id, AP_PERI1_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_PERI1_SS,
					iopmp_permission, 1); //PERI1_SS
		config_iopmp(chip_id, AP_PERI1_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_GMAC_0,
					iopmp_permission, 2); //GMAC_0
		config_iopmp(chip_id, AP_PERI1_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_GMAC_1,
					iopmp_permission, 3); //GMAC_1
		config_iopmp(chip_id, AP_PERI1_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_GMAC_2,
					iopmp_permission, 4); //GMAC_2
		config_iopmp(chip_id, AP_PERI1_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_CHIP_DBG,
					iopmp_permission, 5); //CHIP_DBG
		config_iopmp(chip_id, AP_PERI1_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_IOMMU_PTW,
					iopmp_permission, 6); //IOMMU
	}

	if (ss_cfg & (1 << PCIE_SATA_CPR)) {
		pcie_cpr_init();
		config_iopmp(chip_id, AP_PCIE_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_PCIE_SS,
					iopmp_permission, 0); //PCIE_SS
		config_iopmp(chip_id, AP_PCIE_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_PCIE_0,
					iopmp_permission, 1); //PCIE_0
		config_iopmp(chip_id, AP_PCIE_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_PCIE_1,
					iopmp_permission, 2); //PCIE_1
		config_iopmp(chip_id, AP_PCIE_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_SATA_0,
					iopmp_permission, 3); //SATA_0
		config_iopmp(chip_id, AP_PCIE_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_EIP120_I,
					iopmp_permission, 4); //EIP120_I
		config_iopmp(chip_id, AP_PCIE_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_EIP120_II,
					iopmp_permission, 5); //EIP120_II
		config_iopmp(chip_id, AP_PCIE_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_EIP120_III,
					iopmp_permission, 6); //EIP120_III
		config_iopmp(chip_id, AP_PCIE_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_TEE_DMAC,
					iopmp_permission, 7); //TEE_DMAC
		config_iopmp(chip_id, AP_PCIE_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_DMAC_AP,
					iopmp_permission, 8); //DMAC_AP
		config_iopmp(chip_id, AP_PCIE_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_SD,
					iopmp_permission, 9); //SD
		config_iopmp(chip_id, AP_PCIE_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_EMMC,
					iopmp_permission, 10); //EMMC
		config_iopmp(chip_id, AP_PCIE_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_IOMMU_PTW,
					iopmp_permission, 11); //IOMMU
	}

	if (ss_cfg & (1 << USB_CPR)) {
		usb_cpr_init(ss_cfg);
		config_iopmp(chip_id, AP_USB_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_USB_SS,
					iopmp_permission, 0); //USB_SS
		config_iopmp(chip_id, AP_USB_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_USB3_0,
					iopmp_permission, 1); //USB3_0
		config_iopmp(chip_id, AP_USB_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_USB2_1,
					iopmp_permission, 2); //USB2_1
		config_iopmp(chip_id, AP_USB_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_USB2_2,
					iopmp_permission, 3); //USB2_2
		config_iopmp(chip_id, AP_USB_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_IOMMU_PTW,
					iopmp_permission, 4); //IOMMU
	}

	if (ss_cfg & (1 << TEE_CPR)) {
		tee_cpr_init();
	}

	if (ss_cfg & (1 << GPU_CPR)) {
		gpu_cpr_init();
		config_iopmp(chip_id, AP_GPU_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_GPU_SS,
					iopmp_permission, 0); //GPU_SS
		config_iopmp(chip_id, AP_GPU_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_GPU,
					iopmp_permission, 1); //GPU
	}

	if (ss_cfg & (1 << D2D_CPR)) {
		config_iopmp(chip_id, AP_D2D_RX_DFMU_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_D2D_RX,
					iopmp_permission, 0); //D2D RX
	}

	if (ss_cfg & (1 << D2D_CPU_CPR)) {
		config_iopmp(chip_id, AON_CPU_SS_IOPMP_BADDR, iopmp_mode, IOPMP_DEVICE_ID_REMOTE_CPU,
					iopmp_permission, 0); //D2D ACE
	}

#ifdef CONFIG_SOC_ZHIHE_D2D
	d2d_low_power_init();
#endif

	wr(SAM_INDICATOR,0xdeadbeee);

	/* dwdma clk & rst */
	extern void ss_dwdma_config(void);
	ss_dwdma_config();
}
