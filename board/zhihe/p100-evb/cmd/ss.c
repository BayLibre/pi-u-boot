// SPDX-License-Identifier: GPL-2.0+
/*
 * Command for Sub-system status control.
 *
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <command.h>
#include "../subsys/subsys.h"
#include "ss.h"

/* Enabled PERI and USB by default. */
static unsigned int ss_cfg = SS_CFG_DEFAULT;

const char *ss_names[MAX_CPR] = {
    "VP",
    "VI",
    "NPU",
    "VO",
    "PERI",
    "PCIE_SATA",
    "USB",
    "TEE",
    "GPU",
    "D2D",
    "D2D_CPU",
};

static int ss_dump_status(unsigned int cfg)
{
	unsigned int i = 0;

	for (i = 0; i < MAX_CPR; i++) {
		if (cfg & (1 << i)) {
			printf("%10s enabled\n", ss_names[i]);
		} else {
			printf("%10s disabled\n", ss_names[i]);
		}
	}

	return 0;
}

static int do_ss_dump(int argc, char *const argv[])
{
	return ss_dump_status(ss_cfg);
}

static int do_ss_control(int argc, char *const argv[])
{
	unsigned int i = 0;

	if (argc < 2)
		return CMD_RET_USAGE;

	for (i = 0; i < MAX_CPR; i++) {
		if (strncmp(argv[1], ss_names[i], strlen(ss_names[i])) == 0) {
			if (strncmp(argv[0], "set", 3) == 0) {
				ss_cfg |= (1 << i);
			} else if (strncmp(argv[0], "unset", 5) == 0) {
				ss_cfg &= ~(1 << i);
			} else {
				return CMD_RET_USAGE;
			}
			break;
		}
	}

	if (i == MAX_CPR) {
		printf("Invalid subsystem name: %s\n", argv[1]);
		return CMD_RET_USAGE;
	}

	return 0;
}

static int do_ss_enable(int argc, char *const argv[])
{
	ss_cpr_init(ss_cfg);
	ss_dump_status(ss_cfg);

	return 0;
}

static int do_ss(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	const char *cmd;
	int ret;
	/* need at least two arguments */
	if (argc < 2)
		return CMD_RET_USAGE;

	cmd = argv[1];
	--argc;
	++argv;

	if (strcmp(cmd, "set") == 0 || strcmp(cmd, "unset") == 0)
		ret = do_ss_control(argc, argv);
	else if (strcmp(cmd, "enable") == 0)
		ret = do_ss_enable(argc, argv);
	else if (strcmp(cmd, "status") == 0)
		ret = do_ss_dump(argc, argv);
	else
		ret = CMD_RET_USAGE;

	return ret;
}

U_BOOT_LONGHELP(ss,
	"status                  - dump current subsystem status flags.\n"
	"ss set ss_name             - set the specified subsystem to enable flag.\n"
	"ss unset ss_name           - unset the specified subsystem to disable flag.\n"
	"ss enable                  - enable all subsystems using the current status flags.\n"
	"\nss_name:\n"
	"  VP, VI, NPU, VO, PERI, PCIE_SATA, USB, TEE, GPU, D2D, D2D_CPU\n"
	"\neg:\n"
	"  ss set ss_name0 + ss set ss_name1 + ss enable\n"
	);

U_BOOT_CMD(
	ss,	3,	1,	do_ss,
	"Sub system status control", ss_help_text
);
