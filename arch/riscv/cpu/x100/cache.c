// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2025-2026 Spacemit, Inc
 *
 * X100 (K3) cache control. Only the SPL/M-mode cache-enable path is
 * SoC-specific: the I$/D$, snoop, branch-predictor and prefetch bits live in
 * the custom CSR 0x7c0/0x7f0 and are reachable in M-mode only (the SPL runs in
 * M-mode). The dcache range maintenance (flush/invalidate) is handled by the
 * generic Zicbom implementation in arch/riscv/lib/cache.c, so it is not
 * duplicated here.
 */

#include <cpu_func.h>
#include <dm.h>
#include <asm/cache.h>
#include <cache.h>
#include <asm/csr.h>

void icache_enable(void)
{
#if !CONFIG_IS_ENABLED(SYS_ICACHE_OFF)
#if defined(CONFIG_XPL_BUILD) && CONFIG_SPL_RISCV_MMODE
	/* csr:0x7c0 can be accessed in MMODE only */
	asm volatile("csrsi 0x7c0, 0x2 \n\t");
#endif
#endif
}

void icache_disable(void)
{
#if !CONFIG_IS_ENABLED(SYS_ICACHE_OFF)
#if defined(CONFIG_XPL_BUILD) && CONFIG_SPL_RISCV_MMODE
	/* csr:0x7c0 can be accessed in MMODE only */
	asm volatile("csrci 0x7c0, 0x2 \n\t");
#endif
#endif
}

int icache_status(void)
{
	int ret = 0;

#if !CONFIG_IS_ENABLED(SYS_ICACHE_OFF)
	/*
	 * If the I$ is enabled by configuration, report it as enabled by
	 * default; in M-mode (SPL) refine the answer from CSR 0x7c0.
	 */
	ret = 1;
#if defined(CONFIG_XPL_BUILD) && CONFIG_SPL_RISCV_MMODE
	asm volatile (
		"csrr t1, 0x7c0\n\t"
		"andi	%0, t1, 0x02\n\t"
		: "=r" (ret)
		:
		: "memory"
	);
#endif
#endif

	return ret;
}

void dcache_enable(void)
{
#if !CONFIG_IS_ENABLED(SYS_ICACHE_OFF)
#if defined(CONFIG_XPL_BUILD) && CONFIG_SPL_RISCV_MMODE
	/* csr:0x7c0 can be accessed in MMODE only */
	asm volatile("csrsi 0x7c0, 0x1 \n\t");
#endif
#endif
}

void dcache_disable(void)
{
#if !CONFIG_IS_ENABLED(SYS_ICACHE_OFF)
#if defined(CONFIG_XPL_BUILD) && CONFIG_SPL_RISCV_MMODE
	/* csr:0x7c0 can be accessed in MMODE only */
	asm volatile("csrci 0x7c0, 0x1 \n\t");
#endif
#endif
}

int dcache_status(void)
{
	int ret = 0;

#if !CONFIG_IS_ENABLED(SYS_ICACHE_OFF)
	/*
	 * If the D$ is enabled by configuration, report it as enabled by
	 * default; in M-mode (SPL) refine the answer from CSR 0x7c0.
	 */
	ret = 1;
#if defined(CONFIG_XPL_BUILD) && CONFIG_SPL_RISCV_MMODE
	asm volatile (
		"csrr t1, 0x7c0\n\t"
		"andi	%0, t1, 0x01\n\t"
		: "=r" (ret)
		:
		: "memory"
	);
#endif
#endif

	return ret;
}

void snoop_enable(void)
{
#if !CONFIG_IS_ENABLED(SYS_ICACHE_OFF)
#if defined(CONFIG_XPL_BUILD) && CONFIG_SPL_RISCV_MMODE
	/* snoop should be enabled after dcache is enabled */
	asm volatile("csrsi 0x7f0, 0x1 \n\t");
#endif
#endif
}

/*
 * Called by start.S before the first atomic memory operation (the hart-lottery
 * amoswap). The x100 requires AMOs to target coherent, cacheable memory, so the
 * L1 caches and the snoop (coherency) unit must be live by then or the amoswap
 * faults. Ordering matches the vendor SPL: icache, dcache, then snoop. The
 * enable helpers are no-ops outside the M-mode SPL, so this is safe in proper.
 */
void branch_predict_enable(void);
void prefetch_enable(void);

void harts_early_init(void)
{
	icache_enable();
	dcache_enable();
	snoop_enable();
	branch_predict_enable();
	prefetch_enable();
}

void branch_predict_enable(void)
{
#if !CONFIG_IS_ENABLED(SYS_BRANCH_PREDICT_OFF)
#if defined(CONFIG_XPL_BUILD) && CONFIG_SPL_RISCV_MMODE
	/* csr:0x7c0 can be accessed in MMODE only */
	csr_set(0x7c0, 0x10);
#endif
#endif
}

void branch_predict_disable(void)
{
#if !CONFIG_IS_ENABLED(SYS_BRANCH_PREDICT_OFF)
#if defined(CONFIG_XPL_BUILD) && CONFIG_SPL_RISCV_MMODE
	/* csr:0x7c0 can be accessed in MMODE only */
	csr_clear(0x7c0, 0x10);
#endif
#endif
}

void prefetch_enable(void)
{
#if !CONFIG_IS_ENABLED(SYS_PREFETCH_OFF)
#if defined(CONFIG_XPL_BUILD) && CONFIG_SPL_RISCV_MMODE
	/* csr:0x7c0 can be accessed in MMODE only */
	csr_set(0x7c0, 0x20);
#endif
#endif
}

void prefetch_disable(void)
{
#if !CONFIG_IS_ENABLED(SYS_PREFETCH_OFF)
#if defined(CONFIG_XPL_BUILD) && CONFIG_SPL_RISCV_MMODE
	/* csr:0x7c0 can be accessed in MMODE only */
	csr_clear(0x7c0, 0x20);
#endif
#endif
}

/*
 * dcache range maintenance for the X100, for every boot phase. The cpu nodes
 * declare Zicbom + riscv,cbom-block-size, so OpenSBI enables S-mode CBO
 * (menvcfg.CBCFE/CBIE) and U-Boot proper's generic Zicbom path is armed by
 * enable_caches(). The M-mode SPL runs before any enable_caches(), so its
 * generic flush/invalidate would stay no-ops; override the weak generics with
 * an unconditional, compile-time CBOM-block loop so cache maintenance is
 * uniform across the SPL and U-Boot proper. Without it, DMA buffers in
 * cacheable DRAM (e.g. the dwc3 event buffer) are read back stale. The
 * cbo.flush/cbo.inval encodings are forced via ".option arch, +zicbom" so they
 * assemble regardless of the global -march.
 *
 * These cbo ops are illegal in S-mode unless OpenSBI set menvcfg.CBCFE/CBIE,
 * which it does only when the DT declares Zicbom: the cpu-node riscv,isa
 * "_zicbom" token + riscv,cbom-block-size are required, not optional.
 *
 * X100 L1 cache line = 64 bytes.
 */
#define X100_CBOM_BLOCK		64

void flush_dcache_range(unsigned long start, unsigned long end)
{
	unsigned long addr = start & ~((unsigned long)X100_CBOM_BLOCK - 1);

	for (; addr < end; addr += X100_CBOM_BLOCK)
		__asm__ __volatile__(".option push\n\t"
				     ".option arch, +zicbom\n\t"
				     "cbo.flush 0(%0)\n\t"
				     ".option pop"
				     : : "r"(addr) : "memory");
}

void invalidate_dcache_range(unsigned long start, unsigned long end)
{
	unsigned long addr = start & ~((unsigned long)X100_CBOM_BLOCK - 1);

	for (; addr < end; addr += X100_CBOM_BLOCK)
		__asm__ __volatile__(".option push\n\t"
				     ".option arch, +zicbom\n\t"
				     "cbo.inval 0(%0)\n\t"
				     ".option pop"
				     : : "r"(addr) : "memory");
}
