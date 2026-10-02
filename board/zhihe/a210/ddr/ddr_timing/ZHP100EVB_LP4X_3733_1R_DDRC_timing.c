// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shanghai) Co., Ltd.
 */

#include <string.h>
#include "../../include/utils/utils.h"
#include "../ddr_init.h"

struct dram_cfg_param lpddr4_ddrc_cfg[] = {
    { DBG1(0), 0x00000001 },
    { PWRCTL(0), 0x00000001 },
    { MSTR(0), 0x01080020 },
    // {MRCTRL0(0),0x00003030},
    // {MRCTRL1(0),0x0002d90f},
    { DERATEEN(0), 0x000014f5 },
    { DERATEINT(0), 0x40000000 },
    { DERATECTL(0), 0x00000001 },
    { PWRCTL(0), 0x00000020 },
    { PWRTMG(0), 0x0040ae04 },
    { HWLPCTL(0), 0x00430002 },
    { RFSHCTL0(0), 0x00210004 },
    { RFSHCTL1(0), 0x000f0026 },
    { RFSHCTL3(0), 0x00000001 },
    { RFSHTMG(0), 0x82000098 },
    { RFSHTMG1(0), 0x00610000 },
    { CRCPARCTL0(0), 0x00000000 },
    { INIT0(0), 0xc0020002 },
    { INIT1(0), 0x00010002 },
    { INIT2(0), 0x00002300 },
    { INIT3(0), 0x0074003f },
#ifdef CONFIG_DDR_DBI_OFF
    { INIT4(0), 0x00320000 }, //INIT:MR3 MR13
#else
    { INIT4(0), 0x00f20000 }, //INIT:MR3 MR13
#endif
    { INIT5(0), 0x0005000c },
    { INIT6(0), 0x0000004d },
    { INIT7(0), 0x0000004d },
    { DIMMCTL(0), 0x00000000 },
    { RANKCTL(0), 0x0000ab9f },
    { RANKCTL1(0), 0x0000001a },
    { DRAMTMG0(0), 0x2221482d },
    { DRAMTMG1(0), 0x00090941 },
#ifdef CONFIG_DDR_DBI_OFF
    { DRAMTMG2(0),
      0x09121219 }, //[29:24] -write latency [21:16] read latency [13:8] rd2wr [5:0] wr2rd
#else
    { DRAMTMG2(0),
      0x09141f1a }, //[29:24] -write latency [21:16] read latency [13:8] rd2wr [5:0] wr2rd
#endif
    { DRAMTMG3(0), 0x00f0f000 },
    { DRAMTMG4(0), 0x14040914 },
    { DRAMTMG5(0), 0x02061111 },
    { DRAMTMG6(0), 0x0101000a },
    { DRAMTMG7(0), 0x00000602 },
    { DRAMTMG8(0), 0x00000101 },
    { DRAMTMG12(0), 0x00020000 },
    { DRAMTMG13(0), 0x0e100002 },
    { DRAMTMG14(0), 0x00000133 },
    { ZQCTL0(0), 0xc42d0026 },
    { ZQCTL1(0), 0x03600800 },
    { ZQCTL2(0), 0x00000000 },
#ifdef CONFIG_DDR_DBI_OFF
    { DFITMG0(0), 0x049f820e }, //[22:16] dfi_t_rddate_en=RL-5
#else
    { DFITMG0(0), 0x04a3820e }, //[28:24] dft_t_ctrl_delay [22:16] dfi_t_rddate_en=RL-5
#endif
    { DFITMG1(0), 0x00090303 },
    { DFILPCFG0(0), 0x0351a001 },
    { DFIMISC(0), 0x00001015 },
#ifdef CONFIG_DDR_DBI_OFF
    { DFITMG2(0), 0x0000230e },
    { DBICTL(0), 0x00000001 }, //dbi-off
#else
    { DFITMG2(0), 0x0000230e }, //[14:9] dfi_tphy_rdcslat
    { DBICTL(0), 0x00000007 }, //dbi-on
#endif
    { DFIPHYMSTR(0), 0x14000001 },
    { ODTCFG(0), 0x060a0c44 },
    { DFIUPD0(0), 0x00400018 },
    { DFIUPD1(0), 0x00280032 },
    { DFIUPD2(0), 0x00000000 },
    { DFIPHYMSTR(0), 0x14000001 },
    { ODTMAP(0), 0x00000000 },
    //addrmap
    { ADDRMAP0(0), 0x001f001f },
    { ADDRMAP1(0), 0x00080808 },
    { ADDRMAP2(0), 0x00000000 },
    { ADDRMAP3(0), 0x00000000 },
    { ADDRMAP4(0), 0x00001f1f },
    { ADDRMAP5(0), 0x070f0707 },
    { ADDRMAP6(0), 0x07070707 },
    { ADDRMAP7(0), 0x000000f07 },
    { ADDRMAP9(0), 0x07070707 },
    { ADDRMAP10(0), 0x07070707 },
    { ADDRMAP11(0), 0x00000007 },
    { SCHED(0), 0x1f82bf18 },
    { SCHED1(0), 0x4400b00f },
    { PERFHPR1(0), 0x0f000001 },
    { PERFLPR1(0), 0x0f00007f },
    { PERFWR1(0), 0x0f00007f },
    { SCHED3(0), 0x00000208 },
    { SCHED4(0), 0x08400810 },
    { DBG0(0), 0x00000000 },
    { DBG1(0), 0x00000000 },
    { DBGCMD(0), 0x00000000 },
    { SWCTL(0), 0x00000001 },
    { SWCTLSTATIC(0), 0x00000000 },
    { POISONCFG(0), 0x00000001 },
    { PCTRL_0(0), 0x00000001 },
    { PCCFG(0), 0x00000010 },
    { PCFGR_0(0), 0x0000500f },
    { PCFGW_0(0), 0x0000500f },
    { PWRCTL(0), 0x00000020 },
    { DBG1(0), 0x00000000 },
    { PWRCTL(0), 0x00000020 },
    { PWRCTL(0), 0x00000020 },
    { DFIPHYMSTR(0), 0x14000001 },
    { SWCTL(0), 0x00000000 },
    { DFIMISC(0), 0x00001014 },
    { DFIMISC(0), 0x00001014 },
    { DBG1(0), 0x00000002 },
};

/* lpddr4x 1R timing config params on EVB */
struct dram_timing_info dram_timing = {
    .ddrc_cfg = lpddr4_ddrc_cfg,
    .ddrc_cfg_num = ARRAY_SIZE(lpddr4_ddrc_cfg),
    .fsp_table = { 3733 },
};
