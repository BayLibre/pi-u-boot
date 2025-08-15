// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2024-2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <cpu_func.h>
#include <command.h>
#include <asm/io.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <exports.h>
#include <serial.h>
#include <fdt_support.h>

#include "include/addr_defines.h"
#include "include/board.h"
#include "../common/include/spl_fit.h"
#include "configs/p100-evb.h"
#include "rambus/soc_parameter.h"

/*
 * static functions
 */
static void clk_init(void)
{
#if (IS_ENABLED(CONFIG_CLK_ZHIHE_P100))
	struct udevice *dev;
	uclass_get_device_by_driver(UCLASS_CLK,
					DM_DRIVER_GET(zhihe_p100_clk),
					&dev);
#endif
}

#if IS_ENABLED(CONFIG_FASTBOOT)
static void fastboot_check(void)
{
	int boot_sel = board_get_boot_sel();

	printf("Boot sel=%d\n", boot_sel);
	if (boot_sel != BOOT_SEL_FASTBOOT &&
		boot_sel != BOOT_SEL_MCM_FASTBOOT) {
		return;
	}

	run_command("env default -fa;env save", 0);
	run_command("echo fastboot check success", 0);
	run_command("fastboot usb 0", 0);
}
#endif

/*
 * U-Boot Board init hooks
 */
int board_init(void)
{
	clk_init();
#ifdef CONFIG_ZHIHE_RAMBUS_ALGO
	/* libsecurity.a.bin soc parameter init */
	csi_soc_parmeter_t soc_parameter = {
		EFUSE_LC_RMA_UPDATE_OFFSET,
		TEE_SYS_EFUSE_DBG_KEY1_OFF,
		TEE_SYS_BASE,
		EFUSE_BASE,
		EIP150B_BASE,
		EIP28_BASE,
		EIP120SI_BASE,
		EIP120SII_BASE,
		EIP120SIII_BASE,
		KEYRAM_BASE,
	};
	csi_init_soc_parameter(&soc_parameter);
#endif
	gpio_pin_init();
	return 0;
}

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
#if IS_ENABLED(CONFIG_FASTBOOT)
	/* If it is in fastboot mode, the function does not return */
	fastboot_check();
#endif

	return 0;
}
#endif

#ifdef CONFIG_BOARD_EARLY_INIT_F
int board_early_init_f(void)
{
	return 0;
}
#endif

/*
 * The function is called in the u-boot/boot/image-fdt.c:image_setup_libfdt function.
 * Config kernel dtb memory node
 */
int ft_board_setup(void *blob, struct bd_info *bd)
{
	return fdt_fixup_memory(blob, (u64)CFG_SYS_SDRAM_BASE, (u64)gd->ram_size);
}

/*
 * Board cmds
 */
 /* Boot AON */
static int do_boot_aon(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	/* set e902 in the reset state */
	writel(E902_RSTGEN_RSTCFG_VAL_RESET, (void *)E902_RSTGEN_RSTCFG_ADDR);

	/* set e902 entry address */
	writel(E902_SYSREG_RST_VAL_ENTRY, (void *)E902_SYSREG_RST_ADDR);

	/* flush cache */
	flush_cache((uintptr_t)E902_SYSREG_RST_VAL_ENTRY, E902_MAX_TEXT_SIZE);

	/* set e902 in the de-reset state */
	writel(E902_RSTGEN_RSTCFG_VAL_DERESET, (void *)E902_RSTGEN_RSTCFG_ADDR);

	return 0;
}

U_BOOT_CMD(boot_aon, CONFIG_SYS_MAXARGS, 0, do_boot_aon, "Boot aon e902 core", "");

