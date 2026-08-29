/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "avp_ae_reverb.h"

#define AVP_AE_REVERB_MAX_DELAY_MS 60u

typedef struct {
    float *buffer;
    float *filter;
    uint32_t length;
    uint32_t position;
} avp_ae_reverb_line_t;

struct avp_ae_reverb {
    avp_ae_reverb_config_t config;
    avp_ae_reverb_line_t lines[AVP_AE_REVERB_DELAY_LINES];
};

static int avp_ae_reverb_valid(float value)
{
    return value >= 0.0f && value <= 1.0f;
}

static avp_status_t avp_ae_reverb_validate(const avp_ae_reverb_config_t *config)
{
    if (config == NULL || config->sample_rate == 0u || config->channels == 0u ||
        !avp_ae_reverb_valid(config->room_size) || !avp_ae_reverb_valid(config->damping) ||
        !avp_ae_reverb_valid(config->wet) || !avp_ae_reverb_valid(config->dry) ||
        (config->enable != 0u && config->enable != 1u)) {
        return AVP_EINVAL;
    }
    return AVP_OK;
}

static void avp_ae_reverb_reset(avp_ae_reverb_t *ctx)
{
    uint32_t i;
    for (i = 0u; i < AVP_AE_REVERB_DELAY_LINES; i++) {
        memset(ctx->lines[i].buffer, 0,
               (size_t)ctx->lines[i].length * ctx->config.channels * sizeof(float));
        ctx->lines[i].position = 0u;
        memset(ctx->lines[i].filter, 0, (size_t)ctx->config.channels * sizeof(float));
    }
}

avp_status_t avp_ae_reverb_open(const avp_ae_reverb_config_t *config,
                                avp_ae_reverb_t **handle)
{
    static const uint32_t delay_ms[AVP_AE_REVERB_DELAY_LINES] = { 29u, 37u, 43u, 53u };
    avp_ae_reverb_t *ctx;
    uint32_t i;
    if (config == NULL || handle == NULL || avp_ae_reverb_validate(config) != AVP_OK)
        return AVP_EINVAL;

    ctx = (avp_ae_reverb_t *)avp_calloc(1u, sizeof(*ctx));
    if (ctx == NULL)
        return AVP_ENOMEM;

    ctx->config = *config;
    for (i = 0u; i < AVP_AE_REVERB_DELAY_LINES; i++) {
        ctx->lines[i].length = (ctx->config.sample_rate * delay_ms[i]) / 1000u;
        if (ctx->lines[i].length == 0u || ctx->lines[i].length >
                                              (ctx->config.sample_rate * AVP_AE_REVERB_MAX_DELAY_MS) / 1000u) {
            avp_ae_reverb_close(ctx);
            return AVP_ERANGE;
        }
        ctx->lines[i].buffer = (float *)avp_calloc(
            (size_t)ctx->lines[i].length * ctx->config.channels, sizeof(float));
        if (ctx->lines[i].buffer == NULL) {
            avp_ae_reverb_close(ctx);
            return AVP_ENOMEM;
        }
        ctx->lines[i].filter = (float *)avp_calloc(ctx->config.channels, sizeof(float));
        if (ctx->lines[i].filter == NULL) {
            avp_ae_reverb_close(ctx);
            return AVP_ENOMEM;
        }
    }
    *handle = ctx;
    return AVP_OK;
}

void avp_ae_reverb_close(avp_ae_reverb_t *handle)
{
    uint32_t i;
    if (handle == NULL)
        return;
    for (i = 0u; i < AVP_AE_REVERB_DELAY_LINES; i++) {
        avp_free(handle->lines[i].buffer);
        avp_free(handle->lines[i].filter);
    }
    avp_free(handle);
}

avp_status_t avp_ae_reverb_control(avp_ae_reverb_t *handle,
                                   avp_ae_reverb_cmd_t cmd,
                                   void *arg)
{
    float value;
    if (handle == NULL)
        return AVP_EINVAL;
    switch (cmd) {
        case AVP_AE_REVERB_CMD_SET_ENABLE:
            if (arg == NULL || (*(int *)arg != 0 && *(int *)arg != 1))
                return AVP_EINVAL;
            handle->config.enable = (uint8_t) * (int *)arg;
            return AVP_OK;
        case AVP_AE_REVERB_CMD_GET_ENABLE:
            if (arg == NULL)
                return AVP_EINVAL;
            *(int *)arg = handle->config.enable;
            return AVP_OK;
        case AVP_AE_REVERB_CMD_SET_ROOM_SIZE:
        case AVP_AE_REVERB_CMD_SET_DAMPING:
        case AVP_AE_REVERB_CMD_SET_WET:
        case AVP_AE_REVERB_CMD_SET_DRY:
            if (arg == NULL)
                return AVP_EINVAL;
            value = *(float *)arg;
            if (!avp_ae_reverb_valid(value))
                return AVP_ERANGE;
            if (cmd == AVP_AE_REVERB_CMD_SET_ROOM_SIZE)
                handle->config.room_size = value;
            else if (cmd == AVP_AE_REVERB_CMD_SET_DAMPING)
                handle->config.damping = value;
            else if (cmd == AVP_AE_REVERB_CMD_SET_WET)
                handle->config.wet = value;
            else
                handle->config.dry = value;
            return AVP_OK;
        case AVP_AE_REVERB_CMD_GET_ROOM_SIZE:
            if (arg == NULL)
                return AVP_EINVAL;
            *(float *)arg = handle->config.room_size;
            return AVP_OK;
        case AVP_AE_REVERB_CMD_GET_DAMPING:
            if (arg == NULL)
                return AVP_EINVAL;
            *(float *)arg = handle->config.damping;
            return AVP_OK;
        case AVP_AE_REVERB_CMD_GET_WET:
            if (arg == NULL)
                return AVP_EINVAL;
            *(float *)arg = handle->config.wet;
            return AVP_OK;
        case AVP_AE_REVERB_CMD_GET_DRY:
            if (arg == NULL)
                return AVP_EINVAL;
            *(float *)arg = handle->config.dry;
            return AVP_OK;
        case AVP_AE_REVERB_CMD_RESET:
            avp_ae_reverb_reset(handle);
            return AVP_OK;
        default:
            return AVP_EINVAL;
    }
}

avp_status_t avp_ae_reverb_process(avp_ae_reverb_t *handle,
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
        float reverberated = 0.0f;
        uint32_t line;

        for (line = 0u; line < AVP_AE_REVERB_DELAY_LINES; line++) {
            avp_ae_reverb_line_t *state = &handle->lines[line];
            uint32_t offset = state->position * handle->config.channels + channel;
            float delayed = state->buffer[offset];
            float damped = state->filter[channel] + handle->config.damping * (delayed - state->filter[channel]);

            state->filter[channel] = damped;
            state->buffer[offset] = input + damped * (0.35f + 0.55f * handle->config.room_size);
            reverberated += damped;
            if (channel + 1u == handle->config.channels)
                state->position = (state->position + 1u) % state->length;
        }

        reverberated *= 0.25f;
        input = input * handle->config.dry + reverberated * handle->config.wet;
        if (input > 32767.0f)
            input = 32767.0f;
        if (input < -32768.0f)
            input = -32768.0f;
        out[i] = (int16_t)input;
    }
    return AVP_OK;
}
