/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "avp_ae_howling.h"

#include <float.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define AVP_AE_HOWLING_MIN_FRAME_SAMPLES 64u
#define AVP_AE_HOWLING_MAX_FRAME_SAMPLES 1024u
#define AVP_AE_HOWLING_MIN_THRESHOLD_DB  6.0f
#define AVP_AE_HOWLING_MAX_THRESHOLD_DB  60.0f
#define AVP_AE_HOWLING_MIN_Q             2.0f
#define AVP_AE_HOWLING_MAX_Q             30.0f
#define AVP_AE_HOWLING_HOLD_FRAMES       8u

typedef struct {
    float frequency;
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
    float x1[2];
    float x2[2];
    float y1[2];
    float y2[2];
    uint8_t active;
    uint8_t hold_frames;
} avp_ae_howling_notch_t;

struct avp_ae_howling {
    avp_ae_howling_config_t config;
    float *analysis;
    uint32_t analysis_pos;
    uint8_t input_channel;
    avp_ae_howling_notch_t notches[AVP_AE_HOWLING_MAX_NOTCHES];
};

static int avp_ae_howling_valid_bool(uint8_t value)
{
    return value == 0u || value == 1u;
}

static int avp_ae_howling_is_power_of_two(uint32_t value)
{
    return value != 0u && (value & (value - 1u)) == 0u;
}

static avp_status_t avp_ae_howling_validate(const avp_ae_howling_config_t *config)
{
    if (config == NULL || config->sample_rate == 0u ||
        config->channels == 0u || config->channels > 2u ||
        config->frame_samples < AVP_AE_HOWLING_MIN_FRAME_SAMPLES ||
        config->frame_samples > AVP_AE_HOWLING_MAX_FRAME_SAMPLES ||
        !avp_ae_howling_is_power_of_two(config->frame_samples) ||
        config->max_notches == 0u ||
        config->max_notches > AVP_AE_HOWLING_MAX_NOTCHES ||
        config->threshold_db < AVP_AE_HOWLING_MIN_THRESHOLD_DB ||
        config->threshold_db > AVP_AE_HOWLING_MAX_THRESHOLD_DB ||
        config->notch_q < AVP_AE_HOWLING_MIN_Q ||
        config->notch_q > AVP_AE_HOWLING_MAX_Q ||
        !avp_ae_howling_valid_bool(config->enable)) {
        return AVP_EINVAL;
    }
    return AVP_OK;
}

static void avp_ae_howling_set_notch(avp_ae_howling_notch_t *notch,
                                     float frequency,
                                     float sample_rate,
                                     float q)
{
    float omega = 2.0f * (float)M_PI * frequency / sample_rate;
    float cosine = cosf(omega);
    float radius = expf(-(float)M_PI * frequency / (q * sample_rate));

    notch->frequency = frequency;
    notch->b0 = 1.0f;
    notch->b1 = -2.0f * cosine;
    notch->b2 = 1.0f;
    notch->a1 = -2.0f * radius * cosine;
    notch->a2 = radius * radius;
    notch->active = 1u;
    notch->hold_frames = AVP_AE_HOWLING_HOLD_FRAMES;
}

static void avp_ae_howling_reset_notch(avp_ae_howling_notch_t *notch)
{
    memset(notch, 0, sizeof(*notch));
}

static void avp_ae_howling_analyze(avp_ae_howling_t *ctx)
{
    uint32_t n = ctx->config.frame_samples;
    uint32_t max_bin = n / 2u;
    float average_power = 0.0f;
    float threshold_ratio;
    uint8_t used[AVP_AE_HOWLING_MAX_NOTCHES] = { 0 };
    uint8_t found = 0u;
    uint32_t i;

    for (i = 0u; i < n; i++) {
        average_power += ctx->analysis[i] * ctx->analysis[i];
    }
    average_power = average_power / (float)n + 1.0f;
    threshold_ratio = powf(10.0f, ctx->config.threshold_db / 10.0f);

    for (i = 2u; i + 1u < max_bin && found < ctx->config.max_notches; i++) {
        float real = 0.0f;
        float imag = 0.0f;
        float power;
        float left_real = 0.0f;
        float left_imag = 0.0f;
        float right_real = 0.0f;
        float right_imag = 0.0f;
        uint32_t j;

        for (j = 0u; j < n; j++) {
            float angle = 2.0f * (float)M_PI * (float)(i * j) / (float)n;
            float sample = ctx->analysis[j];
            real += sample * cosf(angle);
            imag -= sample * sinf(angle);
            if (i > 0u) {
                float left_angle = angle - 2.0f * (float)M_PI * (float)j / (float)n;
                left_real += sample * cosf(left_angle);
                left_imag -= sample * sinf(left_angle);
            }
            if (i + 1u < max_bin) {
                float right_angle = angle + 2.0f * (float)M_PI * (float)j / (float)n;
                right_real += sample * cosf(right_angle);
                right_imag -= sample * sinf(right_angle);
            }
        }
        power = real * real + imag * imag;
        if (power < (float)n * average_power * threshold_ratio ||
            power < left_real * left_real + left_imag * left_imag ||
            power < right_real * right_real + right_imag * right_imag) {
            continue;
        }

        uint8_t slot;
        float frequency = (float)i * (float)ctx->config.sample_rate / (float)n;
        float spacing = (float)ctx->config.sample_rate / (float)n * 2.0f;
        for (slot = 0u; slot < ctx->config.max_notches; slot++) {
            if (ctx->notches[slot].active &&
                fabsf(ctx->notches[slot].frequency - frequency) < spacing) {
                avp_ae_howling_set_notch(&ctx->notches[slot], frequency,
                                         (float)ctx->config.sample_rate,
                                         ctx->config.notch_q);
                used[slot] = 1u;
                break;
            }
        }
        if (slot == ctx->config.max_notches) {
            for (slot = 0u; slot < ctx->config.max_notches; slot++) {
                if (!ctx->notches[slot].active || ctx->notches[slot].hold_frames == 0u) {
                    avp_ae_howling_set_notch(&ctx->notches[slot], frequency,
                                             (float)ctx->config.sample_rate,
                                             ctx->config.notch_q);
                    used[slot] = 1u;
                    break;
                }
            }
        }
        if (slot < ctx->config.max_notches) {
            found++;
        }
    }

    for (i = 0u; i < ctx->config.max_notches; i++) {
        if (ctx->notches[i].active && !used[i]) {
            if (ctx->notches[i].hold_frames > 0u) {
                ctx->notches[i].hold_frames--;
            } else {
                avp_ae_howling_reset_notch(&ctx->notches[i]);
            }
        }
    }
}

static float avp_ae_howling_filter_sample(avp_ae_howling_t *ctx,
                                          uint32_t channel,
                                          float sample)
{
    uint32_t i;
    float value = sample;

    if (ctx->config.enable == 0u) {
        return value;
    }
    for (i = 0u; i < ctx->config.max_notches; i++) {
        avp_ae_howling_notch_t *notch = &ctx->notches[i];
        float output;
        if (!notch->active) {
            continue;
        }
        output = notch->b0 * value + notch->b1 * notch->x1[channel] +
                 notch->b2 * notch->x2[channel] - notch->a1 * notch->y1[channel] -
                 notch->a2 * notch->y2[channel];
        notch->x2[channel] = notch->x1[channel];
        notch->x1[channel] = value;
        notch->y2[channel] = notch->y1[channel];
        notch->y1[channel] = output;
        value = output;
    }
    return value;
}

avp_status_t avp_ae_howling_open(const avp_ae_howling_config_t *config,
                                 avp_ae_howling_t **handle)
{
    avp_ae_howling_t *ctx;
    avp_ae_howling_config_t local;

    if (config == NULL || handle == NULL) {
        return AVP_EINVAL;
    }

    local = *config;
    if (local.frame_samples == 0u) {
        local.frame_samples = AVP_AE_HOWLING_DEFAULT_FRAME_SAMPLES;
    }
    if (local.max_notches == 0u) {
        local.max_notches = AVP_AE_HOWLING_MAX_NOTCHES;
    }
    if (local.threshold_db == 0.0f) {
        local.threshold_db = 18.0f;
    }
    if (local.notch_q == 0.0f) {
        local.notch_q = 12.0f;
    }
    if (avp_ae_howling_validate(&local) != AVP_OK) {
        return AVP_EINVAL;
    }

    ctx = (avp_ae_howling_t *)avp_calloc(1u, sizeof(*ctx));
    if (ctx == NULL) {
        return AVP_ENOMEM;
    }

    ctx->analysis = (float *)avp_calloc(local.frame_samples, sizeof(float));
    if (ctx->analysis == NULL) {
        avp_free(ctx);
        return AVP_ENOMEM;
    }

    ctx->config = local;
    *handle = ctx;
    return AVP_OK;
}

void avp_ae_howling_close(avp_ae_howling_t *handle)
{
    if (handle == NULL) {
        return;
    }
    avp_free(handle->analysis);
    avp_free(handle);
}

avp_status_t avp_ae_howling_control(avp_ae_howling_t *handle,
                                    avp_ae_howling_cmd_t cmd,
                                    void *arg)
{
    if (handle == NULL) {
        return AVP_EINVAL;
    }
    switch (cmd) {
        case AVP_AE_HOWLING_CMD_SET_ENABLE:
            if (arg == NULL || (*(int *)arg != 0 && *(int *)arg != 1))
                return AVP_EINVAL;
            handle->config.enable = (uint8_t) * (int *)arg;
            return AVP_OK;
        case AVP_AE_HOWLING_CMD_GET_ENABLE:
            if (arg == NULL)
                return AVP_EINVAL;
            *(int *)arg = handle->config.enable;
            return AVP_OK;
        case AVP_AE_HOWLING_CMD_SET_THRESHOLD_DB:
            if (arg == NULL || *(float *)arg < AVP_AE_HOWLING_MIN_THRESHOLD_DB ||
                *(float *)arg > AVP_AE_HOWLING_MAX_THRESHOLD_DB)
                return AVP_EINVAL;
            handle->config.threshold_db = *(float *)arg;
            return AVP_OK;
        case AVP_AE_HOWLING_CMD_GET_THRESHOLD_DB:
            if (arg == NULL)
                return AVP_EINVAL;
            *(float *)arg = handle->config.threshold_db;
            return AVP_OK;
        case AVP_AE_HOWLING_CMD_SET_NOTCH_Q:
            if (arg == NULL || *(float *)arg < AVP_AE_HOWLING_MIN_Q ||
                *(float *)arg > AVP_AE_HOWLING_MAX_Q)
                return AVP_EINVAL;
            handle->config.notch_q = *(float *)arg;

            {
                uint32_t i;
                for (i = 0u; i < handle->config.max_notches; i++) {
                    if (handle->notches[i].active) {
                        avp_ae_howling_set_notch(&handle->notches[i],
                                                 handle->notches[i].frequency,
                                                 (float)handle->config.sample_rate,
                                                 handle->config.notch_q);
                    }
                }
            }
            return AVP_OK;
        case AVP_AE_HOWLING_CMD_GET_NOTCH_Q:
            if (arg == NULL)
                return AVP_EINVAL;
            *(float *)arg = handle->config.notch_q;
            return AVP_OK;
        case AVP_AE_HOWLING_CMD_SET_MAX_NOTCHES:
            if (arg == NULL || *(uint8_t *)arg == 0u ||
                *(uint8_t *)arg > AVP_AE_HOWLING_MAX_NOTCHES)
                return AVP_EINVAL;
            handle->config.max_notches = *(uint8_t *)arg;

            {
                uint32_t i;
                for (i = handle->config.max_notches; i < AVP_AE_HOWLING_MAX_NOTCHES; i++) {
                    avp_ae_howling_reset_notch(&handle->notches[i]);
                }
            }
            return AVP_OK;
        case AVP_AE_HOWLING_CMD_GET_MAX_NOTCHES:
            if (arg == NULL)
                return AVP_EINVAL;
            *(uint8_t *)arg = handle->config.max_notches;
            return AVP_OK;
        case AVP_AE_HOWLING_CMD_GET_ACTIVE_NOTCHES:
            if (arg == NULL)
                return AVP_EINVAL;
            {
                uint32_t i;
                uint8_t count = 0u;
                for (i = 0u; i < handle->config.max_notches; i++)
                    count += handle->notches[i].active != 0u;
                *(uint8_t *)arg = count;
            }
            return AVP_OK;
        case AVP_AE_HOWLING_CMD_RESET:
            memset(handle->analysis, 0, handle->config.frame_samples * sizeof(float));
            handle->analysis_pos = 0u;
            handle->input_channel = 0u;
            memset(handle->notches, 0, sizeof(handle->notches));
            return AVP_OK;
        default:
            return AVP_EINVAL;
    }
}

avp_status_t avp_ae_howling_process(avp_ae_howling_t *handle,
                                    const int16_t *in,
                                    int16_t *out,
                                    uint32_t sample_count)
{
    uint32_t i;
    if (handle == NULL || (sample_count > 0u && (in == NULL || out == NULL)))
        return AVP_EINVAL;

    for (i = 0u; i < sample_count; i++) {
        uint32_t channel = handle->input_channel;
        float input = (float)in[i];
        float filtered = avp_ae_howling_filter_sample(handle, channel, input);
        uint32_t frame_index = handle->analysis_pos;

        if (handle->config.channels == 1u) {
            handle->analysis[frame_index] = input;
        } else {
            if (channel == 0u)
                handle->analysis[frame_index] = input * 0.5f;
            else
                handle->analysis[frame_index] += input * 0.5f;
        }

        out[i] = (int16_t)(filtered > 32767.0f  ? 32767.0f :
                           filtered < -32768.0f ? -32768.0f :
                                                  filtered);
        handle->input_channel++;
        if (handle->input_channel == handle->config.channels) {
            handle->input_channel = 0u;
            handle->analysis_pos++;
            if (handle->analysis_pos == handle->config.frame_samples) {
                avp_ae_howling_analyze(handle);
                handle->analysis_pos = 0u;
            }
        }
    }
    return AVP_OK;
}
