/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AVP_AE_COMPRESSOR_H
#define AVP_AE_COMPRESSOR_H

#include "avp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Compressor runtime control commands. */
typedef enum {
    AVP_AE_COMPRESSOR_CMD_SET_ENABLE = 0,   /**< Set processing enable flag (int*; 0/1). */
    AVP_AE_COMPRESSOR_CMD_GET_ENABLE,       /**< Get processing enable flag (int*). */
    AVP_AE_COMPRESSOR_CMD_SET_THRESHOLD_DB, /**< Set threshold in dBFS (float*). */
    AVP_AE_COMPRESSOR_CMD_GET_THRESHOLD_DB, /**< Get threshold in dBFS (float*). */
    AVP_AE_COMPRESSOR_CMD_SET_RATIO,        /**< Set compression ratio (float*; 1..20). */
    AVP_AE_COMPRESSOR_CMD_GET_RATIO,        /**< Get compression ratio (float*). */
    AVP_AE_COMPRESSOR_CMD_SET_ATTACK_MS,    /**< Set attack time (float*). */
    AVP_AE_COMPRESSOR_CMD_GET_ATTACK_MS,    /**< Get attack time (float*). */
    AVP_AE_COMPRESSOR_CMD_SET_RELEASE_MS,   /**< Set release time (float*). */
    AVP_AE_COMPRESSOR_CMD_GET_RELEASE_MS,   /**< Get release time (float*). */
    AVP_AE_COMPRESSOR_CMD_SET_MAKEUP_DB,    /**< Set makeup gain (float*). */
    AVP_AE_COMPRESSOR_CMD_GET_MAKEUP_DB,    /**< Get makeup gain (float*). */
    AVP_AE_COMPRESSOR_CMD_RESET,            /**< Reset envelope and gain state. */
} avp_ae_compressor_cmd_t;

/** @brief Compressor configuration. */
typedef struct {
    uint32_t sample_rate; /**< Input sample rate in Hz; must be non-zero. */
    uint8_t channels;     /**< Number of interleaved channels; 1..8. */
    float threshold_db;   /**< Threshold in dBFS, -60..0. */
    float ratio;          /**< Compression ratio, 1..20. */
    float attack_ms;      /**< Attack time in milliseconds. */
    float release_ms;     /**< Release time in milliseconds. */
    float makeup_db;      /**< Output makeup gain in dB, -24..24. */
    uint8_t enable;       /**< Non-zero to process, zero to bypass. */
} avp_ae_compressor_config_t;

typedef struct avp_ae_compressor avp_ae_compressor_t;

/** @brief Create a streaming dynamics compressor. */
avp_status_t avp_ae_compressor_open(const avp_ae_compressor_config_t *config,
                                    avp_ae_compressor_t **handle);

/** @brief Release a compressor instance; NULL is accepted. */
void avp_ae_compressor_close(avp_ae_compressor_t *handle);

/** @brief Change or query compressor parameters at runtime. */
avp_status_t avp_ae_compressor_control(avp_ae_compressor_t *handle,
                                       avp_ae_compressor_cmd_t cmd,
                                       void *arg);

/**
 * @brief Process interleaved signed 16-bit PCM through the compressor.
 * @param[in] handle Compressor instance.
 * @param[in] in Input PCM buffer.
 * @param[out] out Output PCM buffer; may equal @p in.
 * @param[in] sample_count Total interleaved sample count.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_ae_compressor_process(avp_ae_compressor_t *handle,
                                       const int16_t *in,
                                       int16_t *out,
                                       uint32_t sample_count);

#ifdef __cplusplus
}
#endif

#endif
