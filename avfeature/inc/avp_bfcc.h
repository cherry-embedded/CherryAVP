/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AVP_BFCC_H
#define AVP_BFCC_H

#include "avp_common.h"

#define AVP_BFCC_MAX_NUM_COEFFS 22u

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Frame window configuration in milliseconds. */
typedef struct {
    uint32_t size_ms;       /**< Frame length in milliseconds. */
    uint32_t step_size_ms;  /**< Frame step in milliseconds. */
} avp_bfcc_window_config_t;

/** @brief Bark filterbank configuration. */
typedef struct {
    uint32_t num_channels;     /**< Number of Bark channels. */
    uint32_t lower_band_limit; /**< Lowest Bark band edge in Hz. */
    uint32_t upper_band_limit; /**< Highest Bark band edge in Hz. */
} avp_bfcc_filterbank_config_t;

/** @brief BFCC processing configuration. */
typedef struct {
    uint32_t sample_rate;                     /**< Input sample rate in Hz. */
    avp_bfcc_window_config_t window;          /**< Framing configuration. */
    avp_bfcc_filterbank_config_t filterbank; /**< Bark filterbank configuration. */
    uint32_t num_coefficients;                /**< Number of BFCC coefficients per frame. */
} avp_bfcc_config_t;

typedef struct {
    float bfcc_value[AVP_BFCC_MAX_NUM_COEFFS]; /**< BFCC coefficients buffer. */
    uint32_t bfcc_size;                        /**< Number of valid coefficients. */
} avp_bfcc_frame_t;

typedef struct avp_bfcc avp_bfcc_t;

#ifdef CONFIG_CHERRYAVP_BFCC_RFFT_OVERRIDE
/**
 * @brief Run the platform Q15 real FFT used by the BFCC override path.
 *
 * @param[in,out] src In-place Q15 real FFT buffer.
 * @param[in] m       Base-2 logarithm of the FFT sample count.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
int avp_bfcc_dsp_rfft_q15(int16_t *src, uint32_t m);
#endif

/** @brief Create a BFCC processor. */
avp_status_t avp_bfcc_open(const avp_bfcc_config_t *config,
                           avp_bfcc_t **handle);

/** @brief Release a BFCC processor. NULL is accepted. */
void avp_bfcc_close(avp_bfcc_t *handle);

/**
 * @brief Process PCM16 samples and emit at most one BFCC frame.
 *
 * Input is mono PCM16. The processor retains an incomplete frame between
 * calls. Callers should normally pass no more than the step returned by
 * avp_bfcc_get_step_samples().
 */
int avp_bfcc_process(avp_bfcc_t *handle,
                     const int16_t *input,
                     uint32_t sample_count,
                     uint32_t *sample_read,
                     avp_bfcc_frame_t *frame);

/** @return Input samples expected for one process call, or zero for NULL. */
uint32_t avp_bfcc_get_step_samples(const avp_bfcc_t *handle);

/**
 * @brief Get the number of complete BFCC frames in one second of audio.
 *
 * The result is calculated from @p config->sample_rate, frame length, and
 * frame step. It returns zero for an invalid configuration.
 */
uint32_t avp_bfcc_get_1s_frame_count(const avp_bfcc_config_t *config);

#ifdef __cplusplus
}
#endif

#endif
