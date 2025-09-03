/*
 * Copyright(C) 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#ifndef __AP_MAP_CORE_BPC_OFFSET_H__
#define __AP_MAP_CORE_BPC_OFFSET_H__

#include "AP_MAP_CORE_COMMON.h"

#define AP_CORE_BPC_SW_CTR(core) AP_CORE_BPC_ADDR(core, 0x000)
#define AP_CORE_BPC_SW_SLEEP_USER(core) AP_CORE_BPC_ADDR(core, 0x004)
#define AP_CORE_BPC_SW_ACK(core) AP_CORE_BPC_ADDR(core, 0x008)
#define AP_CORE_BPC_SW_RECOVERY_ON(core) AP_CORE_BPC_ADDR(core, 0x00c)
#define AP_CORE_BPC_SW_RECOVERY_OFF(core) AP_CORE_BPC_ADDR(core, 0x010)
#define AP_CORE_BPC_SW_DELAY(core) AP_CORE_BPC_ADDR(core, 0x014)
#define AP_CORE_BPC_SW_USER_DELAY(core) AP_CORE_BPC_ADDR(core, 0x018)
#define AP_CORE_BPC_SW_CFG_PM_0_0(core) AP_CORE_BPC_ADDR(core, 0x01c)
#define AP_CORE_BPC_SW_CFG_PM_0_1(core) AP_CORE_BPC_ADDR(core, 0x020)
#define AP_CORE_BPC_SW_CFG_PM_0_2(core) AP_CORE_BPC_ADDR(core, 0x024)
#define AP_CORE_BPC_SW_CFG_PM_1_0(core) AP_CORE_BPC_ADDR(core, 0x02c)
#define AP_CORE_BPC_SW_CFG_PM_1_1(core) AP_CORE_BPC_ADDR(core, 0x030)
#define AP_CORE_BPC_SW_CFG_PM_1_2(core) AP_CORE_BPC_ADDR(core, 0x034)
#define AP_CORE_BPC_SW_CFG_PM_1_3(core) AP_CORE_BPC_ADDR(core, 0x038)
#define AP_CORE_BPC_SW_CFG_PM_2(core) AP_CORE_BPC_ADDR(core, 0x03c)
#define AP_CORE_BPC_SW_CFG_PM_3(core) AP_CORE_BPC_ADDR(core, 0x04c)
#define AP_CORE_BPC_SW_CFG_PM_4(core) AP_CORE_BPC_ADDR(core, 0x05c)
#define AP_CORE_BPC_SW_CFG_PM_5(core) AP_CORE_BPC_ADDR(core, 0x06c)
#define AP_CORE_BPC_SW_CFG_PM_6(core) AP_CORE_BPC_ADDR(core, 0x07c)
#define AP_CORE_BPC_SW_CFG_PM_7(core) AP_CORE_BPC_ADDR(core, 0x08c)
#define AP_CORE_BPC_SW_CFG_PM_8(core) AP_CORE_BPC_ADDR(core, 0x09c)
#define AP_CORE_BPC_SW_CFG_PM_9(core) AP_CORE_BPC_ADDR(core, 0x10c)
#define AP_CORE_BPC_SW_CFG_PM_10(core) AP_CORE_BPC_ADDR(core, 0x11c)
#define AP_CORE_BPC_SW_CFG_PM_11(core) AP_CORE_BPC_ADDR(core, 0x12c)
#define AP_CORE_BPC_SW_CTR_OPT(core) AP_CORE_BPC_ADDR(core, 0x13c)
#define AP_CORE_BPC_CUR_STATE(core) AP_CORE_BPC_ADDR(core, 0x140)

#endif
