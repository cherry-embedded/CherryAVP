/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AVP_AE_FILTER_H
#define AVP_AE_FILTER_H

#include "avp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Maximum number of channels processed by the filter. */
#define AVP_AE_FILTER_MAX_CHANNELS 8u

/** @brief Single biquad filter types based on the RBJ audio EQ cookbook. */
typedef enum {
    AVP_AE_FILTER_LOW_PASS = 0,  /**< Low-pass filter controlled by frequency and q. */
    AVP_AE_FILTER_HIGH_PASS,     /**< High-pass filter controlled by frequency and q. */
    AVP_AE_FILTER_BAND_PASS,     /**< Band-pass filter controlled by center frequency and q. */
    AVP_AE_FILTER_BAND_STOP,     /**< Band-stop/notch filter controlled by center frequency and q. */
    AVP_AE_FILTER_NOTCH = AVP_AE_FILTER_BAND_STOP, /**< Alias for band-stop filter. */
    AVP_AE_FILTER_ALL_PASS,      /**< All-pass phase filter controlled by frequency and q. */
    AVP_AE_FILTER_PEAKING,       /**< Peaking EQ filter controlled by frequency, q, and gain_db. */
    AVP_AE_FILTER_LOW_SHELF,     /**< Low-frequency shelving filter controlled by frequency, slope, and gain_db. */
    AVP_AE_FILTER_HIGH_SHELF,    /**< High-frequency shelving filter controlled by frequency, slope, and gain_db. */
} avp_ae_filter_type_t;

/** @brief Filter runtime control commands. */
typedef enum {
    AVP_AE_FILTER_CMD_SET_ENABLE = 0, /**< Set processing enable flag (int*; 0/1). */
    AVP_AE_FILTER_CMD_GET_ENABLE,     /**< Get processing enable flag (int*). */
    AVP_AE_FILTER_CMD_SET_CONFIG,     /**< Replace full configuration (avp_ae_filter_config_t*). */
    AVP_AE_FILTER_CMD_GET_CONFIG,     /**< Query full configuration (avp_ae_filter_config_t*). */
    AVP_AE_FILTER_CMD_SET_TYPE,       /**< Set filter type (avp_ae_filter_type_t*). */
    AVP_AE_FILTER_CMD_GET_TYPE,       /**< Get filter type (avp_ae_filter_type_t*). */
    AVP_AE_FILTER_CMD_SET_FREQUENCY,  /**< Set cutoff/center frequency in Hz (float*). */
    AVP_AE_FILTER_CMD_GET_FREQUENCY,  /**< Get cutoff/center frequency in Hz (float*). */
    AVP_AE_FILTER_CMD_SET_Q,          /**< Set Q or shelf slope (float*). */
    AVP_AE_FILTER_CMD_GET_Q,          /**< Get Q or shelf slope (float*). */
    AVP_AE_FILTER_CMD_SET_GAIN_DB,    /**< Set peaking/shelf gain in dB (float*). */
    AVP_AE_FILTER_CMD_GET_GAIN_DB,    /**< Get peaking/shelf gain in dB (float*). */
    AVP_AE_FILTER_CMD_RESET,          /**< Reset filter delay states. */
} avp_ae_filter_cmd_t;

/** @brief Biquad filter configuration. */
typedef struct {
    uint32_t sample_rate;        /**< Input sample rate in Hz; must be non-zero. */
    uint8_t channels;            /**< Number of interleaved channels; 1..8. */
    avp_ae_filter_type_t type;   /**< Filter type. */
    float frequency;             /**< Cutoff/center frequency in Hz. */
    float q;                     /**< Q for most filters, or shelf slope for shelf filters. */
    float gain_db;               /**< Peaking/shelf gain in dB, -24..24. Ignored by other filters. */
    uint8_t enable;              /**< Non-zero to process, zero to bypass. */
} avp_ae_filter_config_t;

typedef struct avp_ae_filter avp_ae_filter_t;

/**
 * @brief Create a streaming biquad audio filter.
 *
 * Input and output are interleaved signed 16-bit PCM and may alias.
 *
 * @param[in]  config  Filter configuration.
 * @param[out] handle  Receives the newly created instance.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_ae_filter_open(const avp_ae_filter_config_t *config,
                                avp_ae_filter_t **handle);

/** @brief Release a filter instance; NULL is accepted. */
void avp_ae_filter_close(avp_ae_filter_t *handle);

/**
 * @brief Change or query filter parameters at runtime.
 *
 * @param[in]     handle  Filter instance.
 * @param[in]     cmd     Control command; see @ref avp_ae_filter_cmd_t.
 * @param[in,out] arg     Command argument; type depends on @p cmd.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_ae_filter_control(avp_ae_filter_t *handle,
                                   avp_ae_filter_cmd_t cmd,
                                   void *arg);

/**
 * @brief Process interleaved signed 16-bit PCM through the filter.
 *
 * @param[in]  handle        Filter instance.
 * @param[in]  in            Input PCM buffer.
 * @param[out] out           Output PCM buffer; may equal @p in.
 * @param[in]  sample_count  Total interleaved sample count.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_ae_filter_process(avp_ae_filter_t *handle,
                                   const int16_t *in,
                                   int16_t *out,
                                   uint32_t sample_count);

#ifdef __cplusplus
}
#endif

#endif
