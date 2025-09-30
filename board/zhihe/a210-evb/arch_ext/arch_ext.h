/*
 * Copyright(C) 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <linux/types.h>

#ifndef _ARCH_EXT_H_
#define _ARCH_EXT_H_

#define ATT_BRAM_DATA __attribute__((section(".bram.data")))
#define ATT_BRAM_TEXT __attribute__((section(".bram.text")))

void bram_entry(void *cb, ulong cb_param);

#endif
