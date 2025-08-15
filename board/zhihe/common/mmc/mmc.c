#include <command.h>
#include <display_options.h>
#include <mmc.h>
#include <vsprintf.h>
#include <stdlib.h>
#include <memalign.h>

static int curr_device = -1;

extern volatile uint32_t DELAY_LANE;
extern volatile int manual_set_delay;

extern unsigned int zhihe_fixed_mmc_caps;
extern unsigned int zhihe_fixed_sd_caps;

extern int snps_sdhci_init(struct mmc *mmc);

static void print_mmcinfo(struct mmc *mmc)
{
	int i;

	printf("Device: %s\n", mmc->cfg->name);
	printf("Manufacturer ID: %x\n", mmc->cid[0] >> 24);
	if (IS_SD(mmc)) {
		printf("OEM: %x\n", (mmc->cid[0] >> 8) & 0xffff);
		printf("Name: %c%c%c%c%c \n", mmc->cid[0] & 0xff,
		(mmc->cid[1] >> 24), (mmc->cid[1] >> 16) & 0xff,
		(mmc->cid[1] >> 8) & 0xff, mmc->cid[1] & 0xff);
	} else {
		printf("OEM: %x\n", (mmc->cid[0] >> 8) & 0xff);
		printf("Name: %c%c%c%c%c%c \n", mmc->cid[0] & 0xff,
		(mmc->cid[1] >> 24), (mmc->cid[1] >> 16) & 0xff,
		(mmc->cid[1] >> 8) & 0xff, mmc->cid[1] & 0xff,
		(mmc->cid[2] >> 24));
	}

	printf("Bus Speed: %d\n", mmc->clock);
#if CONFIG_IS_ENABLED(MMC_VERBOSE)
	printf("Mode: %s\n", mmc_mode_name(mmc->selected_mode));
	mmc_dump_capabilities("card capabilities", mmc->card_caps);
	mmc_dump_capabilities("host capabilities", mmc->host_caps);
#endif
	printf("Rd Block Len: %d\n", mmc->read_bl_len);

	printf("%s version %d.%d", IS_SD(mmc) ? "SD" : "MMC",
			EXTRACT_SDMMC_MAJOR_VERSION(mmc->version),
			EXTRACT_SDMMC_MINOR_VERSION(mmc->version));
	if (EXTRACT_SDMMC_CHANGE_VERSION(mmc->version) != 0)
		printf(".%d", EXTRACT_SDMMC_CHANGE_VERSION(mmc->version));
	printf("\n");

	printf("High Capacity: %s\n", mmc->high_capacity ? "Yes" : "No");
	puts("Capacity: ");
	print_size(mmc->capacity, "\n");

	printf("Bus Width: %d-bit%s\n", mmc->bus_width,
			mmc->ddr_mode ? " DDR" : "");

#if CONFIG_IS_ENABLED(MMC_WRITE)
	puts("Erase Group Size: ");
	print_size(((u64)mmc->erase_grp_size) << 9, "\n");
#endif

	if (!IS_SD(mmc) && mmc->version >= MMC_VERSION_4_41) {
		bool has_enh = (mmc->part_support & ENHNCD_SUPPORT) != 0;
		bool usr_enh = has_enh && (mmc->part_attr & EXT_CSD_ENH_USR);
		ALLOC_CACHE_ALIGN_BUFFER(u8, ext_csd, MMC_MAX_BLOCK_LEN);
		u8 wp;
		int ret;

#if CONFIG_IS_ENABLED(MMC_HW_PARTITIONING)
		puts("HC WP Group Size: ");
		print_size(((u64)mmc->hc_wp_grp_size) << 9, "\n");
#endif

		puts("User Capacity: ");
		print_size(mmc->capacity_user, usr_enh ? " ENH" : "");
		if (mmc->wr_rel_set & EXT_CSD_WR_DATA_REL_USR)
			puts(" WRREL\n");
		else
			putc('\n');
#ifndef CONFIG_SPL_BUILD
		if (usr_enh) {
			puts("User Enhanced Start: ");
			print_size(mmc->enh_user_start, "\n");
			puts("User Enhanced Size: ");
			print_size(mmc->enh_user_size, "\n");
		}
#endif
		puts("Boot Capacity: ");
		print_size(mmc->capacity_boot, has_enh ? " ENH\n" : "\n");
		puts("RPMB Capacity: ");
		print_size(mmc->capacity_rpmb, has_enh ? " ENH\n" : "\n");

		for (i = 0; i < ARRAY_SIZE(mmc->capacity_gp); i++) {
			bool is_enh = has_enh &&
				(mmc->part_attr & EXT_CSD_ENH_GP(i));
			if (mmc->capacity_gp[i]) {
				printf("GP%i Capacity: ", i+1);
				print_size(mmc->capacity_gp[i],
					   is_enh ? " ENH" : "");
				if (mmc->wr_rel_set & EXT_CSD_WR_DATA_REL_GP(i))
					puts(" WRREL\n");
				else
					putc('\n');
			}
		}
		ret = mmc_send_ext_csd(mmc, ext_csd);
		if (ret)
			return;
		wp = ext_csd[EXT_CSD_BOOT_WP_STATUS];
		for (i = 0; i < 2; ++i) {
			printf("Boot area %d is ", i);
			switch (wp & 3) {
			case 0:
				printf("not write protected\n");
				break;
			case 1:
				printf("power on protected\n");
				break;
			case 2:
				printf("permanently protected\n");
				break;
			default:
				printf("in reserved protection state\n");
				break;
			}
			wp >>= 2;
		}
	}
}

static uint mmc_mode2freq(struct mmc *mmc, enum bus_mode mode)
{
	static const int freqs[] = {
	      [MMC_LEGACY]	= 25000000,
	      [MMC_HS]		= 26000000,
	      [SD_HS]		= 50000000,
	      [MMC_HS_52]	= 52000000,
	      [MMC_DDR_52]	= 52000000,
	      [UHS_SDR12]	= 25000000,
	      [UHS_SDR25]	= 50000000,
	      [UHS_SDR50]	= 100000000,
	      [UHS_DDR50]	= 50000000,
	      [UHS_SDR104]	= 208000000,
	      [MMC_HS_200]	= 200000000,
	      [MMC_HS_400]	= 200000000,
	      [MMC_HS_400_ES]	= 200000000,
	};

	if (mode == MMC_LEGACY)
		return mmc->legacy_speed;
	else if (mode >= MMC_MODES_END)
		return 0;
	else
		return freqs[mode];
}

int snps_mmc_init(struct mmc *mmc)
{
    return snps_sdhci_init(mmc);
}

static int mmc_dev_init(void)
{
	if (curr_device < 0) {
		if (get_mmc_num() > 0) {
			curr_device = 0;
		} else {
			printf("No MMC device available\n");
			return CMD_RET_FAILURE;
		}
	}
	return CMD_RET_SUCCESS;
}

static struct mmc *__init_mmc_device(int dev, bool force_init,
				     enum bus_mode speed_mode)
{
	struct mmc *mmc;
	mmc = find_mmc_device(dev);
	if (!mmc) {
		printf("No MMC device at slot %x\n", dev);
		return NULL;
	}

	if (!mmc_getcd(mmc))
		force_init = true;

	if (force_init)
		mmc->has_init = 0;

	if (IS_ENABLED(CONFIG_MMC_SPEED_MODE_SET))
		mmc->user_speed_mode = speed_mode;

	if (mmc_init(mmc))
		return NULL;

#ifdef CONFIG_BLOCK_CACHE
	struct blk_desc *bd = mmc_get_blk_desc(mmc);
	blkcache_invalidate(bd->uclass_id, bd->devnum);
#endif

	return mmc;
}

static struct mmc *init_mmc_device(int dev, bool force_init)
{
	return __init_mmc_device(dev, force_init, MMC_MODES_END);
}

static int do_mmc_set_delay_lane(struct cmd_tbl *cmdtp, int flag,
	       int argc, char * const argv[])
{
	struct mmc *mmc;
	unsigned int val;

	if (argc != 2)
		return CMD_RET_USAGE;
	
	if (mmc_dev_init() != 0)
		return CMD_RET_FAILURE;

	val = dectoul(argv[1], NULL);
	DELAY_LANE = val;
	printf("Set DELAY_LANE:%d\n", DELAY_LANE);

	mmc = find_mmc_device(curr_device);
	if (!mmc) {
		printf("No MMC device at slot %x\n", curr_device);
		goto RET_FAILURE;
	}
	manual_set_delay = 1;
	if (0 != snps_mmc_init(mmc))
		goto RET_FAILURE;

	mmc = init_mmc_device(curr_device, true);
	if (!mmc)
		goto RET_FAILURE;

	manual_set_delay = 0;
	return CMD_RET_SUCCESS;

RET_FAILURE:
	manual_set_delay = 0;
	return CMD_RET_FAILURE;
}

static int do_mmc_set_clk_freq(struct cmd_tbl *cmdtp, int flag,
		       int argc, char * const argv[])
{
	struct mmc *mmc;
	unsigned int freq;
	int ret = CMD_RET_SUCCESS;

	if (argc != 2)
		return CMD_RET_USAGE;

	if (mmc_dev_init() != 0)
		return CMD_RET_FAILURE;

	freq = dectoul(argv[1], NULL);
	mmc = init_mmc_device(curr_device, false);
	if (!mmc)
		return CMD_RET_FAILURE;

	printf("Set freq:%u ... ", freq);

	if (mmc_set_clock(mmc, freq, false) != 0) {
		ret = CMD_RET_FAILURE;
	}

	printf("%s\n", (ret == CMD_RET_SUCCESS) ? "OK" : "ERROR");

	return ret;
}

static int do_mmc_turning(struct cmd_tbl *cmdtp, int flag,
	       int argc, char * const argv[])
{
	struct mmc *mmc;
	int i = 0, n;
	int stop_on_ok = 1;

	if(argc > 1 && (!strncmp(argv[1],"cont",4))){
		stop_on_ok = 0;
	}

	if (mmc_dev_init() != 0)
		return CMD_RET_FAILURE;

	for(i = 0; i < 128; i++) {
		DELAY_LANE = i;
		printf("Set DELAY_LANE = %d\n", DELAY_LANE);

		mmc = find_mmc_device(curr_device);

		if (!mmc) {
			printf("No MMC device at slot %x\n", curr_device);
			return CMD_RET_FAILURE;
		}

		manual_set_delay = 1;
		if (0 != snps_mmc_init(mmc)) {
			printf("Error: MMC init error!\n");
			manual_set_delay = 0;
			return CMD_RET_FAILURE;
		}

		mmc = init_mmc_device(curr_device, true);
		if (!mmc) {
			continue;
		}

		if (mmc_getwp(mmc) == 1) {
			printf("Error: card is write protected!\n");
			manual_set_delay = 0;
			return CMD_RET_FAILURE;
		}

		n = blk_dwrite(mmc_get_blk_desc(mmc), 0, 1, 0);
		if (n == 1) {
			printf("Turning blocks written: %s\n", "OK" );
			manual_set_delay = 0;
			if(stop_on_ok)
				return CMD_RET_SUCCESS;
		} else {
			printf("Turning blocks written: %s\n", "ERROR");
		}
	}

	manual_set_delay = 0;
	if (i >= 128) {
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int do_mmc_set_mode(struct cmd_tbl *cmdtp, int flag,
		       int argc, char * const argv[])
{
	struct mmc *mmc;
	unsigned int mode, width, mmc_caps;
	int ret = CMD_RET_SUCCESS;

	if (argc != 3)
		return CMD_RET_USAGE;

	if (mmc_dev_init() != 0)
		return CMD_RET_FAILURE;

	mode = dectoul(argv[1], NULL);
	if (mode >= MMC_MODES_END) {
		return CMD_RET_FAILURE;
	}
	width = dectoul(argv[2], NULL);
	mmc = find_mmc_device(curr_device);
	if (!mmc) {
		printf("No MMC device at slot %x\n", curr_device);
		return CMD_RET_FAILURE;
	}
	mmc->bus_width = width;

	printf("MMC set mode %s width %d (at %d MHz) ... ",
			mmc_mode_name(mode),
			width,
			mmc_mode2freq(mmc, mode) / 1000000);

	switch(mmc->bus_width) {
		case 1:
			mmc_caps |= MMC_MODE_1BIT;
			break;
		case 4:
			mmc_caps |= MMC_MODE_4BIT;
			break;
		case 8:
			mmc_caps |= MMC_MODE_8BIT;
			break;
		default:
			return CMD_RET_USAGE;
	}
	mmc_caps |= MMC_CAP(mode);

	if (IS_SD(mmc)) {
		zhihe_fixed_sd_caps = mmc_caps;
	} else {
		zhihe_fixed_mmc_caps = mmc_caps;
	}

	if (0 != snps_mmc_init(mmc)) {
		ret = CMD_RET_FAILURE;
		goto out;
	}

	mmc = init_mmc_device(curr_device, true);
	if (!mmc)
		ret = CMD_RET_FAILURE;
out:
	if (IS_SD(mmc)) {
		zhihe_fixed_sd_caps = 0;
	} else {
		zhihe_fixed_mmc_caps = 0;
	}

	printf("%s\n", (ret == CMD_RET_SUCCESS) ? "OK" : "ERROR");

	return ret;
}

static int do_mmc_dev(struct cmd_tbl *cmdtp, int flag,
		      int argc, char *const argv[])
{
	int dev;
	struct mmc *mmc;

	if (argc == 1) {
		dev = curr_device;
	} else if (argc == 2) {
		dev = (int)dectoul(argv[1], NULL);
	} else {
		return CMD_RET_USAGE;
	}

	mmc = init_mmc_device(dev, true);
	if (!mmc)
		return CMD_RET_FAILURE;

	curr_device = dev;
	printf("mmc%d is current device\n", curr_device);

	return CMD_RET_SUCCESS;
}

static int do_mmcinfo(struct cmd_tbl *cmdtp, int flag, int argc,
		      char *const argv[])
{
	struct mmc *mmc;

	if (curr_device < 0) {
		if (get_mmc_num() > 0)
			curr_device = 0;
		else {
			puts("No MMC device available\n");
			return CMD_RET_FAILURE;
		}
	}

	mmc = init_mmc_device(curr_device, false);
	if (!mmc)
		return CMD_RET_FAILURE;

	print_mmcinfo(mmc);
	return CMD_RET_SUCCESS;
}

static struct cmd_tbl cmd_mmc[] = {
	U_BOOT_CMD_MKENT(info, 1, 0, do_mmcinfo, "", ""),
	U_BOOT_CMD_MKENT(dev, 4, 0, do_mmc_dev, "", ""),
	U_BOOT_CMD_MKENT(set_clk, 4, 1, do_mmc_set_clk_freq, "", ""),
	U_BOOT_CMD_MKENT(set_delay, 4, 1, do_mmc_set_delay_lane, "", ""),
	U_BOOT_CMD_MKENT(turning, 4, 1, do_mmc_turning, "", ""),
	U_BOOT_CMD_MKENT(set_mode, 4, 1, do_mmc_set_mode, "", ""),
};

static int do_mmcops(struct cmd_tbl *cmdtp, int flag, int argc,
		     char *const argv[])
{
	struct cmd_tbl *cp;

	cp = find_cmd_tbl(argv[1], cmd_mmc, ARRAY_SIZE(cmd_mmc));

	/* Drop the mmc command */
	argc--;
	argv++;

	if (cp == NULL || argc > cp->maxargs)
		return CMD_RET_USAGE;
	if (flag == CMD_FLAG_REPEAT && !cmd_is_repeatable(cp))
		return CMD_RET_SUCCESS;

	if (curr_device < 0) {
		if (get_mmc_num() > 0) {
			curr_device = 0;
		} else {
			printf("No MMC device available\n");
			return CMD_RET_FAILURE;
		}
	}
	return cp->cmd(cmdtp, flag, argc, argv);
}

U_BOOT_CMD(
	mmcz, 29, 1, do_mmcops,
	"MMC sub system",
	"info - display info of the current MMC device\n"
	"mmcz dev [dev] - show or set current mmc device\n"
	"mmcz set_clk # freq - set mmc clock frequency\n"
	"mmcz set_delay # val - set clk out delay mannaul,reinit host and rescan dev\n"
	"mmcz turning [continue] - loop test for clk delay form 0 to 128, reinit host and rescan dev\n"
	"	- without arg [continue] exit once init and write ok\n"
	"mmcz set_mode speed_mode bus_width - set mmc speed mode and bus_width\n"
	"  speed_mode:\n"
    "    - 0: MMC_LEGACY(emmc supported),\n"
    "    - 1: MMC_HS(emmc supported),\n"
    "    - 2: SD_HS(sd supported),\n"
    "    - 3: MMC_HS_52 (emmc supported),\n"
    "    - 4: MMC_DDR_52(not supported),\n"
    "    - 5: UHS_SDR12(sd supported),\n"
    "    - 6: UHS_SDR25(sd supported),\n"
    "    - 7: UHS_SDR50(sd supported),\n"
    "    - 8: UHS_DDR50(not supported),\n"
    "    - 9: UHS_SDR104(sd supported),\n"
    "    - 10: MMC_HS_200(emmc supported),\n"
    "    - 11: MMC_HS_400(emmc supported),\n"
    "    - 12: MMC_HS_400_ES(not supported),\n"
	"  bus_width:\n"
	"    - 1: bus_width 1\n"
	"    - 4: bus_width 4\n"
	"    - 8: bus_width 8\n"
	);
