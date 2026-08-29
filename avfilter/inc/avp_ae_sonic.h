/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AVP_AE_SONIC_H
#define AVP_AE_SONIC_H

#include "avp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct avp_ae_sonic avp_ae_sonic_t;

/** @brief Sonic runtime control commands. */
typedef enum {
    AVP_AE_SONIC_CMD_SET_SPEED = 0, /**< Set playback speed multiplier (float*). */
    AVP_AE_SONIC_CMD_GET_SPEED,     /**< Get playback speed multiplier (float*). */
    AVP_AE_SONIC_CMD_SET_PITCH,     /**< Set pitch multiplier (float*). */
    AVP_AE_SONIC_CMD_GET_PITCH,     /**< Get pitch multiplier (float*). */
    AVP_AE_SONIC_CMD_FLUSH,         /**< Flush internal buffers to drain tail samples. */
    AVP_AE_SONIC_CMD_GET_AVAILABLE, /**< Get samples per channel available for output (uint32_t*). */
} avp_ae_sonic_cmd_t;

/** @brief Sonic instance configuration. */
typedef struct {
    uint32_t sample_rate; /**< Input sample rate in Hz; must be non-zero. */
    uint8_t channels;     /**< Number of interleaved channels. */
    float speed;          /**< Playback speed multiplier; 0 defaults to 1.0. */
    float pitch;          /**< Pitch multiplier; 0 defaults to 1.0. */
} avp_ae_sonic_config_t;

/**
 * @brief Create a Sonic time/pitch processor instance.
 *
 * @p sample_rate must be non-zero. @p channels supports 1 through
 * AVP_AE_SONIC_MAX_CHANNELS. @p speed, @p pitch and @p rate default to 1.0
 * when set to 0 in the configuration.
 *
 * @param[in]  config  Pointer to the Sonic configuration structure.
 * @param[out] handle  Receives the newly created Sonic instance pointer.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_ae_sonic_open(const avp_ae_sonic_config_t *config,
                               avp_ae_sonic_t **handle);

/**
 * @brief Release all resources owned by the Sonic instance.
 *
 * @param[in] handle  Sonic instance handle to destroy; NULL is accepted.
 */
void avp_ae_sonic_close(avp_ae_sonic_t *handle);

/**
 * @brief Change or query Sonic parameters at runtime.
 *
 * Float commands use @c float*. @c AVP_AE_SONIC_CMD_SET/GET_SAMPLE_RATE uses
 * @c uint32_t*. @c AVP_AE_SONIC_CMD_SET/GET_CHANNELS uses @c uint8_t*.
 * @c AVP_AE_SONIC_CMD_SET/GET_CHORD_PITCH and @c AVP_AE_SONIC_CMD_SET/GET_QUALITY
 * use @c int*. @c AVP_AE_SONIC_CMD_GET_AVAILABLE uses @c uint32_t* and returns
 * the number of samples per channel currently buffered for output.
 *
 * @param[in]     handle  Sonic instance handle.
 * @param[in]     cmd     Control command; see @ref avp_ae_sonic_cmd_t.
 * @param[in,out] arg     Command argument; type depends on @p cmd.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_ae_sonic_control(avp_ae_sonic_t *handle,
                                  avp_ae_sonic_cmd_t cmd,
                                  void *arg);

/**
 * @brief Process interleaved signed 16-bit PCM through the Sonic pipeline.
 *
 * @p in_samples, @p out_capacity and @p out_samples are expressed as samples
 * per channel, not as the total number of interleaved @c int16_t values.
 * The output may contain fewer samples than the input because Sonic is a
 * streaming processor and because speed/pitch/rate changes alter duration.
 *
 * To drain the tail, issue @ref AVP_AE_SONIC_CMD_FLUSH and then call this
 * function with @p in = NULL and @p in_samples = 0 until @p out_samples
 * returns 0.
 *
 * @param[in]  handle        Sonic instance handle.
 * @param[in]  in            Interleaved input PCM buffer; NULL when draining.
 * @param[in]  in_samples    Number of input samples per channel.
 * @param[out] out           Interleaved output PCM buffer.
 * @param[in]  out_capacity  Capacity of @p out in samples per channel.
 * @param[out] out_samples   Receives the number of output samples per channel.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_ae_sonic_process(avp_ae_sonic_t *handle,
                                  const int16_t *in,
                                  uint32_t in_samples,
                                  int16_t *out,
                                  uint32_t out_capacity,
                                  uint32_t *out_samples);

#ifdef __cplusplus
}
#endif

#endif
