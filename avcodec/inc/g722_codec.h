/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef G722_CODEC_H
#define G722_CODEC_H

#include "audio_codec_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t sample_rate;
    uint32_t bitrate;
    uint8_t channels;
    uint8_t packed;
} g722_dec_config_t;

/**
 * \brief Open and initialize the context.
 * \param config Parameter config.
 * \return Decoder handle value.
 */
audio_codec_dec_handle_t g722_pcm_decode_open(const g722_dec_config_t *config);
/**
 * \brief Close the context and release resources.
 * \param handle Parameter handle.
 */
void g722_pcm_decode_close(audio_codec_dec_handle_t handle);
/**
 * \brief Decode one input frame into PCM output.
 * \param handle Parameter handle.
 * \param in_frame Parameter in_frame.
 * \param out_frame Parameter out_frame.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t g722_pcm_decode_frame(audio_codec_dec_handle_t handle,
                                   audio_codec_dec_in_frame_t *in_frame,
                                   audio_codec_dec_out_frame_t *out_frame);

#ifdef __cplusplus
}
#endif

#endif
