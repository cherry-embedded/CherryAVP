/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "avp_ae_mixer.h"

#define AVP_AE_MIXER_MAX_GAIN 4.0f

struct avp_ae_mixer {
    avp_ae_mixer_config_t config;
};

static avp_status_t avp_ae_mixer_validate(const avp_ae_mixer_config_t *config)
{
    uint32_t i;

    if (config == NULL || config->sample_rate == 0u || config->channels == 0u ||
        config->channels > 8u || config->input_count == 0u ||
        config->input_count > AVP_AE_MIXER_MAX_INPUTS ||
        (config->enable != 0u && config->enable != 1u)) {
        return AVP_EINVAL;
    }
    for (i = 0u; i < config->input_count; i++) {
        if (config->gains[i] < 0.0f || config->gains[i] > AVP_AE_MIXER_MAX_GAIN) {
            return AVP_ERANGE;
        }
    }
    return AVP_OK;
}

avp_status_t avp_ae_mixer_open(const avp_ae_mixer_config_t *config,
                               avp_ae_mixer_t **handle)
{
    avp_ae_mixer_config_t local;
    avp_ae_mixer_t *ctx;

    if (config == NULL || handle == NULL) {
        return AVP_EINVAL;
    }

    local = *config;
    if (avp_ae_mixer_validate(&local) != AVP_OK) {
        return avp_ae_mixer_validate(&local);
    }

    ctx = (avp_ae_mixer_t *)avp_calloc(1u, sizeof(*ctx));
    if (ctx == NULL) {
        return AVP_ENOMEM;
    }

    ctx->config = local;
    *handle = ctx;
    return AVP_OK;
}

void avp_ae_mixer_close(avp_ae_mixer_t *handle)
{
    if (handle != NULL) {
        avp_free(handle);
    }
}

avp_status_t avp_ae_mixer_control(avp_ae_mixer_t *handle,
                                  avp_ae_mixer_cmd_t cmd,
                                  void *arg)
{
    avp_ae_mixer_gain_t *gain;

    if (handle == NULL) {
        return AVP_EINVAL;
    }
    switch (cmd) {
        case AVP_AE_MIXER_CMD_SET_ENABLE:
            if (arg == NULL || (*(int *)arg != 0 && *(int *)arg != 1))
                return AVP_EINVAL;
            handle->config.enable = (uint8_t) * (int *)arg;
            return AVP_OK;
        case AVP_AE_MIXER_CMD_GET_ENABLE:
            if (arg == NULL)
                return AVP_EINVAL;
            *(int *)arg = handle->config.enable;
            return AVP_OK;
        case AVP_AE_MIXER_CMD_SET_INPUT_GAIN:
            if (arg == NULL)
                return AVP_EINVAL;
            gain = (avp_ae_mixer_gain_t *)arg;
            if (gain->input >= handle->config.input_count || gain->gain < 0.0f ||
                gain->gain > AVP_AE_MIXER_MAX_GAIN)
                return AVP_ERANGE;
            handle->config.gains[gain->input] = gain->gain;
            return AVP_OK;
        case AVP_AE_MIXER_CMD_GET_INPUT_GAIN:
            if (arg == NULL)
                return AVP_EINVAL;
            gain = (avp_ae_mixer_gain_t *)arg;
            if (gain->input >= handle->config.input_count)
                return AVP_ERANGE;
            gain->gain = handle->config.gains[gain->input];
            return AVP_OK;
        case AVP_AE_MIXER_CMD_RESET_GAINS: {
            uint32_t i;
            for (i = 0u; i < handle->config.input_count; i++)
                handle->config.gains[i] = 1.0f;
        }
            return AVP_OK;
        default:
            return AVP_EINVAL;
    }
}

avp_status_t avp_ae_mixer_process(avp_ae_mixer_t *handle,
                                  const int16_t *const *inputs,
                                  int16_t *out,
                                  uint32_t sample_count)
{
    uint32_t i;
    uint32_t input;

    if (handle == NULL || out == NULL || (sample_count > 0u && inputs == NULL)) {
        return AVP_EINVAL;
    }

    for (input = 0u; input < handle->config.input_count; input++) {
        if (inputs[input] == NULL)
            return AVP_EINVAL;
    }

    for (i = 0u; i < sample_count; i++) {
        float mixed = 0.0f;

        if (handle->config.enable == 0u) {
            mixed = (float)inputs[0][i];
        } else {
            for (input = 0u; input < handle->config.input_count; input++) {
                mixed += (float)inputs[input][i] * handle->config.gains[input];
            }
        }

        if (mixed > 32767.0f)
            mixed = 32767.0f;
        if (mixed < -32768.0f)
            mixed = -32768.0f;
        out[i] = (int16_t)mixed;
    }
    return AVP_OK;
}
