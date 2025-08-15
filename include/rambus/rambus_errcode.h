/*
 * Copyright (C) 2024-2030 Zhihe Group Holding Limited
 */

#ifndef INC_RAMBUS_ERRCODE_H
#define INC_RAMBUS_ERRCODE_H

#include "rambus_common.h"
#include "rambus_log.h"

/* common */

#ifndef CSI_OK
#define CSI_ERROR_BASE 0x10000000
#define CSI_OK CSI_OK
#define CSI_FAIL CSI_ERROR_BASE + 1
#define CSI_MEM_OVERFLOW CSI_ERROR_BASE + 2
#define CSI_PARAM_INV CSI_ERROR_BASE + 3
#define CSI_OPERATION_BUSY CSI_ERROR_BASE + 4
#define CSI_AUTH_FAIL CSI_ERROR_BASE + 5
#define CSI_CRYPT_FAIL CSI_ERROR_BASE + 6
#define CSI_NOT_SUPPORT CSI_ERROR_BASE + 7
#define CSI_INVALID_PADDING CSI_ERROR_BASE + 8
#define CSI_BAD_INPUT_DATA CSI_ERROR_BASE + 9
#define CSI_INVALID_KEY_LENGTH CSI_ERROR_BASE + 10
#define CSI_INVALID_INPUT_LENGTH CSI_ERROR_BASE + 11
#define CSI_FEATURE_UNAVAILABLE CSI_ERROR_BASE + 12
#define CSI_HW_ACCEL_FAILED CSI_ERROR_BASE + 13
#define CSI_CCM_AUTH_FAILED CSI_ERROR_BASE + 14
#define CSI_KEY_GEN_FAILED CSI_ERROR_BASE + 15
#define CSI_KEY_CHECK_FAILED CSI_ERROR_BASE + 16
#define CSI_PUBLIC_FAILED CSI_ERROR_BASE + 17
#define CSI_PRIVATE_FAILED CSI_ERROR_BASE + 18
#define CSI_VERIFY_FAILED CSI_ERROR_BASE + 19
#define CSI_OUTPUT_TOO_LARGE CSI_ERROR_BASE + 20
#define CSI_RNG_FAILED CSI_ERROR_BASE + 21
#define CSI_BUFFER_TOO_SMALL CSI_ERROR_BASE + 22
#define CSI_INVALID_FORMAT CSI_ERROR_BASE + 23
#define CSI_ALLOC_FAILED CSI_ERROR_BASE + 24
#define CSI_DRV_FAILED CSI_ERROR_BASE + 25

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

#define CHECK_16byte_Multiple(size, ret)                  \
    if ((size % 16) != 0) {                               \
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

#endif

#endif
