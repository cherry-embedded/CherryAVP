/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "avp_ae_mixer.h"

#define AVP_AE_MIXER_MAX_GAIN 4.0f

typedef struct {
    avp_ae_mixer_input_cfg_t cfg;
    float current_weight;
    float start_weight;
    float target_weight;
    uint32_t transit_samples;
    uint32_t transit_pos;
} avp_ae_mixer_input_state_t;

struct avp_ae_mixer {
    avp_ae_mixer_config_t config;
    avp_ae_mixer_input_cfg_t *input_cfg;
    avp_ae_mixer_input_state_t *inputs;
};

static int avp_ae_mixer_valid_bool(uint8_t value)
{
    return value == 0u || value == 1u;
}

static avp_status_t avp_ae_mixer_validate_cfg(const avp_ae_mixer_config_t *config)
{
    uint32_t i;

    if (config == NULL || config->sample_rate == 0u || config->channels == 0u ||
        config->channels > 8u || config->input_count == 0u ||
        config->input_count > AVP_AE_MIXER_MAX_INPUTS || config->input_cfg == NULL ||
        !avp_ae_mixer_valid_bool(config->enable)) {
        return AVP_EINVAL;
    }
    if (config->mode != AVP_AE_MIXER_MODE_FADE_DOWNWARD &&
        config->mode != AVP_AE_MIXER_MODE_FADE_UPWARD) {
        return AVP_EINVAL;
    }

    for (i = 0u; i < config->input_count; i++) {
        const avp_ae_mixer_input_cfg_t *cfg = &config->input_cfg[i];

        if (!isfinite(cfg->weight1) || !isfinite(cfg->weight2) ||
            cfg->weight1 < 0.0f || cfg->weight1 > AVP_AE_MIXER_MAX_GAIN ||
            cfg->weight2 < 0.0f || cfg->weight2 > AVP_AE_MIXER_MAX_GAIN) {
            return AVP_ERANGE;
        }
    }

    return AVP_OK;
}

static uint32_t avp_ae_mixer_ms_to_samples(uint32_t sample_rate, uint32_t transit_time_ms)
{
    uint64_t samples = ((uint64_t)sample_rate * (uint64_t)transit_time_ms + 999u) / 1000u;

    if (samples > UINT32_MAX) {
        return UINT32_MAX;
    }
    return (uint32_t)samples;
}

static void avp_ae_mixer_input_apply_target(avp_ae_mixer_input_state_t *state,
                                            uint32_t sample_rate,
                                            avp_ae_mixer_mode_t mode)
{
    float start = state->current_weight;
    float target = (mode == AVP_AE_MIXER_MODE_FADE_UPWARD) ? state->cfg.weight2 : state->cfg.weight1;

    state->start_weight = start;
    state->target_weight = target;
    state->transit_samples = avp_ae_mixer_ms_to_samples(sample_rate, state->cfg.transit_time);
    state->transit_pos = 0u;

    if (state->transit_samples == 0u || start == target) {
        state->current_weight = target;
        state->start_weight = target;
        state->transit_pos = state->transit_samples;
    }
}

static avp_status_t avp_ae_mixer_sync_state(avp_ae_mixer_t *ctx)
{
    uint32_t i;

    for (i = 0u; i < ctx->config.input_count; i++) {
        const avp_ae_mixer_input_cfg_t *cfg = &ctx->input_cfg[i];
        avp_ae_mixer_input_state_t *state = &ctx->inputs[i];

        if (state->cfg.weight1 != cfg->weight1 ||
            state->cfg.weight2 != cfg->weight2 ||
            state->cfg.transit_time != cfg->transit_time) {
            state->cfg = *cfg;
            avp_ae_mixer_input_apply_target(state, ctx->config.sample_rate, ctx->config.mode);
        }
    }

    return AVP_OK;
}

static avp_status_t avp_ae_mixer_set_mode(avp_ae_mixer_t *ctx, avp_ae_mixer_mode_t mode)
{
    uint32_t i;

    if (mode != AVP_AE_MIXER_MODE_FADE_DOWNWARD && mode != AVP_AE_MIXER_MODE_FADE_UPWARD) {
        return AVP_EINVAL;
    }

    if (ctx->config.mode == mode) {
        return AVP_OK;
    }

    ctx->config.mode = mode;
    for (i = 0u; i < ctx->config.input_count; i++) {
        avp_ae_mixer_input_apply_target(&ctx->inputs[i], ctx->config.sample_rate, mode);
    }
    return AVP_OK;
}

avp_status_t avp_ae_mixer_open(const avp_ae_mixer_config_t *config,
                               avp_ae_mixer_t **handle)
{
    avp_ae_mixer_config_t local;
    avp_ae_mixer_t *ctx;
    avp_status_t st;
    uint32_t i;

    if (config == NULL || handle == NULL) {
        return AVP_EINVAL;
    }

    local = *config;
    st = avp_ae_mixer_validate_cfg(&local);
    if (st != AVP_OK) {
        return st;
    }

    ctx = (avp_ae_mixer_t *)avp_calloc(1u, sizeof(*ctx));
    if (ctx == NULL) {
        return AVP_ENOMEM;
    }

    ctx->input_cfg = (avp_ae_mixer_input_cfg_t *)avp_calloc(
        (size_t)local.input_count, sizeof(*ctx->input_cfg));
    if (ctx->input_cfg == NULL) {
        avp_free(ctx);
        return AVP_ENOMEM;
    }

    ctx->inputs = (avp_ae_mixer_input_state_t *)avp_calloc(
        (size_t)local.input_count, sizeof(*ctx->inputs));
    if (ctx->inputs == NULL) {
        avp_free(ctx->input_cfg);
        avp_free(ctx);
        return AVP_ENOMEM;
    }

    ctx->config = local;
    ctx->config.input_cfg = ctx->input_cfg;
    for (i = 0u; i < ctx->config.input_count; i++) {
        ctx->input_cfg[i] = local.input_cfg[i];
        ctx->inputs[i].cfg = ctx->input_cfg[i];
        ctx->inputs[i].current_weight = ctx->inputs[i].cfg.weight1;
        ctx->inputs[i].start_weight = ctx->inputs[i].cfg.weight1;
        ctx->inputs[i].target_weight = ctx->inputs[i].cfg.weight1;
        ctx->inputs[i].transit_samples = avp_ae_mixer_ms_to_samples(
            ctx->config.sample_rate, ctx->inputs[i].cfg.transit_time);
        ctx->inputs[i].transit_pos = ctx->inputs[i].transit_samples;
        if (ctx->config.mode == AVP_AE_MIXER_MODE_FADE_UPWARD) {
            avp_ae_mixer_input_apply_target(&ctx->inputs[i], ctx->config.sample_rate,
                                            ctx->config.mode);
        }
    }

    *handle = ctx;
    return AVP_OK;
}

void avp_ae_mixer_close(avp_ae_mixer_t *handle)
{
    if (handle != NULL) {
        avp_free(handle->inputs);
        avp_free(handle->input_cfg);
        avp_free(handle);
    }
}

avp_status_t avp_ae_mixer_control(avp_ae_mixer_t *handle,
                                  avp_ae_mixer_cmd_t cmd,
                                  void *arg)
{
    avp_ae_mixer_input_control_t *input_ctl;
    int *flag;

    if (handle == NULL) {
        return AVP_EINVAL;
    }

    switch (cmd) {
        case AVP_AE_MIXER_CMD_SET_ENABLE:
            flag = (int *)arg;
            if (flag == NULL || (*flag != 0 && *flag != 1))
                return AVP_EINVAL;
            handle->config.enable = (uint8_t)*flag;
            return AVP_OK;

        case AVP_AE_MIXER_CMD_GET_ENABLE:
            flag = (int *)arg;
            if (flag == NULL)
                return AVP_EINVAL;
            *flag = handle->config.enable;
            return AVP_OK;

        case AVP_AE_MIXER_CMD_SET_MODE:
            if (arg == NULL)
                return AVP_EINVAL;
            return avp_ae_mixer_set_mode(handle, *(avp_ae_mixer_mode_t *)arg);

        case AVP_AE_MIXER_CMD_GET_MODE:
            if (arg == NULL)
                return AVP_EINVAL;
            *(avp_ae_mixer_mode_t *)arg = handle->config.mode;
            return AVP_OK;

        case AVP_AE_MIXER_CMD_SET_INPUT_CFG:
            if (arg == NULL)
                return AVP_EINVAL;
            input_ctl = (avp_ae_mixer_input_control_t *)arg;
            if (input_ctl->input >= handle->config.input_count)
                return AVP_ERANGE;
            if (!isfinite(input_ctl->cfg.weight1) || !isfinite(input_ctl->cfg.weight2) ||
                input_ctl->cfg.weight1 < 0.0f || input_ctl->cfg.weight1 > AVP_AE_MIXER_MAX_GAIN ||
                input_ctl->cfg.weight2 < 0.0f || input_ctl->cfg.weight2 > AVP_AE_MIXER_MAX_GAIN) {
                return AVP_ERANGE;
            }
            handle->input_cfg[input_ctl->input] = input_ctl->cfg;
            handle->inputs[input_ctl->input].cfg = input_ctl->cfg;
            avp_ae_mixer_input_apply_target(&handle->inputs[input_ctl->input],
                                            handle->config.sample_rate,
                                            handle->config.mode);
            return AVP_OK;

        case AVP_AE_MIXER_CMD_GET_INPUT_CFG:
            if (arg == NULL)
                return AVP_EINVAL;
            input_ctl = (avp_ae_mixer_input_control_t *)arg;
            if (input_ctl->input >= handle->config.input_count)
                return AVP_ERANGE;
            input_ctl->cfg = handle->inputs[input_ctl->input].cfg;
            return AVP_OK;

        default:
            return AVP_EINVAL;
    }
}

static float avp_ae_mixer_next_weight(avp_ae_mixer_input_state_t *state)
{
    float weight;

    if (state->transit_samples == 0u || state->transit_pos >= state->transit_samples) {
        state->current_weight = state->target_weight;
        return state->current_weight;
    }

    weight = state->start_weight +
             (state->target_weight - state->start_weight) *
                 ((float)state->transit_pos / (float)state->transit_samples);
    state->transit_pos++;
    if (state->transit_pos >= state->transit_samples) {
        state->current_weight = state->target_weight;
    } else {
        state->current_weight = weight;
    }
    return state->current_weight;
}

avp_status_t avp_ae_mixer_process(avp_ae_mixer_t *handle,
                                  const int16_t *const *inputs,
                                  int16_t *out,
                                  uint32_t sample_count)
{
    uint32_t frame;
    uint32_t input;
    uint32_t channel;
    uint32_t frame_count;
    uint32_t channels;

    if (handle == NULL || out == NULL || (sample_count > 0u && inputs == NULL)) {
        return AVP_EINVAL;
    }

    for (input = 0u; input < handle->config.input_count; input++) {
        if (inputs[input] == NULL)
            return AVP_EINVAL;
    }

    if (handle->config.enable == 0u) {
        if (sample_count > 0u && out != inputs[0]) {
            memcpy(out, inputs[0], (size_t)sample_count * sizeof(int16_t));
        }
        return AVP_OK;
    }

    channels = handle->config.channels;
    if (channels == 0u || (sample_count % channels) != 0u) {
        return AVP_EINVAL;
    }
    frame_count = sample_count / channels;

    (void)avp_ae_mixer_sync_state(handle);

    for (frame = 0u; frame < frame_count; frame++) {
        float coeffs[AVP_AE_MIXER_MAX_INPUTS];
        float mixed;
        uint32_t base = frame * channels;

        for (input = 0u; input < handle->config.input_count; input++) {
            float weight = avp_ae_mixer_next_weight(&handle->inputs[input]);
            coeffs[input] = weight * weight;
        }

        for (channel = 0u; channel < channels; channel++) {
            mixed = 0.0f;
            for (input = 0u; input < handle->config.input_count; input++) {
                mixed += (float)inputs[input][base + channel] * coeffs[input];
            }

            if (mixed > 32767.0f)
                mixed = 32767.0f;
            if (mixed < -32768.0f)
                mixed = -32768.0f;
            out[base + channel] = (int16_t)mixed;
        }
    }
    return AVP_OK;
}
