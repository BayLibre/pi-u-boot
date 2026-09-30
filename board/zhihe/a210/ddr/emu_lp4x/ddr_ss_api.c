#ifndef CONFIG_STUB_DDR_SS
#include <string.h>
#include "../../include/utils/utils.h"
#include "../../include/addr_defines.h"

#include "ddr_ss_reg.h"

#define USE_LPDDR4
#define INIT_4266
#define CONFIG_DDR_DBI_OFF

static void lpddr4_init_4266_ctrl_sdram_init(void);
#ifdef DDR_INIT_DRANK
static void lpddr4_addr_map_drank(void);
static void lpddr4_ctrl_init_4266_drank(void);
#endif
static void ddr_apb_broadcast_en(void);
static void pll_config (int speed);
static void ddr_slc_top_crg_release(void);
static void ddr_deassert_pwrok_apb(void);
static void lpddr4_ctrl_init_4266_srank(void);
static void addr_map(void);
static void de_assert_other_reset_ddr(void);
static void ctrl_en(void);
static void enable_axi_port(void);
static void enable_auto_refresh(void);
static void slc_crg_release(void);

#ifdef lpddr4_skiptrain_4266
static void dwc_ddrphy_phyinit_out_lpddr4_skiptrain_4266(void);
static void dwc_ddrphy_phyinit_userCustom_overrideUserInput(void);
static void dwc_ddrphy_phyinit_userCustom_A_bringupPower(void);
static void dwc_ddrphy_phyinit_userCustom_B_startClockResetPhy(void);
static void dwc_ddrphy_phyinit_userCustom_customPostTrain(void);
static void dwc_ddrphy_phyinit_userCustom_J_enterMissionMode(void);
static void ddr_phy_reg_wr(unsigned int addr,unsigned int wr_data);
#endif

#ifdef ENABLE_DDR_INLINE_ECC
static void lpddr4_init_ctrl_sdram_init_inline_ecc_enable(void);
#endif

#ifdef SPL_ENABLE_DDR_SCRAMBLE
static void enable_ddr_scramble(void);
#endif

//#define ENABLE_DDR_INLINE_ECC
//#define SPL_ENABLE_DDR_SCRAMBLE
void emu_init_ddr(void) {

// TODO, by dongf//    udelay(10);
// TODO, by dongf//    #ifdef USE_LPDDR4
// TODO, by dongf//        #ifdef INIT_4266
// TODO, by dongf//            #ifdef HWFFC_ENABLE
// TODO, by dongf//            lpddr4_init_ctrl_sdram_init();
// TODO, by dongf//            #else
#ifdef ENABLE_DDR_INLINE_ECC
    lpddr4_init_ctrl_sdram_init_inline_ecc_enable();
#else
    lpddr4_init_4266_ctrl_sdram_init(); // TODO, use it
#endif
// TODO, by dongf//            #endif
// TODO, by dongf//        #elif INIT_3733
// TODO, by dongf//            lpddr4_init_3733_ctrl_sdram_init();
// TODO, by dongf//        #elif INIT_3200
// TODO, by dongf//            lpddr4_init_3200_ctrl_sdram_init();
// TODO, by dongf//        #elif INIT_1066
// TODO, by dongf//            lpddr4_init_1066_ctrl_sdram_init();
// TODO, by dongf//        #else
// TODO, by dongf//            printf("DDR NOT INIT !\n ");
// TODO, by dongf//            finish(FAIL);
// TODO, by dongf//        #endif
// TODO, by dongf//    //USE_DDR4
// TODO, by dongf//    #else
// TODO, by dongf//        #ifdef INIT_3200
// TODO, by dongf//        ddr4_init_3200_ctrl_sdram_init();
// TODO, by dongf//        #else
// TODO, by dongf//        printf("DDR NOT INIT !\n ");
// TODO, by dongf//        finish(FAIL);
// TODO, by dongf//        #endif
// TODO, by dongf//    #endif
    //}}}
#ifdef SPL_ENABLE_DDR_SCRAMBLE
    enable_ddr_scramble();
#endif
}


#ifdef DDR_INIT_DRANK
static void lpddr4_addr_map_drank(void) {
    wr(ADDRMAP0,0x001f0017); 
    wr(ADDRMAP1,0x00080808); 
    wr(ADDRMAP2,0x00000000); 
    wr(ADDRMAP3,0x00000000); 
    wr(ADDRMAP4,0x00001f1f); 
    wr(ADDRMAP5,0x070f0707); 
    wr(ADDRMAP6,0x07070707); 
    wr(ADDRMAP7,0x00000f0f);
    wr(ADDRMAP9,0x07070707);
    wr(ADDRMAP10,0x07070707);
    wr(ADDRMAP11,0x00000007);
}
#endif

static void lpddr4_init_4266_ctrl_sdram_init(void)
{ 
    ddr_apb_broadcast_en();
    //pll_cfg(4266);
    //printf("[init_ddr]pll_cfg 4266 success \n");
    pll_config(4266);
    printf("[init_ddr]pll_config 4266 success \n");    

    ddr_slc_top_crg_release();
    slc_crg_release();

    //de_assert_pwrok_apb();
    //printf("[init_ddr]de_assert_pwrok_apb success \n");
    ddr_deassert_pwrok_apb();
    printf("[init_ddr]ddr_deassert_pwrok_apb success \n");
    //ctrl_phy();
#ifndef DDR_INIT_DRANK
    lpddr4_ctrl_init_4266_srank();//需要确认AXI port数量
    printf("[init_ddr]lpddr4_ctrl_init_4266_srank success \n");
    addr_map();
#else // DDR_INIT_DRANK
    lpddr4_ctrl_init_4266_drank();//需要确认AXI port数量, TODO, by dongf
    printf("[init_ddr]lpddr4_ctrl_init_4266_srank success \n");
    lpddr4_addr_map_drank();
#endif // DDR_INIT_DRANK

    printf("[init_ddr]addrmap success \n");
    de_assert_other_reset_ddr();//需要新环境适配,当前为空
    printf("[init_ddr]de_assert_other_reset_ddr success \n");
    //dwc_ddrphy_phyinit_out_lpddr4_skiptrain();

//#ifdef CONFIG_DDR_DBI_OFF
//    printf("[init_ddr]dwc_ddrphy_phyinit_out_lpddr4_skiptrain_4266()... \n");
//    dwc_ddrphy_phyinit_out_lpddr4_skiptrain_4266();
//#else
//    printf("[init_ddr]dwc_ddrphy_phyinit_out_lpddr4_skiptrain_4266_dbi()... \n");
//    dwc_ddrphy_phyinit_out_lpddr4_skiptrain_4266_dbi();
//#endif

    printf("[init_ddr]dwc_ddrphy_phyinit_out_lpddr4_skiptrain success \n");
    //ctrl_enable();
    ctrl_en();
    printf("[init_ddr]ctrl_en success \n");
    //enable_axi_port(1);
    enable_axi_port();//需要确认AXI port数量
    printf("[init_ddr]enable_axi_port success \n");
    //en_auto_refresh();
    enable_auto_refresh();
    printf("[init_ddr]enable_auto_refresh success \n");

}

static void lpddr4_ctrl_init_4266_srank(void)
{
    wr(DBG1,0x00000001);
    wr(PWRCTL,0x00000001);
    while(rd(STAT)!=0x00000000);
    wr(MSTR,0x01080020);
    wr(MRCTRL0,0x00003030);
    wr(MRCTRL1,0x0002d90f);
    //wr(DERATEEN,0x00001404);
    wr(DERATEEN,0x000014f5);
    //wr(DERATEINT,0x726d4ada);
    wr(DERATEINT,0x40000000);
    wr(DERATECTL,0x00000001);
    //wr(PWRCTL,0x00000000);
    wr(PWRCTL,0x00000020);
    wr(PWRTMG,0x0040ae04);
    wr(HWLPCTL,0x00430002);
    //wr(RFSHCTL0,0x00210000);
    wr(RFSHCTL0,0x00210004);
    wr(RFSHCTL1,0x000f0026);
    wr(RFSHCTL3,0x00000001);
    //wr(RFSHTMG,0x00828195);
    wr(RFSHTMG,0x82000098); 
    wr(RFSHTMG1,0x00610000);
    wr(CRCPARCTL0,0x00000000);
    wr(INIT0,0x00020002);
    wr(INIT1,0x00010002);
    wr(INIT2,0x00002300);
    wr(INIT3,0x0074003f);
#ifdef CONFIG_DDR_DBI_OFF
    wr(INIT4,0x00320000);//INIT:MR3 MR13
#else
    wr(INIT4,0x00f20000);//INIT:MR3 MR13
#endif
    wr(INIT5,0x0005000c);
    wr(INIT6,0x0000004d);
    wr(INIT7,0x0000004d);
    wr(DIMMCTL,0x00000000);
    //wr(RANKCTL,0x0000032f);
    wr(RANKCTL,0x0000ab9f);
    //wr(RANKCTL1,0x00000004);
    wr(RANKCTL1,0x0000001a);
    wr(DRAMTMG0,0x2221482d);
    wr(DRAMTMG1,0x00090941);
#ifdef CONFIG_DDR_DBI_OFF
    wr(DRAMTMG2,0x09121219);//[29:24] -write latency [21:16] read latency [13:8] rd2wr [5:0] wr2rd
    printf("Enter LP4 DBI OFF mode\n");
#else
    //wr(DRAMTMG2,0x09141619);//[29:24] -write latency [21:16] read latency [13:8] rd2wr [5:0] wr2rd
    printf("Enter LP4 DBI ON mode\n");
    wr(DRAMTMG2,0x09141f1a);//[29:24] -write latency [21:16] read latency [13:8] rd2wr [5:0] wr2rd
#endif
    wr(DRAMTMG3,0x00f0f000);
    wr(DRAMTMG4,0x14040914);
    wr(DRAMTMG5,0x02061111);
    wr(DRAMTMG6,0x0101000a);
    wr(DRAMTMG7,0x00000602);
    wr(DRAMTMG8,0x00000101);
    wr(DRAMTMG12,0x00020000);
    wr(DRAMTMG13,0x0e100002);
    wr(DRAMTMG14,0x00000133);
    //wr(ZQCTL0,0xc42d0021);
    wr(ZQCTL0,0xc42d0026);
    //wr(ZQCTL1,0x0360ccda);
    wr(ZQCTL1,0x03600800);
    wr(ZQCTL2,0x00000000);

    //DBI-off
    //wr(DFITMG0,0x049f820e);//[28:24] dft_t_ctrl_delay [22:16] dfi_t_rddate_en=RL-5

#ifdef CONFIG_DDR_DBI_OFF
    wr(DFITMG0,0x049f820e);//[22:16] dfi_t_rddate_en=RL-5
#else
    wr(DFITMG0,0x04a3820e);//[28:24] dft_t_ctrl_delay [22:16] dfi_t_rddate_en=RL-5
#endif
    //wr(DFITMG0,0x04a1820e);

    wr(DFITMG1,0x00090303);
    wr(DFILPCFG0,0x0351a001);
    wr(DFIMISC,0x00001015);
#ifdef CONFIG_DDR_DBI_OFF
    //wr(DFITMG2,0x00001f0e);//[14:9] dfi_tphy_rdcslat
    wr(DFITMG2,0x0000230e);
    wr(DBICTL,0x00000001); //dbi-off
#else
    wr(DFITMG2,0x0000230e);//[14:9] dfi_tphy_rdcslat
    wr(DBICTL,0x00000007); //dbi-on
#endif
    wr(DFIPHYMSTR,0x14000001);
    wr(ODTCFG,0x060a0c44);
    //wr(DFIUPD0,0xc0400018);
    wr(DFIUPD0,0x00400018);
    //wr(DFIUPD1,0x00b700c4);
    wr(DFIUPD1,0x00280032);
    //wr(DFIUPD2,0x80000000);
    wr(DFIUPD2,0x00000000);
    //wr(DFIMISC,0x00001011);
    //wr(DFITMG2,0x00001f0e);
    //wr(DBICTL,0x00000001);
    wr(DFIPHYMSTR,0x14000001);
    //wr(ODTCFG,0x060a0c44);
    wr(ODTMAP,0x00000000);
    //wr(SCHED,0x80821f18);
    //wr(SCHED,0x1f829b1c);
    //update by 95P 20240411 [2]page-close enable [14:8] 32'h1b: lpr entry num=62, hpr entry num=4
    wr(SCHED,0x1f82bf18); // hongyi 0718  0x1f82bf1c->0x1f82bf18 for performance testing

    //wr(SCHED1,0x00002000);
    wr(SCHED1,0x4400b00f);
    wr(PERFHPR1,0x0f000001);
    wr(PERFLPR1,0x0f00007f);
    wr(PERFWR1,0x0f00007f);
    //wr(SCHED3,0x04040208);
    wr(SCHED3,0x00000208);
    wr(SCHED4,0x08400810);
    wr(DBG0,0x00000000);
    wr(DBG1,0x00000000);
    wr(DBGCMD,0x00000000);
    wr(SWCTL,0x00000001);
    wr(SWCTLSTATIC,0x00000000);
    wr(POISONCFG,0x00000001);
    wr(PCTRL_0,0x00000001);
    
    while(rd(RFSHCTL3)!=0x00000001);
    
    //update by perf sim
    wr(PCCFG,0x00000010); 
    wr(PCFGR_0,0x0000500f); //CPU read
    wr(PCFGW_0,0x0000500f); //CPU write

    while(rd(PWRCTL)!=0x00000020);
    wr(PWRCTL,0x00000020);

    wr(DBG1,0x00000000);

    while(rd(PWRCTL)!=0x00000020);
    wr(PWRCTL,0x00000020);

    while(rd(PWRCTL)!=0x00000020);
    wr(PWRCTL,0x00000020);

    //while(rd(DFIPHYMSTR)!=0x14000000);
    wr(DFIPHYMSTR,0x14000001);
    wr(SWCTL,0x00000000);
    wr(DFIMISC,0x00001014);
    wr(DFIMISC,0x00001014);
    //wr(SWCTL,0x00000001);
    //while(rd(SWSTAT)!=0x00000001);
    wr(DBG1,0x00000002); 

}//}}}A210:ctrl_init_4266

#ifdef DDR_INIT_DRANK
static void lpddr4_ctrl_init_4266_drank(void)
{
    wr(DBG1,0x00000001);
    wr(PWRCTL,0x00000001);
    while(rd(STAT)!=0x00000000);
    wr(MSTR,0x03080020);//dual rank [25:24] 0011
    wr(MRCTRL0,0x00003030);
    wr(MRCTRL1,0x0002d90f);
    //wr(DERATEEN,0x00001404);
    wr(DERATEEN,0x000014f5);
    //wr(DERATEINT,0x726d4ada);
    wr(DERATEINT,0x40000000);
    wr(DERATECTL,0x00000001);
    //wr(PWRCTL,0x00000000);
    wr(PWRCTL,0x00000020);
    wr(PWRTMG,0x0040ae04);
    wr(HWLPCTL,0x00430002);
    //wr(RFSHCTL0,0x00210000);
    wr(RFSHCTL0,0x00210004);
    wr(RFSHCTL1,0x000f0026);
    wr(RFSHCTL3,0x00000001);
    //wr(RFSHTMG,0x00828195);
    wr(RFSHTMG,0x82000098); 
    wr(RFSHTMG1,0x00610000);
    wr(CRCPARCTL0,0x00000000);
    wr(INIT0,0x00020002);
    wr(INIT1,0x00010002);
    wr(INIT2,0x00002300);
    wr(INIT3,0x0074003f);
#ifdef CONFIG_DDR_DBI_OFF
    wr(INIT4,0x00320000);//INIT:MR3 MR13
#else
    wr(INIT4,0x00f20000);//INIT:MR3 MR13
#endif
    wr(INIT5,0x0005000c);
    wr(INIT6,0x0000004d);
    wr(INIT7,0x0000004d);
    wr(DIMMCTL,0x00000000);
    //wr(RANKCTL,0x0000032f);
    wr(RANKCTL,0x0000ab9f);
    //wr(RANKCTL1,0x00000004);
    wr(RANKCTL1,0x0000001a);
    wr(DRAMTMG0,0x2221482d);
    wr(DRAMTMG1,0x00090941);
#ifdef CONFIG_DDR_DBI_OFF
    wr(DRAMTMG2,0x09121219);//[29:24] -write latency [21:16] read latency [13:8] rd2wr [5:0] wr2rd
    printf("Enter LP4 DBI OFF mode\n");
#else
    //wr(DRAMTMG2,0x09141619);//[29:24] -write latency [21:16] read latency [13:8] rd2wr [5:0] wr2rd
    printf("Enter LP4 DBI ON mode\n");
    wr(DRAMTMG2,0x09141f1a);//[29:24] -write latency [21:16] read latency [13:8] rd2wr [5:0] wr2rd
#endif
    wr(DRAMTMG3,0x00f0f000);
    wr(DRAMTMG4,0x14040914);
    wr(DRAMTMG5,0x02061111);
    wr(DRAMTMG6,0x0101000a);
    wr(DRAMTMG7,0x00000602);
    wr(DRAMTMG8,0x00000101);
    wr(DRAMTMG12,0x00020000);
    wr(DRAMTMG13,0x0e100002);
    wr(DRAMTMG14,0x00000133);
    //wr(ZQCTL0,0xc42d0021);
    wr(ZQCTL0,0xd42d0026);//dual rank: [29] Sets ZQ resistor sharing
    //wr(ZQCTL1,0x0360ccda);
    wr(ZQCTL1,0x03600800);
    wr(ZQCTL2,0x00000000);

    //DBI-off
    //wr(DFITMG0,0x049f820e);//[28:24] dft_t_ctrl_delay [22:16] dfi_t_rddate_en=RL-5

#ifdef CONFIG_DDR_DBI_OFF
    wr(DFITMG0,0x049f820e);//[22:16] dfi_t_rddate_en=RL-5
#else
    wr(DFITMG0,0x04a3820e);//[28:24] dft_t_ctrl_delay [22:16] dfi_t_rddate_en=RL-5
#endif
    //wr(DFITMG0,0x04a1820e);

    wr(DFITMG1,0x00090303);
    wr(DFILPCFG0,0x0351a001);
    wr(DFIMISC,0x00000015);
#ifdef CONFIG_DDR_DBI_OFF
    //wr(DFITMG2,0x00001f0e);//[14:9] dfi_tphy_rdcslat
    wr(DFITMG2,0x0000230e);
    wr(DBICTL,0x00000001); //dbi-off
#else
    wr(DFITMG2,0x0000230e);//[14:9] dfi_tphy_rdcslat
    wr(DBICTL,0x00000007); //dbi-on
#endif
    wr(DFIPHYMSTR,0x14000001);
    wr(ODTCFG,0x060a0c44);
    //wr(DFIUPD0,0xc0400018);
    wr(DFIUPD0,0x00400018);
    //wr(DFIUPD1,0x00b700c4);
    wr(DFIUPD1,0x00280032);
    //wr(DFIUPD2,0x80000000);
    wr(DFIUPD2,0x00000000);
    //wr(DFIMISC,0x00000011);
    //wr(DFITMG2,0x00001f0e);
    //wr(DBICTL,0x00000001);
    wr(DFIPHYMSTR,0x14000001);
    //wr(ODTCFG,0x060a0c44);
    wr(ODTMAP,0x00000000);
    //wr(SCHED,0x80821f18);
    //wr(SCHED,0x1f829b1c);
    wr(SCHED,0x1f82bf1c);//update by 95P 20240411 [2]page-close enable [14:8] 32'h1b: lpr entry num=62, hpr entry num=4
    //wr(SCHED1,0x00002000);
    wr(SCHED1,0x4400b00f);
    wr(PERFHPR1,0x0f000001);
    wr(PERFLPR1,0x0f00007f);
    wr(PERFWR1,0x0f00007f);
    //wr(SCHED3,0x04040208);
    wr(SCHED3,0x00000208);
    wr(SCHED4,0x08400810);
    wr(DBG0,0x00000000);
    wr(DBG1,0x00000000);
    wr(DBGCMD,0x00000000);
    wr(SWCTL,0x00000001);
    wr(SWCTLSTATIC,0x00000000);
    wr(POISONCFG,0x00000001);
    wr(PCTRL_0,0x00000001);
    
    while(rd(RFSHCTL3)!=0x00000001);
    
    //update by perf sim
    wr(PCCFG,0x00000010); 
    wr(PCFGR_0,0x0000500f); //CPU read
    wr(PCFGW_0,0x0000500f); //CPU write

    while(rd(PWRCTL)!=0x00000020);
    wr(PWRCTL,0x00000020);

    wr(DBG1,0x00000000);

    while(rd(PWRCTL)!=0x00000020);
    wr(PWRCTL,0x00000020);

    while(rd(PWRCTL)!=0x00000020);
    wr(PWRCTL,0x00000020);

    //while(rd(DFIPHYMSTR)!=0x14000000);
    wr(DFIPHYMSTR,0x14000001);
    wr(SWCTL,0x00000000);
    wr(DFIMISC,0x00000014);
    wr(DFIMISC,0x00000014);
    //wr(SWCTL,0x00000001);
    //while(rd(SWSTAT)!=0x00000001);
    wr(DBG1,0x00000002); 

}
#endif

static void ddr_apb_broadcast_en(void)
{
    wr(SLC_DUAL_SC+0x50,0x1);
}

static void pll_config (int speed) {
    int rdata;
    if(speed==4266) {
    //4266
    //wr(SLC_DUAL_SC+0xc,0x4b000000);
    rdata = rd(SLC_DUAL_SC+0xc);
    rdata &= 0xff000000;
    wr(SLC_DUAL_SC+0xc,rdata | 0x40400000);
    wr(SLC_DUAL_SC+0x8,0x1310a02);
    udelay(2);
    //wr(SLC_DUAL_SC+0xc,0x0b000000);
    rdata = rd(SLC_DUAL_SC+0xc);
    wr(SLC_DUAL_SC+0xc,rdata & 0xbfffffff);
    } else if(speed==3733) {
    //3733
    //wr(SLC_DUAL_SC+0xc,0x4b000000);
    rdata = rd(SLC_DUAL_SC+0xc);
    rdata &= 0xff000000;
    wr(SLC_DUAL_SC+0xc,rdata | 0x40600000);
    wr(SLC_DUAL_SC+0x8,0x01204d01);
    udelay(2);
    //wr(SLC_DUAL_SC+0xc,0x0b000000);
    rdata = rd(SLC_DUAL_SC+0xc);
    wr(SLC_DUAL_SC+0xc,rdata & 0xbfffffff);
    } else if(speed==3200) {
    //wr(SLC_DUAL_SC+0xc,0x4b000000);
    rdata = rd(SLC_DUAL_SC+0xc);
    rdata &= 0xff000000;
    wr(SLC_DUAL_SC+0xc,rdata | 0x40155555);
    wr(SLC_DUAL_SC+0x8,0x01408501);
    udelay(2);
    //wr(SLC_DUAL_SC+0xc,0x0b000000);
    rdata = rd(SLC_DUAL_SC+0xc);
    wr(SLC_DUAL_SC+0xc,rdata & 0xbfffffff);
    //3200
    } else if(speed==2133) {
    //2133
    //wr(SLC_DUAL_SC+0xc,0x4b000000);
    rdata = rd(SLC_DUAL_SC+0xc);
    rdata &= 0xff000000;
    wr(SLC_DUAL_SC+0xc,rdata | 0x40000000);
    wr(SLC_DUAL_SC+0x8,0x01608501);
    udelay(2);
    //wr(SLC_DUAL_SC+0xc,0x0b000000);
    rdata = rd(SLC_DUAL_SC+0xc);
    wr(SLC_DUAL_SC+0xc,rdata & 0xbfffffff);
    } else if(speed==1066) {
    //2133
    //wr(SLC_DUAL_SC+0xc,0x4b000000);
    rdata = rd(SLC_DUAL_SC+0xc);
    rdata &= 0xff000000;
    wr(SLC_DUAL_SC+0xc,rdata | 0x40aaaaab);
    wr(SLC_DUAL_SC+0x8,0x002608501);
    udelay(2);
    //wr(SLC_DUAL_SC+0xc,0x0b000000);
    rdata = rd(SLC_DUAL_SC+0xc);
    wr(SLC_DUAL_SC+0xc,rdata & 0xbfffffff);
    } else {
#ifdef CONFIG_DDR_MSG
    printf("Reserved Pll setting\n");
#endif
    //wr(SLC_DUAL_SC+0xc,0x4b000000);
    rdata = rd(SLC_DUAL_SC+0xc);
    wr(SLC_DUAL_SC+0xc,rdata | 0x40000000);
    wr(SLC_DUAL_SC+0x8,0x01606601);
    //wr(SLC_DUAL_SC+0xc,0x0b000000);
    rdata = rd(SLC_DUAL_SC+0xc);
    wr(SLC_DUAL_SC+0xc,rdata & 0xbfffffff);
    }
#ifdef CONFIG_DDR_MSG
    printf("Freq    is %0x \n",rd(SLC_DUAL_SC+0x8));
#endif
    while((rd(SLC_DUAL_SC+0x18)&1)!=0x1); //pll lock
    wr(SLC_DUAL_SC+0x18,0x10000);// 
 }

static void ddr_slc_top_crg_release(void)
{
    int rdata;
    rdata = rd(SLC_DUAL_SC+0x40);
    wr(SLC_DUAL_SC+0x40,rdata | 0x300);
    rdata = rd(SLC_DUAL_SC+0x44);
    wr(SLC_DUAL_SC+0x44,rdata | 0x300);

    rdata =  rd(SLC_DUAL_SC+0x48);
    wr(SLC_DUAL_SC+0x48,rdata | 0x80);
    rdata =  rd(SLC_DUAL_SC+0x4c);
    wr(SLC_DUAL_SC+0x4c,rdata | 0x80);
}

static void slc_crg_release(void)
{
    wr(SYSREG_SLC_CH0+0x0,0x3);
}

//A210:addr map{{{
static void addr_map(void) {
    wr(ADDRMAP0,0x001f001f); 
    wr(ADDRMAP1,0x00080808); 
    wr(ADDRMAP2,0x00000000); 
    wr(ADDRMAP3,0x00000000); 
    wr(ADDRMAP4,0x00001f1f); 
    wr(ADDRMAP5,0x070f0707); 
    wr(ADDRMAP6,0x07070707); 
    wr(ADDRMAP7,0x00000f0f);
    wr(ADDRMAP9,0x07070707);
    wr(ADDRMAP10,0x07070707);
    wr(ADDRMAP11,0x00000007);
}//}}}A210:addr map

//A210 update 20240311{{{
static void de_assert_other_reset_ddr(void)
{
    wr(SYSREG_DDR_CH0,0x7); //PHY Reset
    wr(SYSREG_DDR_CH0,0x7); //PHY Reset
    wr(SYSREG_DDR_CH0,0xf); //ddrc rstn
    wr(SYSREG_DDR_CH0,0xf); //ddrc rstn
    wr(SYSREG_DDR_CH0,0x1f); //arsetn
    wr(SYSREG_DDR_CH0,0x1f); //arsetn
    wr(SYSREG_DDR_CH0,0x1f); //arsetn
    wr(SYSREG_DDR_CH0,0x1f); //arsetn
    wr(SYSREG_DDR_CH0,0x1f); //arsetn
}
//}}}
static void ddr_deassert_pwrok_apb(void)
{
    wr(SYSREG_DDR_CH0,0x1); //PwrOK
    wr(SYSREG_DDR_CH0,0x3); //Presetn
}

#ifdef lpddr4_skiptrain_4266
static void dwc_ddrphy_phyinit_out_lpddr4_skiptrain_4266(void){

    printf("dwc_ddrphy_phyinit_userCustom_overrideUserInput()\n");
    dwc_ddrphy_phyinit_userCustom_overrideUserInput();

    printf("dwc_ddrphy_phyinit_userCustom_A_bringupPower()\n");
    dwc_ddrphy_phyinit_userCustom_A_bringupPower();

    printf("dwc_ddrphy_phyinit_userCustom_B_startClockResetPhy()\n");
    dwc_ddrphy_phyinit_userCustom_B_startClockResetPhy();
    
    ddr_phy_reg_wr(0x1005f,0x1ff);
    ddr_phy_reg_wr(0x1015f,0x1ff);
    ddr_phy_reg_wr(0x1105f,0x1ff);
    ddr_phy_reg_wr(0x1115f,0x1ff);
    ddr_phy_reg_wr(0x1205f,0x1ff);
    ddr_phy_reg_wr(0x1215f,0x1ff);
    ddr_phy_reg_wr(0x1305f,0x1ff);
    ddr_phy_reg_wr(0x1315f,0x1ff);
    
    ddr_phy_reg_wr(0x55,0x1ff);
    
    ddr_phy_reg_wr(0x1055,0x1ff);
    
    ddr_phy_reg_wr(0x2055,0x1ff);
    
    ddr_phy_reg_wr(0x3055,0x1ff);
    
    ddr_phy_reg_wr(0x4055,0x1ff);
    
    ddr_phy_reg_wr(0x5055,0x1ff);
    
    ddr_phy_reg_wr(0x6055,0x1ff);
    
    ddr_phy_reg_wr(0x7055,0x1ff);
    
    ddr_phy_reg_wr(0x8055,0x1ff);
    
    ddr_phy_reg_wr(0x9055,0x1ff);
    ddr_phy_reg_wr(0x200c5,0x18);
    ddr_phy_reg_wr(0x2002e,0x2);
    ddr_phy_reg_wr(0x90204,0x0);
    ddr_phy_reg_wr(0x20024,0xe3);
    ddr_phy_reg_wr(0x2003a,0x2);
    ddr_phy_reg_wr(0x2007d,0x212);
    ddr_phy_reg_wr(0x2007c,0x61);
    ddr_phy_reg_wr(0x20056,0x3);
    ddr_phy_reg_wr(0x1004d,0x600);
    ddr_phy_reg_wr(0x1014d,0x600);
    ddr_phy_reg_wr(0x1104d,0x600);
    ddr_phy_reg_wr(0x1114d,0x600);
    ddr_phy_reg_wr(0x1204d,0x600);
    ddr_phy_reg_wr(0x1214d,0x600);
    ddr_phy_reg_wr(0x1304d,0x600);
    ddr_phy_reg_wr(0x1314d,0x600);
    ddr_phy_reg_wr(0x10049,0x61f);
    ddr_phy_reg_wr(0x10149,0x61f);
    ddr_phy_reg_wr(0x11049,0x61f);
    ddr_phy_reg_wr(0x11149,0x61f);
    ddr_phy_reg_wr(0x12049,0x61f);
    ddr_phy_reg_wr(0x12149,0x61f);
    ddr_phy_reg_wr(0x13049,0x61f);
    ddr_phy_reg_wr(0x13149,0x61f);
    ddr_phy_reg_wr(0x43,0x7f);
    ddr_phy_reg_wr(0x1043,0x7f);
    ddr_phy_reg_wr(0x2043,0x7f);
    ddr_phy_reg_wr(0x3043,0x7f);
    ddr_phy_reg_wr(0x4043,0x7f);
    ddr_phy_reg_wr(0x5043,0x7f);
    ddr_phy_reg_wr(0x6043,0x7f);
    ddr_phy_reg_wr(0x7043,0x7f);
    ddr_phy_reg_wr(0x8043,0x7f);
    ddr_phy_reg_wr(0x9043,0x7f);
    ddr_phy_reg_wr(0x20018,0x3);
    ddr_phy_reg_wr(0x20075,0x4);
    ddr_phy_reg_wr(0x20050,0x0);
    ddr_phy_reg_wr(0x2009b,0x2);
    ddr_phy_reg_wr(0x20008,0x42b);
    ddr_phy_reg_wr(0x20088,0x9);
    ddr_phy_reg_wr(0x200b2,0x104);
    ddr_phy_reg_wr(0x10043,0x5a1);
    ddr_phy_reg_wr(0x10143,0x5a1);
    ddr_phy_reg_wr(0x11043,0x5a1);
    ddr_phy_reg_wr(0x11143,0x5a1);
    ddr_phy_reg_wr(0x12043,0x5a1);
    ddr_phy_reg_wr(0x12143,0x5a1);
    ddr_phy_reg_wr(0x13043,0x5a1);
    ddr_phy_reg_wr(0x13143,0x5a1);
    ddr_phy_reg_wr(0x200fa,0x1);
    ddr_phy_reg_wr(0x20019,0x1);
    ddr_phy_reg_wr(0x200f0,0x0);
    ddr_phy_reg_wr(0x200f1,0x0);
    ddr_phy_reg_wr(0x200f2,0x4444);
    ddr_phy_reg_wr(0x200f3,0x8888);
    ddr_phy_reg_wr(0x200f4,0x5555);
    ddr_phy_reg_wr(0x200f5,0x0);
    ddr_phy_reg_wr(0x200f6,0x0);
    ddr_phy_reg_wr(0x200f7,0xf000);
    ddr_phy_reg_wr(0x1004a,0x500);
    ddr_phy_reg_wr(0x1104a,0x500);
    ddr_phy_reg_wr(0x1204a,0x500);
    ddr_phy_reg_wr(0x1304a,0x500);
    ddr_phy_reg_wr(0x20025,0x0);
    ddr_phy_reg_wr(0x2002d,0x0);
    ddr_phy_reg_wr(0x2002c,0x0);
    
    //-tt- fix WDQS_on/off violation, porting from soc/a210
    //ddr_phy_reg_wr(0x20024, 0x1a3);
    ddr_phy_reg_wr(0x20024, 0x1e3);//update by 95P 20240416

    
    ddr_phy_reg_wr(0x10020,0x7);
    ddr_phy_reg_wr(0x11020,0x7);
    ddr_phy_reg_wr(0x12020,0x7);
    ddr_phy_reg_wr(0x13020,0x7);
    ddr_phy_reg_wr(0x20020,0x7);
    ddr_phy_reg_wr(0x100d0,0x100);
    ddr_phy_reg_wr(0x101d0,0x100);
    ddr_phy_reg_wr(0x110d0,0x100);
    ddr_phy_reg_wr(0x111d0,0x100);
    ddr_phy_reg_wr(0x120d0,0x100);
    ddr_phy_reg_wr(0x121d0,0x100);
    ddr_phy_reg_wr(0x130d0,0x100);
    ddr_phy_reg_wr(0x131d0,0x100);
    ddr_phy_reg_wr(0x100c0,0x4c);
    ddr_phy_reg_wr(0x101c0,0x4c);
    ddr_phy_reg_wr(0x102c0,0x4c);
    ddr_phy_reg_wr(0x103c0,0x4c);
    ddr_phy_reg_wr(0x104c0,0x4c);
    ddr_phy_reg_wr(0x105c0,0x4c);
    ddr_phy_reg_wr(0x106c0,0x4c);
    ddr_phy_reg_wr(0x107c0,0x4c);
    ddr_phy_reg_wr(0x108c0,0x4c);
    ddr_phy_reg_wr(0x110c0,0x4c);
    ddr_phy_reg_wr(0x111c0,0x4c);
    ddr_phy_reg_wr(0x112c0,0x4c);
    ddr_phy_reg_wr(0x113c0,0x4c);
    ddr_phy_reg_wr(0x114c0,0x4c);
    ddr_phy_reg_wr(0x115c0,0x4c);
    ddr_phy_reg_wr(0x116c0,0x4c);
    ddr_phy_reg_wr(0x117c0,0x4c);
    ddr_phy_reg_wr(0x118c0,0x4c);
    ddr_phy_reg_wr(0x120c0,0x4c);
    ddr_phy_reg_wr(0x121c0,0x4c);
    ddr_phy_reg_wr(0x122c0,0x4c);
    ddr_phy_reg_wr(0x123c0,0x4c);
    ddr_phy_reg_wr(0x124c0,0x4c);
    ddr_phy_reg_wr(0x125c0,0x4c);
    ddr_phy_reg_wr(0x126c0,0x4c);
    ddr_phy_reg_wr(0x127c0,0x4c);
    ddr_phy_reg_wr(0x128c0,0x4c);
    ddr_phy_reg_wr(0x130c0,0x4c);
    ddr_phy_reg_wr(0x131c0,0x4c);
    ddr_phy_reg_wr(0x132c0,0x4c);
    ddr_phy_reg_wr(0x133c0,0x4c);
    ddr_phy_reg_wr(0x134c0,0x4c);
    ddr_phy_reg_wr(0x135c0,0x4c);
    ddr_phy_reg_wr(0x136c0,0x4c);
    ddr_phy_reg_wr(0x137c0,0x4c);
    ddr_phy_reg_wr(0x138c0,0x4c);
    ddr_phy_reg_wr(0x10080,0x349);
    ddr_phy_reg_wr(0x10180,0x349);
    ddr_phy_reg_wr(0x11080,0x349);
    ddr_phy_reg_wr(0x11180,0x349);
    ddr_phy_reg_wr(0x12080,0x349);
    ddr_phy_reg_wr(0x12180,0x349);
    ddr_phy_reg_wr(0x13080,0x349);
    ddr_phy_reg_wr(0x13180,0x349);
    ddr_phy_reg_wr(0x90201,0x2200);
    ddr_phy_reg_wr(0x90202,0x10);
    ddr_phy_reg_wr(0x90203,0x2e00);
    ddr_phy_reg_wr(0x20072,0x1);
    ddr_phy_reg_wr(0x20073,0x1);
    ddr_phy_reg_wr(0x100ae,0x1c);
    ddr_phy_reg_wr(0x110ae,0x1c);
    ddr_phy_reg_wr(0x120ae,0x1c);
    ddr_phy_reg_wr(0x130ae,0x1c);
    ddr_phy_reg_wr(0x100af,0x1c);
    ddr_phy_reg_wr(0x110af,0x1c);
    ddr_phy_reg_wr(0x120af,0x1c);
    ddr_phy_reg_wr(0x130af,0x1c);
    ddr_phy_reg_wr(0x100aa,0x501);
    ddr_phy_reg_wr(0x110aa,0x50d);
    ddr_phy_reg_wr(0x120aa,0x501);
    ddr_phy_reg_wr(0x130aa,0x50d);
    ddr_phy_reg_wr(0x20077,0x34);
    ddr_phy_reg_wr(0x2007c,0x54);
    ddr_phy_reg_wr(0x2007d,0x2f2);
    ddr_phy_reg_wr(0x400c0,0x10f);
    ddr_phy_reg_wr(0x200cb,0x61f0);
    ddr_phy_reg_wr(0x90028,0x0);
    ddr_phy_reg_wr(0xd0000,0x0);
    ddr_phy_reg_wr(0x90000,0x10);
    ddr_phy_reg_wr(0x90001,0x400);
    ddr_phy_reg_wr(0x90002,0x10e);
    ddr_phy_reg_wr(0x90003,0x0);
    ddr_phy_reg_wr(0x90004,0x0);
    ddr_phy_reg_wr(0x90005,0x8);
    ddr_phy_reg_wr(0x90029,0xb);
    ddr_phy_reg_wr(0x9002a,0x480);
    ddr_phy_reg_wr(0x9002b,0x109);
    ddr_phy_reg_wr(0x9002c,0x8);
    ddr_phy_reg_wr(0x9002d,0x448);
    ddr_phy_reg_wr(0x9002e,0x139);
    ddr_phy_reg_wr(0x9002f,0x8);
    ddr_phy_reg_wr(0x90030,0x478);
    ddr_phy_reg_wr(0x90031,0x109);
    ddr_phy_reg_wr(0x90032,0x0);
    ddr_phy_reg_wr(0x90033,0xe8);
    ddr_phy_reg_wr(0x90034,0x109);
    ddr_phy_reg_wr(0x90035,0x2);
    ddr_phy_reg_wr(0x90036,0x10);
    ddr_phy_reg_wr(0x90037,0x139);
    ddr_phy_reg_wr(0x90038,0xb);
    ddr_phy_reg_wr(0x90039,0x7c0);
    ddr_phy_reg_wr(0x9003a,0x139);
    ddr_phy_reg_wr(0x9003b,0x44);
    ddr_phy_reg_wr(0x9003c,0x633);
    ddr_phy_reg_wr(0x9003d,0x159);
    ddr_phy_reg_wr(0x9003e,0x14f);
    ddr_phy_reg_wr(0x9003f,0x630);
    ddr_phy_reg_wr(0x90040,0x159);
    ddr_phy_reg_wr(0x90041,0x47);
    ddr_phy_reg_wr(0x90042,0x633);
    ddr_phy_reg_wr(0x90043,0x149);
    ddr_phy_reg_wr(0x90044,0x4f);
    ddr_phy_reg_wr(0x90045,0x633);
    ddr_phy_reg_wr(0x90046,0x179);
    ddr_phy_reg_wr(0x90047,0x8);
    ddr_phy_reg_wr(0x90048,0xe0);
    ddr_phy_reg_wr(0x90049,0x109);
    ddr_phy_reg_wr(0x9004a,0x0);
    ddr_phy_reg_wr(0x9004b,0x7c8);
    ddr_phy_reg_wr(0x9004c,0x109);
    ddr_phy_reg_wr(0x9004d,0x0);
    ddr_phy_reg_wr(0x9004e,0x1);
    ddr_phy_reg_wr(0x9004f,0x8);
    ddr_phy_reg_wr(0x90050,0x0);
    ddr_phy_reg_wr(0x90051,0x45a);
    ddr_phy_reg_wr(0x90052,0x9);
    ddr_phy_reg_wr(0x90053,0x0);
    ddr_phy_reg_wr(0x90054,0x448);
    ddr_phy_reg_wr(0x90055,0x109);
    ddr_phy_reg_wr(0x90056,0x40);
    ddr_phy_reg_wr(0x90057,0x633);
    ddr_phy_reg_wr(0x90058,0x179);
    ddr_phy_reg_wr(0x90059,0x1);
    ddr_phy_reg_wr(0x9005a,0x618);
    ddr_phy_reg_wr(0x9005b,0x109);
    ddr_phy_reg_wr(0x9005c,0x40c0);
    ddr_phy_reg_wr(0x9005d,0x633);
    ddr_phy_reg_wr(0x9005e,0x149);
    ddr_phy_reg_wr(0x9005f,0x8);
    ddr_phy_reg_wr(0x90060,0x4);
    ddr_phy_reg_wr(0x90061,0x48);
    ddr_phy_reg_wr(0x90062,0x4040);
    ddr_phy_reg_wr(0x90063,0x633);
    ddr_phy_reg_wr(0x90064,0x149);
    ddr_phy_reg_wr(0x90065,0x0);
    ddr_phy_reg_wr(0x90066,0x4);
    ddr_phy_reg_wr(0x90067,0x48);
    ddr_phy_reg_wr(0x90068,0x40);
    ddr_phy_reg_wr(0x90069,0x633);
    ddr_phy_reg_wr(0x9006a,0x149);
    ddr_phy_reg_wr(0x9006b,0x10);
    ddr_phy_reg_wr(0x9006c,0x4);
    ddr_phy_reg_wr(0x9006d,0x18);
    ddr_phy_reg_wr(0x9006e,0x0);
    ddr_phy_reg_wr(0x9006f,0x4);
    ddr_phy_reg_wr(0x90070,0x78);
    ddr_phy_reg_wr(0x90071,0x549);
    ddr_phy_reg_wr(0x90072,0x633);
    ddr_phy_reg_wr(0x90073,0x159);
    ddr_phy_reg_wr(0x90074,0xd49);
    ddr_phy_reg_wr(0x90075,0x633);
    ddr_phy_reg_wr(0x90076,0x159);
    ddr_phy_reg_wr(0x90077,0x94a);
    ddr_phy_reg_wr(0x90078,0x633);
    ddr_phy_reg_wr(0x90079,0x159);
    ddr_phy_reg_wr(0x9007a,0x441);
    ddr_phy_reg_wr(0x9007b,0x633);
    ddr_phy_reg_wr(0x9007c,0x149);
    ddr_phy_reg_wr(0x9007d,0x42);
    ddr_phy_reg_wr(0x9007e,0x633);
    ddr_phy_reg_wr(0x9007f,0x149);
    ddr_phy_reg_wr(0x90080,0x1);
    ddr_phy_reg_wr(0x90081,0x633);
    ddr_phy_reg_wr(0x90082,0x149);
    ddr_phy_reg_wr(0x90083,0x0);
    ddr_phy_reg_wr(0x90084,0xe0);
    ddr_phy_reg_wr(0x90085,0x109);
    ddr_phy_reg_wr(0x90086,0xa);
    ddr_phy_reg_wr(0x90087,0x10);
    ddr_phy_reg_wr(0x90088,0x109);
    ddr_phy_reg_wr(0x90089,0x9);
    ddr_phy_reg_wr(0x9008a,0x3c0);
    ddr_phy_reg_wr(0x9008b,0x149);
    ddr_phy_reg_wr(0x9008c,0x9);
    ddr_phy_reg_wr(0x9008d,0x3c0);
    ddr_phy_reg_wr(0x9008e,0x159);
    ddr_phy_reg_wr(0x9008f,0x18);
    ddr_phy_reg_wr(0x90090,0x10);
    ddr_phy_reg_wr(0x90091,0x109);
    ddr_phy_reg_wr(0x90092,0x0);
    ddr_phy_reg_wr(0x90093,0x3c0);
    ddr_phy_reg_wr(0x90094,0x109);
    ddr_phy_reg_wr(0x90095,0x18);
    ddr_phy_reg_wr(0x90096,0x4);
    ddr_phy_reg_wr(0x90097,0x48);
    ddr_phy_reg_wr(0x90098,0x18);
    ddr_phy_reg_wr(0x90099,0x4);
    ddr_phy_reg_wr(0x9009a,0x58);
    ddr_phy_reg_wr(0x9009b,0xb);
    ddr_phy_reg_wr(0x9009c,0x10);
    ddr_phy_reg_wr(0x9009d,0x109);
    ddr_phy_reg_wr(0x9009e,0x1);
    ddr_phy_reg_wr(0x9009f,0x10);
    ddr_phy_reg_wr(0x900a0,0x109);
    ddr_phy_reg_wr(0x900a1,0x5);
    ddr_phy_reg_wr(0x900a2,0x7c0);
    ddr_phy_reg_wr(0x900a3,0x109);
    ddr_phy_reg_wr(0x40000,0x811);
    ddr_phy_reg_wr(0x40020,0x880);
    ddr_phy_reg_wr(0x40040,0x0);
    ddr_phy_reg_wr(0x40060,0x0);
    ddr_phy_reg_wr(0x40001,0x4008);
    ddr_phy_reg_wr(0x40021,0x83);
    ddr_phy_reg_wr(0x40041,0x4f);
    ddr_phy_reg_wr(0x40061,0x0);
    ddr_phy_reg_wr(0x40002,0x4040);
    ddr_phy_reg_wr(0x40022,0x83);
    ddr_phy_reg_wr(0x40042,0x51);
    ddr_phy_reg_wr(0x40062,0x0);
    ddr_phy_reg_wr(0x40003,0x811);
    ddr_phy_reg_wr(0x40023,0x880);
    ddr_phy_reg_wr(0x40043,0x0);
    ddr_phy_reg_wr(0x40063,0x0);
    ddr_phy_reg_wr(0x40004,0x720);
    ddr_phy_reg_wr(0x40024,0xf);
    ddr_phy_reg_wr(0x40044,0x1740);
    ddr_phy_reg_wr(0x40064,0x0);
    ddr_phy_reg_wr(0x40005,0x16);
    ddr_phy_reg_wr(0x40025,0x83);
    ddr_phy_reg_wr(0x40045,0x4b);
    ddr_phy_reg_wr(0x40065,0x0);
    ddr_phy_reg_wr(0x40006,0x716);
    ddr_phy_reg_wr(0x40026,0xf);
    ddr_phy_reg_wr(0x40046,0x2001);
    ddr_phy_reg_wr(0x40066,0x0);
    ddr_phy_reg_wr(0x40007,0x716);
    ddr_phy_reg_wr(0x40027,0xf);
    ddr_phy_reg_wr(0x40047,0x2800);
    ddr_phy_reg_wr(0x40067,0x0);
    ddr_phy_reg_wr(0x40008,0x716);
    ddr_phy_reg_wr(0x40028,0xf);
    ddr_phy_reg_wr(0x40048,0xf00);
    ddr_phy_reg_wr(0x40068,0x0);
    ddr_phy_reg_wr(0x40009,0x720);
    ddr_phy_reg_wr(0x40029,0xf);
    ddr_phy_reg_wr(0x40049,0x1400);
    ddr_phy_reg_wr(0x40069,0x0);
    ddr_phy_reg_wr(0x4000a,0xe08);
    ddr_phy_reg_wr(0x4002a,0xc15);
    ddr_phy_reg_wr(0x4004a,0x0);
    ddr_phy_reg_wr(0x4006a,0x0);
    ddr_phy_reg_wr(0x4000b,0x625);
    ddr_phy_reg_wr(0x4002b,0x15);
    ddr_phy_reg_wr(0x4004b,0x0);
    ddr_phy_reg_wr(0x4006b,0x0);
    ddr_phy_reg_wr(0x4000c,0x4028);
    ddr_phy_reg_wr(0x4002c,0x80);
    ddr_phy_reg_wr(0x4004c,0x0);
    ddr_phy_reg_wr(0x4006c,0x0);
    ddr_phy_reg_wr(0x4000d,0xe08);
    ddr_phy_reg_wr(0x4002d,0xc1a);
    ddr_phy_reg_wr(0x4004d,0x0);
    ddr_phy_reg_wr(0x4006d,0x0);
    ddr_phy_reg_wr(0x4000e,0x625);
    ddr_phy_reg_wr(0x4002e,0x1a);
    ddr_phy_reg_wr(0x4004e,0x0);
    ddr_phy_reg_wr(0x4006e,0x0);
    ddr_phy_reg_wr(0x4000f,0x4040);
    ddr_phy_reg_wr(0x4002f,0x80);
    ddr_phy_reg_wr(0x4004f,0x0);
    ddr_phy_reg_wr(0x4006f,0x0);
    ddr_phy_reg_wr(0x40010,0x2604);
    ddr_phy_reg_wr(0x40030,0x15);
    ddr_phy_reg_wr(0x40050,0x0);
    ddr_phy_reg_wr(0x40070,0x0);
    ddr_phy_reg_wr(0x40011,0x708);
    ddr_phy_reg_wr(0x40031,0x5);
    ddr_phy_reg_wr(0x40051,0x0);
    ddr_phy_reg_wr(0x40071,0x2002);
    ddr_phy_reg_wr(0x40012,0x8);
    ddr_phy_reg_wr(0x40032,0x80);
    ddr_phy_reg_wr(0x40052,0x0);
    ddr_phy_reg_wr(0x40072,0x0);
    ddr_phy_reg_wr(0x40013,0x2604);
    ddr_phy_reg_wr(0x40033,0x1a);
    ddr_phy_reg_wr(0x40053,0x0);
    ddr_phy_reg_wr(0x40073,0x0);
    ddr_phy_reg_wr(0x40014,0x708);
    ddr_phy_reg_wr(0x40034,0xa);
    ddr_phy_reg_wr(0x40054,0x0);
    ddr_phy_reg_wr(0x40074,0x2002);
    ddr_phy_reg_wr(0x40015,0x4040);
    ddr_phy_reg_wr(0x40035,0x80);
    ddr_phy_reg_wr(0x40055,0x0);
    ddr_phy_reg_wr(0x40075,0x0);
    ddr_phy_reg_wr(0x40016,0x60a);
    ddr_phy_reg_wr(0x40036,0x15);
    ddr_phy_reg_wr(0x40056,0x1200);
    ddr_phy_reg_wr(0x40076,0x0);
    ddr_phy_reg_wr(0x40017,0x61a);
    ddr_phy_reg_wr(0x40037,0x15);
    ddr_phy_reg_wr(0x40057,0x1300);
    ddr_phy_reg_wr(0x40077,0x0);
    ddr_phy_reg_wr(0x40018,0x60a);
    ddr_phy_reg_wr(0x40038,0x1a);
    ddr_phy_reg_wr(0x40058,0x1200);
    ddr_phy_reg_wr(0x40078,0x0);
    ddr_phy_reg_wr(0x40019,0x642);
    ddr_phy_reg_wr(0x40039,0x1a);
    ddr_phy_reg_wr(0x40059,0x1300);
    ddr_phy_reg_wr(0x40079,0x0);
    ddr_phy_reg_wr(0x4001a,0x4808);
    ddr_phy_reg_wr(0x4003a,0x880);
    ddr_phy_reg_wr(0x4005a,0x0);
    ddr_phy_reg_wr(0x4007a,0x0);
    ddr_phy_reg_wr(0x900a4,0x0);
    ddr_phy_reg_wr(0x900a5,0x790);
    ddr_phy_reg_wr(0x900a6,0x11a);
    ddr_phy_reg_wr(0x900a7,0x8);
    ddr_phy_reg_wr(0x900a8,0x7aa);
    ddr_phy_reg_wr(0x900a9,0x2a);
    ddr_phy_reg_wr(0x900aa,0x10);
    ddr_phy_reg_wr(0x900ab,0x7b2);
    ddr_phy_reg_wr(0x900ac,0x2a);
    ddr_phy_reg_wr(0x900ad,0x0);
    ddr_phy_reg_wr(0x900ae,0x7c8);
    ddr_phy_reg_wr(0x900af,0x109);
    ddr_phy_reg_wr(0x900b0,0x10);
    ddr_phy_reg_wr(0x900b1,0x10);
    ddr_phy_reg_wr(0x900b2,0x109);
    ddr_phy_reg_wr(0x900b3,0x10);
    ddr_phy_reg_wr(0x900b4,0x2a8);
    ddr_phy_reg_wr(0x900b5,0x129);
    ddr_phy_reg_wr(0x900b6,0x8);
    ddr_phy_reg_wr(0x900b7,0x370);
    ddr_phy_reg_wr(0x900b8,0x129);
    ddr_phy_reg_wr(0x900b9,0xa);
    ddr_phy_reg_wr(0x900ba,0x3c8);
    ddr_phy_reg_wr(0x900bb,0x1a9);
    ddr_phy_reg_wr(0x900bc,0xc);
    ddr_phy_reg_wr(0x900bd,0x408);
    ddr_phy_reg_wr(0x900be,0x199);
    ddr_phy_reg_wr(0x900bf,0x14);
    ddr_phy_reg_wr(0x900c0,0x790);
    ddr_phy_reg_wr(0x900c1,0x11a);
    ddr_phy_reg_wr(0x900c2,0x8);
    ddr_phy_reg_wr(0x900c3,0x4);
    ddr_phy_reg_wr(0x900c4,0x18);
    ddr_phy_reg_wr(0x900c5,0xe);
    ddr_phy_reg_wr(0x900c6,0x408);
    ddr_phy_reg_wr(0x900c7,0x199);
    ddr_phy_reg_wr(0x900c8,0x8);
    ddr_phy_reg_wr(0x900c9,0x8568);
    ddr_phy_reg_wr(0x900ca,0x108);
    ddr_phy_reg_wr(0x900cb,0x18);
    ddr_phy_reg_wr(0x900cc,0x790);
    ddr_phy_reg_wr(0x900cd,0x16a);
    ddr_phy_reg_wr(0x900ce,0x8);
    ddr_phy_reg_wr(0x900cf,0x1d8);
    ddr_phy_reg_wr(0x900d0,0x169);
    ddr_phy_reg_wr(0x900d1,0x10);
    ddr_phy_reg_wr(0x900d2,0x8558);
    ddr_phy_reg_wr(0x900d3,0x168);
    ddr_phy_reg_wr(0x900d4,0x70);
    ddr_phy_reg_wr(0x900d5,0x788);
    ddr_phy_reg_wr(0x900d6,0x16a);
    ddr_phy_reg_wr(0x900d7,0x1ff8);
    ddr_phy_reg_wr(0x900d8,0x85a8);
    ddr_phy_reg_wr(0x900d9,0x1e8);
    ddr_phy_reg_wr(0x900da,0x50);
    ddr_phy_reg_wr(0x900db,0x798);
    ddr_phy_reg_wr(0x900dc,0x16a);
    ddr_phy_reg_wr(0x900dd,0x60);
    ddr_phy_reg_wr(0x900de,0x7a0);
    ddr_phy_reg_wr(0x900df,0x16a);
    ddr_phy_reg_wr(0x900e0,0x8);
    ddr_phy_reg_wr(0x900e1,0x8310);
    ddr_phy_reg_wr(0x900e2,0x168);
    ddr_phy_reg_wr(0x900e3,0x8);
    ddr_phy_reg_wr(0x900e4,0xa310);
    ddr_phy_reg_wr(0x900e5,0x168);
    ddr_phy_reg_wr(0x900e6,0xa);
    ddr_phy_reg_wr(0x900e7,0x408);
    ddr_phy_reg_wr(0x900e8,0x169);
    ddr_phy_reg_wr(0x900e9,0x6e);
    ddr_phy_reg_wr(0x900ea,0x0);
    ddr_phy_reg_wr(0x900eb,0x68);
    ddr_phy_reg_wr(0x900ec,0x0);
    ddr_phy_reg_wr(0x900ed,0x408);
    ddr_phy_reg_wr(0x900ee,0x169);
    ddr_phy_reg_wr(0x900ef,0x0);
    ddr_phy_reg_wr(0x900f0,0x8310);
    ddr_phy_reg_wr(0x900f1,0x168);
    ddr_phy_reg_wr(0x900f2,0x0);
    ddr_phy_reg_wr(0x900f3,0xa310);
    ddr_phy_reg_wr(0x900f4,0x168);
    ddr_phy_reg_wr(0x900f5,0x1ff8);
    ddr_phy_reg_wr(0x900f6,0x85a8);
    ddr_phy_reg_wr(0x900f7,0x1e8);
    ddr_phy_reg_wr(0x900f8,0x68);
    ddr_phy_reg_wr(0x900f9,0x798);
    ddr_phy_reg_wr(0x900fa,0x16a);
    ddr_phy_reg_wr(0x900fb,0x78);
    ddr_phy_reg_wr(0x900fc,0x7a0);
    ddr_phy_reg_wr(0x900fd,0x16a);
    ddr_phy_reg_wr(0x900fe,0x68);
    ddr_phy_reg_wr(0x900ff,0x790);
    ddr_phy_reg_wr(0x90100,0x16a);
    ddr_phy_reg_wr(0x90101,0x8);
    ddr_phy_reg_wr(0x90102,0x8b10);
    ddr_phy_reg_wr(0x90103,0x168);
    ddr_phy_reg_wr(0x90104,0x8);
    ddr_phy_reg_wr(0x90105,0xab10);
    ddr_phy_reg_wr(0x90106,0x168);
    ddr_phy_reg_wr(0x90107,0xa);
    ddr_phy_reg_wr(0x90108,0x408);
    ddr_phy_reg_wr(0x90109,0x169);
    ddr_phy_reg_wr(0x9010a,0x58);
    ddr_phy_reg_wr(0x9010b,0x0);
    ddr_phy_reg_wr(0x9010c,0x68);
    ddr_phy_reg_wr(0x9010d,0x0);
    ddr_phy_reg_wr(0x9010e,0x408);
    ddr_phy_reg_wr(0x9010f,0x169);
    ddr_phy_reg_wr(0x90110,0x0);
    ddr_phy_reg_wr(0x90111,0x8b10);
    ddr_phy_reg_wr(0x90112,0x168);
    ddr_phy_reg_wr(0x90113,0x1);
    ddr_phy_reg_wr(0x90114,0xab10);
    ddr_phy_reg_wr(0x90115,0x168);
    ddr_phy_reg_wr(0x90116,0x0);
    ddr_phy_reg_wr(0x90117,0x1d8);
    ddr_phy_reg_wr(0x90118,0x169);
    ddr_phy_reg_wr(0x90119,0x80);
    ddr_phy_reg_wr(0x9011a,0x790);
    ddr_phy_reg_wr(0x9011b,0x16a);
    ddr_phy_reg_wr(0x9011c,0x18);
    ddr_phy_reg_wr(0x9011d,0x7aa);
    ddr_phy_reg_wr(0x9011e,0x6a);
    ddr_phy_reg_wr(0x9011f,0xa);
    ddr_phy_reg_wr(0x90120,0x0);
    ddr_phy_reg_wr(0x90121,0x1e9);
    ddr_phy_reg_wr(0x90122,0x8);
    ddr_phy_reg_wr(0x90123,0x8080);
    ddr_phy_reg_wr(0x90124,0x108);
    ddr_phy_reg_wr(0x90125,0xf);
    ddr_phy_reg_wr(0x90126,0x408);
    ddr_phy_reg_wr(0x90127,0x169);
    ddr_phy_reg_wr(0x90128,0xc);
    ddr_phy_reg_wr(0x90129,0x0);
    ddr_phy_reg_wr(0x9012a,0x68);
    ddr_phy_reg_wr(0x9012b,0x9);
    ddr_phy_reg_wr(0x9012c,0x0);
    ddr_phy_reg_wr(0x9012d,0x1a9);
    ddr_phy_reg_wr(0x9012e,0x0);
    ddr_phy_reg_wr(0x9012f,0x408);
    ddr_phy_reg_wr(0x90130,0x169);
    ddr_phy_reg_wr(0x90131,0x0);
    ddr_phy_reg_wr(0x90132,0x8080);
    ddr_phy_reg_wr(0x90133,0x108);
    ddr_phy_reg_wr(0x90134,0x8);
    ddr_phy_reg_wr(0x90135,0x7aa);
    ddr_phy_reg_wr(0x90136,0x6a);
    ddr_phy_reg_wr(0x90137,0x0);
    ddr_phy_reg_wr(0x90138,0x8568);
    ddr_phy_reg_wr(0x90139,0x108);
    ddr_phy_reg_wr(0x9013a,0xb7);
    ddr_phy_reg_wr(0x9013b,0x790);
    ddr_phy_reg_wr(0x9013c,0x16a);
    ddr_phy_reg_wr(0x9013d,0x1f);
    ddr_phy_reg_wr(0x9013e,0x0);
    ddr_phy_reg_wr(0x9013f,0x68);
    ddr_phy_reg_wr(0x90140,0x8);
    ddr_phy_reg_wr(0x90141,0x8558);
    ddr_phy_reg_wr(0x90142,0x168);
    ddr_phy_reg_wr(0x90143,0xf);
    ddr_phy_reg_wr(0x90144,0x408);
    ddr_phy_reg_wr(0x90145,0x169);
    ddr_phy_reg_wr(0x90146,0xd);
    ddr_phy_reg_wr(0x90147,0x0);
    ddr_phy_reg_wr(0x90148,0x68);
    ddr_phy_reg_wr(0x90149,0x0);
    ddr_phy_reg_wr(0x9014a,0x408);
    ddr_phy_reg_wr(0x9014b,0x169);
    ddr_phy_reg_wr(0x9014c,0x0);
    ddr_phy_reg_wr(0x9014d,0x8558);
    ddr_phy_reg_wr(0x9014e,0x168);
    ddr_phy_reg_wr(0x9014f,0x8);
    ddr_phy_reg_wr(0x90150,0x3c8);
    ddr_phy_reg_wr(0x90151,0x1a9);
    ddr_phy_reg_wr(0x90152,0x3);
    ddr_phy_reg_wr(0x90153,0x370);
    ddr_phy_reg_wr(0x90154,0x129);
    ddr_phy_reg_wr(0x90155,0x20);
    ddr_phy_reg_wr(0x90156,0x2aa);
    ddr_phy_reg_wr(0x90157,0x9);
    ddr_phy_reg_wr(0x90158,0x8);
    ddr_phy_reg_wr(0x90159,0xe8);
    ddr_phy_reg_wr(0x9015a,0x109);
    ddr_phy_reg_wr(0x9015b,0x0);
    ddr_phy_reg_wr(0x9015c,0x8140);
    ddr_phy_reg_wr(0x9015d,0x10c);
    ddr_phy_reg_wr(0x9015e,0x10);
    ddr_phy_reg_wr(0x9015f,0x8138);
    ddr_phy_reg_wr(0x90160,0x104);
    ddr_phy_reg_wr(0x90161,0x8);
    ddr_phy_reg_wr(0x90162,0x448);
    ddr_phy_reg_wr(0x90163,0x109);
    ddr_phy_reg_wr(0x90164,0xf);
    ddr_phy_reg_wr(0x90165,0x7c0);
    ddr_phy_reg_wr(0x90166,0x109);
    ddr_phy_reg_wr(0x90167,0x0);
    ddr_phy_reg_wr(0x90168,0xe8);
    ddr_phy_reg_wr(0x90169,0x109);
    ddr_phy_reg_wr(0x9016a,0x47);
    ddr_phy_reg_wr(0x9016b,0x630);
    ddr_phy_reg_wr(0x9016c,0x109);
    ddr_phy_reg_wr(0x9016d,0x8);
    ddr_phy_reg_wr(0x9016e,0x618);
    ddr_phy_reg_wr(0x9016f,0x109);
    ddr_phy_reg_wr(0x90170,0x8);
    ddr_phy_reg_wr(0x90171,0xe0);
    ddr_phy_reg_wr(0x90172,0x109);
    ddr_phy_reg_wr(0x90173,0x0);
    ddr_phy_reg_wr(0x90174,0x7c8);
    ddr_phy_reg_wr(0x90175,0x109);
    ddr_phy_reg_wr(0x90176,0x8);
    ddr_phy_reg_wr(0x90177,0x8140);
    ddr_phy_reg_wr(0x90178,0x10c);
    ddr_phy_reg_wr(0x90179,0x0);
    ddr_phy_reg_wr(0x9017a,0x478);
    ddr_phy_reg_wr(0x9017b,0x109);
    ddr_phy_reg_wr(0x9017c,0x0);
    ddr_phy_reg_wr(0x9017d,0x1);
    ddr_phy_reg_wr(0x9017e,0x8);
    ddr_phy_reg_wr(0x9017f,0x8);
    ddr_phy_reg_wr(0x90180,0x4);
    ddr_phy_reg_wr(0x90181,0x0);
    ddr_phy_reg_wr(0x90006,0x8);
    ddr_phy_reg_wr(0x90007,0x7c8);
    ddr_phy_reg_wr(0x90008,0x109);
    ddr_phy_reg_wr(0x90009,0x0);
    ddr_phy_reg_wr(0x9000a,0x400);
    ddr_phy_reg_wr(0x9000b,0x106);
    ddr_phy_reg_wr(0xd00e7,0x400);
    ddr_phy_reg_wr(0x90017,0x0);
    ddr_phy_reg_wr(0x9001f,0x29);
    ddr_phy_reg_wr(0x90026,0x68);
    ddr_phy_reg_wr(0x400d0,0x0);
    ddr_phy_reg_wr(0x400d1,0x101);
    ddr_phy_reg_wr(0x400d2,0x105);
    ddr_phy_reg_wr(0x400d3,0x107);
    ddr_phy_reg_wr(0x400d4,0x10f);
    ddr_phy_reg_wr(0x400d5,0x202);
    ddr_phy_reg_wr(0x400d6,0x20a);
    ddr_phy_reg_wr(0x400d7,0x20b);
    ddr_phy_reg_wr(0x2003a,0x2);
    ddr_phy_reg_wr(0x200be,0x3);//3
    ddr_phy_reg_wr(0x2000b,0x85);
    ddr_phy_reg_wr(0x2000c,0x10a);
    ddr_phy_reg_wr(0x2000d,0xa6a);
    ddr_phy_reg_wr(0x2000e,0x2c);
    ddr_phy_reg_wr(0x9000c,0x0);
    ddr_phy_reg_wr(0x9000d,0x173);
    ddr_phy_reg_wr(0x9000e,0x60);
    ddr_phy_reg_wr(0x9000f,0x6110);
    ddr_phy_reg_wr(0x90010,0x2152);
    ddr_phy_reg_wr(0x90011,0xdfbd);
    ddr_phy_reg_wr(0x90012,0xffff);
    ddr_phy_reg_wr(0x90013,0x6152);
    ddr_phy_reg_wr(0x40080,0xe0);
    ddr_phy_reg_wr(0x40081,0x12);
    ddr_phy_reg_wr(0x40082,0xe0);
    ddr_phy_reg_wr(0x40083,0x12);
    ddr_phy_reg_wr(0x40084,0xe0);
    ddr_phy_reg_wr(0x40085,0x12);
    ddr_phy_reg_wr(0x400fd,0xf);
    ddr_phy_reg_wr(0x10011,0x1);
    ddr_phy_reg_wr(0x10012,0x1);
    ddr_phy_reg_wr(0x10013,0x180);
    ddr_phy_reg_wr(0x10018,0x1);
    ddr_phy_reg_wr(0x10002,0x6209);
    ddr_phy_reg_wr(0x100b2,0x1);
    ddr_phy_reg_wr(0x101b4,0x1);
    ddr_phy_reg_wr(0x102b4,0x1);
    ddr_phy_reg_wr(0x103b4,0x1);
    ddr_phy_reg_wr(0x104b4,0x1);
    ddr_phy_reg_wr(0x105b4,0x1);
    ddr_phy_reg_wr(0x106b4,0x1);
    ddr_phy_reg_wr(0x107b4,0x1);
    ddr_phy_reg_wr(0x108b4,0x1);
    ddr_phy_reg_wr(0x11011,0x1);
    ddr_phy_reg_wr(0x11012,0x1);
    ddr_phy_reg_wr(0x11013,0x180);
    ddr_phy_reg_wr(0x11018,0x1);
    ddr_phy_reg_wr(0x11002,0x6209);
    ddr_phy_reg_wr(0x110b2,0x1);
    ddr_phy_reg_wr(0x111b4,0x1);
    ddr_phy_reg_wr(0x112b4,0x1);
    ddr_phy_reg_wr(0x113b4,0x1);
    ddr_phy_reg_wr(0x114b4,0x1);
    ddr_phy_reg_wr(0x115b4,0x1);
    ddr_phy_reg_wr(0x116b4,0x1);
    ddr_phy_reg_wr(0x117b4,0x1);
    ddr_phy_reg_wr(0x118b4,0x1);
    ddr_phy_reg_wr(0x12011,0x1);
    ddr_phy_reg_wr(0x12012,0x1);
    ddr_phy_reg_wr(0x12013,0x180);
    ddr_phy_reg_wr(0x12018,0x1);
    ddr_phy_reg_wr(0x12002,0x6209);
    ddr_phy_reg_wr(0x120b2,0x1);
    ddr_phy_reg_wr(0x121b4,0x1);
    ddr_phy_reg_wr(0x122b4,0x1);
    ddr_phy_reg_wr(0x123b4,0x1);
    ddr_phy_reg_wr(0x124b4,0x1);
    ddr_phy_reg_wr(0x125b4,0x1);
    ddr_phy_reg_wr(0x126b4,0x1);
    ddr_phy_reg_wr(0x127b4,0x1);
    ddr_phy_reg_wr(0x128b4,0x1);
    ddr_phy_reg_wr(0x13011,0x1);
    ddr_phy_reg_wr(0x13012,0x1);
    ddr_phy_reg_wr(0x13013,0x180);
    ddr_phy_reg_wr(0x13018,0x1);
    ddr_phy_reg_wr(0x13002,0x6209);
    ddr_phy_reg_wr(0x130b2,0x1);
    ddr_phy_reg_wr(0x131b4,0x1);
    ddr_phy_reg_wr(0x132b4,0x1);
    ddr_phy_reg_wr(0x133b4,0x1);
    ddr_phy_reg_wr(0x134b4,0x1);
    ddr_phy_reg_wr(0x135b4,0x1);
    ddr_phy_reg_wr(0x136b4,0x1);
    ddr_phy_reg_wr(0x137b4,0x1);
    ddr_phy_reg_wr(0x138b4,0x1);
    ddr_phy_reg_wr(0x20089,0x1);
    ddr_phy_reg_wr(0x20088,0x19);
    ddr_phy_reg_wr(0xc0080,0x2);
    ddr_phy_reg_wr(0xd0000,0x1);
    
    printf("dwc_ddrphy_phyinit_userCustom_customPostTrain()\n");
    dwc_ddrphy_phyinit_userCustom_customPostTrain();

    printf("dwc_ddrphy_phyinit_userCustom_J_enterMissionMode()\n");
    dwc_ddrphy_phyinit_userCustom_J_enterMissionMode();

}//}}}A210:phy_init_skiptrain

//A210:phy_init_skiptrain_related{{{
static void dwc_ddrphy_phyinit_userCustom_overrideUserInput(){

}

static void dwc_ddrphy_phyinit_userCustom_A_bringupPower(){

}

static void dwc_ddrphy_phyinit_userCustom_B_startClockResetPhy(){

}

static void dwc_ddrphy_phyinit_userCustom_customPostTrain(){

}

static void dwc_ddrphy_phyinit_userCustom_J_enterMissionMode(){

}
//}}}A210:phy_init_skiptrain_related
#endif

//A210:ctrl en{{{
static void ctrl_en(void) {

    printf("Enter ctrl_en()...\n");
    wr(SWCTL,0x00000000);
    wr(INIT0,0x00020002);
    wr(SWCTL,0x00000001);

    wr(SWCTL,0x00000000);
    //wr(SWCTL,0x00000000);
    wr(DFIMISC,0x00001034);// [5]dfi_init_start
    //wr(SWCTL,0x00000001);
    //while(rd(SWSTAT)!=0x00000001);
    
    printf("polling dfi_init_complete\n");
    while(rd(DFISTAT)!=0x00000001); //polling dfi_init_complete

    // wr(SWCTL,0x00000000);
    wr(DFIMISC,0x00001014);
    wr(DFIMISC,0x00001015);
    //wr(PWRCTL,0x0000000a); //[3] dfi_dram_clk_disable [1] powerdown_en
    //if u want to enable auto powerdown, u can cfg PWRCTL in *.c/*.sv
    wr(PWRCTL,0x00000000); //SoC init ddr don't care DRAM lowpower feature

    wr(SWCTL,0x00000001);

    while(rd(SWSTAT)!=0x00000001);

    //for(integer i=0;i<2048;i=i+1)
    while(rd(STAT)!=0x00000001);

    wr(DFIPHYMSTR,0x14000001);
    //wr(SWCTL,0x00000000);
    //wr(INIT0,0x00020002);
    //wr(SWCTL,0x00000001);
    
    while(rd(SWSTAT)!=0x00000001);

    printf("Exit ctrl_en()...\n");
}//}}}

static void enable_axi_port(void) {
    wr(DBG1,0);
    wr(PCTRL_0,1);
}

//A210:enable auto refresh{{{
static void enable_auto_refresh(void) {
    wr(RFSHCTL3,0x0);
}//}}}A210:enable auto refresh

void ddr_sysreg_wr(unsigned int addr,unsigned int wr_data) {
  wr(addr+DDR_SYSREG_BADDR,wr_data);
}

unsigned int ddr_sysreg_rd(unsigned int addr) {
  int rdata;
  rdata = rd(addr+DDR_SYSREG_BADDR);
  return rdata;
}

void slc_dual_sysreg_wr(unsigned int addr,unsigned int wr_data) {
  wr(addr+SLC_DUAL_SYSREG_BADDR,wr_data);
}

unsigned int slc_dual_sysreg_rd(unsigned int addr) {
  int rdata;
  rdata = rd(addr+SLC_DUAL_SYSREG_BADDR);
  return rdata;
}

#ifdef lpddr4_skiptrain_4266
//P1:ddr phy reg wr{{{
static void ddr_phy_reg_wr(unsigned int addr,unsigned int wr_data) {
    //unsigned int ddr_phy_sel,addr_low;
    addr<<=1;
    //ddr_phy_sel=(addr)&0x00ffffff;
    //ddr_phy_sel>>=21;
    //addr_low=(addr)&0x001fffff;
    //wr(_SYS_REG_DDR_PHY_PS_SEL,ddr_phy_sel);
    printf("ddr_phy_reg_wr: %x, %x, %x\n", _DDR_PHY_BADDR, addr, wr_data);
    wr16(_DDR_PHY_BADDR+addr, wr_data);

}
#endif

//P1:ddr phy reg rd{{{
unsigned int ddr_phy_reg_rd(unsigned int addr) {
    //unsigned int ddr_phy_sel,addr_low,rd_data;
    unsigned int rd_data;
    addr<<=1;
    //ddr_phy_sel=(addr)&0x00ffffff;
    //ddr_phy_sel>>=21;
    //addr_low=(addr)&0x001fffff;
    //wr(_SYS_REG_DDR_PHY_PS_SEL,ddr_phy_sel);
    rd_data=rd16(_DDR_PHY_BADDR+addr);
    return rd_data;
}

#if 0
void udelay(unsigned int value)
{
    wait_ns(value*1000);
}
#endif


#ifdef ENABLE_DDR_INLINE_ECC
void ecc_scrubber_enable(void) {
    wr (            SBRCTL, 0x00001030 );//scrub_en  // [0] scrub_during_lowpower  // [1] scrub_mode  // [2] scrub_burst  // [6:4] scrub_interval  // [20:8]
    wr (         SBRWDATA0, 0x0ecc0ecc );
    //wr (         SBRSTART0, 0x0ce6169f );
    wr (         SBRSTART0, 0x00000000 );
    wr (         SBRSTART1, 0x00000000 );
    //wr (         SBRRANGE0, 0x0ce61c4a );
    wr (         SBRRANGE0, 0x00000040 );
    wr (         SBRRANGE1, 0x00000000 );
}

void inline_ecc_enable(void) {
   //wr (           ECCCFG0, 0x20324890 );
   wr (           ECCCFG0, 0x083f7f44 );//ecc_mode  // [2:0] dis_scrub  // [4] ecc_ap_en  // [6] ecc_region_remap_en  // [7] ecc_region_map  // [14:8] blk_channel_idle_time_x32  // [21:16] ecc_ap_err_threshold  // [26:24] ecc_region_map_other  // [29] ecc_region_map_granu  // [31:30]
   //wr (           ECCCFG1, 0x00000520 );
   wr (            ECCCTL, 0x00000700 );//ecc_corrected_err_clr  // [0] ecc_uncorrected_err_clr  // [1] ecc_corr_err_cnt_clr  // [2] ecc_uncorr_err_cnt_clr  // [3] ecc_ap_err_intr_clr  // [4]        ecc_corrected_err_intr_en  // [8] ecc_uncorrected_err_intr_en  // [9] ecc_ap_err_intr_en  // [10] ecc_corrected_err_intr_force  // [16] ecc_uncorrected_err_intr_force  // [17] ecc_ap_err_intr_force  // [18]
    //wr (    ECCPOISONADDR0, 0x0000082c );
    //wr (    ECCPOISONADDR1, 0x22026ac6 );
}

void lpddr4_ctrl_init_srank_inline_ecc_enable(void)
{
  
    //[           0] RESET: <aresetn> for Port 0  ASSERTED (ACTIVE LOW)
    //[           0] RESET: <core_ddrc_rstn> ASSERTED (ACTIVE LOW)
    //[           0] RESET: <presetn> ASSERTED (ACTIVE LOW)
    //[     1067976] RESET: <presetn> DEASSERTED
    wr (              DBG1, 0x00000001 );
    wr (            PWRCTL, 0x00000001 );
          while(rd(              STAT)!= 0x00000000 );
    // R uMCTL2 0x00000004 (              STAT, 0x00000000 );
    wr (              MSTR, 0x81080020 );
    wr (           MRCTRL0, 0x40003030 );
    wr (           MRCTRL1, 0x0002fc03 );
    //wr (           MRCTRL2, 0xf7c6dbdb );
    wr (          DERATEEN, 0x00000405 );
    wr (         DERATEINT, 0x9df2d653 );
    wr (             MSTR2, 0x00000001 );
    wr (         DERATECTL, 0x00000000 );
    //wr (            PWRCTL, 0x00000100 );
    wr(PWRCTL,0x00000020);
    wr (            PWRTMG, 0x002ba400 );

    wr (           HWLPCTL, 0x00060001 );
    wr (          HWFFCCTL, 0x0000ba0c );
    wr (     HWFFCEX_RANK1, 0x16070205 );
    wr (          RFSHCTL0, 0x00210004 );
    wr (          RFSHCTL1, 0x000f0026 );
    wr (          RFSHCTL3, 0x00000001 );
    wr (           RFSHTMG, 0x82000098 );
    wr (          RFSHTMG1, 0x00610000 );
    //wr (          RFSHCTL0, 0x80a0b000 );
    //wr (          RFSHCTL1, 0x000f0026 );
    //wr (          RFSHCTL3, 0x00000001 );
    //wr (           RFSHTMG, 0x82000098 );
    //wr (          RFSHTMG1, 0x00610000 );
    inline_ecc_enable();
    printf("Enable inline ecc\n");
    wr (        CRCPARCTL0, 0x00000000 );
    wr (        CRCPARCTL1, 0x00001000 );
    wr (             INIT0, 0x00020002 );
    wr (             INIT1, 0x00010002 );
    wr (             INIT2, 0x00002300 );
    wr (             INIT3, 0x0074003f );
#ifdef CONFIG_DDR_DBI_OFF
    wr (             INIT4, 0x00320000 );
#else
    wr (             INIT4, 0x00f20000 );
#endif
    wr (             INIT5, 0x0005000c );
    wr (             INIT6, 0x0000004d );
    wr (             INIT7, 0x0000004d );
    wr (           DIMMCTL, 0x00000000 );
    wr (           RANKCTL, 0x0000ab9f );
    wr (          RANKCTL1, 0x0000001a );
    wr (          DRAMTMG0, 0x2221242d );
    wr (          DRAMTMG1, 0x00090941 );
#ifdef CONFIG_DDR_DBI_OFF
    wr (          DRAMTMG2, 0x09121219 );//[29:24] -write latency [21:16] read latency [13:8] rd2wr [5:0] wr2rd
    printf("Enter LP4 DBI OFF mode\n");
#else
    //wr(DRAMTMG2,0x09141619);//[29:24] -write latency [21:16] read latency [13:8] rd2wr [5:0] wr2rd
    printf("Enter LP4 DBI ON mode\n");
    wr(DRAMTMG2,0x09141f1a);//[29:24] -write latency [21:16] read latency [13:8] rd2wr [5:0] wr2rd
#endif
    wr (          DRAMTMG3, 0x00f0f000 );
    wr (          DRAMTMG4, 0x14040914 );
    //wr (          DRAMTMG5, 0x02061111 );
    wr (          DRAMTMG5, 0x09ba1111 );
    wr (          DRAMTMG6, 0x0101000a );
    wr (          DRAMTMG7, 0x00000602 );
    wr (          DRAMTMG8, 0x00000101 );
    //wr (          DRAMTMG9, 0x0000003f );
    //wr (         DRAMTMG11, 0x0101001e );
    wr (         DRAMTMG12, 0x00020000 );
    wr (         DRAMTMG13, 0x0e100002 );
    wr (         DRAMTMG14, 0x00000133 );
    //wr (         DRAMTMG15, 0x01000000 );
    wr (         DRAMTMG17, 0x00d6006b );
    wr (            ZQCTL0, 0xc42d0026 );
    wr (            ZQCTL1, 0x03600800 );
    wr (            ZQCTL2, 0x00000000 );
#ifdef CONFIG_DDR_DBI_OFF
    wr (           DFITMG0, 0x049f820e );//[22:16] dfi_t_rddate_en=RL-5
#else
    wr (           DFITMG0, 0x04a3820e );//[28:24] dft_t_ctrl_delay [22:16] dfi_t_rddate_en=RL-5
#endif
    //wr (           DFITMG0, 0x049f820e );
    wr (           DFITMG1, 0x00090303 );
    wr (         DFILPCFG0, 0x0351a001 );
    //wr (         DFILPCFG1, 0x00000021 );
    wr (           DFIMISC, 0x00001015 );
#ifdef CONFIG_DDR_DBI_OFF
    //wr(DFITMG2,0x00001f0e);//[14:9] dfi_tphy_rdcslat
    wr (           DFITMG2, 0x0000230e );
    wr (            DBICTL, 0x00000001 ); //dbi-off
#else
    wr (           DFITMG2, 0x0000230e );
    wr (            DBICTL, 0x00000007 ); //dbi-on
#endif
    wr (        DFIPHYMSTR, 0x14000001 );
    wr (            ODTCFG, 0x060a0c44 );
    wr (           DFIUPD0, 0x00400018 );
    wr (           DFIUPD1, 0x00280032 );
    wr (           DFIUPD2, 0x00000000 );
    wr (        DFIPHYMSTR, 0x14000001 );
    wr (            ODTMAP, 0x00000000 );
    wr (             SCHED, 0x1f82bf1c );//update by 95P 20240411 [2]page-close enable [14:8] 32'h1b: lpr entry num=62, hpr entry num=4
    wr (            SCHED1, 0x4400b00f );
    wr (          PERFHPR1, 0x0f000001 );
    wr (          PERFLPR1, 0x0f00007f );
    wr (           PERFWR1, 0x0f00007f );
    wr (            SCHED3, 0x00000208 );
    wr (            SCHED4, 0x08400810 );
    wr (              DBG0, 0x00000000 );
    wr (              DBG1, 0x00000000 );
    wr (            DBGCMD, 0x00000000 );
    wr (             SWCTL, 0x00000001 );
    wr (       SWCTLSTATIC, 0x00000000 );
    wr (         POISONCFG, 0x00110001 );
    wr (           PCTRL_0, 0x00000001 );

    wr (    FREQ1_DERATEEN, 0x00001302 );
    wr (   FREQ1_DERATEINT, 0x0fe4949a );
    wr (      FREQ1_PWRTMG, 0x0008941d );
    wr (FREQ1_HWFFCEX_RANK1, 0x41050001 );//srank not use
    wr (    FREQ1_RFSHCTL0, 0x28d03004 );
    wr (     FREQ1_RFSHTMG, 0x81c78083 );
    wr (    FREQ1_RFSHTMG1, 0x00540000 );
    wr (       FREQ1_INIT3, 0x00640036 );
#ifdef CONFIG_DDR_DBI_OFF
    wr (       FREQ1_INIT4, 0x00320000 );//INIT:MR3 MR13
#else
    wr (       FREQ1_INIT4, 0x00f20000 );//INIT:MR3 MR13
#endif
    wr (       FREQ1_INIT6, 0x0000004d );
    wr (       FREQ1_INIT7, 0x0000004d );
    wr (     FREQ1_RANKCTL, 0x0000f72d );
    wr (    FREQ1_RANKCTL1, 0x00000006 );
    wr (    FREQ1_DRAMTMG0, 0x1e261f28 );
    wr (    FREQ1_DRAMTMG1, 0x00070738 );
#ifdef CONFIG_DDR_DBI_OFF
    wr (    FREQ1_DRAMTMG2, 0x08101116 );//WL 16 RL32 rd2wr 17 wr2rd 22
    //printf("Enter LP4 DBI OFF mode\n");
#else
    //wr (    FREQ1_DRAMTMG2, 0x08101116 );//[29:24] -write latency [21:16] read latency [13:8] rd2wr [5:0] wr2rd
    //printf("Enter LP4 DBI ON mode\n");
#endif
    wr (    FREQ1_DRAMTMG3, 0x00e0e006 );
    wr (    FREQ1_DRAMTMG4, 0x11040a11 );
    wr (    FREQ1_DRAMTMG5, 0x02720e0e );
    wr (    FREQ1_DRAMTMG6, 0x0e090008 );
    wr (    FREQ1_DRAMTMG7, 0x00000b0d );
    wr (    FREQ1_DRAMTMG8, 0x01010101 );
    //wr (    FREQ1_DRAMTMG9, 0x0000003e );
    //wr (   FREQ1_DRAMTMG11, 0x01010015 );
    wr (   FREQ1_DRAMTMG12, 0x00020000 );
    wr (   FREQ1_DRAMTMG13, 0x0d100002 );
    wr (   FREQ1_DRAMTMG14, 0x0000010c );
    //wr (   FREQ1_DRAMTMG15, 0x01000000 );
    wr (   FREQ1_DRAMTMG17, 0x00d6006b );
    wr (      FREQ1_ZQCTL0, 0x43a5001c );
#ifdef CONFIG_DDR_DBI_OFF
    wr (     FREQ1_DFITMG0, 0x059b820c );//[22:16] dfi_t_rddate_en=RL-5
#else
    wr (     FREQ1_DFITMG0, 0x059f820c );//[28:24] dft_t_ctrl_delay [22:16] dfi_t_rddate_en=RL-5
#endif
    wr (     FREQ1_DFITMG1, 0x000a0404 );
    wr (     FREQ1_DFITMG2, 0x00001f0c );
    wr (      FREQ1_ODTCFG, 0x0c0b0a60 );

    //TODO:UPDATE 3200 MT
    wr (    FREQ2_DERATEEN, 0x00001202 );
    wr (   FREQ2_DERATEINT, 0x726d4ada );
    wr (      FREQ2_PWRTMG, 0x0040ae04 );
    wr (FREQ2_HWFFCEX_RANK1, 0x55010307 );
    wr (    FREQ2_RFSHCTL0, 0x00210004 );
    wr (     FREQ2_RFSHTMG, 0x000c00cc );
    wr (    FREQ2_RFSHTMG1, 0x00480000 );
    wr (       FREQ2_INIT3, 0x0054002d );
#ifdef CONFIG_DDR_DBI_OFF
    wr (       FREQ2_INIT4, 0x00300000 );//WR PRST 0.5 tck
#else
    wr (       FREQ2_INIT4, 0x00f00000 );
#endif
    wr (       FREQ2_INIT6, 0x0000004d );
    wr (       FREQ2_INIT7, 0x0000004d );
    wr (     FREQ2_RANKCTL, 0x0000032f );
    wr (    FREQ2_RANKCTL1, 0x00000004 );
    wr (    FREQ2_DRAMTMG0, 0x1b201b22 );
    wr (    FREQ2_DRAMTMG1, 0x00060630 );
#ifdef CONFIG_DDR_DBI_OFF
    wr (    FREQ2_DRAMTMG2, 0x070e0f14 );//WL 14 RL28 rd2wr 15 wr2rd 20
#else
    wr (    FREQ2_DRAMTMG2, 0x07101214 );
#endif
    wr (    FREQ2_DRAMTMG3, 0x00b0c000 );
    wr (    FREQ2_DRAMTMG4, 0x0f04080f );
    wr (    FREQ2_DRAMTMG5, 0x02040c0c );
    wr (    FREQ2_DRAMTMG6, 0x02040c0c );
    wr (    FREQ2_DRAMTMG7, 0x01010007 );
    wr (    FREQ2_DRAMTMG8, 0x00000101 );
    //wr (    FREQ2_DRAMTMG9, 0x0000003f );
    //wr (   FREQ2_DRAMTMG11, 0x0101001e );
    wr (   FREQ2_DRAMTMG12, 0x00020000 );
    wr (   FREQ2_DRAMTMG13, 0x0c100002 );
    wr (   FREQ2_DRAMTMG14, 0x00000135 );
    //wr (   FREQ1_DRAMTMG15, 0x01000000 );
    wr (   FREQ2_DRAMTMG17, 0x00d6006b );
    wr (      FREQ2_ZQCTL0, 0xc3200018 );
#ifdef CONFIG_DDR_DBI_OFF
    wr (     FREQ2_DFITMG0, 0x0397820a );
#else
#endif
    wr (     FREQ2_DFITMG1, 0x00090202 );
    wr (     FREQ2_DFITMG2, 0x0000170a );
    wr (      FREQ2_ODTCFG, 0x06080a30 );
    
    wr (    FREQ3_DERATEEN, 0x00001000 );
    wr (   FREQ3_DERATEINT, 0x0fe4949a );
    wr (      FREQ3_PWRTMG, 0x0008941d );
    wr (FREQ3_HWFFCEX_RANK1, 0x1d020302 );
    wr (    FREQ3_RFSHCTL0, 0x28103005 );
    wr (     FREQ3_RFSHTMG, 0x80828026 );
    wr (    FREQ3_RFSHTMG1, 0x00180000 );
    wr (       FREQ3_INIT3, 0x00140009 );
#ifdef CONFIG_DDR_DBI_OFF
    wr (       FREQ3_INIT4, 0x00320000 );//INIT:MR3 MR13
#else
    wr (       FREQ3_INIT4, 0x00f20000 );//INIT:MR3 MR13
#endif
    wr (       FREQ3_INIT6, 0x0000004d );
    wr (       FREQ3_INIT7, 0x0000004d );
    wr (     FREQ3_RANKCTL, 0x0000f8cd );
    wr (    FREQ3_RANKCTL1, 0x0000000d );
    wr (    FREQ3_DRAMTMG0, 0x0d0b080c );
    wr (    FREQ3_DRAMTMG1, 0x00030410 );
#ifdef CONFIG_DDR_DBI_OFF
    wr (    FREQ3_DRAMTMG2, 0x0305080c );//WL 6 RL 10 rd2wr 8 wr2rd 12
    //printf("Enter LP4 DBI OFF mode\n");
#else
    //wr(DRAMTMG2,0x09141619);//[29:24] -write latency [21:16] read latency [13:8] rd2wr [5:0] wr2rd
    //printf("Enter LP4 DBI ON mode\n");
#endif 
    wr (    FREQ3_DRAMTMG3, 0x00505006 );
    wr (    FREQ3_DRAMTMG4, 0x05040305 );
    wr (    FREQ3_DRAMTMG5, 0x02720404 );
    wr (    FREQ3_DRAMTMG6, 0x0e090004 );
    wr (    FREQ3_DRAMTMG7, 0x00000b0d );
    wr (    FREQ3_DRAMTMG8, 0x01010101 );
    wr (    FREQ3_DRAMTMG9, 0x0000003e );
    wr (   FREQ3_DRAMTMG11, 0x01010015 );
    wr (   FREQ3_DRAMTMG12, 0x00020000 );
    wr (   FREQ3_DRAMTMG13, 0x0a100002 );
    wr (   FREQ3_DRAMTMG14, 0x0000004d );
    //wr (   FREQ1_DRAMTMG15, 0x01000000 );
    wr (   FREQ3_DRAMTMG17, 0x00d6006b );
    wr (      FREQ3_ZQCTL0, 0x410b0008 );
#ifdef CONFIG_DDR_DBI_OFF
    wr (     FREQ3_DFITMG0, 0x05858202 );//[22:16] dfi_t_rddate_en=RL-5 ???
#else
    wr (     FREQ3_DFITMG0, 0x05878202 );//[28:24] dft_t_ctrl_delay [22:16] dfi_t_rddate_en=RL-5
#endif
    wr (     FREQ3_DFITMG1, 0x000a0404 );
    wr (     FREQ3_DFITMG2, 0x00000502 );
    wr (      FREQ3_ODTCFG, 0x0c180a38 );

    
//while(rd(          RFSHCTL3)!= 0x00000000 );
// R uMCTL2 0x00000060 (          RFSHCTL3, 0x00000000 );
    while(rd(RFSHCTL3)!=0x00000001);
    
    //update by perf sim
    wr(PCCFG,0x00000010); 
    wr(PCFGR_0,0x0000500f); //CPU read
    wr(PCFGW_0,0x0000500f); //CPU write
    ecc_scrubber_enable();
    printf("Enable ecc scrubber\n");

    while(rd(PWRCTL)!=0x00000020);
    wr(PWRCTL,0x00000020);

    wr(DBG1,0x00000000);

    while(rd(PWRCTL)!=0x00000020);
    wr(PWRCTL,0x00000020);

    while(rd(PWRCTL)!=0x00000020);
    wr(PWRCTL,0x00000020);

    //while(rd(DFIPHYMSTR)!=0x14000000);
    wr(DFIPHYMSTR,0x14000001);
    wr(SWCTL,0x00000000);
    wr(DFIMISC,0x00001014);
    wr(DFIMISC,0x00001014);
    //wr(SWCTL,0x00000001);
    //while(rd(SWSTAT)!=0x00000001);
    wr(DBG1,0x00000002); 
}


void lpddr4_addr_map_ilecc(void) {
    wr(ADDRMAP0,0x0000001f); 
    wr(ADDRMAP1,0x00050505);//bank +2 
    wr(ADDRMAP2,0x00000000);//col b5+5 ~ col b2+2 
    wr(ADDRMAP3,0x13131300);//col b9 ~ col b6 
    wr(ADDRMAP4,0x00001f1f);//col b11~ col b10 
    wr(ADDRMAP5,0x040f0404);//row_b11 row b2_10 row b1 row b0  +6 
    wr(ADDRMAP6,0x04040404);//row_b15_12 +18
    wr(ADDRMAP7,0x00000f0f);
    wr(ADDRMAP9,0x04040404);//row b5_2 +8
    wr(ADDRMAP10,0x04040404);//row b9_6 +12
    wr(ADDRMAP11,0x00000004);//row_b10 +16
}

void lpddr4_ctrl_en(void) {

    wr(SWCTL,0x00000000);
    wr(INIT0,0x00020002);
    wr(SWCTL,0x00000001);

    wr(SWCTL,0x00000000);
    //wr(SWCTL,0x00000000);
    wr(DFIMISC,0x00001034);// [5]dfi_init_start
    //wr(SWCTL,0x00000001);
    //while(rd(SWSTAT)!=0x00000001);
    
    while(rd(DFISTAT)!=0x00000001); //polling dfi_init_complete

    // wr(SWCTL,0x00000000);
    wr(DFIMISC,0x00001014);
    wr(DFIMISC,0x00001015);
    wr(PWRCTL,0x00000008); //[3] dfi_dram_clk_disable [1] powerdown_en

    wr(SWCTL,0x00000001);

    while(rd(SWSTAT)!=0x00000001);

    //for(integer i=0;i<2048;i=i+1)
    while(rd(STAT)!=0x00000001);

    wr(DFIPHYMSTR,0x14000001);
    //wr(SWCTL,0x00000000);
    //wr(INIT0,0x00020002);
    //wr(SWCTL,0x00000001);
    
    while(rd(SWSTAT)!=0x00000001);
}

void enable_auto_refresh_for_inline_ecc(void) {
    DDR_UMCTL2_STRUCT_REG_S umctl2_reg;
    int rdata;

    //wr(RFSHCTL3,0x0);
    //wr(RFSHCTL3,0x2);

    umctl2_reg.ddr_umctl2_struct_RFSHCTL3.u32 = rd(RFSHCTL3);
    umctl2_reg.ddr_umctl2_struct_RFSHCTL3.dis_auto_refresh =0;
    wr(RFSHCTL3, umctl2_reg.ddr_umctl2_struct_RFSHCTL3.u32); 
    umctl2_reg.ddr_umctl2_struct_RFSHCTL3.u32 = rd(RFSHCTL3);
    umctl2_reg.ddr_umctl2_struct_RFSHCTL3.refresh_update_level = !umctl2_reg.ddr_umctl2_struct_RFSHCTL3.refresh_update_level;
    wr(RFSHCTL3, umctl2_reg.ddr_umctl2_struct_RFSHCTL3.u32); 

}

void ddr_deassert_apb(void)
{
    wr(AP_DDR0_SYSREG_BADDR,0x2); //Presetn
    //wr(AP_DDR1_SYSREG_BADDR,0x2);//if u enable broadcast mode, u should not cfg again

}


/*force_pwrok*/
void pwrok_release(void) {
    #ifndef FORCE_PWROK_WAIT_NS
        int FORCE_PWROK0_WAIT_NS,FORCE_PWROK1_WAIT_NS;
        FORCE_PWROK0_WAIT_NS = 61;
        FORCE_PWROK1_WAIT_NS = 241;
    #endif
    //cfg aon reg
    //pwrok=0; wait 12 ns => 16 DfiClk update by 20240727 
    wr(AP_AON_SYSREG_BADDR+0X10C, 0x0);
    //force_pwrok(0);
    //wait_ns(12);
    //wait_dficlk(16);//FIXME:wait_dficlk not function
    wait_ns(FORCE_PWROK0_WAIT_NS);//16*(1/266.5)*1000 = 60.03ns
    //pwrok=1; wait 64 ns => 64 DfiClk update by 20240727 
    wr(AP_AON_SYSREG_BADDR+0X10C, 0x3);
    //force_pwrok(1);

    //wait_ns(64);
    //wait_dficlk(64);
    wait_ns(FORCE_PWROK1_WAIT_NS);//64*(1/266.5)*1000 = 240.15ns
    //TODO:For ST,u need to cfg aon sysreg not by force!!!
}

void init_ddrc_scrubber()
{
    u32  rdata;
    rdata=rd(SYSREG_DDR_CH0 + DDR_CFG0);
    rdata|=(0x1 <<5 );
    wr(SYSREG_DDR_CH0 + DDR_CFG0,rdata);

    wr(SBRSTART0,0x00000000);
    wr(SBRSTART1,0x00000000);
    wr(SBRRANGE0,0x100FFFFF);//0x80000000~0x90100000 range
    wr(SBRRANGE1,0x00000000);

    wr(SBRWDATA0,0x5a5a5a5a); //data pattern

    rdata=rd(ECCCFG1);
    rdata|=(0x1<<4);
    wr(ECCCFG1,rdata);

    wr(SBRCTL,0x14);
    wr(SBRCTL,0x15);

    rdata=0;
    while((rdata & 0x2) != 0x2)
         rdata=rd(SBRSTAT);

    while((rdata & 0x1) != 0x0)
        rdata=rd(SBRSTAT);

    printf("SBR is Done！ SBRSTAT %0x \n",rdata);
    wr(SBRCTL,0x14);

}
void lpddr4_init_ctrl_sdram_init_inline_ecc_enable(void)
{
    //pll_cfg(4266);
    printf("[init_ddr] inline ecc enable...\n");
    pll_config(4266);
    printf("[init_ddr]pll_config 4266 success \n");

    ddr_slc_top_crg_release();
    //set broadcast_mode_en = 1
    uint32_t tmp = rd(AON_SLC_DUAL_SYSREG_BADDR+ 0x50);
    tmp |= 0x00000001;
    wr(AON_SLC_DUAL_SYSREG_BADDR + 0x50, tmp);
    slc_crg_release();

    pwrok_release();
    //de_assert_pwrok_apb();
    //printf("[init_ddr]de_assert_pwrok_apb success \n");
    ddr_deassert_apb();
    printf("[init_ddr]ddr_deassert_apb success \n");
    //ctrl_phy();

    lpddr4_ctrl_init_srank_inline_ecc_enable();//需要确认AXI port数量
    printf("[init_ddr]lpddr4_ctrl_init_srank_inline_ecc_enable success \n");
    lpddr4_addr_map_ilecc();
    printf("[init_ddr]addrmap inline ecc success \n");
    de_assert_other_reset_ddr();//需要新环境适配,当前为空
    printf("[init_ddr]de_assert_other_reset_ddr success \n");
    //dwc_ddrphy_phyinit_out_lpddr4_skiptrain();
#if 0
#ifdef CONFIG_DDR_DBI_OFF
    dwc_ddrphy_phyinit_out_lpddr4_skiptrain();
#else
    dwc_ddrphy_phyinit_out_lpddr4_skiptrain_dbi();
#endif
#endif
    printf("[init_ddr]dwc_ddrphy_phyinit_out_lpddr4_skiptrain success \n");
    //ctrl_enable();
    lpddr4_ctrl_en();
    printf("[init_ddr]lpddr4_ctrl_en success \n");
    //enable_axi_port(1);
    enable_axi_port();//需要确认AXI port数量
    printf("[init_ddr]enable_axi_port success \n");
    //en_auto_refresh();
    enable_auto_refresh_for_inline_ecc();
    printf("[init_ddr]enable_auto_refresh success \n");

    init_ddrc_scrubber();
    //set broadcast_mode_en = 0
    tmp = rd(AON_SLC_DUAL_SYSREG_BADDR+ 0x50);
    tmp &= (~0x00000001);
    wr(AON_SLC_DUAL_SYSREG_BADDR + 0x50, tmp);
}
#endif

#ifdef SPL_ENABLE_DDR_SCRAMBLE
void enable_ddr_scramble(void) {

    uint32_t val;

    printf("enable_ddr_scramble\n");
    /*enbale DDR0 scramble*/
    /*Misc_Ctrl.interrupt_enable = 1, Misc_Ctrl.cfg_illegal_det_en = 1*/
    val = rd(AON_DDR0_AXISCR_BADDR + 0x08);
    val |= ~ 0xC00000;
    wr(AON_DDR0_AXISCR_BADDR + 0x08, val);
    
    /*Region0-7_start_addr, end_addr config range addr 0-64G*/
    wr(AON_DDR0_AXISCR_BADDR + 0x40, 0x80000000>>12);
    wr(AON_DDR0_AXISCR_BADDR + 0x40, 0x107FFFFFFF>>12);

    /*Config scramble key 128bit*/
    wr(AON_DDR0_AXISCR_BADDR + 0x100, 0x7A42286A);
    wr(AON_DDR0_AXISCR_BADDR + 0x104, 0x83D6870D);
    wr(AON_DDR0_AXISCR_BADDR + 0x108, 0xB5DD1AD1);
    wr(AON_DDR0_AXISCR_BADDR + 0x10c, 0x5936C2C4);

    /*cypher_ctrl.cfg_dkey_val = 1*/
    val = rd(AON_DDR1_AXISCR_BADDR + 0x14);
    val |= 0x1;
    wr(AON_DDR0_AXISCR_BADDR + 0x14, val);

    /*enable DDR1 scramble*/
    /*Misc_Ctrl.interrupt_enable = 1, Misc_Ctrl.cfg_illegal_det_en = 1*/
    val = rd(AON_DDR1_AXISCR_BADDR + 0x08);
    val |= ~ 0xC00000;
    wr(AON_DDR1_AXISCR_BADDR + 0x08, val);
    
    /*Region0-7_start_addr, end_addr config range addr 0-64G*/
    wr(AON_DDR1_AXISCR_BADDR + 0x40, 0x80000000>>12);
    wr(AON_DDR1_AXISCR_BADDR + 0x40, 0x107FFFFFFF>>12);

    /*Config scramble key 128bit*/
    wr(AON_DDR1_AXISCR_BADDR + 0x100, 0x7A42286A);
    wr(AON_DDR1_AXISCR_BADDR + 0x104, 0x83D6870D);
    wr(AON_DDR1_AXISCR_BADDR + 0x108, 0xB5DD1AD1);
    wr(AON_DDR1_AXISCR_BADDR + 0x10c, 0x5936C2C4);

    /*cypher_ctrl.cfg_dkey_val = 1*/
    val = rd(AON_DDR1_AXISCR_BADDR + 0x14);
    val |= 0x1;
    wr(AON_DDR1_AXISCR_BADDR + 0x14, val);

}
#endif

#endif
