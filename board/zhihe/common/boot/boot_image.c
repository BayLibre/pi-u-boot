// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

//#define DEBUG
#include <log.h>
#include <image.h>
#include <memalign.h>
#include <mapmem.h>
#include <spl.h>
#include <sysinfo.h>
#include <ext4fs.h>
#include "../include/boot.h"
#include "../include/board_porting.h"

#define MAX_UBOOT_SIZE ((1024 + 512) * 1024)
#define SKIP_FIND_OFFSET (512 * 1024)

/**********************************
 * Support spl fixup uboot fdt
 **********************************/
/*
 * Find uboot fdt blob
 */
void *spl_find_uboot_fdt_blob(void)
{
    int ret;
    const uint32_t *blob = (int *)ALIGN(CONFIG_TEXT_BASE, 4);

    /* FDT_MAGIC defined with big-endian mode. */
    uint32_t fdt_magic = fdt32_to_cpu(FDT_MAGIC);
    debug("cpu fdt magic 0x%x\n", fdt_magic);

    /* Find magic */
    for (int i = ALIGN(SKIP_FIND_OFFSET, 4) / 4; i < (MAX_UBOOT_SIZE / 4); i++) {
        if (blob[i] == fdt_magic) {
            ret = fdt_check_header(&blob[i]);
            debug("fdt check %d\n", ret);
            if (ret == 0) {
                return (void *)&blob[i];
            }
        }
    }

    return NULL;
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

 /* 
  * Reload the DTB file to override the content loaded by FIT.
  * This function must be called before booting OpenSBI.
  */
int spl_load_dtb_from_bootfs(void)
{
    int err = 0;
    ulong dtb_addr = 0;
    char dtb_filename_buf[64];
    char *dtb_file = NULL;
    struct mmc *mmc = NULL;

    /* Check dtb filename */
    dtb_file = spl_env_get_os_dtb(&dtb_addr);
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
    int partition = spl_env_get_mmc_bootfs_partid(); //get active slot
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
