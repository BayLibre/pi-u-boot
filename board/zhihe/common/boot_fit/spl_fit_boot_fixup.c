// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

//#define DEBUG
#include <log.h>
#include <errno.h>
#include <image.h>
#include <memalign.h>
#include <mapmem.h>
#include <spl.h>
#include <sysinfo.h>
#include "../include/board_boot.h"
#include "../include/board_check.h"
#include "../include/board_porting.h"

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
static int fit_os_fdt_fixup(void *fit_header, void *os_fdt)
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

/* Override weak imp at common/spl/spl_fit.c */
const char * board_get_fit_config(void)
{
	return spl_get_fit_dtb_name(1);
}


/******************************
 * Main fixups
 ******************************/
/*
 * Fix the issue where the full fit mode cannot access the next level of OS entry
 * spl_perform_fixups is weak imp at u-boot/common/spl/spl.c
 */
void spl_perform_fixups(struct spl_image_info *spl_image)
{
    u64 start;
    u64 size;

    /* reload dtb file */
    if (spl_boot_device() != BOOT_DEVICE_BOOTROM) {
        spl_load_dtb_from_bootfs();
    }

    /*
     * OS fdt fixup 
     */
    /* 1. Add u-boot info to kernel fdt for opensbi can boot to u-boot */
    fit_os_fdt_fixup(map_sysmem(CONFIG_SYS_LOAD_ADDR, 0), spl_image->fdt_addr);

    /* 2. Board user-define fdt fixup */
    if (spl_fixup_os_fdt(spl_image->fdt_addr) !=0 ) {
        printf("spl: Warning, failed fixup os fdt\n");
    }

    /*
     * U-Boot fdt fixup 
     */
    /* 1. Fixup DDR size, write to u-boot fdt */
    void *fdt_uboot = spl_find_uboot_fdt_blob();
    if (!fdt_uboot) {
        return;
    }
    debug("uboot fdt blob 0x%p\n", fdt_uboot);
    if (spl_get_ddr_info(&start, &size) == 0) {
        int ret = fdt_fixup_memory(fdt_uboot, start, size);
        debug("fixup mem ret %d\n", ret);
        if (ret) {
            printf("spl: Warning, failed fixup memeory\n");
        }
    }

    /* 2. Set board type pass to u-boot */
    spl_set_binfo_to_uboot_fdt(fdt_uboot);
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

int board_fit_each_image_post_load(const void *fit, int noffset, ulong loadaddr, ulong len)
{
    ALLOC_CACHE_ALIGN_BUFFER(uint8_t, hash_value, FIT_MAX_HASH_LEN);
    int hash_value_len;
    const char *algo;

    uint8_t *fit_hash_value;
    int fit_hash_value_len;
    int noffset_hash;

    if (loadaddr == 0) {
        return -1;
    }

    //printf("fit %p, noffset %d\n", fit, noffset);
    noffset_hash = fdt_subnode_offset(fit, noffset, "hash");
    if (noffset_hash < 0) {
        //printf("spl: Can't get hash property\n");
        return -1;
    }

    if (fit_image_hash_get_algo(fit, noffset_hash, &algo)) {
        printf("spl: Can't get hash algo property\n");
        return -1;
    }

    //printf("fit %p, noffset_hash %d\n", fit, noffset_hash);
    if (fit_image_hash_get_value(fit, noffset_hash, &fit_hash_value, &fit_hash_value_len)) {
        printf("spl: Can't get hash value property\n");
        return -1;
    }

    if (calculate_hash((void *)loadaddr, len, algo, hash_value, &hash_value_len)) {
        printf("spl: Unsupported hash algorithm\n");
        return -1;
    }

    printf("     Uncompress size: %ld\n", len);
    printf("     Hash:            ");
    for (int i = 0; i < hash_value_len; i++) {
        printf("%02x", hash_value[i]);
    }
    printf("\n");

    // if (hash_value_len != fit_hash_value_len) {
    //     printf("spl: Bad hash value len\n");
    //     return -1;
    // } else if (memcmp(hash_value, fit_hash_value, hash_value_len) != 0) {
    //     printf("spl: Bad hash value\n");
    //     return -1;
    // }
    return 0;
}
