/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef OPUS_CODEC_H
#define OPUS_CODEC_H

#include "audio_codec_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OPUS_DECODE_HEAD_SIZE          19u
#define OPUS_CODEC_MAGIC_HEAD          "OpusHead"
#define OPUS_CODEC_MAGIC_TAGS          "OpusTags"
#define OPUS_CODEC_MAGIC_SIZE          8u
#define OPUS_CODEC_GRANULE_SAMPLE_RATE 48000u
#define OPUS_CODEC_MAX_CHANNELS        2u
#define OPUS_CODEC_MAX_PACKET_SAMPLES  5760u

typedef struct {
    uint32_t sample_rate;
    uint16_t pre_skip;
    uint8_t channels;
    uint8_t stream_count;
    uint8_t coupled_count;
    uint8_t mapping_family;
    uint8_t mapping[OPUS_CODEC_MAX_CHANNELS];
} opus_header_info_t;

typedef struct {
    uint32_t sample_rate;
    uint8_t channels;
} opus_dec_config_t;

/**
 * \brief Open and initialize the context.
 * \param config Parameter config.
 * \return Decoder handle value.
 */
audio_codec_dec_handle_t opus_pcm_decode_open(const opus_dec_config_t *config);
/**
 * \brief Close the context and release resources.
 * \param handle Parameter handle.
 */
void opus_pcm_decode_close(audio_codec_dec_handle_t handle);
/**
 * \brief Decode one input frame into PCM output.
 * \param handle Parameter handle.
 * \param in_frame Parameter in_frame.
 * \param out_frame Parameter out_frame.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t opus_pcm_decode_frame(audio_codec_dec_handle_t handle,
                                   audio_codec_dec_in_frame_t *in_frame,
                                   audio_codec_dec_out_frame_t *out_frame);

#ifdef __cplusplus
}
#endif

#endif
