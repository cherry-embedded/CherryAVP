/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AVP_AE_REVERB_H
#define AVP_AE_REVERB_H

#include "avp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Number of parallel delay lines in the reverb network. */
#define AVP_AE_REVERB_DELAY_LINES 4u

/** @brief Reverb runtime control commands. */
typedef enum {
    AVP_AE_REVERB_CMD_SET_ENABLE = 0, /**< Set processing enable flag (int*; 0/1). */
    AVP_AE_REVERB_CMD_GET_ENABLE,     /**< Get processing enable flag (int*). */
    AVP_AE_REVERB_CMD_SET_ROOM_SIZE,  /**< Set room size/feedback (float*; 0..1). */
    AVP_AE_REVERB_CMD_GET_ROOM_SIZE,  /**< Get room size/feedback (float*). */
    AVP_AE_REVERB_CMD_SET_DAMPING,    /**< Set high-frequency damping (float*; 0..1). */
    AVP_AE_REVERB_CMD_GET_DAMPING,    /**< Get high-frequency damping (float*). */
    AVP_AE_REVERB_CMD_SET_WET,        /**< Set wet mix (float*; 0..1). */
    AVP_AE_REVERB_CMD_GET_WET,        /**< Get wet mix (float*). */
    AVP_AE_REVERB_CMD_SET_DRY,        /**< Set dry mix (float*; 0..1). */
    AVP_AE_REVERB_CMD_GET_DRY,        /**< Get dry mix (float*). */
    AVP_AE_REVERB_CMD_RESET,          /**< Clear all delay memory. */
} avp_ae_reverb_cmd_t;

/** @brief Reverb configuration. */
typedef struct {
    uint32_t sample_rate; /**< Input sample rate in Hz; must be non-zero. */
    uint8_t channels;     /**< Number of interleaved channels; 1..2. */
    float room_size;      /**< Feedback amount, 0..1. */
    float damping;        /**< High-frequency damping, 0..1. */
    float wet;            /**< Reverberated signal mix, 0..1. */
    float dry;            /**< Direct signal mix, 0..1. */
    uint8_t enable;       /**< Non-zero to process, zero to bypass. */
} avp_ae_reverb_config_t;

typedef struct avp_ae_reverb avp_ae_reverb_t;

/** @brief Create a streaming Schroeder-style reverb. */
avp_status_t avp_ae_reverb_open(const avp_ae_reverb_config_t *config,
                                avp_ae_reverb_t **handle);

/** @brief Release a reverb instance; NULL is accepted. */
void avp_ae_reverb_close(avp_ae_reverb_t *handle);

/** @brief Change or query reverb parameters at runtime. */
avp_status_t avp_ae_reverb_control(avp_ae_reverb_t *handle,
                                   avp_ae_reverb_cmd_t cmd,
                                   void *arg);

/**
 * @brief Process interleaved signed 16-bit PCM through the reverb.
 * @param[in] handle Reverb instance.
 * @param[in] in Input PCM buffer.
 * @param[out] out Output PCM buffer; may equal @p in.
 * @param[in] sample_count Total interleaved sample count.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_ae_reverb_process(avp_ae_reverb_t *handle,
                                   const int16_t *in,
                                   int16_t *out,
                                   uint32_t sample_count);

#ifdef __cplusplus
}
#endif

#endif
