/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "avp_swr.h"

#define AVP_SWR_Q15_SHIFT 15u
#define AVP_SWR_Q15_ONE   32767

typedef struct {
    uint32_t src_sample_rate;
    uint32_t dst_sample_rate;
    uint32_t num_rate;
    uint32_t den_rate;
    uint16_t filt_len;
    uint16_t oversample;
    const int16_t *sinc_table;
} avp_swr_sinc_table_t;

/*
 * avp_swr_sinc_tables stores precomputed rate-pair-specific FIR kernels.
 * The numbers are generated offline by sampling a Hann-windowed sinc and
 * quantizing the result to Q15 int16_t.
 *
 * Reference generator:
 *
 * static double avp_swr_table_cutoff(uint32_t src_rate, uint32_t dst_rate)
 * {
 *     if (dst_rate >= src_rate) {
 *         return 0.88;
 *     }
 *     return 0.85 * (double)dst_rate / (double)src_rate;
 * }
 *
 * static int16_t avp_swr_make_sinc_q15(uint32_t i,
 *                                      uint16_t filt_len,
 *                                      uint16_t oversample,
 *                                      double cutoff)
 * {
 *     double x = (double)i / (double)oversample - (double)filt_len / 2.0;
 *     double win = 0.5 * (1.0 + cos(M_PI * fabs(2.0 * x / (double)filt_len)));
 *     double sinc;
 *     double q15;
 *
 *     if (fabs(x) < 1e-12) {
 *         sinc = cutoff;
 *     } else {
 *         sinc = cutoff * sin(M_PI * cutoff * x) / (M_PI * cutoff * x);
 *     }
 *
 *     q15 = sinc * win * 32768.0;
 *     if (q15 > 32767.0) {
 *         q15 = 32767.0;
 *     } else if (q15 < -32768.0) {
 *         q15 = -32768.0;
 *     }
 *     return (int16_t)lrint(q15);
 * }
 *
 * static void avp_swr_make_sinc_table(int16_t *dst,
 *                                     uint16_t filt_len,
 *                                     uint16_t oversample,
 *                                     double cutoff)
 * {
 *     uint32_t i;
 *
 *     dst[0] = dst[1] = dst[2] = dst[3] = 0;
 *     for (i = 0; i < (uint32_t)filt_len * oversample; i++) {
 *         dst[4 + i] = avp_swr_make_sinc_q15(i, filt_len, oversample, cutoff);
 *     }
 *     dst[4 + (uint32_t)filt_len * oversample + 0] = 0;
 *     dst[4 + (uint32_t)filt_len * oversample + 1] = 0;
 *     dst[4 + (uint32_t)filt_len * oversample + 2] = 0;
 *     dst[4 + (uint32_t)filt_len * oversample + 3] = 0;
 * }
 *
 * Layout note:
 *   [0..3] and the last four entries are guard zeros.
 *   Runtime code uses cubic interpolation on four adjacent entries.
 */
#include "avp_swr_sinc_tables.inc"

static inline int avp_swr_check_sample_params(uint8_t channels, uint16_t bits_per_sample)
{
    return channels != 0u &&
           avp_swr_get_sample_bytes(bits_per_sample) != 0u;
}

static inline int avp_swr_check_planar_buffer(avp_sw_sample_t data[], uint8_t channels)
{
    uint8_t ch;

    if (data == NULL || channels == 0u) {
        return 0;
    }

    for (ch = 0u; ch < channels; ch++) {
        if (data[ch] == NULL) {
            return 0;
        }
    }

    return 1;
}

static inline int32_t avp_clip_s32(int64_t sample)
{
    if (sample > INT32_MAX) {
        return INT32_MAX;
    }
    if (sample < INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)sample;
}

static inline int16_t avp_clip_s16(int32_t sample)
{
    if (sample > INT16_MAX) {
        return INT16_MAX;
    }
    if (sample < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t)sample;
}

static inline uint8_t avp_clip_u8(int32_t sample)
{
    if (sample > UINT8_MAX) {
        return UINT8_MAX;
    }
    if (sample < 0) {
        return 0u;
    }
    return (uint8_t)sample;
}

static inline const uint8_t *avp_swr_get_interleaved_sample_ptr(avp_sw_sample_t data,
                                                                uint32_t sample_idx,
                                                                uint8_t ch,
                                                                uint8_t channels,
                                                                uint16_t bits_per_sample)
{
    uint8_t bytes = avp_swr_get_sample_bytes(bits_per_sample);

    return (const uint8_t *)data + ((size_t)sample_idx * channels + ch) * bytes;
}

static inline const uint8_t *avp_swr_get_planar_sample_ptr(avp_sw_sample_t data[],
                                                           uint32_t sample_idx,
                                                           uint8_t ch,
                                                           uint16_t bits_per_sample)
{
    uint8_t bytes = avp_swr_get_sample_bytes(bits_per_sample);

    return (const uint8_t *)data[ch] + (size_t)sample_idx * bytes;
}

static inline int32_t avp_swr_read_sample(const uint8_t *sample,
                                          uint16_t bits_per_sample)
{
    switch (bits_per_sample) {
        case 8u:
            return ((int32_t)sample[0] - 128) * 16777216;
        case 16u: {
            int16_t value;

            memcpy(&value, sample, sizeof(value));
            return (int32_t)value * 65536;
        }
        case 24u: {
            int32_t value;

            value = (int32_t)((uint32_t)sample[0] |
                              ((uint32_t)sample[1] << 8) |
                              ((uint32_t)sample[2] << 16));
            if ((value & 0x00800000) != 0) {
                value |= (int32_t)0xff000000;
            }
            return (int32_t)((int64_t)value * 256LL);
        }
        case 32u: {
            int32_t value;

            memcpy(&value, sample, sizeof(value));
            return value;
        }
        default:
            return 0;
    }
}

static inline void avp_swr_write_sample(uint8_t *dst,
                                        uint16_t bits_per_sample,
                                        int32_t sample)
{
    switch (bits_per_sample) {
        case 8u: {
            int32_t value = (int32_t)(((int64_t)sample + 2147483648LL) / 16777216LL);

            dst[0] = avp_clip_u8(value);
            break;
        }
        case 16u: {
            int16_t value = avp_clip_s16(sample / 65536);

            memcpy(dst, &value, sizeof(value));
            break;
        }
        case 24u: {
            uint32_t value = (uint32_t)(sample / 256);

            dst[0] = (uint8_t)(value & 0xffu);
            dst[1] = (uint8_t)((value >> 8) & 0xffu);
            dst[2] = (uint8_t)((value >> 16) & 0xffu);
            break;
        }
        case 32u:
            memcpy(dst, &sample, sizeof(sample));
            break;
        default:
            break;
    }
}

static inline int32_t avp_swr_read_interleaved_sample(avp_sw_sample_t data,
                                                      uint32_t sample_idx,
                                                      uint8_t ch,
                                                      uint8_t channels,
                                                      uint16_t bits_per_sample)
{
    const uint8_t *sample_ptr;

    sample_ptr = avp_swr_get_interleaved_sample_ptr(data,
                                                    sample_idx,
                                                    ch,
                                                    channels,
                                                    bits_per_sample);
    return avp_swr_read_sample(sample_ptr, bits_per_sample);
}

static inline void avp_swr_write_interleaved_sample(avp_sw_sample_t data,
                                                    uint32_t sample_idx,
                                                    uint8_t ch,
                                                    uint8_t channels,
                                                    uint16_t bits_per_sample,
                                                    int32_t sample)
{
    uint8_t *sample_ptr;

    sample_ptr = (uint8_t *)avp_swr_get_interleaved_sample_ptr(data,
                                                               sample_idx,
                                                               ch,
                                                               channels,
                                                               bits_per_sample);
    avp_swr_write_sample(sample_ptr, bits_per_sample, sample);
}

static inline int32_t avp_swr_read_planar_sample(avp_sw_sample_t data[],
                                                 uint32_t sample_idx,
                                                 uint8_t ch,
                                                 uint16_t bits_per_sample)
{
    const uint8_t *sample_ptr;

    sample_ptr = avp_swr_get_planar_sample_ptr(data,
                                               sample_idx,
                                               ch,
                                               bits_per_sample);
    return avp_swr_read_sample(sample_ptr, bits_per_sample);
}

static inline void avp_swr_write_planar_sample(avp_sw_sample_t data[],
                                               uint32_t sample_idx,
                                               uint8_t ch,
                                               uint16_t bits_per_sample,
                                               int32_t sample)
{
    uint8_t *sample_ptr;

    sample_ptr = (uint8_t *)avp_swr_get_planar_sample_ptr(data,
                                                          sample_idx,
                                                          ch,
                                                          bits_per_sample);
    avp_swr_write_sample(sample_ptr, bits_per_sample, sample);
}

static inline int32_t avp_swr_clip_q31_i64(int64_t sample)
{
    if (sample > INT32_MAX) {
        return INT32_MAX;
    }
    if (sample < INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)sample;
}

static const avp_swr_sinc_table_t *avp_swr_find_sinc_table(uint32_t src_sample_rate,
                                                           uint32_t dst_sample_rate)
{
    uint32_t i;

    for (i = 0u; i < (uint32_t)(sizeof(avp_swr_sinc_tables) / sizeof(avp_swr_sinc_tables[0])); i++) {
        if (avp_swr_sinc_tables[i].src_sample_rate == src_sample_rate &&
            avp_swr_sinc_tables[i].dst_sample_rate == dst_sample_rate) {
            return &avp_swr_sinc_tables[i];
        }
    }

    return NULL;
}

static inline void avp_swr_cubic_coef_q15(uint32_t frac_num,
                                          uint32_t frac_den,
                                          int16_t interp[4])
{
    int32_t x;
    int32_t x2;
    int32_t x3;
    int32_t c0;
    int32_t c1;
    int32_t c3;
    int32_t c2;

    if (frac_den == 0u) {
        frac_den = 1u;
    }

    x = (int32_t)(((uint64_t)frac_num << AVP_SWR_Q15_SHIFT) / frac_den);
    if (x > AVP_SWR_Q15_ONE) {
        x = AVP_SWR_Q15_ONE;
    }

    x2 = (int32_t)(((int64_t)x * x + (1 << (AVP_SWR_Q15_SHIFT - 1))) >> AVP_SWR_Q15_SHIFT);
    x3 = (int32_t)(((int64_t)x2 * x + (1 << (AVP_SWR_Q15_SHIFT - 1))) >> AVP_SWR_Q15_SHIFT);

    c0 = (int32_t)(((-5461LL * x + 5461LL * x3) + (1 << (AVP_SWR_Q15_SHIFT - 1))) >> AVP_SWR_Q15_SHIFT);
    c1 = x + ((x2 - x3) >> 1);
    c3 = (int32_t)(((-10923LL * x + 16384LL * x2 - 5461LL * x3) + (1 << (AVP_SWR_Q15_SHIFT - 1))) >> AVP_SWR_Q15_SHIFT);
    c2 = AVP_SWR_Q15_ONE - c0 - c1 - c3;

    interp[0] = (int16_t)c0;
    interp[1] = (int16_t)c1;
    interp[2] = (int16_t)c2;
    interp[3] = (int16_t)c3;
}

static inline int32_t avp_swr_sinc_coef_q15(const int16_t *sinc,
                                            int16_t interp[4])
{
    int64_t coef;

    coef = (int64_t)sinc[0] * interp[0] +
           (int64_t)sinc[1] * interp[1] +
           (int64_t)sinc[2] * interp[2] +
           (int64_t)sinc[3] * interp[3];
    return (int32_t)((coef + (1 << (AVP_SWR_Q15_SHIFT - 1))) >> AVP_SWR_Q15_SHIFT);
}

static int32_t avp_swr_read_mixed_interleaved_channel(avp_sw_sample_t in,
                                                      uint32_t sample,
                                                      uint8_t out_ch,
                                                      uint8_t src_channels,
                                                      uint8_t dst_channels,
                                                      uint16_t bits_per_sample)
{
    uint8_t ch;
    int64_t mixed;

    if (src_channels == 1u) {
        return avp_swr_read_interleaved_sample(in, sample, 0u, src_channels, bits_per_sample);
    }

    if (dst_channels == 1u) {
        mixed = 0;
        for (ch = 0u; ch < src_channels; ch++) {
            mixed += avp_swr_read_interleaved_sample(in,
                                                     sample,
                                                     ch,
                                                     src_channels,
                                                     bits_per_sample);
        }
        return avp_clip_s32(mixed / src_channels);
    }

    if (out_ch < src_channels) {
        return avp_swr_read_interleaved_sample(in,
                                               sample,
                                               out_ch,
                                               src_channels,
                                               bits_per_sample);
    }

    mixed = 0;
    for (ch = 0u; ch < src_channels; ch++) {
        mixed += avp_swr_read_interleaved_sample(in,
                                                 sample,
                                                 ch,
                                                 src_channels,
                                                 bits_per_sample);
    }
    return avp_clip_s32(mixed / src_channels);
}

static int32_t avp_swr_read_mixed_planar_channel(avp_sw_sample_t in[],
                                                 uint32_t sample,
                                                 uint8_t out_ch,
                                                 uint8_t src_channels,
                                                 uint8_t dst_channels,
                                                 uint16_t bits_per_sample)
{
    uint8_t ch;
    int64_t mixed;

    if (src_channels == 1u) {
        return avp_swr_read_planar_sample(in, sample, 0u, bits_per_sample);
    }

    if (dst_channels == 1u) {
        mixed = 0;
        for (ch = 0u; ch < src_channels; ch++) {
            mixed += avp_swr_read_planar_sample(in, sample, ch, bits_per_sample);
        }
        return avp_clip_s32(mixed / src_channels);
    }

    if (out_ch < src_channels) {
        return avp_swr_read_planar_sample(in, sample, out_ch, bits_per_sample);
    }

    mixed = 0;
    for (ch = 0u; ch < src_channels; ch++) {
        mixed += avp_swr_read_planar_sample(in, sample, ch, bits_per_sample);
    }
    return avp_clip_s32(mixed / src_channels);
}

avp_status_t avp_bits_convert_interleaved(uint16_t src_bits_per_sample,
                                          uint16_t dst_bits_per_sample,
                                          uint8_t channels,
                                          avp_sw_sample_t in,
                                          avp_sw_sample_t out,
                                          uint32_t samples)
{
    uint32_t sample;
    uint8_t ch;

    if (in == NULL || out == NULL || avp_swr_check_sample_params(channels, src_bits_per_sample) == 0) {
        return AVP_EINVAL;
    }

    for (sample = 0u; sample < samples; sample++) {
        for (ch = 0u; ch < channels; ch++) {
            int32_t value = avp_swr_read_interleaved_sample(in,
                                                            sample,
                                                            ch,
                                                            channels,
                                                            src_bits_per_sample);
            avp_swr_write_interleaved_sample(out,
                                             sample,
                                             ch,
                                             channels,
                                             dst_bits_per_sample,
                                             value);
        }
    }

    return AVP_OK;
}

avp_status_t avp_bits_convert_planar(uint16_t src_bits_per_sample,
                                     uint16_t dst_bits_per_sample,
                                     uint8_t channels,
                                     avp_sw_sample_t in[],
                                     avp_sw_sample_t out[],
                                     uint32_t samples)
{
    uint32_t sample;
    uint8_t ch;

    if (!avp_swr_check_planar_buffer(in, channels) ||
        !avp_swr_check_planar_buffer(out, channels) ||
        avp_swr_check_sample_params(channels, src_bits_per_sample) == 0) {
        return AVP_EINVAL;
    }

    for (ch = 0u; ch < channels; ch++) {
        for (sample = 0u; sample < samples; sample++) {
            int32_t value = avp_swr_read_planar_sample(in,
                                                       sample,
                                                       ch,
                                                       src_bits_per_sample);
            avp_swr_write_planar_sample(out,
                                        sample,
                                        ch,
                                        dst_bits_per_sample,
                                        value);
        }
    }

    return AVP_OK;
}

avp_status_t avp_channel_convert_interleaved(uint8_t src_channels,
                                             uint8_t dst_channels,
                                             uint16_t bits_per_sample,
                                             avp_sw_sample_t in,
                                             avp_sw_sample_t out,
                                             uint32_t samples)
{
    uint32_t sample;
    uint8_t ch;

    if (in == NULL || out == NULL) {
        return AVP_EINVAL;
    }

    for (sample = 0u; sample < samples; sample++) {
        for (ch = 0u; ch < dst_channels; ch++) {
            int32_t value = avp_swr_read_mixed_interleaved_channel(in,
                                                                   sample,
                                                                   ch,
                                                                   src_channels,
                                                                   dst_channels,
                                                                   bits_per_sample);
            avp_swr_write_interleaved_sample(out,
                                             sample,
                                             ch,
                                             dst_channels,
                                             bits_per_sample,
                                             value);
        }
    }

    return AVP_OK;
}

avp_status_t avp_channel_convert_planar(uint8_t src_channels,
                                        uint8_t dst_channels,
                                        uint16_t bits_per_sample,
                                        avp_sw_sample_t in[],
                                        avp_sw_sample_t out[],
                                        uint32_t samples)
{
    uint32_t sample;
    uint8_t ch;

    if (!avp_swr_check_planar_buffer(in, src_channels) ||
        !avp_swr_check_planar_buffer(out, dst_channels) ||
        avp_swr_check_sample_params(dst_channels, bits_per_sample) == 0) {
        return AVP_EINVAL;
    }

    for (ch = 0u; ch < dst_channels; ch++) {
        for (sample = 0u; sample < samples; sample++) {
            int32_t value = avp_swr_read_mixed_planar_channel(in,
                                                              sample,
                                                              ch,
                                                              src_channels,
                                                              dst_channels,
                                                              bits_per_sample);
            avp_swr_write_planar_sample(out,
                                        sample,
                                        ch,
                                        bits_per_sample,
                                        value);
        }
    }

    return AVP_OK;
}

static int32_t avp_swr_resample_interleaved_sample(const avp_swr_sinc_table_t *table,
                                                   avp_sw_sample_t in,
                                                   uint32_t in_samples,
                                                   uint8_t ch,
                                                   uint8_t channels,
                                                   uint16_t bits_per_sample,
                                                   uint32_t last_sample,
                                                   uint32_t samp_frac_num)
{
    int64_t sample_sum = 0;
    int64_t weight_sum = 0;
    uint64_t frac_scaled;
    uint32_t offset;
    uint32_t frac_num;
    int32_t filt_offset;
    uint16_t tap;
    int16_t interp[4];

    frac_scaled = (uint64_t)samp_frac_num * table->oversample;
    offset = (uint32_t)(frac_scaled / table->den_rate);
    frac_num = (uint32_t)(frac_scaled % table->den_rate);
    filt_offset = (int32_t)table->filt_len / 2 - 1;
    avp_swr_cubic_coef_q15(frac_num, table->den_rate, interp);

    for (tap = 0u; tap < table->filt_len; tap++) {
        int32_t idx = (int32_t)last_sample + (int32_t)tap - filt_offset;
        const int16_t *sinc;
        int32_t coef;

        if (idx < 0 || idx >= (int32_t)in_samples) {
            continue;
        }

        sinc = table->sinc_table + 4u + ((uint32_t)tap + 1u) * table->oversample - offset - 2u;
        coef = avp_swr_sinc_coef_q15(sinc, interp);
        sample_sum += (int64_t)avp_swr_read_interleaved_sample(in,
                                                               (uint32_t)idx,
                                                               ch,
                                                               channels,
                                                               bits_per_sample) *
                      coef;
        weight_sum += coef;
    }

    if (weight_sum == 0) {
        if (last_sample >= in_samples) {
            last_sample = in_samples - 1u;
        }
        return avp_swr_read_interleaved_sample(in,
                                               last_sample,
                                               ch,
                                               channels,
                                               bits_per_sample);
    }

    return avp_swr_clip_q31_i64(sample_sum / weight_sum);
}

static int32_t avp_swr_resample_planar_sample(const avp_swr_sinc_table_t *table,
                                              avp_sw_sample_t in[],
                                              uint32_t in_samples,
                                              uint8_t ch,
                                              uint16_t bits_per_sample,
                                              uint32_t last_sample,
                                              uint32_t samp_frac_num)
{
    int64_t sample_sum = 0;
    int64_t weight_sum = 0;
    uint64_t frac_scaled;
    uint32_t offset;
    uint32_t frac_num;
    int32_t filt_offset;
    uint16_t tap;
    int16_t interp[4];

    frac_scaled = (uint64_t)samp_frac_num * table->oversample;
    offset = (uint32_t)(frac_scaled / table->den_rate);
    frac_num = (uint32_t)(frac_scaled % table->den_rate);
    filt_offset = (int32_t)table->filt_len / 2 - 1;
    avp_swr_cubic_coef_q15(frac_num, table->den_rate, interp);

    for (tap = 0u; tap < table->filt_len; tap++) {
        int32_t idx = (int32_t)last_sample + (int32_t)tap - filt_offset;
        const int16_t *sinc;
        int32_t coef;

        if (idx < 0 || idx >= (int32_t)in_samples) {
            continue;
        }

        sinc = table->sinc_table + 4u + ((uint32_t)tap + 1u) * table->oversample - offset - 2u;
        coef = avp_swr_sinc_coef_q15(sinc, interp);
        sample_sum += (int64_t)avp_swr_read_planar_sample(in,
                                                          (uint32_t)idx,
                                                          ch,
                                                          bits_per_sample) *
                      coef;
        weight_sum += coef;
    }

    if (weight_sum == 0) {
        if (last_sample >= in_samples) {
            last_sample = in_samples - 1u;
        }
        return avp_swr_read_planar_sample(in,
                                          last_sample,
                                          ch,
                                          bits_per_sample);
    }

    return avp_swr_clip_q31_i64(sample_sum / weight_sum);
}

avp_status_t avp_sample_rate_convert_interleaved(uint32_t src_sample_rate,
                                                 uint32_t dst_sample_rate,
                                                 uint8_t channels,
                                                 uint16_t bits_per_sample,
                                                 avp_sw_sample_t in,
                                                 uint32_t in_samples,
                                                 avp_sw_sample_t out,
                                                 uint32_t *out_samples)
{
    const avp_swr_sinc_table_t *table;
    uint32_t sample;
    uint32_t produced;
    uint32_t last_sample = 0u;
    uint32_t samp_frac_num = 0u;
    uint32_t int_advance;
    uint32_t frac_advance;
    uint8_t ch;

    if (in == NULL || out == NULL || out_samples == NULL ||
        in_samples == 0u ||
        avp_swr_check_sample_params(channels, bits_per_sample) == 0) {
        return AVP_EINVAL;
    }

    produced = avp_swr_get_out_samples(src_sample_rate,
                                       dst_sample_rate,
                                       in_samples);

    if (src_sample_rate == dst_sample_rate) {
        memcpy(out,
               in,
               (size_t)in_samples * channels *
                   avp_swr_get_sample_bytes(bits_per_sample));
        *out_samples = in_samples;
        return AVP_OK;
    }

    table = avp_swr_find_sinc_table(src_sample_rate, dst_sample_rate);
    if (table == NULL) {
        return AVP_EUNSUPPORTED;
    }

    int_advance = table->num_rate / table->den_rate;
    frac_advance = table->num_rate % table->den_rate;

    for (sample = 0u; sample < produced; sample++) {
        for (ch = 0u; ch < channels; ch++) {
            int32_t value;

            value = avp_swr_resample_interleaved_sample(table,
                                                        in,
                                                        in_samples,
                                                        ch,
                                                        channels,
                                                        bits_per_sample,
                                                        last_sample,
                                                        samp_frac_num);
            avp_swr_write_interleaved_sample(out,
                                             sample,
                                             ch,
                                             channels,
                                             bits_per_sample,
                                             value);
        }

        last_sample += int_advance;
        samp_frac_num += frac_advance;
        if (samp_frac_num >= table->den_rate) {
            samp_frac_num -= table->den_rate;
            last_sample++;
        }
    }

    *out_samples = produced;
    return AVP_OK;
}

avp_status_t avp_sample_rate_convert_planar(uint32_t src_sample_rate,
                                            uint32_t dst_sample_rate,
                                            uint8_t channels,
                                            uint16_t bits_per_sample,
                                            avp_sw_sample_t in[],
                                            uint32_t in_samples,
                                            avp_sw_sample_t out[],
                                            uint32_t *out_samples)
{
    const avp_swr_sinc_table_t *table;
    uint32_t sample;
    uint32_t produced;
    uint32_t last_sample = 0u;
    uint32_t samp_frac_num = 0u;
    uint32_t int_advance;
    uint32_t frac_advance;
    uint8_t ch;

    if (!avp_swr_check_planar_buffer(in, channels) ||
        !avp_swr_check_planar_buffer(out, channels) ||
        out_samples == NULL ||
        in_samples == 0u ||
        avp_swr_check_sample_params(channels, bits_per_sample) == 0) {
        return AVP_EINVAL;
    }

    produced = avp_swr_get_out_samples(src_sample_rate,
                                       dst_sample_rate,
                                       in_samples);

    if (src_sample_rate == dst_sample_rate) {
        uint8_t bytes = avp_swr_get_sample_bytes(bits_per_sample);

        for (ch = 0u; ch < channels; ch++) {
            memcpy(out[ch], in[ch], (size_t)in_samples * bytes);
        }
        *out_samples = in_samples;
        return AVP_OK;
    }

    table = avp_swr_find_sinc_table(src_sample_rate, dst_sample_rate);
    if (table == NULL) {
        return AVP_EUNSUPPORTED;
    }

    int_advance = table->num_rate / table->den_rate;
    frac_advance = table->num_rate % table->den_rate;

    for (sample = 0u; sample < produced; sample++) {
        for (ch = 0u; ch < channels; ch++) {
            int32_t value;

            value = avp_swr_resample_planar_sample(table,
                                                   in,
                                                   in_samples,
                                                   ch,
                                                   bits_per_sample,
                                                   last_sample,
                                                   samp_frac_num);
            avp_swr_write_planar_sample(out,
                                        sample,
                                        ch,
                                        bits_per_sample,
                                        value);
        }

        last_sample += int_advance;
        samp_frac_num += frac_advance;
        if (samp_frac_num >= table->den_rate) {
            samp_frac_num -= table->den_rate;
            last_sample++;
        }
    }

    *out_samples = produced;
    return AVP_OK;
}

avp_status_t avp_swr_planar2interleave(uint8_t channels,
                                       uint16_t bits_per_sample,
                                       avp_sw_sample_t in[],
                                       avp_sw_sample_t out,
                                       uint32_t samples)
{
    uint32_t sample;
    uint8_t ch;

    if (!avp_swr_check_planar_buffer(in, channels) ||
        out == NULL ||
        avp_swr_check_sample_params(channels, bits_per_sample) == 0) {
        return AVP_EINVAL;
    }

    for (sample = 0u; sample < samples; sample++) {
        for (ch = 0u; ch < channels; ch++) {
            int32_t value = avp_swr_read_planar_sample(in,
                                                       sample,
                                                       ch,
                                                       bits_per_sample);
            avp_swr_write_interleaved_sample(out,
                                             sample,
                                             ch,
                                             channels,
                                             bits_per_sample,
                                             value);
        }
    }

    return AVP_OK;
}

avp_status_t avp_swr_interleave2planar(uint8_t channels,
                                       uint16_t bits_per_sample,
                                       avp_sw_sample_t in,
                                       avp_sw_sample_t out[],
                                       uint32_t samples)
{
    uint32_t sample;
    uint8_t ch;

    if (in == NULL ||
        !avp_swr_check_planar_buffer(out, channels) ||
        avp_swr_check_sample_params(channels, bits_per_sample) == 0) {
        return AVP_EINVAL;
    }

    for (sample = 0u; sample < samples; sample++) {
        for (ch = 0u; ch < channels; ch++) {
            int32_t value = avp_swr_read_interleaved_sample(in,
                                                            sample,
                                                            ch,
                                                            channels,
                                                            bits_per_sample);
            avp_swr_write_planar_sample(out,
                                        sample,
                                        ch,
                                        bits_per_sample,
                                        value);
        }
    }

    return AVP_OK;
}
