# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (c) 2019, Softathome

ifdef CONFIG_ZHIHE_RAMBUS_ALGO
obj-$(CONFIG_$(SPL_)FIT_CIPHER) += rb-aes-decrypt.o
else
obj-$(CONFIG_$(SPL_)FIT_CIPHER) += aes-decrypt.o
endif
