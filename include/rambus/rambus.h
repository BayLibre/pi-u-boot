/*
 * Copyright (C) 2024-2030 Zhihe Group Holding Limited
 */

#ifndef INC_RAMBUS_H
#define INC_RAMBUS_H

#include "device_types.h"
#ifdef SEC_LIB_VERSION
#include "device_rw.h"
#include "drv/rambus_common.h"
#include "rambus_errcode.h"
#include "rambus_log.h"
#else
#include "rambus_common.h"
#endif

extern uint64_t g_freq_timer;
extern uint64_t g_freq_ip;
extern uint64_t g_start_ctr;
extern uint64_t g_end_ctr;
extern uint64_t g_data_len_in_bits;
extern uint32_t g_type;

enum rambus_cipher_padding_mode {
    PADDING_ZERO,
    PADDING_FF,
    PADDING_RANDOM,
};

uint32_t rambus_get_random_byte(uint8_t *buf, uint32_t count);

uint32_t rambus_get_random_byte_nozero(uint8_t *buf, uint32_t count);

uint32_t rambus_kdf_get_mask(uint8_t *mask, uint32_t len);

/* 1 bpc, 2 tps, 3 bps */
void rambus_perf_init(uint32_t data_len_in_bits, uint32_t type);
void rambus_perf_start(void);
void rambus_perf_end(void);
void rambus_perf_get(char *ncase);

#define DEFAULT_TIMEOUT 1000U

#ifdef CONFIG_ALG_PERF_TEST
#define RB_PERF_INIT(bits, type)                   \
    do {                                           \
        if (data_len_in_bits != 0) {               \
            g_data_len_in_bits = data_len_in_bits; \
        }                                          \
        if (type != 0) {                           \
            g_type = type;                         \
        }                                          \
    } while (0)

#define RB_PERF_START_POINT()                                                                \
    do {                                                                                     \
        g_start_ctr = ((((uint64_t)csi_coret_get_valueh() << 32U) | csi_coret_get_value())); \
    } while (0)

#define RB_PERF_END_POINT()                                                                \
    do {                                                                                   \
        g_end_ctr = ((((uint64_t)csi_coret_get_valueh() << 32U) | csi_coret_get_value())); \
    } while (0)

#else
#define RB_PERF_INIT(...)
#define RB_PERF_START_POINT(...)
#define RB_PERF_END_POINT(...)
#endif

static inline void rambus_xor(uint32_t *a, uint32_t *b, uint32_t *c, uint32_t len)
{
    for (int i = 0; i < (int)len; i++) {
        c[i] = a[i] ^ b[i];
    }
}

/**
 * @brief Get the aes sca enable config
 *
 * @param is_en is enable
 * @return uint32_t
 */
uint32_t rambus_get_aes_sca(uint32_t *is_en);

/**
 * @brief Get the sm4 sca enable config
 *
 * @param is_en is enable
 * @return uint32_t
 */
uint32_t rambus_get_sm4_sca(uint32_t *is_en);

/**
 * @brief Get the pka sca enable config
 *
 * @param is_en is enable
 * @return uint32_t
 */
uint32_t rambus_get_pka_sca(uint32_t *is_en);

/**
 * @brief rambus_cache_en
 * @return uint32_t enable: 1
 *
 */
uint32_t rambus_cache_en(void);

/**
 * @brief trng init
 *
 * @return csi_error_t
 */
csi_error_t trng_init(void);

/**
 * @brief rb wait status
 *
 * @param dev
 * @param offset
 * @param mask
 * @param status
 * @return uint32_t
 */
csi_error_t rambus_wait_status(Device_Handle_t *dev, const uint32_t offset, uint32_t mask, uint32_t status);

/**
 * \brief          rambus crypto init.
 * \return         0 if successful, or error code
 */
uint32_t rambus_crypto_open(void);

/**
 * @brief rambus crypto uninit.
 *
 */
void rambus_crypto_close(void);

/**
 * \brief              rambus set cipher padding type.
 * @param padding_mode cipher padding mode
 * \return             0 if successful, or error code
 */
uint32_t rambus_enable_cipher_pd_type(enum rambus_cipher_padding_mode padding_mode);

uint32_t rambus_get_padding_byte(uint8_t *buf, uint32_t count);

#endif
