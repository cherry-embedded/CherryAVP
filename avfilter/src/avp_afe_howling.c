/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "avp_afe_howling.h"

#include <float.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define AVP_AFE_HOWLING_MIN_FRAME_SAMPLES 64u
#define AVP_AFE_HOWLING_MAX_FRAME_SAMPLES 1024u
#define AVP_AFE_HOWLING_MIN_PAPR_TH       (-10.0f)
#define AVP_AFE_HOWLING_MAX_PAPR_TH       20.0f
#define AVP_AFE_HOWLING_MIN_PHPR_TH       0.0f
#define AVP_AFE_HOWLING_MAX_PHPR_TH       100.0f
#define AVP_AFE_HOWLING_MIN_PNPR_TH       0.0f
#define AVP_AFE_HOWLING_MAX_PNPR_TH       100.0f
#define AVP_AFE_HOWLING_MIN_Q             2.0f
#define AVP_AFE_HOWLING_MAX_Q             30.0f
#define AVP_AFE_HOWLING_HOLD_FRAMES       8u

typedef struct {
    float frequency;
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
    uint8_t active;
    uint8_t hold_frames;
} avp_afe_howling_notch_t;

struct avp_afe_howling {
    avp_afe_howling_config_t config;
    uint32_t frame_samples;
    float *analysis;
    float *window;
    float *fft;
    int16_t *fft_i16;
    uint32_t analysis_pos;
    uint8_t input_channel;
    float *notch_x1;
    float *notch_x2;
    float *notch_y1;
    float *notch_y2;
    avp_afe_howling_notch_t notches[AVP_AFE_HOWLING_MAX_NOTCHES];
};

static int avp_afe_howling_valid_bool(uint8_t value)
{
    return value == 0u || value == 1u;
}

static uint32_t avp_afe_howling_calc_frame_samples(uint32_t sample_rate)
{
    uint64_t target = (uint64_t)sample_rate * AVP_AFE_HOWLING_FRAME_MS / 1000u;
    uint32_t frame_samples = AVP_AFE_HOWLING_MIN_FRAME_SAMPLES;

    if (target < AVP_AFE_HOWLING_MIN_FRAME_SAMPLES) {
        target = AVP_AFE_HOWLING_MIN_FRAME_SAMPLES;
    } else if (target > AVP_AFE_HOWLING_MAX_FRAME_SAMPLES) {
        target = AVP_AFE_HOWLING_MAX_FRAME_SAMPLES;
    }
    while ((frame_samples << 1u) <= target) {
        frame_samples <<= 1u;
    }
    return frame_samples;
}

static uint32_t avp_afe_howling_bit_reverse(uint32_t value, uint32_t bits)
{
    uint32_t reversed = 0u;
    uint32_t bit;

    for (bit = 0u; bit < bits; bit++)
        reversed = (reversed << 1u) | ((value >> bit) & 1u);
    return reversed;
}

static int avp_afe_howling_valid_fft(const avp_afe_howling_fft_t *fft)
{
    if (fft->type == AVP_AFE_HOWLING_FFT_FLOAT)
        return fft->fft.f32 != NULL;
    return fft->type == AVP_AFE_HOWLING_FFT_INT16 && fft->fft.i16 != NULL;
}

static avp_status_t avp_afe_howling_validate(const avp_afe_howling_config_t *config)
{
    if (config == NULL || config->sample_rate == 0u ||
        config->channels == 0u ||
        config->max_notches == 0u ||
        config->max_notches > AVP_AFE_HOWLING_MAX_NOTCHES ||
        !isfinite(config->papr_th) ||
        config->papr_th < AVP_AFE_HOWLING_MIN_PAPR_TH ||
        config->papr_th > AVP_AFE_HOWLING_MAX_PAPR_TH ||
        !isfinite(config->phpr_th) ||
        config->phpr_th < AVP_AFE_HOWLING_MIN_PHPR_TH ||
        config->phpr_th > AVP_AFE_HOWLING_MAX_PHPR_TH ||
        !isfinite(config->pnpr_th) ||
        config->pnpr_th < AVP_AFE_HOWLING_MIN_PNPR_TH ||
        config->pnpr_th > AVP_AFE_HOWLING_MAX_PNPR_TH ||
        config->notch_q < AVP_AFE_HOWLING_MIN_Q ||
        config->notch_q > AVP_AFE_HOWLING_MAX_Q ||
        !avp_afe_howling_valid_bool(config->enable) ||
        ((config->fft.fft.f32 != NULL || config->fft.fft.i16 != NULL) &&
         !avp_afe_howling_valid_fft(&config->fft))) {
        return AVP_EINVAL;
    }
    return AVP_OK;
}

static void avp_afe_howling_set_notch(avp_afe_howling_notch_t *notch,
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
    notch->hold_frames = AVP_AFE_HOWLING_HOLD_FRAMES;
}

static void avp_afe_howling_clear_notch_states(avp_afe_howling_t *ctx,
                                               uint32_t slot)
{
    uint32_t channels = ctx->config.channels;
    uint32_t offset = slot * channels;

    memset(ctx->notch_x1 + offset, 0, channels * sizeof(float));
    memset(ctx->notch_x2 + offset, 0, channels * sizeof(float));
    memset(ctx->notch_y1 + offset, 0, channels * sizeof(float));
    memset(ctx->notch_y2 + offset, 0, channels * sizeof(float));
}

static void avp_afe_howling_reset_notch(avp_afe_howling_t *ctx, uint32_t slot)
{
    memset(&ctx->notches[slot], 0, sizeof(ctx->notches[slot]));
    avp_afe_howling_clear_notch_states(ctx, slot);
}

static void avp_afe_howling_fft_float(float *src, uint32_t m)
{
    uint32_t stage;
    uint32_t span;
    uint32_t bits;

    for (span = 1u, bits = 0u; span < m; span <<= 1u, bits++)
        ;
    for (uint32_t i = 0u; i < m; i++) {
        uint32_t reversed = avp_afe_howling_bit_reverse(i, bits);
        if (i < reversed) {
            float real = src[2u * i];
            float imag = src[2u * i + 1u];
            src[2u * i] = src[2u * reversed];
            src[2u * i + 1u] = src[2u * reversed + 1u];
            src[2u * reversed] = real;
            src[2u * reversed + 1u] = imag;
        }
    }

    for (span = 1u; span < m; span <<= 1u) {
        float step = (float)M_PI / (float)span;

        for (stage = 0u; stage < m; stage += span << 1u) {
            uint32_t k;
            for (k = 0u; k < span; k++) {
                uint32_t even = stage + k;
                uint32_t odd = even + span;
                float real = src[2u * odd];
                float imag = src[2u * odd + 1u];
                float twiddle_real = cosf((float)k * step);
                float twiddle_imag = -sinf((float)k * step);
                float product_real = real * twiddle_real - imag * twiddle_imag;
                float product_imag = real * twiddle_imag + imag * twiddle_real;

                src[2u * odd] = src[2u * even] - product_real;
                src[2u * odd + 1u] = src[2u * even + 1u] - product_imag;
                src[2u * even] += product_real;
                src[2u * even + 1u] += product_imag;
            }
        }
    }
}

static void avp_afe_howling_run_fft(avp_afe_howling_t *ctx)
{
    uint32_t n = ctx->frame_samples;
    uint32_t i;

    if (ctx->config.fft.type == AVP_AFE_HOWLING_FFT_FLOAT &&
        ctx->config.fft.fft.f32 != NULL) {
        ctx->config.fft.fft.f32(ctx->fft, n);
        return;
    }
    if (ctx->config.fft.type == AVP_AFE_HOWLING_FFT_INT16 &&
        ctx->config.fft.fft.i16 != NULL) {
        ctx->config.fft.fft.i16(ctx->fft_i16, n);
        for (i = 0u; i < n; i++) {
            ctx->fft[2u * i] = (float)ctx->fft_i16[2u * i];
            ctx->fft[2u * i + 1u] = (float)ctx->fft_i16[2u * i + 1u];
        }
        return;
    }
    avp_afe_howling_fft_float(ctx->fft, n);
}

static void avp_afe_howling_decay_notches(avp_afe_howling_t *ctx,
                                          const uint8_t *used)
{
    uint32_t i;

    for (i = 0u; i < ctx->config.max_notches; i++) {
        if (ctx->notches[i].active && !used[i]) {
            if (ctx->notches[i].hold_frames > 0u) {
                ctx->notches[i].hold_frames--;
            } else {
                avp_afe_howling_reset_notch(ctx, i);
            }
        }
    }
}

static float avp_afe_howling_frequency_at_bin(avp_afe_howling_t *ctx,
                                              uint32_t bin)
{
    uint32_t n = ctx->frame_samples;
    uint32_t max_bin = n / 2u;
    const uint32_t start_bin = 2u;
    float peak = fabsf(ctx->analysis[bin]);
    float left = peak;
    float right = peak;
    float denominator;
    float delta = 0.0f;

    if (bin > start_bin) {
        left = fabsf(ctx->analysis[bin - 1u]);
    }
    if (bin + 1u < max_bin) {
        right = fabsf(ctx->analysis[bin + 1u]);
    }

    denominator = left - 2.0f * peak + right;
    if (fabsf(denominator) > FLT_MIN) {
        delta = 0.5f * (left - right) / denominator;
    }
    if (delta > 1.0f) {
        delta = 1.0f;
    } else if (delta < -1.0f) {
        delta = -1.0f;
    }

    return ((float)bin + delta) * (float)ctx->config.sample_rate / (float)n;
}

static float avp_afe_howling_harmonic_power(avp_afe_howling_t *ctx,
                                            uint32_t bin)
{
    uint32_t max_bin = ctx->frame_samples / 2u;
    uint32_t harmonic;
    float harmonic_power = 0.0f;

    for (harmonic = 2u; harmonic <= 4u; harmonic++) {
        uint32_t harmonic_bin = bin * harmonic;

        if (harmonic_bin < max_bin) {
            float power = fabsf(ctx->analysis[harmonic_bin]);
            if (power > harmonic_power) {
                harmonic_power = power;
            }
        }
    }
    return harmonic_power;
}

static float avp_afe_howling_neighbor_power(avp_afe_howling_t *ctx,
                                            uint32_t bin)
{
    uint32_t max_bin = ctx->frame_samples / 2u;
    const uint32_t start_bin = 2u;
    float neighbor_power = 0.0f;

    if (bin > start_bin + 1u && bin - 2u >= start_bin) {
        neighbor_power = fabsf(ctx->analysis[bin - 2u]);
    }
    if (bin + 2u < max_bin) {
        float power = fabsf(ctx->analysis[bin + 2u]);
        if (power > neighbor_power) {
            neighbor_power = power;
        }
    }
    return neighbor_power;
}

static uint8_t avp_afe_howling_assign_notch(avp_afe_howling_t *ctx,
                                            float frequency,
                                            uint8_t *used)
{
    float spacing = 2.0f * (float)ctx->config.sample_rate /
                    (float)ctx->frame_samples;
    uint32_t slot;

    for (slot = 0u; slot < ctx->config.max_notches; slot++) {
        if (ctx->notches[slot].active &&
            fabsf(ctx->notches[slot].frequency - frequency) < spacing) {
            avp_afe_howling_set_notch(&ctx->notches[slot], frequency,
                                      (float)ctx->config.sample_rate,
                                      ctx->config.notch_q);
            used[slot] = 1u;
            return 1u;
        }
    }

    for (slot = 0u; slot < ctx->config.max_notches; slot++) {
        if (!ctx->notches[slot].active ||
            ctx->notches[slot].hold_frames == 0u) {
            avp_afe_howling_set_notch(&ctx->notches[slot], frequency,
                                      (float)ctx->config.sample_rate,
                                      ctx->config.notch_q);
            used[slot] = 1u;
            return 1u;
        }
    }
    return 0u;
}

static void avp_afe_howling_analyze(avp_afe_howling_t *ctx)
{
    uint32_t n = ctx->frame_samples;
    uint32_t max_bin = n / 2u;
    const uint32_t start_bin = 2u;
    uint8_t used[AVP_AFE_HOWLING_MAX_NOTCHES] = { 0 };
    uint8_t found = 0u;
    uint32_t i;
    float average_power = 0.0f;
    float global_peak_power = 0.0f;
    float papr;

    for (i = 0u; i < n; i++) {
        float windowed = ctx->analysis[i] * ctx->window[i];
        ctx->fft[2u * i] = windowed;
        ctx->fft[2u * i + 1u] = 0.0f;
    }

    if (ctx->config.fft.type == AVP_AFE_HOWLING_FFT_INT16 &&
        ctx->config.fft.fft.i16 != NULL) {
        memset(ctx->fft_i16, 0, 2u * n * sizeof(ctx->fft_i16[0]));
        for (i = 0u; i < n; i++) {
            ctx->fft_i16[2u * i] = (int16_t)ctx->fft[2u * i];
        }
    }

    avp_afe_howling_run_fft(ctx);

    for (i = start_bin; i < max_bin; i++) {
        float real = ctx->fft[2u * i];
        float imag = ctx->fft[2u * i + 1u];
        float power = real * real + imag * imag;

        ctx->analysis[i] = power;
        average_power += power;
        if (power > global_peak_power) {
            global_peak_power = power;
        }
    }

    if (global_peak_power <= 0.0f) {
        avp_afe_howling_decay_notches(ctx, used);
        return;
    }

    average_power /= (float)(max_bin - start_bin);
    papr = 10.0f * log10f(global_peak_power / average_power);
    if (papr < ctx->config.papr_th) {
        avp_afe_howling_decay_notches(ctx, used);
        return;
    }

    while (found < ctx->config.max_notches) {
        uint32_t candidate_bin = max_bin;
        float candidate_power = 0.0f;
        float harmonic_power;
        float neighbor_power;
        float frequency;
        float phpr;
        float pnpr;
        uint32_t slot;

        for (i = start_bin; i < max_bin; i++) {
            float power = ctx->analysis[i];
            if (power > candidate_power) {
                candidate_power = power;
                candidate_bin = i;
            }
        }
        if (candidate_bin >= max_bin) {
            break;
        }

        ctx->analysis[candidate_bin] = -candidate_power;
        frequency = avp_afe_howling_frequency_at_bin(ctx, candidate_bin);

        for (slot = 0u; slot < ctx->config.max_notches; slot++) {
            float spacing = 2.0f * (float)ctx->config.sample_rate / (float)n;
            if (used[slot] &&
                fabsf(ctx->notches[slot].frequency - frequency) < spacing) {
                break;
            }
        }
        if (slot < ctx->config.max_notches) {
            continue;
        }

        harmonic_power = avp_afe_howling_harmonic_power(ctx, candidate_bin);
        phpr = harmonic_power > 0.0f ? 10.0f * log10f(candidate_power / harmonic_power) : AVP_AFE_HOWLING_MAX_PHPR_TH;
        if (phpr < ctx->config.phpr_th) {
            continue;
        }

        neighbor_power = avp_afe_howling_neighbor_power(ctx, candidate_bin);
        pnpr = neighbor_power > 0.0f ? 10.0f * log10f(candidate_power / neighbor_power) : AVP_AFE_HOWLING_MAX_PNPR_TH;
        if (pnpr < ctx->config.pnpr_th) {
            continue;
        }

        if (avp_afe_howling_assign_notch(ctx, frequency, used)) {
            found++;
        }
    }

    avp_afe_howling_decay_notches(ctx, used);
}

static float avp_afe_howling_filter_sample(avp_afe_howling_t *ctx,
                                           uint32_t channel,
                                           float sample)
{
    uint32_t i;
    float value = sample;

    if (ctx->config.enable == 0u) {
        return value;
    }
    for (i = 0u; i < ctx->config.max_notches; i++) {
        avp_afe_howling_notch_t *notch = &ctx->notches[i];
        uint32_t state_index = i * ctx->config.channels + channel;
        float output;
        if (!notch->active) {
            continue;
        }
        output = notch->b0 * value + notch->b1 * ctx->notch_x1[state_index] +
                 notch->b2 * ctx->notch_x2[state_index] -
                 notch->a1 * ctx->notch_y1[state_index] -
                 notch->a2 * ctx->notch_y2[state_index];
        ctx->notch_x2[state_index] = ctx->notch_x1[state_index];
        ctx->notch_x1[state_index] = value;
        ctx->notch_y2[state_index] = ctx->notch_y1[state_index];
        ctx->notch_y1[state_index] = output;
        value = output;
    }
    return value;
}

avp_status_t avp_afe_howling_open(const avp_afe_howling_config_t *config,
                                  avp_afe_howling_t **handle)
{
    avp_afe_howling_t *ctx;
    avp_afe_howling_config_t local;
    uint32_t i;
    uint32_t state_count;

    if (config == NULL || handle == NULL) {
        return AVP_EINVAL;
    }

    local = *config;
    if (local.max_notches == 0u) {
        local.max_notches = AVP_AFE_HOWLING_MAX_NOTCHES;
    }
    if (local.notch_q == 0.0f) {
        local.notch_q = 12.0f;
    }
    if (local.fft.fft.f32 == NULL && local.fft.fft.i16 == NULL) {
        local.fft.type = AVP_AFE_HOWLING_FFT_FLOAT;
    }
    if (avp_afe_howling_validate(&local) != AVP_OK) {
        return AVP_EINVAL;
    }

    ctx = (avp_afe_howling_t *)avp_calloc(1u, sizeof(*ctx));
    if (ctx == NULL) {
        return AVP_ENOMEM;
    }
    ctx->frame_samples = avp_afe_howling_calc_frame_samples(local.sample_rate);

    ctx->analysis = (float *)avp_calloc(ctx->frame_samples, sizeof(float));
    if (ctx->analysis == NULL) {
        goto error;
    }
    ctx->window = (float *)avp_calloc(ctx->frame_samples, sizeof(float));
    if (ctx->window == NULL) {
        goto error;
    }
    for (i = 0u; i < ctx->frame_samples; i++) {
        ctx->window[i] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI *
                                             (float)i /
                                             (float)ctx->frame_samples));
    }
    ctx->fft = (float *)avp_calloc(2u * ctx->frame_samples, sizeof(float));
    if (ctx->fft == NULL) {
        goto error;
    }
    state_count = (uint32_t)AVP_AFE_HOWLING_MAX_NOTCHES * local.channels;
    ctx->notch_x1 = (float *)avp_calloc(state_count, sizeof(float));
    if (ctx->notch_x1 == NULL) {
        goto error;
    }
    ctx->notch_x2 = (float *)avp_calloc(state_count, sizeof(float));
    if (ctx->notch_x2 == NULL) {
        goto error;
    }
    ctx->notch_y1 = (float *)avp_calloc(state_count, sizeof(float));
    if (ctx->notch_y1 == NULL) {
        goto error;
    }
    ctx->notch_y2 = (float *)avp_calloc(state_count, sizeof(float));
    if (ctx->notch_y2 == NULL) {
        goto error;
    }
    if (local.fft.type == AVP_AFE_HOWLING_FFT_INT16 &&
        local.fft.fft.i16 != NULL) {
        ctx->fft_i16 = (int16_t *)avp_calloc(2u * ctx->frame_samples, sizeof(int16_t));
        if (ctx->fft_i16 == NULL) {
            goto error;
        }
    }

    ctx->config = local;
    *handle = ctx;
    return AVP_OK;

error:
    avp_free(ctx->notch_y2);
    avp_free(ctx->notch_y1);
    avp_free(ctx->notch_x2);
    avp_free(ctx->notch_x1);
    avp_free(ctx->fft_i16);
    avp_free(ctx->fft);
    avp_free(ctx->window);
    avp_free(ctx->analysis);
    avp_free(ctx);
    return AVP_ENOMEM;
}

void avp_afe_howling_close(avp_afe_howling_t *handle)
{
    if (handle == NULL) {
        return;
    }
    avp_free(handle->analysis);
    avp_free(handle->window);
    avp_free(handle->fft);
    avp_free(handle->fft_i16);
    avp_free(handle->notch_y2);
    avp_free(handle->notch_y1);
    avp_free(handle->notch_x2);
    avp_free(handle->notch_x1);
    avp_free(handle);
}

avp_status_t avp_afe_howling_control(avp_afe_howling_t *handle,
                                     avp_afe_howling_cmd_t cmd,
                                     void *arg)
{
    if (handle == NULL) {
        return AVP_EINVAL;
    }
    switch (cmd) {
        case AVP_AFE_HOWLING_CMD_SET_ENABLE:
            if (arg == NULL || (*(int *)arg != 0 && *(int *)arg != 1))
                return AVP_EINVAL;
            handle->config.enable = (uint8_t) * (int *)arg;
            return AVP_OK;
        case AVP_AFE_HOWLING_CMD_GET_ENABLE:
            if (arg == NULL)
                return AVP_EINVAL;
            *(int *)arg = handle->config.enable;
            return AVP_OK;
        case AVP_AFE_HOWLING_CMD_SET_PAPR_TH:
            if (arg == NULL || !isfinite(*(float *)arg) ||
                *(float *)arg < AVP_AFE_HOWLING_MIN_PAPR_TH ||
                *(float *)arg > AVP_AFE_HOWLING_MAX_PAPR_TH)
                return AVP_EINVAL;
            handle->config.papr_th = *(float *)arg;
            return AVP_OK;
        case AVP_AFE_HOWLING_CMD_GET_PAPR_TH:
            if (arg == NULL)
                return AVP_EINVAL;
            *(float *)arg = handle->config.papr_th;
            return AVP_OK;
        case AVP_AFE_HOWLING_CMD_SET_PHPR_TH:
            if (arg == NULL || !isfinite(*(float *)arg) ||
                *(float *)arg < AVP_AFE_HOWLING_MIN_PHPR_TH ||
                *(float *)arg > AVP_AFE_HOWLING_MAX_PHPR_TH)
                return AVP_EINVAL;
            handle->config.phpr_th = *(float *)arg;
            return AVP_OK;
        case AVP_AFE_HOWLING_CMD_GET_PHPR_TH:
            if (arg == NULL)
                return AVP_EINVAL;
            *(float *)arg = handle->config.phpr_th;
            return AVP_OK;
        case AVP_AFE_HOWLING_CMD_SET_PNPR_TH:
            if (arg == NULL || !isfinite(*(float *)arg) ||
                *(float *)arg < AVP_AFE_HOWLING_MIN_PNPR_TH ||
                *(float *)arg > AVP_AFE_HOWLING_MAX_PNPR_TH)
                return AVP_EINVAL;
            handle->config.pnpr_th = *(float *)arg;
            return AVP_OK;
        case AVP_AFE_HOWLING_CMD_GET_PNPR_TH:
            if (arg == NULL)
                return AVP_EINVAL;
            *(float *)arg = handle->config.pnpr_th;
            return AVP_OK;
        case AVP_AFE_HOWLING_CMD_SET_NOTCH_Q:
            if (arg == NULL || *(float *)arg < AVP_AFE_HOWLING_MIN_Q ||
                *(float *)arg > AVP_AFE_HOWLING_MAX_Q)
                return AVP_EINVAL;
            handle->config.notch_q = *(float *)arg;

            {
                uint32_t i;
                for (i = 0u; i < handle->config.max_notches; i++) {
                    if (handle->notches[i].active) {
                        avp_afe_howling_set_notch(&handle->notches[i],
                                                  handle->notches[i].frequency,
                                                  (float)handle->config.sample_rate,
                                                  handle->config.notch_q);
                    }
                }
            }
            return AVP_OK;
        case AVP_AFE_HOWLING_CMD_GET_NOTCH_Q:
            if (arg == NULL)
                return AVP_EINVAL;
            *(float *)arg = handle->config.notch_q;
            return AVP_OK;
        case AVP_AFE_HOWLING_CMD_SET_MAX_NOTCHES:
            if (arg == NULL || *(uint8_t *)arg == 0u ||
                *(uint8_t *)arg > AVP_AFE_HOWLING_MAX_NOTCHES)
                return AVP_EINVAL;
            handle->config.max_notches = *(uint8_t *)arg;

            {
                uint32_t i;
                for (i = handle->config.max_notches; i < AVP_AFE_HOWLING_MAX_NOTCHES; i++) {
                    avp_afe_howling_reset_notch(handle, i);
                }
            }
            return AVP_OK;
        case AVP_AFE_HOWLING_CMD_GET_MAX_NOTCHES:
            if (arg == NULL)
                return AVP_EINVAL;
            *(uint8_t *)arg = handle->config.max_notches;
            return AVP_OK;
        case AVP_AFE_HOWLING_CMD_GET_ACTIVE_NOTCHES:
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
        case AVP_AFE_HOWLING_CMD_RESET:
            memset(handle->analysis, 0, handle->frame_samples * sizeof(float));
            memset(handle->fft, 0, 2u * handle->frame_samples * sizeof(float));
            if (handle->fft_i16 != NULL)
                memset(handle->fft_i16, 0, 2u * handle->frame_samples * sizeof(int16_t));
            handle->analysis_pos = 0u;
            handle->input_channel = 0u;
            {
                uint32_t i;
                for (i = 0u; i < AVP_AFE_HOWLING_MAX_NOTCHES; i++) {
                    avp_afe_howling_reset_notch(handle, i);
                }
            }
            return AVP_OK;
        default:
            return AVP_EINVAL;
    }
}

avp_status_t avp_afe_howling_process(avp_afe_howling_t *handle,
                                     const int16_t *in,
                                     int16_t *out,
                                     uint32_t sample_count)
{
    uint32_t i;
    uint32_t pcm_samples;
    float downmix_scale;

    if (handle == NULL || in == NULL || out == NULL)
        return AVP_EINVAL;
    pcm_samples = handle->frame_samples * handle->config.channels;
    if (sample_count != pcm_samples)
        return AVP_EINVAL;
    downmix_scale = 1.0f / (float)handle->config.channels;

    for (i = 0u; i < sample_count; i++) {
        uint32_t channel = handle->input_channel;
        float input = (float)in[i];
        float filtered = avp_afe_howling_filter_sample(handle, channel, input);
        uint32_t frame_index = handle->analysis_pos;

        if (channel == 0u) {
            handle->analysis[frame_index] = input * downmix_scale;
        } else {
            handle->analysis[frame_index] += input * downmix_scale;
        }

        out[i] = (int16_t)(filtered > 32767.0f  ? 32767.0f :
                           filtered < -32768.0f ? -32768.0f :
                                                  filtered);
        handle->input_channel++;
        if (handle->input_channel == handle->config.channels) {
            handle->input_channel = 0u;
            handle->analysis_pos++;
            if (handle->analysis_pos == handle->frame_samples) {
                avp_afe_howling_analyze(handle);
                handle->analysis_pos = 0u;
            }
        }
    }
    return AVP_OK;
}

uint32_t avp_afe_howling_get_frame_samples(const avp_afe_howling_t *handle)
{
    return handle == NULL ? 0u : handle->frame_samples;
}
