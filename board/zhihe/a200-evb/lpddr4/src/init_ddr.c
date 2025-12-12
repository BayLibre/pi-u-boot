#include "../include/common_lib.h"
#include "../include/ddr_init.h"
#include "../include/lpddr4_init.h"

/*
Attention:
The following variable must not be initialized to zero.
This global variable is assigned in the 'f' stage and
must persist into the 'r' stage of the SPL.
If it is initialized to zero and becomes a BSS variable,
it will be re-zeroed upon entering the 'r' stage, causing data loss.
*/
static struct ddr_config _ddr_cfg = {
    DDR_PINMUX_MAX,
    DDR_TYPE_MAX,
    1,
    3733,
};

int init_ddr(struct ddr_config *ddrcfg)
{
    bool dbi_off = false;
    enum DDR_BITWIDTH bits = DDR_BITWIDTH_64;

#ifdef CONFIG_DDR_MSG
    printf("enter init_ddr\n");
#endif

#ifdef CONFIG_DDR_H32_MODE
    bits = DDR_BITWIDTH_32;
#elif CONFIG_DDR_H16_MODE
    bits = DDR_BITWIDTH_16;
#else
    bits = DDR_BITWIDTH_64;
#endif

    if (ddrcfg->rank_num != 1 && ddrcfg->rank_num != 2) {
        printf("unsupport ddr rank_num config!!!\n");
        return -1;
    }

    if (ddrcfg->freq != 4266 && ddrcfg->freq != 3733 && ddrcfg->freq != 3200 && ddrcfg->freq != 2133) {
        printf("unsupport ddr freq config!!!\n");
        return -1;
    }

    _ddr_cfg = *ddrcfg;

#ifdef CONFIG_DDR_DBI_OFF
    dbi_off = true;
#endif

    printf("DDR info: lpddr4%c %s freq=%d %dbits dbi_off=%c\n",
            (_ddr_cfg.type == DDR_TYPE_LPDDR4X?'x':' '),
            (_ddr_cfg.rank_num == 1 ? "singlerank" : "dualrank"),
            _ddr_cfg.freq, bits,
            (dbi_off ? 'y' : 'n'));

#ifdef CONFIG_LPDDR_EYE
    printf("lpddr diag eye test\n");
    lp4_diag_eye(_ddr_cfg.type, _ddr_cfg.rank_num, _ddr_cfg.freq, bits);
#else
    lpddr4_init(_ddr_cfg.type, _ddr_cfg.rank_num, _ddr_cfg.freq, bits);
#endif

#ifdef CONFIG_DDR_MSG
    printf("exit init_ddr\n");
#endif
    return 0;
}

enum DDR_PINMUX get_ddr_pinmux(void)
{
    return _ddr_cfg.pinmux;
}

enum DDR_TYPE get_ddr_type() {
    return _ddr_cfg.type;
}

int get_ddr_rank_number() {
    return _ddr_cfg.rank_num;
}

int get_ddr_freq() {
    return _ddr_cfg.freq;
}

enum DDR_BITWIDTH get_ddr_bitwidth() {
#ifdef CONFIG_DDR_H32_MODE
    return DDR_BITWIDTH_32;
#elif CONFIG_DDR_H16_MODE
    return DDR_BITWIDTH_16;
#else
    return DDR_BITWIDTH_64;
#endif
}
