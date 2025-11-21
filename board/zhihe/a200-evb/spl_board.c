/*
* Copyright (C) 2024-2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
*
* SPDX-License-Identifier: GPL-2.0+
*/

#include <linux/types.h>
#include <console.h>
#include <cpu_func.h>
#include <cpu.h>
#include <asm/csr.h>
#include <asm/io.h>
#include <asm/barrier.h>
#include <spl.h>
#include <asm/spl.h>
#include <string.h>
#include <asm/global_data.h>
#include <linux/libfdt.h>
#include <fdt_support.h>
#include <fdtdec.h>
#include <linux/delay.h>
#include <time.h>
#include <image.h>
#include <hang.h>
#include <rand.h>

#include <asm/arch-zhxtc9/cpu_ext.h>

#include "lpddr-regu/ddr_regu.h"
#include "include/board.h"
#include "include/sys_clk.h"
#include "include/ddr.h"
#include "rambus/soc_parameter.h"

DECLARE_GLOBAL_DATA_PTR;

struct light_reset_list {
        u32 val;
        u64 reg;
};

static struct light_reset_list light_pre_reset_lists[] = {
	{0x00000037, 0xFFFFF4403C}, /* Aon: Audio sys rst */
};

static struct light_reset_list light_post_reset_lists[] = {
	{0x00000001, 0xFFFF0151B0}, /* AP rst_gen: NPU rst */
	{0xFFFFFFFF, 0xFFFF041028}, /* DSP sys_reg: DSP rst */
	{0x00000002, 0xFFEF528000}, /* VO sys_reg: GPU rst */
	{0x00000003, 0xFFEF528000}, /* VO sys_reg: GPU rst */
	{0x00000007, 0xFFFF529004}, /* VO sys_reg: DPU rst */
	{0x07FFFF18, 0xFFCB000014}, /* Audio sys_reg: DMA rst */
};

static void light_pre_reset_config(void)
{
	/* Reset VI/VO/VP/DSP/NPU/GPU/DPU */
	int i = 0;
	int entry_size;

	entry_size = ARRAY_SIZE(light_pre_reset_lists);

	while (i < entry_size) {
		writel(light_pre_reset_lists[i].val, (void *)(light_pre_reset_lists[i].reg));
		i++;
	}
}

static void light_post_reset_config(void)
{
	/* Reset VI/VO/VP/DSP/NPU/GPU/DPU */
	int i = 0;
	int entry_size;

	entry_size = ARRAY_SIZE(light_post_reset_lists);

	while (i < entry_size) {
		writel(light_post_reset_lists[i].val, (void *)(light_post_reset_lists[i].reg));
        udelay(2);
		i++;
	}
}

static void setup_ddr_pmp(void)
{
	/* 
	 * Clear the entry0 configuration and open the access permission of DDR;
	 * Keep the configuration of entry1
	 */
	writel(0x000000, (void *)(PMP_BASE_ADDR + 0x104));
	writel(0x000000, (void *)(PMP_BASE_ADDR + 0x100));
	//writel(0x0 >> 12, (void *)(PMP_BASE_ADDR + 0x10c));
	//writel(0x0 >> 12, (void *)(PMP_BASE_ADDR + 0x108));

	writel(0x4000, (void *)(PMP_BASE_ADDR + 0x000));

	sync_is();
}

static void clear_ddr_pmp(void)
{
	/* restore pmp entry0,entry1 setting in bootrom */
	writel(0x0400000000 >> 12, (void *)(PMP_BASE_ADDR + 0x104));
	writel(0x0 >> 12, (void *)(PMP_BASE_ADDR + 0x100));
	writel(0xffe1000000 >> 12, (void *)(PMP_BASE_ADDR + 0x10c));
	writel(0xffe0180000 >> 12, (void *)(PMP_BASE_ADDR + 0x108));

	writel(0x4040, (void *)(PMP_BASE_ADDR + 0x000));

	sync_is();
}

static inline void _l2cache_ciall(void)
{
	asm volatile (".long 0x0170000b");
}

static int get_rng(unsigned int *rng, int cnt)
{
	int i;
	u64 seed = timer_get_us();
	srand((unsigned int)seed);
	for (i = 0; i < cnt; i++)
		rng[i] = rand();
	return 0;
}
struct axiscr_region {
	long start;
	long end;
};

#define AXISCR_MAX_REGION_CNT 8
#define OFFSET_AXISCR_LOCK 0x4
#define OFFSET_AXISCR_MISC 0x8
#define OFFSET_AXISCR_CYPHER 0x14
#define OFFSET_AXISCR_REGION 0x40
#define OFFSET_AXISCR_TRNG 0x100
static void setup_ddr_scramble(void)
{
	int node, scr;
	unsigned int i, tmp;
	long base_addr, start, size, end;
	const fdt32_t *reg;
	const char *status;
	int lock_r, lock_w;
	const void *blob = (const void *)gd->fdt_blob;
	const char path[] = "/soc/axiscr";
	struct axiscr_region region[AXISCR_MAX_REGION_CNT] = {0};
	unsigned int rng[AXISCR_MAX_REGION_CNT*2] = {0}; // dual word per region
	int cnt = 0;

	node = fdt_path_offset(blob, path);
	if (node < 0) {
		printf("found no %s node in fdt\n", path);
		return;
	}

	reg = fdt_getprop(blob, node, "reg", NULL);
	if (!reg) {
		printf("Warning: device tree node '%s' has no address.\n", path);
		return;
	}
	base_addr = fdt_translate_address(blob, node, reg);

	status = fdt_getprop(blob, node, "lock-read", NULL);
	lock_r = (!strcmp(status, "okay")) ? 1:0;
	status = fdt_getprop(blob, node, "lock-write", NULL);
	lock_w = (!strcmp(status, "okay")) ? 1:0;

	for (scr = fdt_first_subnode(blob, node);
		scr >= 0; scr = fdt_next_subnode(blob, scr)) {
		if (!strcmp("okay", fdt_getprop(blob, scr, "status", NULL))) {
			reg = fdt_getprop(blob, scr, "region", NULL);
			start = fdt_translate_address(blob, scr, reg);
			reg += 2;
			size = fdt_translate_address(blob, scr, reg);
			end = start + size;

			region[cnt].start = start;
			region[cnt++].end = end;
			// TODO, check overlap
		}
	}

	if (cnt > 0) {
		if (cnt > AXISCR_MAX_REGION_CNT) {
			printf("failed to setup ddr scramble, since illegal axiscr region cnt<%d>", cnt);
			return;
		}

		get_rng(rng, cnt*2);
		for (i=0; i< cnt; i++) {
			// config region
			writel(region[i].start >> 12, (void *)(base_addr + OFFSET_AXISCR_REGION + i*8));
			writel(region[i].end >> 12, (void *)(base_addr + OFFSET_AXISCR_REGION + i*8+4));

			// config rng
			writel(rng[i*2], (void *)(base_addr + OFFSET_AXISCR_TRNG + i*8));
			writel(rng[i*2+1], (void *)(base_addr + OFFSET_AXISCR_TRNG + i*8+4));
		}
		// enable axi scramble
		tmp = readl((void *)(base_addr + OFFSET_AXISCR_MISC));
		tmp |= 1 << 18;
		writel(tmp, (void *)(base_addr + OFFSET_AXISCR_MISC));

		writel(1 << 0, (void *)(base_addr + OFFSET_AXISCR_CYPHER));

		tmp = readl((void *)(base_addr + OFFSET_AXISCR_MISC));
		tmp &= ~(0xff << 24);
		tmp |= 1 << 24;
		writel(tmp, (void *)(base_addr + OFFSET_AXISCR_MISC));

		// lock r/w
		tmp = readl((void *)(base_addr + OFFSET_AXISCR_LOCK));
		if (lock_r) {
			tmp |= 1 << 7;
			writel(tmp, (void *)(base_addr + OFFSET_AXISCR_LOCK));
		}
		if (lock_w) {
			tmp |= 1 << 8;
			writel(tmp, (void *)(base_addr + OFFSET_AXISCR_LOCK));
		}
		sync_is();
	}
}

struct axiparity_region {
	long start;
	long size;
};

#define AXIPARITY_MAX_REGION_CNT 8
#define OFFSET_AXIPARITY_CFG 0x0
#define OFFSET_AXIPARITY_REGION_CFG0 0x4
#define OFFSET_AXIPARITY_REGION_CFG1 0x8
#define OFFSET_AXIPARITY_SLFT_CFG0 0x44
#define OFFSET_AXIPARITY_SLFT_CFG1 0x48
#define OFFSET_AXIPARITY_SLFT_CFG2 0x4C
static void setup_ddr_parity(void)
{
	int node, parity;
	unsigned int i, tmp;
	long base_addr, start, size;
	const fdt32_t *reg;
	const char *status;
	int lock;
	const void *blob = (const void *)gd->fdt_blob;
	const char path[] = "/soc/axiparity";
	struct axiparity_region region[AXIPARITY_MAX_REGION_CNT] = {0};
	int cnt = 0;

	node = fdt_path_offset(blob, path);
	if (node < 0) {
		printf("found no %s node in fdt\n", path);
		return;
	}

	reg = fdt_getprop(blob, node, "reg", NULL);
	if (!reg) {
		printf("Warning: device tree node '%s' has no address.\n", path);
		return;
	}
	base_addr = fdt_translate_address(blob, node, reg);

	status = fdt_getprop(blob, node, "lock", NULL);
	lock = (!strcmp(status, "okay")) ? 1:0;

	for (parity = fdt_first_subnode(blob, node);
		parity >= 0; parity = fdt_next_subnode(blob, parity)) {
		if (!strcmp("okay", fdt_getprop(blob, parity, "status", NULL))) {
			reg = fdt_getprop(blob, parity, "region", NULL);
			start = fdt_translate_address(blob, parity, reg);
			reg += 2;
			size = fdt_translate_address(blob, parity, reg);

			region[cnt].start = start;
			region[cnt++].size = size;
			// TODO, check overlap
		}
	}

	if (cnt > 0) {
		if (cnt > AXIPARITY_MAX_REGION_CNT) {
			printf("failed to setup ddr parity, since illegal axiparity region cnt<%d>", cnt);
			return;
		}

		for (i=0; i< cnt; i++) {
			// config region
			writel(region[i].start >> 12, (void *)(base_addr + OFFSET_AXIPARITY_REGION_CFG0 + i*8));
			writel(region[i].size >> 12, (void *)(base_addr + OFFSET_AXIPARITY_REGION_CFG1 + i*8));
		}

		// parity region number
		tmp = readl((void *)(base_addr + OFFSET_AXIPARITY_CFG));
		tmp |= cnt << 0;
		writel(tmp, (void *)(base_addr + OFFSET_AXIPARITY_CFG));

		for (i=0; i< cnt; i++) {
			// selftest config
			writel((region[i].start >> 12) << 8, (void *)(base_addr + OFFSET_AXIPARITY_SLFT_CFG0 + i*0xc));
			writel((region[i].size >> 12) << 8, (void *)(base_addr + OFFSET_AXIPARITY_SLFT_CFG1 + i*0xc));
			writel(1 << 0, (void *)(base_addr + OFFSET_AXIPARITY_SLFT_CFG2 + i*0xc));
		}
		mdelay(10); //4ms for 4GB SLFT

		// enable axi parity
		tmp = readl((void *)(base_addr + OFFSET_AXIPARITY_CFG));
		tmp &= ~(0xff << 24);
		tmp |= 1 << 24;
		writel(tmp, (void *)(base_addr + OFFSET_AXIPARITY_CFG));

		// lock
		if (lock) {
			tmp = readl((void *)(base_addr + OFFSET_AXIPARITY_CFG));
			tmp |= 1 << 8;
			writel(tmp, (void *)(base_addr + OFFSET_AXIPARITY_CFG));
		}
		sync_is();
	}
}

#ifdef CONFIG_FIXUP_MEMORY_REGION

#define MAGIC_DATA (0xF4240)
#define MAGIC_DATA2 (0x5AA5)
#define MAGIC_DATA3 (0x3C3C)
#define MAGIC_DATA4 (0xF0F0)

#define MINIMAL_DDR_DENSITY_MB (1*1024)
#define MAXIMAL_DDR_DENSITY_MB (16*1024)
#define UNIT_MB (1024*1024)

/*
return: 0: found boundary;
*/
int boundary_verify(unsigned long boundary) {
	phys_addr_t verify_addr = (phys_addr_t)CFG_SYS_SDRAM_BASE;
	phys_addr_t verify_addr2 = ((phys_addr_t)boundary + CFG_SYS_SDRAM_BASE)/4;
	phys_addr_t verify_addr3 = ((phys_addr_t)boundary + CFG_SYS_SDRAM_BASE)/2;
	phys_addr_t verify_addr4 = (phys_addr_t)boundary + CFG_SYS_SDRAM_BASE;

	// verify data accessing result firstly
	writel(MAGIC_DATA2, (void __iomem *)verify_addr);
	invalidate_dcache_range(verify_addr, verify_addr + CONFIG_SYS_CACHELINE_SIZE);
	if (readl((void __iomem *)verify_addr) != MAGIC_DATA2) {
		printf("ddr rw test failed\n");
		return -1;
	}
	writel(MAGIC_DATA, (void __iomem *)verify_addr);  // writing at beginning
	invalidate_dcache_range(verify_addr, verify_addr + CONFIG_SYS_CACHELINE_SIZE);
	if (readl((void __iomem *)verify_addr) != MAGIC_DATA) {
		printf("ddr rw test failed\n");
		return -1;
	}
	writel(MAGIC_DATA2, (void __iomem *)verify_addr2); // writing at one-quarter addr
	writel(MAGIC_DATA3, (void __iomem *)verify_addr3); // writing at half addr
	invalidate_dcache_range(verify_addr, verify_addr + CONFIG_SYS_CACHELINE_SIZE);
	invalidate_dcache_range(verify_addr2, verify_addr2 + CONFIG_SYS_CACHELINE_SIZE);
	invalidate_dcache_range(verify_addr3, verify_addr3 + CONFIG_SYS_CACHELINE_SIZE);

	if (boundary == (unsigned long)MAXIMAL_DDR_DENSITY_MB * UNIT_MB) { // boundary by design
		if ((readl((void __iomem *)verify_addr) == MAGIC_DATA) &&
			(readl((void __iomem *)verify_addr2) == MAGIC_DATA2) &&
			(readl((void __iomem *)verify_addr3) == MAGIC_DATA3))
			return 0;
	}
	else {
		writel(MAGIC_DATA4, (void __iomem *)verify_addr4); // writing out of boundary
		invalidate_dcache_range(verify_addr4, verify_addr4 + CONFIG_SYS_CACHELINE_SIZE);
		if ((readl((void __iomem *)verify_addr) == MAGIC_DATA4) && // overwrite by verify_addr4
			(readl((void __iomem *)verify_addr2) == MAGIC_DATA2) &&
			(readl((void __iomem *)verify_addr3) == MAGIC_DATA3) &&
			(readl((void __iomem *)verify_addr4) == MAGIC_DATA4))
			return 0;
	}

	return -1;
}

static int setup_ddr_addrmap(void)
{
	unsigned long boundary = (unsigned long)MAXIMAL_DDR_DENSITY_MB * UNIT_MB;

	// verify data accessing result firstly
	writel(MAGIC_DATA, (phys_addr_t)CFG_SYS_SDRAM_BASE);
	invalidate_dcache_range(CFG_SYS_SDRAM_BASE, CFG_SYS_SDRAM_BASE + CONFIG_SYS_CACHELINE_SIZE);
	if (readl((phys_addr_t)CFG_SYS_SDRAM_BASE) != MAGIC_DATA) {
		printf("ddr rw test failed\n");
		goto addrmap_err;
	}
	writel(MAGIC_DATA2, (phys_addr_t)CFG_SYS_SDRAM_BASE);
	invalidate_dcache_range(CFG_SYS_SDRAM_BASE, CFG_SYS_SDRAM_BASE + CONFIG_SYS_CACHELINE_SIZE);
	if (readl((phys_addr_t)CFG_SYS_SDRAM_BASE) != MAGIC_DATA2) {
		printf("ddr rw test failed\n");
		goto addrmap_err;
	}

	// try to find memory boundary
	while (boundary >= (unsigned long)MINIMAL_DDR_DENSITY_MB * UNIT_MB) {
		if (query_ddr_boundary(boundary) == 0) {
			clear_ddr_pmp();
			fixup_ddr_addrmap(boundary);
			setup_ddr_pmp();
			if (boundary_verify(boundary) == 0) {
				gd->ram_size = boundary;
				printf("found ddr boundary <0x%lx>\n", boundary);
				return 0;
			}
		}
		boundary = boundary >> 1;
	}

	gd->ram_size = get_ddr_density();
addrmap_err:
	printf("failed to setup ddr addrmap\n");
	return -1;
}
#endif

static void cpu_performance_enable(void)
{
	csr_write(CSR_SMPEN, 0x1);
	csr_write(CSR_MCCR2, 0xe2490009);
	csr_write(CSR_MXSTATUS, 0x638000);
	csr_write(CSR_MHINT, 0x6e30c | (1<<21) | (1<<22)); // set bit21 & bit 22 to close tlb & fence broadcast
	// FIXME: Clear bit[12] to disable L0BTB.
	csr_write(CSR_MHCR, 0x17f); // clear bit7 to disable indirect brantch prediction
	// FIXME set mhint2[22] to enable core icg en
	csr_write(CSR_MHINT2, csr_read(CSR_MHINT2) | 0x420000);
	csr_write(CSR_MHINT4, csr_read(CSR_MHINT4) | 0x410);
}

/* weak imp at arch/riscv/lib/spl.c: after board_init_f */
int spl_board_init_f(void)
{
	/* Due to SPL not supporting DM_EVENT, 
	 * cpu_probe_all cannot be automatically
	 * called during dm_init_and_scan
	 */
	cpu_probe_all();

	light_pre_reset_config();
	sys_clk_config();

	light_post_reset_config();

#ifdef CONFIG_PMIC_VOL_INIT
	int ret = aon_local_init();
	if (ret) {
		printf("%s aon local init failed %d \n",__func__,ret);
		hang();
	}

	ret = pmic_ddr_set_voltage();
	if (ret) {
		printf("%s set ddr voltage failed \n",__func__);
		hang();
	}

	ret = pmic_reset_apcpu_voltage();
	if (ret) {
		printf("%s set apcpu voltage failed \n",__func__);
		hang();
	}
#endif

#ifdef CONFIG_RV_BOOK
	cpu_clk_config(750000000);
#else
	cpu_clk_config(0);
#endif

	init_ddr();
	setup_ddr_scramble();
	setup_ddr_parity();
	setup_ddr_pmp();
#ifdef CONFIG_FIXUP_MEMORY_REGION
	setup_ddr_addrmap();
#else
	// update ram_size from board config
	gd->ram_size = get_ddr_density();
#endif

	printf("ddr initialized, jump to uboot\n");
	return 0;
}

/* call from common/spl/spl.c:board_init_r */
#ifdef CONFIG_FIT

void board_boot_order(u32 *spl_boot_list)
{
	/* Config first boot device */
	spl_boot_list[0] = spl_boot_device();

	/* Boot SPL with fit */
	spl_boot_list[1] = BOOT_DEVICE_BOOTROM;

	cpu_performance_enable();
}
#else
void board_boot_order(u32 *spl_boot_list)
{
	spl_boot_list[0] = BOOT_DEVICE_BOOTROM;
	cpu_performance_enable();
}
#endif

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

/*****************************
 * Board Porting
 ****************************/
/*
 * Get ddr base addr & size 
 * call at spl_fit_boot_fixup.c
 */
int board_get_ddr_info(u64 *start, u64 *size)
{
	*start = 0x0;
	*size = get_ddr_density();
	return 0;
}

/*
 * Get Board info
 */
const char * board_get_fit_dtb_name(int do_multi_check)
{
	/* Use the U-Boot device tree name to 
	 *   match the device tree used by the kernel.
	 */
	if (strcmp("p1", CONFIG_DEFAULT_DEVICE_TREE) == 0) {
		return "th1520-lichee-pi-4a";
	} else if (strcmp("a200-evb", CONFIG_DEFAULT_DEVICE_TREE) == 0) {
		return "a200-evb";
	}
	return CONFIG_DEFAULT_DEVICE_TREE;
}

/* 
 * Board user-define fdt fixup
 */
int board_fixup_os_fdt(void *fdt)
{
	return 0;
}
