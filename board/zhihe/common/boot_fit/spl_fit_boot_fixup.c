// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

//#define DEBUG
#include <errno.h>
#include <image.h>
#include <log.h>
#include <mapmem.h>
#include <spl.h>
#include <sysinfo.h>
#include <ext4fs.h>
#include "../include/boot.h"
#include "../include/pkg_header.h"

/******************************
 * Fixup Kernel boot
 * The fit boot information in SPL is passed to U-Boot via FDT.
 ******************************/
static int fit_image_check(const void *fit, const char *image_name, int image_noffset, void *os_fdt)
{
    uint8_t type, arch, os;
    size_t size;
    ulong load, entry;
    const void *data;
    int ret;

    /* get os type */
    ret = fit_image_get_os(fit, image_noffset, &os);
    if (ret) {
        return -1;
    }

    /* check uboot os */
    if (os != IH_OS_U_BOOT) {
        return -1;
    }

    /*
     * get image info 
     * the image has been successfully loaded, and no further checks are required here
     */
    fit_image_get_load(fit, image_noffset, &load);
    fit_image_get_entry(fit, image_noffset, &entry);
    fit_image_get_data_and_size(fit, image_noffset, &data, &size);
    fit_image_get_type(fit, image_noffset, &type);
    fit_image_get_arch(fit, image_noffset, &arch);

    /* alloc fdt write space */
    ret = fdt_shrink_to_minimum(os_fdt, 8192);
    if (ret < 0) {
        return -1;
    }

    /* write image info to system fdt */
    fdt_record_loadable(os_fdt, 0 /*invalid*/, image_name, load, size, entry,
                        genimg_get_type_name(type), genimg_get_os_name(os),
                        genimg_get_arch_name(arch));

    return 0;
}

/* 
 * Parse fit fdt get uboot image info and write to OS fdt
 */
static int spl_fdt_fixup(void *fit_header, void *os_fdt)
{
    const char *fit_uname_config = NULL;
    const char *uname;
    int idx, conf_noffset, noffset;
    int ret;

    /* get default configurations node */
    conf_noffset = fit_conf_get_node((const void *)fit_header, fit_uname_config);
    if (conf_noffset < 0)
        return 0;

    /* loop through the images in the loadables list */
    for (idx = 0; uname = fdt_stringlist_get((const void *)fit_header, conf_noffset,
                                             FIT_LOADABLE_PROP, idx, NULL),
        uname;
         idx++) {
        noffset = fit_image_get_node(fit_header, uname);

        /* check image */
        ret = fit_image_check(fit_header, uname, noffset, os_fdt);

        if (ret == 0)
            break;
    }

    return 0;
}

/**********************************
 * Support dtb reload from BootFS
 **********************************/
 /* 
  * Overlay boot mode, support boot from emmcboot only
  * weak function u-boot/common/spl/spl_mmc.c
  * riscv-boot.itb in EMMCBOOT
  * kernel & dtb in BootFS 
  */
u32 spl_mmc_boot_mode(struct mmc *mmc, const u32 boot_device)
{
    return MMCSD_MODE_EMMCBOOT;
}

static int spl_mmc_find_device(struct mmc **mmcp, int mmc_dev)
{
    int err;

    if (mmc_dev < 0)
        return mmc_dev;

    err = mmc_init_device(mmc_dev);
    if (err) {
        return err;
    }

    *mmcp = find_mmc_device(mmc_dev);
    err = *mmcp ? 0 : -ENODEV;
    if (err) {
        return err;
    }

    return 0;
}

static int reload_dtb_from_ext4fs(void)
{
    int err = 0;
    ulong dtb_addr = 0;
    char dtb_filename_buf[64];
    char *dtb_file = NULL;
    struct mmc *mmc = NULL;

    /* Check dtb filename */
    dtb_file = spl_get_osfdt_info(&dtb_addr);
    if (dtb_file == NULL) {
        sprintf(dtb_filename_buf, "%s.dtb", board_get_fit_dtb_name(1));
        dtb_file = dtb_filename_buf;
    }

    if (dtb_addr == 0) {
        printf("spl: dtb addr check fail, use default\n");
        return -1;
    }

    /* MMC Init */
    err = spl_mmc_find_device(&mmc, CONFIG_FASTBOOT_FLASH_MMC_DEV);
    if (err) {
        printf("spl: mmc %d not found\n", CONFIG_FASTBOOT_FLASH_MMC_DEV);
        return err;
    }

    err = mmc_init(mmc);
    if (err) {
        printf("spl: mmc init error\n");
        mmc = NULL;
        return err;
    }

    /* Switch to User Data hwpart */
    int hwpart = 0; /* 0:user data 1:boot0 2:boot1 */
    err = blk_dselect_hwpart(mmc_get_blk_desc(mmc), hwpart);
    if (err) {
        printf("spl: swith to hwpart %d error\n", hwpart);
        return err;
    }

    /* Mount ext4fs */
    int partition = spl_get_mmc_bootfs_partition(); //get active slot
    struct disk_partition part_info = {};

    struct blk_desc *block_dev = mmc_get_blk_desc(mmc);
    if (part_get_info(block_dev, partition, &part_info)) {
        printf("spl: no partition table found\n");
        return -1;
    }

    ext4fs_set_blk_dev(block_dev, &part_info);

    err = ext4fs_mount();
    if (!err) {
        printf("spl: ext4fs_mount failed\n");
        return -1;
    }

    /* Read dtb file */
    loff_t filelen;
    err = ext4fs_open(dtb_file, &filelen);
    if (err < 0) {
        printf("spl: ext4fs_open %s failed\n", dtb_file);
        return -1;
    }

    loff_t actlen;
    char *buf=(char*)dtb_addr;
    err = ext4fs_read(buf, 0, filelen, &actlen);
    if (err == 0) {
        printf("## Load %s to 0x%lx(%lld)\n", dtb_file, dtb_addr, actlen);
    } else {
        printf("spl: ext4fs_read failed\n");
    }
    ext4fs_close();

    return err;
}

/******************************
 * Main fixups
 ******************************/
/*
 * Get ddr start addres & size
 * Please implement this function in the
 * board-level code to override the weak implementation.
 */
__weak int board_get_ddr_info(u64 *start, u64 *size)
{
    return -1;
}

/*
 * Fix the issue where the full fit mode cannot access the next level of OS entry
 * spl_perform_fixups is weak imp at u-boot/common/spl/spl.c
 */
void spl_perform_fixups(struct spl_image_info *spl_image)
{
    u64 start;
    u64 size;

    /* reload dtb file */
    reload_dtb_from_ext4fs();

    void *fdt_uboot = spl_find_uboot_fdt_blob();
    if (!fdt_uboot) {
        return;
    }
    debug("uboot fdt blob 0x%p\n", fdt_uboot);

    /* 1. Add u-boot info to os fdt for opensbi can boot to u-boot */
    spl_fdt_fixup(map_sysmem(CONFIG_SYS_LOAD_ADDR, 0), spl_image->fdt_addr);

    /* 2. Fixup DDR size, write to u-boot fdt */
    if (board_get_ddr_info(&start, &size) == 0) {
        int ret = fdt_fixup_memory(fdt_uboot, start, size);
        debug("fixup mem ret %d\n", ret);
        if (ret) {
            printf("Warning: failed fixup memeory\n");
        }
    }

    /* 3. Set board type pass to u-boot */
    board_set_binfo_to_fdt(fdt_uboot);
}

#ifdef CONFIG_SPL_FIT_SIGNATURE
/* weak imp at common/spl/spl_fit.c */
int board_spl_fit_is_verify(void)
{
    struct zhihe_image_header *header = (struct zhihe_image_header *)(CONFIG_SPL_TEXT_BASE - ZHIHE_PKG_HEAD_SIZE);
    debug("Verify check: 0x%x 0x%x\n", header->magic, header->verify_next);
    if (header->magic == RVBL_MAGIC) {
        /* RVBL Header */
        return 0;
    } else if (header->magic == PKSE_MAGIC) {
        return header->verify_next ? 1 : 0;
    }
    return 1;
}
#endif
