/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "avp_ae_vol.h"

#define AVP_AE_VOL_Q14_SHIFT 14
#define AVP_AE_VOL_Q14_ONE   (1 << AVP_AE_VOL_Q14_SHIFT)
#define AVP_AE_VOL_MIN_DB    (-120)
#define AVP_AE_VOL_MAX_DB    60

struct avp_ae_vol {
    avp_ae_vol_config_t config;
    int32_t factor_q14[AVP_AE_VOL_STEPS];
};

static int avp_ae_vol_valid_bool(uint8_t value)
{
    return value == 0u || value == 1u;
}

static avp_status_t avp_ae_vol_validate_range(int min_db, int max_db)
{
    if (min_db < AVP_AE_VOL_MIN_DB ||
        max_db > AVP_AE_VOL_MAX_DB ||
        min_db > max_db) {
        return AVP_EINVAL;
    }

    return AVP_OK;
}

static avp_status_t avp_ae_vol_build_table(avp_ae_vol_t *ctx)
{
    uint32_t i;

    if (avp_ae_vol_validate_range(ctx->config.min_db,
                                  ctx->config.max_db) != AVP_OK) {
        return AVP_EINVAL;
    }

    ctx->factor_q14[0] = 0;
    for (i = 1u; i < AVP_AE_VOL_STEPS; i++) {
        double db;
        double gain;
        double scaled;

        db = (double)ctx->config.min_db +
             ((double)(ctx->config.max_db - ctx->config.min_db) *
              (double)i / (double)(AVP_AE_VOL_STEPS - 1u));
        gain = pow(10.0, db / 20.0);
        scaled = gain * (double)AVP_AE_VOL_Q14_ONE;
        if (scaled > (double)INT32_MAX) {
            return AVP_ERANGE;
        }
        ctx->factor_q14[i] = (int32_t)(scaled + 0.5);
    }

    return AVP_OK;
}

static void avp_ae_vol_apply_default_config(avp_ae_vol_config_t *config)
{
    if (config->min_db == 0 && config->max_db == 0) {
        config->min_db = AVP_AE_VOL_DEFAULT_MIN_DB;
        config->max_db = AVP_AE_VOL_DEFAULT_MAX_DB;
    }
}

avp_status_t avp_ae_vol_open(const avp_ae_vol_config_t *config,
                             avp_ae_vol_t **handle)
{
    avp_ae_vol_t *ctx;
    avp_ae_vol_config_t local_config;
    avp_status_t st;

    if (config == NULL || handle == NULL) {
        return AVP_EINVAL;
    }

    local_config = *config;
    avp_ae_vol_apply_default_config(&local_config);
    if (!avp_ae_vol_valid_bool(local_config.enable)) {
        return AVP_EINVAL;
    }

    ctx = (avp_ae_vol_t *)avp_calloc(1u, sizeof(avp_ae_vol_t));
    if (ctx == NULL) {
        return AVP_ENOMEM;
    }

    ctx->config = local_config;
    st = avp_ae_vol_build_table(ctx);
    if (st != AVP_OK) {
        avp_free(ctx);
        return st;
    }

    *handle = ctx;
    return AVP_OK;
}

void avp_ae_vol_close(avp_ae_vol_t *handle)
{
    if (handle == NULL) {
        return;
    }

    avp_free(handle);
}

avp_status_t avp_ae_vol_control(avp_ae_vol_t *handle,
                                avp_ae_vol_cmd_t cmd,
                                void *arg)
{
    if (handle == NULL) {
        return AVP_EINVAL;
    }

    switch (cmd) {
        case AVP_AE_VOL_CMD_SET_INDEX:
            if (arg == NULL) {
                return AVP_EINVAL;
            }
            handle->config.index = *(uint8_t *)arg;
            return AVP_OK;

        case AVP_AE_VOL_CMD_GET_INDEX:
            if (arg == NULL) {
                return AVP_EINVAL;
            }
            *(uint8_t *)arg = handle->config.index;
            return AVP_OK;

        case AVP_AE_VOL_CMD_SET_DB_RANGE:
            if (arg == NULL) {
                return AVP_EINVAL;
            }
            {
                avp_ae_vol_db_range_t *range = (avp_ae_vol_db_range_t *)arg;
                avp_ae_vol_config_t old_config = handle->config;
                avp_status_t st;

                handle->config.min_db = range->min_db;
                handle->config.max_db = range->max_db;
                st = avp_ae_vol_build_table(handle);
                if (st != AVP_OK) {
                    handle->config = old_config;
                    (void)avp_ae_vol_build_table(handle);
                }
                return st;
            }

        case AVP_AE_VOL_CMD_GET_DB_RANGE:
            if (arg == NULL) {
                return AVP_EINVAL;
            }
            ((avp_ae_vol_db_range_t *)arg)->min_db = handle->config.min_db;
            ((avp_ae_vol_db_range_t *)arg)->max_db = handle->config.max_db;
            return AVP_OK;

        case AVP_AE_VOL_CMD_SET_ENABLE:
            if (arg == NULL || (*(int *)arg != 0 && *(int *)arg != 1)) {
                return AVP_EINVAL;
            }
            handle->config.enable = (uint8_t) * (int *)arg;
            return AVP_OK;

        case AVP_AE_VOL_CMD_GET_ENABLE:
            if (arg == NULL) {
                return AVP_EINVAL;
            }
            *(int *)arg = handle->config.enable;
            return AVP_OK;

        case AVP_AE_VOL_CMD_GET_GAIN_Q14:
            if (arg == NULL) {
                return AVP_EINVAL;
            }
            *(int32_t *)arg = handle->factor_q14[handle->config.index];
            return AVP_OK;

        default:
            return AVP_EINVAL;
    }
}

avp_status_t avp_ae_vol_process(avp_ae_vol_t *handle,
                                const int16_t *in,
                                int16_t *out,
                                uint32_t sample_count)
{
    uint32_t i;
    int32_t factor;

    if (handle == NULL ||
        (sample_count > 0u && (in == NULL || out == NULL))) {
        return AVP_EINVAL;
    }

    if (sample_count == 0u) {
        return AVP_OK;
    }

    if (handle->config.enable == 0u) {
        if (out != in) {
            memcpy(out, in, (size_t)sample_count * sizeof(int16_t));
        }
        return AVP_OK;
    }

    factor = handle->factor_q14[handle->config.index];
    if (factor > AVP_AE_VOL_Q14_ONE) {
        for (i = 0u; i < sample_count; i++) {
            int64_t value;

            value = ((int64_t)in[i] * factor) >> AVP_AE_VOL_Q14_SHIFT;
            if (value > INT16_MAX) {
                value = INT16_MAX;
            } else if (value < INT16_MIN) {
                value = INT16_MIN;
            }
            out[i] = (int16_t)value;
        }
    } else {
        for (i = 0u; i < sample_count; i++) {
            out[i] = (int16_t)(((int32_t)in[i] * factor) >>
                               AVP_AE_VOL_Q14_SHIFT);
        }
    }

    return AVP_OK;
}
