# SPDX-License-Identifier: GPL-2.0+
# Copyright(C) 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.

# PLATFORM_LIBS requires absolute paths.
# SRC_TREE_ABS converts $(srctree) into an absolute path
# to resolve compatibility issues when the O= parameter is either used or omitted.
SRC_TREE_ABS := $(shell cd $(srctree) && pwd)

ifdef CONFIG_ZHIHE_RAMBUS_ALGO
PLATFORM_LIBS += $(SRC_TREE_ABS)/board/zhihe/common/libs/libsecurity.a.bin
endif

zhihe-rvbl.bin: u-boot-with-spl.bin
	@$(SRC_TREE_ABS)/board/zhihe/common/script/generate_firmware.sh rvbl spl/u-boot-spl.bin none spl/u-boot-spl-rvbl.bin
	@$(SRC_TREE_ABS)/board/zhihe/common/script/generate_firmware.sh rvbl spl/u-boot-spl.bin u-boot.bin u-boot-with-spl-rvbl.bin
	@cp u-boot-with-spl-rvbl.bin u-boot-with-spl.bin
	@echo /dev/mmcblk$(CONFIG_SYS_MMC_ENV_DEV)	$(CONFIG_ENV_OFFSET)	$(CONFIG_ENV_SIZE) > fw_env.config
	@echo /dev/mmcblk$(CONFIG_SYS_MMC_ENV_DEV)	$(CONFIG_ENV_OFFSET_REDUND)	$(CONFIG_ENV_SIZE) >> fw_env.config
