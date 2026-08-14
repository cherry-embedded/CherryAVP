/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef VORBIS_CODEC_H
#define VORBIS_CODEC_H

#include "audio_codec_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VORBIS_CODEC_MAGIC        "vorbis"
#define VORBIS_CODEC_MAGIC_SIZE   6u
#define VORBIS_DECODE_HEADER_SIZE 7u

typedef struct {
    uint32_t sample_rate;
    int32_t bitrate_upper;
    int32_t bitrate_nominal;
    int32_t bitrate_lower;
    uint8_t channels;
} vorbis_header_info_t;

typedef struct {
    const uint8_t *identification;
    uint32_t identification_size;
    const uint8_t *comment;
    uint32_t comment_size;
    const uint8_t *setup;
    uint32_t setup_size;
} vorbis_dec_config_t;

/**
 * \brief Open and initialize the context.
 * \param config Parameter config.
 * \return Decoder handle value.
 */
audio_codec_dec_handle_t vorbis_pcm_decode_open(const vorbis_dec_config_t *config);
/**
 * \brief Close the context and release resources.
 * \param handle Parameter handle.
 */
void vorbis_pcm_decode_close(audio_codec_dec_handle_t handle);
/**
 * \brief Decode one input frame into PCM output.
 * \param handle Parameter handle.
 * \param in_frame Parameter in_frame.
 * \param out_frame Parameter out_frame.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t vorbis_pcm_decode_frame(audio_codec_dec_handle_t handle,
                                     audio_codec_dec_in_frame_t *in_frame,
                                     audio_codec_dec_out_frame_t *out_frame);

#ifdef __cplusplus
}
#endif

#endif
