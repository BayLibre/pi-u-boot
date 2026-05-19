/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * This is from the Android Project,
 * Repository: https://android.googlesource.com/platform/system/tools/mkbootimg
 * File: include/bootimg/bootimg.h
 * Commit: cce5b1923e3cd2fcb765b512610bdc5c42bc501d
 *
 * Copyright (C) 2007 The Android Open Source Project
 */

#ifndef _ANDROID_IMAGE_H_
#define _ANDROID_IMAGE_H_

#include <linux/compiler.h>
#include <linux/types.h>

#define ANDR_GKI_PAGE_SIZE 4096
#define ANDR_BOOT_MAGIC "ANDROID!"
#define ANDR_BOOT_MAGIC_SIZE 8
#define ANDR_BOOT_NAME_SIZE 16
#define ANDR_BOOT_ARGS_SIZE 512
#define ANDR_BOOT_EXTRA_ARGS_SIZE 1024
#define VENDOR_BOOT_MAGIC "VNDRBOOT"
#define ANDR_VENDOR_BOOT_V3_SIZE 2112
#define ANDR_VENDOR_BOOT_V4_SIZE 2128
#define ANDR_VENDOR_BOOT_MAGIC_SIZE 8
#define ANDR_VENDOR_BOOT_ARGS_SIZE 2048
#define ANDR_VENDOR_BOOT_NAME_SIZE 16

#define BOOTCONFIG_MAGIC "#BOOTCONFIG\n"
#define BOOTCONFIG_MAGIC_SIZE 12
#define BOOTCONFIG_SIZE_SIZE 4
#define BOOTCONFIG_CHECKSUM_SIZE 4
#define BOOTCONFIG_TRAILER_SIZE BOOTCONFIG_MAGIC_SIZE + \
				BOOTCONFIG_SIZE_SIZE + \
				BOOTCONFIG_CHECKSUM_SIZE

struct andr_boot_img_hdr_v3 {
	u8 magic[ANDR_BOOT_MAGIC_SIZE];

	u32 kernel_size;    /* size in bytes */
	u32 ramdisk_size;   /* size in bytes */

	u32 os_version;

	u32 header_size;    /* size of boot image header in bytes */
	u32 reserved[4];
	u32 header_version; /* offset remains constant for version check */

	u8 cmdline[ANDR_BOOT_ARGS_SIZE + ANDR_BOOT_EXTRA_ARGS_SIZE];
	/* for boot image header v4 only */
	u32 signature_size; /* size in bytes */
};

struct andr_vnd_boot_img_hdr {
	u8 magic[ANDR_VENDOR_BOOT_MAGIC_SIZE];
	u32 header_version;
	u32 page_size;           /* flash page size we assume */

	u32 kernel_addr;         /* physical load addr */
	u32 ramdisk_addr;        /* physical load addr */

	u32 vendor_ramdisk_size; /* size in bytes */

	u8 cmdline[ANDR_VENDOR_BOOT_ARGS_SIZE];

	u32 tags_addr;           /* physical addr for kernel tags */

	u8 name[ANDR_VENDOR_BOOT_NAME_SIZE]; /* asciiz product name */
	u32 header_size;         /* size of vendor boot image header in bytes */
	u32 dtb_size;            /* size of dtb image */
	u64 dtb_addr;            /* physical load address */
	/* for boot image header v4 only */
	u32 vendor_ramdisk_table_size;
	u32 vendor_ramdisk_table_entry_num;
	u32 vendor_ramdisk_table_entry_size;
	u32 bootconfig_size;
};

/* The bootloader expects the structure of andr_boot_img_hdr_v0 with header
 * version 0 to be as follows: */
struct andr_boot_img_hdr_v0 {
    /* Must be ANDR_BOOT_MAGIC. */
    char magic[ANDR_BOOT_MAGIC_SIZE];

    u32 kernel_size; /* size in bytes */
    u32 kernel_addr; /* physical load addr */

    u32 ramdisk_size; /* size in bytes */
    u32 ramdisk_addr; /* physical load addr */

    u32 second_size; /* size in bytes */
    u32 second_addr; /* physical load addr */

    u32 tags_addr; /* physical addr for kernel tags */
    u32 page_size; /* flash page size we assume */

    /* Version of the boot image header. */
    u32 header_version;

    /* Operating system version and security patch level.
     * For version "A.B.C" and patch level "Y-M-D":
     *   (7 bits for each of A, B, C; 7 bits for (Y-2000), 4 bits for M)
     *   os_version = A[31:25] B[24:18] C[17:11] (Y-2000)[10:4] M[3:0] */
    u32 os_version;

    char name[ANDR_BOOT_NAME_SIZE]; /* asciiz product name */

    char cmdline[ANDR_BOOT_ARGS_SIZE];

    u32 id[8]; /* timestamp / checksum / sha1 / etc */

    /* Supplemental command line data; kept here to maintain
     * binary compatibility with older versions of mkbootimg. */
    char extra_cmdline[ANDR_BOOT_EXTRA_ARGS_SIZE];

    /* Fields in boot_img_hdr_v1 and newer. */
    u32 recovery_dtbo_size;   /* size in bytes for recovery DTBO/ACPIO image */
    u64 recovery_dtbo_offset; /* offset to recovery dtbo/acpio in boot image */
    u32 header_size;

    /* Fields in boot_img_hdr_v2 and newer. */
    u32 dtb_size; /* size in bytes for DTB image */
    u64 dtb_addr; /* physical load address for DTB image */
} __attribute__((packed));

/*
 * 2022.10 compatibility: the v0 header struct was named andr_img_hdr.
 * Keep the old name usable so existing callers (image.h, image-fdt.c,
 * bootmeth_android.c, abootimg.c) continue to compile.
 */
#define andr_img_hdr andr_boot_img_hdr_v0

struct andr_image_data {
	ulong kernel_ptr;  /* kernel address */
	u32 kernel_size;  /* size in bytes */
	u32 ramdisk_size;  /* size in bytes */
	ulong vendor_ramdisk_ptr;  /* vendor ramdisk address */
	u32 vendor_ramdisk_size;  /* vendor ramdisk size */
	u32 boot_ramdisk_size;  /* size in bytes */
	ulong second_ptr;  /* secondary bootloader address */
	u32 second_size;  /* secondary bootloader size */
	ulong dtb_ptr;  /* address of dtb image */
	u32 dtb_size;  /* size of dtb image */
	ulong recovery_dtbo_ptr;  /* recovery DTBO/ACPIO address */
	u32 recovery_dtbo_size;  /* recovery DTBO/ACPIO size */

	const char *kcmdline;  /* boot kernel cmdline */
	const char *kcmdline_extra;  /* vendor-boot extra kernel cmdline */
	const char *image_name;  /* asciiz product name */

	ulong bootconfig_addr;  /* bootconfig image address */
	ulong bootconfig_size;  /* bootconfig image size */

	ulong kernel_addr;  /* physical load addr */
	ulong ramdisk_addr;  /* physical load addr */
	ulong ramdisk_ptr;  /* ramdisk address */
	ulong dtb_load_addr;  /* physical load address for DTB image */
	ulong tags_addr;  /* physical addr for kernel tags */
	u32 header_version;  /* version of the boot image header */
	u32 boot_img_total_size;  /* boot image size */
	u32 vendor_boot_img_total_size;  /* vendor boot image size */
};

bool is_android_boot_image_header(const void *hdr);
bool is_android_vendor_boot_image_header(const void *hdr);
bool android_image_get_bootimg_size(const void *hdr, u32 *boot_img_size);
bool android_image_get_vendor_bootimg_size(const void *hdr,
					   u32 *vendor_boot_img_size);
bool android_image_get_data(const void *boot_hdr, const void *vendor_boot_hdr,
			    struct andr_image_data *data);

ulong get_abootimg_addr(void);
void set_abootimg_addr(ulong addr);
ulong get_avendor_bootimg_addr(void);
void set_avendor_bootimg_addr(ulong addr);

#endif
