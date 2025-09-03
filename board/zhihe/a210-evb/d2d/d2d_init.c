/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */
#include <asm/system.h>
#include <asm/io.h>
#include <stdio.h>

#include "../include/addr_defines.h"
#include "../subsys/ss_config.h"
#include "../include/utils/io.h"

#include "d2d.h"

#define printf_chip(fmt, ...) \
        printf("chip-%d: "fmt, chip_id, ##__VA_ARGS__)

#define SAM_MAGIC 0xdeadbeee
static void d2d_sam_en(u32 chip_id)
{
	u64 offset = chip_id * 0x2000000000;
	wr(AP_CPU_SS_SAM_BADDR + 0x10, 1);
	/* Compiler barrier */
	barrier();
	/* Cpu barrier */
	mb();
	wr(AP_PCIE_DFMU_SAM_BADDR + 0x10 + offset, 1);
	wr(AP_PERI1_DFMU_SAM_BADDR + 0x10 + offset, 1);
	wr(AP_NPU_DFMU_SAM_BADDR + 0x10 + offset, 1);
	wr(AP_VO_DFMU_SAM_BADDR + 0x10 + offset, 1);
	/***
	* SAM enable.
	* Need to enable this configuration for 2 or 4 dies.
	* AON_TOP_SAM_BADDR == 0x20268000
	***/
	wr(AON_TOP_SAM_BADDR + 0x10 + offset, 0x1);
	wr(SAM_INDICATOR + offset, SAM_MAGIC);
}

#define MAX_TIMER_TIME 5
/* chip0 configurate others */
static void d2d_timer_sync_en_chip(u32 chip_id)
{
	u64 offset = chip_id * 0x2000000000;
	u32 v;
	u32 i;

	/* 1. IO MUX */
	v = rd(AP_AON_PADCTRL_BADDR + 0x410 + offset);
	v &= 0xfffff0ff;
	v |= (0x7 << 8);
	wr(AP_AON_PADCTRL_BADDR + 0x410 + offset, v);

	/* chip0 AONGPIO1_1 IE disable / chip1 AONGPIO1_1 PE disable */
	v = (chip_id == 0) ? 0x2080008 : 0x2080208;
	wr(AP_AON_PADCTRL_BADDR + 0x44 + offset, v);

	/* 2. enable chip0->chipx sync */
	if (chip_id == 0) {
		/* N dies configure */
		v = (SOC_CHIP_NUM == 2) ? 0x1001 : 0x1007;
		wr(AP_AON_MCM_TIMER_BADDR + 0x4, v);
	}

	/* enable timer sync mode */
	wr(AP_AON_MCM_TIMER_BADDR + 0x0 + offset , 0x1);

	if (chip_id != 0)
		return;

	/* 3. trigger chip0->chipx sync(MSB) manually firstly */
	wr(AP_AON_MCM_TIMER_BADDR + 0x14, 0x00010000);

	/* 4. poll sync status */
	for (i = 0; i < MAX_TIMER_TIME; i++) {
		udelay(1000);
		v = rd(AP_AON_MCM_TIMER_BADDR + 0x10);
		if (v & (1 << 17)) {
			break;
		}
	}

	if (i == MAX_TIMER_TIME)
		printf_chip("%s poll fail 0x%x\n", __func__, v);
}

static void d2d_timer_sync_en(void)
{
	d2d_timer_sync_en_chip(1);
	d2d_timer_sync_en_chip(0);
}

#define CSR_MSMPR        	0x7f3
#define MSMPR_MSPEN		(0x00000001)
static void d2d_core_lpm_prepare(void)
{
	csr_set(CSR_MIE, 0);
	csr_read_clear(CSR_MSMPR, MSMPR_MSPEN);
	mb();
}

#define TRY_MAX_COUNT (1000)
void sync_to_chip(u32 chip_id)
{
	u64 offset = chip_id * 0x2000000000;
	int count = 0;

	while((rd(SAM_INDICATOR + offset) != SAM_MAGIC) && (count++ < TRY_MAX_COUNT)) {
		udelay(100);
	}

	if (count == TRY_MAX_COUNT) {
		printf("sync failure\n");
	}
}

static int d2d_ss_core_init(u32 chip_id)
{
	d2d_sam_en(chip_id);

	if (chip_id == 0) {
		/* expect die1 sam_en firstly */
		sync_to_chip(1);
		d2d_timer_sync_en();
	} else if (chip_id == 1) {
		wfi();
	} else if (chip_id == 2) {
		d2d_core_lpm_prepare();
  		wr(0x4030846120, 0xdeaddead);
		wfi();
	} else if (chip_id == 3) {
		d2d_core_lpm_prepare();
  		wr(0x6030846120, 0xdeaddead);
		wfi();
	}

	return 0;
}

int d2d_ss_init(void)
{
	u32 dfmu = rd(AP_SYSREG_BADDR + 0x14);
	/* bit[4:3] chip_id */
	u32 chip_id = (dfmu >> 3) & 0x3;

	d2d_ss_core_init(chip_id);

	return 0;
}
