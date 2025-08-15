/*
* Copyright (C) 2025 Zhihe Group Holding Limited
*
* SPDX-License-Identifier: GPL-2.0+
*/

#include <command.h>
#include <vsprintf.h>
#include <console.h>
#include <malloc.h>
#include <linux/errno.h>
#include "rambus/csi_efuse_api.h"
#include "rambus/soc_parameter.h"

static int strtou32(const char *str, unsigned int base, u32 *result)
{
	char *ep;

	*result = simple_strtoul(str, &ep, base);
	if (ep == str || *ep != '\0')
		return -EINVAL;

	return 0;
}

static int confirm_prog(void)
{
	puts("Warning: Programming fuses is an irreversible operation!\n"
			"         This may brick your system.\n"
			"         Use this command only if you are sure of "
					"what you are doing!\n"
			"\nReally perform this fuse programming? <y/N>\n");

	if (confirm_yesno())
		return 1;

	puts("Fuse programming aborted\n");
	return 0;
}

static int do_fuse(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	const char *op = argc >= 2 ? argv[1] : NULL;
	int confirmed = argc >= 3 && !strcmp(argv[2], "-y");
	u32 addr, cnt, val;
	u8 *data;
	int ret, i;

	/* Initialize eFuse module */
	ret = csi_efuse_api_init();
	if (ret) {
		printf("efuse init faild[%d]\n", ret);
		goto err;
	}

	if (!strcmp(op, "read")) {
		argc -= 2 + confirmed;
		argv += 2 + confirmed;
	
		if (argc < 1 || strtou32(argv[0], 0, &addr))
			return CMD_RET_USAGE;

		if (argc == 1)
			cnt = 1;
		else if (argc != 2 || strtou32(argv[1], 0, &cnt))
			return CMD_RET_USAGE;

		printf("Reading addr %u:\n", addr);
		{
			data = malloc(cnt);
			ret = csi_efuse_read_raw(addr, data, cnt);
			if (ret) {
				free(data);
				goto err;
			}
			for (i = 0; i < cnt; i++)
				printf(" 0x%.2x", data[i]);
			free(data);
		}
		putc('\n');
	} else if (!strcmp(op, "write")) {
		argc -= 2 + confirmed;
		argv += 2 + confirmed;
	
		if (argc < 1 || strtou32(argv[0], 0, &addr))
			return CMD_RET_USAGE;

		if (argc < 2)
			return CMD_RET_USAGE;

		data = malloc(argc - 1);
		printf("Programming addr %u  to\n", addr);

		for (i = 1; i < argc; i++) {
			if (strtou32(argv[i], 16, &val))
				return CMD_RET_USAGE;

			data[i-1] = val;
			printf(" 0x%.2x\n", val);
		}

		cnt = argc - 1;

		if (!confirmed && !confirm_prog()) {
			free(data);
			return CMD_RET_FAILURE;
		}

		ret = csi_efuse_write_raw(addr, data, cnt);
		if (ret) {
			free(data);
			goto err;
		}
		free(data);
	} else if (!strcmp(op, "lc")) {
		int lc;
		ret = csi_efuse_get_lc(&lc);
		if (ret) {
			goto err;
		}

		switch(lc) {
			case 0:
				printf("life cycle: init\n");
				break;
			case 1:
				printf("life cycle: dev\n");
				break;
			case 2:
				printf("life cycle: oem\n");
				break;
			case 3:
				printf("life cycle: pro\n");
				break;
			case 4:
				printf("life cycle: rma\n");
				break;
			case 5:
				printf("life cycle: rip\n");
				break;
			default:
				printf("invalid life cycle\n");
				goto err;
		}
	} else {
		return CMD_RET_USAGE;
	}

	return 0;

err:
	puts("ERROR\n");
	return CMD_RET_FAILURE;
}

U_BOOT_CMD(
	efuse, CONFIG_SYS_MAXARGS, 0, do_fuse,
	"eFuse sub-system",
	"read <addr> [<cnt>] - read 1 or 'cnt' fuse bytes,\n"
	" starting at 'addr'\n"
	"efuse write [-y] <addr> <hexval> [<hexval>...]  - program 1 or\n"
	" several fuse bytes, starting at 'addr'\n"
	"efuse lc, get efuse life cycle\n"
);
