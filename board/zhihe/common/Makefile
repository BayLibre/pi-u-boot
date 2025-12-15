# SPDX-License-Identifier:	GPL-2.0+

ifdef CONFIG_SPL_BUILD
# SPL
obj-$(CONFIG_FIT) += boot_fit/spl_with_fit.o
obj-$(CONFIG_FIT) += boot_fit/spl_fit_boot_fixup.o
obj-$(CONFIG_FIT) += boot_fit/spl_ram_fit.o
obj-y += boot_mmode/spl_rvbl_payload.o
obj-y += boot_mmode/spl_boot_fixup.o
obj-y += boot/boot_method.o
obj-y += boot/boot_image.o
else
# U-Boot
obj-$(CONFIG_ZHIHE_EFUSE_CMD_SUPPORT) += efuse/efuse_cmd.o
obj-$(CONFIG_ZHIHE_MMC_OP_SUPPORT) += mmc/mmc.o
obj-$(CONFIG_ENV_IS_IN_MMC) += factory_nv/nv_mmc.o
endif

obj-y += board_check.o
