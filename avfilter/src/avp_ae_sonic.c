/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "avp_ae_sonic.h"
#include "sonic.h"

struct avp_ae_sonic {
    avp_ae_sonic_config_t config;
    sonicStream stream;
};

static int avp_ae_sonic_valid_channels(uint8_t channels)
{
    return channels >= 1u && channels <= AVP_AE_SONIC_MAX_CHANNELS;
}

static int avp_ae_sonic_valid_bool(uint8_t value)
{
    return value == 0u || value == 1u;
}

static int avp_ae_sonic_valid_positive(float value)
{
    return value > 0.0f;
}

static void avp_ae_sonic_set_default_params(avp_ae_sonic_config_t *config)
{
    if (config->speed == 0.0f) {
        config->speed = 1.0f;
    }
    if (config->pitch == 0.0f) {
        config->pitch = 1.0f;
    }
    if (config->rate == 0.0f) {
        config->rate = 1.0f;
    }
    if (config->volume == 0.0f) {
        config->volume = 1.0f;
    }
}

static avp_status_t avp_ae_sonic_validate_config(const avp_ae_sonic_config_t *config)
{
    if (config == NULL || config->sample_rate == 0u ||
        !avp_ae_sonic_valid_channels(config->channels) ||
        !avp_ae_sonic_valid_positive(config->speed) ||
        !avp_ae_sonic_valid_positive(config->pitch) ||
        !avp_ae_sonic_valid_positive(config->rate) ||
        !avp_ae_sonic_valid_positive(config->volume) ||
        !avp_ae_sonic_valid_bool(config->use_chord_pitch) ||
        !avp_ae_sonic_valid_bool(config->quality)) {
        return AVP_EINVAL;
    }

    return AVP_OK;
}

static void avp_ae_sonic_apply_config(avp_ae_sonic_t *ctx)
{
    sonicSetSpeed(ctx->stream, ctx->config.speed);
    sonicSetPitch(ctx->stream, ctx->config.pitch);
    sonicSetRate(ctx->stream, ctx->config.rate);
    sonicSetVolume(ctx->stream, ctx->config.volume);
    sonicSetChordPitch(ctx->stream, ctx->config.use_chord_pitch);
    sonicSetQuality(ctx->stream, ctx->config.quality);
}

static avp_status_t avp_ae_sonic_recreate_stream(avp_ae_sonic_t *ctx)
{
    sonicStream stream;

    stream = sonicCreateStream((int)ctx->config.sample_rate,
                               (int)ctx->config.channels);
    if (stream == NULL) {
        return AVP_ENOMEM;
    }

    if (ctx->stream != NULL) {
        sonicDestroyStream(ctx->stream);
    }

    ctx->stream = stream;
    avp_ae_sonic_apply_config(ctx);
    return AVP_OK;
}

avp_status_t avp_ae_sonic_open(const avp_ae_sonic_config_t *config,
                               avp_ae_sonic_t **handle)
{
    avp_ae_sonic_t *ctx;
    avp_ae_sonic_config_t local_config;
    avp_status_t ret;

    if (config == NULL || handle == NULL) {
        return AVP_EINVAL;
    }

    local_config = *config;
    avp_ae_sonic_set_default_params(&local_config);
    ret = avp_ae_sonic_validate_config(&local_config);
    if (ret != AVP_OK) {
        return ret;
    }

    ctx = (avp_ae_sonic_t *)avp_calloc(1, sizeof(avp_ae_sonic_t));
    if (ctx == NULL) {
        return AVP_ENOMEM;
    }

    ctx->config = local_config;
    ret = avp_ae_sonic_recreate_stream(ctx);
    if (ret != AVP_OK) {
        avp_free(ctx);
        return ret;
    }

    *handle = ctx;
    return AVP_OK;
}

void avp_ae_sonic_close(avp_ae_sonic_t *handle)
{
    if (handle == NULL) {
        return;
    }

    if (handle->stream != NULL) {
        sonicDestroyStream(handle->stream);
    }
    avp_free(handle);
}

avp_status_t avp_ae_sonic_control(avp_ae_sonic_t *handle,
                                  avp_ae_sonic_cmd_t cmd,
                                  void *arg)
{
    float value_f;
    uint32_t value_u32;
    uint8_t value_u8;
    int value_i;

    if (handle == NULL) {
        return AVP_EINVAL;
    }

    switch (cmd) {
        case AVP_AE_SONIC_CMD_SET_SPEED:
            if (arg == NULL || !avp_ae_sonic_valid_positive(*(float *)arg)) {
                return AVP_EINVAL;
            }
            value_f = *(float *)arg;
            sonicSetSpeed(handle->stream, value_f);
            handle->config.speed = value_f;
            return AVP_OK;

        case AVP_AE_SONIC_CMD_GET_SPEED:
            if (arg == NULL) {
                return AVP_EINVAL;
            }
            *(float *)arg = sonicGetSpeed(handle->stream);
            return AVP_OK;

        case AVP_AE_SONIC_CMD_SET_PITCH:
            if (arg == NULL || !avp_ae_sonic_valid_positive(*(float *)arg)) {
                return AVP_EINVAL;
            }
            value_f = *(float *)arg;
            sonicSetPitch(handle->stream, value_f);
            handle->config.pitch = value_f;
            return AVP_OK;

        case AVP_AE_SONIC_CMD_GET_PITCH:
            if (arg == NULL) {
                return AVP_EINVAL;
            }
            *(float *)arg = sonicGetPitch(handle->stream);
            return AVP_OK;

        case AVP_AE_SONIC_CMD_SET_RATE:
            if (arg == NULL || !avp_ae_sonic_valid_positive(*(float *)arg)) {
                return AVP_EINVAL;
            }
            value_f = *(float *)arg;
            sonicSetRate(handle->stream, value_f);
            handle->config.rate = value_f;
            return AVP_OK;

        case AVP_AE_SONIC_CMD_GET_RATE:
            if (arg == NULL) {
                return AVP_EINVAL;
            }
            *(float *)arg = sonicGetRate(handle->stream);
            return AVP_OK;

        case AVP_AE_SONIC_CMD_SET_VOLUME:
            if (arg == NULL || !avp_ae_sonic_valid_positive(*(float *)arg)) {
                return AVP_EINVAL;
            }
            value_f = *(float *)arg;
            sonicSetVolume(handle->stream, value_f);
            handle->config.volume = value_f;
            return AVP_OK;

        case AVP_AE_SONIC_CMD_GET_VOLUME:
            if (arg == NULL) {
                return AVP_EINVAL;
            }
            *(float *)arg = sonicGetVolume(handle->stream);
            return AVP_OK;

        case AVP_AE_SONIC_CMD_SET_CHORD_PITCH:
            if (arg == NULL) {
                return AVP_EINVAL;
            }
            value_i = *(int *)arg;
            if (value_i != 0 && value_i != 1) {
                return AVP_EINVAL;
            }
            sonicSetChordPitch(handle->stream, value_i);
            handle->config.use_chord_pitch = (uint8_t)value_i;
            return AVP_OK;

        case AVP_AE_SONIC_CMD_GET_CHORD_PITCH:
            if (arg == NULL) {
                return AVP_EINVAL;
            }
            *(int *)arg = sonicGetChordPitch(handle->stream);
            return AVP_OK;

        case AVP_AE_SONIC_CMD_SET_QUALITY:
            if (arg == NULL) {
                return AVP_EINVAL;
            }
            value_i = *(int *)arg;
            if (value_i != 0 && value_i != 1) {
                return AVP_EINVAL;
            }
            sonicSetQuality(handle->stream, value_i);
            handle->config.quality = (uint8_t)value_i;
            return AVP_OK;

        case AVP_AE_SONIC_CMD_GET_QUALITY:
            if (arg == NULL) {
                return AVP_EINVAL;
            }
            *(int *)arg = sonicGetQuality(handle->stream);
            return AVP_OK;

        case AVP_AE_SONIC_CMD_SET_SAMPLE_RATE:
            if (arg == NULL || *(uint32_t *)arg == 0u ||
                *(uint32_t *)arg > (uint32_t)INT32_MAX) {
                return AVP_EINVAL;
            }
            value_u32 = *(uint32_t *)arg;
            {
                avp_ae_sonic_config_t old_config = handle->config;
                avp_status_t ret;

                handle->config.sample_rate = value_u32;
                ret = avp_ae_sonic_recreate_stream(handle);
                if (ret != AVP_OK) {
                    handle->config = old_config;
                }
                return ret;
            }

        case AVP_AE_SONIC_CMD_GET_SAMPLE_RATE:
            if (arg == NULL) {
                return AVP_EINVAL;
            }
            *(uint32_t *)arg = (uint32_t)sonicGetSampleRate(handle->stream);
            return AVP_OK;

        case AVP_AE_SONIC_CMD_SET_CHANNELS:
            if (arg == NULL || !avp_ae_sonic_valid_channels(*(uint8_t *)arg)) {
                return AVP_EINVAL;
            }
            value_u8 = *(uint8_t *)arg;
            {
                avp_ae_sonic_config_t old_config = handle->config;
                avp_status_t ret;

                handle->config.channels = value_u8;
                ret = avp_ae_sonic_recreate_stream(handle);
                if (ret != AVP_OK) {
                    handle->config = old_config;
                }
                return ret;
            }

        case AVP_AE_SONIC_CMD_GET_CHANNELS:
            if (arg == NULL) {
                return AVP_EINVAL;
            }
            *(uint8_t *)arg = (uint8_t)sonicGetNumChannels(handle->stream);
            return AVP_OK;

        case AVP_AE_SONIC_CMD_FLUSH:
            return sonicFlushStream(handle->stream) ? AVP_OK : AVP_ENOMEM;

        case AVP_AE_SONIC_CMD_GET_AVAILABLE:
            if (arg == NULL) {
                return AVP_EINVAL;
            }
            *(uint32_t *)arg = (uint32_t)sonicSamplesAvailable(handle->stream);
            return AVP_OK;

        case AVP_AE_SONIC_CMD_RESET:
            return avp_ae_sonic_recreate_stream(handle);

        default:
            return AVP_EINVAL;
    }
}

avp_status_t avp_ae_sonic_process(avp_ae_sonic_t *handle,
                                  const int16_t *in,
                                  uint32_t in_samples,
                                  int16_t *out,
                                  uint32_t out_capacity,
                                  uint32_t *out_samples)
{
    int read_samples;

    if (handle == NULL ||
        (in_samples > 0u && in == NULL) ||
        (out_capacity > 0u && out == NULL)) {
        return AVP_EINVAL;
    }

    if (out_samples != NULL) {
        *out_samples = 0u;
    }

    if (in_samples > 0u) {
        if (!sonicWriteShortToStream(handle->stream, (short *)in,
                                     (int)in_samples)) {
            return AVP_ENOMEM;
        }
    }

    if (out_capacity == 0u) {
        return AVP_OK;
    }

    read_samples = sonicReadShortFromStream(handle->stream, (short *)out,
                                            (int)out_capacity);
    if (read_samples < 0) {
        return AVP_EINVAL;
    }

    if (out_samples != NULL) {
        *out_samples = (uint32_t)read_samples;
    }

    return AVP_OK;
}
