// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025, Spacemit
 */

#include <common.h>
#include <dm.h>
#include <dm/ofnode.h>
#include <env.h>
#include <env_internal.h>
#include <fdtdec.h>
#include <g_dnl.h>
#include <image.h>
#include <log.h>
#include <mapmem.h>
#include <spl.h>
#include <init.h>
#include <virtio_types.h>
#include <virtio.h>
#include <asm/io.h>
#include <asm/sections.h>
#include <power/regulator.h>
#include <fb_spacemit.h>
#include <misc.h>
#include <net.h>
#include <tlv_eeprom.h>
#include <clk.h>
#include <scsi.h>
#include <ufs.h>
#include <fs.h>
#include <usb.h>
#include <part.h>
#include <mmc.h>
#include "nfs_env.h"
#include <power/pmic.h>
#include <u-boot/crc.h>
#include <linux/log2.h>
#include <spi_flash.h>

#ifdef CONFIG_ESPI
extern bool spacemit_espi_is_ready(void);
#endif

extern u32 ddr_get_density(void);
extern int get_tlvinfo(uint8_t id, uint8_t *buffer, int max_size);
extern int set_tlvinfo(int tcode, char* val);
extern int flush_tlvinfo(void);
extern int update_tlvinfo(void);

int get_chipid_from_efuse(uint64_t *chipid);

bool is_video_connected = false;
static char found_partition[64] = {0};
static uint32_t reboot_config;

DECLARE_GLOBAL_DATA_PTR;

struct boot_storage_op
{
	uint32_t boot_storage_type;
	// private partition byte offset
	uint32_t priv_partition_offset;
	ulong (*read)(ulong byte_addr, ulong byte_size, void *buff);
	bool (*write)(ulong byte_addr, ulong byte_size, void *buff);
};

static struct k3_nor_boot_target k3_nor_boot_prio[] = {
#ifdef CONFIG_SCSI
	{ K3_NOR_BOOT_TARGET_SCSI, "scsi", "ufs_devnum",
	  K3_NOR_UFS_DEVNUM_DEFAULT },
#endif
#ifdef CONFIG_NVME
	{ K3_NOR_BOOT_TARGET_NVME, "nvme", "ssd_devnum",
	  K3_NOR_SSD_DEVNUM_DEFAULT },
#endif
#ifdef CONFIG_MMC
	{ K3_NOR_BOOT_TARGET_MMC, "mmc", "emmc_devnum",
	  K3_NOR_EMMC_DEVNUM_DEFAULT },
#endif
#if defined(CONFIG_USB) && defined(CONFIG_USB_STORAGE)
	{ K3_NOR_BOOT_TARGET_UDISK, "usb", "usb_devnum",
	  K3_NOR_USB_DEVNUM_DEFAULT },
#endif
};

/*
 * Returns if the target was found
 */
static bool k3_nor_set_highest_prio(const char *target_name)
{
	int count = ARRAY_SIZE(k3_nor_boot_prio);
	const char *mapped_name = target_name;
	struct k3_nor_boot_target temp;
	int i, j;

	if (!target_name || !strlen(target_name))
		return false;

	// for ease of use, string substitution is provided
	if (strcasecmp(target_name, "UFS") == 0) {
		mapped_name = "scsi";
	} else if (strcasecmp(target_name, "SSD") == 0) {
		mapped_name = "nvme";
	}

	for (i = 0; i < count; i++) {
		if (strcasecmp(k3_nor_boot_prio[i].blk_name, mapped_name) == 0) {
			if (i == 0)
				return true;

			// shift its previous element by one position
			temp = k3_nor_boot_prio[i];
			for (j = i; j > 0; j--) {
				k3_nor_boot_prio[j] = k3_nor_boot_prio[j - 1];
			}

			k3_nor_boot_prio[0] = temp;
			return true;
		}
	}

	return false;
}

static bool already_updated = false;

static void k3_nor_update_prio_from_dtb(void)
{
	const char *prior_target;
	ofnode node;

	node = ofnode_path(NOR_BOOT_PRIORITY_NODE);
	if (!ofnode_valid(node)) {
		return;
	}

	prior_target = ofnode_read_string(node, "highest-priority");
	if (prior_target)
		k3_nor_set_highest_prio(prior_target);
}


/*
 * Returns if check dtb is needed
 */
bool k3_nor_update_prio_from_tlv(void)
{
	char read_buf[64] = {0};
	int read_len;

	read_len = get_tlvinfo(TLV_CODE_SECOND_BOOT_DEV, read_buf, sizeof(read_buf) - 1);

	if (read_len > 0) {
		if (k3_nor_set_highest_prio(read_buf)) {
			return false;
		}
	}

	return true;
}

const struct k3_nor_boot_target *k3_nor_get_boot_prio(unsigned int *count)
{
	// support customize second boot device
	// tlv is prior than dtb
	if (!already_updated) {
		already_updated = true;
		if (k3_nor_update_prio_from_tlv())
			k3_nor_update_prio_from_dtb();
	}

	if (count)
		*count = ARRAY_SIZE(k3_nor_boot_prio);

	return k3_nor_boot_prio;
}

void import_env_from_bootfs(void);
void setenv_boot_mode(void);
void set_env_ethaddr(void);
void refresh_config_info(void);

static u32 env_get_u32_default(const char *name, u32 default_value)
{
	const char *val = env_get(name);
	char *endp;
	ulong parsed;

	if (!val || !*val)
		return default_value;

	parsed = simple_strtoul(val, &endp, 10);
	if (endp == val)
		return default_value;

	return (u32)parsed;
}

extern char usb_started;
#if defined(CONFIG_USB) && defined(CONFIG_USB_STORAGE)
static struct blk_desc *k3_nor_get_usb_desc(u32 devnum)
{
	static bool usb_scanned;

	if (BOOT_MODE_USB == get_boot_mode()) {
		pr_info("Should NOT scan USB storage during fastboot download mode\n");
		return NULL;
	}

	if (!usb_scanned) {
		if (!usb_started) {
			usb_init();
		}
		usb_stor_scan(1);
		usb_scanned = true;
	}

	return blk_get_dev("usb", devnum);
}
#endif

extern bool nvme_scanned;
#ifdef CONFIG_NVME
static struct blk_desc *k3_nor_get_nvme_desc(u32 devnum)
{
	if (!nvme_scanned) {
		run_command("nvme scan", 0);
		nvme_scanned = true;
	}

	return blk_get_dev("nvme", devnum);
}
#endif

#ifdef CONFIG_SCSI
int board_scsi_scan_once(bool verbose)
{
	static bool scsi_scanned;
	int ret;

	if (scsi_scanned)
		return 0;

	ret = scsi_scan(verbose);
	if (!ret) {
		scsi_scanned = true;
#ifndef CONFIG_SPL_BUILD
		env_set("scsi_scanned", "1");
#endif
	}

	return ret;
}

int k3_prepare_scsi_flash_target(u32 devnum)
{
	static bool scsi_flash_target_prepared;
	int ret;

	if (scsi_flash_target_prepared)
		return 0;

#ifdef CONFIG_UFS
	ret = ufs_prepare_dev_for_flash(devnum);
	if (ret) {
		pr_err("failed to prepare UFS device %u for flashing: %d\n",
		       devnum, ret);
		return ret;
	}
#endif

	ret = board_scsi_scan_once(false);
	if (ret) {
		pr_err("failed to scan SCSI bus for device %u: %d\n", devnum, ret);
		return ret;
	}

	scsi_flash_target_prepared = true;
	return 0;
}

static struct blk_desc *k3_nor_get_scsi_desc(u32 devnum)
{
	if (board_scsi_scan_once(false))
		return NULL;

	return blk_get_dev("scsi", devnum);
}
#endif

#ifdef CONFIG_MMC
static struct blk_desc *k3_nor_get_mmc_desc(u32 devnum)
{
	struct mmc *mmc = find_mmc_device(devnum);

	if (!mmc || mmc_init(mmc))
		return NULL;

	return mmc_get_blk_desc(mmc);
}
#endif

static struct blk_desc *k3_nor_get_desc_by_target(const struct k3_nor_boot_target *target,
						  u32 devnum)
{
	switch (target->type) {
#if defined(CONFIG_USB) && defined(CONFIG_USB_STORAGE)
	case K3_NOR_BOOT_TARGET_UDISK:
		return k3_nor_get_usb_desc(devnum);
#endif
#ifdef CONFIG_NVME
	case K3_NOR_BOOT_TARGET_NVME:
		return k3_nor_get_nvme_desc(devnum);
#endif
#ifdef CONFIG_SCSI
	case K3_NOR_BOOT_TARGET_SCSI:
		return k3_nor_get_scsi_desc(devnum);
#endif
#ifdef CONFIG_MMC
	case K3_NOR_BOOT_TARGET_MMC:
		return k3_nor_get_mmc_desc(devnum);
#endif
	default:
		return NULL;
	}
}

static bool k3_nor_has_bootfs(struct blk_desc *desc)
{
	struct disk_partition info;

	if (!desc)
		return false;

	return part_get_info_by_name(desc, BOOTFS_NAME, &info) >= 0;
}

#if defined(CONFIG_MMC) || defined(CONFIG_SCSI)
static ulong read_block_device(struct blk_desc *dev_desc, ulong byte_addr, ulong byte_size, void *buff)
{
	ulong ret, tail_bytes, total_bytes;
	void *temp;

	if (!dev_desc || dev_desc->type == DEV_TYPE_UNKNOWN) {
		pr_err("invalid block device\n");
		return 0;
	}

	tail_bytes = byte_size - round_down(byte_size, dev_desc->blksz);

	ret = blk_dread(dev_desc, byte_addr / dev_desc->blksz, byte_size / dev_desc->blksz, buff);
	total_bytes = ret * dev_desc->blksz;
	if (tail_bytes) {
		temp = malloc(dev_desc->blksz);
		blk_dread(dev_desc, byte_addr / dev_desc->blksz + ret, 1, temp);
		memcpy((char *)buff + total_bytes, temp, tail_bytes);
		free(temp);

		total_bytes += tail_bytes;
	}
	return total_bytes;
}
#endif

#ifdef CONFIG_MMC
static ulong read_boot_storage_emmc(ulong byte_addr, ulong byte_size, void* buff)
{
	return read_block_device(blk_get_dev("mmc", MMC_DEV_EMMC),
		byte_addr, byte_size, buff);
}
#endif

static ulong read_boot_storage_spinor(ulong byte_addr, ulong byte_size, void* buff)
{
	struct udevice *dev;

	mtd_probe_devices();
	uclass_get_device(UCLASS_SPI_FLASH, 0, &dev);

	if ((NULL != dev) && (0 == spi_flash_read_dm(dev, byte_addr, byte_size, buff))) {
		return byte_size;
	} else {
		return 0;
	}
}

#ifdef CONFIG_SCSI
static ulong read_boot_storage_ufs(ulong byte_addr, ulong byte_size, void* buff)
{
	return read_block_device(k3_nor_get_scsi_desc(K3_NOR_UFS_DEVNUM_DEFAULT),
		byte_addr, byte_size, buff);
}
#endif

#if defined(CONFIG_SPL_BUILD)

// NOT support write operation in SPL stage
static const struct boot_storage_op storage_op[] = {
#ifdef CONFIG_MMC
	{ BOOT_MODE_EMMC, 0x140000, read_boot_storage_emmc, NULL },
#endif
	{ BOOT_MODE_NOR, 0x10000, read_boot_storage_spinor, NULL },
#ifdef CONFIG_SCSI
	{ BOOT_MODE_UFS, 0x140000, read_boot_storage_ufs, NULL },
#endif
};

#else

#if defined(CONFIG_MMC) || defined(CONFIG_SCSI)
static bool write_block_device(struct blk_desc *dev_desc, ulong byte_addr, ulong byte_size, void *buff)
{
	if (!dev_desc || dev_desc->type == DEV_TYPE_UNKNOWN) {
		pr_err("invalid block device\n");
		return 0;
	}

	pr_info("write %ldbyte to block device(%d) @address %ld\n",
		byte_size, dev_desc->if_type, byte_addr);
	blk_dwrite(dev_desc, byte_addr / dev_desc->blksz,
		round_up(byte_size, dev_desc->blksz) / dev_desc->blksz, buff);
	return true;
}
#endif

#ifdef CONFIG_MMC
static bool write_boot_storage_emmc(ulong byte_addr, ulong byte_size, void* buff)
{
	return write_block_device(blk_get_dev("mmc", MMC_DEV_EMMC),
		byte_addr, byte_size, buff);
}
#endif

static bool write_boot_storage_spinor(ulong byte_addr, ulong byte_size, void* buff)
{
	struct udevice *dev;

	/* Probe MTD devices first */
	mtd_probe_devices();
	uclass_get_device(UCLASS_SPI_FLASH, 0, &dev);

	if ((NULL != dev) && (0 == spi_flash_erase_dm(dev, byte_addr, round_up(byte_size, 0x1000)))
		&& (0 == spi_flash_write_dm(dev, byte_addr, byte_size, buff))) {
		pr_info("write %ldbyte to spinor @offset %ld\n", byte_size, byte_addr);
		return true;
	} else
		return false;
}

#ifdef CONFIG_SCSI
static bool write_boot_storage_ufs(ulong byte_addr, ulong byte_size, void* buff)
{
	return write_block_device(k3_nor_get_scsi_desc(K3_NOR_UFS_DEVNUM_DEFAULT),
		byte_addr, byte_size, buff);
}
#endif

static const struct boot_storage_op storage_op[] = {
#ifdef CONFIG_MMC
	{ BOOT_MODE_EMMC, 0x140000, read_boot_storage_emmc, write_boot_storage_emmc },
#endif
	{ BOOT_MODE_NOR, 0x10000, read_boot_storage_spinor, write_boot_storage_spinor },
#ifdef CONFIG_SCSI
	{ BOOT_MODE_UFS, 0x140000, read_boot_storage_ufs, write_boot_storage_ufs },
#endif
};
#endif

static ulong read_boot_priv_partition(void* buff, ulong offset, ulong byte_size)
{
	int i;
	// read data from boot storage
	enum board_boot_mode boot_storage = get_boot_pin_select();

	for (i = 0; i < ARRAY_SIZE(storage_op); i++) {
		if (boot_storage == storage_op[i].boot_storage_type) {
			offset += storage_op[i].priv_partition_offset;
			return storage_op[i].read(offset, byte_size, buff);
		}
	}

	return 0;
}

static bool write_boot_priv_partition(void* buff, ulong offset, ulong byte_size)
{
	int i;
	// save data to boot storage
	enum board_boot_mode boot_storage = get_boot_pin_select();

	for (i = 0; i < ARRAY_SIZE(storage_op); i++) {
		if (boot_storage == storage_op[i].boot_storage_type) {
			offset += storage_op[i].priv_partition_offset;
			return storage_op[i].write(offset, byte_size, buff);
		}
	}

	return false;
}

ulong read_ddr_training_info(struct ddr_info_t* info)
{
	return read_boot_priv_partition(info, 0, sizeof(*info));
}

bool save_ddr_training_info(void)
{
	struct ddr_info_t* info = (struct ddr_info_t*)DDR_TRAINING_INFO_BUFF;

	// this only happened during first boot
	if ((DDR_TRAINING_INFO_MAGIC == info->magic)
		&& (info->crc32 == crc32(0, (const uchar*)&info->chipid, sizeof(*info) - 8))) {
#if CONFIG_IS_ENABLED(SPACEMIT_K1X_EFUSE)
		uint64_t chipid = 0;
		if (0 == get_chipid_from_efuse(&chipid)) {
			info->chipid = chipid;
			info->crc32 = crc32(0, (const uint8_t*)&info->chipid, sizeof(*info) - 8);
		}
#endif
		pr_info("save DDR training info\n");
		return write_boot_priv_partition(info, 0, sizeof(*info));
	}

	return false;
}

#if CONFIG_IS_ENABLED(SPACEMIT_K1X_EFUSE)
int get_chipid_from_efuse(uint64_t *chipid)
{
	struct udevice *dev;
	uint8_t fuses[9];
	int ret;

	if (NULL == chipid)
		return EACCES;

	/* retrieve the device */
	ret = uclass_get_device_by_driver(UCLASS_MISC,
			DM_DRIVER_GET(spacemit_k1x_efuse), &dev);
	if (ret) {
		return ENODEV;
	}

	// read from efuse, each bank has 32byte efuse data
	// chipid in bank7 bit191~251
	ret = misc_read(dev, 7 * 32 + 23, fuses, sizeof(fuses));
	if (0 == ret) {
		// 1. get bit 192~251
		// 2. left shift 1bit, and merge with efuse_bank[7].bit191
		*chipid = 0;
		memcpy(chipid, &fuses[1], 8);
		*chipid <<= 1;
		*chipid |= (fuses[0] & 0x80) >> 7;
		pr_debug("Get chipid %llx\n", *chipid);
	}

	return ret;
}

int get_dro_from_efuse(uint32_t *dro)
{
	struct udevice *dev;
	uint8_t fuses[2];
	int ret;

	if (NULL == dro)
		return EACCES;

	*dro = SVT_DRO_DEFAULT_VALUE;

	/* retrieve the device */
	ret = uclass_get_device_by_driver(UCLASS_MISC,
			DM_DRIVER_GET(spacemit_k1x_efuse), &dev);
	if (ret) {
		return ENODEV;
	}

	// read from efuse, each bank has 32byte efuse data
	// SVT-DRO in bank7 bit173~bit181
	ret = misc_read(dev, 7 * 32 + 21, fuses, sizeof(fuses));
	if (0 == ret) {
		// (byte1 bit0~bit5) << 3 | (byte0 bit5~7) >> 5
		*dro = (fuses[0] >> 5) & 0x07;
		*dro |= (fuses[1] & 0x3F) << 3;
	}

	return 0;
}

int get_chipinfo_from_efuse(uint32_t *product_id, uint32_t *wafer_tid)
{
	struct udevice *dev;
	uint8_t fuses[3];
	int ret;

	if ((NULL == product_id) || (NULL == wafer_tid))
		return EACCES;

	*product_id = 0;
	*wafer_tid = 0;

	/* retrieve the device */
	ret = uclass_get_device_by_driver(UCLASS_MISC,
			DM_DRIVER_GET(spacemit_k1x_efuse), &dev);
	if (ret) {
		return ENODEV;
	}

	// read from efuse, each bank has 32byte efuse data
	// product id in bank7 bit182~bit190
	ret = misc_read(dev, 7 * 32 + 22, fuses, sizeof(fuses));
	if (0 == ret) {
		// (byte1 bit0~bit6) << 2 | (byte0 bit6~7) >> 6
		*product_id = (fuses[0] >> 6) & 0x03;
		*product_id |= (fuses[1] & 0x7F) << 2;
	}

	// read from efuse, each bank has 32byte efuse data
	// product id in bank7 bit139~bit154
	ret = misc_read(dev, 7 * 32 + 17, fuses, sizeof(fuses));
	if (0 == ret) {
		// (byte1 bit0~bit6) << 2 | (byte0 bit3~7) >> 3
		*wafer_tid = (fuses[0] >> 3) & 0x1F;
		*wafer_tid |= fuses[1] << 5;
		*wafer_tid |= (fuses[2] & 0x07) << 13;
	}

	return ret;
}
#endif

uint32_t get_serial_number(char *sn, uint32_t max_size)
{
	uint64_t chipid = 0;
	uint32_t i, seed;
	int ret = -1;
	char temp[32], *serial;

	memset(temp, 0, sizeof(temp));
	memset(sn, 0, max_size);

	serial = env_get("serial#");
	if (NULL != serial) {
		strlcpy(temp, serial, sizeof(temp));
	}
	else if (get_tlvinfo(TLV_CODE_SERIAL_NUMBER, temp, sizeof(temp)) <= 0) {
#if CONFIG_IS_ENABLED(SPACEMIT_K1X_EFUSE)
		// use chipid in efuse as serial number
		ret = get_chipid_from_efuse(&chipid);
#endif
		// check if chipid is valid
		if ((0 != ret) || (0 == chipid)) {
			seed = get_ticks();
			for (i = 0; i < sizeof(chipid); i++) {
				((uint8_t*)&chipid)[i] = rand_r(&seed);
			}
		}
		snprintf(temp, sizeof(temp), "%016llx", chipid);
	}

	i = min(max_size, (uint32_t)strlen(temp));
	memcpy(sn, temp, i);
	return i;
}

void update_usb_serial_number(void)
{
#ifdef CONFIG_USB_SET_SERIAL_NUMBER
	char temp[32];

	if (0 != get_serial_number(temp, sizeof(temp))) {
		g_dnl_set_serialnumber(temp);
	}
#endif
}

void set_dev_serial_no(void)
{
	char serial[32], temp[32];

	if (0 != get_serial_number(serial, sizeof(serial))) {
		memset(temp, 0, sizeof(temp));
		// save serial number to TLV when it is NOT in TLV
		if ((get_tlvinfo(TLV_CODE_SERIAL_NUMBER, temp, sizeof(temp)) <= 0)
			|| (0 != strcmp(serial, temp))) {
			set_tlvinfo(TLV_CODE_SERIAL_NUMBER, serial);
			flush_tlvinfo();
		}
	}
}

int board_init(void)
{
	int ret = 0;

#if CONFIG_IS_ENABLED(RT7451_RETIMER)
	struct udevice *dev;
#endif

#ifdef CONFIG_DM_REGULATOR_SPM8XX
	ret = regulators_enable_boot_on(true);
	if (ret)
		pr_debug("%s: Cannot enable boot on regulator\n", __func__);
#endif

#ifdef CONFIG_ESPI
	ret = uclass_probe_all(UCLASS_ESPI);
	if (ret) {
		if (ret != -ENODEV)
			printf("Warn: Failed to probe eSPI (err=%d), skipping EC\n", ret);
	} else if (!spacemit_espi_is_ready()) {
		printf("eSPI not ready, skipping EC probe\n");
	} else {
		/* Probe CrosEC and its children (I2C tunnel devices) */
		ret = uclass_probe_all(UCLASS_CROS_EC);
		if (ret)
			printf("Warn: Failed to probe CrosEC (err=%d)\n", ret);
	}
#endif

#if CONFIG_IS_ENABLED(RT7451_RETIMER)
	ret = uclass_get_device_by_driver(UCLASS_MISC,
					  DM_DRIVER_GET(rt7451), &dev);
	if (ret && ret != -ENODEV)
		printf("Warn: Failed to init RT7451 retimer (err=%d)\n", ret);
#endif

	return 0;
}

void get_reboot_config(void)
{
	int ret;

	struct udevice *dev;
	uint8_t value;
	reboot_config = get_boot_mode();

	ret = uclass_get_device_by_driver(UCLASS_PMIC,
			DM_DRIVER_GET(spacemit_pm8xx), &dev);
	if (ret) {
		pr_err("spacemit reboot: get PMIC device failed: %d\n", ret);
		return;
	}

	/* Read related P1 register's value to check if we should go to fastboot */
	ret = pmic_read(dev, P1_NON_VOLATILE_REG, &value, 1);
	if (ret < 0) {
		pr_err("spacemit reboot: PMIC read failed\n");
		return;
	}
	pr_info("spacemit reboot: read PMIC reg 0x%x value 0x%x\n", P1_NON_VOLATILE_REG, value);
	if ((value & P1_NON_VOLATILE_REG_MASK) == P1_NON_VOLATILE_REG_FASTBOOT) {
		reboot_config = BOOT_MODE_USB;
	}

	/* Clear related P1 register's value */
	value &= ~P1_NON_VOLATILE_REG_MASK;
	ret = pmic_write(dev, P1_NON_VOLATILE_REG, &value, 1);
	if (ret < 0) {
		pr_err("spacemit reboot: PMIC write failed\n");
		return;
	}
}

void run_fastboot_command(void)
{
	if (BOOT_MODE_USB == get_boot_mode() || reboot_config == BOOT_MODE_USB) {
		/* show flash log*/
		env_set("stdout", env_get("stdout_flash"));

		update_usb_serial_number();

#if !defined(CONFIG_SPL_BUILD) && CONFIG_IS_ENABLED(FASTBOOT_CMD_OEM_SPEED)
		g_dnl_set_max_speed(spacemit_k3_fastboot_requested_speed());
#endif

		char *cmd_para = "fastboot 0";
		run_command(cmd_para, 0);

		// it may update tlv during USB fastboot stage
		refresh_config_info();
	}
}

void try_flash_image_from_card(void)
{
#ifdef CONFIG_MMC
	struct mmc *mmc;
	struct disk_partition info;
	int part;
	char cmd[128] = {"\0"};

	mmc = find_mmc_device(MMC_DEV_SD);
	if ((NULL == mmc) || (0 != mmc_init(mmc)))
		return;

	part = part_get_info_by_name(mmc_get_blk_desc(mmc), BOOTFS_NAME, &info);
	if (part < 0) {
		pr_err("NO partition %s in card\n", BOOTFS_NAME);
		return;
	}

	/*check if flash config file is in sd card*/
	sprintf(cmd, "fatsize mmc %d:%d %s", MMC_DEV_SD, part, FLASH_CONFIG_FILE_NAME);
	pr_debug("cmd: %s\n", cmd);
	if (0 != run_command(cmd, 0)) {
		pr_err("Can NOT find partition table %s in card\n", FLASH_CONFIG_FILE_NAME);
		return;
	}

	/* show flash log*/
	env_set("stdout", env_get("stdout_flash"));
	run_command("flash_image mmc", 0);
#endif
	return;
}

#ifdef CONFIG_BOOT_FROM_USB_DISK

static int part_num;
static int udisk_dev_num;
static void try_boot_from_udisk(void)
{
	char bootfs_part[16] = "";
	char dev_num_str[4] = "";
	struct blk_desc *dev_desc;
	struct disk_partition info;
	bool env_found = false, perform_flash = false;

	pr_info("udisk boot: Try verifying if any USB dev is a boot disk\n");

	if (run_command("usb start", 0) != 0) {
		pr_info("udisk boot: usb start failed\n");
		return;
	}

	/* search all udisk devices */
	for (udisk_dev_num = 0; ; udisk_dev_num++) {
		dev_desc = blk_get_dev("usb", udisk_dev_num);

		if (!dev_desc) {
			break;
		}

		part_num = part_get_info_by_name(dev_desc, "bootfs", &info);
		if (part_num < 0) {
			pr_info("udisk boot: Partition 'bootfs' not found on usb %d\n", udisk_dev_num);
			continue;
		}

		pr_info("udisk boot: Found 'bootfs' at usb %d, partition %d %s %s\n",
			udisk_dev_num, part_num, dev_desc->vendor, dev_desc->product);

		sprintf(bootfs_part, "%d:%d", udisk_dev_num, part_num);
		if (fs_set_blk_dev("usb", bootfs_part, FS_TYPE_ANY) != 0) {
			pr_info("udisk boot: set blk dev fail on usb %d\n", udisk_dev_num);
			continue;
		}

		if (fs_exists("partition_flash.json")) {
			pr_info("udisk boot: found partition_flash.json on usb %d! starting to flash...\n",
			       udisk_dev_num);
			perform_flash = true;
			break;
		}

		// reset related state, or env_k3.txt may not be detected even if this file exist
		fs_set_blk_dev("usb", bootfs_part, FS_TYPE_ANY);

		if (fs_exists("env_k3.txt")) {
			pr_info("udisk boot: found env_k3.txt on usb %d! setting environment value...\n",
			       udisk_dev_num);
			env_found = true;
			break;
		}

		pr_info("udisk boot: file env_k3.txt or partition_flash.json not found on usb %d\n", udisk_dev_num);
	}

	if (perform_flash) {
		run_command("flash_image usb", 0);
		return;
	}

	if (!env_found) {
		pr_info("udisk boot: Could not find a valid bootable USB drive\n");
		return;
	}

	env_set("boot_override", "udisk");
	env_set("boot_device", "udisk");
	env_set("boot_devname", "usb");

	sprintf(dev_num_str, "%d", udisk_dev_num);
	env_set("boot_devnum", dev_num_str);
}
#endif

static int k3_clear_sram(void)
{
	uint64_t *buf64;
	ulong addr = SRAM_BASE_ADDR;
	size_t size = SRAM_TOTAL_SIZE;

	if (!size || (size % sizeof(*buf64))) {
		pr_warn("Failed to clear SRAM 0x%08lx (size=0x%lx)\n", addr, size);
		return -EINVAL;
	}

	buf64 = (uint64_t *)map_sysmem(addr, size);
	memset(buf64, 0, size);
	unmap_sysmem(buf64);

	pr_info("SRAM cleared: addr=0x%08lx size=0x%lx\n", addr, size);
	return 0;
}

int board_late_init(void)
{
#if CONFIG_IS_ENABLED(HWMON_SENSORS_CTF2301) || CONFIG_IS_ENABLED(RT7451_RETIMER)
	struct udevice *dev;
#endif
	ulong kernel_start;
	ofnode chosen_node;
	int ret;

#if CONFIG_IS_ENABLED(HWMON_SENSORS_CTF2301)
	ret = uclass_get_device_by_driver(UCLASS_MISC,
					  DM_DRIVER_GET(ctf2301), &dev);
	if (ret && ret != -ENODEV)
		printf("Warn: Failed to force init ctf2301 fan (err=%d)\n", ret);
#endif

	ret = uclass_probe_all(UCLASS_VIDEO);
	if (ret) {
		pr_info("video devices not found or not probed yet: %d\n", ret);
	}
	ret = uclass_probe_all(UCLASS_DISPLAY);
	if (ret) {
		pr_info("display devices not found or not probed yet: %d\n", ret);
	}

	set_env_ethaddr();
	set_dev_serial_no();
	refresh_config_info();
#ifdef CONFIG_DDR_TRAINING_SAVE_RESTORE
	save_ddr_training_info();
#endif

	get_reboot_config();
	run_fastboot_command();

	if (BOOT_MODE_SD == get_boot_mode()) {
		try_flash_image_from_card();
	}

#ifdef CONFIG_BOOT_FROM_USB_DISK
	try_boot_from_udisk();
#endif

	// MUST NOT clear SRAM before any image operation, it contain critical info inside
	k3_clear_sram();

	import_env_from_bootfs();

	setenv_boot_mode();

	/* Defaults for NOR boot fallback scripting */
	if (!env_get("usb_devnum"))
		env_set("usb_devnum", simple_itoa(K3_NOR_USB_DEVNUM_DEFAULT));
	if (!env_get("ssd_devnum"))
		env_set("ssd_devnum", simple_itoa(K3_NOR_SSD_DEVNUM_DEFAULT));
	if (!env_get("ufs_devnum"))
		env_set("ufs_devnum", simple_itoa(K3_NOR_UFS_DEVNUM_DEFAULT));
	if (!env_get("emmc_devnum"))
		env_set("emmc_devnum", simple_itoa(K3_NOR_EMMC_DEVNUM_DEFAULT));

	chosen_node = ofnode_path("/chosen");
	if (!ofnode_valid(chosen_node)) {
		debug("No chosen node found, can't get kernel start address\n");
		return 0;
	}

	ret = ofnode_read_u64(chosen_node, "riscv,kernel-start",
			      (u64 *)&kernel_start);
	if (ret) {
		debug("Can't find kernel start address in device tree\n");
		return 0;
	}

	env_set_hex("kernel_start", kernel_start);

	return 0;
}

int misc_init_r(void)
{
	return 0;
}

u64 read_memory_size_from_dtb(void)
{
	ofnode node, subnode;
	u64 dram_size = 0;
	fdt_size_t size;
	const void *fdt = gd->fdt_blob;
	const char *prop;

	if (fdt_check_header(fdt)) {
		pr_err("Invalid uboot DTB\n");
		return 0;
	}

	node = ofnode_path("/");
	ofnode_for_each_subnode(subnode, node) {
		pr_debug("subnode name: %s\n", ofnode_get_name(subnode));
		prop = ofnode_get_property(subnode, "device_type", NULL);
		if (prop && !strcmp(prop, "memory")) {
			size = ofnode_get_size(subnode);
			if (FDT_SIZE_T_NONE != size) {
				dram_size += size;
				pr_debug("Found memory node, size: 0x%llx\n", dram_size);
			}
		}
	}

	pr_debug("Get memory size 0x%llx from dtb\n", dram_size);
	return dram_size;
}

int dram_init(void)
{
	u64 dram_size;

#if CONFIG_K3_BOARD_FPGA
	dram_size = SZ_2GB;
	gd->ram_base = CONFIG_SYS_SDRAM_BASE;
	gd->ram_size = dram_size - SEC_IMG_SIZE;
#else
#ifdef CONFIG_SPL_BUILD
	// report real ddr size during spl stage
	dram_size = (u64)ddr_get_density() * SZ_1MB;
	gd->ram_base = CONFIG_SYS_SDRAM_BASE - SEC_IMG_SIZE;
	gd->ram_size = dram_size;
#else
	// memory info would be update to dtb by spl_perform_fixups
	dram_size = read_memory_size_from_dtb();
	// initial 32MB of memory is invisible, reserved for openSBI and esos.
	gd->ram_base = CONFIG_SYS_SDRAM_BASE;
	gd->ram_size = dram_size - SEC_IMG_SIZE;
#endif
#endif

	return 0;
}

int dram_init_banksize(void)
{
	u64 dram_size;

	if (0 == gd->ram_size)
		dram_init();

	memset(gd->bd->bi_dram, 0, sizeof(gd->bd->bi_dram));

	dram_size = gd->ram_base + gd->ram_size - (CONFIG_SYS_SDRAM_BASE - SEC_IMG_SIZE);
	gd->bd->bi_dram[0].start = gd->ram_base;
	gd->bd->bi_dram[0].size = gd->ram_size;

	if (!is_power_of_2(dram_size)) {
		// skip memory address gap when DDR density is not power of 2
		gd->bd->bi_dram[0].size -= dram_size / 2;

		gd->bd->bi_dram[1].start = CONFIG_SYS_SDRAM_BASE - SEC_IMG_SIZE + roundup_pow_of_two(dram_size / 2);
		gd->bd->bi_dram[1].size = dram_size / 2;
	}

	return 0;
}

ulong board_get_usable_ram_top(ulong total_size)
{
	u64 dram_size, memory_top;

	memory_top = gd->ram_base + gd->ram_size;
	dram_size = memory_top - (CONFIG_SYS_SDRAM_BASE - SEC_IMG_SIZE);

	if (!is_power_of_2(dram_size)) {
		// only use half memory when DDR density is not power of 2
		memory_top -= dram_size / 2;
	}

	// DPU can only access 34bit address space
	if (memory_top > 0x400000000) {
		return 0x400000000;
	} else {
		return memory_top;
	}
}

void *board_fdt_blob_setup(int *err)
{
	*err = 0;

	/* Stored the DTB address there during our init */
	if (IS_ENABLED(CONFIG_OF_SEPARATE) || IS_ENABLED(CONFIG_OF_BOARD)) {
		if (gd->arch.firmware_fdt_addr){
			if (!fdt_check_header((void *)(ulong)gd->arch.firmware_fdt_addr)){
				return (void *)(ulong)gd->arch.firmware_fdt_addr;
			}
		}
	}
	return (ulong *)&_end;
}


/******************************************************************************
 * Boot mode support function
 *******************************************************************************/
/*
 * Boot mode strap pins (4 bits total)
 *
 * bit3 bit2 bit1-0  Meaning
 *  1     0     XX   Update from USB
 *  1     1     XX   Update from UART
 *  0     X     00   Normal boot from eMMC
 *  0     X     10   Normal boot from SPI Nand
 *  0     X     01   Normal boot from SPI Nor
 *  0     X     11   Normal boot from UFS
 */
enum board_boot_mode get_boot_pin_select(void)
{
	/* Decode full 4-bit strap field starting at BOOT_STRAP_BIT_OFFSET */
	u32 strap = readl((void *)BOOT_PIN_SELECT);
	u32 pins = (strap >> BOOT_STRAP_BIT_OFFSET) & 0xF; /* bit3:bit0 */
	pr_debug("strap_pins:%#x\n", pins);

	/* boot storage only use bit1-0 */
	switch (pins & 0x3) {
	case BOOT_STRAP_BIT_EMMC: /* 00 */
		return BOOT_MODE_EMMC;
	case BOOT_STRAP_BIT_NOR:  /* 01 */
		return BOOT_MODE_NOR;
	case BOOT_STRAP_BIT_NAND: /* 10 */
		return BOOT_MODE_NAND;
	case BOOT_STRAP_BIT_UFS:  /* 11 */
		return BOOT_MODE_UFS;
	default:
		return BOOT_MODE_SD;
    }
}

/* Get boot mode based on bootrom implementation */
enum board_boot_mode get_boot_mode(void)
{
	u32 boot_flag_reg;
	enum board_boot_mode mode = BOOT_MODE_USB;

	/* Read bootrom boot flag from BOOT_DEV_FLAG_REG */
	boot_flag_reg = readl((void*)BOOT_DEV_FLAG_REG);
	pr_debug("%s boot_flag_reg:0x%x\n", __func__, boot_flag_reg);

	/* Check bootrom set boot type */
	u32 boot_type = boot_flag_reg & BOOT_TYPE_MASK;
	switch (boot_type) {
	case BOOT_MODE_USB:
		mode = BOOT_MODE_USB;
		break;
	case BOOT_MODE_UART:
		mode = BOOT_MODE_UART;
		break;
	case BOOT_MODE_EMMC:
		mode = BOOT_MODE_EMMC;
		break;
	case BOOT_MODE_NAND:
		mode = BOOT_MODE_NAND;
		break;
	case BOOT_MODE_NOR:
		mode = BOOT_MODE_NOR;
		break;
	case BOOT_MODE_UFS:
		mode = BOOT_MODE_UFS;
		break;
	case BOOT_MODE_SD:
		mode = BOOT_MODE_SD;
		break;
	default:
		mode = BOOT_MODE_SHELL;  /* Default to shell if unknown */
		break;
	}

	pr_debug("Final boot mode: 0x%x\n", mode);
	return mode;
}

void setenv_boot_mode(void)
{
	/*
	 * Allow env (e.g. env_k3.txt from bootfs) to force a boot path without
	 * being overwritten by bootrom strap detection. Clear after use so it
	 * doesn't "stick" across boots unless explicitly set again.
	 */
	const char *boot_override = env_get("boot_override");

	if (boot_override) {
		env_set("boot_device", boot_override);
		env_set("boot_override", NULL);
		return;
	}

	u32 boot_mode = get_boot_mode();
	switch (boot_mode) {
	case BOOT_MODE_NAND:
		env_set("boot_device", "nand");
		break;
	case BOOT_MODE_NOR: {
		const struct k3_nor_boot_target *boot_prio;
		unsigned int prio_count;
		u32 i;

		env_set("boot_device", "nor");
		/*
		 * NOR-boot: select external boot device by bootfs presence, with
		 * priority UFS(SCSI) -> SSD(NVMe) -> eMMC(MMC) -> USB(UMS).
		 */
		boot_prio = k3_nor_get_boot_prio(&prio_count);
		for (i = 0; i < prio_count; i++) {
			u32 dev = env_get_u32_default(boot_prio[i].devnum_env,
						      boot_prio[i].devnum_default);
			struct blk_desc *desc;

			desc = k3_nor_get_desc_by_target(&boot_prio[i], dev);
			if (desc && desc->type != DEV_TYPE_UNKNOWN &&
			    k3_nor_has_bootfs(desc)) {
				env_set("boot_devname", boot_prio[i].blk_name);
				env_set("boot_devnum", simple_itoa(dev));
				break;
			}
		}
		break;
	}
	case BOOT_MODE_EMMC:
		env_set("boot_device", "mmc");
		env_set("boot_devnum", simple_itoa(MMC_DEV_EMMC));
		break;
	case BOOT_MODE_SD:
		env_set("boot_device", "mmc");
		env_set("boot_devnum", simple_itoa(MMC_DEV_SD));
		break;
	case BOOT_MODE_UFS:
		env_set("boot_device", "ufs");
		env_set("boot_devnum", "0");
		env_set("boot_devname", "scsi");
		break;
	case BOOT_MODE_USB:
		// for fastboot image download and run test
		env_set("bootcmd", USB_BOOT_COMMAND);
		break;
	default:
		env_set("boot_device", "");
		break;
	}
}

static bool k3_is_nfs_boot_path(void)
{
	const char *boot_device = env_get("boot_device");

	return boot_device && !strcmp(boot_device, "nfs");
}

static bool k3_is_net_flash_path(void)
{
	const char *net_flash_mode = env_get("net_flash_protocol");

	return net_flash_mode && !strcmp(net_flash_mode, "spacemit_tftp");
}

bool board_should_init_net(void)
{
	/*
	 * Keep normal local boot path lean. Network can still be initialized
	 * lazily by on-demand net commands.
	 */
	if (env_get_ulong("force_net_init", 10, 0))
		return true;

	if (k3_is_nfs_boot_path())
		return true;

	if (k3_is_net_flash_path())
		return true;

	return false;
}

/******************************************************************************
 * Load environment support function
 *******************************************************************************/
int mmc_get_env_dev(void)
{
	u32 boot_mode = get_boot_mode();
	pr_debug("%s, uboot boot_mode:%x\n", __func__, boot_mode);

	/* In USB mode, use hardware boot pin to determine target device */
	if (boot_mode == BOOT_MODE_USB)
		boot_mode = get_boot_pin_select();

	if (boot_mode == BOOT_MODE_EMMC)
		return MMC_DEV_EMMC;
	else
		return MMC_DEV_SD;
}

enum env_location env_get_location(enum env_operation op, int prio)
{
	if (prio >= 1)
		return ENVL_UNKNOWN;

	u32 boot_mode = get_boot_mode();
	/* In USB mode, use hardware boot pin to determine env location */
	if (boot_mode == BOOT_MODE_USB)
		boot_mode = get_boot_pin_select();

	switch (boot_mode) {
#if CONFIG_IS_ENABLED(ENV_IS_IN_MTD)
	case BOOT_MODE_NAND:
	case BOOT_MODE_NOR:
		return ENVL_MTD;
#endif
#if CONFIG_IS_ENABLED(ENV_IS_IN_MMC)
	case BOOT_MODE_EMMC:
	case BOOT_MODE_SD:
		return ENVL_MMC;
#endif
#if CONFIG_IS_ENABLED(ENV_IS_IN_UFS)
	case BOOT_MODE_UFS:
		return ENVL_UFS;
#endif
	default:
#if CONFIG_IS_ENABLED(ENV_IS_NOWHERE)
		return ENVL_NOWHERE;
#else
		return ENVL_UNKNOWN;
#endif
	}
}

static int _load_env_from_blk(struct blk_desc *dev_desc, const char *dev_name, int dev)
{
	int err;
	u32 part;
	char cmd[128];
	struct disk_partition info;

	// env "boot_devname" should not bind with "bootfs" partition
	env_set("boot_devname", dev_name);

	for (part = 1; part <= MAX_SEARCH_PARTITIONS; part++) {
		err = part_get_info(dev_desc, part, &info);
		if (err)
			continue;
		if (!strcmp(BOOTFS_NAME, info.name)){
			pr_debug("match info.name:%s\n", info.name);
			break;
		}
	}
	if (part > MAX_SEARCH_PARTITIONS)
		return -1;

	env_set("bootfs_part", simple_itoa(part));

	/*load env.txt and import to uboot*/
	memset((void *)CONFIG_FASTBOOT_BUF_ADDR, 0, CONFIG_ENV_SIZE);
	sprintf(cmd, "load %s %d:%d 0x%lx env_%s.txt", dev_name,
			dev, part, CONFIG_FASTBOOT_BUF_ADDR, CONFIG_SYS_CONFIG_NAME);
	pr_debug("cmd:%s\n", cmd);
	if (run_command(cmd, 0))
		return -1;

	memset(cmd, '\0', 128);
	sprintf(cmd, "env import -t 0x%lx", CONFIG_FASTBOOT_BUF_ADDR);
	pr_debug("cmd:%s\n", cmd);
	if (!run_command(cmd, 0)){
		pr_info("load env_%s.txt from bootfs successful\n", CONFIG_SYS_CONFIG_NAME);
		return 0;
	}
	return -1;
}

char* parse_mtdparts_and_find_bootfs(void) {
	const char *mtdparts = env_get("mtdparts");
	char cmd_buf[256];

	if (!mtdparts) {
		pr_debug("mtdparts not set\n");
		return NULL;
	}

	/* Find the last partition */
	const char *last_part_start = strrchr(mtdparts, '(');
	if (last_part_start) {
		last_part_start++; /* Skip the left parenthesis */
		const char *end = strchr(last_part_start, ')');
		if (end && (end - last_part_start < sizeof(found_partition))) {
			int len = end - last_part_start;
			strncpy(found_partition, last_part_start, len);
			found_partition[len] = '\0';

			snprintf(cmd_buf, sizeof(cmd_buf), "ubi part %s", found_partition);
			if (run_command(cmd_buf, 0) == 0) {
				/* Check if the bootfs volume exists */
				snprintf(cmd_buf, sizeof(cmd_buf), "ubi check %s", BOOTFS_NAME);
				if (run_command(cmd_buf, 0) == 0) {
					pr_info("Found bootfs in partition: %s\n", found_partition);
					return found_partition;
				}
			}
		}
	}

	pr_debug("bootfs not found in any partition\n");
	return NULL;
}

/* Load environment variables from NAND bootfs partition */
static int load_env_from_nand_bootfs(void)
{
#if CONFIG_IS_ENABLED(ENV_IS_IN_MTD)
	/*load env from nand bootfs*/
	const char *bootfs_name = BOOTFS_NAME ;
	char cmd[128];

	if (!bootfs_name) {
		pr_err("bootfs not set\n");
		return -1;
	}

	/* Parse mtdparts to find the partition containing the BOOTFS_NAME volume */
	char *mtd_partition   = parse_mtdparts_and_find_bootfs();
	if (!mtd_partition  ) {
		pr_err("Bootfs not found in any partition\n");
		return -1;
	}

	sprintf(cmd, "ubifsmount ubi0:%s", bootfs_name);
	if (run_command(cmd, 0)) {
		pr_err("Cannot mount ubifs partition '%s'\n", bootfs_name);
		return -1;
	}

	memset((void *)CONFIG_FASTBOOT_BUF_ADDR, 0, CONFIG_ENV_SIZE);
	sprintf(cmd, "ubifsload 0x%lx env_%s.txt", CONFIG_FASTBOOT_BUF_ADDR, CONFIG_SYS_CONFIG_NAME);
	if (run_command(cmd, 0)) {
		pr_err("Failed to load env_%s.txt from bootfs\n", CONFIG_SYS_CONFIG_NAME);
		return -1;
	}

	memset(cmd, '\0', 128);
	sprintf(cmd, "env import -t 0x%lx", CONFIG_FASTBOOT_BUF_ADDR);
	if (!run_command(cmd, 0)) {
		pr_debug("Imported environment from 'env_%s.txt'\n", CONFIG_SYS_CONFIG_NAME);
		return 0;
	}

	return -1;
#else
	pr_debug("ENV_IS_IN_MTD not enabled, skipping NAND bootfs env load\n");
	return 0;
#endif
}

void import_env_from_bootfs(void)
{
	u32 boot_mode = get_boot_mode();

#ifdef CONFIG_RSA_VERIFY
	/*
	 * In secure boot mode, do not load environment from external flash
	 * to prevent unauthorized environment modification
	 */
	pr_info("Secure boot enabled, skip loading environment from bootfs\n");
	return;
#endif

#ifdef CONFIG_BOOT_FROM_USB_DISK
	char *boot_device = env_get("boot_device");
	char cmd[128];

	if (boot_device && !strncmp(boot_device, "udisk", 5)) {
		memset(cmd, 0, 128);
		sprintf(cmd, "load usb %d:%d ${kernel_addr_r} env_k3.txt",
			udisk_dev_num, part_num);
		if (run_command(cmd, 0)) {
			pr_info("run command: %s failed\n", cmd);
			return;
		}
		memset(cmd, 0, 128);
		sprintf(cmd, "env import -t ${kernel_addr_r} ${filesize}");
		if (run_command(cmd, 0))
			pr_info("run command: %s failed\n", cmd);

		pr_info("successfully imported env from udisk's bootfs\n");
		return;
	}
#endif

#ifdef CONFIG_ENV_IS_IN_NFS
	// Check if local bootfs exists
	if ((BOOT_MODE_USB != boot_mode) && check_bootfs_exists() != 0) {
		#ifdef CONFIG_CMD_NET
			eth_initialize();
		#endif
		// Local bootfs not found, try to load from NFS
		if (load_env_from_nfs() == 0) {
			return;
		}
	}
#endif

	switch (boot_mode) {
	case BOOT_MODE_NAND:
		load_env_from_nand_bootfs();
		break;
	case BOOT_MODE_NOR: {
		const struct k3_nor_boot_target *boot_prio;
		unsigned int prio_count;
		u32 i;

		/*
		 * NOR-boot boards: prefer loading env from external bootfs with
		 * explicit priority: UFS -> SSD(NVMe) -> eMMC -> USB.
		 */
		boot_prio = k3_nor_get_boot_prio(&prio_count);
		for (i = 0; i < prio_count; i++) {
			u32 dev = env_get_u32_default(boot_prio[i].devnum_env,
						      boot_prio[i].devnum_default);
			struct blk_desc *desc;

			desc = k3_nor_get_desc_by_target(&boot_prio[i], dev);
			if (desc && desc->type != DEV_TYPE_UNKNOWN) {
				if (!_load_env_from_blk(desc, boot_prio[i].blk_name, dev))
					return;
			}
		}
		break;
	}
	case BOOT_MODE_EMMC:
	case BOOT_MODE_SD:
#ifdef CONFIG_MMC
		int dev;
		struct mmc *mmc;

		dev = mmc_get_env_dev();
		mmc = find_mmc_device(dev);
		if (!mmc) {
			pr_err("Cannot find mmc device\n");
			return;
		}
		if (mmc_init(mmc)){
			return;
		}

		_load_env_from_blk(mmc_get_blk_desc(mmc), "mmc", dev);
		break;
#endif
	case BOOT_MODE_UFS:
#ifdef CONFIG_SCSI
		struct blk_desc *ufs_dev_desc;

		board_scsi_scan_once(false);
		ufs_dev_desc = blk_get_dev("scsi", 0);
		if (ufs_dev_desc)
			_load_env_from_blk(ufs_dev_desc, "scsi", 0);
		break;
#endif
	default:
		break;
	}
	return;
}

static void increase_eth_addr(uint8_t *mac_addr)
{
	mac_addr[5]++;
	if (0 == mac_addr[5]) {
		mac_addr[4]++;
		if (0 == mac_addr[4]) {
			mac_addr[3]++;
		}
	}
}

int read_mac_from_tlv(void)
{
	unsigned int i;
	uint32_t mac_size;
	u8 macbase[6];
	int maccount;

	maccount = 1;
	if (get_tlvinfo(TLV_CODE_MAC_SIZE, (char*)&mac_size, 2) > 0) {
		maccount = be16_to_cpu(mac_size);
	}

	if ((get_tlvinfo(TLV_CODE_MAC_BASE, (char*)macbase, 6) <= 0)
		|| !is_valid_ethaddr(macbase)) {
		return 0;
	}

	for (i = 0; i < maccount; i++) {
		char ethaddr[18];
		char enetvar[11];

		sprintf(ethaddr, "%02X:%02X:%02X:%02X:%02X:%02X",
			macbase[0], macbase[1], macbase[2],
			macbase[3], macbase[4], macbase[5]);
		sprintf(enetvar, i ? "eth%daddr" : "ethaddr", i);
		/* Only initialize environment variables that are blank
			* (i.e. have not yet been set)
			*/
		if (!env_get(enetvar))
			env_set(enetvar, ethaddr);

		increase_eth_addr(macbase);
	}

	return maccount;
}

void set_env_ethaddr(void)
{
	uint8_t mac_addr[6];
	char mac_str[32];
	int i, maccount;

	/* Determine source of MAC address and attempt to read it */
	maccount = read_mac_from_tlv();
	if (maccount > 0) {
		pr_info("Found %d valid MAC addresses.\n", maccount);
		return;
	}

	/*if there is NO valid MAC address, create 4 random ethaddr */
	maccount = 4;
	pr_info("generate %d random ethaddr.\n", maccount);
	net_random_ethaddr(mac_addr);
	mac_addr[0] = 0xfe;
	mac_addr[1] = 0xfe;
	mac_addr[2] = 0xfe;

	/* save mac address to TLV */
	snprintf(mac_str, (sizeof(mac_str) - 1), "%02x:%02x:%02x:%02x:%02x:%02x",
		mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
	set_tlvinfo(TLV_CODE_MAC_BASE, mac_str);
	sprintf(mac_str, "%d", maccount);
	set_tlvinfo(TLV_CODE_MAC_SIZE, mac_str);
	flush_tlvinfo();

	/* write ethaddr to env */
	eth_env_set_enetaddr("ethaddr", mac_addr);
	for (i = 1; i < maccount; i++) {
		/* Increase the last byte of MAC address for each additional interface */
		increase_eth_addr(mac_addr);
		sprintf(mac_str, "eth%daddr", i);
		pr_info("Update env %s with mac address: %02x:%02x:%02x:%02x:%02x:%02x\n", mac_str,
			mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
		eth_env_set_enetaddr(mac_str, mac_addr);
	}
}

void refresh_config_info(void)
{
	char *strval = malloc(64);
	int i, num, ret;

	const struct code_desc_info {
		u8    m_code;
		u8    is_data;
		char *m_name;
	} info[] = {
		{ TLV_CODE_PRODUCT_NAME,   false, "product_name"},
		{ TLV_CODE_PART_NUMBER,    false, "part#"},
		{ TLV_CODE_SERIAL_NUMBER,  false, "serial#"},
		{ TLV_CODE_MANUF_DATE,     false, "manufacture_date"},
		{ TLV_CODE_MANUF_NAME,     false, "manufacturer"},
		{ TLV_CODE_WIFI_MAC_ADDR,  false, "wifi_addr"},
		{ TLV_CODE_BLUETOOTH_ADDR, false, "bt_addr"},
		{ TLV_CODE_DEVICE_VERSION, true,  "device_version"},
		{ TLV_CODE_SDK_VERSION,    true,  "sdk_version"},
		{ TLV_CODE_DDR_DATARATE,   true,  "ddr_datarate"},
		{ TLV_CODE_DDR_PARTNUMBER, false,  "ddr_partnumber"},
	};

	for (i = 0; i < ARRAY_SIZE(info); i++) {
		ret = get_tlvinfo(info[i].m_code, strval, 64 - 1);
		if (ret <= 0) {
			continue;
		}

		if (info[i].is_data) {
			num = 0;
			// Convert the numeric value to string
			for (int j = 0; j < ret && j < sizeof(num); j++) {
				num = (num << 8) | strval[j];
			}
			sprintf(strval, "%d", num);
		} else {
			strval[ret] = '\0';
		}
		pr_info("TLV item: %s = %s\n", info[i].m_name, strval);
		env_set(info[i].m_name, strval);
	}

	free(strval);
}

#if !defined(CONFIG_SPL_BUILD)
int board_fit_config_name_match(const char *name)
{
	char *product_name = env_get("product_name");

	if (NULL == product_name) {
		product_name = DEFAULT_PRODUCT_NAME;
	}

	if (0 == strcmp(product_name, name)) {
		log_info("Boot from fit configuration %s\n", name);
		return 0;
	}
	else
		return -1;
}
#endif

static int ft_board_cpu_fixup(void *blob, struct bd_info *bd)
{
#if CONFIG_IS_ENABLED(SPACEMIT_K1X_EFUSE)
	int node, ret;
	uint32_t product_id, wafer_tid;
	uint32_t dro = SVT_DRO_DEFAULT_VALUE;

	node = fdt_path_offset(blob, "/");
	if (node < 0) {
		pr_err("Can't find root node!\n");
		return -EINVAL;
	}

	get_chipinfo_from_efuse(&product_id, &wafer_tid);
	product_id = cpu_to_fdt32(product_id);
	wafer_tid = cpu_to_fdt32(wafer_tid);
	fdt_setprop(blob, node, "product-id", &product_id, sizeof(product_id));
	fdt_setprop(blob, node, "wafer-id", &wafer_tid, sizeof(wafer_tid));

	node = fdt_path_offset(blob, "/cpus");
	if (node < 0) {
		pr_err("Can't find cpus node!\n");
		return -EINVAL;
	}

	get_dro_from_efuse(&dro);
	dro = cpu_to_fdt32(dro);
	ret = fdt_setprop(blob, node, "svt-dro", &dro, sizeof(dro));
	if (ret < 0)
		return ret;
#endif
	return 0;
}

static int ft_board_info_fixup(void *blob, struct bd_info *bd)
{
	int node;
	const char *part_number;

	node = fdt_path_offset(blob, "/");
	if (node < 0) {
		pr_err("Can't find root node!\n");
		return -EINVAL;
	}

	part_number = env_get("part#");
	if (NULL != part_number)
		fdt_setprop(blob, node, "part-number", part_number, strlen(part_number));

	return 0;
}

static int ft_board_mac_addr_fixup(void *blob, struct bd_info *bd)
{
	int node, i;
	const char *addr_value;
	// char addr_str[ARP_HLEN_ASCII + 1];
	const char *mac_item[] = {"wifi_addr", "bt_addr"};

	node = fdt_path_offset(blob, "/soc");
	if (node < 0) {
		pr_err("Can't find soc node!\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(mac_item); i++) {
		addr_value = env_get(mac_item[i]);
		if (NULL != addr_value) {
			// memset(addr_str, 0, sizeof(addr_str));
			// sprintf(addr_str, "%pM", addr_value);
			fdt_setprop(blob, node, mac_item[i], addr_value, strlen(addr_value));
		}
	}

	return 0;
}

int ft_board_setup(void *blob, struct bd_info *bd)
{
	__maybe_unused struct fdt_memory mem;

	if (CONFIG_IS_ENABLED(FDT_SIMPLEFB)) {
		/* reserved with no-map tag the video buffer */
		mem.start = gd->video_bottom;
		mem.end = gd->video_top - 1;

		fdtdec_add_reserved_memory(blob, "framebuffer", &mem, NULL, 0, NULL, 0);
	}

	ft_board_cpu_fixup(blob, bd);
	ft_board_info_fixup(blob, bd);
	ft_board_mac_addr_fixup(blob, bd);
	return 0;
}

static void update_boot_mode_to_bootargs(void)
{
	uint32_t boot_mode;
	char *boot_mode_str, *new_boot_args;
	const char *boot_args;

	/* when boot from sdcard, can NOT get its boot mode from strap pin */
	boot_mode = get_boot_mode();
	if (BOOT_MODE_SD != boot_mode)
		boot_mode = get_boot_pin_select();

	switch (boot_mode) {
	case BOOT_MODE_EMMC:
		boot_mode_str = "boot_mode=emmc";
		break;
	case BOOT_MODE_NOR:
		boot_mode_str = "boot_mode=nor";
		break;
	case BOOT_MODE_NAND:
		boot_mode_str = "boot_mode=nand";
		break;
	case BOOT_MODE_UFS:
		boot_mode_str = "boot_mode=ufs";
		break;
	default:
		boot_mode_str = "boot_mode=sdcard";
		break;
    }

	boot_args = env_get("bootargs");
	if (NULL == boot_args) {
		new_boot_args = calloc(1, 32);
	} else {
		new_boot_args = calloc(1, strlen(boot_args) + 32);
		strcpy(new_boot_args, boot_args);
	}
	strcat(new_boot_args, " ");
	strcat(new_boot_args, boot_mode_str);
	env_set("bootargs", new_boot_args);
	free(new_boot_args);
}

static bool has_bootarg(const char *args, const char *param, size_t param_len)
{
	const char *p = args;

	if (!args || !param || !param_len)
		return false;

	// Iterate through all parameters in args
	while (*p) {
		// Skip spaces
		while (*p == ' ')
			p++;
		if (!*p)
			break;

		// Check if current parameter matches
		if (strncmp(p, param, param_len) == 0 &&
			(p[param_len] == '\0' || p[param_len] == ' ' || p[param_len] == '=')) {
			return true;
		}

		// Move to next parameter
		while (*p && *p != ' ')
			p++;
	}
	return false;
}

char *board_fdt_chosen_bootargs(void)
{
	const void *fdt;
	const char *env_args;
	const char *dts_args = NULL;
	char *merged = NULL;
	int nodeoffset;

	update_boot_mode_to_bootargs();

	env_args = env_get("bootargs");
	fdt = (void *)env_get_hex("fdt_addr", 0);
	if (!fdt) {
		return (char *)env_args;
	}

	if (fdt_check_header(fdt)) {
		pr_err("Invalid kernel DTB\n");
		return (char *)env_args;
	}

	nodeoffset = fdt_path_offset(fdt, "/chosen");
	if (nodeoffset >= 0)
		dts_args = fdt_getprop(fdt, nodeoffset, "bootargs", NULL);

	// Print env bootargs
	pr_debug("Env bootargs:\n    %s\n", env_args ? env_args : "NULL");
	// Print DTS bootargs
	pr_debug("DTS bootargs:\n    %s\n", dts_args ? dts_args : "NULL");

	if (!dts_args)
		return (char *)env_args;

	size_t total_len = 1;
	if (env_args)
		total_len += strlen(env_args);
	if (dts_args)
		total_len += strlen(dts_args) + 1;

	merged = calloc(1, total_len);
	if (!merged) {
		pr_err("Memory allocation failed\n");
		return NULL;
	}

	if (env_args)
		strcpy(merged, env_args);

	const char *p = dts_args;
	bool need_space = (merged[0] != '\0');

	while (p && *p) {
		while (*p && *p == ' ')
			p++;
		if (!*p)
			break;

		const char *end = p;
		while (*end && *end != ' ')
			end++;

		size_t param_len;
		const char *eq = memchr(p, '=', end - p);
		param_len = eq ? (size_t)(eq - p) : (size_t)(end - p);

		if (!has_bootarg(env_args, p, param_len)) {
			if (need_space)
				strcat(merged, " ");
			strncat(merged, p, end - p);
			need_space = true;
		}

		p = end;
	}

	if (!merged[0]) {
		free(merged);
		merged = NULL;
	}

	return merged;
}

#ifdef CONFIG_DDR_TRAINING_UPDATE_FSBL
static int find_first_diff_offset(const uint64_t *buf1, const uint64_t *buf2, size_t len)
{
	int i;

	for (i = 0; i < len / sizeof(uint64_t); i++) {
		if (buf1[i] != buf2[i]) {
			return i * sizeof(uint64_t);
		}
	}

	return len;
}

void flash_pre_process(char *partition, void *data_buffer)
{
	int i;
	struct fsbl_payload_t *payload;

	/* Check if this is FSBL partition and update its data before flash:
	1. update training info data to image
	2. update crc of the image payload
	*/
	if (0 == strcmp(partition, "fsbl")) {
		i = find_first_diff_offset((const uint64_t *)SRAM_BASE_ADDR,
			(const uint64_t *)data_buffer, SRAM_TOTAL_SIZE);
		// check if is the same version of fsbl, fsbl has 0x1000 byte at least
		if (i > 0x1000) {
			payload = (struct fsbl_payload_t *)(data_buffer + FSBL_PAYLOAD_OFFSET);
			if ((i + sizeof(struct ddr_info_t)) <= payload->imgsize) {
				pr_info("update ddr training info to fsbl @0x%x\n", i);
				memcpy(data_buffer + i, (const void *)SRAM_BASE_ADDR + i,
					sizeof(struct ddr_info_t));
				// update crc32
				payload->code_crc = crc32(0, payload->code, payload->imgsize);
			}
		}
	}
}
#endif
