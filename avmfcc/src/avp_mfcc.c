/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "avp_mfcc.h"

#ifndef CONFIG_CHERRYAVP_MFCC_RFFT_OVERRIDE
#undef FIXED_POINT
#define FIXED_POINT 16
#include "kiss_fft.h"
#include "kiss_fftr.h"
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define AVP_MFCC_MIN_MEL_FREQ                  0.0f
#define AVP_MFCC_MIN_CHANNELS                  1u
#define AVP_MFCC_LOG_FLOOR                     1.0e-12f
#define AVP_MFCC_WINDOW_BITS                   12u
#define AVP_MFCC_FILTERBANK_BITS               12u
#define AVP_MFCC_FILTERBANK_INDEX_ALIGNMENT    4u
#define AVP_MFCC_FILTERBANK_CHANNEL_BLOCK_SIZE 4u

typedef struct {
    uint32_t fft_size;
    uint32_t fft_spectrum_size;
    uint32_t fft_power;
    uint32_t frame_length;
    uint32_t frame_step;
    uint32_t sample_rate;
    uint32_t num_coefficients;
    uint32_t num_channels;
    float lower_band_limit;
    float upper_band_limit;
} avp_mfcc_config_runtime_t;

struct avp_mfcc {
    avp_mfcc_config_runtime_t config;
#ifndef CONFIG_CHERRYAVP_MFCC_RFFT_OVERRIDE
    kiss_fftr_cfg fft_cfg;
    void *fft_mem;
    size_t fft_mem_size;
#endif
    int16_t *frame;
    int16_t *window_coefficients;
    int16_t *fft_input;
#ifndef CONFIG_CHERRYAVP_MFCC_RFFT_OVERRIDE
    kiss_fft_cpx *spectrum;
#endif
    uint32_t *energy;
    int16_t *channel_frequency_starts;
    int16_t *channel_weight_starts;
    int16_t *channel_widths;
    int16_t *filterbank_weights;
    int16_t *filterbank_unweights;
    uint64_t *filterbank_work;
    float *mel_energies;
    float *log_mel_energies;
    float *dct_matrix;
    uint32_t frame_pos;
};

static uint32_t avp_mfcc_round_u32(float value)
{
    if (value <= 0.0f) {
        return 0u;
    }
    return (uint32_t)(value + 0.5f);
}

static float avp_mfcc_mel_from_hz(float hz)
{
    return 1127.0f * log1pf(hz / 700.0f);
}

/* Calculate mel-spaced center frequencies. */
static void avp_calculate_center_frequencies(int num_channels,
                                             float lower_frequency_limit,
                                             float upper_frequency_limit,
                                             float *center_frequencies)
{
    float mel_low;
    float mel_hi;
    float mel_spacing;
    int i;

    if (num_channels <= 0 || center_frequencies == NULL) {
        return;
    }

    mel_low = avp_mfcc_mel_from_hz(lower_frequency_limit);
    mel_hi = avp_mfcc_mel_from_hz(upper_frequency_limit);
    mel_spacing = (mel_hi - mel_low) / (float)num_channels;

    for (i = 0; i < num_channels; ++i) {
        center_frequencies[i] = mel_low + (mel_spacing * (float)(i + 1));
    }
}

/* Convert one triangular filterbank ratio into Q12 weight/unweight values. */
static void avp_quantize_filterbank_weights(float float_weight,
                                            int16_t *weight,
                                            int16_t *unweight)
{
    *weight = (int16_t)floorf(float_weight *
                                  (float)(1u << AVP_MFCC_FILTERBANK_BITS) +
                              0.5f);
    *unweight = (int16_t)floorf((1.0f - float_weight) *
                                    (float)(1u << AVP_MFCC_FILTERBANK_BITS) +
                                0.5f);
}

static int16_t avp_mfcc_quantize_window(float value)
{
    if (value < 0.0f) {
        value = 0.0f;
    } else if (value > 1.0f) {
        value = 1.0f;
    }
    return (int16_t)floorf(value * (float)(1u << AVP_MFCC_WINDOW_BITS) + 0.5f);
}

static uint32_t avp_mfcc_next_pow2(uint32_t value, uint32_t *m)
{
    uint32_t power = 0u;
    uint32_t n = 1u;

    while (n < value && power < 30u) {
        n <<= 1;
        power++;
    }
    if (m != NULL) {
        *m = power;
    }
    return n;
}

static avp_status_t avp_mfcc_validate_config(const avp_mfcc_config_t *config)
{
    if (config == NULL || config->sample_rate == 0u ||
        config->window.size_ms == 0u || config->window.step_size_ms == 0u ||
        config->filterbank.num_channels < AVP_MFCC_MIN_CHANNELS ||
        config->filterbank.lower_band_limit < AVP_MFCC_MIN_MEL_FREQ ||
        config->filterbank.upper_band_limit <= config->filterbank.lower_band_limit ||
        config->filterbank.upper_band_limit > (float)config->sample_rate * 0.5f ||
        config->num_coefficients < AVP_MFCC_MIN_CHANNELS ||
        config->num_coefficients > AVP_MFCC_MAX_NUM_COEFFS) {
        return AVP_EINVAL;
    }
    return AVP_OK;
}

static avp_status_t avp_mfcc_build_window(avp_mfcc_t *ctx)
{
    uint32_t i;
    float arg;

    ctx->window_coefficients = (int16_t *)avp_calloc(
        ctx->config.frame_length, sizeof(*ctx->window_coefficients));
    if (ctx->window_coefficients == NULL) {
        return AVP_ENOMEM;
    }

    arg = 2.0f * (float)M_PI / (float)ctx->config.frame_length;
    for (i = 0u; i < ctx->config.frame_length; ++i) {
        float value = 0.5f - (0.5f * cosf(arg * ((float)i + 0.5f)));
        ctx->window_coefficients[i] = avp_mfcc_quantize_window(value);
    }
    return AVP_OK;
}

static avp_status_t avp_mfcc_build_filterbank(avp_mfcc_t *ctx, const avp_mfcc_config_t *config)
{
    uint32_t i;
    uint32_t num_channels_plus_1 = config->filterbank.num_channels + 1u;
    uint32_t index_alignment = AVP_MFCC_FILTERBANK_INDEX_ALIGNMENT / sizeof(int16_t);
    float *center_mel_freqs = NULL;
    int16_t *actual_channel_starts = NULL;
    int16_t *actual_channel_widths = NULL;
    uint32_t weight_index_start = 0u;
    int chan_freq_index_start;
    int needs_zeros = 0;

    if (index_alignment == 0u) {
        index_alignment = 1u;
    }

    ctx->channel_frequency_starts = (int16_t *)avp_calloc(
        num_channels_plus_1, sizeof(*ctx->channel_frequency_starts));
    ctx->channel_weight_starts = (int16_t *)avp_calloc(
        num_channels_plus_1, sizeof(*ctx->channel_weight_starts));
    ctx->channel_widths = (int16_t *)avp_calloc(
        num_channels_plus_1, sizeof(*ctx->channel_widths));
    ctx->filterbank_work = (uint64_t *)avp_calloc(
        num_channels_plus_1, sizeof(*ctx->filterbank_work));
    center_mel_freqs = (float *)avp_calloc(num_channels_plus_1, sizeof(*center_mel_freqs));
    actual_channel_starts = (int16_t *)avp_calloc(num_channels_plus_1, sizeof(*actual_channel_starts));
    actual_channel_widths = (int16_t *)avp_calloc(num_channels_plus_1, sizeof(*actual_channel_widths));

    if (ctx->channel_frequency_starts == NULL || ctx->channel_weight_starts == NULL ||
        ctx->channel_widths == NULL || ctx->filterbank_work == NULL ||
        center_mel_freqs == NULL || actual_channel_starts == NULL ||
        actual_channel_widths == NULL) {
        avp_free(center_mel_freqs);
        avp_free(actual_channel_starts);
        avp_free(actual_channel_widths);
        return AVP_ENOMEM;
    }

    avp_calculate_center_frequencies((int)num_channels_plus_1,
                                     config->filterbank.lower_band_limit,
                                     config->filterbank.upper_band_limit,
                                     center_mel_freqs);

    float hz_per_sbin = 0.5f * (float)config->sample_rate /
                        ((float)ctx->config.fft_spectrum_size - 1.0f);
    chan_freq_index_start = (int)(1.5f + config->filterbank.lower_band_limit / hz_per_sbin);
    for (i = 0u; i < num_channels_plus_1; ++i) {
        int freq_index = chan_freq_index_start;
        while (freq_index < (int)ctx->config.fft_spectrum_size &&
               avp_mfcc_mel_from_hz((float)freq_index * hz_per_sbin) <= center_mel_freqs[i]) {
            ++freq_index;
        }

        int width = freq_index - chan_freq_index_start;
        actual_channel_starts[i] = (int16_t)chan_freq_index_start;
        actual_channel_widths[i] = (int16_t)width;

        if (width == 0) {
            ctx->channel_frequency_starts[i] = 0;
            ctx->channel_weight_starts[i] = 0;
            ctx->channel_widths[i] = AVP_MFCC_FILTERBANK_CHANNEL_BLOCK_SIZE;
            if (!needs_zeros) {
                uint32_t j;
                needs_zeros = 1;
                for (j = 0u; j < i; ++j) {
                    ctx->channel_weight_starts[j] += AVP_MFCC_FILTERBANK_CHANNEL_BLOCK_SIZE;
                }
                weight_index_start += AVP_MFCC_FILTERBANK_CHANNEL_BLOCK_SIZE;
            }
        } else {
            int aligned_start = (chan_freq_index_start / (int)index_alignment) *
                                (int)index_alignment;
            int aligned_width = (chan_freq_index_start - aligned_start + width);
            int padded_width = (((aligned_width - 1) /
                                 AVP_MFCC_FILTERBANK_CHANNEL_BLOCK_SIZE) +
                                1) *
                               AVP_MFCC_FILTERBANK_CHANNEL_BLOCK_SIZE;

            ctx->channel_frequency_starts[i] = (int16_t)aligned_start;
            ctx->channel_weight_starts[i] = (int16_t)weight_index_start;
            ctx->channel_widths[i] = (int16_t)padded_width;
            weight_index_start += (uint32_t)padded_width;
        }

        chan_freq_index_start = freq_index;
    }

    ctx->filterbank_weights = (int16_t *)avp_calloc(
        weight_index_start, sizeof(*ctx->filterbank_weights));
    ctx->filterbank_unweights = (int16_t *)avp_calloc(
        weight_index_start, sizeof(*ctx->filterbank_unweights));
    if (ctx->filterbank_weights == NULL || ctx->filterbank_unweights == NULL) {
        avp_free(center_mel_freqs);
        avp_free(actual_channel_starts);
        avp_free(actual_channel_widths);
        return AVP_ENOMEM;
    }

    float mel_low = avp_mfcc_mel_from_hz(config->filterbank.lower_band_limit);
    for (i = 0u; i < num_channels_plus_1; ++i) {
        int frequency = actual_channel_starts[i];
        int num_frequencies = actual_channel_widths[i];
        int frequency_offset = frequency - ctx->channel_frequency_starts[i];
        int weight_start = ctx->channel_weight_starts[i];
        float denom_val = (i == 0u) ? mel_low : center_mel_freqs[i - 1u];
        int j;

        for (j = 0; j < num_frequencies; ++j, ++frequency) {
            float mel = avp_mfcc_mel_from_hz((float)frequency * hz_per_sbin);
            float weight = (center_mel_freqs[i] - mel) /
                           (center_mel_freqs[i] - denom_val);
            int weight_index = weight_start + frequency_offset + j;

            avp_quantize_filterbank_weights(weight,
                                            &ctx->filterbank_weights[weight_index],
                                            &ctx->filterbank_unweights[weight_index]);
        }
    }

    avp_free(center_mel_freqs);
    avp_free(actual_channel_starts);
    avp_free(actual_channel_widths);
    return AVP_OK;
}

static avp_status_t avp_mfcc_build_dct_matrix(avp_mfcc_t *ctx)
{
    uint32_t coeff;

    ctx->dct_matrix = (float *)avp_calloc(
        ctx->config.num_coefficients * ctx->config.num_channels,
        sizeof(*ctx->dct_matrix));
    if (ctx->dct_matrix == NULL) {
        return AVP_ENOMEM;
    }

    for (coeff = 0u; coeff < ctx->config.num_coefficients; ++coeff) {
        uint32_t ch;
        float scale = sqrtf(2.0f / (float)ctx->config.num_channels);
        if (coeff == 0u) {
            scale = sqrtf(1.0f / (float)ctx->config.num_channels);
        }
        for (ch = 0u; ch < ctx->config.num_channels; ++ch) {
            float angle = (float)M_PI * (float)coeff *
                          (((float)ch + 0.5f) / (float)ctx->config.num_channels);
            ctx->dct_matrix[coeff * ctx->config.num_channels + ch] =
                scale * cosf(angle);
        }
    }
    return AVP_OK;
}

static avp_status_t avp_mfcc_build_fft(avp_mfcc_t *ctx)
{
#ifndef CONFIG_CHERRYAVP_MFCC_RFFT_OVERRIDE
    ctx->fft_mem_size = 0u;
    kiss_fftr_alloc((int)ctx->config.fft_size, 0, NULL, &ctx->fft_mem_size);
    ctx->fft_mem = avp_calloc(1u, ctx->fft_mem_size);
    if (ctx->fft_mem == NULL) {
        return AVP_ENOMEM;
    }

    ctx->fft_cfg = kiss_fftr_alloc((int)ctx->config.fft_size, 0,
                                   ctx->fft_mem, &ctx->fft_mem_size);
    if (ctx->fft_cfg == NULL) {
        return AVP_ENOMEM;
    }
#endif

    ctx->fft_input = (int16_t *)avp_calloc(ctx->config.fft_size,
                                           sizeof(*ctx->fft_input));
#ifndef CONFIG_CHERRYAVP_MFCC_RFFT_OVERRIDE
    ctx->spectrum = (kiss_fft_cpx *)avp_calloc(ctx->config.fft_spectrum_size,
                                               sizeof(*ctx->spectrum));
#endif
    ctx->energy = (uint32_t *)avp_calloc(ctx->config.fft_spectrum_size,
                                         sizeof(*ctx->energy));
    if (ctx->fft_input == NULL || ctx->energy == NULL) {
        return AVP_ENOMEM;
    }
#ifndef CONFIG_CHERRYAVP_MFCC_RFFT_OVERRIDE
    if (ctx->spectrum == NULL) {
        return AVP_ENOMEM;
    }
#endif

    return AVP_OK;
}

static avp_status_t avp_mfcc_build_all(avp_mfcc_t *ctx,
                                       const avp_mfcc_config_t *config)
{
    uint32_t fft_power = 0u;
    uint32_t frame_length = avp_mfcc_round_u32(
        (float)config->sample_rate * config->window.size_ms / 1000.0f);
    uint32_t frame_step = avp_mfcc_round_u32(
        (float)config->sample_rate * config->window.step_size_ms / 1000.0f);
    uint32_t fft_size = avp_mfcc_next_pow2(frame_length, &fft_power);

    if (frame_length < 2u || frame_step == 0u || frame_step > frame_length ||
        fft_size < 2u) {
        return AVP_EINVAL;
    }

    ctx->config.sample_rate = config->sample_rate;
    ctx->config.lower_band_limit = config->filterbank.lower_band_limit;
    ctx->config.upper_band_limit = config->filterbank.upper_band_limit;
    ctx->config.num_channels = config->filterbank.num_channels;
    ctx->config.num_coefficients = config->num_coefficients;
    ctx->config.frame_length = frame_length;
    ctx->config.frame_step = frame_step;
    ctx->config.fft_size = fft_size;
    ctx->config.fft_spectrum_size = (fft_size / 2u) + 1u;
    ctx->config.fft_power = fft_power;

    ctx->frame = (int16_t *)avp_calloc(ctx->config.frame_length, sizeof(*ctx->frame));
    ctx->mel_energies = (float *)avp_calloc(ctx->config.num_channels,
                                            sizeof(*ctx->mel_energies));
    ctx->log_mel_energies = (float *)avp_calloc(ctx->config.num_channels,
                                                sizeof(*ctx->log_mel_energies));
    if (ctx->frame == NULL || ctx->mel_energies == NULL ||
        ctx->log_mel_energies == NULL) {
        return AVP_ENOMEM;
    }

    if (avp_mfcc_build_window(ctx) != AVP_OK ||
        avp_mfcc_build_fft(ctx) != AVP_OK ||
        avp_mfcc_build_filterbank(ctx, config) != AVP_OK ||
        avp_mfcc_build_dct_matrix(ctx) != AVP_OK) {
        return AVP_ENOMEM;
    }

    return AVP_OK;
}

static void avp_mfcc_destroy(avp_mfcc_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    avp_free(ctx->dct_matrix);
    avp_free(ctx->log_mel_energies);
    avp_free(ctx->mel_energies);
    avp_free(ctx->filterbank_unweights);
    avp_free(ctx->filterbank_weights);
    avp_free(ctx->filterbank_work);
    avp_free(ctx->channel_widths);
    avp_free(ctx->channel_weight_starts);
    avp_free(ctx->channel_frequency_starts);
    avp_free(ctx->energy);
#ifndef CONFIG_CHERRYAVP_MFCC_RFFT_OVERRIDE
    avp_free(ctx->spectrum);
#endif
    avp_free(ctx->fft_input);
    avp_free(ctx->window_coefficients);
    avp_free(ctx->frame);
#ifndef CONFIG_CHERRYAVP_MFCC_RFFT_OVERRIDE
    avp_free(ctx->fft_mem);
#endif
    avp_free(ctx);
}

static void avp_mfcc_apply_window(avp_mfcc_t *ctx)
{
    uint32_t i;

    for (i = 0u; i < ctx->config.frame_length; ++i) {
        int32_t value = ((int32_t)ctx->frame[i] * (int32_t)ctx->window_coefficients[i]) >>
                        AVP_MFCC_WINDOW_BITS;
        if (value > 32767) {
            value = 32767;
        } else if (value < -32768) {
            value = -32768;
        }
        ctx->fft_input[i] = (int16_t)value;
    }
    for (; i < ctx->config.fft_size; ++i) {
        ctx->fft_input[i] = 0;
    }
}

static avp_status_t avp_mfcc_preprocess_fft(avp_mfcc_t *ctx,
                                            int16_t *src,
                                            uint32_t m)
{
    uint32_t fft_size;

    if (m >= 31u) {
        return AVP_EINVAL;
    }
    fft_size = 1u << m;
    if (src == NULL || fft_size != ctx->config.fft_size) {
        return AVP_EINVAL;
    }

#ifdef CONFIG_CHERRYAVP_MFCC_RFFT_OVERRIDE
    return (avp_status_t)avp_mfcc_dsp_rfft_q15(src, m);
#else
    kiss_fftr(ctx->fft_cfg, src, ctx->spectrum);
    return AVP_OK;
#endif
}

static void avp_mfcc_compute_energy(avp_mfcc_t *ctx)
{
    uint32_t bin;

#ifdef CONFIG_CHERRYAVP_MFCC_RFFT_OVERRIDE
    uint32_t half_fft_size = ctx->config.fft_size / 2u;

    ctx->energy[0] = (uint32_t)((int64_t)ctx->fft_input[0] *
                                (int64_t)ctx->fft_input[0]);
    for (bin = 1u; bin < half_fft_size; ++bin) {
        int64_t real = (int64_t)ctx->fft_input[2u * bin];
        int64_t imag = (int64_t)ctx->fft_input[2u * bin + 1u];
        ctx->energy[bin] = (uint32_t)(real * real + imag * imag);
    }
    ctx->energy[half_fft_size] = (uint32_t)((int64_t)ctx->fft_input[half_fft_size] *
                                            (int64_t)ctx->fft_input[half_fft_size]);
#else
    for (bin = 0u; bin < ctx->config.fft_spectrum_size; ++bin) {
        int64_t real = (int64_t)ctx->spectrum[bin].r;
        int64_t imag = (int64_t)ctx->spectrum[bin].i;
        ctx->energy[bin] = (uint32_t)(real * real + imag * imag);
    }
#endif
}

static void avp_mfcc_apply_filterbank(avp_mfcc_t *ctx)
{
    uint64_t weight_accumulator = 0u;
    uint64_t unweight_accumulator = 0u;
    uint32_t ch;

    memset(ctx->filterbank_work, 0,
           (ctx->config.num_channels + 1u) * sizeof(*ctx->filterbank_work));

    for (ch = 0u; ch < ctx->config.num_channels + 1u; ++ch) {
        const uint32_t *magnitudes;
        const int16_t *weights;
        const int16_t *unweights;
        int width;
        int j;
        int start_index = ctx->channel_frequency_starts[ch];

        if (start_index < 0) {
            start_index = 0;
        }
        magnitudes = ctx->energy + (uint32_t)start_index;
        weights = ctx->filterbank_weights + ctx->channel_weight_starts[ch];
        unweights = ctx->filterbank_unweights + ctx->channel_weight_starts[ch];
        width = ctx->channel_widths[ch];

        for (j = 0; j < width; ++j) {
            uint64_t magnitude = *magnitudes++;
            weight_accumulator += (uint64_t)(*weights++) * magnitude;
            unweight_accumulator += (uint64_t)(*unweights++) * magnitude;
        }
        ctx->filterbank_work[ch] = weight_accumulator;
        weight_accumulator = unweight_accumulator;
        unweight_accumulator = 0u;
    }

    for (ch = 0u; ch < ctx->config.num_channels; ++ch) {
        ctx->mel_energies[ch] = (float)ctx->filterbank_work[ch + 1u];
    }
}

static void avp_mfcc_apply_log(avp_mfcc_t *ctx)
{
    uint32_t ch;

    for (ch = 0u; ch < ctx->config.num_channels; ++ch) {
        float value = ctx->mel_energies[ch];
        if (value < AVP_MFCC_LOG_FLOOR) {
            value = AVP_MFCC_LOG_FLOOR;
        }
        ctx->log_mel_energies[ch] = logf(value);
    }
}

static void avp_mfcc_apply_dct(avp_mfcc_t *ctx, avp_mfcc_frame_t *frame)
{
    uint32_t coeff;

    for (coeff = 0u; coeff < ctx->config.num_coefficients; ++coeff) {
        uint32_t ch;
        float sum = 0.0f;
        const float *row = &ctx->dct_matrix[coeff * ctx->config.num_channels];

        for (ch = 0u; ch < ctx->config.num_channels; ++ch) {
            sum += row[ch] * ctx->log_mel_energies[ch];
        }
        frame->mfcc_value[coeff] = sum;
    }
}

static void avp_mfcc_advance_frame(avp_mfcc_t *ctx)
{
    if (ctx->config.frame_step < ctx->config.frame_length) {
        uint32_t remain = ctx->config.frame_length - ctx->config.frame_step;
        memmove(ctx->frame, &ctx->frame[ctx->config.frame_step],
                remain * sizeof(*ctx->frame));
        ctx->frame_pos = remain;
    } else {
        ctx->frame_pos = 0u;
    }
}

static avp_status_t avp_mfcc_emit_frame(avp_mfcc_t *ctx,
                                        avp_mfcc_frame_t *frame)
{
    avp_status_t st;

    if (frame == NULL) {
        return AVP_EINVAL;
    }

    avp_mfcc_apply_window(ctx);
    st = avp_mfcc_preprocess_fft(ctx, ctx->fft_input, ctx->config.fft_power);
    if (st != AVP_OK) {
        return st;
    }
    avp_mfcc_compute_energy(ctx);
    avp_mfcc_apply_filterbank(ctx);
    avp_mfcc_apply_log(ctx);
    avp_mfcc_apply_dct(ctx, frame);
    frame->mfcc_size = ctx->config.num_coefficients;
    return AVP_OK;
}

avp_status_t avp_mfcc_open(const avp_mfcc_config_t *config,
                           avp_mfcc_t **handle)
{
    avp_mfcc_t *ctx;
    avp_status_t st;

    if (handle == NULL || avp_mfcc_validate_config(config) != AVP_OK) {
        return AVP_EINVAL;
    }

    ctx = (avp_mfcc_t *)avp_calloc(1u, sizeof(*ctx));
    if (ctx == NULL) {
        return AVP_ENOMEM;
    }

    st = avp_mfcc_build_all(ctx, config);
    if (st != AVP_OK) {
        avp_mfcc_destroy(ctx);
        return st;
    }

    *handle = ctx;
    return AVP_OK;
}

void avp_mfcc_close(avp_mfcc_t *handle)
{
    avp_mfcc_destroy(handle);
}

int avp_mfcc_process(avp_mfcc_t *handle,
                     const int16_t *input,
                     uint32_t sample_count,
                     uint32_t *sample_read,
                     avp_mfcc_frame_t *frame)
{
    uint32_t consumed = 0u;
    uint32_t step_samples;
    int emitted = 0;

    if (handle == NULL || input == NULL || frame == NULL || sample_read == NULL) {
        return AVP_EINVAL;
    }

    frame->mfcc_size = 0u;
    *sample_read = 0u;

    step_samples = avp_mfcc_get_step_samples(handle);
    if (sample_count > step_samples) {
        return AVP_EINVAL;
    }

    while (consumed < sample_count) {
        uint32_t copy_count;

        if (handle->frame_pos >= handle->config.frame_length) {
            break;
        }

        copy_count = handle->config.frame_length - handle->frame_pos;
        if (copy_count > (sample_count - consumed)) {
            copy_count = sample_count - consumed;
        }
        memcpy(&handle->frame[handle->frame_pos], &input[consumed],
               copy_count * sizeof(*handle->frame));
        handle->frame_pos += copy_count;
        consumed += copy_count;

        if (handle->frame_pos < handle->config.frame_length) {
            continue;
        }

        if (emitted == 0) {
            avp_status_t st = avp_mfcc_emit_frame(handle, frame);
            if (st != AVP_OK) {
                return st;
            }
            emitted = 1;
        }
        avp_mfcc_advance_frame(handle);
    }

    if (sample_read != NULL) {
        *sample_read = consumed;
    }

    if (handle->frame_pos < handle->config.frame_length) {
        return AVP_OK;
    }

    if (emitted == 0) {
        return avp_mfcc_emit_frame(handle, frame);
    }
    return AVP_OK;
}

uint32_t avp_mfcc_get_step_samples(const avp_mfcc_t *handle)
{
    return handle == NULL ? 0u : handle->config.frame_step;
}
