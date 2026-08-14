/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AUDIO_DECODE_COMMON_H
#define AUDIO_DECODE_COMMON_H

#include "avp_packet.h"
#include "avp_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *audio_codec_dec_handle_t;

#define AUDIO_CODEC_FOURCC(a, b, c, d)                        \
    ((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) | \
     ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24))

#define AUDIO_CODEC_ID_IMA_ADPCM AUDIO_CODEC_FOURCC('I', 'M', 'A', ' ')
#define AUDIO_CODEC_ID_G711_ALAW AUDIO_CODEC_FOURCC('A', 'L', 'A', 'W')
#define AUDIO_CODEC_ID_G711_ULAW AUDIO_CODEC_FOURCC('U', 'L', 'A', 'W')
#define AUDIO_CODEC_ID_G722      AUDIO_CODEC_FOURCC('G', '7', '2', '2')
#define AUDIO_CODEC_ID_MP3       AUDIO_CODEC_FOURCC('M', 'P', '3', ' ')
#define AUDIO_CODEC_ID_AAC       AUDIO_CODEC_FOURCC('A', 'A', 'C', ' ')
#define AUDIO_CODEC_ID_AMR       AUDIO_CODEC_FOURCC('A', 'M', 'R', ' ')
#define AUDIO_CODEC_ID_OPUS      AUDIO_CODEC_FOURCC('O', 'P', 'U', 'S')
#define AUDIO_CODEC_ID_VORBIS    AUDIO_CODEC_FOURCC('V', 'O', 'R', 'B')
#define AUDIO_CODEC_ID_FLAC      AUDIO_CODEC_FOURCC('F', 'L', 'A', 'C')
#define AUDIO_CODEC_ID_ALAC      AUDIO_CODEC_FOURCC('A', 'L', 'A', 'C')
#define AUDIO_CODEC_ID_PCM       AUDIO_CODEC_FOURCC('P', 'C', 'M', ' ')

typedef struct {
    uint8_t *buffer;        /*< Input buffer containing encoded audio data */
    uint32_t size;          /*< Size of input buffer in bytes */
    uint32_t consumed_size; /*< Number of bytes consumed from input buffer */
} audio_codec_dec_in_frame_t;

typedef struct {
    int16_t *buffer;       /*< Decoded PCM data buffer */
    uint32_t size;         /*!< Decoded PCM data max size */
    uint32_t require_size; /*!< Set when output buffer size not enough */
    uint32_t pcm_size;     /*!< Decoded PCM data size */

    uint32_t sample_rate;         /*!< Sample rate in Hz */
    uint32_t bitrate;             /*!< Bitrate in kbps */
    uint32_t samples_per_channel; /*!< Number of samples per channel */
    uint32_t duration_ms;         /*!< Duration in milliseconds */
    uint8_t channels;             /*!< Number of channels */
    uint8_t bits_per_sample;      /*!< Bits per sample */
} audio_codec_dec_out_frame_t;

static inline uint32_t audio_codec_calc_bitrate_kbps(uint32_t frame_bytes,
                                                     uint32_t sample_rate,
                                                     uint32_t samples_per_channel)
{
    if (frame_bytes == 0u || sample_rate == 0u || samples_per_channel == 0u) {
        return 0u;
    }

    {
        uint32_t bitrate = ((uint64_t)frame_bytes * 8u * sample_rate) /
                           ((uint64_t)samples_per_channel * 1000u);

        return bitrate;
    }
}

static inline uint32_t audio_codec_calc_duration_ms(uint32_t samples_per_channel,
                                                    uint32_t sample_rate)
{
    if (samples_per_channel == 0u || sample_rate == 0u) {
        return 0u;
    }
    return (samples_per_channel * 1000u) / sample_rate;
}

#ifdef __cplusplus
}
#endif

#endif
