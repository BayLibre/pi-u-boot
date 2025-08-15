// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright(C) 2024-2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <cpu_func.h>

#include <asm/arch-zhxtc9/cpu_ext.h>

void flush_dcache_all(void)
{
    asm volatile("fence");
    asm volatile(".long 0x0010000b");  /* dcache.call */
    asm volatile("fence");
}

void invalidate_dcache_all(void)
{
    asm volatile("fence");
    asm volatile(".long 0x0020000b");  /* dcache.iall */
    asm volatile("fence");
}

void flush_dcache_range(unsigned long start, unsigned long end)
{
    register unsigned long a0 asm("a0") = start & ~(CONFIG_SYS_CACHELINE_SIZE - 1);

    asm volatile("fence");
    for (; a0 < end; a0 += CONFIG_SYS_CACHELINE_SIZE) {
        asm volatile(".long 0x02b5000b");  /* dcache.cipa a0 */
    }
    sync_i();
    asm volatile("fence");
}

void invalidate_dcache_range(unsigned long start, unsigned long end)
{
    register unsigned long a0 asm("a0") = start & ~(CONFIG_SYS_CACHELINE_SIZE - 1);

    asm volatile("fence");
    for (; a0 < end; a0 += CONFIG_SYS_CACHELINE_SIZE)
        asm volatile(".long 0x02a5000b");  /* dcache.ipa a0 */

    sync_i();
    asm volatile("fence");
}
