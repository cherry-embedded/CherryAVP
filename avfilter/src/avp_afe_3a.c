/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "avp_afe_3a.h"

#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/common_audio/vad/include/webrtc_vad.h"
#include "webrtc/modules/audio_processing/aec/aec_core.h"
#include "webrtc/modules/audio_processing/aec/echo_cancellation.h"
#include "webrtc/modules/audio_processing/agc/legacy/gain_control.h"
#if defined(CONFIG_CHERRYAVP_AFE_3A_NS_FIXED)
#include "webrtc/modules/audio_processing/ns/noise_suppression_x.h"
#else
#include "webrtc/modules/audio_processing/ns/noise_suppression.h"
#endif

#define AVP_AFE_3A_MAX_BANDS          2u
#define AVP_AFE_3A_AEC_SOUNDCARD_RATE 48000

struct avp_afe_3a {
    avp_afe_3a_config_t config;

    uint32_t sample_rate;
    uint32_t frame_samples;
    uint32_t num_bands;
    uint32_t band_samples;

    void *aec;
#if defined(CONFIG_CHERRYAVP_AFE_3A_NS_FIXED)
    NsxHandle *ns;
#else
    NsHandle *ns;
#endif
    void *agc;
    VadInst *vad;

    int16_t *near_bands_i16[AVP_AFE_3A_MAX_BANDS];
    int16_t *far_bands_i16[AVP_AFE_3A_MAX_BANDS];
    int16_t *pcm_bands_i16[AVP_AFE_3A_MAX_BANDS];
    int16_t *ns_bands_i16[AVP_AFE_3A_MAX_BANDS];
    int16_t *agc_bands_i16[AVP_AFE_3A_MAX_BANDS];

    float *near_bands_f[AVP_AFE_3A_MAX_BANDS];
    float *far_band0_f;
    float *aec_bands_f[AVP_AFE_3A_MAX_BANDS];
#if !defined(CONFIG_CHERRYAVP_AFE_3A_NS_FIXED)
    float *ns_bands_f[AVP_AFE_3A_MAX_BANDS];
#endif

    int32_t qmf_near_analysis_state1[6];
    int32_t qmf_near_analysis_state2[6];
    int32_t qmf_near_synthesis_state1[6];
    int32_t qmf_near_synthesis_state2[6];
    int32_t qmf_far_analysis_state1[6];
    int32_t qmf_far_analysis_state2[6];

    int16_t hpf_x[2];
    int16_t hpf_y[4];

    int32_t agc_mic_level;
    int32_t analog_capture_level;
    int32_t stream_drift_samples;
    int last_vad;
    int last_echo_status;
    int last_agc_saturation;

    float ns_speech_probability;
    float *ns_noise_estimate;
    size_t ns_num_freq;
};

static int avp_afe_3a_valid_sample_rate(uint32_t sample_rate)
{
    return sample_rate == 8000u || sample_rate == 16000u || sample_rate == 32000u;
}

static int avp_afe_3a_valid_agc_mode(avp_afe_3a_agc_mode_t mode)
{
    return mode == AVP_AFE_3A_AGC_MODE_FIXED_DIGITAL ||
           mode == AVP_AFE_3A_AGC_MODE_ADAPTIVE_DIGITAL ||
           mode == AVP_AFE_3A_AGC_MODE_ADAPTIVE_ANALOG;
}

static int16_t avp_afe_3a_map_agc_mode(avp_afe_3a_agc_mode_t mode)
{
    switch (mode) {
        case AVP_AFE_3A_AGC_MODE_ADAPTIVE_ANALOG:
            return kAgcModeAdaptiveAnalog;
        case AVP_AFE_3A_AGC_MODE_ADAPTIVE_DIGITAL:
            return kAgcModeAdaptiveDigital;
        case AVP_AFE_3A_AGC_MODE_FIXED_DIGITAL:
        default:
            return kAgcModeFixedDigital;
    }
}

static int avp_afe_3a_valid_bool(uint8_t value)
{
    return value == 0u || value == 1u;
}

static void avp_afe_3a_free_band_i16(int16_t **bands, uint32_t count)
{
    uint32_t i;

    for (i = 0u; i < count; i++) {
        if (bands[i] != NULL) {
            avp_free(bands[i]);
        }
        bands[i] = NULL;
    }
}

static void avp_afe_3a_free_band_f(float **bands, uint32_t count)
{
    uint32_t i;

    for (i = 0u; i < count; i++) {
        if (bands[i] != NULL) {
            avp_free(bands[i]);
        }
        bands[i] = NULL;
    }
}

static void avp_afe_3a_free(avp_afe_3a_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    if (ctx->aec != NULL) {
        WebRtcAec_Free(ctx->aec);
    }
    if (ctx->ns != NULL) {
#if defined(CONFIG_CHERRYAVP_AFE_3A_NS_FIXED)
        WebRtcNsx_Free(ctx->ns);
#else
        WebRtcNs_Free(ctx->ns);
#endif
    }
    if (ctx->agc != NULL) {
        WebRtcAgc_Free(ctx->agc);
    }
    if (ctx->vad != NULL) {
        WebRtcVad_Free(ctx->vad);
    }

    avp_afe_3a_free_band_i16(ctx->near_bands_i16, AVP_AFE_3A_MAX_BANDS);
    avp_afe_3a_free_band_i16(ctx->far_bands_i16, AVP_AFE_3A_MAX_BANDS);
    avp_afe_3a_free_band_i16(ctx->pcm_bands_i16, AVP_AFE_3A_MAX_BANDS);
    avp_afe_3a_free_band_i16(ctx->ns_bands_i16, AVP_AFE_3A_MAX_BANDS);
    avp_afe_3a_free_band_i16(ctx->agc_bands_i16, AVP_AFE_3A_MAX_BANDS);

    avp_afe_3a_free_band_f(ctx->near_bands_f, AVP_AFE_3A_MAX_BANDS);
    if (ctx->far_band0_f != NULL) {
        avp_free(ctx->far_band0_f);
    }
    avp_afe_3a_free_band_f(ctx->aec_bands_f, AVP_AFE_3A_MAX_BANDS);
#if !defined(CONFIG_CHERRYAVP_AFE_3A_NS_FIXED)
    avp_afe_3a_free_band_f(ctx->ns_bands_f, AVP_AFE_3A_MAX_BANDS);
#endif

    if (ctx->ns_noise_estimate != NULL) {
        avp_free(ctx->ns_noise_estimate);
    }
    avp_free(ctx);
}

static avp_status_t avp_afe_3a_alloc_bands(avp_afe_3a_t *ctx)
{
    uint32_t i;

    ctx->far_band0_f = (float *)avp_malloc((size_t)ctx->band_samples * sizeof(float));
    ctx->ns_noise_estimate = (float *)avp_malloc(ctx->ns_num_freq * sizeof(float));

    if (ctx->far_band0_f == NULL || ctx->ns_noise_estimate == NULL) {
        return AVP_ENOMEM;
    }

    for (i = 0u; i < ctx->num_bands; i++) {
        int alloc_failed;

        ctx->near_bands_i16[i] = (int16_t *)avp_malloc((size_t)ctx->band_samples * sizeof(int16_t));
        ctx->far_bands_i16[i] = (int16_t *)avp_malloc((size_t)ctx->band_samples * sizeof(int16_t));
        ctx->pcm_bands_i16[i] = (int16_t *)avp_malloc((size_t)ctx->band_samples * sizeof(int16_t));
        ctx->ns_bands_i16[i] = (int16_t *)avp_malloc((size_t)ctx->band_samples * sizeof(int16_t));
        ctx->agc_bands_i16[i] = (int16_t *)avp_malloc((size_t)ctx->band_samples * sizeof(int16_t));
        ctx->near_bands_f[i] = (float *)avp_malloc((size_t)ctx->band_samples * sizeof(float));
        ctx->aec_bands_f[i] = (float *)avp_malloc((size_t)ctx->band_samples * sizeof(float));
#if !defined(CONFIG_CHERRYAVP_AFE_3A_NS_FIXED)
        ctx->ns_bands_f[i] = (float *)avp_malloc((size_t)ctx->band_samples * sizeof(float));
#endif

        alloc_failed = ctx->near_bands_i16[i] == NULL ||
                       ctx->far_bands_i16[i] == NULL ||
                       ctx->pcm_bands_i16[i] == NULL ||
                       ctx->ns_bands_i16[i] == NULL ||
                       ctx->agc_bands_i16[i] == NULL ||
                       ctx->near_bands_f[i] == NULL ||
                       ctx->aec_bands_f[i] == NULL;
#if !defined(CONFIG_CHERRYAVP_AFE_3A_NS_FIXED)
        alloc_failed = alloc_failed || ctx->ns_bands_f[i] == NULL;
#endif

        if (alloc_failed) {
            return AVP_ENOMEM;
        }
    }

    return AVP_OK;
}

static avp_status_t avp_afe_3a_agc_reinit(avp_afe_3a_t *ctx)
{
    int16_t agc_mode;
    WebRtcAgcConfig config;

    if (ctx->agc != NULL) {
        WebRtcAgc_Free(ctx->agc);
        ctx->agc = NULL;
    }

    ctx->agc = WebRtcAgc_Create();
    if (ctx->agc == NULL) {
        return AVP_ENOMEM;
    }

    agc_mode = avp_afe_3a_map_agc_mode(ctx->config.agc_mode);
    if (WebRtcAgc_Init(ctx->agc, 0, 255, agc_mode, ctx->sample_rate) != 0) {
        return AVP_EINVAL;
    }

    config.targetLevelDbfs = ctx->config.agc_config.target_level_dbfs;
    config.compressionGaindB = ctx->config.agc_config.compression_gain_db;
    config.limiterEnable = ctx->config.agc_config.limiter_enable;
    if (WebRtcAgc_set_config(ctx->agc, config) != 0) {
        return AVP_EINVAL;
    }

    ctx->agc_mic_level = 0;
    ctx->analog_capture_level = 0;
    ctx->last_agc_saturation = 0;
    return AVP_OK;
}

static void avp_afe_3a_reset_hpf(avp_afe_3a_t *ctx)
{
    memset(ctx->hpf_x, 0, sizeof(ctx->hpf_x));
    memset(ctx->hpf_y, 0, sizeof(ctx->hpf_y));
}

static avp_status_t avp_afe_3a_set_aec_config(avp_afe_3a_t *ctx)
{
    AecConfig aec_config;
    AecCore *aec_core;

    aec_core = WebRtcAec_aec_core(ctx->aec);
    if (aec_core == NULL) {
        return AVP_EINVAL;
    }

    WebRtcAec_enable_extended_filter(
        aec_core, ctx->config.aec_advanced_config.enable_extended_filter != 0u ? 1 : 0);
    WebRtcAec_enable_delay_agnostic(
        aec_core, ctx->config.aec_advanced_config.enable_delay_agnostic != 0u ? 1 : 0);
    WebRtcAec_enable_next_generation_aec(
        aec_core, ctx->config.aec_advanced_config.enable_next_generation != 0u ? 1 : 0);

    aec_config.nlpMode = ctx->config.aec_nlp_mode;
    aec_config.skewMode =
        ctx->config.aec_advanced_config.enable_drift_compensation != 0u ? kAecTrue : kAecFalse;
    aec_config.metricsMode =
        ctx->config.aec_advanced_config.enable_metrics != 0u ? kAecTrue : kAecFalse;
    aec_config.delay_logging =
        ctx->config.aec_advanced_config.enable_delay_logging != 0u ? kAecTrue : kAecFalse;

    return WebRtcAec_set_config(ctx->aec, aec_config) == 0 ? AVP_OK : AVP_EINVAL;
}

avp_status_t avp_afe_3a_open(const avp_afe_3a_config_t *config,
                             avp_afe_3a_t **handle)
{
    avp_afe_3a_t *ctx;
    avp_status_t status;

    if (config == NULL || handle == NULL) {
        return AVP_EINVAL;
    }
    *handle = NULL;

    if (!avp_afe_3a_valid_sample_rate(config->sample_rate) ||
        config->aec_nlp_mode < AVP_AFE_3A_AEC_NLP_CONSERVATIVE ||
        config->aec_nlp_mode > AVP_AFE_3A_AEC_NLP_AGGRESSIVE ||
        config->ns_policy < AVP_AFE_3A_NS_POLICY_MILD ||
        config->ns_policy > AVP_AFE_3A_NS_POLICY_AGGRESSIVE ||
        config->vad_mode < AVP_AFE_3A_VAD_MODE_NORMAL ||
        config->vad_mode > AVP_AFE_3A_VAD_MODE_VERY_AGGRESSIVE ||
        !avp_afe_3a_valid_agc_mode(config->agc_mode) ||
        !avp_afe_3a_valid_bool(config->enable_hpf) ||
        !avp_afe_3a_valid_bool(config->enable_aec) ||
        !avp_afe_3a_valid_bool(config->enable_ns) ||
        !avp_afe_3a_valid_bool(config->enable_agc) ||
        !avp_afe_3a_valid_bool(config->enable_vad) ||
        !avp_afe_3a_valid_bool(config->aec_advanced_config.enable_metrics) ||
        !avp_afe_3a_valid_bool(config->aec_advanced_config.enable_delay_logging) ||
        !avp_afe_3a_valid_bool(config->aec_advanced_config.enable_drift_compensation) ||
        !avp_afe_3a_valid_bool(config->aec_advanced_config.enable_extended_filter) ||
        !avp_afe_3a_valid_bool(config->aec_advanced_config.enable_delay_agnostic) ||
        !avp_afe_3a_valid_bool(config->aec_advanced_config.enable_next_generation) ||
        config->stream_delay_ms < 0 || config->stream_delay_ms > 500) {
        return AVP_EINVAL;
    }

    ctx = (avp_afe_3a_t *)avp_calloc(1u, sizeof(*ctx));
    if (ctx == NULL) {
        return AVP_ENOMEM;
    }

    ctx->config = *config;
    ctx->sample_rate = config->sample_rate;
    ctx->frame_samples = (uint32_t)config->sample_rate * AVP_AFE_3A_FRAME_MS / 1000u;
    ctx->num_bands = ctx->sample_rate == 32000u ? 2u : 1u;
    ctx->band_samples = ctx->frame_samples / ctx->num_bands;
#if defined(CONFIG_CHERRYAVP_AFE_3A_NS_FIXED)
    ctx->ns_num_freq = WebRtcNsx_num_freq();
#else
    ctx->ns_num_freq = WebRtcNs_num_freq();
#endif
    ctx->last_vad = -1;
    ctx->last_echo_status = 0;
    ctx->last_agc_saturation = 0;
    ctx->ns_speech_probability = 0.0f;

    WebRtcSpl_Init();

    status = avp_afe_3a_alloc_bands(ctx);
    if (status != AVP_OK) {
        avp_afe_3a_free(ctx);
        return status;
    }

    ctx->aec = WebRtcAec_Create();
    if (ctx->aec == NULL ||
        WebRtcAec_Init(ctx->aec, (int32_t)ctx->sample_rate,
                       AVP_AFE_3A_AEC_SOUNDCARD_RATE) != 0) {
        avp_afe_3a_free(ctx);
        return AVP_ENOMEM;
    }

    if (avp_afe_3a_set_aec_config(ctx) != AVP_OK) {
        avp_afe_3a_free(ctx);
        return AVP_EINVAL;
    }

#if defined(CONFIG_CHERRYAVP_AFE_3A_NS_FIXED)
    ctx->ns = WebRtcNsx_Create();
    if (ctx->ns == NULL || WebRtcNsx_Init(ctx->ns, ctx->sample_rate) != 0 ||
        WebRtcNsx_set_policy(ctx->ns, config->ns_policy) != 0) {
        avp_afe_3a_free(ctx);
        return AVP_ENOMEM;
    }
#else
    ctx->ns = WebRtcNs_Create();
    if (ctx->ns == NULL || WebRtcNs_Init(ctx->ns, ctx->sample_rate) != 0 ||
        WebRtcNs_set_policy(ctx->ns, config->ns_policy) != 0) {
        avp_afe_3a_free(ctx);
        return AVP_ENOMEM;
    }
#endif

    ctx->agc = WebRtcAgc_Create();
    if (ctx->agc == NULL) {
        avp_afe_3a_free(ctx);
        return AVP_ENOMEM;
    }

    status = avp_afe_3a_agc_reinit(ctx);
    if (status != AVP_OK) {
        avp_afe_3a_free(ctx);
        return status;
    }

    ctx->vad = WebRtcVad_Create();
    if (ctx->vad == NULL || WebRtcVad_Init(ctx->vad) != 0 ||
        WebRtcVad_set_mode(ctx->vad, config->vad_mode) != 0) {
        avp_afe_3a_free(ctx);
        return AVP_ENOMEM;
    }

    *handle = ctx;
    return AVP_OK;
}

void avp_afe_3a_close(avp_afe_3a_t *handle)
{
    avp_afe_3a_free(handle);
}

avp_status_t avp_afe_3a_control(avp_afe_3a_t *handle,
                                avp_afe_3a_cmd_t cmd,
                                void *arg)
{
    avp_afe_3a_agc_config_t *agc;
    avp_afe_3a_aec_advanced_config_t *aec_advanced;

    if (handle == NULL || arg == NULL) {
        return AVP_EINVAL;
    }

    switch (cmd) {
        case AVP_AFE_3A_CMD_ENABLE_HPF:
            if (*(int *)arg != 0 && *(int *)arg != 1) {
                return AVP_EINVAL;
            }
            if (handle->config.enable_hpf == 0u && *(int *)arg != 0) {
                avp_afe_3a_reset_hpf(handle);
            }
            handle->config.enable_hpf = (uint8_t) * (int *)arg;
            return AVP_OK;

        case AVP_AFE_3A_CMD_ENABLE_AEC:
            if (*(int *)arg != 0 && *(int *)arg != 1) {
                return AVP_EINVAL;
            }
            handle->config.enable_aec = (uint8_t) * (int *)arg;
            return AVP_OK;

        case AVP_AFE_3A_CMD_ENABLE_NS:
            if (*(int *)arg != 0 && *(int *)arg != 1) {
                return AVP_EINVAL;
            }
            handle->config.enable_ns = (uint8_t) * (int *)arg;
            return AVP_OK;

        case AVP_AFE_3A_CMD_ENABLE_AGC:
            if (*(int *)arg != 0 && *(int *)arg != 1) {
                return AVP_EINVAL;
            }
            handle->config.enable_agc = (uint8_t) * (int *)arg;
            return AVP_OK;

        case AVP_AFE_3A_CMD_ENABLE_VAD:
            if (*(int *)arg != 0 && *(int *)arg != 1) {
                return AVP_EINVAL;
            }
            handle->config.enable_vad = (uint8_t) * (int *)arg;
            return AVP_OK;

        case AVP_AFE_3A_CMD_SET_AEC_MODE:
            if (*(int *)arg < AVP_AFE_3A_AEC_NLP_CONSERVATIVE ||
                *(int *)arg > AVP_AFE_3A_AEC_NLP_AGGRESSIVE) {
                return AVP_EINVAL;
            }
            handle->config.aec_nlp_mode = (avp_afe_3a_aec_mode_t) * (int *)arg;
            return avp_afe_3a_set_aec_config(handle);

        case AVP_AFE_3A_CMD_SET_NS_POLICY:
            if (*(int *)arg < AVP_AFE_3A_NS_POLICY_MILD ||
                *(int *)arg > AVP_AFE_3A_NS_POLICY_AGGRESSIVE) {
                return AVP_EINVAL;
            }
            handle->config.ns_policy = (avp_afe_3a_ns_policy_t) * (int *)arg;
#if defined(CONFIG_CHERRYAVP_AFE_3A_NS_FIXED)
            return WebRtcNsx_set_policy(handle->ns, *(int *)arg) == 0 ? AVP_OK : AVP_EINVAL;
#else
            return WebRtcNs_set_policy(handle->ns, *(int *)arg) == 0 ? AVP_OK : AVP_EINVAL;
#endif

        case AVP_AFE_3A_CMD_SET_AGC_MODE:
            if (!avp_afe_3a_valid_agc_mode((avp_afe_3a_agc_mode_t) * (int *)arg)) {
                return AVP_EINVAL;
            }
            handle->config.agc_mode = (avp_afe_3a_agc_mode_t) * (int *)arg;
            return avp_afe_3a_agc_reinit(handle);

        case AVP_AFE_3A_CMD_SET_AGC_CONFIG:
            agc = (avp_afe_3a_agc_config_t *)arg;
            handle->config.agc_config.target_level_dbfs = agc->target_level_dbfs;
            handle->config.agc_config.compression_gain_db = agc->compression_gain_db;
            handle->config.agc_config.limiter_enable = agc->limiter_enable;
            return avp_afe_3a_agc_reinit(handle);

        case AVP_AFE_3A_CMD_SET_VAD_MODE:
            if (*(int *)arg < AVP_AFE_3A_VAD_MODE_NORMAL ||
                *(int *)arg > AVP_AFE_3A_VAD_MODE_VERY_AGGRESSIVE) {
                return AVP_EINVAL;
            }
            handle->config.vad_mode = (avp_afe_3a_vad_mode_t) * (int *)arg;
            return WebRtcVad_set_mode(handle->vad, *(int *)arg) == 0 ? AVP_OK : AVP_EINVAL;

        case AVP_AFE_3A_CMD_SET_STREAM_DELAY_MS:
            if (*(int *)arg < 0 || *(int *)arg > 500) {
                return AVP_EINVAL;
            }
            handle->config.stream_delay_ms = (avp_afe_3a_stream_delay_t) * (int *)arg;
            return AVP_OK;

        case AVP_AFE_3A_CMD_SET_ANALOG_LEVEL:
            if (*(int *)arg < 0 || *(int *)arg > 255) {
                return AVP_EINVAL;
            }
            handle->analog_capture_level = *(int *)arg;
            handle->agc_mic_level = *(int *)arg;
            return AVP_OK;

        case AVP_AFE_3A_CMD_GET_ANALOG_LEVEL:
            *(int *)arg = handle->analog_capture_level;
            return AVP_OK;

        case AVP_AFE_3A_CMD_SET_STREAM_DRIFT_SAMPLES:
            handle->stream_drift_samples = *(int *)arg;
            return AVP_OK;

        case AVP_AFE_3A_CMD_SET_AEC_ADVANCED_CONFIG:
            aec_advanced = (avp_afe_3a_aec_advanced_config_t *)arg;
            if (!avp_afe_3a_valid_bool(aec_advanced->enable_metrics) ||
                !avp_afe_3a_valid_bool(aec_advanced->enable_delay_logging) ||
                !avp_afe_3a_valid_bool(aec_advanced->enable_drift_compensation) ||
                !avp_afe_3a_valid_bool(aec_advanced->enable_extended_filter) ||
                !avp_afe_3a_valid_bool(aec_advanced->enable_delay_agnostic) ||
                !avp_afe_3a_valid_bool(aec_advanced->enable_next_generation)) {
                return AVP_EINVAL;
            }
            handle->config.aec_advanced_config.enable_metrics = aec_advanced->enable_metrics;
            handle->config.aec_advanced_config.enable_delay_logging = aec_advanced->enable_delay_logging;
            handle->config.aec_advanced_config.enable_drift_compensation =
                aec_advanced->enable_drift_compensation;
            handle->config.aec_advanced_config.enable_extended_filter =
                aec_advanced->enable_extended_filter;
            handle->config.aec_advanced_config.enable_delay_agnostic =
                aec_advanced->enable_delay_agnostic;
            handle->config.aec_advanced_config.enable_next_generation =
                aec_advanced->enable_next_generation;
            return avp_afe_3a_set_aec_config(handle);

        case AVP_AFE_3A_CMD_GET_ECHO_STATUS:
            *(int *)arg = handle->last_echo_status;
            return AVP_OK;

        case AVP_AFE_3A_CMD_GET_VAD:
            *(int *)arg = handle->last_vad;
            return AVP_OK;

        case AVP_AFE_3A_CMD_GET_AGC_SATURATION:
            *(int *)arg = handle->last_agc_saturation;
            return AVP_OK;

        default:
            return AVP_EINVAL;
    }
}

static void avp_afe_3a_s16_to_float(const int16_t *in, float *out, uint32_t samples)
{
    uint32_t i;

    for (i = 0u; i < samples; i++) {
        out[i] = (float)in[i];
    }
}

static void avp_afe_3a_float_to_s16(const float *in, int16_t *out, uint32_t samples)
{
    uint32_t i;
    float value;

    for (i = 0u; i < samples; i++) {
        value = in[i];
        if (value >= 32767.0f) {
            out[i] = INT16_MAX;
        } else if (value <= -32768.0f) {
            out[i] = INT16_MIN;
        } else {
            out[i] = (int16_t)(value > 0.0f ? value + 0.5f : value - 0.5f);
        }
    }
}

static void avp_afe_3a_split_full_to_i16_bands(avp_afe_3a_t *ctx,
                                               const int16_t *full,
                                               int16_t **bands,
                                               int32_t *state1,
                                               int32_t *state2)
{
    if (ctx->num_bands == 1u) {
        memcpy(bands[0], full, (size_t)ctx->frame_samples * sizeof(int16_t));
        return;
    }

    WebRtcSpl_AnalysisQMF(full, (size_t)ctx->frame_samples,
                          bands[0], bands[1], state1, state2);
}

static void avp_afe_3a_merge_i16_bands_to_full(avp_afe_3a_t *ctx,
                                               int16_t *const *bands,
                                               int16_t *full)
{
    if (ctx->num_bands == 1u) {
        memcpy(full, bands[0], (size_t)ctx->frame_samples * sizeof(int16_t));
        return;
    }

    WebRtcSpl_SynthesisQMF(bands[0], bands[1], (size_t)ctx->band_samples,
                           full, ctx->qmf_near_synthesis_state1,
                           ctx->qmf_near_synthesis_state2);
}

static void avp_afe_3a_i16_bands_to_float(avp_afe_3a_t *ctx,
                                          int16_t *const *i16_bands,
                                          float *const *float_bands)
{
    uint32_t band;

    for (band = 0u; band < ctx->num_bands; band++) {
        avp_afe_3a_s16_to_float(i16_bands[band], float_bands[band], ctx->band_samples);
    }
}

static void avp_afe_3a_float_bands_to_i16(avp_afe_3a_t *ctx,
                                          float *const *float_bands,
                                          int16_t *const *i16_bands)
{
    uint32_t band;

    for (band = 0u; band < ctx->num_bands; band++) {
        avp_afe_3a_float_to_s16(float_bands[band], i16_bands[band], ctx->band_samples);
    }
}

static const int16_t avp_afe_3a_hpf_coeffs_8k[5] = { 3798, -7596, 3798, 7807, -3733 };
static const int16_t avp_afe_3a_hpf_coeffs[5] = { 4012, -8024, 4012, 8002, -3913 };

static void avp_afe_3a_high_pass_filter(avp_afe_3a_t *ctx, int16_t *data)
{
    const int16_t *ba;
    int16_t *x;
    int16_t *y;
    uint32_t i;

    ba = ctx->sample_rate == 8000u ? avp_afe_3a_hpf_coeffs_8k : avp_afe_3a_hpf_coeffs;
    x = ctx->hpf_x;
    y = ctx->hpf_y;

    for (i = 0u; i < ctx->band_samples; i++) {
        int32_t tmp;

        tmp = y[1] * ba[3];
        tmp += y[3] * ba[4];
        tmp >>= 15;
        tmp += y[0] * ba[3];
        tmp += y[2] * ba[4];
        tmp <<= 1;
        tmp += data[i] * ba[0];
        tmp += x[0] * ba[1];
        tmp += x[1] * ba[2];

        x[1] = x[0];
        x[0] = data[i];

        y[2] = y[0];
        y[3] = y[1];
        y[0] = (int16_t)(tmp >> 13);
        y[1] = (int16_t)((tmp - ((int32_t)y[0] << 13)) << 2);

        tmp += 2048;
        tmp = WEBRTC_SPL_SAT(134217727, tmp, -134217728);
        data[i] = (int16_t)(tmp >> 12);
    }
}

static void avp_afe_3a_update_ns_metrics(avp_afe_3a_t *ctx)
{
#if defined(CONFIG_CHERRYAVP_AFE_3A_NS_FIXED)
    const uint32_t *noise;
#else
    const float *noise;
#endif
    size_t i;

    if (ctx->config.enable_ns == 0u) {
        ctx->ns_speech_probability = 0.0f;
        memset(ctx->ns_noise_estimate, 0, ctx->ns_num_freq * sizeof(float));
        return;
    }

#if defined(CONFIG_CHERRYAVP_AFE_3A_NS_FIXED)
    ctx->ns_speech_probability = 0.0f;
    noise = WebRtcNsx_noise_estimate(ctx->ns);
    if (noise == NULL) {
        memset(ctx->ns_noise_estimate, 0, ctx->ns_num_freq * sizeof(float));
        return;
    }

    for (i = 0u; i < ctx->ns_num_freq; i++) {
        ctx->ns_noise_estimate[i] = (float)noise[i] / 8388608.0f;
    }
#else
    ctx->ns_speech_probability = WebRtcNs_prior_speech_probability(ctx->ns);
    noise = WebRtcNs_noise_estimate(ctx->ns);
    if (noise == NULL) {
        memset(ctx->ns_noise_estimate, 0, ctx->ns_num_freq * sizeof(float));
        return;
    }

    for (i = 0u; i < ctx->ns_num_freq; i++) {
        ctx->ns_noise_estimate[i] = noise[i] / 32768.0f;
    }
#endif
}

avp_status_t avp_afe_3a_process(avp_afe_3a_t *handle,
                                const int16_t *near_in,
                                const int16_t *far_in,
                                int16_t *near_out,
                                int *vad)
{
    float *process_bands_f[AVP_AFE_3A_MAX_BANDS];
    int16_t *process_bands_i16[AVP_AFE_3A_MAX_BANDS];
    uint32_t band;
    int vad_result = -1;
    int has_far = far_in != NULL;
    int32_t agc_level_in;
    int32_t agc_level_out;

    if (handle == NULL || near_in == NULL || near_out == NULL) {
        return AVP_EINVAL;
    }

    agc_level_in = handle->agc_mic_level;
    agc_level_out = agc_level_in;
    handle->last_echo_status = 0;
    handle->last_agc_saturation = 0;

    if (has_far &&
        (handle->config.enable_aec != 0u || handle->config.enable_agc != 0u)) {
        avp_afe_3a_split_full_to_i16_bands(handle, far_in,
                                           handle->far_bands_i16,
                                           handle->qmf_far_analysis_state1,
                                           handle->qmf_far_analysis_state2);
    }

    avp_afe_3a_split_full_to_i16_bands(handle, near_in,
                                       handle->near_bands_i16,
                                       handle->qmf_near_analysis_state1,
                                       handle->qmf_near_analysis_state2);

    if (handle->config.enable_hpf != 0u) {
        avp_afe_3a_high_pass_filter(handle, handle->near_bands_i16[0]);
    }

    if (handle->config.enable_agc != 0u) {
        if (has_far) {
            (void)WebRtcAgc_AddFarend(handle->agc,
                                      handle->far_bands_i16[0],
                                      (size_t)handle->band_samples);
        }

        if (handle->config.agc_mode == AVP_AFE_3A_AGC_MODE_ADAPTIVE_DIGITAL) {
            if (WebRtcAgc_VirtualMic(handle->agc,
                                     handle->near_bands_i16,
                                     handle->num_bands,
                                     handle->band_samples,
                                     handle->analog_capture_level,
                                     &agc_level_out) != 0) {
                return AVP_EINVAL;
            }
            agc_level_in = agc_level_out;
        } else if (handle->config.agc_mode == AVP_AFE_3A_AGC_MODE_ADAPTIVE_ANALOG) {
            agc_level_in = handle->analog_capture_level;
            agc_level_out = agc_level_in;
            if (WebRtcAgc_AddMic(handle->agc,
                                 handle->near_bands_i16,
                                 handle->num_bands,
                                 handle->band_samples) != 0) {
                return AVP_EINVAL;
            }
        }
        handle->agc_mic_level = agc_level_in;
    }

#if defined(CONFIG_CHERRYAVP_AFE_3A_NS_FIXED)
    if (handle->config.enable_aec != 0u && has_far) {
#else
    if ((handle->config.enable_aec != 0u && has_far) ||
        handle->config.enable_ns != 0u) {
#endif
        avp_afe_3a_i16_bands_to_float(handle, handle->near_bands_i16,
                                      handle->near_bands_f);
    }

#if !defined(CONFIG_CHERRYAVP_AFE_3A_NS_FIXED)
    if (handle->config.enable_ns != 0u) {
        WebRtcNs_Analyze(handle->ns, handle->near_bands_f[0]);
    }
#endif

    if (handle->config.enable_aec != 0u && has_far) {
        int aec_ret;

        avp_afe_3a_s16_to_float(handle->far_bands_i16[0],
                                handle->far_band0_f,
                                handle->band_samples);

        aec_ret = WebRtcAec_BufferFarend(handle->aec, handle->far_band0_f,
                                         (size_t)handle->band_samples);
        if (aec_ret != 0 && aec_ret != AEC_BAD_PARAMETER_WARNING) {
            return AVP_EINVAL;
        }

        aec_ret = WebRtcAec_Process(handle->aec,
                                    (const float *const *)handle->near_bands_f,
                                    handle->num_bands,
                                    handle->aec_bands_f,
                                    handle->band_samples,
                                    handle->config.stream_delay_ms,
                                    handle->stream_drift_samples);
        if (aec_ret != 0 && aec_ret != AEC_BAD_PARAMETER_WARNING) {
            return AVP_EINVAL;
        }

        (void)WebRtcAec_get_echo_status(handle->aec, &handle->last_echo_status);
        for (band = 0u; band < handle->num_bands; band++) {
            process_bands_f[band] = handle->aec_bands_f[band];
        }
    } else {
        handle->last_echo_status = 0;
#if !defined(CONFIG_CHERRYAVP_AFE_3A_NS_FIXED)
        if (handle->config.enable_ns != 0u) {
            for (band = 0u; band < handle->num_bands; band++) {
                process_bands_f[band] = handle->near_bands_f[band];
            }
        }
#endif
    }

#if defined(CONFIG_CHERRYAVP_AFE_3A_NS_FIXED)
    if (handle->config.enable_aec != 0u && has_far) {
        avp_afe_3a_float_bands_to_i16(handle, process_bands_f,
                                      handle->pcm_bands_i16);
        for (band = 0u; band < handle->num_bands; band++) {
            process_bands_i16[band] = handle->pcm_bands_i16[band];
        }
    } else {
        for (band = 0u; band < handle->num_bands; band++) {
            process_bands_i16[band] = handle->near_bands_i16[band];
        }
    }

    if (handle->config.enable_ns != 0u) {
        WebRtcNsx_Process(handle->ns,
                          (const int16_t *const *)process_bands_i16,
                          (int)handle->num_bands,
                          handle->ns_bands_i16);
        for (band = 0u; band < handle->num_bands; band++) {
            process_bands_i16[band] = handle->ns_bands_i16[band];
        }
    }
    avp_afe_3a_update_ns_metrics(handle);
#else
    if (handle->config.enable_ns != 0u) {
        WebRtcNs_Process(handle->ns, (const float *const *)process_bands_f,
                         handle->num_bands,
                         handle->ns_bands_f);
        for (band = 0u; band < handle->num_bands; band++) {
            process_bands_f[band] = handle->ns_bands_f[band];
        }
    }

    if ((handle->config.enable_aec != 0u && has_far) ||
        handle->config.enable_ns != 0u) {
        avp_afe_3a_float_bands_to_i16(handle, process_bands_f,
                                      handle->pcm_bands_i16);
        for (band = 0u; band < handle->num_bands; band++) {
            process_bands_i16[band] = handle->pcm_bands_i16[band];
        }
    } else {
        for (band = 0u; band < handle->num_bands; band++) {
            process_bands_i16[band] = handle->near_bands_i16[band];
        }
    }
    avp_afe_3a_update_ns_metrics(handle);
#endif

    if (handle->config.enable_vad != 0u) {
        vad_result = WebRtcVad_Process(handle->vad,
                                       handle->sample_rate == 32000u ?
                                           16000 :
                                           (int)handle->sample_rate,
                                       process_bands_i16[0],
                                       (size_t)handle->band_samples);
        if (vad_result < 0) {
            vad_result = -1;
        }
    }
    handle->last_vad = vad_result;
    if (vad != NULL) {
        *vad = vad_result;
    }

    if (handle->config.enable_agc != 0u) {
        uint8_t saturation_warning = 0;

        if (WebRtcAgc_Process(handle->agc,
                              (const int16_t *const *)process_bands_i16,
                              handle->num_bands,
                              handle->band_samples,
                              handle->agc_bands_i16,
                              agc_level_in,
                              &agc_level_out,
                              (int16_t)handle->last_echo_status,
                              &saturation_warning) != 0) {
            return AVP_EINVAL;
        }

        handle->agc_mic_level = agc_level_out;
        if (handle->config.agc_mode == AVP_AFE_3A_AGC_MODE_ADAPTIVE_ANALOG) {
            handle->analog_capture_level = agc_level_out;
        }
        if (saturation_warning != 0) {
            handle->last_agc_saturation = 1;
        }
        for (band = 0u; band < handle->num_bands; band++) {
            process_bands_i16[band] = handle->agc_bands_i16[band];
        }
    }

    avp_afe_3a_merge_i16_bands_to_full(handle, process_bands_i16,
                                       near_out);
    return AVP_OK;
}

float avp_afe_3a_get_ns_speech_probability(const avp_afe_3a_t *handle)
{
    return handle == NULL ? 0.0f : handle->ns_speech_probability;
}

const float *avp_afe_3a_get_ns_noise_estimate(const avp_afe_3a_t *handle,
                                              size_t *num_freq)
{
    if (handle == NULL || handle->config.enable_ns == 0u) {
        if (num_freq != NULL) {
            *num_freq = 0u;
        }
        return NULL;
    }

    if (num_freq != NULL) {
        *num_freq = handle->ns_num_freq;
    }
    return handle->ns_noise_estimate;
}

static void avp_afe_3a_copy_aec_level(avp_afe_3a_aec_level_t *dst,
                                      const AecLevel *src)
{
    dst->instant = src->instant;
    dst->average = src->average;
    dst->max = src->max;
    dst->min = src->min;
}

avp_status_t avp_afe_3a_get_aec_metrics(avp_afe_3a_t *handle,
                                        avp_afe_3a_aec_metrics_t *metrics)
{
    AecMetrics aec_metrics;

    if (handle == NULL || metrics == NULL) {
        return AVP_EINVAL;
    }
    if (handle->config.enable_aec == 0u || handle->config.aec_advanced_config.enable_metrics == 0u) {
        return AVP_EINVAL;
    }
    if (WebRtcAec_GetMetrics(handle->aec, &aec_metrics) != 0) {
        return AVP_EINVAL;
    }

    avp_afe_3a_copy_aec_level(&metrics->rerl, &aec_metrics.rerl);
    avp_afe_3a_copy_aec_level(&metrics->erl, &aec_metrics.erl);
    avp_afe_3a_copy_aec_level(&metrics->erle, &aec_metrics.erle);
    avp_afe_3a_copy_aec_level(&metrics->a_nlp, &aec_metrics.aNlp);
    return AVP_OK;
}

avp_status_t avp_afe_3a_get_aec_delay_metrics(
    avp_afe_3a_t *handle,
    avp_afe_3a_aec_delay_metrics_t *metrics)
{
    if (handle == NULL || metrics == NULL) {
        return AVP_EINVAL;
    }
    if (handle->config.enable_aec == 0u || handle->config.aec_advanced_config.enable_delay_logging == 0u) {
        return AVP_EINVAL;
    }
    if (WebRtcAec_GetDelayMetrics(handle->aec, &metrics->median,
                                  &metrics->std,
                                  &metrics->fraction_poor_delays) != 0) {
        return AVP_EINVAL;
    }
    return AVP_OK;
}

uint32_t avp_afe_3a_get_frame_samples(const avp_afe_3a_t *handle)
{
    return handle == NULL ? 0u : handle->frame_samples;
}
