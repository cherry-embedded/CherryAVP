/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AVP_AFE_HOWLING_H
#define AVP_AFE_HOWLING_H

#include "avp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Maximum number of adaptive notch filters. */
#define AVP_AFE_HOWLING_MAX_NOTCHES 4u

/** @brief Analysis frame duration used to derive the FFT length from the sample rate. */
#define AVP_AFE_HOWLING_FRAME_MS 10u

/**
 * @brief Built-in FFT formats.
 *
 * @c uint32_t m is the FFT length in samples. The float format uses
 * interleaved real/imaginary input and output, while the int16 format follows
 * WebRTC ComplexFFT's interleaved real/imaginary representation.
 */
typedef enum {
    AVP_AFE_HOWLING_FFT_FLOAT = 0, /**< @c avp_afe_howling_fft_f32_t. */
    AVP_AFE_HOWLING_FFT_INT16,     /**< @c avp_afe_howling_fft_i16_t. */
} avp_afe_howling_fft_type_t;

/** @brief In-place radix-2 FFT callback: @p src is interleaved real/imaginary. */
typedef void (*avp_afe_howling_fft_f32_t)(float *src, uint32_t m);

/** @brief In-place FFT callback: @p src is interleaved real/imaginary. */
typedef void (*avp_afe_howling_fft_i16_t)(int16_t *src, uint32_t m);

/** @brief External FFT callback and its input format. */
typedef struct {
    avp_afe_howling_fft_type_t type; /**< Callback argument format. */
    union {
        avp_afe_howling_fft_f32_t f32; /**< Float FFT callback. */
        avp_afe_howling_fft_i16_t i16; /**< Int16 FFT callback. */
    } fft;                             /**< FFT implementation. */
} avp_afe_howling_fft_t;

/** @brief Runtime control commands for the howling suppressor. */
typedef enum {
    AVP_AFE_HOWLING_CMD_SET_ENABLE = 0,     /**< Set processing enable flag (int*; 0/1). */
    AVP_AFE_HOWLING_CMD_GET_ENABLE,         /**< Get processing enable flag (int*). */
    AVP_AFE_HOWLING_CMD_SET_PAPR_TH,        /**< Set PAPR threshold in dB (float*). */
    AVP_AFE_HOWLING_CMD_GET_PAPR_TH,        /**< Get PAPR threshold in dB (float*). */
    AVP_AFE_HOWLING_CMD_SET_PHPR_TH,        /**< Set PHPR threshold in dB (float*). */
    AVP_AFE_HOWLING_CMD_GET_PHPR_TH,        /**< Get PHPR threshold in dB (float*). */
    AVP_AFE_HOWLING_CMD_SET_PNPR_TH,        /**< Set PNPR threshold in dB (float*). */
    AVP_AFE_HOWLING_CMD_GET_PNPR_TH,        /**< Get PNPR threshold in dB (float*). */
    AVP_AFE_HOWLING_CMD_SET_NOTCH_Q,        /**< Set notch quality factor (float*). */
    AVP_AFE_HOWLING_CMD_GET_NOTCH_Q,        /**< Get notch quality factor (float*). */
    AVP_AFE_HOWLING_CMD_SET_MAX_NOTCHES,    /**< Set active notch limit (uint8_t*). */
    AVP_AFE_HOWLING_CMD_GET_MAX_NOTCHES,    /**< Get active notch limit (uint8_t*). */
    AVP_AFE_HOWLING_CMD_GET_ACTIVE_NOTCHES, /**< Get active notch count (uint8_t*). */
    AVP_AFE_HOWLING_CMD_RESET,              /**< Clear analysis state and all active notches. */
} avp_afe_howling_cmd_t;

/** @brief Howling suppressor instance configuration. */
typedef struct {
    uint32_t sample_rate;      /**< Input sample rate in Hz; must be non-zero. */
    uint8_t channels;          /**< Number of interleaved channels; 1..255. */
    uint8_t max_notches;       /**< Maximum simultaneous notch filters, 1..4. */
    float papr_th;             /**< PAPR threshold in dB; -10..20. */
    float phpr_th;             /**< PHPR threshold in dB; 0..100. */
    float pnpr_th;             /**< PNPR threshold in dB; 0..100. */
    float notch_q;             /**< Notch quality factor; 2..30. */
    uint8_t enable;            /**< Non-zero to suppress detected tones. */
    avp_afe_howling_fft_t fft; /**< Optional external FFT; use built-in FFT when NULL. */
} avp_afe_howling_config_t;

typedef struct avp_afe_howling avp_afe_howling_t;

/**
 * @brief Create an adaptive howling suppressor.
 *
 * The suppressor analyzes a mono downmix of short frames and places narrow
 * biquad notch filters at strong, narrow-band peaks. Each interleaved channel
 * is filtered with independent notch state. Input and output may alias.
 *
 * @param[in]  config  Howling suppressor configuration.
 * @param[out] handle  Receives the newly created instance.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_afe_howling_open(const avp_afe_howling_config_t *config,
                                  avp_afe_howling_t **handle);

/**
 * @brief Release all resources owned by a howling suppressor.
 *
 * @param[in] handle  Instance handle; NULL is accepted.
 */
void avp_afe_howling_close(avp_afe_howling_t *handle);

/**
 * @brief Change or query howling suppressor parameters at runtime.
 *
 * @param[in]     handle  Instance handle.
 * @param[in]     cmd     Control command; see @ref avp_afe_howling_cmd_t.
 * @param[in,out] arg     Command argument; type depends on @p cmd and may be NULL for reset.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_afe_howling_control(avp_afe_howling_t *handle,
                                     avp_afe_howling_cmd_t cmd,
                                     void *arg);

/**
 * @brief Process one analysis frame of interleaved signed 16-bit PCM samples.
 *
 * @p sample_count must equal
 * <tt>avp_afe_howling_get_frame_samples(handle) * channels</tt>, so exactly
 * one FFT analysis frame is processed per call.
 *
 * @param[in]  handle        Howling suppressor instance.
 * @param[in]  in            Input PCM buffer.
 * @param[out] out           Output PCM buffer; may equal @p in.
 * @param[in]  sample_count  Number of interleaved int16 samples; must be
 *                           avp_afe_howling_get_frame_samples(handle) * channels.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_afe_howling_process(avp_afe_howling_t *handle,
                                     const int16_t *in,
                                     int16_t *out,
                                     uint32_t sample_count);

/**
 * @brief Return the analysis frame length in samples per channel.
 *
 * The frame length is derived from @c sample_rate, targeting
 * @ref AVP_AFE_HOWLING_FRAME_MS milliseconds rounded down to a power of two
 * and clamped to 64..1024.
 *
 * @param[in] handle  Instance handle.
 * @return Frame samples per channel, or 0 if @p handle is NULL.
 */
uint32_t avp_afe_howling_get_frame_samples(const avp_afe_howling_t *handle);

#ifdef __cplusplus
}
#endif

#endif
