// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include "adc.h"
#include "../include/utils/utils.h"

void adc_init(void)
{
    wr(0x00540200, rd(0x00540200) | 0x8000); // adc_pclk_en
    wr(0x00540400, rd(0x00540400) | 0x800); // sw_adc_prst_n

    wr(0x005a0004, 0x10);     // adc_phy_reset
    wr(0x005a0004, 0x0);      // adc_phy_enadc: standby

    wr(0x005a0000, 0x3);      // adc_phy_selres: 12bit mode
    wr(0x005a000c, 0x1);      // adc_op_ctrl: single mode
    wr(0x005a0050, 0xf);      // adc_int_actual_mask: 0~3 channel
    wr(0x005a0054, 0xf);      // adc_int_delta_mask: 0~3 channel
    wr(0x005a0014, 0x10004);  // adc_fclk_ctrl: default value
    wr(0x005a0018, 0x160);    // adc_start_time: default value
    wr(0x005a001c, 0xe);      // adc_sample_time
}

u64 adc_read(u32 chid, u32 avr)
{
    u32 event = 0;
    u64 result = 0;
    u64 scale = 439453125;
    u32 sample_times = avr;

    u32 ch_en = (0x1<<(12 + chid));
    ch_en |= 0x1;

    for (int i = 0; i < sample_times; i++, event = 0) {
        wr(0x005a000c, ch_en);  // 12bit ch0 enable
        wr(0x005a0004, 0x1);    // adc_phy_enadc: normal
        wr(0x005a0010, 0x1);    // adc_op_single_start: to start single convertion
        while (!(event & 0x8000)) { // why ?
            event = rd(0x005a0020);
        }
        wr(0x005a0004, 0x0);    // adc_phy_enadc: standby
        result += event & 0xFFF;
    }
    wr(0x005a0004, 0x10);       // adc_phy_reset

    result /= sample_times;
    result *= scale;
    result /= 1000000000;

    return result;
}
