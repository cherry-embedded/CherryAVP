/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "avp_swr.h"

#define AVP_RESAMPLE_TMP_COUNT 4u

typedef struct {
    avp_audio_format_t fmt;
    uint8_t **planes;
    uint8_t plane_count;
    uint32_t capacity_samples;
} avp_audio_buffer_t;

struct avp_swr_context {
    avp_audio_format_t in_fmt;
    avp_audio_format_t out_fmt;
    avp_audio_buffer_t tmp[AVP_RESAMPLE_TMP_COUNT];
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

static inline uint8_t avp_swr_get_plane_count(const avp_audio_format_t *fmt)
{
    return fmt->sample_layout == AVP_SAMPLE_LAYOUT_PLANAR ? fmt->channels : 1u;
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

static inline int avp_swr_check_input(const avp_audio_format_t *fmt,
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

static inline int avp_swr_check_output(const avp_audio_format_t *fmt,
                                       void *const data[])
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

static inline void avp_audio_buffer_free(avp_audio_buffer_t *buffer)
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

static inline avp_status_t avp_audio_buffer_prepare(avp_audio_buffer_t *buffer,
                                                    const avp_audio_format_t *fmt,
                                                    uint32_t samples)
{
    uint8_t plane_count;
    uint8_t plane;

    if (buffer == NULL || avp_swr_check_format(fmt) != AVP_OK) {
        return AVP_EINVAL;
    }

    plane_count = avp_swr_get_plane_count(fmt);
    if (buffer->planes != NULL &&
        buffer->capacity_samples >= samples &&
        buffer->plane_count == plane_count &&
        buffer->fmt.channels == fmt->channels &&
        buffer->fmt.bits_per_sample == fmt->bits_per_sample &&
        buffer->fmt.sample_layout == fmt->sample_layout) {
        buffer->fmt = *fmt;
        return AVP_OK;
    }

    avp_audio_buffer_free(buffer);

    buffer->planes = (uint8_t **)avp_calloc(plane_count, sizeof(buffer->planes[0]));
    if (buffer->planes == NULL) {
        return AVP_ENOMEM;
    }
    buffer->plane_count = plane_count;
    buffer->fmt = *fmt;
    buffer->capacity_samples = samples;

    for (plane = 0u; plane < plane_count; plane++) {
        size_t plane_size;
        avp_status_t st;

        st = avp_swr_get_plane_size(fmt, samples, plane, &plane_size);
        if (st != AVP_OK) {
            avp_audio_buffer_free(buffer);
            return st;
        }

        if (plane_size != 0u) {
            buffer->planes[plane] = (uint8_t *)avp_malloc(plane_size);
            if (buffer->planes[plane] == NULL) {
                avp_audio_buffer_free(buffer);
                return AVP_ENOMEM;
            }
        }
    }

    return AVP_OK;
}

static inline const uint8_t *avp_swr_get_interleaved_input_sample_ptr(const void *data,
                                                                      uint32_t sample_idx,
                                                                      uint8_t ch,
                                                                      uint8_t channels,
                                                                      uint16_t bits_per_sample)
{
    uint8_t bytes = avp_swr_get_sample_bytes(bits_per_sample);

    return (const uint8_t *)data + ((size_t)sample_idx * channels + ch) * bytes;
}

static inline uint8_t *avp_swr_get_interleaved_output_sample_ptr(void *data,
                                                                 uint32_t sample_idx,
                                                                 uint8_t ch,
                                                                 uint8_t channels,
                                                                 uint16_t bits_per_sample)
{
    uint8_t bytes = avp_swr_get_sample_bytes(bits_per_sample);

    return (uint8_t *)data + ((size_t)sample_idx * channels + ch) * bytes;
}

static inline const uint8_t *avp_swr_get_planar_input_sample_ptr(const void *const data[],
                                                                 uint32_t sample_idx,
                                                                 uint8_t ch,
                                                                 uint16_t bits_per_sample)
{
    uint8_t bytes = avp_swr_get_sample_bytes(bits_per_sample);

    return (const uint8_t *)data[ch] + (size_t)sample_idx * bytes;
}

static inline uint8_t *avp_swr_get_planar_output_sample_ptr(void *const data[],
                                                            uint32_t sample_idx,
                                                            uint8_t ch,
                                                            uint16_t bits_per_sample)
{
    uint8_t bytes = avp_swr_get_sample_bytes(bits_per_sample);

    return (uint8_t *)data[ch] + (size_t)sample_idx * bytes;
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

static inline int32_t avp_swr_read_sample_q31(const uint8_t *sample,
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

static inline void avp_swr_write_sample_q31(uint8_t *dst,
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

static inline int32_t avp_swr_read_interleaved_sample_q31(const void *data,
                                                          uint32_t sample_idx,
                                                          uint8_t ch,
                                                          uint8_t channels,
                                                          uint16_t bits_per_sample)
{
    return avp_swr_read_sample_q31(avp_swr_get_interleaved_input_sample_ptr(data,
                                                                            sample_idx,
                                                                            ch,
                                                                            channels,
                                                                            bits_per_sample),
                                   bits_per_sample);
}

static inline void avp_swr_write_interleaved_sample_q31(void *data,
                                                        uint32_t sample_idx,
                                                        uint8_t ch,
                                                        uint8_t channels,
                                                        uint16_t bits_per_sample,
                                                        int32_t sample)
{
    avp_swr_write_sample_q31(avp_swr_get_interleaved_output_sample_ptr(data,
                                                                       sample_idx,
                                                                       ch,
                                                                       channels,
                                                                       bits_per_sample),
                             bits_per_sample,
                             sample);
}

static inline int32_t avp_swr_read_planar_sample_q31(const void *const data[],
                                                     uint32_t sample_idx,
                                                     uint8_t ch,
                                                     uint16_t bits_per_sample)
{
    return avp_swr_read_sample_q31(avp_swr_get_planar_input_sample_ptr(data,
                                                                       sample_idx,
                                                                       ch,
                                                                       bits_per_sample),
                                   bits_per_sample);
}

static inline void avp_swr_write_planar_sample_q31(void *const data[],
                                                   uint32_t sample_idx,
                                                   uint8_t ch,
                                                   uint16_t bits_per_sample,
                                                   int32_t sample)
{
    avp_swr_write_sample_q31(avp_swr_get_planar_output_sample_ptr(data,
                                                                  sample_idx,
                                                                  ch,
                                                                  bits_per_sample),
                             bits_per_sample,
                             sample);
}

static inline int32_t avp_lerp_q31(int32_t a, int32_t b, uint32_t frac, uint32_t div)
{
    int64_t delta;

    if (frac == 0u || div == 0u) {
        return a;
    }

    delta = (int64_t)b - a;
    return avp_clip_s32((int64_t)a + (delta * frac) / div);
}

static inline avp_status_t avp_audio_copy(const avp_audio_format_t *fmt,
                                          const void *const in[],
                                          void *const out[],
                                          uint32_t samples)
{
    uint8_t planes;
    uint8_t plane;

    if (!avp_swr_check_input(fmt, in) ||
        !avp_swr_check_output(fmt, out)) {
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

static inline int avp_swr_check_sample_params(uint8_t channels, uint16_t bits_per_sample)
{
    return channels != 0u &&
           avp_swr_get_sample_bytes(bits_per_sample) != 0u;
}

static inline int avp_swr_check_planar_input(const void *const data[], uint8_t channels)
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

static inline int avp_swr_check_planar_output(void *const data[], uint8_t channels)
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

static int32_t avp_swr_read_mixed_interleaved_channel_q31(const void *in,
                                                          uint32_t sample,
                                                          uint8_t out_ch,
                                                          uint8_t src_channels,
                                                          uint8_t dst_channels,
                                                          uint16_t bits_per_sample)
{
    uint8_t ch;
    int64_t mixed;

    if (src_channels == 1u) {
        return avp_swr_read_interleaved_sample_q31(in, sample, 0u, src_channels, bits_per_sample);
    }

    if (dst_channels == 1u) {
        mixed = 0;
        for (ch = 0u; ch < src_channels; ch++) {
            mixed += avp_swr_read_interleaved_sample_q31(in,
                                                         sample,
                                                         ch,
                                                         src_channels,
                                                         bits_per_sample);
        }
        return avp_clip_s32(mixed / src_channels);
    }

    if (out_ch < src_channels) {
        return avp_swr_read_interleaved_sample_q31(in,
                                                   sample,
                                                   out_ch,
                                                   src_channels,
                                                   bits_per_sample);
    }

    mixed = 0;
    for (ch = 0u; ch < src_channels; ch++) {
        mixed += avp_swr_read_interleaved_sample_q31(in,
                                                     sample,
                                                     ch,
                                                     src_channels,
                                                     bits_per_sample);
    }
    return avp_clip_s32(mixed / src_channels);
}

static int32_t avp_swr_read_mixed_planar_channel_q31(const void *const in[],
                                                     uint32_t sample,
                                                     uint8_t out_ch,
                                                     uint8_t src_channels,
                                                     uint8_t dst_channels,
                                                     uint16_t bits_per_sample)
{
    uint8_t ch;
    int64_t mixed;

    if (src_channels == 1u) {
        return avp_swr_read_planar_sample_q31(in, sample, 0u, bits_per_sample);
    }

    if (dst_channels == 1u) {
        mixed = 0;
        for (ch = 0u; ch < src_channels; ch++) {
            mixed += avp_swr_read_planar_sample_q31(in, sample, ch, bits_per_sample);
        }
        return avp_clip_s32(mixed / src_channels);
    }

    if (out_ch < src_channels) {
        return avp_swr_read_planar_sample_q31(in, sample, out_ch, bits_per_sample);
    }

    mixed = 0;
    for (ch = 0u; ch < src_channels; ch++) {
        mixed += avp_swr_read_planar_sample_q31(in, sample, ch, bits_per_sample);
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

    if (!avp_swr_check_sample_params(channels, src_bits_per_sample) ||
        avp_swr_get_sample_bytes(dst_bits_per_sample) == 0u ||
        in == NULL ||
        out == NULL) {
        return AVP_EINVAL;
    }

    for (sample = 0u; sample < samples; sample++) {
        for (ch = 0u; ch < channels; ch++) {
            avp_swr_write_interleaved_sample_q31(out,
                                                 sample,
                                                 ch,
                                                 channels,
                                                 dst_bits_per_sample,
                                                 avp_swr_read_interleaved_sample_q31(in,
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

    if (!avp_swr_check_sample_params(channels, src_bits_per_sample) ||
        avp_swr_get_sample_bytes(dst_bits_per_sample) == 0u ||
        !avp_swr_check_planar_input(in, channels) ||
        !avp_swr_check_planar_output(out, channels)) {
        return AVP_EINVAL;
    }

    for (ch = 0u; ch < channels; ch++) {
        for (sample = 0u; sample < samples; sample++) {
            avp_swr_write_planar_sample_q31(out,
                                            sample,
                                            ch,
                                            dst_bits_per_sample,
                                            avp_swr_read_planar_sample_q31(in,
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

    if (!avp_swr_check_sample_params(src_channels, bits_per_sample) ||
        dst_channels == 0u ||
        in == NULL ||
        out == NULL) {
        return AVP_EINVAL;
    }

    for (sample = 0u; sample < samples; sample++) {
        for (ch = 0u; ch < dst_channels; ch++) {
            avp_swr_write_interleaved_sample_q31(out,
                                                 sample,
                                                 ch,
                                                 dst_channels,
                                                 bits_per_sample,
                                                 avp_swr_read_mixed_interleaved_channel_q31(in,
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

    if (!avp_swr_check_sample_params(src_channels, bits_per_sample) ||
        dst_channels == 0u ||
        !avp_swr_check_planar_input(in, src_channels) ||
        !avp_swr_check_planar_output(out, dst_channels)) {
        return AVP_EINVAL;
    }

    for (ch = 0u; ch < dst_channels; ch++) {
        for (sample = 0u; sample < samples; sample++) {
            avp_swr_write_planar_sample_q31(out,
                                            sample,
                                            ch,
                                            bits_per_sample,
                                            avp_swr_read_mixed_planar_channel_q31(in,
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

    if (src_sample_rate == 0u ||
        dst_sample_rate == 0u ||
        !avp_swr_check_sample_params(channels, bits_per_sample) ||
        in == NULL ||
        out == NULL ||
        out_samples == NULL ||
        in_samples == 0u) {
        return AVP_EINVAL;
    }

    produced = avp_swr_get_out_samples(src_sample_rate,
                                       dst_sample_rate,
                                       in_samples);

    if (src_sample_rate == dst_sample_rate || produced == in_samples) {
        memmove(out,
                in,
                (size_t)in_samples * channels *
                    avp_swr_get_sample_bytes(bits_per_sample));
        *out_samples = in_samples;
        return AVP_OK;
    }

    for (sample = 0u; sample < produced; sample++) {
        uint64_t pos = (uint64_t)sample * src_sample_rate;
        uint32_t index = (uint32_t)(pos / dst_sample_rate);
        uint32_t frac = (uint32_t)(pos % dst_sample_rate);

        if (index >= in_samples) {
            index = in_samples - 1u;
            frac = 0u;
        }

        for (ch = 0u; ch < channels; ch++) {
            int32_t a = avp_swr_read_interleaved_sample_q31(in,
                                                            index,
                                                            ch,
                                                            channels,
                                                            bits_per_sample);
            int32_t b = index + 1u < in_samples ?
                            avp_swr_read_interleaved_sample_q31(in,
                                                                index + 1u,
                                                                ch,
                                                                channels,
                                                                bits_per_sample) :
                            a;

            avp_swr_write_interleaved_sample_q31(out,
                                                 sample,
                                                 ch,
                                                 channels,
                                                 bits_per_sample,
                                                 avp_lerp_q31(a, b, frac, dst_sample_rate));
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

    if (src_sample_rate == 0u ||
        dst_sample_rate == 0u ||
        !avp_swr_check_sample_params(channels, bits_per_sample) ||
        !avp_swr_check_planar_input(in, channels) ||
        !avp_swr_check_planar_output(out, channels) ||
        out_samples == NULL ||
        in_samples == 0u) {
        return AVP_EINVAL;
    }

    produced = avp_swr_get_out_samples(src_sample_rate,
                                       dst_sample_rate,
                                       in_samples);

    if (src_sample_rate == dst_sample_rate || produced == in_samples) {
        uint8_t bytes = avp_swr_get_sample_bytes(bits_per_sample);

        for (ch = 0u; ch < channels; ch++) {
            memmove(out[ch], in[ch], (size_t)in_samples * bytes);
        }
        *out_samples = in_samples;
        return AVP_OK;
    }

    for (ch = 0u; ch < channels; ch++) {
        for (sample = 0u; sample < produced; sample++) {
            uint64_t pos = (uint64_t)sample * src_sample_rate;
            uint32_t index = (uint32_t)(pos / dst_sample_rate);
            uint32_t frac = (uint32_t)(pos % dst_sample_rate);
            int32_t a;
            int32_t b;

            if (index >= in_samples) {
                index = in_samples - 1u;
                frac = 0u;
            }

            a = avp_swr_read_planar_sample_q31(in, index, ch, bits_per_sample);
            b = index + 1u < in_samples ?
                    avp_swr_read_planar_sample_q31(in, index + 1u, ch, bits_per_sample) :
                    a;
            avp_swr_write_planar_sample_q31(out,
                                            sample,
                                            ch,
                                            bits_per_sample,
                                            avp_lerp_q31(a, b, frac, dst_sample_rate));
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
    uint8_t bytes;

    if (!avp_swr_check_sample_params(channels, bits_per_sample) ||
        !avp_swr_check_planar_input(in, channels) ||
        out == NULL) {
        return AVP_EINVAL;
    }

    bytes = avp_swr_get_sample_bytes(bits_per_sample);
    for (sample = 0u; sample < samples; sample++) {
        for (ch = 0u; ch < channels; ch++) {
            memcpy((uint8_t *)out + ((size_t)sample * channels + ch) * bytes,
                   (const uint8_t *)in[ch] + (size_t)sample * bytes,
                   bytes);
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
    uint8_t bytes;

    if (!avp_swr_check_sample_params(channels, bits_per_sample) ||
        in == NULL ||
        !avp_swr_check_planar_output(out, channels)) {
        return AVP_EINVAL;
    }

    bytes = avp_swr_get_sample_bytes(bits_per_sample);
    for (sample = 0u; sample < samples; sample++) {
        for (ch = 0u; ch < channels; ch++) {
            memcpy((uint8_t *)out[ch] + (size_t)sample * bytes,
                   (const uint8_t *)in + ((size_t)sample * channels + ch) * bytes,
                   bytes);
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
        avp_audio_buffer_free(&ctx->tmp[i]);
    }
    avp_free(ctx);
}

static inline avp_audio_buffer_t *avp_swr_next_tmp(avp_swr_t *ctx,
                                                   uint8_t *index)
{
    avp_audio_buffer_t *buffer;

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
        !avp_swr_check_input(&ctx->in_fmt, in) ||
        !avp_swr_check_output(&ctx->out_fmt, out)) {
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
        avp_audio_buffer_t *tmp = avp_swr_next_tmp(ctx, &tmp_index);
        avp_audio_format_t dst_fmt = src_fmt;

        dst_fmt.bits_per_sample = ctx->out_fmt.bits_per_sample;
        st = avp_audio_buffer_prepare(tmp, &dst_fmt, src_samples);
        if (st != AVP_OK) {
            return st;
        }

        if (src_fmt.sample_layout == AVP_SAMPLE_LAYOUT_INTERLEAVED) {
            st = avp_bits_convert_interleaved(
                src_fmt.bits_per_sample,
                dst_fmt.bits_per_sample,
                src_fmt.channels,
                src_in[0],
                tmp->planes[0],
                src_samples);
        } else {
            st = avp_bits_convert_planar(
                src_fmt.bits_per_sample,
                dst_fmt.bits_per_sample,
                src_fmt.channels,
                src_in,
                (void *const *)tmp->planes,
                src_samples);
        }
        if (st != AVP_OK) {
            return st;
        }

        src_fmt = dst_fmt;
        src_in = (const void *const *)tmp->planes;
    }

    if (src_fmt.channels != ctx->out_fmt.channels) {
        avp_audio_buffer_t *tmp = avp_swr_next_tmp(ctx, &tmp_index);
        avp_audio_format_t dst_fmt = src_fmt;

        dst_fmt.channels = ctx->out_fmt.channels;
        st = avp_audio_buffer_prepare(tmp, &dst_fmt, src_samples);
        if (st != AVP_OK) {
            return st;
        }

        if (src_fmt.sample_layout == AVP_SAMPLE_LAYOUT_INTERLEAVED) {
            st = avp_channel_convert_interleaved(src_fmt.channels,
                                                 dst_fmt.channels,
                                                 src_fmt.bits_per_sample,
                                                 src_in[0],
                                                 tmp->planes[0],
                                                 src_samples);
        } else {
            st = avp_channel_convert_planar(src_fmt.channels,
                                            dst_fmt.channels,
                                            src_fmt.bits_per_sample,
                                            src_in,
                                            (void *const *)tmp->planes,
                                            src_samples);
        }
        if (st != AVP_OK) {
            return st;
        }

        src_fmt = dst_fmt;
        src_in = (const void *const *)tmp->planes;
    }

    if (src_fmt.sample_rate != ctx->out_fmt.sample_rate) {
        avp_audio_buffer_t *tmp = avp_swr_next_tmp(ctx, &tmp_index);
        avp_audio_format_t dst_fmt = src_fmt;
        uint32_t next_samples;

        dst_fmt.sample_rate = ctx->out_fmt.sample_rate;
        next_samples = avp_swr_get_out_samples(src_fmt.sample_rate,
                                               dst_fmt.sample_rate,
                                               src_samples);
        st = avp_audio_buffer_prepare(tmp, &dst_fmt, next_samples);
        if (st != AVP_OK) {
            return st;
        }

        if (src_fmt.sample_layout == AVP_SAMPLE_LAYOUT_INTERLEAVED) {
            st = avp_sample_rate_convert_interleaved(src_fmt.sample_rate,
                                                     dst_fmt.sample_rate,
                                                     src_fmt.channels,
                                                     src_fmt.bits_per_sample,
                                                     src_in[0],
                                                     src_samples,
                                                     tmp->planes[0],
                                                     &next_samples);
        } else {
            st = avp_sample_rate_convert_planar(src_fmt.sample_rate,
                                                dst_fmt.sample_rate,
                                                src_fmt.channels,
                                                src_fmt.bits_per_sample,
                                                src_in,
                                                src_samples,
                                                (void *const *)tmp->planes,
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
        avp_audio_buffer_t *tmp = avp_swr_next_tmp(ctx, &tmp_index);
        avp_audio_format_t dst_fmt = src_fmt;

        dst_fmt.sample_layout = ctx->out_fmt.sample_layout;
        st = avp_audio_buffer_prepare(tmp, &dst_fmt, src_samples);
        if (st != AVP_OK) {
            return st;
        }

        if (src_fmt.sample_layout == AVP_SAMPLE_LAYOUT_INTERLEAVED) {
            st = avp_swr_interleave2planar(src_fmt.channels,
                                           src_fmt.bits_per_sample,
                                           src_in[0],
                                           (void *const *)tmp->planes,
                                           src_samples);
        } else {
            st = avp_swr_planar2interleave(src_fmt.channels,
                                           src_fmt.bits_per_sample,
                                           src_in,
                                           tmp->planes[0],
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
