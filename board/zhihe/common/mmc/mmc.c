#include <command.h>
#include <display_options.h>
#include <mmc.h>
#include <vsprintf.h>
#include <stdlib.h>
#include <memalign.h>
#include <console.h>

static int curr_device = -1;

extern int zhihe_sdhci_set_delay(unsigned int mode, char delay);

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
	unsigned int mode;
	int delay;
	char cmd[64];

	if (argc != 3)
		return CMD_RET_USAGE;

	mode = dectoul(argv[1], NULL);
	delay = dectoul(argv[2], NULL);

	printf("Set tx delay: mode %d, delay %d\n", mode, delay);

	if (zhihe_sdhci_set_delay(mode, delay) == 0 ) {
		sprintf(cmd, "mmc rescan %d", mode);
		return run_command(cmd, 0);
	}

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

/*
 * Perform a single round of MMC write-read-compare test.
 *
 * @mem_start: base memory address for buffers
 * @start_blk: start block address on eMMC
 * @num_blks:  number of blocks to test (each block = 512 bytes)
 *
 * Write buffer: mem_start
 * Read buffer:  mem_start + num_blks * 512
 *
 * Returns: 0 on success, non-zero on failure
 */
static int mmc_rw_test_single(ulong mem_start, ulong start_blk, ulong num_blks)
{
	ulong byte_size = num_blks * 512ULL;
	ulong write_addr = mem_start;
	ulong read_addr = mem_start + byte_size;
	char cmd_buf[256];
	int ret;

	/* Step 1: Generate random data */
	snprintf(cmd_buf, sizeof(cmd_buf), "random 0x%lx 0x%lx", write_addr, byte_size);
	ret = run_command(cmd_buf, 0);
	if (ret != 0) {
		printf("random failed\n");
		return ret;
	}

	/* Step 2: Write to eMMC */
	snprintf(cmd_buf, sizeof(cmd_buf),
				"mmc write 0x%lx 0x%lx 0x%lx", write_addr, start_blk, num_blks);
	ret = run_command(cmd_buf, 0);
	if (ret != 0) {
		printf("mmc write failed\n");
		return ret;
	}

	/* Step 3: Read back */
	snprintf(cmd_buf, sizeof(cmd_buf),
				"mmc read 0x%lx 0x%lx 0x%lx", read_addr, start_blk, num_blks);
	ret = run_command(cmd_buf, 0);
	if (ret != 0) {
		printf("mmc read failed\n");
		return ret;
	}

	/* Step 4: Compare */
	#ifdef CMP_VALIDITY_CHECK
	// Optional debug corruption (e.g., for testing cmp failure)
	static int first_call = 1;
	if (first_call && num_blks > 0) {
		first_call = 0;
		printf("read_addr[0x5] = 0x%02x\n", *((uint8_t *)(read_addr + 0x5)));
		*((uint8_t *)(read_addr + 0x5)) = 0xA5;
		printf("read_addr[0x5] = 0x%02x (corrupted)\n", *((uint8_t *)(read_addr + 0x5)));
	}
	#endif

	snprintf(cmd_buf, sizeof(cmd_buf),
				"cmp.b 0x%lx 0x%lx 0x%lx", write_addr, read_addr, byte_size);
	ret = run_command(cmd_buf, 0);
	if (ret != 0) {
		printf("data compare mismatch\n");
		return ret;
	}

	return 0;
}

#define TEST_LBA (0x00081000) /* boot_b */
static char tx_delay_eye[128 + 1];
static int do_mmc_tuning(struct cmd_tbl *cmdtp, int flag,
	       int argc, char * const argv[])
{
	int mode;
	int i = 0, ret;
	int valid_start_delay = -1;
	int start_tx_delay = 0;
	int end_tx_delay = 128;
	int scan_detail = 0;
	char cmd_buf[64];
	char *temp_buf = NULL;

	if (argc < 4) {
		return CMD_RET_USAGE; 
	}

	if (curr_device < 0) {
		printf("run <mmcz dev 0|1> first\n");
		return CMD_RET_FAILURE;
	}

	mode = dectoul(argv[1], NULL);

	if (mode > MMC_HS_400_ES) {
		printf("Unknown mode %d\n", mode);
		return CMD_RET_FAILURE;
	}

	start_tx_delay = dectoul(argv[2], NULL);
	end_tx_delay = dectoul(argv[3], NULL);
	if (start_tx_delay > end_tx_delay || start_tx_delay < 0 || start_tx_delay >= 128
		|| end_tx_delay < 0 || end_tx_delay >= 128) {
		return CMD_RET_USAGE;
	}

	if (argc == 5) {
		scan_detail = 1;
	}

	temp_buf = (char *)env_get_hex("tmp_addr", 0);
	if (temp_buf == NULL) {
		printf("Unknown env tmp_addr\n");
		return CMD_RET_FAILURE;
	}

	printf("MMC txdelay scan:\n");
	printf("  devid %d, mode %d\n",  curr_device, mode);
	printf("  range (%d, %d)\n", start_tx_delay, end_tx_delay);
	printf("  temp buf 0x%p\n",temp_buf);

	memset(tx_delay_eye, '?', sizeof(tx_delay_eye));
	tx_delay_eye[sizeof(tx_delay_eye) - 1] = '\0';
	for(i = start_tx_delay; i <= end_tx_delay; i++) {
		printf("\n>>>Scan DELAY_LANE %d\n", i);

		if (ctrlc()) {
            return CMD_RET_FAILURE;
        }

		/* Set txdelay */
		if (zhihe_sdhci_set_delay(mode, i) == 0 ) {
			sprintf(cmd_buf, "mmc dev %d 0 %d", curr_device, mode);
			ret = run_command(cmd_buf, 0);
			if (ret != CMD_RET_SUCCESS) {
				printf("ERROR: Switch mode\n");
				if ((valid_start_delay >= 0) && (scan_detail == 0)) {
					printf(">>>Scan result: %d ~ %d\n", valid_start_delay, i - 1);
					return CMD_RET_FAILURE;
				} else {
					tx_delay_eye[i] = 'S';
					continue;
				}
			}
		}

		/* Write test */
		memset(temp_buf, 0xa5, 20 * 512);
		sprintf(cmd_buf, "mmc write %p %x %x", temp_buf, TEST_LBA + (i * 20), 20);
		ret = run_command(cmd_buf, 0);
		if (ret == CMD_RET_SUCCESS) {
			tx_delay_eye[i] = '-';
			if (valid_start_delay < 0) {
				valid_start_delay = i;
			}
		} else {
			tx_delay_eye[i] = 'X';
			if ((valid_start_delay >= 0) && (scan_detail == 0)) {
				printf(">>>Scan result: %d ~ %d\n", valid_start_delay, i - 1);
				return CMD_RET_FAILURE;
			}
		}
	}

	if (scan_detail) {
		printf(">>>Scan result: [%s]\n", tx_delay_eye);
	} else {
		printf(">>>Scan result: %d ~ %d\n", valid_start_delay, i - 1);
	}
	return CMD_RET_SUCCESS;
}

/*
 * Usage: mmcz rw_test <mem_start> <start_blk> <num_blks> [loop_count]
 */
static int do_mmc_rw_test(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	unsigned long start_blk = 0x8000;	// default start from block 0x8000
	unsigned long num_blks = 0x40000;	// default 128MB
	ulong mem_start = 0x82000000;
	ulong loop_count = 1;
	ulong byte_size;
	unsigned long i;
	int ret;

	if (argc < 4 || argc > 5) {
		printf("Usage: %s <mem_start> <start_blk> <num_blks> [loop_count]\n", argv[0]);
		return CMD_RET_USAGE;
	}

	mem_start  = simple_strtoul(argv[1], NULL, 16);
	start_blk  = simple_strtoul(argv[2], NULL, 16);
	num_blks   = simple_strtoul(argv[3], NULL, 16);

	if (num_blks == 0) {
		printf("Error: num_blks must be > 0\n");
		return CMD_RET_FAILURE;
	}

	if (argc == 5) {
		loop_count = simple_strtoul(argv[4], NULL, 16);
		if (loop_count <= 0) {
			printf("Warning: loop_count=0, setting to 1\n");
			loop_count = 1;
		}
	}

	/* Check address overflow */
	byte_size = num_blks * 512ULL;
	if (byte_size > (0xFFFFFFFFUL - mem_start)) {
		printf("Error: memory range exceeds 32-bit address space!\n");
		return CMD_RET_FAILURE;
	}

	printf("MMC RW Test:\n");
	printf("  Write buffer: 0x%08lx\n", mem_start);
	printf("  Read buffer : 0x%08lx\n", mem_start + byte_size);
	printf("  Block range : 0x%lx ~ 0x%lx (size=%lu MiB)\n",
			start_blk, start_blk + num_blks - 1, byte_size / (1024 * 1024));
	printf("  Loops       : %lu\n", loop_count);

	for (i = 0; i < loop_count; i++) {
		printf("Loop %lu/%lu Start.\n", i + 1, loop_count);

		ret = mmc_rw_test_single(mem_start, start_blk, num_blks);
		if (ret != 0) {
			printf("Test FAILED at loop %lu!\n", i + 1);
			return CMD_RET_FAILURE;
		}

		printf("Loop %lu/%lu Passed.\n", i + 1, loop_count);
	}

	printf("All %lu loop(s) PASSED.\n", loop_count);
	return CMD_RET_SUCCESS;
}

static int do_mmc_dev(struct cmd_tbl *cmdtp, int flag,
		      int argc, char *const argv[])
{
	char cmd[64];

	if (argc == 4) {
		sprintf(cmd, "mmc %s %s %s %s", argv[0], argv[1],argv[2],argv[3]);
	} else if (argc == 2) {
		sprintf(cmd, "mmc %s %s", argv[0], argv[1]);
	} else {
		return CMD_RET_USAGE;
	}

	curr_device = (int)dectoul(argv[1], NULL);
	return run_command(cmd, 0);;
}

static struct cmd_tbl cmd_mmc[] = {
	U_BOOT_CMD_MKENT(dev, 4, 0, do_mmc_dev, "", ""),
	U_BOOT_CMD_MKENT(set_clk, 4, 1, do_mmc_set_clk_freq, "", ""),
	U_BOOT_CMD_MKENT(set_delay, 4, 1, do_mmc_set_delay_lane, "", ""),
	U_BOOT_CMD_MKENT(tuning, 6, 1, do_mmc_tuning, "", ""),
	U_BOOT_CMD_MKENT(rw_test, 5, 1, do_mmc_rw_test, "", ""),
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
	"dev devid [part] [mode] - show or set current mmc device\n"
	"mmcz set_clk freq - set mmc clock frequency\n"
	"mmcz set_delay <mode> <delay> - set mode & delay, rescan dev\n"
	"mmcz tuning mode start end - scan tx delay from start to end\n"
    "mmcz rw_test <mem_start> <start_blk> <num_blks> [loop_count] - perform MMC R/W test\n"
    "    loop_count defaults to 1 if omitted\n"
    "    (block size = 512 bytes)\n"
	"mode list:\n"
    "    - 0: MMC_LEGACY(emmc supported),\n"
    "    - 1: MMC_HS(emmc supported),\n"
    "    - 2: SD_HS(sd supported),\n"
    "    - 3: MMC_HS_52 (emmc supported),\n"
    "    - 4: MMC_DDR_52(emmc supported),\n"
    "    - 5: UHS_SDR12(sd supported),\n"
    "    - 6: UHS_SDR25(sd supported),\n"
    "    - 7: UHS_SDR50(sd supported),\n"
    "    - 8: UHS_DDR50(not supported),\n"
    "    - 9: UHS_SDR104(sd supported),\n"
    "    - 10: MMC_HS_200(emmc supported),\n"
    "    - 11: MMC_HS_400(emmc supported),\n"
    "    - 12: MMC_HS_400_ES(not supported),\n"
	);
