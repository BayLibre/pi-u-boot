// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2024 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */
#include <cpu_func.h>

#include "ddr_init.h"
#include "include/ddrphy.h"

#include "board_boot.h"

#include "../include/utils/utils.h"
#include "../include/addr_defines.h"
#include "../include/board.h"

// #define UTILS_TEST
#ifdef UTILS_TEST
static void utils_test(void)
{
    u64 reg32_addr = (0x120000 - 8);
    u64 reg16_addr = (0x120000 - 16);

    u32 val32 = 0;
    u16 val16 = 0;

    chip_wr(reg32_addr, 0x12345678);
    chip_wr16(reg16_addr, 0x1234);

    val32 = chip_rd((u64)reg32_addr);
    val16 = chip_rd16((u64)reg16_addr);

    printf("\nutils test val32=0x%x val16=0x%x\n", val32, val16);

    printf("mdelay(100) %luus\n", timer_get_us());
    mdelay(100);
    printf("mdelay(100) %luus\n", timer_get_us());

    printf("udelay(100000) %luus\n", timer_get_us());
    udelay(100000);
    printf("udelay(100000) %luus\n", timer_get_us());

    printf("ndelay(100000000) %luus\n", timer_get_us());
    ndelay(100000000);
    printf("ndelay(100000000) %luus\n", timer_get_us());

    srand(timer_get_us());
    for (int i = 0; i < 10; i++) {
        int ret = rand();
        printf("rand 0x%08x\n", ret);
    }
}
#endif

#if !(defined(CONFIG_ZHP100EVB_DDR_DUMMY) || defined(CONFIG_ZHP100EMU_LP4X))

static void ddr_ss_crg_release(void)
{
    int rdata;
    /*slc dual top sysreg */
    //enable slc0 aclk
    rdata = chip_rd(SLC_DUAL_SC + 0x40);
    chip_wr(SLC_DUAL_SC + 0x40, rdata | 0x200);
    //deassert slc0 sw resetn
    rdata = chip_rd(SLC_DUAL_SC + 0x40);
    chip_wr(SLC_DUAL_SC + 0x40, rdata | 0x100);
    //enable slc1 aclk
    rdata = chip_rd(SLC_DUAL_SC + 0x44);
    chip_wr(SLC_DUAL_SC + 0x44, rdata | 0x200);
    //deassert slc1 sw resetn
    rdata = chip_rd(SLC_DUAL_SC + 0x44);
    chip_wr(SLC_DUAL_SC + 0x44, rdata | 0x100);
    //deassert ddr0 sw resetn
    rdata = chip_rd(SLC_DUAL_SC + 0x48);
    chip_wr(SLC_DUAL_SC + 0x48, rdata | 0x80);
    //deassert ddr1 sw resetn
    rdata = chip_rd(SLC_DUAL_SC + 0x4c);
    chip_wr(SLC_DUAL_SC + 0x4c, rdata | 0x80);
}

// static void ddr_apb_broadcast_en(void)
// {
//     chip_wr(SLC_DUAL_SC + 0x50, 0x1);
// }

static void ddr_dch_sysreg_wr(u32 addr, u32 data)
{
    chip_wr(addr, data);
    chip_wr(addr + DDR_DCH1_OFFSET, data);
}

static void slc_crg_release(void)
{
    //slc sysreg : release slc0/1 aresetn & presetn
    ddr_dch_sysreg_wr(SYSREG_SLC_CH0 + 0x0, 0x3);
}

void ddr_pwrok_release(void)
{
    //pwrok stay low
    chip_wr(AP_AON_SYSREG_BADDR + 0X10C, 0x0);
    udelay(1); //pwrok stay low at least 8 dficlk actually, with dficlk stable
    chip_wr(AP_AON_SYSREG_BADDR + 0X10C, 0x3);
    udelay(1); //pwrok stay hight at leat 64 dficlk actually
}

void ddr_phy_reset_deassert(void)
{
    /*ddr top sysreg*/
    ddr_dch_sysreg_wr(SYSREG_DDR_CH0 + DDR_CFG0, 0x4); //phy RESET release
    ddr_dch_sysreg_wr(SYSREG_DDR_CH0 + DDR_CFG0, 0x6); //apb resetn release
}

void ddr_cfg_umctl2(struct dram_cfg_param *ddrc_cfg, int num)
{
    int i = 0;

    for (i = 0; i < num; i++) {
        ddr_dch_sysreg_wr(ddrc_cfg->reg, ddrc_cfg->val);
        ddrc_cfg++;
    }
}

static void ddr_ctrl_reset_deassert(void)
{
    ddr_dch_sysreg_wr(SYSREG_DDR_CH0 + DDR_CFG0, 0xe); //ddr cresetn
    ddr_dch_sysreg_wr(SYSREG_DDR_CH0 + DDR_CFG0, 0xe); //ddr cresetn
    ddr_dch_sysreg_wr(SYSREG_DDR_CH0 + DDR_CFG0, 0x1e); //ddr aresetn
    ddr_dch_sysreg_wr(SYSREG_DDR_CH0 + DDR_CFG0, 0x1e); //ddr aresetn
}

static void ddr_phy_dqmux(void)
{
    //PHY0 DBYTE0
    ddr_phy0_reg_wr(0x100a0, 0x0);
    ddr_phy0_reg_wr(0x100a1, 0x1);
    ddr_phy0_reg_wr(0x100a2, 0x4);
    ddr_phy0_reg_wr(0x100a3, 0x5);
    ddr_phy0_reg_wr(0x100a4, 0x7);
    ddr_phy0_reg_wr(0x100a5, 0x6);
    ddr_phy0_reg_wr(0x100a6, 0x2);
    ddr_phy0_reg_wr(0x100a7, 0x3);
    //PHY0 DBYTE1
    ddr_phy0_reg_wr(0x110a0, 0x2);
    ddr_phy0_reg_wr(0x110a1, 0x7);
    ddr_phy0_reg_wr(0x110a2, 0x4);
    ddr_phy0_reg_wr(0x110a3, 0x5);
    ddr_phy0_reg_wr(0x110a4, 0x0);
    ddr_phy0_reg_wr(0x110a5, 0x1);
    ddr_phy0_reg_wr(0x110a6, 0x3);
    ddr_phy0_reg_wr(0x110a7, 0x6);
    //PHY0 DBYTE2
    ddr_phy0_reg_wr(0x120a0, 0x3);
    ddr_phy0_reg_wr(0x120a1, 0x2);
    ddr_phy0_reg_wr(0x120a2, 0x1);
    ddr_phy0_reg_wr(0x120a3, 0x0);
    ddr_phy0_reg_wr(0x120a4, 0x4);
    ddr_phy0_reg_wr(0x120a5, 0x5);
    ddr_phy0_reg_wr(0x120a6, 0x6);
    ddr_phy0_reg_wr(0x120a7, 0x7);
    //PHY0 DBYTE3
    ddr_phy0_reg_wr(0x130a0, 0x4);
    ddr_phy0_reg_wr(0x130a1, 0x5);
    ddr_phy0_reg_wr(0x130a2, 0x7);
    ddr_phy0_reg_wr(0x130a3, 0x6);
    ddr_phy0_reg_wr(0x130a4, 0x3);
    ddr_phy0_reg_wr(0x130a5, 0x2);
    ddr_phy0_reg_wr(0x130a6, 0x0);
    ddr_phy0_reg_wr(0x130a7, 0x1);

    //PHY1 DBYTE0
    ddr_phy1_reg_wr(0x100a0, 0x0);
    ddr_phy1_reg_wr(0x100a1, 0x1);
    ddr_phy1_reg_wr(0x100a2, 0x4);
    ddr_phy1_reg_wr(0x100a3, 0x5);
    ddr_phy1_reg_wr(0x100a4, 0x7);
    ddr_phy1_reg_wr(0x100a5, 0x6);
    ddr_phy1_reg_wr(0x100a6, 0x2);
    ddr_phy1_reg_wr(0x100a7, 0x3);
    //PHY1 DBYTE1
    ddr_phy1_reg_wr(0x110a0, 0x2);
    ddr_phy1_reg_wr(0x110a1, 0x7);
    ddr_phy1_reg_wr(0x110a2, 0x4);
    ddr_phy1_reg_wr(0x110a3, 0x5);
    ddr_phy1_reg_wr(0x110a4, 0x0);
    ddr_phy1_reg_wr(0x110a5, 0x1);
    ddr_phy1_reg_wr(0x110a6, 0x3);
    ddr_phy1_reg_wr(0x110a7, 0x6);
    //PHY1 DBYTE2
    ddr_phy1_reg_wr(0x120a0, 0x3);
    ddr_phy1_reg_wr(0x120a1, 0x2);
    ddr_phy1_reg_wr(0x120a2, 0x1);
    ddr_phy1_reg_wr(0x120a3, 0x0);
    ddr_phy1_reg_wr(0x120a4, 0x4);
    ddr_phy1_reg_wr(0x120a5, 0x5);
    ddr_phy1_reg_wr(0x120a6, 0x6);
    ddr_phy1_reg_wr(0x120a7, 0x7);
    //PHY1 DBYTE3
    ddr_phy1_reg_wr(0x130a0, 0x4);
    ddr_phy1_reg_wr(0x130a1, 0x5);
    ddr_phy1_reg_wr(0x130a2, 0x7);
    ddr_phy1_reg_wr(0x130a3, 0x6);
    ddr_phy1_reg_wr(0x130a4, 0x3);
    ddr_phy1_reg_wr(0x130a5, 0x2);
    ddr_phy1_reg_wr(0x130a6, 0x0);
    ddr_phy1_reg_wr(0x130a7, 0x1);
}

void ddr_enter_mission_mode(void)
{
    ddr_dch_sysreg_wr(SWCTL(0), 0x00000000);
    ddr_dch_sysreg_wr(INIT0(0), 0xc0020002); //skip sdram initialization
    ddr_dch_sysreg_wr(SWCTL(0), 0x00000001);

    ddr_dch_sysreg_wr(SWCTL(0), 0x00000000);
    ddr_dch_sysreg_wr(DFIMISC(0), 0x00000034); // [5]dfi_init_start

    while (chip_rd(DFISTAT(0)) != 0x00000001) {
        ; //polling dfi_init_complete
    }

    while (chip_rd(DFISTAT(1)) != 0x00000001) { 
        ; //polling dfi_init_complete
    }

    ddr_dch_sysreg_wr(DFIMISC(0), 0x00000014);
    ddr_dch_sysreg_wr(DFIMISC(0), 0x00000015);
    ddr_dch_sysreg_wr(PWRCTL(0), 0x0000010B); //[8] lpddr4_sr_allowed [3] dfi_dram_clk_disable [1] powerdown_en [0]selfref_en

    ddr_dch_sysreg_wr(SWCTL(0), 0x00000001);

    while (chip_rd(SWSTAT(0)) != 0x00000001) {
        ;
    }
    while (chip_rd(SWSTAT(1)) != 0x00000001) {
        ;
    }

    while (chip_rd(STAT(0)) != 0x00000001) {
        ;
    }
    while (chip_rd(STAT(1)) != 0x00000001) {
        ;
    }

    ddr_dch_sysreg_wr(DFIPHYMSTR(0), 0x14000001);

    while (chip_rd(SWSTAT(0)) != 0x00000001) {
        ;
    }
    while (chip_rd(SWSTAT(1)) != 0x00000001) {
        ;
    }
}

static void ddr_axi_port_en(void)
{
    ddr_dch_sysreg_wr(DBG1(0), 0);
    ddr_dch_sysreg_wr(PCTRL_0(0), 1);
}

static void ddr_auto_refresh_en(void)
{
    ddr_dch_sysreg_wr(RFSHCTL3(0), 0x0);
}
#endif

int ddr_init(enum ddr_type type)
{
    struct dram_timing_info *dram_timing = NULL;
    void (* ddr_phy_training)(unsigned int *) = NULL;

#ifdef UTILS_TEST
    utils_test();
#endif

#ifdef CONFIG_ZHP100EVB_DDR_DUMMY
    /* dummy ddr do nothing */
#elif defined(CONFIG_ZHP100EMU_LP4X)
    extern void emu_init_ddr(void);
    emu_init_ddr();
#else
    switch(type) {
        case DDR_LP4X_4266_1Rank_2GBx2:
        case DDR_LP4X_4266_1Rank_4GBx2:
            dram_timing = &dram_timing_4266_1r;
            ddr_phy_training = &ddr_phy_training_4266_1r;
            break;
        case DDR_LP4X_4266_2Rank_8GBx2:
            dram_timing = &dram_timing_4266_2r;
            ddr_phy_training = &ddr_phy_training_4266_2r;
            break;
        default:
            printf("unsupported type:%d\n", type);
            break;
    }

    if (dram_timing == NULL) {
        return -1;
    }

    if (dram_timing->ddrc_cfg == NULL) {
        return -1;
    }

    int initial_drate = (int)dram_timing->fsp_table[0];
    debug("DDRINFO: start DDR init, data rate: %d \n", initial_drate);
    //broadcast mode for ddr ch0 and ch1, for future fast ddr init
    // ddr_apb_broadcast_en();

    /* Step1: Follow the crg up procedure */
    // default to the frequency point 0 clock
    spl_switch_ddrpll(initial_drate);

    //ddr top crg release
    ddr_ss_crg_release();
    //slc internal crg release,ch0 and ch1
    slc_crg_release();
    //phy pwrok release, cold reset
    ddr_pwrok_release();
    //phy RESET and apbresetn release
    ddr_phy_reset_deassert();

    /* Step2: Program the dwc_ddr_umctl2 registers */
    debug("DDRINFO: ddrc config start\n");
    ddr_cfg_umctl2(dram_timing->ddrc_cfg, dram_timing->ddrc_cfg_num);
    debug("DDRINFO: ddrc config done\n");

    /* Step3: De-assert reset signal(core_ddrc_rstn & aresetn_n) */
    ddr_ctrl_reset_deassert();

    /*
     * Step4: Start PHY initialization and training by
     * accessing relevant PUB registers
     */
    debug("DDRINFO:ddrphy config start\n");

    ddr_phy_dqmux();

    ddr_phy_training(dram_timing->fsp_table);

    debug("DDRINFO: ddrphy config done\n");

#ifdef CONFIG_LPDDR_EYE
    lp4_diag_eye();
#else

    /* save the ddr PHY trained CSR in memory for low power use */
    // ddrphy_trained_csr_save(ddrphy_trained_csr, ddrphy_trained_csr_num);

    /* Step5:  Initialize the PHY to Mission Mode through DFI Initializatio*/
    ddr_enter_mission_mode();

    /* Step6:  axi port enable*/
    ddr_axi_port_en();
    /* Step7:  auto refresh enable*/
    ddr_auto_refresh_en();
    /* Step8:  auto self refresh enable*/

    debug("DDRINFO: DDR init pass!\n");

#endif

#endif
    return 0;
}

#define  DDR0_MT_BADDR   0x0004820000
#define  DDR1_MT_BADDR   0x0005820000

#define DDR0_SLC_SYSREG 0x0004861000
#define DDR1_SLC_SYSREG 0x0005861000


void ddr_mt_ocd_sel(u32 ch)
{
    u32 rdata;
    if (ch == 0) {
        rdata = chip_rd(DDR0_SLC_SYSREG+0x4);
        rdata |= (0x1<<2); 
        chip_wr(DDR0_SLC_SYSREG + 0x4, rdata);
    } else
    {
        rdata = chip_rd(DDR1_SLC_SYSREG+0x4);
        rdata |= (0x1<<2); 
        chip_wr(DDR1_SLC_SYSREG + 0x4, rdata);
    }
}


void ddr_mt_cfg(u32 ch, u32 offset, u32 v)
{
    if (ch == 0) {
        chip_wr(DDR0_MT_BADDR + offset, v);
    } else
    {
        chip_wr(DDR1_MT_BADDR + offset, v);
    }

}

int ddr_mt_rd(u32 ch, u32 offset)
{
    u32 rdata=0;
    if (ch == 0) {
        rdata=chip_rd(DDR0_MT_BADDR + offset);
    } else
    {
        rdata=chip_rd(DDR1_MT_BADDR + offset);
    }
    return rdata;
}

static void ddr_mt_single(u32 ch) {
    ddr_mt_ocd_sel(ch);
    ddr_mt_cfg(ch,0x08, 0x02008000);//start adddr left shift 8bit,fix id

    ddr_mt_cfg(ch,0x0c, 0x1b11f100);
    ddr_mt_cfg(ch,0x10, 0x07fd07fd);    //[2:0]:awsize=5; [7:3]:aw ostd; [15:8]:awlen=7;
                                        //[18:16]:arsize;[23:19]:ar ostd;[31:24]:arlen
                                        //32B*8LEN, unlimt OSTD
    // ddr_mt_cfg(ch,0x14, 0x01400000);    //[23:0]:xact_num;[31:24]:loop num(4M*256B=1GB)
    ddr_mt_cfg(ch,0x14, 0x01000fff);    //[23:0]:xact_num;[31:24]:loop num(4M*256B=1GB)

    // ddr_mt_cfg(ch,0x18, 0x12153524);//prbs seed
    ddr_mt_cfg(ch,0x18, rand());//prbs seed
    ddr_mt_cfg(ch,0x1c, 0x00400000); // 0x00002000 -> 0x4000_0000
    ddr_mt_cfg(ch,0x2c, 0xffffffff); // msk
    ddr_mt_cfg(ch,0x00, 0x9956000e);//user pattern, prbs_23
}

static void ddr_mt_infinite(u32 ch)
{
    ddr_mt_ocd_sel(ch);
    ddr_mt_cfg(ch,0x08, 0x02008000);//start adddr left shift 8bit,fix id

    ddr_mt_cfg(ch,0x0c, 0x1b11f100);
    ddr_mt_cfg(ch,0x10, 0x07fd07fd);//[2:0]:awsize; [7:3]:aw ostd; [15:8]:awlen;[18:16]:arsize;[23:19]:ar ostd;[31:24]:arlen
                                        //32B*8LEN, unlimt OSTD
    ddr_mt_cfg(ch,0x14, 0xff000FFF);//[23:0]:xact_num;[31:24]:loop num
                                        //4096, inifinite loop

    ddr_mt_cfg(ch,0x18, rand());//prbs seed


    ddr_mt_cfg(ch,0x1c, 0x00400000); // 0x00002000 -> 0x4000_0000
    ddr_mt_cfg(ch,0x2c, 0x005fffff); // 0x00002fff

    ddr_mt_cfg(ch,0x00, 0x2A56000E);//user pattern, 0x5a 0x2a50000e
    ddr_mt_cfg(ch,0x00, 0x2A56000F);//mt en 
}

uint64_t times;

static void  ddr_mt_result_check_single(u32 ch)
{
    chip_wr(0xC0000000, 0xbeefbeef);
    chip_wr(0xCF000000, 0xdeaddead);

    //flush cache
    flush_dcache_range((unsigned long)0xC0000000, (unsigned long)(0xC0000000 + CONFIG_SYS_CACHELINE_SIZE));
    flush_dcache_range((unsigned long)0xCF000000, (unsigned long)(0xCF000000 + CONFIG_SYS_CACHELINE_SIZE));

    ddr_mt_cfg(ch,0x00, 0x9956000f);//user pattern, prbs_23 mt en 

    if (ch == 0) {
        while(1) {
            u32 rdata = chip_rd(DDR0_MT_BADDR + 0x04);
            if((rdata & 0x8) == 0) { // test_done
                continue;
            }
            udelay(1);
            /* wait & poll */
            times++;
            rdata = chip_rd(DDR0_MT_BADDR + 0x04);
            if((rdata & 0x1) == 0) { // check_result
                if (times % 100 == 0) {
                    printf("CH0 DDR MT is Runing, everything ok,status is:0x%x ! addr=0x%x size=512M\n",rdata, ddr_mt_rd(0, 0x1c) << 8);
                    printf("CH0 DDR  MT  AXI status:0x%x !\n",chip_rd(DDR0_MT_BADDR + 0x40));
                    break;
                }
            } else {
                printf("CH0 DDR MT MT has Error, status is:0x%x !\n",rdata);
                while(1) {;}
            }
        }
    }
    
    // /* wait & poll */
    // rdata = chip_rd(DDR1_MT_BADDR + 0x04);
    // if((rdata & 0x1) == 0) {
    //     if (times % 100 == 0)
    //         printf("CH1 DDR MT is Runing, everything ok,status is:0x%x ! addr=0x%x size=512M\n",rdata, ddr_mt_rd(1, 0x1c) << 8);
    // } else {
    //     printf("CH1 DDR MT MT has Error, status is:0x%x !\n",rdata);
    //     while(1) {;}
    // }
    // if (times % 100 == 0)
    //     printf("CH1 DDR  MT  AXI status:0x%x !\n",chip_rd(DDR1_MT_BADDR + 0x40));
    printf("0xC0000000 read 0x%x\n", chip_rd(0xC0000000));
    printf("0xCF000000 read 0x%x\n", chip_rd(0xCF000000));
}

static void  ddr_mt_result_check(void)
{
    static unsigned long long int ddr0_pass = 0;
    static unsigned long long int ddr0_fail = 0;
    static unsigned long long int ddr1_pass = 0;
    static unsigned long long int ddr1_fail = 0;

    /* wait & poll */
    times++;
    u32 rdata = chip_rd(DDR0_MT_BADDR + 0x04);
    if((rdata & 0x1) == 0) {
        if (times % 100 == 0)
            printf("CH0 DDR MT is Runing, everything ok,status is:0x%x ! addr=0x%x size=512M pass count=%llu fail count=%llu\n",
                    rdata, ddr_mt_rd(0, 0x1c) << 8, ++ddr0_pass, ddr0_fail);
    } else {
        printf("CH0 DDR MT MT has Error, status is:0x%x ! fail count=%llu\n",rdata, ++ddr0_fail);
        // while(1) {;}
    }
    if (times % 100 == 0)
        printf("CH0 DDR  MT  AXI status:0x%x !\n",chip_rd(DDR0_MT_BADDR + 0x40));
    
    /* wait & poll */
    rdata = chip_rd(DDR1_MT_BADDR + 0x04);
    if((rdata & 0x1) == 0) {
        if (times % 100 == 0)
            printf("CH1 DDR MT is Runing, everything ok,status is:0x%x ! addr=0x%x size=512M pass count=%llu fail count=%llu\n",
                    rdata, ddr_mt_rd(1, 0x1c) << 8, ++ddr1_pass, ddr1_fail);
    } else {
        printf("CH1 DDR MT MT has Error, status is:0x%x ! fail count=%llu\n",rdata, ++ddr1_fail);
        // while(1) {;}
    }
    if (times % 100 == 0)
        printf("CH1 DDR  MT  AXI status:0x%x !\n",chip_rd(DDR1_MT_BADDR + 0x40));
}

void ddr_dfmu_mt_test_single(void)
{
    printf("now using PRBS23!\n");
    while(1) {
        ddr_mt_single(0);
        // ddr_mt_single(1);
            udelay(200);
            ddr_mt_result_check_single(0);
            // ddr_mt_result_check_single(1);
    }

}

void ddr_dfmu_mt_test(void)
{
    ddr_mt_infinite(0);
    ddr_mt_infinite(1);
    while(1) {
        udelay(5000);
        ddr_mt_result_check();
    }
}

int lp4_mrr(int addr, int ddrc) {
    uint32_t ddrc_base = 0x04800000;
    if (ddrc == 1)
        ddrc_base = 0x05800000;

DWC_DDR_UMCTL2_C_STRUCT_REG_S umctl2_reg;
    umctl2_reg.dwc_ddr_umctl2_c_struct_mrctrl0.u32 = chip_rd(ddrc_base + 0x10);
    //umctl2_reg.dwc_ddr_umctl2_c_struct_mrctrl0.mr_addr = addr; //do not care for lp4
    umctl2_reg.dwc_ddr_umctl2_c_struct_mrctrl0.mr_rank = 0x1;//rank0 only
    umctl2_reg.dwc_ddr_umctl2_c_struct_mrctrl0.mr_type = 1;//read
    chip_wr(ddrc_base + 0x10, umctl2_reg.dwc_ddr_umctl2_c_struct_mrctrl0.u32);

    umctl2_reg.dwc_ddr_umctl2_c_struct_mrctrl1.u32 = chip_rd(ddrc_base + 0x14);
    umctl2_reg.dwc_ddr_umctl2_c_struct_mrctrl1.mr_data = addr << 8;
    chip_wr(ddrc_base + 0x14, umctl2_reg.dwc_ddr_umctl2_c_struct_mrctrl1.u32);

    umctl2_reg.dwc_ddr_umctl2_c_struct_mrctrl1.u32 = chip_rd(ddrc_base + 0x14);
    umctl2_reg.dwc_ddr_umctl2_c_struct_mrctrl1.mr_data = addr << 8;
    chip_wr(ddrc_base + 0x14, umctl2_reg.dwc_ddr_umctl2_c_struct_mrctrl1.u32);


    umctl2_reg.dwc_ddr_umctl2_c_struct_mrctrl0.u32 = chip_rd(ddrc_base + 0x10);
    umctl2_reg.dwc_ddr_umctl2_c_struct_mrctrl0.mr_wr = 1;//trigger wr/rd
    chip_wr(ddrc_base + 0x10, umctl2_reg.dwc_ddr_umctl2_c_struct_mrctrl0.u32);

    udelay(20);
    while ((chip_rd(ddrc_base + 0x18) & 0x1) == 0x1);
    return chip_rd(ddrc_base + 0x10074);
}

void ddr_registers_dump(void)
{
    printf("DDR SS data version=0x%x\n", chip_rd(0x04900000));
    printf("DDR PLL lock=%d\n", chip_rd(0x04900018) & 0x1);
    chip_wr(0x04900024, 0x0);
    chip_wr(0x04900024, 0x1);
    mdelay(10);
    printf("DDR PLL=0x%x KHz\n", chip_rd(0x04900020));
    printf("AMUX_HWLP_CH0=0x%x\n", chip_rd(0x04900040));
    printf("AMUX_HWLP_CH1=0x%x\n", chip_rd(0x04900044));
    printf("DDRC_HWLP_CH0=0x%x\n", chip_rd(0x04900048));
    printf("DDRC_HWLP_CH1=0x%x\n", chip_rd(0x0490004c));
    printf("SLC_CFG0=0x%x\n", chip_rd(0x04861000));
    printf("SLC_ICG0=0x%x\n", chip_rd(0x04861008));
    printf("DDR_CFG0=0x%x\n", chip_rd(0x04810000));
    printf("DDR_ICG0=0x%x\n", chip_rd(0x04810008));
    printf("SLC_CFG1=0x%x\n", chip_rd(0x05861000));
    printf("SLC_ICG1=0x%x\n", chip_rd(0x05861008));
    printf("DDR_CFG1=0x%x\n", chip_rd(0x05810000));
    printf("DDR_ICG1=0x%x\n", chip_rd(0x05810008));
    printf("DDRC0 VER NUMBER=0x%x\n", chip_rd(0x04800000));
    printf("DDRC1 VER NUMBER=0x%x\n", chip_rd(0x05800000));
    printf("AMUX_OST0=0x%x\n", chip_rd(0x04861010));
    printf("CAM_STATS0=0x%x\n", chip_rd(0x04810030));
    printf("WR_OSTD_CNT0=0x%x\n", chip_rd(0x04841000));
    printf("RD_OSTD_CNT0=0x%x\n", chip_rd(0x04841004));
    printf("AW_STATS=0x%x\n", chip_rd(0x04841008));
    printf("AR_STATS=0x%x\n", chip_rd(0x0484100c));
    printf("AMUX_OST0=0x%x\n", chip_rd(0x05861010));
    printf("CAM_STATS0=0x%x\n", chip_rd(0x05810030));
    printf("WR_OSTD_CNT0=0x%x\n", chip_rd(0x05841000));
    printf("RD_OSTD_CNT0=0x%x\n", chip_rd(0x05841004));
    printf("AW_STATS=0x%x\n", chip_rd(0x05841008));
    printf("AR_STATS=0x%x\n", chip_rd(0x0584100c));

    printf("DDRC0 STAT=0x%x\n", chip_rd(0x04800004));
    printf("DDRC0 DFISTAT=0x%x\n", chip_rd(0x048001bc));
    printf("DDRC0 PSTAT=0x%x\n", chip_rd(0x048003fc));
    printf("DDRC1 STAT=0x%x\n", chip_rd(0x05800004));
    printf("DDRC1 DFISTAT=0x%x\n", chip_rd(0x058001bc));
    printf("DDRC1 PSTAT=0x%x\n", chip_rd(0x058003fc));

    ddr_phy_reg_wr(0xd0000, 0x0);
    ddr_phy_reg_wr(0xc0080, 0x3);
    printf("phy0 reg 0x1004d = 0x%x\n", ddr_phys_reg_rd(0, 0x1004d));
    printf("phy0 reg 0x1014d = 0x%x\n", ddr_phys_reg_rd(0, 0x1014d));
    printf("phy0 reg 0x1104d = 0x%x\n", ddr_phys_reg_rd(0, 0x1104d));
    printf("phy0 reg 0x1114d = 0x%x\n", ddr_phys_reg_rd(0, 0x1114d));
    printf("phy0 reg 0x1204d = 0x%x\n", ddr_phys_reg_rd(0, 0x1204d));
    printf("phy0 reg 0x1214d = 0x%x\n", ddr_phys_reg_rd(0, 0x1214d));
    printf("phy0 reg 0x1304d = 0x%x\n", ddr_phys_reg_rd(0, 0x1304d));
    printf("phy0 reg 0x1314d = 0x%x\n", ddr_phys_reg_rd(0, 0x1314d));
    printf("phy0 reg 0x10049 = 0x%x\n", ddr_phys_reg_rd(0, 0x10049));
    printf("phy0 reg 0x10149 = 0x%x\n", ddr_phys_reg_rd(0, 0x10149));
    printf("phy0 reg 0x11049 = 0x%x\n", ddr_phys_reg_rd(0, 0x11049));
    printf("phy0 reg 0x11149 = 0x%x\n", ddr_phys_reg_rd(0, 0x11149));
    printf("phy0 reg 0x12049 = 0x%x\n", ddr_phys_reg_rd(0, 0x12049));
    printf("phy0 reg 0x12149 = 0x%x\n", ddr_phys_reg_rd(0, 0x12149));
    printf("phy0 reg 0x13049 = 0x%x\n", ddr_phys_reg_rd(0, 0x13049));
    printf("phy0 reg 0x13149 = 0x%x\n", ddr_phys_reg_rd(0, 0x13149));
    printf("phy0 reg 0x43 = 0x%x\n", ddr_phys_reg_rd(0, 0x43));
    printf("phy0 reg 0x1043 = 0x%x\n", ddr_phys_reg_rd(0, 0x1043));
    printf("phy0 reg 0x2043 = 0x%x\n", ddr_phys_reg_rd(0, 0x2043));
    printf("phy0 reg 0x3043 = 0x%x\n", ddr_phys_reg_rd(0, 0x3043));
    printf("phy0 reg 0x4043 = 0x%x\n", ddr_phys_reg_rd(0, 0x4043));
    printf("phy0 reg 0x5043 = 0x%x\n", ddr_phys_reg_rd(0, 0x5043));
    printf("phy0 reg 0x6043 = 0x%x\n", ddr_phys_reg_rd(0, 0x6043));
    printf("phy0 reg 0x7043 = 0x%x\n", ddr_phys_reg_rd(0, 0x7043));
    printf("phy0 reg 0x8043 = 0x%x\n", ddr_phys_reg_rd(0, 0x8043));
    printf("phy0 reg 0x9043 = 0x%x\n", ddr_phys_reg_rd(0, 0x9043));
    printf("phy0 reg 0x20088 = 0x%x\n", ddr_phys_reg_rd(0, 0x20088));
    printf("phy0 reg 0x200b2 = 0x%x\n", ddr_phys_reg_rd(0, 0x200b2));
    printf("phy0 reg 0x10043 = 0x%x\n", ddr_phys_reg_rd(0, 0x10043));
    printf("phy0 reg 0x10143 = 0x%x\n", ddr_phys_reg_rd(0, 0x10143));
    printf("phy0 reg 0x11043 = 0x%x\n", ddr_phys_reg_rd(0, 0x11043));
    printf("phy0 reg 0x11143 = 0x%x\n", ddr_phys_reg_rd(0, 0x11143));
    printf("phy0 reg 0x12043 = 0x%x\n", ddr_phys_reg_rd(0, 0x12043));
    printf("phy0 reg 0x12143 = 0x%x\n", ddr_phys_reg_rd(0, 0x12143));
    printf("phy0 reg 0x13043 = 0x%x\n", ddr_phys_reg_rd(0, 0x13043));
    printf("phy0 reg 0x13143 = 0x%x\n", ddr_phys_reg_rd(0, 0x13143));

    printf("phy1 reg 0x1004d = 0x%x\n", ddr_phys_reg_rd(1, 0x1004d));
    printf("phy1 reg 0x1014d = 0x%x\n", ddr_phys_reg_rd(1, 0x1014d));
    printf("phy1 reg 0x1104d = 0x%x\n", ddr_phys_reg_rd(1, 0x1104d));
    printf("phy1 reg 0x1114d = 0x%x\n", ddr_phys_reg_rd(1, 0x1114d));
    printf("phy1 reg 0x1204d = 0x%x\n", ddr_phys_reg_rd(1, 0x1204d));
    printf("phy1 reg 0x1214d = 0x%x\n", ddr_phys_reg_rd(1, 0x1214d));
    printf("phy1 reg 0x1304d = 0x%x\n", ddr_phys_reg_rd(1, 0x1304d));
    printf("phy1 reg 0x1314d = 0x%x\n", ddr_phys_reg_rd(1, 0x1314d));
    printf("phy1 reg 0x10049 = 0x%x\n", ddr_phys_reg_rd(1, 0x10049));
    printf("phy1 reg 0x10149 = 0x%x\n", ddr_phys_reg_rd(1, 0x10149));
    printf("phy1 reg 0x11049 = 0x%x\n", ddr_phys_reg_rd(1, 0x11049));
    printf("phy1 reg 0x11149 = 0x%x\n", ddr_phys_reg_rd(1, 0x11149));
    printf("phy1 reg 0x12049 = 0x%x\n", ddr_phys_reg_rd(1, 0x12049));
    printf("phy1 reg 0x12149 = 0x%x\n", ddr_phys_reg_rd(1, 0x12149));
    printf("phy1 reg 0x13049 = 0x%x\n", ddr_phys_reg_rd(1, 0x13049));
    printf("phy1 reg 0x13149 = 0x%x\n", ddr_phys_reg_rd(1, 0x13149));
    printf("phy1 reg 0x43 = 0x%x\n", ddr_phys_reg_rd(1, 0x43));
    printf("phy1 reg 0x1043 = 0x%x\n", ddr_phys_reg_rd(1, 0x1043));
    printf("phy1 reg 0x2043 = 0x%x\n", ddr_phys_reg_rd(1, 0x2043));
    printf("phy1 reg 0x3043 = 0x%x\n", ddr_phys_reg_rd(1, 0x3043));
    printf("phy1 reg 0x4043 = 0x%x\n", ddr_phys_reg_rd(1, 0x4043));
    printf("phy1 reg 0x5043 = 0x%x\n", ddr_phys_reg_rd(1, 0x5043));
    printf("phy1 reg 0x6043 = 0x%x\n", ddr_phys_reg_rd(1, 0x6043));
    printf("phy1 reg 0x7043 = 0x%x\n", ddr_phys_reg_rd(1, 0x7043));
    printf("phy1 reg 0x8043 = 0x%x\n", ddr_phys_reg_rd(1, 0x8043));
    printf("phy1 reg 0x9043 = 0x%x\n", ddr_phys_reg_rd(1, 0x9043));
    printf("phy1 reg 0x20088 = 0x%x\n", ddr_phys_reg_rd(1, 0x20088));
    printf("phy1 reg 0x200b2 = 0x%x\n", ddr_phys_reg_rd(1, 0x200b2));
    printf("phy1 reg 0x10043 = 0x%x\n", ddr_phys_reg_rd(1, 0x10043));
    printf("phy1 reg 0x10143 = 0x%x\n", ddr_phys_reg_rd(1, 0x10143));
    printf("phy1 reg 0x11043 = 0x%x\n", ddr_phys_reg_rd(1, 0x11043));
    printf("phy1 reg 0x11143 = 0x%x\n", ddr_phys_reg_rd(1, 0x11143));
    printf("phy1 reg 0x12043 = 0x%x\n", ddr_phys_reg_rd(1, 0x12043));
    printf("phy1 reg 0x12143 = 0x%x\n", ddr_phys_reg_rd(1, 0x12143));
    printf("phy1 reg 0x13043 = 0x%x\n", ddr_phys_reg_rd(1, 0x13043));
    printf("phy1 reg 0x13143 = 0x%x\n", ddr_phys_reg_rd(1, 0x13143));

    for (int ddrc = 0; ddrc < 2; ddrc++) {
        for (int mr = 0; mr < 8; mr++) {
            printf("ddrc%d mr%d=0x%x\n",ddrc, mr, lp4_mrr(mr, ddrc));
        }
    }
}

u64 ddr_determine_size(enum ddr_type type)
{
    switch(type) {
        case DDR_LP4X_4266_1Rank_2GBx2:
            return 0x100000000; // 4GB
        case DDR_LP4X_4266_1Rank_4GBx2:
            return 0x200000000; // 8GB
        case DDR_LP4X_4266_2Rank_8GBx2:
            return 0x400000000; // 16GB
        default:
            printf("unsupported type:%d\n", type);
            return 0;
    }
}
