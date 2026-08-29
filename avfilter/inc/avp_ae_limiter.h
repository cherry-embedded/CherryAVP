/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AVP_AE_LIMITER_H
#define AVP_AE_LIMITER_H

#include "avp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Limiter runtime control commands. */
typedef enum {
    AVP_AE_LIMITER_CMD_SET_ENABLE = 0, /**< Set processing enable flag (int*; 0/1). */
    AVP_AE_LIMITER_CMD_GET_ENABLE,     /**< Get processing enable flag (int*). */
    AVP_AE_LIMITER_CMD_SET_CEILING_DB, /**< Set output ceiling in dBFS (float*). */
    AVP_AE_LIMITER_CMD_GET_CEILING_DB, /**< Get output ceiling in dBFS (float*). */
    AVP_AE_LIMITER_CMD_SET_ATTACK_MS,  /**< Set attack time (float*). */
    AVP_AE_LIMITER_CMD_GET_ATTACK_MS,  /**< Get attack time (float*). */
    AVP_AE_LIMITER_CMD_SET_RELEASE_MS, /**< Set release time (float*). */
    AVP_AE_LIMITER_CMD_GET_RELEASE_MS, /**< Get release time (float*). */
    AVP_AE_LIMITER_CMD_RESET,          /**< Reset peak envelope and gain state. */
} avp_ae_limiter_cmd_t;

/** @brief Limiter configuration. */
typedef struct {
    uint32_t sample_rate; /**< Input sample rate in Hz. */
    uint8_t channels;     /**< Number of interleaved channels.*/
    float ceiling_db;     /**< Maximum output level in dBFS, -100..0. */
    float attack_ms;      /**< Attack time in milliseconds. */
    float release_ms;     /**< Release time in milliseconds. */
    uint8_t enable;       /**< Non-zero to process, zero to bypass. */
} avp_ae_limiter_config_t;

typedef struct avp_ae_limiter avp_ae_limiter_t;

/** @brief Create a streaming peak limiter. */
avp_status_t avp_ae_limiter_open(const avp_ae_limiter_config_t *config,
                                 avp_ae_limiter_t **handle);

/** @brief Release a limiter instance; NULL is accepted. */
void avp_ae_limiter_close(avp_ae_limiter_t *handle);

/** @brief Change or query limiter parameters at runtime. */
avp_status_t avp_ae_limiter_control(avp_ae_limiter_t *handle,
                                    avp_ae_limiter_cmd_t cmd,
                                    void *arg);

/**
 * @brief Process interleaved signed 16-bit PCM through the limiter.
 * @param[in] handle Limiter instance.
 * @param[in] in Input PCM buffer.
 * @param[out] out Output PCM buffer; may equal @p in.
 * @param[in] sample_count Total interleaved sample count.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_ae_limiter_process(avp_ae_limiter_t *handle,
                                    const int16_t *in,
                                    int16_t *out,
                                    uint32_t sample_count);

#ifdef __cplusplus
}
#endif

#endif
