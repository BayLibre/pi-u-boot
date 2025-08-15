/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */
#ifndef D2D_H
#define D2D_H

int d2d_ss_init(void);

#ifdef CONFIG_SOC_ZHIHE_D2D
/* TODO: replace by board_get_die_count() */
#define SOC_CHIP_NUM 2
#else
#define SOC_CHIP_NUM 1
#endif

#endif
