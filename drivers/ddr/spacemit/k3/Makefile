# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2025 SPACEMIT Micro Limited
#

ifdef CONFIG_XPL_BUILD
obj-y += ddr_init.o
obj-$(CONFIG_K3_BOARD_FPGA) += ddr-snps-lp4x-fpga-init.o
obj-$(CONFIG_K3_BOARD_ASIC) += lpddr5_init.o lpddr5_training_table.o lpddr5_pre_training_table.o
obj-$(CONFIG_K3_BOARD_ASIC) += lpddr4x_init.o lpddr4x_training_table.o lpddr4x_pre_training_table.o
obj-$(CONFIG_K3_BOARD_ASIC) += lpddr_training_result.o lpddr_quick_boot.o
obj-$(CONFIG_K3_BOARD_ASIC) += lpddr5_quickboot_table.o lpddr4x_quickboot_table.o
endif
