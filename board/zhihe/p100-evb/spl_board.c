// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */
//#define DEBUG
#include <asm/spl.h>
#include <cpu_func.h>
#include <cpu.h>
#include <mapmem.h>
#include <image.h>
#include <spl.h>
#include <init.h>
#include <time.h>
#include <log.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include "ddr/ddr_init.h"
#include "cpusys/cpu_ss_init.h"
#include "subsys/subsys.h"
#include "cmd/ss.h"
#ifdef CONFIG_SOC_ZHIHE_D2D
#include "d2d/d2d.h"
#endif
#include "include/board.h"
#include "rambus/soc_parameter.h"

//#define DDR_CHECK 1

#ifdef DDR_CHECK
static void ddr_check(long blocksize /*MB*/, long total /*GB*/)
{
	long i = 0, j = 0;
	long block = 0x100000 * blocksize;
	long memsize = 0x40000000UL * total;
	unsigned int *p=(unsigned int *)0x80000000;
	unsigned int magic = 0;//0xf0f0f0f0;

	for (i = 0; i < (memsize / block); i++) {
		printf("%p\n", p);

		for (j = 0; j < block; j++) {
			p[j] = (unsigned int)(unsigned long)(&p[j]) + magic;
			//printf("j = %ld\n", j);
		}

		for (j = 0; j < block; j++) {
			if (p[j] != ((unsigned int)(unsigned long)(&p[j]) + magic)) {
				printf("failed %p %x\n", &p[j], p[j]);
			}
		}

		p += (block / 4);
	}
	printf("test ok,%lx\n", i);
}

static void sim_ddr_check(long blocksize /*MB*/, long total /*GB*/)
{
	int i = 0;
	unsigned int *p=(unsigned int *)0x80000000;
	long block = 0x100000 * blocksize;
	long memsize = 0x40000000UL * total;
	unsigned int magic = 0x55555555;

	for (i = 0; i < (memsize / block); i++) {
		p[0] = (unsigned int)(unsigned long)(&p[0]) + magic;
		flush_dcache_range((unsigned long)p, (unsigned long)(p + CONFIG_SYS_CACHELINE_SIZE));

		invalidate_dcache_range((unsigned long)p, (unsigned long)(p + CONFIG_SYS_CACHELINE_SIZE));
		if (p[0] != ((unsigned int)(unsigned long)(&p[0]) + magic)) {
			printf("fail %p %x\n", &p[0], p[0]);
		} else {
			printf("succ %p %x\n", &p[0], p[0]);
		}
		p += (block / 4);
	}
}
#endif

/* call from common/spl/spl.c:board_init_r */
void board_boot_order(u32 *spl_boot_list)
{
	/* Boot SPL with fit */
	spl_boot_list[0] = BOOT_DEVICE_BOARD;

	/* FIT Debug */
	spl_boot_list[1] = BOOT_DEVICE_RAM;

	/* Boot EMMC */
	spl_boot_list[2] = BOOT_DEVICE_MMC1;
}

static void clk_init(void)
{
	struct udevice *dev;

#if (IS_ENABLED(CONFIG_CLK_ZHIHE_P100))
	uclass_get_device_by_driver(UCLASS_CLK,
					DM_DRIVER_GET(zhihe_p100_clk),
					&dev);
#endif
}

static void ddr_low_power_init(void)
{
	if (ofnode_conf_read_int("ddr-ccu-enable", 0) == 1)
		lp_ddr_ss_ccu_init();
	if (ofnode_conf_read_int("ddr-ppu-dyn", 0) == 1)
		lp_ddr_ss_pctrl_init();
}

/* weak imp at arch/riscv/lib/spl.c */
int spl_board_init_f(void)
{
	unsigned long t_start, t_end;

	/* Due to SPL not supporting DM_EVENT,
	 * cpu_probe_all cannot be automatically
	 * called during dm_init_and_scan
	 */
	cpu_probe_all();

	cpu_ss_init();

	clk_init();

#ifdef CONFIG_ZHP100EVB_DDR_INIT
	debug("ddr init\n");
	ddr_low_power_init();
	t_start = timer_get_us();
	ddr_init(&dram_timing);
	t_end = timer_get_us();
	debug("ddr init duration: %ld us\n", t_end - t_start);
#endif

	/* EMU DDR debug stop trigger */
	//printf("%c%c%c%c\n",0xDE,0xAD,0xBE,0xFF & 0);

#ifdef DDR_CHECK
	/* DDR read write check*/
	ddr_check(1/*MB*/, 1/*GB*/);
	//sim_ddr_check(256, 4);
#endif

	// ddr_registers_dump();

	// ddr_dfmu_mt_test();
	// ddr_dfmu_mt_test_single();

	return 0;
}

/* call from common/spl/spl.c:board_init_r */
void spl_board_init(void)
{
#ifdef CONFIG_SPL_ENV_SUPPORT
	int ret;
	ret = env_init();
	if (ret == 0) {
		ret = env_load();
	}
#endif

	/* Boot serial check */
	//g_boot_spl_with_fit = board_spl_boot_check();

	/* CPR init */
	ss_cpr_init(SS_CFG_DEFAULT);
#ifdef CONFIG_SOC_ZHIHE_D2D
	/* Note: After this call, all cores except die0 core0 will enter WFI state */
	d2d_ss_init();
#endif

#ifdef CONFIG_ZHIHE_RAMBUS_ALGO
	/* libsecurity.a.bin soc parameter init */
	csi_soc_parmeter_t soc_parameter = {
		EFUSE_LC_RMA_UPDATE_OFFSET,
		TEE_SYS_EFUSE_DBG_KEY1_OFF,
		TEE_SYS_BASE,
		EFUSE_BASE,
		EIP150B_BASE,
		EIP28_BASE,
		EIP120SI_BASE,
		EIP120SII_BASE,
		EIP120SIII_BASE,
		KEYRAM_BASE,
	};
	csi_init_soc_parameter(&soc_parameter);
#endif
}

/* simple fit, need read buffer, call from common/spl/spl_fit.c:spl_simple_fit_read */
void *board_spl_fit_buffer_addr(ulong fit_size, int sectors, int bl_len)
{
	return map_sysmem(CONFIG_SYS_LOAD_ADDR, 0);
}

int board_get_ddr_info(u64 *start, u64 *size)
{
	*start = CFG_SYS_SDRAM_BASE;
	*size = ddr_determine_size();
	return 0;
}
