/*
 * Copyright (C) 2024-2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef _A200_DDR_H_
#define _A200_DDR_H_

#include "ddr_common_func.h"

struct ddr_config {
    enum DDR_PINMUX pinmux;
    enum DDR_TYPE type;
    int rank_num;
    int freq;
};

int init_ddr(struct ddr_config *ddrcfg);
int fixup_ddr_addrmap(unsigned long size);
int query_ddr_boundary(unsigned long size);
unsigned long get_ddr_density(void);
int pmic_reset_apcpu_voltage(void);

#endif
