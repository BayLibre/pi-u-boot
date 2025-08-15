# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (c) 2013, Google Inc.
#
# (C) Copyright 2000-2007
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
ifdef CONFIG_ZHIHE_RAMBUS_ALGO
obj-$(CONFIG_$(SPL_TPL_)RSA_VERIFY) += rb-rsa-verify.o
else
obj-$(CONFIG_$(SPL_TPL_)RSA_VERIFY) += rsa-verify.o
endif
obj-$(CONFIG_$(SPL_TPL_)RSA_VERIFY_WITH_PKEY) += rsa-keyprop.o
obj-$(CONFIG_RSA_SOFTWARE_EXP) += rsa-mod-exp.o
