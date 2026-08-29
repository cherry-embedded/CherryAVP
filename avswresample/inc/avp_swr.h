/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AVP_SWR_H
#define AVP_SWR_H

#include "avp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *avp_sw_sample_t;

typedef enum {
    AVP_SAMPLE_LAYOUT_INTERLEAVED = 0,
    AVP_SAMPLE_LAYOUT_PLANAR,
} avp_sample_layout_t;

typedef struct {
    uint32_t sample_rate;
    uint16_t bits_per_sample;
    uint8_t channels;
    avp_sample_layout_t sample_layout;
} avp_audio_format_t;

static inline uint32_t avp_swr_get_out_samples(uint32_t in_rate,
                                               uint32_t out_rate,
                                               uint32_t in_samples)
{
    if (in_rate == 0u || out_rate == 0u || in_samples == 0u) {
        return 0u;
    }

    if (in_rate == out_rate) {
        return in_samples;
    }

    return (uint32_t)(((uint64_t)in_samples * out_rate + in_rate - 1u) /
                      in_rate);
}

static inline uint8_t avp_swr_get_sample_bytes(uint16_t bits_per_sample)
{
    switch (bits_per_sample) {
        case 8u:
            return 1u;
        case 16u:
            return 2u;
        case 24u:
            return 3u;
        case 32u:
            return 4u;
        default:
            return 0u;
    }
}

/**
 * \brief Convert one input buffer to the requested output format.
 * \param src_bits_per_sample Parameter src_bits_per_sample.
 * \param dst_bits_per_sample Parameter dst_bits_per_sample.
 * \param channels Parameter channels.
 * \param in Parameter in.
 * \param out Parameter out.
 * \param samples Parameter samples.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t avp_bits_convert_interleaved(uint16_t src_bits_per_sample,
                                          uint16_t dst_bits_per_sample,
                                          uint8_t channels,
                                          avp_sw_sample_t in,
                                          avp_sw_sample_t out,
                                          uint32_t samples);
/**
 * \brief Convert one input buffer to the requested output format.
 * \param src_bits_per_sample Parameter src_bits_per_sample.
 * \param dst_bits_per_sample Parameter dst_bits_per_sample.
 * \param channels Parameter channels.
 * \param in Parameter in.
 * \param out Parameter out.
 * \param samples Parameter samples.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t avp_bits_convert_planar(uint16_t src_bits_per_sample,
                                     uint16_t dst_bits_per_sample,
                                     uint8_t channels,
                                     avp_sw_sample_t in[],
                                     avp_sw_sample_t out[],
                                     uint32_t samples);

/**
 * \brief Convert one input buffer to the requested output format.
 * \param src_channels Parameter src_channels.
 * \param dst_channels Parameter dst_channels.
 * \param bits_per_sample Parameter bits_per_sample.
 * \param in Parameter in.
 * \param out Parameter out.
 * \param samples Parameter samples.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t avp_channel_convert_interleaved(uint8_t src_channels,
                                             uint8_t dst_channels,
                                             uint16_t bits_per_sample,
                                             avp_sw_sample_t in,
                                             avp_sw_sample_t out,
                                             uint32_t samples);
/**
 * \brief Convert one input buffer to the requested output format.
 * \param src_channels Parameter src_channels.
 * \param dst_channels Parameter dst_channels.
 * \param bits_per_sample Parameter bits_per_sample.
 * \param in Parameter in.
 * \param out Parameter out.
 * \param samples Parameter samples.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t avp_channel_convert_planar(uint8_t src_channels,
                                        uint8_t dst_channels,
                                        uint16_t bits_per_sample,
                                        avp_sw_sample_t in[],
                                        avp_sw_sample_t out[],
                                        uint32_t samples);

/**
 * \brief Convert one input buffer to the requested output format.
 * \param src_sample_rate Parameter src_sample_rate.
 * \param dst_sample_rate Parameter dst_sample_rate.
 * \param channels Parameter channels.
 * \param bits_per_sample Parameter bits_per_sample.
 * \param in Parameter in.
 * \param in_samples Parameter in_samples.
 * \param out Parameter out.
 * \param out_samples Parameter out_samples.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t avp_sample_rate_convert_interleaved(uint32_t src_sample_rate,
                                                 uint32_t dst_sample_rate,
                                                 uint8_t channels,
                                                 uint16_t bits_per_sample,
                                                 avp_sw_sample_t in,
                                                 uint32_t in_samples,
                                                 avp_sw_sample_t out,
                                                 uint32_t *out_samples);
/**
 * \brief Convert one input buffer to the requested output format.
 * \param src_sample_rate Parameter src_sample_rate.
 * \param dst_sample_rate Parameter dst_sample_rate.
 * \param channels Parameter channels.
 * \param bits_per_sample Parameter bits_per_sample.
 * \param in Parameter in.
 * \param in_samples Parameter in_samples.
 * \param out Parameter out.
 * \param out_samples Parameter out_samples.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t avp_sample_rate_convert_planar(uint32_t src_sample_rate,
                                            uint32_t dst_sample_rate,
                                            uint8_t channels,
                                            uint16_t bits_per_sample,
                                            avp_sw_sample_t in[],
                                            uint32_t in_samples,
                                            avp_sw_sample_t out[],
                                            uint32_t *out_samples);

/**
 * \brief Perform this API operation.
 * \param channels Parameter channels.
 * \param bits_per_sample Parameter bits_per_sample.
 * \param in Parameter in.
 * \param out Parameter out.
 * \param samples Parameter samples.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t avp_swr_planar2interleave(uint8_t channels,
                                       uint16_t bits_per_sample,
                                       avp_sw_sample_t in[],
                                       avp_sw_sample_t out,
                                       uint32_t samples);
/**
 * \brief Perform this API operation.
 * \param channels Parameter channels.
 * \param bits_per_sample Parameter bits_per_sample.
 * \param in Parameter in.
 * \param out Parameter out.
 * \param samples Parameter samples.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t avp_swr_interleave2planar(uint8_t channels,
                                       uint16_t bits_per_sample,
                                       avp_sw_sample_t in,
                                       avp_sw_sample_t out[],
                                       uint32_t samples);

#ifdef __cplusplus
}
#endif

#endif
