/*
 * Copyright (C) 2024-2030 Zhihe Group Holding Limited
 */

#ifndef __KDF_H__
#define __KDF_H__
#include "rambus_aes.h"
#include "rambus_sm4.h"
#include "rambus_common.h"
#include <stdint.h>

typedef enum {
    /*CHIP DRIVERED KEYS*/
    KDF_CHIP_DFT_CHALLENGE_EK,
    KDF_CHIP_CPUJTAG_CHALLENGE_EK,
    KDF_CHIP_IMAGE_EK,
    KDF_CHIP_RESERVED_EK1,
    KDF_CHIP_RESERVED_EK2,
    KDF_CHIP_RESERVED_EK3,
    KDF_CHIP_RESERVED_EK4,
    KDF_CHIP_RESERVED_EK5,
    KDF_CHIP_RESERVED_EK6,
    KDF_CHIP_RESERVED_EK7,
    KDF_CHIP_RESERVED_EK8,

    /*PRODUCTION DREIVERED KEYS*/
    KDF_PRO_USER_IMAGE_EK,
    KDF_PRO_RESERVED_EK1,
    KDF_PRO_RESERVED_EK2,
    KDF_PRO_RESERVED_EK3,
    KDF_PRO_RESERVED_EK4,
    KDF_PRO_RESERVED_EK5,
    KDF_PRO_RESERVED_EK6,
    KDF_PRO_RESERVED_EK7,
    KDF_PRO_RESERVED_EK8,

    /*DEVICE DEREIVED KEYS*/
    KDF_DEVICE_SECURE_STORAGE_EK1,
    KDF_DEVICE_SECURE_STORAGE_EK2,
    KDF_DEVICE_SECURE_STORAGE_EK3,
    KDF_DEVICE_SECURE_STORAGE_EK4,
    KDF_DEVICE_SECURE_STORAGE_EK5,
    KDF_DEVICE_SECURE_STORAGE_EK6,
    KDF_DEVICE_SECURE_STORAGE_EK7,
    KDF_DEVICE_SECURE_STORAGE_EK8,
    KDF_DEVICE_SECURE_STORAGE_EK9,
    KDF_DEVICE_SECURE_STORAGE_EK10,
    KDF_DEVICE_SECURE_STORAGE_EK11,
    KDF_DEVICE_SECURE_STORAGE_EK12,
    KDF_DEVICE_SECURE_STORAGE_EK13,
    KDF_DEVICE_SECURE_STORAGE_EK14,
    KDF_DEVICE_SECURE_STORAGE_EK15,
    KDF_DEVICE_SECURE_STORAGE_EK16,
    KDF_DEVICE_RPMB_ACCESS_EK,
    KDF_DEVICE_RESERVED_EK1,
    KDF_DEVICE_RESERVED_EK2,
    KDF_DEVICE_RESERVED_EK3,
    KDF_DEVICE_RESERVED_EK4,
    KDF_DEVICE_RESERVED_EK5,
    KDF_DEVICE_RESERVED_EK6,
    KDF_DEVICE_RESERVED_EK7,
    KDF_DEVICE_RESERVED_EK8,

    KDF_DERIVED_MAX,
} csi_kdf_derived_key_t;

typedef enum {
    KDF_KEY_TYPE_AES_256,
    KDF_KEY_TYPE_AES_192,
    KDF_KEY_TYPE_AES_128,
    KDF_KEY_TYPE_SM4,
    KDF_KEY_TYPE_TDES_192,
    KDF_KEY_TYPE_TDES_128,
    KDF_KEY_TYPE_DES,
    KDF_KEY_TYPE_MAX,
} csi_kdf_key_type_t;

/**
\brief KDF Ctrl Block
*/
typedef struct {
    union {
        csi_aes_t *aes;
        csi_sm4_t *sm4;
    };
    csi_kdf_key_type_t type;
} csi_kdf_key_handle_t;

/**
\brief KDF Ctrl Block
*/
typedef struct {
    csi_dev_t dev;
    void *priv;
} csi_kdf_t;

/**
  \brief       Set key to algorithim engine.
  \param[in]   handle    Handle to cipher.
  \param[in]   dkey derived key type.
  \return      error code
*/
csi_error_t csi_kdf_set_key(csi_kdf_key_handle_t *handle, csi_kdf_derived_key_t dkey);

#endif
