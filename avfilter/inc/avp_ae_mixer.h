/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AVP_AE_MIXER_H
#define AVP_AE_MIXER_H

#include "avp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AVP_AE_MIXER_MAX_INPUTS 8u

/** @brief Mixer runtime control commands. */
typedef enum {
    AVP_AE_MIXER_CMD_SET_ENABLE = 0, /**< Set processing enable flag (int*; 0/1). */
    AVP_AE_MIXER_CMD_GET_ENABLE,     /**< Get processing enable flag (int*). */
    AVP_AE_MIXER_CMD_SET_MODE,       /**< Set fade mode (avp_ae_mixer_mode_t*). */
    AVP_AE_MIXER_CMD_GET_MODE,       /**< Get fade mode (avp_ae_mixer_mode_t*). */
    AVP_AE_MIXER_CMD_SET_INPUT_CFG,  /**< Set one input config (avp_ae_mixer_input_control_t*). */
    AVP_AE_MIXER_CMD_GET_INPUT_CFG,  /**< Get one input config (avp_ae_mixer_input_control_t*). */
} avp_ae_mixer_cmd_t;

/** @brief Mixer fade direction. */
typedef enum {
    AVP_AE_MIXER_MODE_FADE_DOWNWARD = 0, /**< Fade each input toward weight1. */
    AVP_AE_MIXER_MODE_FADE_UPWARD,       /**< Fade each input toward weight2. */
} avp_ae_mixer_mode_t;

/** @brief Per-input mixer weight configuration. */
typedef struct {
    float weight1;         /**< Initial weight and stable weight in FADE_DOWNWARD mode. */
    float weight2;         /**< Stable weight in FADE_UPWARD mode. */
    uint32_t transit_time; /**< Fade time between weight1 and weight2, in ms. */
} avp_ae_mixer_input_cfg_t;

/** @brief Runtime update/query for one mixer input. */
typedef struct {
    uint32_t input;               /**< Zero-based input index. */
    avp_ae_mixer_input_cfg_t cfg; /**< Per-input weight and transition configuration. */
} avp_ae_mixer_input_control_t;

/** @brief Mixer configuration. */
typedef struct {
    uint32_t sample_rate;                      /**< Input sample rate in Hz; must be non-zero. */
    uint8_t channels;                          /**< Number of interleaved channels. */
    uint32_t input_count;                      /**< Number of input buffers. */
    const avp_ae_mixer_input_cfg_t *input_cfg; /**< Per-input config array; input_count entries. */
    avp_ae_mixer_mode_t mode;                  /**< Initial fade mode. */
    uint8_t enable;                            /**< Non-zero to mix, zero to copy the first input. */
} avp_ae_mixer_config_t;

typedef struct avp_ae_mixer avp_ae_mixer_t;

/**
 * @brief Create a PCM mixer.
 *
 * All input buffers must contain the same number of interleaved samples and
 * use the configured channel count. Input and output buffers may alias.
 *
 * @param[in]  config  Mixer configuration.
 * @param[out] handle  Receives the newly created mixer instance.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_ae_mixer_open(const avp_ae_mixer_config_t *config,
                               avp_ae_mixer_t **handle);

/** @brief Release a mixer instance; NULL is accepted. */
void avp_ae_mixer_close(avp_ae_mixer_t *handle);

/**
 * @brief Change or query mixer controls.
 *
 * @param[in]     handle  Mixer instance.
 * @param[in]     cmd     Control command; see @ref avp_ae_mixer_cmd_t.
 * @param[in,out] arg     Command argument; type depends on @p cmd.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_ae_mixer_control(avp_ae_mixer_t *handle,
                                  avp_ae_mixer_cmd_t cmd,
                                  void *arg);

/**
 * @brief Mix interleaved signed 16-bit PCM buffers.
 *
 * When enabled, the mixer output is computed as
 * @c output=w0^2*input0+w1^2*input1+... using the current per-input fade
 * weights. Fade progress advances by PCM frame, not by interleaved sample.
 *
 * @param[in]  handle        Mixer instance.
 * @param[in]  inputs        Array of @c input_count input pointers.
 * @param[out] out           Output PCM buffer; may alias an input.
 * @param[in]  sample_count  Total interleaved sample count.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_ae_mixer_process(avp_ae_mixer_t *handle,
                                  const int16_t *const *inputs,
                                  int16_t *out,
                                  uint32_t sample_count);

#ifdef __cplusplus
}
#endif

#endif
