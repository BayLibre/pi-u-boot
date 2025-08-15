/*
 * Copyright (C) 2024-2030 Zhihe Group Holding Limited
 */

/******************************************************************************
 * @file     drv/rambus_common.h
 * @brief    Header File for Common Driver
 * @version  V1.0
 * @date     31. March 2020
 * @model    common
 ******************************************************************************/

#ifndef _DRV_COMMON_H_
#define _DRV_COMMON_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
// #include "list.h"
#include "dev_tag.h"
#include "rambus_log.h"

typedef enum {
    CSI_OK = 0,
    CSI_ERROR = -1,
    CSI_BUSY = -2,
    CSI_TIMEOUT = -3,
    CSI_UNSUPPORTED = -4,
    CSI_INVALID_PARAM = -5,
    CSI_CRYPT_FAIL = -6,
} csi_error_t;

typedef struct {
    uint8_t readable;
    uint8_t writeable;
    uint8_t error;
} csi_state_t;

typedef struct csi_dev csi_dev_t;

struct csi_dev {
    unsigned long reg_base;
    uint8_t irq_num;
    uint8_t idx;
    uint16_t dev_tag;
    void (*irq_handler)(void *);
};

#define HANDLE_REG_BASE(handle) (handle->dev.reg_base)
#define HANDLE_IRQ_NUM(handle) (handle->dev.irq_num)
#define HANDLE_DEV_IDX(handle) (handle->dev.idx)
#define HANDLE_IRQ_HANDLER(handle) (handle->dev.irq_handler)

typedef struct {
    unsigned long reg_base;
    uint8_t irq_num;
    uint8_t idx;
    uint16_t dev_tag;
} csi_perip_info_t;

csi_error_t target_get(csi_dev_tag_t dev_tag, uint32_t idx, csi_dev_t *dev);
csi_error_t target_get_optimal_dma_channel(void *dma_list, uint32_t ctrl_num, csi_dev_t *parent_dev, void *ch_info);
void mdelay(uint32_t ms);
extern void udelay(uint32_t us);
void msleep(uint32_t ms);

#define CHECK_RET(x)                                          \
    do {                                                      \
        if (!(x)) {                                           \
            LOG_CRIT("err %s, %d\n", __FUNCTION__, __LINE__); \
            return;                                           \
        }                                                     \
    } while (0)

#define CHECK_RET_WITH_RET(x, ret)                            \
    do {                                                      \
        if (!(x)) {                                           \
            LOG_CRIT("err %s, %d\n", __FUNCTION__, __LINE__); \
            return ret;                                       \
        }                                                     \
    } while (0)

#define CHECK_16byte_ALIGNMENT(_i, ret)                   \
    if ((((uint64_t)_i) & 0xF) != 0) {                    \
        LOG_CRIT("err %s, %d\n", __FUNCTION__, __LINE__); \
        return ret;                                       \
    }

#define CHECK_32byte_ALIGNMENT(_i, ret)                   \
    if ((((uint64_t)_i) & 0x1F) != 0) {                   \
        LOG_CRIT("err %s, %d\n", __FUNCTION__, __LINE__); \
        return ret;                                       \
    }

#define CHECK_64byte_ALIGNMENT(_i, ret)                   \
    if ((((uint64_t)_i) & 0x3F) != 0) {                   \
        LOG_CRIT("err %s, %d\n", __FUNCTION__, __LINE__); \
        return ret;                                       \
    }

#define CHECK_PARAM_RET(c, r)                                 \
    do {                                                      \
        if (!(c)) {                                           \
            LOG_CRIT("err %s, %d\n", __FUNCTION__, __LINE__); \
            return (r);                                       \
        }                                                     \
    } while (0)

#define CHECK_RET_VOID(c)                                     \
    do {                                                      \
        if (!(c)) {                                           \
            LOG_CRIT("err %s, %d\n", __FUNCTION__, __LINE__); \
            return;                                           \
        }                                                     \
    } while (0)

#define CHECK_PARAM CHECK_PARAM_RET

#endif /* _DRV_COMMON_H_ */
