/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "avp_ae_filter.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define AVP_AE_FILTER_MIN_Q       0.1f
#define AVP_AE_FILTER_MAX_Q       30.0f
#define AVP_AE_FILTER_MIN_GAIN_DB (-24.0f)
#define AVP_AE_FILTER_MAX_GAIN_DB 24.0f

typedef struct {
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
    float z1[AVP_AE_FILTER_MAX_CHANNELS];
    float z2[AVP_AE_FILTER_MAX_CHANNELS];
} avp_ae_filter_coeff_t;

struct avp_ae_filter {
    avp_ae_filter_config_t config;
    avp_ae_filter_coeff_t coeff;
};

static int avp_ae_filter_valid_type(avp_ae_filter_type_t type)
{
    return type >= AVP_AE_FILTER_LOW_PASS && type <= AVP_AE_FILTER_HIGH_SHELF;
}

static void avp_ae_filter_normalize(avp_ae_filter_coeff_t *coeff,
                                    float b0, float b1, float b2,
                                    float a0, float a1, float a2)
{
    coeff->b0 = b0 / a0;
    coeff->b1 = b1 / a0;
    coeff->b2 = b2 / a0;
    coeff->a1 = a1 / a0;
    coeff->a2 = a2 / a0;
}

static avp_status_t avp_ae_filter_validate_config(const avp_ae_filter_config_t *config)
{
    if (config == NULL || config->sample_rate == 0u || config->channels == 0u ||
        config->channels > AVP_AE_FILTER_MAX_CHANNELS ||
        !avp_ae_filter_valid_type(config->type) ||
        !isfinite(config->frequency) || !isfinite(config->q) ||
        !isfinite(config->gain_db) ||
        config->frequency <= 0.0f ||
        config->frequency >= (float)config->sample_rate * 0.5f ||
        config->q < AVP_AE_FILTER_MIN_Q || config->q > AVP_AE_FILTER_MAX_Q ||
        config->gain_db < AVP_AE_FILTER_MIN_GAIN_DB ||
        config->gain_db > AVP_AE_FILTER_MAX_GAIN_DB ||
        (config->enable != 0u && config->enable != 1u)) {
        return AVP_EINVAL;
    }
    if ((config->type == AVP_AE_FILTER_LOW_SHELF ||
         config->type == AVP_AE_FILTER_HIGH_SHELF) &&
        config->q > 1.0f) {
        return AVP_EINVAL;
    }
    return AVP_OK;
}

static avp_status_t avp_ae_filter_build(avp_ae_filter_t *ctx)
{
    const avp_ae_filter_config_t *config = &ctx->config;
    avp_ae_filter_coeff_t *coeff = &ctx->coeff;
    float omega;
    float sine;
    float cosine;
    float alpha;
    float A;
    float sqrt_a;
    float b0;
    float b1;
    float b2;
    float a0;
    float a1;
    float a2;

    if (avp_ae_filter_validate_config(config) != AVP_OK)
        return AVP_EINVAL;

    omega = 2.0f * (float)M_PI * config->frequency / (float)config->sample_rate;
    sine = sinf(omega);
    cosine = cosf(omega);
    alpha = sine / (2.0f * config->q);
    A = powf(10.0f, config->gain_db / 40.0f);

    b0 = 1.0f;
    b1 = -2.0f * cosine;
    b2 = 1.0f;
    a0 = 1.0f + alpha;
    a1 = -2.0f * cosine;
    a2 = 1.0f - alpha;

    switch (config->type) {
        case AVP_AE_FILTER_LOW_PASS:
            b0 = (1.0f - cosine) * 0.5f;
            b1 = 1.0f - cosine;
            b2 = b0;
            break;
        case AVP_AE_FILTER_HIGH_PASS:
            b0 = (1.0f + cosine) * 0.5f;
            b1 = -(1.0f + cosine);
            b2 = b0;
            break;
        case AVP_AE_FILTER_BAND_PASS:
            b0 = alpha;
            b1 = 0.0f;
            b2 = -alpha;
            break;
        case AVP_AE_FILTER_BAND_STOP:
            b0 = 1.0f;
            b1 = -2.0f * cosine;
            b2 = 1.0f;
            break;
        case AVP_AE_FILTER_ALL_PASS:
            b0 = 1.0f - alpha;
            b1 = -2.0f * cosine;
            b2 = 1.0f + alpha;
            break;
        case AVP_AE_FILTER_PEAKING:
            b0 = 1.0f + alpha * A;
            b1 = -2.0f * cosine;
            b2 = 1.0f - alpha * A;
            a0 = 1.0f + alpha / A;
            a1 = -2.0f * cosine;
            a2 = 1.0f - alpha / A;
            break;
        case AVP_AE_FILTER_LOW_SHELF:
        case AVP_AE_FILTER_HIGH_SHELF:
            sqrt_a = sqrtf(A);
            alpha = sine * 0.5f *
                    sqrtf((A + 1.0f / A) * (1.0f / config->q - 1.0f) + 2.0f);
            if (config->type == AVP_AE_FILTER_LOW_SHELF) {
                b0 = A * ((A + 1.0f) - (A - 1.0f) * cosine +
                          2.0f * sqrt_a * alpha);
                b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosine);
                b2 = A * ((A + 1.0f) - (A - 1.0f) * cosine -
                          2.0f * sqrt_a * alpha);
                a0 = (A + 1.0f) + (A - 1.0f) * cosine +
                     2.0f * sqrt_a * alpha;
                a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosine);
                a2 = (A + 1.0f) + (A - 1.0f) * cosine -
                     2.0f * sqrt_a * alpha;
            } else {
                b0 = A * ((A + 1.0f) + (A - 1.0f) * cosine +
                          2.0f * sqrt_a * alpha);
                b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosine);
                b2 = A * ((A + 1.0f) + (A - 1.0f) * cosine -
                          2.0f * sqrt_a * alpha);
                a0 = (A + 1.0f) - (A - 1.0f) * cosine +
                     2.0f * sqrt_a * alpha;
                a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosine);
                a2 = (A + 1.0f) - (A - 1.0f) * cosine -
                     2.0f * sqrt_a * alpha;
            }
            break;
        default:
            return AVP_EINVAL;
    }

    avp_ae_filter_normalize(coeff, b0, b1, b2, a0, a1, a2);
    return AVP_OK;
}

static void avp_ae_filter_reset(avp_ae_filter_t *ctx)
{
    memset(ctx->coeff.z1, 0, sizeof(ctx->coeff.z1));
    memset(ctx->coeff.z2, 0, sizeof(ctx->coeff.z2));
}

avp_status_t avp_ae_filter_open(const avp_ae_filter_config_t *config,
                                avp_ae_filter_t **handle)
{
    avp_ae_filter_t *ctx;
    if (config == NULL || handle == NULL ||
        avp_ae_filter_validate_config(config) != AVP_OK) {
        return AVP_EINVAL;
    }

    ctx = (avp_ae_filter_t *)avp_calloc(1u, sizeof(*ctx));
    if (ctx == NULL)
        return AVP_ENOMEM;

    ctx->config = *config;
    if (avp_ae_filter_build(ctx) != AVP_OK) {
        avp_free(ctx);
        return AVP_EINVAL;
    }
    *handle = ctx;
    return AVP_OK;
}

void avp_ae_filter_close(avp_ae_filter_t *handle)
{
    if (handle != NULL)
        avp_free(handle);
}

avp_status_t avp_ae_filter_control(avp_ae_filter_t *handle,
                                   avp_ae_filter_cmd_t cmd,
                                   void *arg)
{
    avp_ae_filter_config_t old_config;
    float value;
    avp_ae_filter_type_t type;

    if (handle == NULL)
        return AVP_EINVAL;

    switch (cmd) {
        case AVP_AE_FILTER_CMD_SET_ENABLE:
            if (arg == NULL || (*(int *)arg != 0 && *(int *)arg != 1))
                return AVP_EINVAL;
            handle->config.enable = (uint8_t) * (int *)arg;
            return AVP_OK;
        case AVP_AE_FILTER_CMD_GET_ENABLE:
            if (arg == NULL)
                return AVP_EINVAL;
            *(int *)arg = handle->config.enable;
            return AVP_OK;
        case AVP_AE_FILTER_CMD_SET_CONFIG:
            if (arg == NULL ||
                avp_ae_filter_validate_config((avp_ae_filter_config_t *)arg) != AVP_OK) {
                return AVP_EINVAL;
            }
            old_config = handle->config;
            handle->config = *(avp_ae_filter_config_t *)arg;
            if (avp_ae_filter_build(handle) != AVP_OK) {
                handle->config = old_config;
                (void)avp_ae_filter_build(handle);
                return AVP_EINVAL;
            }
            avp_ae_filter_reset(handle);
            return AVP_OK;
        case AVP_AE_FILTER_CMD_GET_CONFIG:
            if (arg == NULL)
                return AVP_EINVAL;
            *(avp_ae_filter_config_t *)arg = handle->config;
            return AVP_OK;
        case AVP_AE_FILTER_CMD_SET_TYPE:
            if (arg == NULL)
                return AVP_EINVAL;
            type = *(avp_ae_filter_type_t *)arg;
            if (!avp_ae_filter_valid_type(type))
                return AVP_ERANGE;
            old_config = handle->config;
            handle->config.type = type;
            if (avp_ae_filter_build(handle) != AVP_OK) {
                handle->config = old_config;
                (void)avp_ae_filter_build(handle);
                return AVP_EINVAL;
            }
            return AVP_OK;
        case AVP_AE_FILTER_CMD_GET_TYPE:
            if (arg == NULL)
                return AVP_EINVAL;
            *(avp_ae_filter_type_t *)arg = handle->config.type;
            return AVP_OK;
        case AVP_AE_FILTER_CMD_SET_FREQUENCY:
        case AVP_AE_FILTER_CMD_SET_Q:
        case AVP_AE_FILTER_CMD_SET_GAIN_DB:
            if (arg == NULL)
                return AVP_EINVAL;
            value = *(float *)arg;
            if (!isfinite(value))
                return AVP_ERANGE;
            old_config = handle->config;
            if (cmd == AVP_AE_FILTER_CMD_SET_FREQUENCY)
                handle->config.frequency = value;
            else if (cmd == AVP_AE_FILTER_CMD_SET_Q)
                handle->config.q = value;
            else
                handle->config.gain_db = value;
            if (avp_ae_filter_build(handle) != AVP_OK) {
                handle->config = old_config;
                (void)avp_ae_filter_build(handle);
                return AVP_ERANGE;
            }
            return AVP_OK;
        case AVP_AE_FILTER_CMD_GET_FREQUENCY:
            if (arg == NULL)
                return AVP_EINVAL;
            *(float *)arg = handle->config.frequency;
            return AVP_OK;
        case AVP_AE_FILTER_CMD_GET_Q:
            if (arg == NULL)
                return AVP_EINVAL;
            *(float *)arg = handle->config.q;
            return AVP_OK;
        case AVP_AE_FILTER_CMD_GET_GAIN_DB:
            if (arg == NULL)
                return AVP_EINVAL;
            *(float *)arg = handle->config.gain_db;
            return AVP_OK;
        case AVP_AE_FILTER_CMD_RESET:
            avp_ae_filter_reset(handle);
            return AVP_OK;
        default:
            return AVP_EINVAL;
    }
}

avp_status_t avp_ae_filter_process(avp_ae_filter_t *handle,
                                   const int16_t *in,
                                   int16_t *out,
                                   uint32_t sample_count)
{
    uint32_t i;
    if (handle == NULL || (sample_count > 0u && (in == NULL || out == NULL)))
        return AVP_EINVAL;

    if (handle->config.enable == 0u) {
        if (sample_count > 0u && out != in)
            memcpy(out, in, (size_t)sample_count * sizeof(*out));
        return AVP_OK;
    }

    for (i = 0u; i < sample_count; i++) {
        uint32_t channel = i % handle->config.channels;
        float input = (float)in[i];
        float value = handle->coeff.b0 * input + handle->coeff.z1[channel];

        handle->coeff.z1[channel] = handle->coeff.b1 * input -
                                    handle->coeff.a1 * value +
                                    handle->coeff.z2[channel];
        handle->coeff.z2[channel] = handle->coeff.b2 * input -
                                    handle->coeff.a2 * value;

        if (value > 32767.0f)
            value = 32767.0f;
        if (value < -32768.0f)
            value = -32768.0f;
        out[i] = (int16_t)value;
    }
    return AVP_OK;
}
