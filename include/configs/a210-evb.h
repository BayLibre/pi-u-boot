/*
 * (C) Copyright 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef __CONFIG_A210_EVB_H
#define __CONFIG_A210_EVB_H

/* ENV Flags */
#define CFG_ENV_FLAGS_LIST_STATIC "^nv_.*#$:so,"

#define EVN_COMMON \
	"tty_dev=ttyS4\0" \
	"kernel_loglevel=4\0" \
	"tmp_addr=0x8d000000\0" \
	"opensbi_addr=0x80000000\0" \
	"kernel_addr=0x80200000\0" \
	"dtb_addr=0x8c000000\0" \
	"initrd_addr=0x9e000000\0" \
	"aon_addr=0x308f8000\0" \
	"opensbi_file=fw_dynamic.bin\0" \
	"kernel_file=Image\0" \
	"dtb_file=\0" \
	"initrd_file=initrd\0" \
	"aon_file=a210-aon.bin\0" \
	"str_file=str.bin\0" \
	"initrd_size=0x400000\0" \
	"init_file=/sbin/init\0" \
	"fdt_high=0xffffffffffffffff\0" \
	"splashimage=0x30000000\0" \
	"splashpos=m,m\0" \
	"rdsize=200M\0" \
	"ramdisk_size=204800\0" \
	"set_bargs_pre=setenv barg_pre console=${tty_dev},${baudrate} root=${root_device} init=${init_file} rootwait rw earlycon clk_ignore_unused loglevel=${kernel_loglevel} crashkernel=${kdump_buf}\0"

#define EVN_PARTITION \
	"fastboot.has-slot:mmc0boot0=no\0" \
	"fastboot.has-slot:mmc0boot1=no\0" \
	"fastboot.has-slot:boot_a=no\0" \
	"fastboot.has-slot:boot_b=no\0" \
	"fastboot.has-slot:system_a=no\0" \
	"fastboot.has-slot:system_b=no\0" \
	"fastboot.has-slot:app_a=no\0" \
	"fastboot.has-slot:app_b=no\0" \
	"fastboot.has-slot:data=no\0" \
	"fastboot.has-slot:gpt=no\0" \
	"fastboot.has-slot:factory=no\0" \
	"fastboot.has-slot:uboot_env=no\0"

/*
 * CONFIG_BOOTCOMMAND can be added to the default environment before
 * CFG_EXTRA_ENV_SETTINGS (see include/env_default.h).
 */
#ifndef CONFIG_BOOTCOMMAND
#define BOOT_FIT_BOOTCMD \
	"bootcmd=run select_slot; run load_image; run set_bargs_pre; setenv bootargs ${barg_pre}; booti $kernel_addr $initrd_addr:$initrd_size $dtb_addr;\0"
#define BOOT_XT_BOOTCMD \
	"bootcmd=run select_slot; run load_image; run set_bargs_pre; setenv bootargs ${barg_pre}; booti $kernel_addr $initrd_addr:$initrd_size $dtb_addr $opensbi_addr;\0"
#else
#define BOOT_FIT_BOOTCMD
#define BOOT_XT_BOOTCMD
#endif

#ifdef CONFIG_RISCV_SMODE
#define BOOT_FIT \
	"loadkernel=ext4load ${boot_device} ${kernel_addr}  ${kernel_file}; md5sum ${kernel_addr} $filesize\0" \
	"loadinitrd=ext4load ${boot_device} ${initrd_addr}  ${initrd_file}; setenv initrd_size $filesize\0" \
	"load_image=run loadkernel; run loadinitrd\0" \
	BOOT_FIT_BOOTCMD \
	"altbootcmd=run rollback; run rollback_finish; reset;\0"
#else
#define BOOT_FIT
#endif

#ifdef CONFIG_RISCV_MMODE
#define BOOT_XT \
	"loadfdt=ext4load    ${boot_device} ${dtb_addr}     ${dtb_file}\0" \
	"loadkernel=ext4load ${boot_device} ${kernel_addr}  ${kernel_file}\0" \
	"loadsbi=ext4load    ${boot_device} ${opensbi_addr} ${opensbi_file}\0" \
	"loadinitrd=ext4load ${boot_device} ${initrd_addr}  ${initrd_file}; setenv initrd_size $filesize\0" \
	"load_image=run loadfdt; run loadsbi; run loadkernel; run loadinitrd;\0" \
	BOOT_XT_BOOTCMD
#else
#define BOOT_XT
#endif

#define BOOT_NFS \
	"nfsroot=10.0.11.6:/mnt/ssd/rootfs\0" \
	"set_nfsbootargs=setenv bootargs ip=${ipaddr}::${gatewayip}:${netmask}:myhostname:eth0:off nfsroot=${nfsroot},proto=tcp,nfsvers=4,rw ${barg_pre}\0" \
	"boot_nfs=setenv autoload no; dhcp; run select_slot; setenv root_device /dev/nfs; run set_bargs_pre; run set_nfsbootargs; booti $kernel_addr - $dtb_addr\0"

/*
 * boot/bootmeth_android.c:boot_android_normal() stages the "vendor_boot"
 * half of a v3+ Android boot image at env "vendor_boot_comp_addr_r"
 * (ulong, via env_get_hex(), defaulting silently to 0 if unset). Address 0
 * isn't valid DRAM on the A210 (DRAM starts at 0x80000000), so leaving this
 * unset makes get_avendor_bootimg_addr() report an unmapped image and
 * booting fails with "For boot header v3+ vendor boot image has to be
 * provided". CONFIG_FASTBOOT_BUF_ADDR's region is reused here since it is
 * only live during an active fastboot session, never during a normal boot.
 *
 * boot/image-android.c:android_image_get_ramdisk() similarly needs
 * "ramdisk_addr_r" to assemble the final vendor+boot ramdisk before booting
 * a v3+ image, and fails with "Invalid ramdisk_addr_r to copy ramdisk into"
 * if unset. Reuses EVN_COMMON's initrd_addr, an already-valid A210 DRAM
 * address used by the Linux-BSP boot flow, which sits comfortably above the
 * kernel/vendor_boot staging areas.
 *
 * boot/image-fdt.c's Android branch first looks for a DTB embedded in
 * vendor_boot's own DTB area (android_image_get_dtb_by_index(), selected by
 * "adtb_idx", already correctly defaulting to 0); when that image doesn't
 * carry one it falls back to the env var "fdtaddr", defaulting silently to
 * 0 (=> "Device tree not found"). SPL/OpenSBI already load this board's DTB
 * to 0x8c000000 before jumping to U-Boot (see the FIT's "fdt-*" subimage
 * load address) and nothing overwrites it before Linux boots, so reuse it
 * as the fallback until vendor_boot.img embeds its own DTB.
 */
#ifdef CONFIG_ANDROID_BOOT_IMAGE
#define ANDROID_ENV_SETTINGS \
	"vendor_boot_comp_addr_r=0x91000000\0" \
	"init_boot_comp_addr_r=0x93000000\0" \
	"ramdisk_addr_r=0x9e000000\0" \
	"fdtaddr=0x8c000000\0"
#else
#define ANDROID_ENV_SETTINGS
#endif

#define CFG_EXTRA_ENV_SETTINGS \
	EVN_COMMON \
	EVN_PARTITION \
	"devtype=mmc\0" \
	"devnum=0\0" \
	"active_slot=a\0" \
	"a_loaderpart=0\0" \
	"a_bootpart=3\0" \
	"a_boot_partuuid=04fb8c79-34ec-403e-ad5d-db205c76eff1\0" \
	"a_systempart=5\0" \
	"a_system_partuuid=ff2a7ab6-5290-4d1c-bcb4-2b60f62ea961\0" \
	"a_apppart=7\0" \
	"a_app_partuuid=c2d963d5-5601-4f0b-9959-c6b543b40a41\0" \
	"a_version=0.0.1\0" \
	"a_boot_success=1\0" \
	"b_loaderpart=0\0" \
	"b_bootpart=4\0" \
	"b_boot_partuuid=999c1c6d-eb10-4656-a4ea-0bd5a88fa4e2\0" \
	"b_systempart=6\0" \
	"b_system_partuuid=3ee62a15-2457-4b7a-9e8e-785e1a9867f2\0" \
	"b_apppart=8\0" \
	"b_app_partuuid=d52e57e6-8bb7-4974-9282-fdecd05c7c92\0" \
	"data_partuuid=b8753fb5-3a4c-4de7-b6c0-4fd4a705f750\0" \
	"b_version=0.0.1\0" \
	"b_boot_success=0\0" \
	"set_slot_a=setenv boot_device ${devtype} ${devnum}:${a_bootpart}; setenv root_device /dev/${devtype}blk${devnum}p${a_systempart};\0" \
	"set_slot_b=setenv boot_device ${devtype} ${devnum}:${b_bootpart}; setenv root_device /dev/${devtype}blk${devnum}p${b_systempart};\0" \
	"select_slot=run set_slot_${active_slot}\0" \
	"bootcount_mode=0\0" \
	"bootcount=0\0" \
	"bootlimit=3\0" \
	"upgrade_available=0\0" \
	"rollback_a=echo Roll back to slot b; if test $b_boot_success -eq 1; then setenv active_slot b; setenv a_boot_success 0; else echo Roll back failed; fi\0" \
	"rollback_b=echo Roll back to slot a; if test $a_boot_success -eq 1; then setenv active_slot a; setenv b_boot_success 0; else echo Roll back failed; fi\0" \
	"rollback=run rollback_${active_slot}\0" \
	"rollback_finish=if test $bootcount_mode -eq 0; then setenv upgrade_available 0; fi; setenv bootcount 0; saveenv;\0" \
	BOOT_FIT \
	BOOT_XT \
	BOOT_NFS \
	ANDROID_ENV_SETTINGS \
	"\0"
#endif /* __CONFIG_A210_EVB_H */
