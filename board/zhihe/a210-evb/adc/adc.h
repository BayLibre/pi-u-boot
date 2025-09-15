// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#ifndef __ZHIHE_ADC_H_
#define __ZHIHE_ADC_H_

#include <linux/types.h>

void adc_init(void);
u64 adc_read(u32 chid, u32 avr);

#endif