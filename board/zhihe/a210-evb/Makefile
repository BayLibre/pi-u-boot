# SPDX-License-Identifier:	GPL-2.0+

ifdef CONFIG_SPL_BUILD
obj-y += spl_board.o
obj-y += spl_call_bram.o
obj-y += spl_call_opensbi.o
obj-y += cpusys/ccu_cfg.o
obj-y += cpusys/cpu_ss_init.o
obj-y += cpusys/pctrl_cfg.o
obj-y += cpusys/pll_cfg.o
obj-y += subsys/ss_config.o
obj-y += arch_ext/bram_start.o
obj-y += arch_ext/arch_ext.o
obj-y += adc/adc.o
else
obj-y += board.o pinctrl.o
obj-y += cmd/emmc.o
obj-y += cmd/emmc_sw_tuning.o
obj-y += cmd/iopmp.o
obj-$(CONFIG_USB_GADGET_DWC2_OTG) += board_dwc2_otg.o
endif

obj-y += board_common.o

# Build DDR?
#                   (ifdef CONFIG_SPL)  (ifndef CONFIG_SPL)
# ifdef  SPL_BUILD       v                --
# ifndef SPL_BUILD       x                v
ifndef CONFIG_SPL_BUILD
ifdef CONFIG_SPL
BUILD_DDR := n
else
BUILD_DDR := y
endif
else
BUILD_DDR := y
endif

ifeq ($(BUILD_DDR), y)
ifndef CONFIG_ZHP100EMU_LP4X
obj-$(CONFIG_ZHP100EVB_DDR_INIT) += ddr/phy/ddrphy_csr.o ddr/phy/ddrphy_utils.o
endif
obj-$(CONFIG_ZHP100EVB_DDR_INIT) += ddr/ddr_init.o

obj-$(CONFIG_LPDDR_EYE) += ddr/lp4_diag_eye.o

# ddr haps(dummy) & emu
obj-$(CONFIG_ZHP100EVB_DDR_DUMMY) += ddr/ddr_timing/ZHP100EVB_DDR_DUMMY_timing.o
obj-$(CONFIG_ZHP100EMU_LP4X) += ddr/emu_lp4x/ddr_ss_api.o ddr/emu_lp4x/emu_lp4x_timing.o

# single rank 4266 lp4x 2D
obj-$(CONFIG_ZHP100EVB_LP4X_4266_1R_2D) += ddr/ddr_timing/ZHP100EVB_LP4X_4266_1R_DDRC_timing.o
obj-$(CONFIG_ZHP100EVB_LP4X_4266_1R_2D) +=  ddr/ddr_timing/ZHP100EVB_LP4X_4266_1R_PHY_2D_timing.o

# single rank 3733 lp4x 2D
obj-$(CONFIG_ZHP100EVB_LP4X_3733_1R_2D) += ddr/ddr_timing/ZHP100EVB_LP4X_3733_1R_DDRC_timing.o
obj-$(CONFIG_ZHP100EVB_LP4X_3733_1R_2D) += ddr/ddr_timing/ZHP100EVB_LP4X_3733_1R_PHY_2D_timing.o

# single rank 3200 lp4x 2D
obj-$(CONFIG_ZHP100EVB_LP4X_3200_1R_2D) += ddr/ddr_timing/ZHP100EVB_LP4X_3200_1R_DDRC_timing.o
obj-$(CONFIG_ZHP100EVB_LP4X_3200_1R_2D) += ddr/ddr_timing/ZHP100EVB_LP4X_3200_1R_PHY_2D_timing.o

# dual rank 4266 lp4x 2D
obj-$(CONFIG_ZHP100EVB_LP4X_4266_2R_2D) += ddr/ddr_timing/ZHP100EVB_LP4X_4266_2R_DDRC_timing.o
obj-$(CONFIG_ZHP100EVB_LP4X_4266_2R_2D) += ddr/ddr_timing/ZHP100EVB_LP4X_4266_2R_PHY_2D_timing.o
endif

obj-$(CONFIG_ZHP100EVB_AUXDISP_CMD_SUPPORT) += cmd/auxdisp_hdmi.o
obj-$(CONFIG_ZHP100EVB_USBATE_CMD_SUPPORT) += cmd/usb_ate.o
obj-$(CONFIG_ZHP100EVB_PCIEATE_CMD_SUPPORT) += cmd/ate_pcie.o
obj-$(CONFIG_ZHP100EVB_GPU_MAX_POWER_TEST) += cmd/gpu_max_power.o
obj-$(CONFIG_ZHP100EVB_GPU_MAX_POWER_TEST) += cmd/gpu_BinLitePdumpPlayer.o
