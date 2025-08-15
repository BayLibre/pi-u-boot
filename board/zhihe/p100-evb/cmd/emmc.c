#include <command.h>
#include "emmc_sw_tuning.h"

static int do_emmc_tuning(struct cmd_tbl *cmdtp, int flag, int argc,
    char *const argv[])
{
    emmc_read_tuning_seq();
    return 0;
}

U_BOOT_CMD(emmctuning, CONFIG_SYS_MAXARGS, 0, do_emmc_tuning, "emmc tuning", "");


static int do_sdio_tuning(struct cmd_tbl *cmdtp, int flag, int argc,
    char *const argv[])
{
    sdio_read_tuning_seq();
    return 0;
}

U_BOOT_CMD(sdiotuning, CONFIG_SYS_MAXARGS, 0, do_sdio_tuning, "sdio tuning", "");
