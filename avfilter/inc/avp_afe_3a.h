/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AVP_AFE_3A_H
#define AVP_AFE_3A_H

#include "avp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct avp_afe_3a avp_afe_3a_t;

/** @brief Fixed frame duration in milliseconds for all AFE processing. */
#define AVP_AFE_3A_FRAME_MS 10u

/** @brief AFE runtime control commands. */
typedef enum {
    AVP_AFE_3A_CMD_ENABLE_AEC,               /**< Enable or disable AEC. */
    AVP_AFE_3A_CMD_ENABLE_NS,                /**< Enable or disable NS. */
    AVP_AFE_3A_CMD_ENABLE_AGC,               /**< Enable or disable AGC. */
    AVP_AFE_3A_CMD_ENABLE_VAD,               /**< Enable or disable VAD. */
    AVP_AFE_3A_CMD_SET_AEC_MODE,             /**< Set AEC NLP suppression aggressiveness. */
    AVP_AFE_3A_CMD_SET_NS_POLICY,            /**< Set NS noise suppression policy. */
    AVP_AFE_3A_CMD_SET_AGC_MODE,             /**< Set AGC operating mode. */
    AVP_AFE_3A_CMD_SET_AGC_CONFIG,           /**< Set AGC configuration parameters. */
    AVP_AFE_3A_CMD_SET_VAD_MODE,             /**< Set VAD sensitivity mode. */
    AVP_AFE_3A_CMD_SET_STREAM_DELAY_MS,      /**< Set far-end stream delay in milliseconds. */
    AVP_AFE_3A_CMD_GET_ECHO_STATUS,          /**< Get current AEC echo status. */
    AVP_AFE_3A_CMD_GET_VAD,                  /**< Get the latest VAD decision. */
    AVP_AFE_3A_CMD_ENABLE_HPF,               /**< Enable or disable the high-pass filter. */
    AVP_AFE_3A_CMD_SET_ANALOG_LEVEL,         /**< Set microphone analog gain level. */
    AVP_AFE_3A_CMD_GET_ANALOG_LEVEL,         /**< Get current microphone analog gain level. */
    AVP_AFE_3A_CMD_SET_STREAM_DRIFT_SAMPLES, /**< Set sample-clock drift compensation value. */
    AVP_AFE_3A_CMD_SET_AEC_ADVANCED_CONFIG,  /**< Set AEC advanced configuration. */
    AVP_AFE_3A_CMD_GET_AGC_SATURATION,       /**< Get AGC saturation flag. */
} avp_afe_3a_cmd_t;

/** @brief AEC NLP suppression aggressiveness mode. */
typedef enum {
    AVP_AFE_3A_AEC_NLP_CONSERVATIVE = 0, /**< Conservative mode; weakest echo suppression. */
    AVP_AFE_3A_AEC_NLP_MODERATE,         /**< Moderate mode; balanced echo suppression. */
    AVP_AFE_3A_AEC_NLP_AGGRESSIVE,       /**< Aggressive mode; strongest echo suppression. */
} avp_afe_3a_aec_mode_t;

/** @brief NS noise suppression policy. */
typedef enum {
    AVP_AFE_3A_NS_POLICY_MILD = 0,   /**< Mild suppression; minimal noise reduction. */
    AVP_AFE_3A_NS_POLICY_MEDIUM,     /**< Medium suppression; balanced noise reduction. */
    AVP_AFE_3A_NS_POLICY_AGGRESSIVE, /**< Aggressive suppression; maximum noise reduction. */
} avp_afe_3a_ns_policy_t;

/** @brief VAD sensitivity mode. */
typedef enum {
    AVP_AFE_3A_VAD_MODE_NORMAL = 0,      /**< Normal mode; lowest sensitivity. */
    AVP_AFE_3A_VAD_MODE_LOW_BITRATE,     /**< Low-bitrate mode; higher sensitivity. */
    AVP_AFE_3A_VAD_MODE_AGGRESSIVE,      /**< Aggressive mode; highly sensitive to non-speech. */
    AVP_AFE_3A_VAD_MODE_VERY_AGGRESSIVE, /**< Very aggressive mode; highest non-speech detection sensitivity. */
} avp_afe_3a_vad_mode_t;

/** @brief Preset far-end stream delay values. */
typedef enum {
    AVP_AFE_3A_STREAM_DELAY_NONE = 0,    /**< No delay. */
    AVP_AFE_3A_STREAM_DELAY_10MS = 10,   /**< 10 ms delay. */
    AVP_AFE_3A_STREAM_DELAY_20MS = 20,   /**< 20 ms delay. */
    AVP_AFE_3A_STREAM_DELAY_50MS = 50,   /**< 50 ms delay. */
    AVP_AFE_3A_STREAM_DELAY_100MS = 100, /**< 100 ms delay. */
    AVP_AFE_3A_STREAM_DELAY_200MS = 200, /**< 200 ms delay. */
} avp_afe_3a_stream_delay_t;

/** @brief AGC operating mode. */
typedef enum {
    AVP_AFE_3A_AGC_MODE_FIXED_DIGITAL = 0, /**< Fixed digital gain. */
    AVP_AFE_3A_AGC_MODE_ADAPTIVE_DIGITAL,  /**< Adaptive digital gain control. */
    AVP_AFE_3A_AGC_MODE_ADAPTIVE_ANALOG,   /**< Adaptive analog gain control. */
} avp_afe_3a_agc_mode_t;

/** @brief AGC configuration parameters. */
typedef struct {
    int16_t target_level_dbfs;   /**< Target output level in dBFS (negative value). */
    int16_t compression_gain_db; /**< Maximum compression gain in dB. */
    uint8_t limiter_enable;      /**< Non-zero to enable the output limiter. */
} avp_afe_3a_agc_config_t;

/** @brief AEC advanced configuration flags. */
typedef struct {
    uint8_t enable_metrics;            /**< Enable AEC performance metrics collection. */
    uint8_t enable_delay_logging;      /**< Enable AEC delay metrics logging. */
    uint8_t enable_drift_compensation; /**< Enable sample-clock drift compensation. */
    uint8_t enable_extended_filter;    /**< Enable extended filter length. */
    uint8_t enable_delay_agnostic;     /**< Enable delay-agnostic AEC mode. */
    uint8_t enable_next_generation;    /**< Enable next-generation AEC algorithm. */
} avp_afe_3a_aec_advanced_config_t;

/** @brief AFE instance configuration. */
typedef struct {
    uint32_t sample_rate;                                 /**< Input sample rate in Hz; supports 8000, 16000, 32000. */
    uint8_t enable_aec;                                   /**< Non-zero to enable AEC. */
    uint8_t enable_ns;                                    /**< Non-zero to enable NS. */
    uint8_t enable_agc;                                   /**< Non-zero to enable AGC. */
    uint8_t enable_vad;                                   /**< Non-zero to enable VAD. */
    uint8_t enable_hpf;                                   /**< Non-zero to enable the high-pass filter. */
    avp_afe_3a_aec_mode_t aec_nlp_mode;                   /**< AEC NLP suppression aggressiveness mode. */
    avp_afe_3a_ns_policy_t ns_policy;                     /**< NS noise suppression policy. */
    avp_afe_3a_agc_mode_t agc_mode;                       /**< AGC operating mode. */
    avp_afe_3a_vad_mode_t vad_mode;                       /**< VAD sensitivity mode. */
    avp_afe_3a_stream_delay_t stream_delay_ms;            /**< Far-end stream delay. */
    avp_afe_3a_agc_config_t agc_config;                   /**< AGC configuration parameters. */
    avp_afe_3a_aec_advanced_config_t aec_advanced_config; /**< AEC advanced configuration. */
} avp_afe_3a_config_t;

/** @brief AEC level statistics (instant / average / max / min). */
typedef struct {
    int instant; /**< Instantaneous level. */
    int average; /**< Average level. */
    int max;     /**< Maximum level. */
    int min;     /**< Minimum level. */
} avp_afe_3a_aec_level_t;

/** @brief AEC performance metrics. */
typedef struct {
    avp_afe_3a_aec_level_t rerl;  /**< Residual echo return loss (RERL) statistics. */
    avp_afe_3a_aec_level_t erl;   /**< Echo return loss (ERL) statistics. */
    avp_afe_3a_aec_level_t erle;  /**< Echo return loss enhancement (ERLE) statistics. */
    avp_afe_3a_aec_level_t a_nlp; /**< A-weighted NLP gain statistics. */
} avp_afe_3a_aec_metrics_t;

/** @brief AEC delay metrics. */
typedef struct {
    int median;                 /**< Median delay in samples. */
    int std;                    /**< Standard deviation of delay in samples. */
    float fraction_poor_delays; /**< Fraction of frames with poor delay estimates, in [0, 1]. */
} avp_afe_3a_aec_delay_metrics_t;

/**
 * @brief Create an AFE instance and allocate all WebRTC algorithm handles.
 *
 * The current implementation supports 8 kHz, 16 kHz and 32 kHz mono input.
 * The frame duration is fixed to @ref AVP_AFE_3A_FRAME_MS because the WebRTC
 * AEC/NS/AGC legacy APIs operate on 10 ms blocks.
 *
 * @param[in]  config  Pointer to the AFE configuration structure.
 * @param[out] handle  Receives the newly created AFE instance pointer.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_afe_3a_open(const avp_afe_3a_config_t *config,
                             avp_afe_3a_t **handle);

/**
 * @brief Release all resources owned by the AFE instance.
 *
 * @param[in] handle  AFE instance handle to destroy; NULL is accepted.
 */
void avp_afe_3a_close(avp_afe_3a_t *handle);

/**
 * @brief Change an AFE parameter at runtime.
 *
 * For enable/set commands, @p arg points to an int (or the matching config
 * struct). For get commands, @p arg points to an int that receives the value.
 *
 * @param[in]     handle  AFE instance handle.
 * @param[in]     cmd     Control command; see @ref avp_afe_3a_cmd_t.
 * @param[in,out] arg     Command argument; type depends on @p cmd.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_afe_3a_control(avp_afe_3a_t *handle,
                                avp_afe_3a_cmd_t cmd,
                                void *arg);

/**
 * @brief Process one mono audio frame through the full AFE pipeline.
 *
 * The frame length is <tt>sample_rate * AVP_AFE_3A_FRAME_MS / 1000</tt> samples.
 *
 * @param[in]  handle    AFE instance handle.
 * @param[in]  near_in   Microphone signal; frame_samples samples.
 * @param[in]  far_in    Reference/speaker signal; frame_samples samples.
 *                       May be NULL when AEC is disabled or no reference is available.
 * @param[out] near_out  Processed microphone signal; frame_samples samples.
 *                       May alias @p near_in (in-place processing).
 * @param[out] vad       VAD decision output: 0 (non-speech), 1 (speech), or
 *                       -1 (VAD disabled). Pass NULL if not needed.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_afe_3a_process(avp_afe_3a_t *handle,
                                const int16_t *near_in,
                                const int16_t *far_in,
                                int16_t *near_out,
                                int *vad);

/**
 * @brief Return the latest NS prior speech probability.
 *
 * @param[in] handle  AFE instance handle.
 * @return Speech probability in [0, 1]; 0 if NS is disabled.
 */
float avp_afe_3a_get_ns_speech_probability(const avp_afe_3a_t *handle);

/**
 * @brief Return the latest normalised NS noise spectrum estimate.
 *
 * The returned pointer is owned by the AFE instance and remains valid until
 * the next call to @ref avp_afe_3a_process or @ref avp_afe_3a_close.
 *
 * @param[in]  handle    AFE instance handle.
 * @param[out] num_freq  Receives the number of frequency bins.
 * @return Pointer to the noise spectrum array; NULL if NS is disabled.
 */
const float *avp_afe_3a_get_ns_noise_estimate(const avp_afe_3a_t *handle,
                                              size_t *num_freq);

/**
 * @brief Return AEC performance metrics.
 *
 * AEC metrics collection must be enabled in @ref avp_afe_3a_config_t or via
 * @ref AVP_AFE_3A_CMD_SET_AEC_ADVANCED_CONFIG before calling this function.
 *
 * @param[in]  handle   AFE instance handle.
 * @param[out] metrics  Receives the AEC performance metrics.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_afe_3a_get_aec_metrics(avp_afe_3a_t *handle,
                                        avp_afe_3a_aec_metrics_t *metrics);

/**
 * @brief Return AEC delay metrics.
 *
 * AEC delay logging must be enabled in @ref avp_afe_3a_config_t or via
 * @ref AVP_AFE_3A_CMD_SET_AEC_ADVANCED_CONFIG before calling this function.
 *
 * @param[in]  handle   AFE instance handle.
 * @param[out] metrics  Receives the AEC delay metrics.
 * @return @ref AVP_OK on success, or a negative error code on failure.
 */
avp_status_t avp_afe_3a_get_aec_delay_metrics(
    avp_afe_3a_t *handle,
    avp_afe_3a_aec_delay_metrics_t *metrics);

/**
 * @brief Return the number of samples consumed and produced per call to
 *        @ref avp_afe_3a_process.
 *
 * @param[in] handle  AFE instance handle.
 * @return Number of PCM samples per frame.
 */
uint32_t avp_afe_3a_get_frame_samples(const avp_afe_3a_t *handle);

#ifdef __cplusplus
}
#endif

#endif
