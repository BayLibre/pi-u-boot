// SPDX-License-Identifier: GPL-2.0+
/*
 * Command for Sub-system status control.
 *
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#include <command.h>
#include "../include/addr_defines.h"
#include "../subsys/subsys.h"
#include "../include/utils/utils.h"

#define IOPMP_DATA_MAX 10
#define IOPMP_CFG_COUNT_MAX 64
struct iopmp_data_t {
	unsigned char name[32];
	uint64_t base_addr;
};

struct iopmp_entry_t {
	uint8_t permission : 3;
	uint8_t mode : 2;
	uint8_t reserved : 2;
	uint8_t lock : 1;
}__attribute__((packed));

const struct iopmp_data_t iopmp_datas[IOPMP_DATA_MAX] = {
	{"VP", AP_VP_DFMU_IOPMP_BADDR},
	{"VI", AP_VI_DFMU_IOPMP_BADDR},
	{"NPU", AP_NPU_DFMU_IOPMP_BADDR},
	{"VO", AP_VO_DFMU_IOPMP_BADDR},
	{"PERI1", AP_PERI1_DFMU_IOPMP_BADDR},
	{"PCIE", AP_PCIE_DFMU_IOPMP_BADDR},
	{"USB", AP_USB_DFMU_IOPMP_BADDR},
	{"GPU", AP_GPU_DFMU_IOPMP_BADDR},
	{"D2D", AP_D2D_RX_DFMU_IOPMP_BADDR},
	{"D2D_CPU", AON_CPU_SS_IOPMP_BADDR},
};

static void dump_configuration(int index)
{
	uint32_t pmpcfg;
	uint32_t high_pmpaddr0, high_pmpaddr1, high_pmpaddr2, high_pmpaddr3;
	for (int count = 0; count < IOPMP_CFG_COUNT_MAX; count++) {
		pmpcfg = chip_rd(iopmp_datas[index].base_addr + count * 4);
		high_pmpaddr0 = chip_rd(iopmp_datas[index].base_addr +  + 0x804 + count * 0x20);
		high_pmpaddr1 = chip_rd(iopmp_datas[index].base_addr +  + 0x80c + count * 0x20);
		high_pmpaddr2 = chip_rd(iopmp_datas[index].base_addr +  + 0x814 + count * 0x20);
		high_pmpaddr3 = chip_rd(iopmp_datas[index].base_addr +  + 0x81c + count * 0x20);
		//printf("0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n", pmpcfg, high_pmpaddr0, high_pmpaddr1, high_pmpaddr2, high_pmpaddr3);
		if (pmpcfg != 0) {
			struct iopmp_entry_t entry0, entry1, entry2, entry3;
			uint8_t dev0, dev1, dev2, dev3;
			uint8_t entry;

			entry = (uint8_t)(pmpcfg & 0x000000ff);
			memcpy(&entry0, &entry, sizeof(entry0));
			entry = (uint8_t)((pmpcfg & 0x0000ff00) >> 8);
			memcpy(&entry1, &entry, sizeof(entry1));
			entry = (uint8_t)((pmpcfg & 0x00ff0000) >> 16);
			memcpy(&entry2, &entry, sizeof(entry2));
			entry = (uint8_t)((pmpcfg & 0xff000000) >> 24);
			memcpy(&entry3, &entry, sizeof(entry3));

			dev0 = (uint8_t)((high_pmpaddr0 & 0x003fc000) >> 14);
			dev1 = (uint8_t)((high_pmpaddr1 & 0x003fc000) >> 14);
			dev2 = (uint8_t)((high_pmpaddr2 & 0x003fc000) >> 14);
			dev3 = (uint8_t)((high_pmpaddr3 & 0x003fc000) >> 14);


			printf("----------pmpcfg-%d: 0x%x----------\n", count, pmpcfg);
			printf("|	entry 0	|	entry 1	|   entry  2	|	entry 3	| \n");
			printf("|  mode:%d p:0x%x |  mode:%d p:0x%x |  mode:%d p:0x%x |  mode:%d p:0x%x | \n ",
				entry0.mode, entry0.permission, entry1.mode, entry1.permission,
				entry2.mode, entry2.permission, entry3.mode, entry3.permission);
			printf("|  dev:  0x%x   |   dev:  0x%x   |  dev:  0x%x	|  dev:  0x%x	| \n",
				dev0, dev1, dev2, dev3);
			printf("----------end----------\n");
		}
		else
			return;
	}
}

static int dump_iopmp(const char* sub_sysname)
{
	int index;
	if (sub_sysname == NULL)
		return CMD_RET_USAGE;

	for (index = 0; index < IOPMP_DATA_MAX; index++) {
		if (strncmp(sub_sysname, iopmp_datas[index].name, strlen(iopmp_datas[index].name)) == 0) {
			printf("%s iopmp configs: \n", sub_sysname);
			dump_configuration(index);
			break;
		}
	}

	if (index == IOPMP_DATA_MAX) {
		printf("Invalid subsystem name: %s\n", sub_sysname);
		return CMD_RET_USAGE;
	}

	return 0;
}

static int do_iopmp_dump(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	const char *name;
	/* need at least two arguments */
	if (argc < 2)
		return CMD_RET_USAGE;

	name = argv[1];

	dump_iopmp(name);

	return 0;
}

U_BOOT_LONGHELP(iopmp,
	"sub_sysname \n"
	"sub_sysname:\n"
	"  VP, VI, NPU, VO, PERI1, PCIE, USB, GPU, D2D, D2D_CPU\n"
	"\neg:\n"
	"  iopmp_dump PERI\n"
	);

U_BOOT_CMD(
	iopmp_dump,	3,	1,	do_iopmp_dump,
	"dump subsystem iopmp configuration", iopmp_help_text
);
