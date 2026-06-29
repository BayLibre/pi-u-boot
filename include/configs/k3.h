/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2025-2026, Spacemit
 *
 * SpacemiT K3 (X100) board config header. Ported from the vendor U-Boot
 * 2022.10 tree alongside the SPL/DDR/FSBL support. Holds the SoC register
 * map used by the boot-mode strap detection, the TLV/EEPROM codes, the DDR
 * training save/restore layout (struct ddr_info_t) consumed by the K3 DDR
 * driver, the FSBL payload header and the default environment.
 */

#ifndef __SPACEMIT_K3_CONFIG_H
#define __SPACEMIT_K3_CONFIG_H

#include <linux/sizes.h>

#define CFG_SYS_NS16550_IER		0x40 /* UART Unit Enable */

#define PHY_ANEG_TIMEOUT		20000

#define SZ_1MB				0x00100000
#define SZ_2GB				0x80000000
#define SZ_4GB				0x100000000ULL
#define SZ_8GB				0x200000000ULL
#define SZ_16GB				0x400000000ULL
#define SEC_IMG_SIZE			0x2000000
#define CFG_SYS_SDRAM_BASE		(0x100000000ULL + SEC_IMG_SIZE)
#define SYS_SDRAM_UPPER_LIMIT_ADDR	(0x1100000000ULL)

#define RAMDISK_LOAD_ADDR		0x130000000
#define KERNEL_DTB_ADDR		0x138000000

#define PMIC_I2C_BUS			8

#define RISCV_MMODE_TIMERBASE		0xf1810000
#define RISCV_MMODE_TIMEROFF		0xbff8
#ifdef CONFIG_K3_BOARD_FPGA
#define RISCV_MMODE_TIMER_FREQ		5000000
#else
#define RISCV_MMODE_TIMER_FREQ		24000000
#endif
#define RISCV_SMODE_TIMER_FREQ		RISCV_MMODE_TIMER_FREQ
#define RISCV_TIMER_FREQ		(RISCV_SMODE_TIMER_FREQ)

#ifndef CONFIG_FASTBOOT_FLASH_MMC_DEV
#define CONFIG_FASTBOOT_FLASH_MMC_DEV	0
#endif

#define SRAM_BASE_ADDR		0xC0800000UL
#define SRAM_TOTAL_SIZE		(512 * 1024)

// sram buffer address that save the DDR software training result
#define DDR_TRAINING_INFO_BUFF		(0xC08D0000)
#define DDR_TRAINING_INFO_SAVE_ADDR	(0)
// magic string: "DDRT"
#define DDR_TRAINING_INFO_MAGIC		(0x54524444)
// ddr training software version: xx.xx.xxxx
#define DDR_TRAINING_INFO_VER		(0x00010000)
// default ddr channel number
#define DDR_CS_NUM			(1)

#define RECOVERY_LOAD_IMG_ADDR		(CONFIG_FASTBOOT_BUF_ADDR + CONFIG_FASTBOOT_BUF_SIZE)
#define RECOVERY_LOAD_IMG_SIZE		((unsigned long long)CONFIG_FASTBOOT_BUF_SIZE)

/* boot mode configs */
/* System boot control register */
#define BOOT_DEV_FLAG_REG		(0xD4282D10)
/* CIU debug registers (scratch) */
#define BOOT_CIU_REG			(0xD4282C00)
#define BOOT_CIU_DEBUG_REG0		(BOOT_CIU_REG + 0x0390)
#define BOOT_CIU_DEBUG_REG1		(BOOT_CIU_REG + 0x0394)
#define BOOT_CIU_DEBUG_REG2		(BOOT_CIU_REG + 0x0398)
/* Boot flag dummy register */
#define BOOT_PIN_SELECT			(0xD4282c20)
#define BOOT_STRAP_BIT_OFFSET		(9)
#define BOOT_STRAP_BIT_EMMC		(0x0)
#define BOOT_STRAP_BIT_NOR		(0x1)
#define BOOT_STRAP_BIT_NAND		(0x2)
#define BOOT_STRAP_BIT_UFS		(0x3)

/* Boot type mask and values */
#define BOOT_TYPE_MASK			0xfff

/* TLV code */
#define TLV_CODE_SDK_VERSION		0x40
#define TLV_CODE_DDR_CSNUM		0x41
#define TLV_CODE_DDR_TYPE		0x42
#define TLV_CODE_DDR_DATARATE		0x43
#define TLV_CODE_DDR_TX_ODT		0x44
#define TLV_CODE_DDR_PARTNUMBER		0x45

#define TLV_CODE_WIFI_MAC_ADDR		0x60
#define TLV_CODE_BLUETOOTH_ADDR		0x61
#define TLV_CODE_PMIC_TYPE		0x80
#define TLV_CODE_EEPROM_I2C_INDEX	0x81
#define TLV_CODE_EEPROM_PIN_GROUP	0x82
#define TLV_CODE_SECOND_BOOT_DEV	0x83

#if defined(CONFIG_XPL_BUILD)
#define MMC_DEV_EMMC			(1)
#else
#define MMC_DEV_EMMC			(2)
#endif
#define MMC_DEV_SD			(0)

#define K3_NOR_USB_DEVNUM_DEFAULT	(0)
#define K3_NOR_SSD_DEVNUM_DEFAULT	(0)
#define K3_NOR_UFS_DEVNUM_DEFAULT	(0)
#define K3_NOR_EMMC_DEVNUM_DEFAULT	(MMC_DEV_EMMC)

#define DEFAULT_PRODUCT_NAME		"k3_deb1"
#define BOOTFS_NAME			("bootfs")

// for those has NOT been through test procedure(ATE)
#define SVT_DRO_DEFAULT_VALUE		(205)

// non-volatile register in P1
#define P1_NON_VOLATILE_REG		(0xab)
#define P1_NON_VOLATILE_REG_FASTBOOT	(0x1)
#define P1_NON_VOLATILE_REG_MASK	(0x7)

#define USB_BOOT_COMMAND 		"bootm 0x140000000"

#define NOR_BOOT_PRIORITY_NODE		"/nor-boot-priority-helper"

#ifndef __ASSEMBLY__
#include "linux/types.h"

enum board_boot_mode {
	BOOT_MODE_NONE	= 0,
	BOOT_MODE_USB	= 0x55a,
	BOOT_MODE_SHELL	= 0x560,
	BOOT_MODE_UART	= 0x66b,
	BOOT_MODE_EMMC	= 0xb00,
	BOOT_MODE_NOR	= 0xb01,
	BOOT_MODE_NAND	= 0xb02,
	BOOT_MODE_UFS	= 0xb03,
	BOOT_MODE_SD	= 0xb10,
	BOOT_MODE_BOOTSTRAP,
};

enum k3_nor_boot_target_type {
	K3_NOR_BOOT_TARGET_UDISK = 0,
	K3_NOR_BOOT_TARGET_NVME,
	K3_NOR_BOOT_TARGET_SCSI,
	K3_NOR_BOOT_TARGET_MMC,
};

struct k3_nor_boot_target {
	enum k3_nor_boot_target_type type;
	const char *blk_name;
	const char *devnum_env;
	unsigned int devnum_default;
};

struct ddr_info_t {
	uint32_t magic;
	uint32_t crc32;
	uint64_t chipid;
	uint64_t mac_addr;
	uint32_t version;
	uint32_t type;
	uint32_t cs_num;
	uint32_t data_rate;
	uint8_t reserved[128 - 40];

	uint8_t training_info[0x9800 - 128];
};

#define FSBL_PAYLOAD_OFFSET		(0x1000 - 32)
struct fsbl_payload_t {
	uint32_t magic;
	uint8_t version;
	uint8_t secure;
	uint8_t reserved[2];
	uint64_t imgsize;
	uint64_t load_addr;
	uint32_t header_crc;
	uint32_t code_crc;

	uint8_t code[0];
};

const struct k3_nor_boot_target *k3_nor_get_boot_prio(unsigned int *count);
#endif

/* ****************************************************************************************
 * Environment
 * ***************************************************************************************/
#define CFG_EXTRA_ENV_SETTINGS \
	"kernel_comp_addr_r=" __stringify(CONFIG_SYS_LOAD_ADDR) "\0" \
	"kernel_comp_size=" __stringify(CONFIG_FASTBOOT_BUF_SIZE) "\0" \
	"kernel_addr_r=" __stringify(CONFIG_FASTBOOT_BUF_ADDR) "\0" \
	"ramdisk_addr_r=" __stringify(RAMDISK_LOAD_ADDR) "\0" \
	"fdt_addr_r=" __stringify(KERNEL_DTB_ADDR) "\0" \
	"pxefile_addr_r=" __stringify(CONFIG_FASTBOOT_BUF_ADDR) "\0" \
	"esos_itb_path=esos.itb\0" \
	"uboot_itb_path=u-boot.itb\0" \
	"extra_esos_partition=esos\0" \
	"extra_uboot_partition=uboot\0" \
	"opensbi_offset=0x700000\0" \
	"esos_offset=0x400000\0" \
	"uboot_offset=0x800000\0" \
	"splashimage=" __stringify(CONFIG_FASTBOOT_BUF_ADDR) "\0" \
	"splashpos=m,m\0" \
	"splashfile=bianbu.bmp\0" \
	"fastboot_raw_partition_bootinfo=0x0 0x20000\0" \
	"fastboot_raw_partition_fsbl=0x20000 0x80000\0" \
	"fastboot_raw_partition_env=0xa0000 0x10000\0" \
	"fastboot_raw_partition_esos=0xb0000 0x100000\0" \
	"fastboot_raw_partition_opensbi=0x1b0000 0x60000\0" \
	"fastboot_raw_partition_uboot=0x210000 0x5f0000\0" \
	"partitions=name=misc,size=1M;name=vbmeta_a,size=1M;name=vbmeta_b,size=1M;name=boot_a,size=40M;name=boot_b,size=40M;name=init_boot_a,size=8M;name=init_boot_b,size=8M;name=vendor_boot_a,size=32M;name=vendor_boot_b,size=32M;name=dtbo_a,size=8M;name=dtbo_b,size=8M;name=vbmeta_vendor_dlkm_a,size=64K;name=vbmeta_vendor_dlkm_b,size=64K;name=vbmeta_system_dlkm_a,size=64K;name=vbmeta_system_dlkm_b,size=64K;name=frp,size=512K;name=persist,size=32M;name=super,start=256M,size=4608M;name=metadata,size=64M;name=userdata,size=-\0"

#define CFG_ENV_FLAGS_LIST_STATIC "serial#:sa"

#endif /* __SPACEMIT_K3_CONFIG_H */
