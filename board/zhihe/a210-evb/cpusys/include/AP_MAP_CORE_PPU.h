/*
 * Copyright(C) 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#ifndef __AP_MAP_CORE_PPU_OFFSET_H__
#define __AP_MAP_CORE_PPU_OFFSET_H__

#include "AP_MAP_CORE_COMMON.h"

#define AP_CORE_PPU_SW_R2P(core) AP_CORE_PPU_ADDR(core, 0x00)
#define AP_CORE_PPU_SW_REQ(core) AP_CORE_PPU_ADDR(core, 0x04)
#define AP_CORE_PPU_SW_STATE(core) AP_CORE_PPU_ADDR(core, 0x08)
#define AP_CORE_PPU_CTRL(core) AP_CORE_PPU_ADDR(core, 0x0c)
#define AP_CORE_PPU_OVR_TIME(core) AP_CORE_PPU_ADDR(core, 0x10)
#define AP_CORE_PPU_DLY_TIME_CTRL(core) AP_CORE_PPU_ADDR(core, 0x14)
#define AP_CORE_PPU_DLY_TIME_0(core) AP_CORE_PPU_ADDR(core, 0x18)
#define AP_CORE_PPU_DLY_TIME_1(core) AP_CORE_PPU_ADDR(core, 0x1c)
#define AP_CORE_PPU_DLY_TIME_2(core) AP_CORE_PPU_ADDR(core, 0x20)
#define AP_CORE_PPU_FORCE_MODE(core) AP_CORE_PPU_ADDR(core, 0x24)
#define AP_CORE_PPU_INT_EN(core) AP_CORE_PPU_ADDR(core, 0x28)
#define AP_CORE_PPU_INT_CLR(core) AP_CORE_PPU_ADDR(core, 0x2c)
#define AP_CORE_PPU_INT_STS(core) AP_CORE_PPU_ADDR(core, 0x30)
#define AP_CORE_PPU_INT_RAW_STS(core) AP_CORE_PPU_ADDR(core, 0x34)
#define AP_CORE_PPU_RECOVERY(core) AP_CORE_PPU_ADDR(core, 0x38)
#define AP_CORE_PPU_DYN_SWITCH_STS(core) AP_CORE_PPU_ADDR(core, 0x3c)
#define AP_CORE_PPU_PWR_STS(core) AP_CORE_PPU_ADDR(core, 0x40)

#endif
