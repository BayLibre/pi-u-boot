// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2025 Spacemit, Inc
 * Board extra FIT loader: load additional ITB files from MTD partitions
 */

#include <image.h>
#include <log.h>
#include <spl.h>
#include <asm/global_data.h>
#include <cpu_func.h>
#include <mtd.h>
#include <linux/err.h>
#include <env.h>
#include <mapmem.h>
#include <linux/libfdt.h>
#include <linux/string.h>
#include <vsprintf.h>
#include <part.h>
#include <fs.h>
#include <asm/io.h>

#ifdef CONFIG_SPL_MMC
#include <mmc.h>
#include <blk.h>
#endif

#ifdef CONFIG_SPL_UFS_SUPPORT
#include <ufs.h>
#include <scsi.h>
#include <blk.h>
#endif

enum board_boot_mode get_boot_mode(void);
void spl_perform_fixups(struct spl_image_info *spl_image);

static void spl_extra_import_env(void)
{
#ifdef CONFIG_SPL_ENV_SUPPORT
	env_init();
	env_load();
#endif
}

static ulong spl_extra_env_offset(const char *name)
{
#ifdef CONFIG_SPL_ENV_SUPPORT
	return env_get_ulong(name, 16, 0);
#else
	return 0;
#endif
}

#ifdef CONFIG_SPL_MMC
/* ================= MMC support ================= */
static ulong spl_extra_mmc_read(struct spl_load_info *load, ulong sector, ulong count, void *buf)
{
	struct mmc *mmc = load->priv;
	struct blk_desc *bd = mmc_get_blk_desc(mmc);
	ulong n;

	pr_debug("%s: sector %lx, count %lx, buf %lx\n", __func__, sector, count, (ulong) buf);
	n = blk_dread(bd, sector, count, buf);
	return n == count ? count : 0;
}

static int extra_mmc_load_image(struct spl_image_info *spl_image, struct mmc *mmc,
				ulong start_lba)
{
	struct legacy_img_hdr *header;
	int err = 0;
	struct blk_desc *bd = mmc_get_blk_desc(mmc);
	ulong hdr_cnt = DIV_ROUND_UP(sizeof(*header), bd->blksz);

	header = spl_get_load_buffer(-sizeof(*header), sizeof(*header));
	if (blk_dread(bd, start_lba, hdr_cnt, header) != hdr_cnt)
		return -EIO;

	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) &&
	    image_get_magic(header) == FDT_MAGIC) {
		struct spl_load_info load;

		/* bl_len must be the block size in bytes for FIT loader */
		spl_load_init(&load, spl_extra_mmc_read, mmc, bd->blksz);

		err = spl_load_simple_fit(spl_image, &load, start_lba, header);
	} else {
		return -EINVAL;
	}

	return err;
}

static int select_mmc_user_part(struct mmc *mmc)
{
	int ret;

	if (CONFIG_IS_ENABLED(MMC_TINY))
		ret = mmc_switch_part(mmc, 0);
	else
		ret = blk_dselect_hwpart(mmc_get_blk_desc(mmc), 0);

	if (ret)
		pr_debug("MMC: switch to user part failed (%d)\n", ret);

	return ret;
}

/* Helper to get an MMC device based on boot mode */
static struct mmc *get_default_mmc_device(void)
{
	struct mmc *mmc;
	int mmc_dev_index;
	enum board_boot_mode boot_mode;

	/* Get current boot mode to determine MMC device index */
	boot_mode = get_boot_mode();

	/* Map boot mode to MMC device index */
	switch (boot_mode) {
	case BOOT_MODE_SD:
		mmc_dev_index = 0; /* SD card is typically MMC device 1 */
		break;
	case BOOT_MODE_EMMC:
		mmc_dev_index = 1; /* eMMC is typically MMC device 0 */
		break;
	default:
		mmc_dev_index = 0;
		break;
	}

	/* Use only the boot mode specific device */
	mmc = find_mmc_device(mmc_dev_index);
	if (mmc) {
		if (!mmc_init(mmc)) {
			if (select_mmc_user_part(mmc))
				return NULL;
			return mmc;
		}
	}
	return NULL;
}

/* Unified MMC loader using built-in defaults
 * image: "esos" or "uboot"; selects default LBA by compile-time macros
 * out_entry: only meaningful when image == "uboot"
 */
static int load_fit_from_mmc(struct spl_image_info *caller_spl_image, const char *image,
			     ulong *out_entry)
{
	int ret = -1;
	ulong lba = 0;
	struct mmc *mmc = get_default_mmc_device();
	struct blk_desc *bd = mmc ? mmc_get_blk_desc(mmc) : NULL;
	struct spl_image_info temp = { 0 };

	if (!bd)
		return -ENODEV;

	/* Strategy:
	 * 1) If 'image' is decimal digits -> treat as raw LBA
	 * 2) Else, treat 'image' as partition name -> scan partitions and use info.start
	 * 3) Fallback to legacy compile-time LBA macros (uboot/esos)
	 */

	if (image && *image) {
		bool all_digit = true;
		const char *p = image;
		while (*p) {
			if (*p < '0' || *p > '9') {
				all_digit = false;
				break;
			}
			p++;
		}

		if (all_digit) {
			/* numeric string -> raw LBA */
			lba = simple_strtoul(image, NULL, 10);
			pr_debug("MMC: interpret '%s' as LBA=%lu\n", image, lba);
			pr_debug("MMC: load FIT @ LBA 0x%lx (numeric)\n", lba);
			ret = extra_mmc_load_image(&temp, mmc, lba);
		} else {
			/* treat as partition name and scan */
			struct disk_partition info;
			int found = 0;
			for (int part = 1; part <= MAX_SEARCH_PARTITIONS; part++) {
				int pe = part_get_info(bd, part, &info);
				if (pe)
					continue;
				pr_debug("MMC: scan p=%d name=%s start=%lu size=%lu\n", part,
					 info.name, info.start, info.size);
				if (!strcmp(image, info.name)) {
					lba = info.start;
					found = 1;
					break;
				}
			}
			if (found) {
				pr_debug("MMC: found partition '%s', start LBA=0x%lx\n", image,
					 lba);
				ret = extra_mmc_load_image(&temp, mmc, lba);
			} else {
				pr_debug("MMC: partition '%s' not found, will fallback to default "
					 "LBA\n",
					 image);
			}
		}
	}

	/* No compile-time fallback: if neither numeric LBA nor partition name matched,
	 * keep ret as error and let caller handle it.
	 */

	pr_debug("extra_mmc_load_image ret:%d\n", ret);
	if (!ret) {
		if (temp.fdt_addr) {
			caller_spl_image->fdt_addr = temp.fdt_addr;
			pr_debug("Set DTB from loaded FIT @ %p\n", temp.fdt_addr);
		}
		if (out_entry && image && !strcmp(image, "uboot")) {
			if (temp.entry_point)
				*out_entry = temp.entry_point;
			else
				*out_entry = CONFIG_TEXT_BASE;
			pr_debug("Loaded entry: 0x%lx\n", *out_entry);
		}
	}
	return ret;
}

static int load_fit_from_mmc_offset(struct spl_image_info *caller_spl_image,
				    ulong offset, ulong *out_entry)
{
	struct mmc *mmc = get_default_mmc_device();
	struct blk_desc *bd = mmc ? mmc_get_blk_desc(mmc) : NULL;
	struct spl_image_info temp = { 0 };
	ulong lba;
	int ret;

	if (!bd)
		return -ENODEV;
	if (!offset)
		return -EINVAL;
	if (offset % bd->blksz)
		return -EINVAL;

	lba = offset / bd->blksz;
	ret = extra_mmc_load_image(&temp, mmc, lba);
	if (!ret) {
		if (temp.fdt_addr)
			caller_spl_image->fdt_addr = temp.fdt_addr;

		if (out_entry && temp.entry_point)
			*out_entry = temp.entry_point;
	}

	return ret;
}
#endif /* CONFIG_SPL_MMC */

#ifdef CONFIG_SPL_UFS_SUPPORT
/* ================= UFS support ================= */
static ulong spl_extra_ufs_read(struct spl_load_info *load, ulong sector, ulong count, void *buf)
{
	struct blk_desc *bd = load->priv;
	ulong n;

	pr_debug("%s: sector %lx, count %lx, buf %lx\n", __func__, sector, count, (ulong) buf);
	n = blk_dread(bd, sector, count, buf);
	return n == count ? count : 0;
}

static int extra_ufs_load_image(struct spl_image_info *spl_image, struct blk_desc *bd,
				ulong start_lba)
{
	struct legacy_img_hdr *header;
	int err = 0;
	ulong hdr_cnt = DIV_ROUND_UP(sizeof(*header), bd->blksz);

	header = spl_get_load_buffer(-sizeof(*header), sizeof(*header));

	if (blk_dread(bd, start_lba, hdr_cnt, header) != hdr_cnt) {
		pr_err("UFS: read header failed\n");
		return -EIO;
	}

	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) && image_get_magic(header) == FDT_MAGIC) {
		struct spl_load_info load;
		pr_debug("UFS: found FIT at LBA 0x%lx\n", start_lba);
		/* bl_len must be the block size in bytes for FIT loader */
		spl_load_init(&load, spl_extra_ufs_read, bd, bd->blksz);
		err = spl_load_simple_fit(spl_image, &load, start_lba, header);
	} else {
		pr_err("UFS: unsupported legacy image\n");
		return -EINVAL;
	}
	return err;
}

/* Helper to get UFS/SCSI block device */
static struct blk_desc *get_ufs_blk_desc(void)
{
	struct blk_desc *bd;

	/* UFS devices appear as SCSI block devices */
	bd = blk_get_devnum_by_uclass_id(UCLASS_SCSI, 0);
	if (bd) {
		pr_debug("UFS: got SCSI block device, blksz=%lu, lba=%lu\n",
			 bd->blksz, bd->lba);
		return bd;
	}

	pr_err("UFS: no SCSI block device available\n");
	return NULL;
}

/* Unified UFS loader using partition name
 * image: partition name like "esos" or "uboot"
 * out_entry: only meaningful when image == "uboot"
 */
static int __maybe_unused load_fit_from_ufs(struct spl_image_info *caller_spl_image, const char *image,
			     ulong *out_entry)
{
	int ret = -1;
	ulong lba = 0;
	struct blk_desc *bd = get_ufs_blk_desc();
	struct spl_image_info temp = { 0 };

	if (!bd)
		return -ENODEV;

	/* Strategy:
	 * 1) If 'image' is decimal digits -> treat as raw LBA
	 * 2) Else, treat 'image' as partition name -> scan partitions and use info.start
	 */

	if (image && *image) {
		bool all_digit = true;
		const char *p = image;
		while (*p) {
			if (*p < '0' || *p > '9') {
				all_digit = false;
				break;
			}
			p++;
		}

		if (all_digit) {
			/* numeric string -> raw LBA */
			lba = simple_strtoul(image, NULL, 10);
			pr_debug("UFS: interpret '%s' as LBA=%lu\n", image, lba);
			ret = extra_ufs_load_image(&temp, bd, lba);
		} else {
			/* treat as partition name and scan */
			struct disk_partition info;
			int found = 0;
			for (int part = 1; part <= MAX_SEARCH_PARTITIONS; part++) {
				int pe = part_get_info(bd, part, &info);
				if (pe)
					continue;
				if (!strcmp(image, info.name)) {
					lba = info.start;
					found = 1;
					break;
				}
			}
			if (found) {
				pr_debug("UFS: found partition '%s' at LBA %lu\n", image, lba);
				ret = extra_ufs_load_image(&temp, bd, lba);
			} else {
				pr_debug("UFS: partition '%s' not found\n", image);
			}
		}
	}

	if (!ret) {
		if (temp.fdt_addr)
			caller_spl_image->fdt_addr = temp.fdt_addr;
		if (out_entry && image && !strcmp(image, "uboot")) {
			if (temp.entry_point)
				*out_entry = temp.entry_point;
			else
				*out_entry = CONFIG_TEXT_BASE;
		}
	}
	return ret;
}

static int load_fit_from_ufs_offset(struct spl_image_info *caller_spl_image, ulong offset,
				    ulong *out_entry)
{
	struct blk_desc *bd = get_ufs_blk_desc();
	struct spl_image_info temp = { 0 };
	ulong lba;
	int ret;

	if (!bd)
		return -ENODEV;
	if (!offset)
		return -EINVAL;
	if (offset % bd->blksz) {
		pr_err("UFS: unaligned offset 0x%lx (blksz 0x%lx)\n",
		       offset, (ulong)bd->blksz);
		return -EINVAL;
	}

	lba = offset / bd->blksz;
	ret = extra_ufs_load_image(&temp, bd, lba);
	if (!ret) {
		if (temp.fdt_addr)
			caller_spl_image->fdt_addr = temp.fdt_addr;
		if (out_entry && temp.entry_point)
			*out_entry = temp.entry_point;
	}
	return ret;
}
#endif /* CONFIG_SPL_UFS_SUPPORT */

#ifdef CONFIG_SPL_MTD_LOAD
static uint mtd_len_to_pages(struct mtd_info *mtd, u64 len)
{
	do_div(len, mtd->writesize);

	return len;
}

static bool mtd_is_aligned_with_min_io_size(struct mtd_info *mtd, u64 size)
{
	return !do_div(size, mtd->writesize);
}

static bool mtd_is_aligned_with_block_size(struct mtd_info *mtd, u64 size)
{
	return !do_div(size, mtd->erasesize);
}

int spl_extra_mtd_read(struct mtd_info *mtd, ulong sector, ulong count, void *buf)
{
	bool read = true, raw = false, woob = false, has_pages = false;
	u64 start_off, off, len, remaining;
	struct mtd_oob_ops io_op = {};
	u32 npages;
	int ret = -1;

	u8 *buffer = map_sysmem((u64) buf, 0);
	if (!buffer)
		return -1;

	pr_debug("mtd_read sector:%lx, count:%lx, buffer:%lx\n", sector, count, (ulong) buffer);
	start_off = sector;
	if (!mtd_is_aligned_with_min_io_size(mtd, start_off)) {
		pr_err("Offset not aligned with a page (0x%x)\n", mtd->writesize);
		return ret;
	}

	len = count;
	if (!mtd_is_aligned_with_min_io_size(mtd, len)) {
		len = round_up(len, mtd->writesize);
		pr_debug("Size not on a page boundary (0x%x), rounding to 0x%llx\n", mtd->writesize,
			 len);
	}
	if (mtd->type == MTD_NANDFLASH || mtd->type == MTD_MLCNANDFLASH)
		has_pages = true;

	remaining = len;
	npages = mtd_len_to_pages(mtd, len);

	io_op.mode = raw ? MTD_OPS_RAW : MTD_OPS_AUTO_OOB;
	io_op.len = has_pages ? mtd->writesize : len;
	io_op.ooblen = woob ? mtd->oobsize : 0;
	io_op.datbuf = buffer;
	io_op.oobbuf = woob ? &buffer[len] : NULL;

	/* Search for the first good block after the given offset */
	off = start_off;
	while (mtd_block_isbad(mtd, off))
		off += mtd->erasesize;

	/* Loop over the pages to do the actual read */
	while (remaining) {
		/* Skip the block if it is bad */
		if (mtd_is_aligned_with_block_size(mtd, off) && mtd_block_isbad(mtd, off)) {
			off += mtd->erasesize;
			continue;
		}

		ret = mtd_read_oob(mtd, off, &io_op);
		if (ret) {
			pr_err("Failure while %s at offset 0x%llx\n", read ? "reading" : "writing",
			       off);
			break;
		}

		off += io_op.retlen;
		remaining -= io_op.retlen;
		io_op.datbuf += io_op.retlen;
		io_op.oobbuf += io_op.oobretlen;
	}
	return ret;
}

static ulong spl_extra_load_read(struct spl_load_info *load, ulong sector, ulong count, void *buf)
{
	int ret;

	pr_debug("%s: sector %lx, count %lx, buf %lx\n", __func__, sector, count, (ulong) buf);

	struct mtd_info *mtd = load->priv;
	pr_debug("%s, get mtd:%p\n", __func__, mtd);
	ret = spl_extra_mtd_read(mtd, sector, count, buf);
	if (!ret)
		return count;
	else
		return 0;
}

static int extra_mtd_load_image(struct spl_image_info *spl_image, struct spl_boot_device *bootdev,
				struct mtd_info *mtd)
{
	struct legacy_img_hdr *header;
	ulong len;
	int err = 0;
	len = sizeof(*header);
	if (!mtd_is_aligned_with_min_io_size(mtd, len)) {
		len = round_up(len, mtd->writesize);
		pr_debug("Size not on a page boundary (0x%x), rounding to 0x%lx\n", mtd->writesize,
			 len);
	}

	header = spl_get_load_buffer(-sizeof(*header), sizeof(*header));
	err = spl_extra_mtd_read(mtd, 0, len, (void *) header);
	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) && image_get_magic(header) == FDT_MAGIC) {
		struct spl_load_info load;

		pr_debug("MTD: found FIT\n");
		spl_load_init(&load, spl_extra_load_read, mtd, 1);

		err = spl_load_simple_fit(spl_image, &load, 0, header);
	} else {
		pr_err("MTD: unsupported legacy image\n");
		return -1;
	}

	return err;
}

/* Unified MTD loader using partition name directly
 * part: partition name, typically "esos" or "uboot"
 * out_entry: only meaningful when part == "uboot"
 */
static int load_fit_from_mtd(struct spl_image_info *caller_spl_image, const char *part,
			     ulong *out_entry)
{
	struct mtd_info *mtd;
	int ret = -1;
	struct spl_image_info temp = { 0 };

	/* Directly use input parameter as partition name */

	pr_debug("MTD: load FIT from partition '%s'\n", part);
	mtd = get_mtd_device_nm(part);
	if (IS_ERR_OR_NULL(mtd)) {
		pr_err("MTD device %s not found\n", part);
		return -ENODEV;
	}

	pr_debug("MTD %s: mtd:%p erasesize:0x%x writesize:0x%x type:%d\n", part, mtd,
		 mtd->erasesize, mtd->writesize, mtd->type);
	ret = extra_mtd_load_image(&temp, NULL, mtd);
	pr_debug("extra_mtd_load_image ret:%d\n", ret);
	if (!ret) {
		if (temp.fdt_addr) {
			caller_spl_image->fdt_addr = temp.fdt_addr;
			pr_debug("Set DTB from loaded FIT @ %p\n", temp.fdt_addr);
		}
		if (out_entry && part && !strcmp(part, "uboot") && temp.entry_point) {
			*out_entry = temp.entry_point;
			pr_debug("Loaded entry: 0x%lx\n", *out_entry);
		}
	}
	return ret;
}

/* MTD loader using absolute offset
 * offset: absolute byte offset in MTD device
 * out_entry: only meaningful for uboot loading
 */
static int load_fit_from_mtd_offset(struct spl_image_info *caller_spl_image, ulong offset,
				    ulong *out_entry)
{
	struct mtd_info *mtd;
	int ret = -1;
	struct spl_image_info temp = { 0 };
	struct legacy_img_hdr *header;
	ulong len;

	/* Get the first available MTD device for absolute offset access */
	mtd = get_mtd_device(NULL, 0);
	if (IS_ERR_OR_NULL(mtd)) {
		pr_err("MTD device not found\n");
		return -ENODEV;
	}

	pr_debug("MTD: load FIT from offset 0x%lx\n", offset);
	pr_debug("MTD: mtd:%p erasesize:0x%x writesize:0x%x type:%d\n", mtd,
		 mtd->erasesize, mtd->writesize, mtd->type);

	/* Read header first */
	len = sizeof(*header);
	if (!mtd_is_aligned_with_min_io_size(mtd, len))
		len = round_up(len, mtd->writesize);

	header = spl_get_load_buffer(-sizeof(*header), sizeof(*header));
	ret = spl_extra_mtd_read(mtd, offset, len, (void *)header);
	if (ret) {
		pr_err("MTD: failed to read header at offset 0x%lx\n", offset);
		return ret;
	}

	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) && image_get_magic(header) == FDT_MAGIC) {
		struct spl_load_info load;

		pr_debug("MTD: found FIT at offset 0x%lx\n", offset);
		spl_load_init(&load, spl_extra_load_read, mtd, 1);

		ret = spl_load_simple_fit(&temp, &load, offset, header);
	} else {
		pr_err("MTD: unsupported legacy image at offset 0x%lx\n", offset);
		return -EINVAL;
	}

	pr_debug("load_fit_from_mtd_offset ret:%d\n", ret);
	if (!ret) {
		if (temp.fdt_addr) {
			caller_spl_image->fdt_addr = temp.fdt_addr;
			pr_debug("Set DTB from loaded FIT @ %p\n", temp.fdt_addr);
		}
		if (out_entry && temp.entry_point) {
			*out_entry = temp.entry_point;
			pr_debug("Loaded entry: 0x%lx\n", *out_entry);
		}
	}
	return ret;
}
#endif /* CONFIG_SPL_MTD_LOAD */

#if defined(CONFIG_SYS_BOOTLOADER_FS_PARTITION_NAME)
/**
 * Load firmware from bootloader file system partition
 * Loads fw_dynamic.itb and esos.itb FIT images, same as legacy mode
 *
 * @image:      SPL image info structure
 * @image_path: Image path and name.
 * @return: 0 on success, negative on error
 */
static int load_image_from_mmc_blfs(struct spl_image_info *image, const char *image_path)
{
	struct mmc *mmc;
	struct blk_desc *bd;
	struct disk_partition info;
	int ret = -1, part;
	const char *blfs_name;

	/* Get mmc block device */
	mmc = get_default_mmc_device();
	bd = mmc ? mmc_get_blk_desc(mmc) : NULL;
	if (!bd) {
		pr_err("BLFS: failed to get block device\n");
		return -ENODEV;
	}

	blfs_name = CONFIG_SYS_BOOTLOADER_FS_PARTITION_NAME;
	part = part_get_info_by_name(bd, blfs_name, &info);
	if (part < 0) {
		pr_err("Partition %s NOT exist\n", blfs_name);
		return -ENOENT;
	}

	memset(image, 0, sizeof(struct spl_image_info));
#ifdef CONFIG_SPL_FS_FAT
	// first try in FAT
	ret = spl_load_image_fat(image, NULL, bd, part, image_path);
#endif
#ifdef CONFIG_SPL_FS_EXT4
	// then try in EXT4
	if (ret)
		ret = spl_load_image_ext(image, NULL, bd, part, image_path);
#endif
	if (ret) {
		pr_err("BLFS: image(%s) load failed (non-fatal)\n", image_path);
	}

	return ret;
}

#ifdef CONFIG_SPL_UFS_SUPPORT
/**
 * Load firmware from UFS bootloader file system partition
 * Similar to MMC version but uses UFS block device
 *
 * @image:      SPL image info structure
 * @image_path: Image path and name.
 * @return: 0 on success, negative on error
 */
static int load_image_from_ufs_blfs(struct spl_image_info *image, const char *image_path)
{
	struct blk_desc *bd;
	struct disk_partition info;
	int ret = -1, part;
	const char *blfs_name;

	/* Get UFS block device */
	bd = get_ufs_blk_desc();
	if (!bd) {
		pr_err("UFS BLFS: failed to get block device\n");
		return -ENODEV;
	}

	blfs_name = CONFIG_SYS_BOOTLOADER_FS_PARTITION_NAME;
	part = part_get_info_by_name(bd, blfs_name, &info);
	if (part < 0) {
		pr_err("UFS BLFS: partition '%s' not found\n", blfs_name);
		return -ENOENT;
	}

	memset(image, 0, sizeof(struct spl_image_info));
#ifdef CONFIG_SPL_FS_FAT
	/* first try in FAT */
	ret = spl_load_image_fat(image, NULL, bd, part, image_path);
#endif
#ifdef CONFIG_SPL_FS_EXT4
	/* then try in EXT4 */
	if (ret)
		ret = spl_load_image_ext(image, NULL, bd, part, image_path);
#endif
	if (ret)
		pr_err("UFS BLFS: image(%s) load failed\n", image_path);

	return ret;
}
#endif /* CONFIG_SPL_UFS_SUPPORT */
#endif /* CONFIG_SYS_BOOTLOADER_FS_PARTITION_NAME */

int board_load_extra_fits(struct spl_image_info *spl_image, ulong *uboot_entry)
{
	/* Use board_boot_order to infer primary boot device */
	extern void board_boot_order(u32 * spl_boot_list);
	u32 spl_boot_list[2] = { 0 };
	int load_esos_res = -1, load_uboot_res = -1;
	ulong esos_off = 0, uboot_off = 0;

	board_boot_order(spl_boot_list);

	/*
	 * UFS path already imported env in SPL UFS loader. Avoid reloading here
	 * to skip duplicated UFS/SCSI initialization work.
	 */
#ifdef CONFIG_SPL_UFS_SUPPORT
	if (spl_boot_list[0] != BOOT_DEVICE_UFS)
#endif
		spl_extra_import_env();

	esos_off = spl_extra_env_offset("esos_offset");
	uboot_off = spl_extra_env_offset("uboot_offset");

	switch (spl_boot_list[0]) {
#ifdef CONFIG_SPL_MMC
	case BOOT_DEVICE_MMC1:
	case BOOT_DEVICE_MMC2:
	case BOOT_DEVICE_MMC2_2: {
#if defined(CONFIG_SYS_BOOTLOADER_FS_PARTITION_NAME)
		const char *blfs_mode_str;
		int blfs_load_mode = 0;
		bool blfs_load_failed = false;
		const char *uboot_itb_path, *esos_itb_path;
		struct spl_image_info image;

		/* Check if bootloader file system load mode is enabled, default to be enabled */
		blfs_mode_str = env_get("bootloader_from_fs");
		if (blfs_mode_str)
			blfs_load_mode = simple_strtol(blfs_mode_str, NULL, 10);
		else
			blfs_load_mode = 1;

		if (blfs_load_mode) {
			/* Get environment variables for file paths */
			/* Use blfs-specific env vars for file names (not partition names) */
			esos_itb_path = env_get("esos_itb_path");
			uboot_itb_path = env_get("uboot_itb_path");

			/* Fallback to hardcoded paths if env_get fails or returns wrong value */
			if (!uboot_itb_path) {
				uboot_itb_path = "u-boot.itb";
			}
			if (!esos_itb_path || !strcmp(esos_itb_path, uboot_itb_path)) {
				esos_itb_path = "esos.itb";
			}

			/* load firmware from bootloader file system, MUST load uboot at the last */
			if ((0 == load_image_from_mmc_blfs(&image, esos_itb_path)) &&
				(0 == load_image_from_mmc_blfs(&image, uboot_itb_path))) {
				load_esos_res = 0;
				load_uboot_res = 0;

				/* Copy DTB address to caller's spl_image (shared between opensbi and uboot) */
				if (image.fdt_addr)
					spl_image->fdt_addr = image.fdt_addr;

				/* Extract U-Boot entry point for opensbi to jump to */
				if (uboot_entry && image.entry_point)
					*uboot_entry = image.entry_point;
			} else {
				blfs_load_failed = true;
			}
		}

		/* Fallback to legacy mode if bootloader file system disabled or failed */
		if (blfs_load_failed)
#endif /* CONFIG_SYS_BOOTLOADER_FS_PARTITION_NAME */
		{
			/* Legacy mode: load FIT images from partitions */
			const char *tmp;
			char *part_esos = NULL, *part_uboot = NULL;
			pr_debug("MMC: using legacy FIT load mode\n");
			tmp = env_get("extra_esos_partition");
			if (tmp)
				part_esos = strdup(tmp);
			tmp = env_get("extra_uboot_partition");
			if (tmp)
				part_uboot = strdup(tmp);

			if (part_esos && *part_esos) {
				if (esos_off)
					load_esos_res = load_fit_from_mmc_offset(spl_image, esos_off, NULL);
				else
					load_esos_res = load_fit_from_mmc(spl_image, part_esos, NULL);
			} else {
				pr_debug("extra_esos_partition not set, skip MMC esos\n");
			}
			if (part_uboot && *part_uboot) {
				if (uboot_off)
					load_uboot_res = load_fit_from_mmc_offset(spl_image, uboot_off, uboot_entry);
				else
					load_uboot_res =
						load_fit_from_mmc(spl_image, part_uboot, uboot_entry);
			} else {
				pr_debug("extra_uboot_partition not set, skip MMC uboot\n");
			}

			if (part_esos)
				free(part_esos);
			if (part_uboot)
				free(part_uboot);
		}
		break;
	}
#endif
#ifdef CONFIG_SPL_MTD_LOAD
	case BOOT_DEVICE_NOR:
	case BOOT_DEVICE_NAND: {
		const char *tmp;
		char *part_esos = NULL, *part_uboot = NULL;
		ulong esos_off = 0, uboot_off = 0;

		tmp = env_get("extra_esos_partition");
		if (tmp)
			part_esos = strdup(tmp);
		tmp = env_get("extra_uboot_partition");
		if (tmp)
			part_uboot = strdup(tmp);

		/* Get offset as fallback if partition not set */
		esos_off = env_get_ulong("esos_offset", 16, 0);
		uboot_off = env_get_ulong("uboot_offset", 16, 0);

		mtd_probe_devices();

		/* Load esos: env partition -> default "esos" -> offset */
		if (part_esos && *part_esos) {
			load_esos_res = load_fit_from_mtd(
				spl_image, strcmp(part_esos, "1") == 0 ? "esos" : part_esos, NULL);
		} else {
			/* Try default partition name "esos" */
			struct mtd_info *mtd = get_mtd_device_nm("esos");
			if (!IS_ERR_OR_NULL(mtd)) {
				pr_debug("MTD: loading esos from default partition 'esos'\n");
				load_esos_res = load_fit_from_mtd(spl_image, "esos", NULL);
			} else if (esos_off) {
				pr_debug("MTD: loading esos from offset 0x%lx\n", esos_off);
				load_esos_res = load_fit_from_mtd_offset(spl_image, esos_off, NULL);
			} else {
				pr_debug("MTD: no esos partition or offset available\n");
			}
		}

		/* Load uboot: env partition -> default "uboot" -> offset */
		if (part_uboot && *part_uboot) {
			load_uboot_res = load_fit_from_mtd(
				spl_image, strcmp(part_uboot, "1") == 0 ? "uboot" : part_uboot,
				uboot_entry);
		} else {
			/* Try default partition name "uboot" */
			struct mtd_info *mtd = get_mtd_device_nm("uboot");
			if (!IS_ERR_OR_NULL(mtd)) {
				pr_debug("MTD: loading uboot from default partition 'uboot'\n");
				load_uboot_res = load_fit_from_mtd(spl_image, "uboot", uboot_entry);
			} else if (uboot_off) {
				pr_debug("MTD: loading uboot from offset 0x%lx\n", uboot_off);
				load_uboot_res = load_fit_from_mtd_offset(spl_image, uboot_off, uboot_entry);
			} else {
				pr_debug("MTD: no uboot partition or offset available\n");
			}
		}

		if (part_esos)
			free(part_esos);
		if (part_uboot)
			free(part_uboot);
		break;
	}
#endif
#ifdef CONFIG_SPL_UFS_SUPPORT
	case BOOT_DEVICE_UFS: {
#if defined(CONFIG_SYS_BOOTLOADER_FS_PARTITION_NAME)
		/* Try loading from bootloader filesystem partition (like MMC does) */
		const char *uboot_itb_path, *esos_itb_path;
		struct spl_image_info image;

		/* Get environment variables for file paths */
		esos_itb_path = env_get("esos_itb_path");
		uboot_itb_path = env_get("uboot_itb_path");

		/* Fallback to hardcoded paths if env_get fails */
		if (!uboot_itb_path)
			uboot_itb_path = "u-boot.itb";
		if (!esos_itb_path || !strcmp(esos_itb_path, uboot_itb_path))
			esos_itb_path = "esos.itb";

		/* Load esos.itb first */
		load_esos_res = load_image_from_ufs_blfs(&image, esos_itb_path);
		if (!load_esos_res && image.fdt_addr)
			spl_image->fdt_addr = image.fdt_addr;

		/* Load u-boot.itb - this sets the entry point */
		load_uboot_res = load_image_from_ufs_blfs(&image, uboot_itb_path);
		if (!load_uboot_res) {
			/* Copy DTB address if not already set */
			if (image.fdt_addr)
				spl_image->fdt_addr = image.fdt_addr;

			/* Extract U-Boot entry point */
			if (uboot_entry && image.entry_point)
				*uboot_entry = image.entry_point;
		}
#else
		/* Fallback: try raw partition loading */
		const char *tmp;
		char *part_esos = NULL, *part_uboot = NULL;

		tmp = env_get("extra_esos_partition");
		if (tmp)
			part_esos = strdup(tmp);
		else
			part_esos = strdup("esos");

		tmp = env_get("extra_uboot_partition");
		if (tmp)
			part_uboot = strdup(tmp);
		else
			part_uboot = strdup("uboot");

		if (part_esos && *part_esos) {
			if (esos_off)
				load_esos_res = load_fit_from_ufs_offset(spl_image, esos_off, NULL);
			else
				load_esos_res = load_fit_from_ufs(spl_image, part_esos, NULL);
		}
		if (part_uboot && *part_uboot) {
			if (uboot_off)
				load_uboot_res = load_fit_from_ufs_offset(spl_image, uboot_off, uboot_entry);
			else
				load_uboot_res = load_fit_from_ufs(spl_image, part_uboot, uboot_entry);
		}

		if (part_esos)
			free(part_esos);
		if (part_uboot)
			free(part_uboot);
#endif /* CONFIG_SYS_BOOTLOADER_FS_PARTITION_NAME */
		break;
	}
#endif
	default:
		pr_info("unknown boot device: %u\n", spl_boot_list[0]);
		break;
	}

	if (!load_uboot_res || !load_esos_res) {
		return 0;
	} else {
		pr_err("load failed: uboot=%d esos=%d\n", load_uboot_res, load_esos_res);
		return -1;
	}
}

#if defined(CONFIG_SPL_MTD_LOAD)
/*
 * Register the K3 MTD loader for NOR/NAND boot. board_load_extra_fits() picks
 * the device via board_boot_order() and loads u-boot.itb from the named MTD
 * partitions. The generic XIP spl_nor.c is not usable here (the QSPI is not
 * memory-mapped for SPL reads), so the FIT is read over the MTD/SPI path.
 */
static int spl_k3_mtd_load_image(struct spl_image_info *spl_image,
				 struct spl_boot_device *bootdev)
{
	ulong uboot_entry = 0;

	return board_load_extra_fits(spl_image, &uboot_entry);
}
SPL_LOAD_IMAGE_METHOD("K3-NOR", 0, BOOT_DEVICE_NOR, spl_k3_mtd_load_image);
SPL_LOAD_IMAGE_METHOD("K3-NAND", 0, BOOT_DEVICE_NAND, spl_k3_mtd_load_image);
#endif
