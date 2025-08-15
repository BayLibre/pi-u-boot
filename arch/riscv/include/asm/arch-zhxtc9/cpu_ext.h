/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2021 Alibaba Group Holding Limited
 */

#ifndef _ASM_CSR_H
#define _ASM_CSR_H

/* CSR extend define */
#define CSR_SMPEN        0x7f3
#define CSR_MCPUID       0xfc0
#define CSR_MCCR2        0x7c3
#define CSR_MHCR         0x7c1
#define CSR_MHINT        0x7c5
#define CSR_MHINT2       0x7cc
#define CSR_MHINT3       0x7cd
#define CSR_MHINT4       0x7ce
#define CSR_MXSTATUS     0x7c0
#define CSR_MSMPR        0x7f3

/* ASM extend define */
#define sync_is()   asm volatile (".long 0x01b0000b")
#define sync_i()   asm volatile (".long 0x01a0000b")

#endif
