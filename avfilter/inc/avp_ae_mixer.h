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

/** @brief Maximum number of interleaved PCM inputs. */
#define AVP_AE_MIXER_MAX_INPUTS 4u

/** @brief Mixer runtime control commands. */
typedef enum {
    AVP_AE_MIXER_CMD_SET_ENABLE = 0, /**< Set processing enable flag (int*; 0/1). */
    AVP_AE_MIXER_CMD_GET_ENABLE,     /**< Get processing enable flag (int*). */
    AVP_AE_MIXER_CMD_SET_INPUT_GAIN, /**< Set one input gain (avp_ae_mixer_gain_t*). */
    AVP_AE_MIXER_CMD_GET_INPUT_GAIN, /**< Get one input gain (avp_ae_mixer_gain_t*). */
    AVP_AE_MIXER_CMD_RESET_GAINS,    /**< Reset all gains to 1.0. */
} avp_ae_mixer_cmd_t;

/** @brief Gain update/query for one mixer input. */
typedef struct {
    uint8_t input; /**< Zero-based input index. */
    float gain;    /**< Linear gain, normally in the range 0.0..4.0. */
} avp_ae_mixer_gain_t;

/** @brief Mixer configuration. */
typedef struct {
    uint32_t sample_rate;                 /**< Input sample rate in Hz; must be non-zero. */
    uint8_t channels;                     /**< Number of interleaved channels; 1..8. */
    uint8_t input_count;                  /**< Number of input buffers; 1..4. */
    float gains[AVP_AE_MIXER_MAX_INPUTS]; /**< Initial linear gains. */
    uint8_t enable;                       /**< Non-zero to mix, zero to copy the first input. */
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
