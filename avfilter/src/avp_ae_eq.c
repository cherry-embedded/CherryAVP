/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "avp_ae_eq.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define AVP_AE_EQ_MIN_Q       0.1f
#define AVP_AE_EQ_MAX_Q       30.0f
#define AVP_AE_EQ_MIN_GAIN_DB (-24.0f)
#define AVP_AE_EQ_MAX_GAIN_DB 24.0f

typedef struct {
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
    float z1[AVP_AE_EQ_MAX_CHANNELS];
    float z2[AVP_AE_EQ_MAX_CHANNELS];
} avp_ae_eq_coeff_t;

struct avp_ae_eq {
    avp_ae_eq_config_t config;
    avp_ae_eq_coeff_t coeffs[AVP_AE_EQ_MAX_BANDS];
};

static int avp_ae_eq_valid_type(avp_ae_eq_type_t type)
{
    return type >= AVP_AE_EQ_PEAKING && type <= AVP_AE_EQ_HIGH_PASS;
}

static avp_status_t avp_ae_eq_validate_band(const avp_ae_eq_t *ctx,
                                            const avp_ae_eq_band_t *band)
{
    if (ctx == NULL || band == NULL || !avp_ae_eq_valid_type(band->type) ||
        band->frequency <= 0.0f || band->frequency >= (float)ctx->config.sample_rate * 0.5f ||
        band->q < AVP_AE_EQ_MIN_Q || band->q > AVP_AE_EQ_MAX_Q ||
        band->gain_db < AVP_AE_EQ_MIN_GAIN_DB || band->gain_db > AVP_AE_EQ_MAX_GAIN_DB) {
        return AVP_EINVAL;
    }
    if ((band->type == AVP_AE_EQ_LOW_SHELF || band->type == AVP_AE_EQ_HIGH_SHELF) &&
        band->q > 1.0f) {
        return AVP_EINVAL;
    }
    return AVP_OK;
}

static void avp_ae_eq_normalize(avp_ae_eq_coeff_t *coeff,
                                float b0, float b1, float b2,
                                float a0, float a1, float a2)
{
    coeff->b0 = b0 / a0;
    coeff->b1 = b1 / a0;
    coeff->b2 = b2 / a0;
    coeff->a1 = a1 / a0;
    coeff->a2 = a2 / a0;
}

static avp_status_t avp_ae_eq_build_band(avp_ae_eq_t *ctx, uint32_t index)
{
    const avp_ae_eq_band_t *band = &ctx->config.bands[index];
    avp_ae_eq_coeff_t *coeff = &ctx->coeffs[index];
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

    if (avp_ae_eq_validate_band(ctx, band) != AVP_OK) {
        return AVP_EINVAL;
    }
    omega = 2.0f * (float)M_PI * band->frequency / (float)ctx->config.sample_rate;
    sine = sinf(omega);
    cosine = cosf(omega);
    A = powf(10.0f, band->gain_db / 40.0f);
    alpha = sine / (2.0f * band->q);
    b0 = 1.0f;
    b1 = -2.0f * cosine;
    b2 = 1.0f;
    a0 = 1.0f + alpha;
    a1 = -2.0f * cosine;
    a2 = 1.0f - alpha;

    switch (band->type) {
        case AVP_AE_EQ_PEAKING:
            b0 = 1.0f + alpha * A;
            b1 = -2.0f * cosine;
            b2 = 1.0f - alpha * A;
            a0 = 1.0f + alpha / A;
            a1 = -2.0f * cosine;
            a2 = 1.0f - alpha / A;
            break;
        case AVP_AE_EQ_LOW_SHELF:
        case AVP_AE_EQ_HIGH_SHELF:
            sqrt_a = sqrtf(A);
            alpha = sine * 0.5f * sqrtf((A + 1.0f / A) * (1.0f / band->q - 1.0f) + 2.0f);
            if (band->type == AVP_AE_EQ_LOW_SHELF) {
                b0 = A * ((A + 1.0f) - (A - 1.0f) * cosine + 2.0f * sqrt_a * alpha);
                b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosine);
                b2 = A * ((A + 1.0f) - (A - 1.0f) * cosine - 2.0f * sqrt_a * alpha);
                a0 = (A + 1.0f) + (A - 1.0f) * cosine + 2.0f * sqrt_a * alpha;
                a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosine);
                a2 = (A + 1.0f) + (A - 1.0f) * cosine - 2.0f * sqrt_a * alpha;
            } else {
                b0 = A * ((A + 1.0f) + (A - 1.0f) * cosine + 2.0f * sqrt_a * alpha);
                b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosine);
                b2 = A * ((A + 1.0f) + (A - 1.0f) * cosine - 2.0f * sqrt_a * alpha);
                a0 = (A + 1.0f) - (A - 1.0f) * cosine + 2.0f * sqrt_a * alpha;
                a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosine);
                a2 = (A + 1.0f) - (A - 1.0f) * cosine - 2.0f * sqrt_a * alpha;
            }
            break;
        case AVP_AE_EQ_LOW_PASS:
            b0 = (1.0f - cosine) * 0.5f;
            b1 = 1.0f - cosine;
            b2 = b0;
            break;
        case AVP_AE_EQ_HIGH_PASS:
            b0 = (1.0f + cosine) * 0.5f;
            b1 = -(1.0f + cosine);
            b2 = b0;
            break;
        default:
            return AVP_EINVAL;
    }
    avp_ae_eq_normalize(coeff, b0, b1, b2, a0, a1, a2);
    return AVP_OK;
}

static avp_status_t avp_ae_eq_validate_config(const avp_ae_eq_config_t *config)
{
    if (config == NULL || config->sample_rate == 0u || config->channels == 0u ||
        config->channels > AVP_AE_EQ_MAX_CHANNELS || config->band_count > AVP_AE_EQ_MAX_BANDS ||
        (config->enable != 0u && config->enable != 1u)) {
        return AVP_EINVAL;
    }
    return AVP_OK;
}

avp_status_t avp_ae_eq_open(const avp_ae_eq_config_t *config,
                            avp_ae_eq_t **handle)
{
    avp_ae_eq_t *ctx;
    uint32_t i;

    if (config == NULL || handle == NULL || avp_ae_eq_validate_config(config) != AVP_OK) {
        return AVP_EINVAL;
    }

    ctx = (avp_ae_eq_t *)avp_calloc(1u, sizeof(*ctx));
    if (ctx == NULL)
        return AVP_ENOMEM;

    ctx->config = *config;
    for (i = 0u; i < ctx->config.band_count; i++) {
        if (avp_ae_eq_build_band(ctx, i) != AVP_OK) {
            avp_free(ctx);
            return AVP_EINVAL;
        }
    }
    *handle = ctx;
    return AVP_OK;
}

void avp_ae_eq_close(avp_ae_eq_t *handle)
{
    if (handle != NULL)
        avp_free(handle);
}

avp_status_t avp_ae_eq_control(avp_ae_eq_t *handle,
                               avp_ae_eq_cmd_t cmd,
                               void *arg)
{
    avp_ae_eq_band_update_t *update;
    if (handle == NULL)
        return AVP_EINVAL;
    switch (cmd) {
        case AVP_AE_EQ_CMD_SET_ENABLE:
            if (arg == NULL || (*(int *)arg != 0 && *(int *)arg != 1))
                return AVP_EINVAL;
            handle->config.enable = (uint8_t) * (int *)arg;
            return AVP_OK;
        case AVP_AE_EQ_CMD_GET_ENABLE:
            if (arg == NULL)
                return AVP_EINVAL;
            *(int *)arg = handle->config.enable;
            return AVP_OK;
        case AVP_AE_EQ_CMD_SET_BAND:
            if (arg == NULL)
                return AVP_EINVAL;
            update = (avp_ae_eq_band_update_t *)arg;
            if (update->index >= handle->config.band_count)
                return AVP_ERANGE;
            {
                avp_ae_eq_band_t old = handle->config.bands[update->index];
                handle->config.bands[update->index] = update->band;
                if (avp_ae_eq_build_band(handle, update->index) != AVP_OK) {
                    handle->config.bands[update->index] = old;
                    (void)avp_ae_eq_build_band(handle, update->index);
                    return AVP_EINVAL;
                }
            }
            return AVP_OK;
        case AVP_AE_EQ_CMD_GET_BAND:
            if (arg == NULL)
                return AVP_EINVAL;
            update = (avp_ae_eq_band_update_t *)arg;
            if (update->index >= handle->config.band_count)
                return AVP_ERANGE;
            update->band = handle->config.bands[update->index];
            return AVP_OK;
        case AVP_AE_EQ_CMD_RESET:
            memset(handle->coeffs, 0, sizeof(handle->coeffs));
            {
                uint32_t i;
                uint32_t c;
                for (i = 0u; i < handle->config.band_count; i++) {
                    (void)avp_ae_eq_build_band(handle, i);
                    for (c = 0u; c < AVP_AE_EQ_MAX_CHANNELS; c++) {
                        handle->coeffs[i].z1[c] = 0.0f;
                        handle->coeffs[i].z2[c] = 0.0f;
                    }
                }
            }
            return AVP_OK;
        default:
            return AVP_EINVAL;
    }
}

avp_status_t avp_ae_eq_process(avp_ae_eq_t *handle,
                               const int16_t *in,
                               int16_t *out,
                               uint32_t sample_count)
{
    uint32_t i;
    if (handle == NULL || (sample_count > 0u && (in == NULL || out == NULL)))
        return AVP_EINVAL;

    for (i = 0u; i < sample_count; i++) {
        uint32_t band;
        uint32_t channel = i % handle->config.channels;

        float value = (float)in[i];
        if (handle->config.enable != 0u) {
            for (band = 0u; band < handle->config.band_count; band++) {
                avp_ae_eq_coeff_t *coeff = &handle->coeffs[band];
                float output = coeff->b0 * value + coeff->z1[channel];
                coeff->z1[channel] = coeff->b1 * value - coeff->a1 * output + coeff->z2[channel];
                coeff->z2[channel] = coeff->b2 * value - coeff->a2 * output;
                value = output;
            }
        }

        if (value > 32767.0f)
            value = 32767.0f;
        if (value < -32768.0f)
            value = -32768.0f;
        out[i] = (int16_t)value;
    }
    return AVP_OK;
}
