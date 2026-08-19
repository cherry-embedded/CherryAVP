/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "avp_swr.h"

#define AVP_RESAMPLE_TMP_COUNT 4u
#define AVP_SWR_PI             3.14159265358979323846
#define AVP_SWR_SINC_RADIUS    8u

typedef struct {
    uint8_t **planes;
    uint8_t plane_count;
    size_t plane_bytes;
} avp_swr_buffer_t;

struct avp_swr_context {
    avp_audio_format_t in_fmt;
    avp_audio_format_t out_fmt;
    avp_swr_buffer_t tmp[AVP_RESAMPLE_TMP_COUNT];
};

static inline avp_status_t avp_swr_check_format(const avp_audio_format_t *fmt)
{
    if (fmt == NULL ||
        fmt->sample_rate == 0u ||
        fmt->channels == 0u ||
        avp_swr_get_sample_bytes(fmt->bits_per_sample) == 0u ||
        (fmt->sample_layout != AVP_SAMPLE_LAYOUT_INTERLEAVED &&
         fmt->sample_layout != AVP_SAMPLE_LAYOUT_PLANAR)) {
        return AVP_EINVAL;
    }

    return AVP_OK;
}

static inline int avp_swr_check_sample_params(uint8_t channels, uint16_t bits_per_sample)
{
    return channels != 0u &&
           avp_swr_get_sample_bytes(bits_per_sample) != 0u;
}

static inline int avp_swr_check_planar_buffer(const void *const data[], uint8_t channels)
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

static inline uint8_t avp_swr_get_plane_count(const avp_audio_format_t *fmt)
{
    return fmt->sample_layout == AVP_SAMPLE_LAYOUT_PLANAR ? fmt->channels : 1u;
}

static inline int avp_swr_check_buffer(const avp_audio_format_t *fmt,
                                       const void *const data[])
{
    uint8_t planes;
    uint8_t i;

    if (fmt == NULL || data == NULL) {
        return 0;
    }

    planes = avp_swr_get_plane_count(fmt);
    for (i = 0u; i < planes; i++) {
        if (data[i] == NULL) {
            return 0;
        }
    }

    return 1;
}

static inline avp_status_t avp_swr_get_plane_size(const avp_audio_format_t *fmt,
                                                  uint32_t samples,
                                                  uint32_t plane,
                                                  size_t *size)
{
    uint64_t total;
    uint8_t bytes;

    if (fmt == NULL || size == NULL) {
        return AVP_EINVAL;
    }

    bytes = avp_swr_get_sample_bytes(fmt->bits_per_sample);
    if (bytes == 0u) {
        return AVP_EINVAL;
    }

    total = (uint64_t)samples * bytes;
    if (fmt->sample_layout == AVP_SAMPLE_LAYOUT_INTERLEAVED) {
        if (plane != 0u) {
            return AVP_EINVAL;
        }
        total *= fmt->channels;
    } else if (plane >= fmt->channels) {
        return AVP_EINVAL;
    }

    if (total > (uint64_t)SIZE_MAX) {
        return AVP_ERANGE;
    }

    *size = (size_t)total;
    return AVP_OK;
}

static inline void avp_swr_buffer_free(avp_swr_buffer_t *buffer)
{
    uint8_t i;

    if (buffer == NULL) {
        return;
    }

    if (buffer->planes != NULL) {
        for (i = 0u; i < buffer->plane_count; i++) {
            avp_free(buffer->planes[i]);
        }
        avp_free(buffer->planes);
    }

    memset(buffer, 0, sizeof(*buffer));
}

static inline avp_status_t avp_swr_buffer_prepare(avp_swr_buffer_t *buffer,
                                                  const avp_audio_format_t *fmt,
                                                  uint32_t samples)
{
    uint8_t plane_count;
    uint8_t plane;
    size_t plane_bytes;

    if (buffer == NULL || avp_swr_check_format(fmt) != AVP_OK) {
        return AVP_EINVAL;
    }

    plane_count = avp_swr_get_plane_count(fmt);
    if (avp_swr_get_plane_size(fmt, samples, 0u, &plane_bytes) != AVP_OK) {
        return AVP_EINVAL;
    }

    if (buffer->planes != NULL &&
        buffer->plane_count == plane_count &&
        buffer->plane_bytes >= plane_bytes) {
        return AVP_OK;
    }

    avp_swr_buffer_free(buffer);

    buffer->planes = (uint8_t **)avp_calloc(plane_count, sizeof(buffer->planes[0]));
    if (buffer->planes == NULL) {
        return AVP_ENOMEM;
    }
    buffer->plane_count = plane_count;
    buffer->plane_bytes = plane_bytes;

    for (plane = 0u; plane < plane_count; plane++) {
        if (plane_bytes != 0u) {
            buffer->planes[plane] = (uint8_t *)avp_malloc(plane_bytes);
            if (buffer->planes[plane] == NULL) {
                avp_swr_buffer_free(buffer);
                return AVP_ENOMEM;
            }
        }
    }

    return AVP_OK;
}

static inline avp_status_t avp_audio_copy(const avp_audio_format_t *fmt,
                                          const void *const in[],
                                          void *const out[],
                                          uint32_t samples)
{
    uint8_t planes;
    uint8_t plane;

    if (!avp_swr_check_buffer(fmt, in) ||
        !avp_swr_check_buffer(fmt, (const void *const *)out)) {
        return AVP_EINVAL;
    }

    planes = avp_swr_get_plane_count(fmt);
    for (plane = 0u; plane < planes; plane++) {
        size_t plane_size;
        avp_status_t st;

        st = avp_swr_get_plane_size(fmt, samples, plane, &plane_size);
        if (st != AVP_OK) {
            return st;
        }
        memmove(out[plane], in[plane], plane_size);
    }

    return AVP_OK;
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

static inline const uint8_t *avp_swr_get_interleaved_sample_ptr(const void *data,
                                                                uint32_t sample_idx,
                                                                uint8_t ch,
                                                                uint8_t channels,
                                                                uint16_t bits_per_sample)
{
    uint8_t bytes = avp_swr_get_sample_bytes(bits_per_sample);

    return (const uint8_t *)data + ((size_t)sample_idx * channels + ch) * bytes;
}

static inline const uint8_t *avp_swr_get_planar_sample_ptr(const void *const data[],
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
        case 32u:
            memcpy(dst, &sample, sizeof(sample));
            break;
        default:
            break;
    }
}

static inline int32_t avp_swr_read_interleaved_sample(const void *data,
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

static inline void avp_swr_write_interleaved_sample(void *data,
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

static inline int32_t avp_swr_read_planar_sample(const void *const data[],
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

static inline void avp_swr_write_planar_sample(void *const data[],
                                               uint32_t sample_idx,
                                               uint8_t ch,
                                               uint16_t bits_per_sample,
                                               int32_t sample)
{
    uint8_t *sample_ptr;

    sample_ptr = (uint8_t *)avp_swr_get_planar_sample_ptr((const void *const *)data,
                                                          sample_idx,
                                                          ch,
                                                          bits_per_sample);
    avp_swr_write_sample(sample_ptr, bits_per_sample, sample);
}

static inline double avp_swr_sinc(double x)
{
    if (x == 0.0) {
        return 1.0;
    }

    x *= AVP_SWR_PI;
    return sin(x) / x;
}

static inline double avp_swr_sinc_window(double distance)
{
    double abs_distance;

    abs_distance = fabs(distance);
    if (abs_distance > (double)AVP_SWR_SINC_RADIUS) {
        return 0.0;
    }

    return 0.5 * (1.0 + cos(AVP_SWR_PI * abs_distance / (double)AVP_SWR_SINC_RADIUS));
}

static inline double avp_swr_sinc_weight(double distance, double cutoff)
{
    return cutoff * avp_swr_sinc(cutoff * distance) * avp_swr_sinc_window(distance);
}

static inline int32_t avp_swr_clip_q31(double sample)
{
    if (sample > (double)INT32_MAX) {
        return INT32_MAX;
    }
    if (sample < (double)INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)(sample >= 0.0 ? sample + 0.5 : sample - 0.5);
}

static inline double avp_swr_resample_cutoff(uint32_t src_sample_rate,
                                             uint32_t dst_sample_rate)
{
    if (src_sample_rate <= dst_sample_rate) {
        return 1.0;
    }

    return (double)dst_sample_rate / (double)src_sample_rate;
}

static inline int32_t avp_swr_resample_interleaved_sample(const void *in,
                                                          uint32_t sample,
                                                          uint8_t ch,
                                                          uint8_t channels,
                                                          uint16_t bits_per_sample,
                                                          uint32_t in_samples,
                                                          uint32_t src_sample_rate,
                                                          uint32_t dst_sample_rate)
{
    double cutoff;
    double sample_sum;
    double src_pos;
    double weight_sum;
    int64_t center;
    int64_t idx;

    src_pos = ((double)sample * (double)src_sample_rate) / (double)dst_sample_rate;
    center = (int64_t)src_pos;
    if (center < 0) {
        center = 0;
    } else if (center >= (int64_t)in_samples) {
        center = (int64_t)in_samples - 1;
    }

    cutoff = avp_swr_resample_cutoff(src_sample_rate, dst_sample_rate);
    sample_sum = 0.0;
    weight_sum = 0.0;

    for (idx = center - (int64_t)AVP_SWR_SINC_RADIUS;
         idx <= center + (int64_t)AVP_SWR_SINC_RADIUS;
         idx++) {
        double weight;

        if (idx < 0 || idx >= (int64_t)in_samples) {
            continue;
        }

        weight = avp_swr_sinc_weight(src_pos - (double)idx, cutoff);
        if (weight == 0.0) {
            continue;
        }

        sample_sum += (double)avp_swr_read_interleaved_sample(in,
                                                              (uint32_t)idx,
                                                              ch,
                                                              channels,
                                                              bits_per_sample) *
                      weight;
        weight_sum += weight;
    }

    if (weight_sum == 0.0) {
        return avp_swr_read_interleaved_sample(in,
                                               (uint32_t)center,
                                               ch,
                                               channels,
                                               bits_per_sample);
    }

    return avp_swr_clip_q31(sample_sum / weight_sum);
}

static inline int32_t avp_swr_resample_planar_sample(const void *const in[],
                                                     uint32_t sample,
                                                     uint8_t ch,
                                                     uint16_t bits_per_sample,
                                                     uint32_t in_samples,
                                                     uint32_t src_sample_rate,
                                                     uint32_t dst_sample_rate)
{
    double cutoff;
    double sample_sum;
    double src_pos;
    double weight_sum;
    int64_t center;
    int64_t idx;

    src_pos = ((double)sample * (double)src_sample_rate) / (double)dst_sample_rate;
    center = (int64_t)src_pos;
    if (center < 0) {
        center = 0;
    } else if (center >= (int64_t)in_samples) {
        center = (int64_t)in_samples - 1;
    }

    cutoff = avp_swr_resample_cutoff(src_sample_rate, dst_sample_rate);
    sample_sum = 0.0;
    weight_sum = 0.0;

    for (idx = center - (int64_t)AVP_SWR_SINC_RADIUS;
         idx <= center + (int64_t)AVP_SWR_SINC_RADIUS;
         idx++) {
        double weight;

        if (idx < 0 || idx >= (int64_t)in_samples) {
            continue;
        }

        weight = avp_swr_sinc_weight(src_pos - (double)idx, cutoff);
        if (weight == 0.0) {
            continue;
        }

        sample_sum += (double)avp_swr_read_planar_sample(in,
                                                         (uint32_t)idx,
                                                         ch,
                                                         bits_per_sample) *
                      weight;
        weight_sum += weight;
    }

    if (weight_sum == 0.0) {
        return avp_swr_read_planar_sample(in,
                                          (uint32_t)center,
                                          ch,
                                          bits_per_sample);
    }

    return avp_swr_clip_q31(sample_sum / weight_sum);
}

static int32_t avp_swr_read_mixed_interleaved_channel(const void *in,
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

static int32_t avp_swr_read_mixed_planar_channel(const void *const in[],
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
                                          const void *in,
                                          void *out,
                                          uint32_t samples)
{
    uint32_t sample;
    uint8_t ch;

    if (in == NULL || out == NULL) {
        return AVP_EINVAL;
    }

    for (sample = 0u; sample < samples; sample++) {
        for (ch = 0u; ch < channels; ch++) {
            avp_swr_write_interleaved_sample(out,
                                             sample,
                                             ch,
                                             channels,
                                             dst_bits_per_sample,
                                             avp_swr_read_interleaved_sample(in,
                                                                             sample,
                                                                             ch,
                                                                             channels,
                                                                             src_bits_per_sample));
        }
    }

    return AVP_OK;
}

avp_status_t avp_bits_convert_planar(uint16_t src_bits_per_sample,
                                     uint16_t dst_bits_per_sample,
                                     uint8_t channels,
                                     const void *const in[],
                                     void *const out[],
                                     uint32_t samples)
{
    uint32_t sample;
    uint8_t ch;

    if (!avp_swr_check_planar_buffer(in, channels) ||
        !avp_swr_check_planar_buffer((const void *const *)out, channels)) {
        return AVP_EINVAL;
    }

    for (ch = 0u; ch < channels; ch++) {
        for (sample = 0u; sample < samples; sample++) {
            avp_swr_write_planar_sample(out,
                                        sample,
                                        ch,
                                        dst_bits_per_sample,
                                        avp_swr_read_planar_sample(in,
                                                                   sample,
                                                                   ch,
                                                                   src_bits_per_sample));
        }
    }

    return AVP_OK;
}

avp_status_t avp_channel_convert_interleaved(uint8_t src_channels,
                                             uint8_t dst_channels,
                                             uint16_t bits_per_sample,
                                             const void *in,
                                             void *out,
                                             uint32_t samples)
{
    uint32_t sample;
    uint8_t ch;

    if (in == NULL || out == NULL) {
        return AVP_EINVAL;
    }

    for (sample = 0u; sample < samples; sample++) {
        for (ch = 0u; ch < dst_channels; ch++) {
            avp_swr_write_interleaved_sample(out,
                                             sample,
                                             ch,
                                             dst_channels,
                                             bits_per_sample,
                                             avp_swr_read_mixed_interleaved_channel(in,
                                                                                    sample,
                                                                                    ch,
                                                                                    src_channels,
                                                                                    dst_channels,
                                                                                    bits_per_sample));
        }
    }

    return AVP_OK;
}

avp_status_t avp_channel_convert_planar(uint8_t src_channels,
                                        uint8_t dst_channels,
                                        uint16_t bits_per_sample,
                                        const void *const in[],
                                        void *const out[],
                                        uint32_t samples)
{
    uint32_t sample;
    uint8_t ch;

    if (!avp_swr_check_planar_buffer(in, src_channels) ||
        !avp_swr_check_planar_buffer((const void *const *)out, dst_channels)) {
        return AVP_EINVAL;
    }

    for (ch = 0u; ch < dst_channels; ch++) {
        for (sample = 0u; sample < samples; sample++) {
            avp_swr_write_planar_sample(out,
                                        sample,
                                        ch,
                                        bits_per_sample,
                                        avp_swr_read_mixed_planar_channel(in,
                                                                          sample,
                                                                          ch,
                                                                          src_channels,
                                                                          dst_channels,
                                                                          bits_per_sample));
        }
    }

    return AVP_OK;
}

avp_status_t avp_sample_rate_convert_interleaved(uint32_t src_sample_rate,
                                                 uint32_t dst_sample_rate,
                                                 uint8_t channels,
                                                 uint16_t bits_per_sample,
                                                 const void *in,
                                                 uint32_t in_samples,
                                                 void *out,
                                                 uint32_t *out_samples)
{
    uint32_t sample;
    uint32_t produced;
    uint8_t ch;

    if (in == NULL || out == NULL || out_samples == NULL) {
        return AVP_EINVAL;
    }

    produced = avp_swr_get_out_samples(src_sample_rate,
                                       dst_sample_rate,
                                       in_samples);

    if (src_sample_rate == dst_sample_rate) {
        memmove(out,
                in,
                (size_t)in_samples * channels *
                    avp_swr_get_sample_bytes(bits_per_sample));
        *out_samples = in_samples;
        return AVP_OK;
    }

    for (sample = 0u; sample < produced; sample++) {
        for (ch = 0u; ch < channels; ch++) {
            int32_t value;

            value = avp_swr_resample_interleaved_sample(in,
                                                        sample,
                                                        ch,
                                                        channels,
                                                        bits_per_sample,
                                                        in_samples,
                                                        src_sample_rate,
                                                        dst_sample_rate);
            avp_swr_write_interleaved_sample(out,
                                             sample,
                                             ch,
                                             channels,
                                             bits_per_sample,
                                             value);
        }
    }

    *out_samples = produced;
    return AVP_OK;
}

avp_status_t avp_sample_rate_convert_planar(uint32_t src_sample_rate,
                                            uint32_t dst_sample_rate,
                                            uint8_t channels,
                                            uint16_t bits_per_sample,
                                            const void *const in[],
                                            uint32_t in_samples,
                                            void *const out[],
                                            uint32_t *out_samples)
{
    uint32_t sample;
    uint32_t produced;
    uint8_t ch;

    if (!avp_swr_check_planar_buffer(in, channels) ||
        !avp_swr_check_planar_buffer((const void *const *)out, channels) ||
        out_samples == NULL ||
        in_samples == 0u) {
        return AVP_EINVAL;
    }

    produced = avp_swr_get_out_samples(src_sample_rate,
                                       dst_sample_rate,
                                       in_samples);

    if (src_sample_rate == dst_sample_rate) {
        uint8_t bytes = avp_swr_get_sample_bytes(bits_per_sample);

        for (ch = 0u; ch < channels; ch++) {
            memmove(out[ch], in[ch], (size_t)in_samples * bytes);
        }
        *out_samples = in_samples;
        return AVP_OK;
    }

    for (ch = 0u; ch < channels; ch++) {
        for (sample = 0u; sample < produced; sample++) {
            int32_t value;

            value = avp_swr_resample_planar_sample(in,
                                                   sample,
                                                   ch,
                                                   bits_per_sample,
                                                   in_samples,
                                                   src_sample_rate,
                                                   dst_sample_rate);
            avp_swr_write_planar_sample(out,
                                        sample,
                                        ch,
                                        bits_per_sample,
                                        value);
        }
    }

    *out_samples = produced;
    return AVP_OK;
}

avp_status_t avp_swr_planar2interleave(uint8_t channels,
                                       uint16_t bits_per_sample,
                                       const void *const in[],
                                       void *out,
                                       uint32_t samples)
{
    uint32_t sample;
    uint8_t ch;

    if (!avp_swr_check_planar_buffer(in, channels) ||
        out == NULL) {
        return AVP_EINVAL;
    }

    for (sample = 0u; sample < samples; sample++) {
        for (ch = 0u; ch < channels; ch++) {
            int32_t value;

            value = avp_swr_read_planar_sample(in, sample, ch, bits_per_sample);
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
                                       const void *in,
                                       void *const out[],
                                       uint32_t samples)
{
    uint32_t sample;
    uint8_t ch;

    if (in == NULL ||
        !avp_swr_check_planar_buffer((const void *const *)out, channels)) {
        return AVP_EINVAL;
    }

    for (sample = 0u; sample < samples; sample++) {
        for (ch = 0u; ch < channels; ch++) {
            int32_t value;

            value = avp_swr_read_interleaved_sample(in,
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

avp_swr_t *avp_swr_open(const avp_audio_format_t *in_fmt,
                        const avp_audio_format_t *out_fmt)
{
    avp_swr_t *ctx;

    if (avp_swr_check_format(in_fmt) != AVP_OK ||
        avp_swr_check_format(out_fmt) != AVP_OK) {
        return NULL;
    }

    ctx = (avp_swr_t *)avp_calloc(1u, sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->in_fmt = *in_fmt;
    ctx->out_fmt = *out_fmt;
    return ctx;
}

void avp_swr_close(avp_swr_t *ctx)
{
    uint8_t i;

    if (ctx == NULL) {
        return;
    }

    for (i = 0u; i < AVP_RESAMPLE_TMP_COUNT; i++) {
        avp_swr_buffer_free(&ctx->tmp[i]);
    }
    avp_free(ctx);
}

static inline avp_swr_buffer_t *avp_swr_next_tmp(avp_swr_t *ctx,
                                                 uint8_t *index)
{
    avp_swr_buffer_t *buffer;

    buffer = &ctx->tmp[*index];
    *index = (uint8_t)((*index + 1u) % AVP_RESAMPLE_TMP_COUNT);
    return buffer;
}

int avp_swr_convert(avp_swr_t *ctx,
                    const void *const in[],
                    uint32_t in_samples,
                    void *const out[],
                    uint32_t out_samples)
{
    const void *const *src_in;
    avp_audio_format_t src_fmt;
    uint32_t src_samples;
    uint8_t tmp_index = 0u;
    avp_status_t st;

    if (ctx == NULL ||
        in_samples == 0u ||
        out_samples == 0u ||
        !avp_swr_check_buffer(&ctx->in_fmt, in) ||
        !avp_swr_check_buffer(&ctx->out_fmt, (const void *const *)out)) {
        if (ctx != NULL && (in == NULL || in_samples == 0u)) {
            return 0;
        }
        return AVP_EINVAL;
    }

    if (avp_swr_get_out_samples(ctx->in_fmt.sample_rate,
                                ctx->out_fmt.sample_rate,
                                in_samples) > out_samples) {
        return AVP_EBUFFER;
    }

    src_in = in;
    src_fmt = ctx->in_fmt;
    src_samples = in_samples;

    if (src_fmt.bits_per_sample != ctx->out_fmt.bits_per_sample) {
        avp_swr_buffer_t *tmp = avp_swr_next_tmp(ctx, &tmp_index);
        avp_audio_format_t dst_fmt = src_fmt;
        const void *src_data;
        void *dst_data;

        dst_fmt.bits_per_sample = ctx->out_fmt.bits_per_sample;
        st = avp_swr_buffer_prepare(tmp, &dst_fmt, src_samples);
        if (st != AVP_OK) {
            return st;
        }

        if (src_fmt.sample_layout == AVP_SAMPLE_LAYOUT_INTERLEAVED) {
            src_data = src_in[0];
            dst_data = tmp->planes[0];
            st = avp_bits_convert_interleaved(
                src_fmt.bits_per_sample,
                dst_fmt.bits_per_sample,
                src_fmt.channels,
                src_data,
                dst_data,
                src_samples);
        } else {
            const void *const *src_planes = src_in;
            void *const *dst_planes = (void *const *)tmp->planes;

            st = avp_bits_convert_planar(
                src_fmt.bits_per_sample,
                dst_fmt.bits_per_sample,
                src_fmt.channels,
                src_planes,
                dst_planes,
                src_samples);
        }
        if (st != AVP_OK) {
            return st;
        }

        src_fmt = dst_fmt;
        src_in = (const void *const *)tmp->planes;
    }

    if (src_fmt.channels != ctx->out_fmt.channels) {
        avp_swr_buffer_t *tmp = avp_swr_next_tmp(ctx, &tmp_index);
        avp_audio_format_t dst_fmt = src_fmt;
        const void *src_data;
        void *dst_data;

        dst_fmt.channels = ctx->out_fmt.channels;
        st = avp_swr_buffer_prepare(tmp, &dst_fmt, src_samples);
        if (st != AVP_OK) {
            return st;
        }

        if (src_fmt.sample_layout == AVP_SAMPLE_LAYOUT_INTERLEAVED) {
            src_data = src_in[0];
            dst_data = tmp->planes[0];
            st = avp_channel_convert_interleaved(src_fmt.channels,
                                                 dst_fmt.channels,
                                                 src_fmt.bits_per_sample,
                                                 src_data,
                                                 dst_data,
                                                 src_samples);
        } else {
            const void *const *src_planes = src_in;
            void *const *dst_planes = (void *const *)tmp->planes;

            st = avp_channel_convert_planar(src_fmt.channels,
                                            dst_fmt.channels,
                                            src_fmt.bits_per_sample,
                                            src_planes,
                                            dst_planes,
                                            src_samples);
        }
        if (st != AVP_OK) {
            return st;
        }

        src_fmt = dst_fmt;
        src_in = (const void *const *)tmp->planes;
    }

    if (src_fmt.sample_rate != ctx->out_fmt.sample_rate) {
        avp_swr_buffer_t *tmp = avp_swr_next_tmp(ctx, &tmp_index);
        avp_audio_format_t dst_fmt = src_fmt;
        uint32_t next_samples;
        const void *src_data;
        void *dst_data;

        dst_fmt.sample_rate = ctx->out_fmt.sample_rate;
        next_samples = avp_swr_get_out_samples(src_fmt.sample_rate,
                                               dst_fmt.sample_rate,
                                               src_samples);
        st = avp_swr_buffer_prepare(tmp, &dst_fmt, next_samples);
        if (st != AVP_OK) {
            return st;
        }

        if (src_fmt.sample_layout == AVP_SAMPLE_LAYOUT_INTERLEAVED) {
            src_data = src_in[0];
            dst_data = tmp->planes[0];
            st = avp_sample_rate_convert_interleaved(src_fmt.sample_rate,
                                                     dst_fmt.sample_rate,
                                                     src_fmt.channels,
                                                     src_fmt.bits_per_sample,
                                                     src_data,
                                                     src_samples,
                                                     dst_data,
                                                     &next_samples);
        } else {
            const void *const *src_planes = src_in;
            void *const *dst_planes = (void *const *)tmp->planes;

            st = avp_sample_rate_convert_planar(src_fmt.sample_rate,
                                                dst_fmt.sample_rate,
                                                src_fmt.channels,
                                                src_fmt.bits_per_sample,
                                                src_planes,
                                                src_samples,
                                                dst_planes,
                                                &next_samples);
        }
        if (st != AVP_OK) {
            return st;
        }

        src_fmt = dst_fmt;
        src_samples = next_samples;
        src_in = (const void *const *)tmp->planes;
    }

    if (src_fmt.sample_layout != ctx->out_fmt.sample_layout) {
        avp_swr_buffer_t *tmp = avp_swr_next_tmp(ctx, &tmp_index);
        avp_audio_format_t dst_fmt = src_fmt;

        dst_fmt.sample_layout = ctx->out_fmt.sample_layout;
        st = avp_swr_buffer_prepare(tmp, &dst_fmt, src_samples);
        if (st != AVP_OK) {
            return st;
        }

        if (src_fmt.sample_layout == AVP_SAMPLE_LAYOUT_INTERLEAVED) {
            const void *src_data = src_in[0];
            void *const *dst_planes = (void *const *)tmp->planes;

            st = avp_swr_interleave2planar(src_fmt.channels,
                                           src_fmt.bits_per_sample,
                                           src_data,
                                           dst_planes,
                                           src_samples);
        } else {
            const void *const *src_planes = src_in;
            void *dst_data = tmp->planes[0];

            st = avp_swr_planar2interleave(src_fmt.channels,
                                           src_fmt.bits_per_sample,
                                           src_planes,
                                           dst_data,
                                           src_samples);
        }
        if (st != AVP_OK) {
            return st;
        }

        src_fmt = dst_fmt;
        src_in = (const void *const *)tmp->planes;
    }

    st = avp_audio_copy(&src_fmt, src_in, out, src_samples);
    if (st != AVP_OK) {
        return st;
    }

    return (int)src_samples;
}
