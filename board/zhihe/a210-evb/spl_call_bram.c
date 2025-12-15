/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <asm/csr.h>

#include "include/board.h"
#include "include/utils/io.h"
#include "arch_ext/arch_ext.h"

/*********************
 * System
 *********************/
#ifdef DEBUG_LOG
ATT_BRAM_TEXT static void putchar(int c)
{
    volatile int *thr = (int *)0x8401000;
    volatile int *lsr = (int *)0x8401014;
    while((*lsr & 0x20) == 0);
    *thr = c;
}

ATT_BRAM_TEXT static void puts(const char *s)
{
    while(*s != '\0') {  
        putchar(*s);
        s++;
    }
}

ATT_BRAM_TEXT static void put_ulong(ulong val)
{
    ATT_BRAM_DATA static char hex[] = "0123456789abcdef";

    putchar('0'); putchar('x');
    for (int i = 0; i < 16; i++) {
        ulong tmp = (val & 0xf000000000000000) >> 60;
        putchar((char)hex[tmp]);
        val <<= 4;
    }
}
#endif

#ifndef RISCV_SMODE_TIMER_FREQ
#define RISCV_SMODE_TIMER_FREQ 24000000UL
#endif
ATT_BRAM_TEXT static void udelay(ulong usec)
{
    ulong total_count = (usec * RISCV_SMODE_TIMER_FREQ) / 1000000;
    ulong end_count = csr_read(CSR_TIME) + total_count;
    while(csr_read(CSR_TIME) < end_count);
}

/*********************
 * PMP
 *********************/
ATT_BRAM_TEXT void pmp_init_enable_bram(void)
{
    /* TOR: 0x0 ~ 0x70000000: L=0 XWR=0x7 BRAM enable */
    csr_write(pmpaddr0, 0x70000000 >> 2);

    /* TOR: 0x70000000 ~ 0x1080000000: L=1 XWR=0x0 OCRAM/DDR disable */
    csr_write(pmpaddr1, 0x1080000000 >> 2);

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

ATT_BRAM_TEXT void pmp_init_enable_bram_ocram(void)
{
    /* TOR: 0x0 ~ 0x70200000: L=0 XWR=0x7 BRAM/OCRAM enable */
    csr_write(pmpaddr0, 0x70200000 >> 2);

    /* TOR: 0x70200000 ~ 0x1080000000: L=1 XWR=0x0 DDR disable*/
    csr_write(pmpaddr1, 0x1080000000 >> 2);

    /* config */
    csr_write(pmpcfg0, 0x880F);
}


ATT_BRAM_TEXT static void pmp_init_enable_bram_ddr(void)
{
    /* TOR: 0x0 ~ 0x70000000: L=0 XWR=0x7 */
    csr_write(pmpaddr0, 0x70000000 >> 2);

    /*
    * TOR: 0x70000000 ~ 0x80000000: L=1 XWR=0x0
    * Configure OCRAM regions with inaccessible permissions.
    */
    csr_write(pmpaddr1, 0x80000000 >> 2);

    /* 
    * TOR: 0x80000000 ~ 0x180000000: L=1 XWR=0x7
    * Configure 2G DDR as valid, and set the remaining space's valid configuration in OpenSBI.
    */
    csr_write(pmpaddr2, 0x180000000 >> 2);

    /* Config entry0~2 */
    csr_write(pmpcfg0, 0x0F880F);
}

/*********************
 * SLC & Jump OpenSBI
 *********************/
#include "opensbi.h"
#include "include/addr_defines.h"
struct opensbi_entry_info {
    void *entry;
    ulong hartid;
    ulong dtb;
    struct fw_dynamic_info opensbi_info;
};
typedef void __noreturn (*opensbi_entry_t)(ulong hartid, ulong dtb, ulong info);
ATT_BRAM_DATA static struct opensbi_entry_info entry_info;

ATT_BRAM_TEXT static void slc_cache_init(u64 addr_sysreg_base, u64 addr_core_base)
{
    //u64 addr;
    u32 wdata;
    u32 rdata;

    //*********************************
    // SLC0/1 System Config
    //*********************************
    //slc0/1 rst_n
    wdata = 0x0;
    chip_wr(addr_sysreg_base+0x0,wdata);
	udelay(20);
    wdata = 0x3;
    chip_wr(addr_sysreg_base+0x0,wdata);

    //slc0/1 enbaled/csr_en
    rdata = chip_rd(addr_sysreg_base+0x4);
    wdata = ((rdata & 0xffffffcf)|0x30);
    chip_wr(addr_sysreg_base+0x4,wdata);

    //*********************************
    // SLC0/1 Init
    //*********************************
    //cahche0/1 init cfg
    wdata = 0x0;
    chip_wr(addr_core_base+0x100, wdata); //cache0, Initialize tag mem
    rdata = 0x1;
    while((rdata & 0x1) != 0x0) {
        rdata = chip_rd(addr_core_base+0x104);
    }
    wdata = 0x10000;
    chip_wr(addr_core_base+0x100, wdata); //cache0, Initialize data mem
    rdata = 0x1;
    while((rdata & 0x1) != 0x0) {
        rdata = chip_rd(addr_core_base+0x104);
    }

    //enable lookup & fill
    wdata = 0x3;
    chip_wr(addr_core_base+0x10, wdata); //cache0, cache enable

    //Policy Select
    // wdata = 0xff0007; //write-back, rd/wr allocate
    // wdata = 0xbf0007; //write-back, rd no-allocated, wr allocate
    // wdata = 0xf70007; //write-back, rd allocated, wr no-allocate
    // wdata = 0xb70007; //write-back, rd no-allocated, wr no-allocate
    wdata = 0xbf0007; //write-back, rd/wr allocate
    chip_wr(addr_core_base+0x18, wdata); //cache0, allocate override

    //enable cache
    wdata = 0x1;
    chip_wr(addr_core_base+0x0, wdata); //cache0, transaction enbale

    //*********************************
    //SLC0/1 Perf monitor setting
    //*********************************
    //duration
    wdata = 0xffffffff; //*256 cycle
    chip_wr(addr_core_base+0x484, wdata); //cache0, transaction enbale

    //rtt/wtt watermark
    wdata = (50 << 16) + 50;
    chip_wr(addr_core_base+0x488, wdata); //cache0, transaction enbale

    //enable perf, free-run + enable
    wdata = 0x6;
    chip_wr(addr_core_base+0x480, wdata); //cache0, transaction enbale

}

ATT_BRAM_TEXT static void slc_cache_disable(int die_count)
{
    for(int i = 0; i < die_count; i++) {
        chip_set(i);
        chip_wr(AP_DDR0_SLC_SYSREG_BADDR + 0x4, 0x2);
        chip_wr(AP_DDR1_SLC_SYSREG_BADDR + 0x4, 0x2);
    }
    chip_set(0);
}

ATT_BRAM_TEXT static void bram_switch_slc(ulong param)
{
    int slc_en = param & 0xffffffff;
    int die_count = (param >> 32) & 0xffffffff;
    pmp_init_enable_bram();

    /* Ensure PMP takes effect */
    udelay(1000);

#ifdef DEBUG_LOG
    ATT_BRAM_DATA static char str[] = "switch_slc ";
    puts(str);
    put_ulong(slc_en);
    put_ulong(die_count);
    putchar('\n');
#endif
    /* Disabled SRAM & Cache */
    if (slc_en) {
        for(int i = 0; i < die_count; i++) {
            chip_set(i);
            slc_cache_init(AP_DDR0_SLC_SYSREG_BADDR, AP_DDR0_SLC_CORE_BADDR);
            slc_cache_init(AP_DDR1_SLC_SYSREG_BADDR, AP_DDR1_SLC_CORE_BADDR);
        }
        chip_set(0);
    } else {
        slc_cache_disable(die_count);
    }

    pmp_init_enable_bram_ddr();

    /* Jump to OpenSBI*/
    opensbi_entry_t opensbi_entry = (opensbi_entry_t)entry_info.entry;
    opensbi_entry(entry_info.hartid, entry_info.dtb, (ulong)&entry_info.opensbi_info);
}

int spl_call_opensbi(void * entry, ulong hartid, ulong dtb, ulong info, ulong slc_en)
{
    /* Prepare BRAM OpenSBI Eentry Info */
    entry_info.entry = entry;
    entry_info.hartid = hartid;
    entry_info.dtb = dtb;
    entry_info.opensbi_info = *((struct fw_dynamic_info *)info);

    ulong die_count = loader_get_die_count();
    
    /* high32: die_count, low32: slc_en */
    ulong param = die_count << 32;
    param |= (slc_en & 0xffffffff);
    bram_entry(bram_switch_slc, param);

    /* Never arrive here */
    return 0;
}

/*********************
 * DDR PLL
 *********************/
ATT_BRAM_TEXT static void ddr_pll_config(int speed)
{
    int rdata;
    if (speed == 4266) {
        // 4266
        rdata = chip_rd(AP_SLC_DUAL_SYSREG_BADDR + 0xc);
        rdata &= 0xff000000;
        chip_wr(AP_SLC_DUAL_SYSREG_BADDR + 0xc, rdata | 0x40400000);
        chip_wr(AP_SLC_DUAL_SYSREG_BADDR + 0x8, 0x1310a02);
        udelay(2);
        rdata = chip_rd(AP_SLC_DUAL_SYSREG_BADDR + 0xc);
        chip_wr(AP_SLC_DUAL_SYSREG_BADDR + 0xc, rdata & 0xbfffffff);
    } else if (speed == 3733) {
        // 3733
        rdata = chip_rd(AP_SLC_DUAL_SYSREG_BADDR + 0xc);
        rdata &= 0xff000000;
        chip_wr(AP_SLC_DUAL_SYSREG_BADDR + 0xc, rdata | 0x40600000);
        chip_wr(AP_SLC_DUAL_SYSREG_BADDR + 0x8, 0x01204d01);
        udelay(2);
        rdata = chip_rd(AP_SLC_DUAL_SYSREG_BADDR + 0xc);
        chip_wr(AP_SLC_DUAL_SYSREG_BADDR + 0xc, rdata & 0xbfffffff);
    } else if (speed == 3200) {
        // 3200
        rdata = chip_rd(AP_SLC_DUAL_SYSREG_BADDR + 0xc);
        rdata &= 0xff000000;
        chip_wr(AP_SLC_DUAL_SYSREG_BADDR + 0xc, rdata | 0x40155555);
        chip_wr(AP_SLC_DUAL_SYSREG_BADDR + 0x8, 0x01408501);
        udelay(2);
        rdata = chip_rd(AP_SLC_DUAL_SYSREG_BADDR + 0xc);
        chip_wr(AP_SLC_DUAL_SYSREG_BADDR + 0xc, rdata & 0xbfffffff);
    } else if (speed == 2133) {
        // 2133
        rdata = chip_rd(AP_SLC_DUAL_SYSREG_BADDR + 0xc);
        rdata &= 0xff000000;
        chip_wr(AP_SLC_DUAL_SYSREG_BADDR + 0xc, rdata | 0x40000000);
        chip_wr(AP_SLC_DUAL_SYSREG_BADDR + 0x8, 0x01608501);
        udelay(2);
        rdata = chip_rd(AP_SLC_DUAL_SYSREG_BADDR + 0xc);
        chip_wr(AP_SLC_DUAL_SYSREG_BADDR + 0xc, rdata & 0xbfffffff);
    } else if (speed == 1066) {
        // 1066
        rdata = chip_rd(AP_SLC_DUAL_SYSREG_BADDR + 0xc);
        rdata &= 0xff000000;
        chip_wr(AP_SLC_DUAL_SYSREG_BADDR + 0xc, rdata | 0x40aaaaab);
        chip_wr(AP_SLC_DUAL_SYSREG_BADDR + 0x8, 0x002608501);
        udelay(2);
        rdata = chip_rd(AP_SLC_DUAL_SYSREG_BADDR + 0xc);
        chip_wr(AP_SLC_DUAL_SYSREG_BADDR + 0xc, rdata & 0xbfffffff);
    } else {
        ;
    }
    while ((chip_rd(AP_SLC_DUAL_SYSREG_BADDR + 0x18) & 1) != 0x1) {
        ; // pll lock
    }
    chip_wr(AP_SLC_DUAL_SYSREG_BADDR + 0x18, 0x10000);
}

ATT_BRAM_TEXT static void bram_switch_ddrpll(ulong speed)
{
    pmp_init_enable_bram();

    /* Ensure PMP takes effect */
    udelay(1000);

#ifdef DEBUG_LOG
    ATT_BRAM_DATA static char str[] = "switch_ddrpll ";
    puts(str);
    put_ulong(speed);
    putchar('\n');
#endif

    ddr_pll_config((int)speed);

    pmp_init_enable_bram_ocram();

    /* Ensure PMP takes effect */
    udelay(1000);
}

void spl_switch_ddrpll(int speed)
{
    bram_entry(bram_switch_ddrpll, speed);
}
