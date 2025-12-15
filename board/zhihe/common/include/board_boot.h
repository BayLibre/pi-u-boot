/*
 * Copyright(C) 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#ifndef __BOOT_IMAGE_H_
#define __BOOT_IMAGE_H_

#include <linux/types.h>

/* ZHIHE PKG Head */
#define ZHIHE_PKG_PUBKEY_HEAD_SIZE 4096
#define ZHIHE_PKG_HEAD_SIZE 2048
#define RVBL_MAGIC 0x4C425652
#define PKSE_MAGIC 0x45534B50
#define PUBKEYC_SIZE 0x280
#define SIGOFCODE_SIZE 0x100

struct __attribute__((packed)) zhihe_image_header {
    uint32_t boot_code;
    uint32_t magic;
    uint32_t image_size;
    union {
        uint32_t rvbl_payload_size; /* RVBL payload size */
        uint32_t pkse_version;      /* PKSE Header & Image Version */
    };
    uint32_t check_sum;             /* The following are all members of the PKSE header */
    uint64_t run_addr;
    uint8_t  reserved[11];
    uint8_t  verify_next;
};

/* boot_method */
u32 spl_boot_device(void);

/* boot_image */
int spl_load_dtb_from_bootfs(void);
void *spl_find_uboot_fdt_blob(void);

#endif
