/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AVP_AE_EQ_H
#define AVP_AE_EQ_H

#include "avp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Maximum number of parametric EQ bands. */
#define AVP_AE_EQ_MAX_BANDS 8u

/** @brief Maximum number of channels processed by the EQ. */
#define AVP_AE_EQ_MAX_CHANNELS 8u

/** @brief EQ filter types based on the RBJ biquad cookbook. */
typedef enum {
    AVP_AE_EQ_PEAKING = 0, /**< Peaking EQ band controlled by gain_db and q. */
    AVP_AE_EQ_LOW_SHELF,   /**< Low-frequency shelving filter. */
    AVP_AE_EQ_HIGH_SHELF,  /**< High-frequency shelving filter. */
    AVP_AE_EQ_LOW_PASS,    /**< Low-pass filter controlled by frequency and q. */
    AVP_AE_EQ_HIGH_PASS,   /**< High-pass filter controlled by frequency and q. */
} avp_ae_eq_type_t;

/** @brief One parametric EQ band. */
typedef struct {
    avp_ae_eq_type_t type; /**< Filter type. */
    float frequency;       /**< Center/cutoff frequency in Hz. */
    float gain_db;         /**< Peaking/shelf gain in dB. */
    float q;               /**< Q for peak/pass filters, or shelf slope. */
} avp_ae_eq_band_t;

/** @brief EQ runtime control commands. */
typedef enum {
    AVP_AE_EQ_CMD_SET_ENABLE = 0, /**< Set processing enable flag (int*; 0/1). */
    AVP_AE_EQ_CMD_GET_ENABLE,     /**< Get processing enable flag (int*). */
    AVP_AE_EQ_CMD_SET_BAND,       /**< Replace one band (avp_ae_eq_band_update_t*). */
    AVP_AE_EQ_CMD_GET_BAND,       /**< Query one band (avp_ae_eq_band_update_t*). */
    AVP_AE_EQ_CMD_RESET,          /**< Reset all filter states. */
} avp_ae_eq_cmd_t;

/** @brief Band index plus band parameters for runtime control. */
typedef struct {
    uint8_t index;         /**< Zero-based band index. */
    avp_ae_eq_band_t band; /**< New or returned band parameters. */
} avp_ae_eq_band_update_t;

/** @brief EQ configuration. */
typedef struct {
    uint32_t sample_rate;                        /**< Input sample rate in Hz; must be non-zero. */
    uint8_t channels;                            /**< Number of interleaved channels; 1..8. */
    uint8_t band_count;                          /**< Number of active bands; 0..8. */
    avp_ae_eq_band_t bands[AVP_AE_EQ_MAX_BANDS]; /**< Initial band parameters. */
    uint8_t enable;                              /**< Non-zero to process, zero to bypass. */
} avp_ae_eq_config_t;

typedef struct avp_ae_eq avp_ae_eq_t;

/**
 * @brief Create a parametric EQ instance.
 *
 * Input and output are interleaved signed 16-bit PCM and may alias.
 *
 * @param[in]  config  EQ configuration.
 * @param[out] handle  Receives the newly created instance.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_ae_eq_open(const avp_ae_eq_config_t *config,
                            avp_ae_eq_t **handle);

/** @brief Release an EQ instance; NULL is accepted. */
void avp_ae_eq_close(avp_ae_eq_t *handle);

/**
 * @brief Change or query EQ parameters at runtime.
 *
 * @param[in]     handle  EQ instance.
 * @param[in]     cmd     Control command; see @ref avp_ae_eq_cmd_t.
 * @param[in,out] arg     Command argument; type depends on @p cmd.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_ae_eq_control(avp_ae_eq_t *handle,
                               avp_ae_eq_cmd_t cmd,
                               void *arg);

/**
 * @brief Process interleaved signed 16-bit PCM samples through the EQ.
 *
 * @param[in]  handle        EQ instance.
 * @param[in]  in            Input PCM buffer.
 * @param[out] out           Output PCM buffer; may equal @p in.
 * @param[in]  sample_count  Total interleaved sample count.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_ae_eq_process(avp_ae_eq_t *handle,
                               const int16_t *in,
                               int16_t *out,
                               uint32_t sample_count);

#ifdef __cplusplus
}
#endif

#endif
