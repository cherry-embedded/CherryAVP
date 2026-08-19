/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AVP_AE_HOWLING_H
#define AVP_AE_HOWLING_H

#include "avp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Maximum number of adaptive notch filters. */
#define AVP_AE_HOWLING_MAX_NOTCHES 4u

/** @brief Default analysis frame length in samples per channel. */
#define AVP_AE_HOWLING_DEFAULT_FRAME_SAMPLES 256u

/** @brief Runtime control commands for the howling suppressor. */
typedef enum {
    AVP_AE_HOWLING_CMD_SET_ENABLE = 0,     /**< Set processing enable flag (int*; 0/1). */
    AVP_AE_HOWLING_CMD_GET_ENABLE,         /**< Get processing enable flag (int*). */
    AVP_AE_HOWLING_CMD_SET_THRESHOLD_DB,   /**< Set tonal peak threshold (float*). */
    AVP_AE_HOWLING_CMD_GET_THRESHOLD_DB,   /**< Get tonal peak threshold (float*). */
    AVP_AE_HOWLING_CMD_SET_NOTCH_Q,        /**< Set notch quality factor (float*). */
    AVP_AE_HOWLING_CMD_GET_NOTCH_Q,        /**< Get notch quality factor (float*). */
    AVP_AE_HOWLING_CMD_SET_MAX_NOTCHES,    /**< Set active notch limit (uint8_t*). */
    AVP_AE_HOWLING_CMD_GET_MAX_NOTCHES,    /**< Get active notch limit (uint8_t*). */
    AVP_AE_HOWLING_CMD_GET_ACTIVE_NOTCHES, /**< Get active notch count (uint8_t*). */
    AVP_AE_HOWLING_CMD_RESET,              /**< Clear analysis state and all active notches. */
} avp_ae_howling_cmd_t;

/** @brief Howling suppressor instance configuration. */
typedef struct {
    uint32_t sample_rate;   /**< Input sample rate in Hz; must be non-zero. */
    uint8_t channels;       /**< Number of interleaved channels; mono or stereo. */
    uint16_t frame_samples; /**< Analysis frame length; 64..1024 and a power of two. */
    uint8_t max_notches;    /**< Maximum simultaneous notch filters, 1..4. */
    float threshold_db;     /**< Required peak-to-average ratio in dB; 6..60. */
    float notch_q;          /**< Notch quality factor; 2..30. */
    uint8_t enable;         /**< Non-zero to suppress detected tones. */
} avp_ae_howling_config_t;

typedef struct avp_ae_howling avp_ae_howling_t;

/**
 * @brief Create an adaptive howling suppressor.
 *
 * The suppressor analyzes short mono downmix frames and places narrow biquad
 * notch filters at strong, narrow-band peaks. Input and output may alias.
 *
 * @param[in]  config  Howling suppressor configuration.
 * @param[out] handle  Receives the newly created instance.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_ae_howling_open(const avp_ae_howling_config_t *config,
                                 avp_ae_howling_t **handle);

/**
 * @brief Release all resources owned by a howling suppressor.
 *
 * @param[in] handle  Instance handle; NULL is accepted.
 */
void avp_ae_howling_close(avp_ae_howling_t *handle);

/**
 * @brief Change or query howling suppressor parameters at runtime.
 *
 * @param[in]     handle  Instance handle.
 * @param[in]     cmd     Control command; see @ref avp_ae_howling_cmd_t.
 * @param[in,out] arg     Command argument; type depends on @p cmd and may be NULL for reset.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_ae_howling_control(avp_ae_howling_t *handle,
                                    avp_ae_howling_cmd_t cmd,
                                    void *arg);

/**
 * @brief Process interleaved signed 16-bit PCM samples.
 *
 * @p sample_count is the total number of interleaved values, including all
 * channels. The function is streaming-safe and accepts arbitrary chunk sizes.
 *
 * @param[in]  handle        Howling suppressor instance.
 * @param[in]  in            Input PCM buffer.
 * @param[out] out           Output PCM buffer; may equal @p in.
 * @param[in]  sample_count  Number of interleaved int16 samples.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_ae_howling_process(avp_ae_howling_t *handle,
                                    const int16_t *in,
                                    int16_t *out,
                                    uint32_t sample_count);

#ifdef __cplusplus
}
#endif

#endif
