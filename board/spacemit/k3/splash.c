#include <common.h>
#include <dm.h>
#include <env.h>
#include <image.h>
#include <splash.h>
#include <mmc.h>
#include <fb_spacemit.h>


#if defined(CONFIG_SPLASH_SCREEN) && defined(CONFIG_CMD_BMP)

struct k3_splash_dev_cfg {
	const char *splashsource;
	enum splash_storage storage;
};

static void k3_print_splash_loaded_from(const struct splash_location *location)
{
	if (!location || !location->name) {
		printf("bootlogo loaded successfully\n");
		return;
	}

	if (location->devpart) {
		printf("bootlogo loaded from %s (%s)\n",
		       location->name, location->devpart);
		return;
	}

	if (location->mtdpart) {
		printf("bootlogo loaded from %s (%s)\n",
		       location->name, location->mtdpart);
		return;
	}

	printf("bootlogo loaded from %s\n", location->name);
}

static const struct k3_nor_boot_target *k3_find_nor_boot_target(const char *blk_name)
{
	const struct k3_nor_boot_target *boot_prio;
	unsigned int prio_count;
	unsigned int i;

	boot_prio = k3_nor_get_boot_prio(&prio_count);
	for (i = 0; i < prio_count; i++) {
		if (!strcmp(boot_prio[i].blk_name, blk_name))
			return &boot_prio[i];
	}

	return NULL;
}

static int k3_nor_boot_target_to_splash_cfg(const struct k3_nor_boot_target *target,
					    struct k3_splash_dev_cfg *cfg)
{
	if (!target || !cfg)
		return -EINVAL;

	switch (target->type) {
	case K3_NOR_BOOT_TARGET_NVME:
		cfg->splashsource = "nvme_fs";
		cfg->storage = SPLASH_STORAGE_NVME;
		return 0;
	case K3_NOR_BOOT_TARGET_SCSI:
		cfg->splashsource = "ufs_fs";
		cfg->storage = SPLASH_STORAGE_SCSI;
		return 0;
	case K3_NOR_BOOT_TARGET_MMC:
		cfg->splashsource = "emmc_fs";
		cfg->storage = SPLASH_STORAGE_MMC;
		return 0;
	case K3_NOR_BOOT_TARGET_UDISK:
		cfg->splashsource = "udisk_fs";
		cfg->storage = SPLASH_STORAGE_USB;
		return 0;
	default:
		return -EINVAL;
	}
}

static int k3_detect_nor_splash_part(char **blk_name, int *blk_index, int *part)
{
	if (get_available_boot_blk_dev(blk_name, blk_index)) {
		pr_err("Error: Cannot get available block device\n");
		return -1;
	}

	*part = detect_blk_dev_or_partition_exist(*blk_name, *blk_index, BOOTFS_NAME);
	if (*part < 0) {
		pr_err("Error: Failed to detect partition %s on %s:%d\n",
		       BOOTFS_NAME, *blk_name, *blk_index);
		return -1;
	}

	return 0;
}

int set_emmc_splash_location(struct splash_location *locations) {
	int dev_index = mmc_get_env_dev();
	int part_index;
	char devpart_str[16];
	int err;

	err = get_partition_index_by_name(BOOTFS_NAME, &part_index);
	if (err) {
		pr_err("Failed to get partition index for %s\n", BOOTFS_NAME);
		return -1;
	}

	snprintf(devpart_str, sizeof(devpart_str), "%d:%d", dev_index, part_index);

	locations[0].name = "emmc_fs";
	locations[0].storage = SPLASH_STORAGE_MMC;
	locations[0].flags = SPLASH_STORAGE_FS;
	locations[0].devpart = strdup(devpart_str);
	return 0;
}

int set_mmc_splash_location(struct splash_location *locations) {
	int dev_index = mmc_get_env_dev();
	int part_index;
	char devpart_str[16];
	int err;

	err = get_partition_index_by_name(BOOTFS_NAME, &part_index);
	if (err) {
		pr_err("Failed to get partition index for %s\n", BOOTFS_NAME);
		return -1;
	}

	snprintf(devpart_str, sizeof(devpart_str), "%d:%d", dev_index, part_index);

	locations[0].name = "mmc_fs";
	locations[0].storage = SPLASH_STORAGE_MMC;
	locations[0].flags = SPLASH_STORAGE_FS;
	locations[0].devpart = strdup(devpart_str);
	return 0;
}

int set_nor_splash_location(struct splash_location *locations) {
	int part, blk_index;
	char *blk_name;
	char devpart_str[16];
	const struct k3_nor_boot_target *target;
	struct k3_splash_dev_cfg cfg;

	/* Keep splash source selection aligned with NOR boot priority. */
	if (k3_detect_nor_splash_part(&blk_name, &blk_index, &part))
		return -1;

	target = k3_find_nor_boot_target(blk_name);
	if (k3_nor_boot_target_to_splash_cfg(target, &cfg)) {
		pr_err("Error: Unsupported boot target for splash: %s\n", blk_name);
		return -1;
	}

	snprintf(devpart_str, sizeof(devpart_str), "%d:%d", blk_index, part);

	locations[0].name = (char *)cfg.splashsource;
	locations[0].storage = cfg.storage;
	locations[0].flags = SPLASH_STORAGE_FS;
	locations[0].devpart = strdup(devpart_str);
	if (!locations[0].devpart)
		return -ENOMEM;

	env_set("splashsource", cfg.splashsource);

	return 0;
}

int set_nand_splash_location(struct splash_location *locations)
{
	char *nand_part = parse_mtdparts_and_find_bootfs();
	if (nand_part) {
		locations[0].name = "nand_fs";
		locations[0].storage = SPLASH_STORAGE_NAND;
		locations[0].flags = SPLASH_STORAGE_FS;
		locations[0].mtdpart = strdup(nand_part);
		locations[0].ubivol = strdup(BOOTFS_NAME);
		return 0;
	} else {
		pr_err("Failed to find bootfs on NAND\n");
		return -1;
	}
}

int set_ufs_splash_location(struct splash_location *locations)
{
	int part, blk_index;
	char *blk_name;
	char devpart_str[16];

	if (get_available_boot_blk_dev(&blk_name, &blk_index)) {
		pr_err("Error: Cannot get available block device\n");
		return -1;
	}

	part = detect_blk_dev_or_partition_exist(blk_name, blk_index, BOOTFS_NAME);
	if (part < 0) {
		pr_err("Error: Failed to detect partition %s on %s:%d\n", BOOTFS_NAME, blk_name, blk_index);
		return -1;
	}

	if (strcmp("scsi", blk_name)) {
		pr_err("Error: Unsupported block device type for UFS: %s\n", blk_name);
		return -1;
	}

	snprintf(devpart_str, sizeof(devpart_str), "%d:%d", blk_index, part);

	locations[0].name = "ufs_fs";
	locations[0].storage = SPLASH_STORAGE_SCSI;
	locations[0].flags = SPLASH_STORAGE_FS;
	locations[0].devpart = strdup(devpart_str);

	return 0;
}

int load_splash_screen(void) {
	int ret = -1;
	enum board_boot_mode boot_mode = get_boot_mode();
	struct splash_location splash_locations[1];

	memset(splash_locations, 0, sizeof(splash_locations));

	switch (boot_mode) {
		case BOOT_MODE_EMMC:
			ret = set_emmc_splash_location(splash_locations);
			break;
		case BOOT_MODE_SD:
			ret = set_mmc_splash_location(splash_locations);
			break;
		case BOOT_MODE_NAND:
			ret = set_nand_splash_location(splash_locations);
			break;
		case BOOT_MODE_NOR:
			ret = set_nor_splash_location(splash_locations);
			break;
		case BOOT_MODE_UFS:
			ret = set_ufs_splash_location(splash_locations);
			break;
		default:
			pr_err("Unsupported boot mode for splash screen\n");
			ret = -1;
			break;
	}
	if(ret)
		return -1;

	if (CONFIG_IS_ENABLED(SPLASH_SOURCE)) {
		ret = splash_source_load(splash_locations, ARRAY_SIZE(splash_locations));
		if (!ret)
			k3_print_splash_loaded_from(&splash_locations[0]);
		return ret;
	}

	ret = splash_video_logo_load();
	if (!ret)
		printf("bootlogo loaded from video logo\n");

	return ret;
}

int splash_screen_prepare(void)
{
	enum board_boot_mode boot_mode = get_boot_mode();
	switch (boot_mode) {
	case BOOT_MODE_EMMC:
		env_set("splashsource", "emmc_fs");
		break;
	case BOOT_MODE_SD:
		env_set("splashsource", "mmc_fs");
		break;
	case BOOT_MODE_NAND:
		env_set("splashsource", "nand_fs");
		break;
	case BOOT_MODE_NOR:
		/* set_nor_splash_location() picks and sets splashsource dynamically */
		break;
	case BOOT_MODE_UFS:
		env_set("splashsource", "ufs_fs");
		break;
	case BOOT_MODE_SHELL:
	case BOOT_MODE_USB:
	default:
		pr_err("Cannot support showing bootlogo in this boot mode!\n");
		break;
	}

	return load_splash_screen();
}

#endif
