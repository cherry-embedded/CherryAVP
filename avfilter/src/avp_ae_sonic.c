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

avp_status_t avp_ae_sonic_open(const avp_ae_sonic_config_t *config,
                               avp_ae_sonic_t **handle)
{
    avp_ae_sonic_t *ctx;
    sonicStream stream;

    if (config == NULL || handle == NULL) {
        return AVP_EINVAL;
    }

    ctx = (avp_ae_sonic_t *)avp_calloc(1, sizeof(avp_ae_sonic_t));
    if (ctx == NULL) {
        return AVP_ENOMEM;
    }

    ctx->config = *config;
    stream = sonicCreateStream((int)ctx->config.sample_rate,
                               (int)ctx->config.channels);
    if (stream == NULL) {
        avp_free(ctx);
        return AVP_ENOMEM;
    }
    ctx->stream = stream;

    sonicSetSpeed(ctx->stream, ctx->config.speed);
    sonicSetPitch(ctx->stream, ctx->config.pitch);

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

    if (handle == NULL) {
        return AVP_EINVAL;
    }

    switch (cmd) {
        case AVP_AE_SONIC_CMD_SET_SPEED:
            if (arg == NULL) {
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
            if (arg == NULL) {
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

        case AVP_AE_SONIC_CMD_FLUSH:
            return sonicFlushStream(handle->stream) ? AVP_OK : AVP_ENOMEM;

        case AVP_AE_SONIC_CMD_GET_AVAILABLE:
            if (arg == NULL) {
                return AVP_EINVAL;
            }
            *(uint32_t *)arg = (uint32_t)sonicSamplesAvailable(handle->stream);
            return AVP_OK;

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
