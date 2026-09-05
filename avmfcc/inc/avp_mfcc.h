/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AVP_MFCC_H
#define AVP_MFCC_H

#include "avp_common.h"

#define AVP_MFCC_MAX_NUM_COEFFS 13u

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Frame window configuration in milliseconds. */
typedef struct {
    uint32_t size_ms;      /**< Frame length in milliseconds. */
    uint32_t step_size_ms; /**< Frame step in milliseconds. */
} avp_mfcc_window_config_t;

/** @brief Mel filterbank configuration. */
typedef struct {
    uint32_t num_channels;     /**< Number of mel channels. */
    uint32_t lower_band_limit; /**< Lowest mel band edge in Hz. */
    uint32_t upper_band_limit; /**< Highest mel band edge in Hz. */
} avp_mfcc_filterbank_config_t;

/** @brief MFCC processing configuration. */
typedef struct {
    uint32_t sample_rate;                    /**< Input sample rate in Hz. */
    avp_mfcc_window_config_t window;         /**< Framing configuration. */
    avp_mfcc_filterbank_config_t filterbank; /**< Mel filterbank configuration. */
    uint32_t num_coefficients;               /**< Number of MFCC coefficients per frame. */
} avp_mfcc_config_t;

typedef struct {
    float mfcc_value[AVP_MFCC_MAX_NUM_COEFFS]; /**< MFCC coefficients buffer. */
    uint32_t mfcc_size;                        /**< Number of valid coefficients in @p mfcc_value. */
} avp_mfcc_frame_t;

typedef struct avp_mfcc avp_mfcc_t;

#ifdef CONFIG_CHERRYAVP_MFCC_RFFT_OVERRIDE
/**
 * @brief Run the platform Q15 real FFT used by the MFCC override path.
 *
 * @param[in,out] src  In-place Q15 real FFT buffer. The sample count is 1 << m.
 * @param[in]     m    Base-2 logarithm of the FFT sample count.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
int avp_mfcc_dsp_rfft_q15(int16_t *src, uint32_t m);
#endif

/**
 * @brief Create an MFCC processor.
 *
 * The processor consumes mono PCM16 audio in streaming chunks and emits one
 * MFCC vector per completed frame.
 *
 * @param[in]  config  MFCC configuration.
 * @param[out] handle  Receives the new processor instance.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_mfcc_open(const avp_mfcc_config_t *config,
                           avp_mfcc_t **handle);

/**
 * @brief Release an MFCC processor.
 *
 * @param[in] handle  Processor instance; NULL is accepted.
 */
void avp_mfcc_close(avp_mfcc_t *handle);

/**
 * @brief Process input PCM16 samples and emit at most one MFCC frame.
 *
 * The processor keeps any incomplete tail in its internal buffer. When there
 * are not enough samples to complete a frame, the call returns @ref AVP_OK
 * with @p frame->mfcc_size set to zero. Callers should normally pass at most
 * the value returned by @ref avp_mfcc_get_step_samples.
 *
 * @param[in]  handle        Processor instance.
 * @param[in]  input         Input PCM16 samples.
 * @param[in]  sample_count  Number of input samples in @p input.
 * @param[out] sample_read   Number of input samples consumed.
 * @param[out] frame         MFCC frame result. @p mfcc_size is zero when no frame is ready yet.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
int avp_mfcc_process(avp_mfcc_t *handle,
                     const int16_t *input,
                     uint32_t sample_count,
                     uint32_t *sample_read,
                     avp_mfcc_frame_t *frame);

/**
 * @brief Get the input sample count expected for one process call.
 *
 * This returns the frame step in samples, so callers can read exactly one
 * chunk per loop iteration.
 *
 * @param[in] handle  Processor instance.
 * @return Step size in samples, or zero if @p handle is NULL.
 */
uint32_t avp_mfcc_get_step_samples(const avp_mfcc_t *handle);

#ifdef __cplusplus
}
#endif

#endif
