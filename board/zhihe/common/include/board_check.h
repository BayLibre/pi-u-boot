/*
 * Copyright(C) 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#ifndef _BOARD_CHECK_
#define _BOARD_CHECK_

#define MAX_DTB_FILENAME_LEN 64

enum board_type {
    /* A200 */
    BOARD_A200_EVB,
    BOARD_A200_DEV,
    BOARD_TH1520,

    /* A210 */
    BOARD_A210_EVB,
    BOARD_A210_DEV,
    BOARD_A210_D2D,

    BOARD_UNKNOWN,
};

enum ddr_type {
    /* Auto size check */
    DDR_LP4X_3200_1Rank, /* a200-evb */
    DDR_LP4X_3200_2Rank,
    DDR_LP4X_3733_1Rank,  
    DDR_LP4X_3733_2Rank, /* p1 */
    DDR_LP4X_4266_1Rank,
    DDR_LP4X_4266_2Rank,

    /* Fix size */
    DDR_LP4X_4266_1Rank_2GBx2, /* a210-evb */
    DDR_LP4X_4266_1Rank_4GBx2, /* a210-evb */
    DDR_LP4X_4266_2Rank_8GBx2, /* a210-evb */
    DDR_LP4X_3733_2Rank_8GBx2, /* a210-evb */

    DDR_TYPE_UNKNOWN,
};

/* board check */
int spl_set_board_info(enum board_type typeboard, enum ddr_type typeddr);
enum board_type spl_get_board_type(void);
enum ddr_type spl_get_ddr_type(void);

/* board info convert */
const char *uboot_get_binfo_from_fdt(void *fdt_uboot);
const char *uboot_sync_fdt_binfo_to_env(void *fdt_uboot);
int spl_set_binfo_to_uboot_fdt(void *fdt_uboot);
const char *spl_multi_fit_check(const char *suffix);

#endif
