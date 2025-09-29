/*
 * Copyright(C) 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include "include/cpu_ss.h"
#include "../include/addr_defines.h"
#include "../include/utils/io.h"

void cpuss_c908_pll_cfg(u32 freq)
{
    u32 locked;
    wr(AP_CPU_SS_CPU_PLL_C908_PLL_CFG1, 0x43000000);
    switch (freq) {
    case 2600:
        wr(AP_CPU_SS_CPU_PLL_C908_PLL_CFG0, 0x120d902);
        break;
    case 2000:
        wr(AP_CPU_SS_CPU_PLL_C908_PLL_CFG0, 0x120a702);
        break;
    case 1896:
        wr(AP_CPU_SS_CPU_PLL_C908_PLL_CFG0, 0x1104f01);
	break;
    case 1800:
        wr(AP_CPU_SS_CPU_PLL_C908_PLL_CFG0, 0x1204b01);
        break;
    case 1500:
        wr(AP_CPU_SS_CPU_PLL_C908_PLL_CFG0, 0x1107d02);
	break;
    case 1200:
        wr(AP_CPU_SS_CPU_PLL_C908_PLL_CFG0, 0x1203201);
        break;
    case 1000:
        wr(AP_CPU_SS_CPU_PLL_C908_PLL_CFG0, 0x1202a01);
        break;
    default:
        printf("ERROR: freq %d not config yet\n", freq);
        break;
    }
    wr(AP_CPU_SS_CPU_PLL_C908_PLL_CFG1, 0x03000000);
    while (1) {
        locked = rd(AP_CPU_SS_CPU_PLL_C908_PLL_STS);
        if (locked == 1) {
            break;
        }
    }
    debug("c908 pll locked\n");
}

void cpuss_c920_pll_cfg(u32 freq)
{
    u32 locked;
    wr(AP_CPU_SS_CPU_PLL_C920_PLL_CFG1, 0x43000000);
    switch (freq) {
    case 2500:
        wr(AP_CPU_SS_CPU_PLL_C920_PLL_CFG0, 0x120d002);
        break;
    case 2298:
        wr(AP_CPU_SS_CPU_PLL_C920_PLL_CFG0, 0x1117F04);
        break;
    case 1896:
        wr(AP_CPU_SS_CPU_PLL_C920_PLL_CFG0, 0x1104f01);
	break;
    case 1900:
        wr(AP_CPU_SS_CPU_PLL_C920_PLL_CFG0, 0x1209e02);
        break;
    case 1700:
        wr(AP_CPU_SS_CPU_PLL_C920_PLL_CFG0, 0x1208e02);
        break;
    case 1500:
        wr(AP_CPU_SS_CPU_PLL_C920_PLL_CFG0, 0x1107d02);
	break;
    case 1000:
        wr(AP_CPU_SS_CPU_PLL_C920_PLL_CFG0, 0x1202a01);
        break;
    default:
        printf("ERROR: freq %d not config yet\n", freq);
        break;
    }
    wr(AP_CPU_SS_CPU_PLL_C920_PLL_CFG1, 0x03000000);
    while (1) {
        locked = rd(AP_CPU_SS_CPU_PLL_C920_PLL_STS);
        if (locked == 1) {
            break;
        }
    }
    debug("c920 pll locked\n");
}
