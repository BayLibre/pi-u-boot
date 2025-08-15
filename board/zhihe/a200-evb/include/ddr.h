/*
 * Copyright (C) 2024-2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef _A200_DDR_H_
#define _A200_DDR_H_

void init_ddr(void);
int fixup_ddr_addrmap(unsigned long size);
int query_ddr_boundary(unsigned long size);
unsigned long get_ddr_density(void);
int pmic_reset_apcpu_voltage(void);

#endif
