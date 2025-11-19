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
#include "include/board.h"
#include "../common/include/boot.h"
#include "../common/include/board_porting.h"
#include "rambus/soc_parameter.h"
#include "include/utils/utils.h"
#include "adc/adc.h"

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

static void pmp_init_eanble_bram_ocram_ddr(void)
{
    /* TOR: 0x0 ~ 0x70200000: L=0 XWR=0x7 Enable BRAM & OCRAM */
    csr_write(pmpaddr0, 0x70200000 >> 2);

    /* TOR: 0x70200000 ~ 0x80000000: L=1 XWR=0x0 Disable Invalid memory */
    csr_write(pmpaddr1, 0x80000000 >> 2);

    /* 0x80000000 ~ : No configuration, Enabled DDR */

    /*
     * PMPCFG 8~15, One address table entry uses one byte configuration attribute
     * Attribute: 0xLUUAAXWR
     *            L:
     *                 0-Machine mode ignores permission configuration
     *                 1-Lock and All modes need to check permission configuration
     *            U:   Reserved
     *            AA:  00-OFF 01-TOR 10-NA4(unsupported) 11-NAPOT
     *            XWR: permission configuration
     */
    csr_write(pmpcfg0, 0x880F);
}

/* call from common/spl/spl.c:board_init_r */
void board_boot_order(u32 *spl_boot_list)
{
	/* Config first boot device */
	spl_boot_list[0] = spl_boot_device();

	/* Boot SPL with fit */
	spl_boot_list[1] = BOOT_DEVICE_BOOTROM;
}

static void clk_init(void)
{
	struct udevice *dev;

#if (IS_ENABLED(CONFIG_CLK_ZHIHE_A210))
	uclass_get_device_by_driver(UCLASS_CLK,
					DM_DRIVER_GET(zhihe_a210_clk),
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

static void ss_sam_en(void)
{
	chip_wr(AP_PCIE_DFMU_SAM_BADDR + 0x10, 1);
	chip_wr(AP_PERI1_DFMU_SAM_BADDR + 0x10, 1);
	chip_wr(AP_NPU_DFMU_SAM_BADDR + 0x10, 1);
	chip_wr(AP_VO_DFMU_SAM_BADDR + 0x10, 1);
}

static int init_chip(int chip_id, int num_chips)
{
	int ret;
	chip_set(chip_id);

	cpu_ss_init();
	clk_init();
	ddr_low_power_init();

	/* DDR init */
	ret = ddr_init(board_get_ddrtype());

	/* CPR init */
	ss_cpr_init(SS_CFG_DEFAULT, chip_id);

	if (num_chips > 1)
		ss_sam_en();

	chip_set(0);
	return ret;
}

static void init_all_chips(void)
{
	int die_count = board_get_die_count();
	int ret = 0;

	/* fastboot mode, should not init other chips */
	if (spl_boot_device() == BOOT_DEVICE_BOOTROM)
		die_count = 1;

	for (int i = 0; i < die_count; i++) {
		printf("Chip: init chip-%d\n", i);
		ret = init_chip(i, die_count);
		if (ret) {
			printf("spl: init chip-%d fail\n", i);
			while(1);
		}
	}
}

/* weak imp at arch/riscv/lib/spl.c */
int spl_board_init_f(void)
{
	/* Due to SPL not supporting DM_EVENT,
	 * cpu_probe_all cannot be automatically
	 * called during dm_init_and_scan
	 */
	cpu_probe_all();

	/* Check board type by adc value */
	board_type_check();

	/* Bram call init */
	board_spl_prepare_bram_section();
	invalidate_icache_all();

	/* Init chips */
	init_all_chips();

	/* DDR Debug */
	// ddr_registers_dump();
	// ddr_dfmu_mt_test();
	// ddr_dfmu_mt_test_single();

	/* Reset pmp */
	pmp_init_eanble_bram_ocram_ddr();

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

/*****************************
 * Board Porting
 ****************************/
 /*
 * Get ddr base addr & size 
 * call at spl_fit_boot_fixup.c
 */
int board_get_ddr_info(u64 *start, u64 *size)
{
	*start = CFG_SYS_SDRAM_BASE;
	*size = ddr_determine_size(board_get_ddrtype());
	return 0;
}

/*
 * Get Board info
 */
const char * board_get_fit_dtb_name(int do_multi_check)
{
	static char dtb_name_buf[64];

	const char * name;
	enum board_type type = board_get_type();

	/* Convert to string type */
	switch(type) {
	case BOARD_EVB:
		name = STR_BOARD_EVB;
		break;
	case BOARD_DEV:
		name = STR_BOARD_DEV;
		break;
	case BOARD_EVB_D2D:

		name = STR_BOARD_EVB_D2D;
		break;
	default:
		name = STR_BOARD_EVB;
	}
	strcpy(dtb_name_buf, name);

	/* Convert to string type */
	if (do_multi_check) {
		if (board_multi_fit_check("-sec")) {
			strcat(dtb_name_buf, "-sec");
		}
	}

	return dtb_name_buf;
}

/* 
 * Board user-define fdt fixup
 */
int board_fixup_os_fdt(void *fdt)
{
	if ((spl_boot_device() == BOOT_DEVICE_BOOTROM) && board_get_die_count() > 1) {
		/* For a multi-DIE SoC, only the CPU on DIE0 is booted in fastboot mode. */
		int node_offset;
		uint32_t entry_cnt[2] = { cpu_to_fdt32(4), cpu_to_fdt32(4) };
		uint32_t control_val[2] = { cpu_to_fdt32(0x1f), cpu_to_fdt32(0x1f) };

		node_offset = fdt_path_offset(fdt, "/soc/reset-sample");
		if (node_offset < 0) {
			return -1;
		}

		int ret = fdt_setprop(fdt, node_offset, "entry-cnt", entry_cnt, sizeof(entry_cnt));
		if (ret < 0) {
			return -1;
		}
		ret = fdt_setprop(fdt, node_offset, "control-val", control_val, sizeof(control_val));
		if (ret < 0) {
			return -1;
		}
	}
	return 0;
}

/*
 * Do board type check
 */
/*
Attention:
The following variable must not be initialized to zero.
This global variable is assigned in the 'f' stage and
must persist into the 'r' stage of the SPL.
If it is initialized to zero and becomes a BSS variable,
it will be re-zeroed upon entering the 'r' stage, causing data loss.
*/
static enum board_type _board_type = BOARD_UNKNOWN;
static enum ddr_type _ddr_type = DDR_UNKNOWN;

void board_type_check(void)
{
	if (board_get_die_count() > 1) {
		_board_type = BOARD_EVB_D2D;
		_ddr_type = DDR_4266_1Rank_4GB;

		return;
	}

	adc_init();

	u64 adc_ch0_mv = adc_read(0, 16);
	u64 adc_ch2_mv = adc_read(2, 16);
	printf("Board check: ch0=%llumV ch2=%llumV\n", adc_ch0_mv, adc_ch2_mv);

	/* BOARD_EVB ch2 (800mv ~ 1300mv) */
	if (adc_ch2_mv >= 800 && adc_ch2_mv <= 1300) {
		_board_type = BOARD_EVB;
	} else {
		_board_type = BOARD_DEV;
	}

	if (_board_type == BOARD_EVB) {
		if (adc_ch0_mv >= 0 && adc_ch0_mv <= 100) {
			printf("Board info: bid=%d, DDR_4266_1Rank_2GB * 2\n", _board_type);
			_ddr_type = DDR_4266_1Rank_2GB;
		} else if (adc_ch0_mv >= 500 && adc_ch0_mv <= 700) {
			printf("Board info: bid=%d, DDR_4266_1Rank_4GB * 2\n", _board_type);
			_ddr_type = DDR_4266_1Rank_4GB;
		} else if (adc_ch0_mv >= 1100 && adc_ch0_mv <= 1300) {
			printf("Board info: bid=%d, DDR_4266_2Rank_8GB * 2\n", _board_type);
			_ddr_type = DDR_4266_2Rank_8GB;
		}else {
			printf("Board info: bid=%d, ch0 value is not supported, set default DDR_4266_1Rank_2GB)\n", _board_type);
			_ddr_type = DDR_4266_1Rank_2GB;
		}
	} else if (_board_type == BOARD_DEV) {
		if (adc_ch2_mv >= 1700 && adc_ch2_mv <= 1900) {
			printf("Board info: bid=%d, DDR_4266_1Rank_4GB * 2\n", _board_type);
			_ddr_type = DDR_4266_1Rank_4GB;
		} else {
			printf("Board info: bid=%d, DDR_4266_1Rank_2GB * 2\n", _board_type);
			_ddr_type = DDR_4266_1Rank_2GB;
		}
	} else {
		printf("Board info: bid=%d, ch2 value is not supported, set default DDR_4266_1Rank_2GB)\n", _board_type);
		_ddr_type = DDR_4266_1Rank_2GB;
	}
}

enum board_type board_get_type(void)
{
	return _board_type;
}

enum ddr_type board_get_ddrtype(void)
{
	return _ddr_type;
}
