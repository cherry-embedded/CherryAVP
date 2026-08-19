/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "avp_ae_compressor.h"

struct avp_ae_compressor {
    avp_ae_compressor_config_t config;
    float envelope;
    float gain;
};

static int avp_ae_compressor_valid(const avp_ae_compressor_config_t *c)
{
    return c != NULL && c->sample_rate != 0u && c->channels > 0u && c->channels <= 8u &&
           c->threshold_db >= -60.0f && c->threshold_db <= 0.0f && c->ratio >= 1.0f && c->ratio <= 20.0f &&
           c->attack_ms > 0.0f && c->attack_ms <= 2000.0f && c->release_ms > 0.0f && c->release_ms <= 5000.0f &&
           c->makeup_db >= -24.0f && c->makeup_db <= 24.0f && (c->enable == 0u || c->enable == 1u);
}

static float avp_ae_compressor_db_to_linear(float db)
{
    return powf(10.0f, db / 20.0f);
}

static float avp_ae_compressor_coeff(float ms, uint32_t sample_rate)
{
    return expf(-1.0f / (0.001f * ms * (float)sample_rate));
}

avp_status_t avp_ae_compressor_open(const avp_ae_compressor_config_t *config,
                                    avp_ae_compressor_t **handle)
{
    avp_ae_compressor_t *ctx;
    if (config == NULL || handle == NULL || !avp_ae_compressor_valid(config))
        return AVP_EINVAL;

    ctx = (avp_ae_compressor_t *)avp_calloc(1u, sizeof(*ctx));
    if (ctx == NULL)
        return AVP_ENOMEM;

    ctx->config = *config;
    ctx->gain = 1.0f;
    *handle = ctx;
    return AVP_OK;
}

void avp_ae_compressor_close(avp_ae_compressor_t *handle)
{
    if (handle != NULL)
        avp_free(handle);
}

avp_status_t avp_ae_compressor_control(avp_ae_compressor_t *handle,
                                       avp_ae_compressor_cmd_t cmd,
                                       void *arg)
{
    float *value;
    if (handle == NULL)
        return AVP_EINVAL;
    switch (cmd) {
        case AVP_AE_COMPRESSOR_CMD_SET_ENABLE:
            if (arg == NULL || (*(int *)arg != 0 && *(int *)arg != 1))
                return AVP_EINVAL;
            handle->config.enable = (uint8_t) * (int *)arg;
            return AVP_OK;
        case AVP_AE_COMPRESSOR_CMD_GET_ENABLE:
            if (arg == NULL)
                return AVP_EINVAL;
            *(int *)arg = handle->config.enable;
            return AVP_OK;
        case AVP_AE_COMPRESSOR_CMD_SET_THRESHOLD_DB:
        case AVP_AE_COMPRESSOR_CMD_SET_RATIO:
        case AVP_AE_COMPRESSOR_CMD_SET_ATTACK_MS:
        case AVP_AE_COMPRESSOR_CMD_SET_RELEASE_MS:
        case AVP_AE_COMPRESSOR_CMD_SET_MAKEUP_DB:
            if (arg == NULL)
                return AVP_EINVAL;
            value = (float *)arg;
            if (!isfinite(*value))
                return AVP_ERANGE;
            if (cmd == AVP_AE_COMPRESSOR_CMD_SET_THRESHOLD_DB && (*value < -60.0f || *value > 0.0f))
                return AVP_ERANGE;
            if (cmd == AVP_AE_COMPRESSOR_CMD_SET_RATIO && (*value < 1.0f || *value > 20.0f))
                return AVP_ERANGE;
            if (cmd == AVP_AE_COMPRESSOR_CMD_SET_ATTACK_MS && (*value <= 0.0f || *value > 2000.0f))
                return AVP_ERANGE;
            if (cmd == AVP_AE_COMPRESSOR_CMD_SET_RELEASE_MS && (*value <= 0.0f || *value > 5000.0f))
                return AVP_ERANGE;
            if (cmd == AVP_AE_COMPRESSOR_CMD_SET_MAKEUP_DB && (*value < -24.0f || *value > 24.0f))
                return AVP_ERANGE;
            if (cmd == AVP_AE_COMPRESSOR_CMD_SET_THRESHOLD_DB)
                handle->config.threshold_db = *value;
            else if (cmd == AVP_AE_COMPRESSOR_CMD_SET_RATIO)
                handle->config.ratio = *value;
            else if (cmd == AVP_AE_COMPRESSOR_CMD_SET_ATTACK_MS)
                handle->config.attack_ms = *value;
            else if (cmd == AVP_AE_COMPRESSOR_CMD_SET_RELEASE_MS)
                handle->config.release_ms = *value;
            else
                handle->config.makeup_db = *value;
            return AVP_OK;
        case AVP_AE_COMPRESSOR_CMD_GET_THRESHOLD_DB:
            if (arg == NULL)
                return AVP_EINVAL;
            *(float *)arg = handle->config.threshold_db;
            return AVP_OK;
        case AVP_AE_COMPRESSOR_CMD_GET_RATIO:
            if (arg == NULL)
                return AVP_EINVAL;
            *(float *)arg = handle->config.ratio;
            return AVP_OK;
        case AVP_AE_COMPRESSOR_CMD_GET_ATTACK_MS:
            if (arg == NULL)
                return AVP_EINVAL;
            *(float *)arg = handle->config.attack_ms;
            return AVP_OK;
        case AVP_AE_COMPRESSOR_CMD_GET_RELEASE_MS:
            if (arg == NULL)
                return AVP_EINVAL;
            *(float *)arg = handle->config.release_ms;
            return AVP_OK;
        case AVP_AE_COMPRESSOR_CMD_GET_MAKEUP_DB:
            if (arg == NULL)
                return AVP_EINVAL;
            *(float *)arg = handle->config.makeup_db;
            return AVP_OK;
        case AVP_AE_COMPRESSOR_CMD_RESET:
            handle->envelope = 0.0f;
            handle->gain = 1.0f;
            return AVP_OK;
        default:
            return AVP_EINVAL;
    }
}

avp_status_t avp_ae_compressor_process(avp_ae_compressor_t *handle,
                                       const int16_t *in,
                                       int16_t *out,
                                       uint32_t sample_count)
{
    uint32_t i;
    float attack;
    float release;
    float makeup;
    if (handle == NULL || (sample_count > 0u && (in == NULL || out == NULL)))
        return AVP_EINVAL;

    attack = avp_ae_compressor_coeff(handle->config.attack_ms, handle->config.sample_rate);
    release = avp_ae_compressor_coeff(handle->config.release_ms, handle->config.sample_rate);
    makeup = avp_ae_compressor_db_to_linear(handle->config.makeup_db);

    if (handle->config.enable == 0u) {
        if (sample_count > 0u && out != in)
            memcpy(out, in, (size_t)sample_count * sizeof(*out));
        return AVP_OK;
    }

    for (i = 0u; i < sample_count;) {
        uint32_t frame_samples = handle->config.channels;
        uint32_t channel;
        float level = 0.0f;
        float target_gain = 1.0f;
        float threshold = avp_ae_compressor_db_to_linear(handle->config.threshold_db);
        float db;

        if (frame_samples > sample_count - i)
            frame_samples = sample_count - i;

        for (channel = 0u; channel < frame_samples; channel++) {
            float channel_level = fabsf((float)in[i + channel]) / 32768.0f;
            if (channel_level > level)
                level = channel_level;
        }

        if (level > handle->envelope)
            handle->envelope = attack * handle->envelope + (1.0f - attack) * level;
        else
            handle->envelope = release * handle->envelope + (1.0f - release) * level;

        db = 20.0f * log10f(handle->envelope + 1.0e-12f);
        if (handle->envelope > threshold) {
            float compressed_db = handle->config.threshold_db + (db - handle->config.threshold_db) / handle->config.ratio;
            target_gain = avp_ae_compressor_db_to_linear(compressed_db - db);
        }

        if (target_gain < handle->gain)
            handle->gain = attack * handle->gain + (1.0f - attack) * target_gain;
        else
            handle->gain = release * handle->gain + (1.0f - release) * target_gain;

        for (channel = 0u; channel < frame_samples; channel++) {
            float output = (float)in[i + channel] * handle->gain * makeup;
            if (output > 32767.0f)
                output = 32767.0f;
            if (output < -32768.0f)
                output = -32768.0f;
            out[i + channel] = (int16_t)output;
        }

        i += frame_samples;
    }
    return AVP_OK;
}
