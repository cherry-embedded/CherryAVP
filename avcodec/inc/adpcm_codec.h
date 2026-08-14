/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ADPCM_CODEC_H
#define ADPCM_CODEC_H

#include "audio_codec_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ADPCM_IMA_MAX_CHANNELS 8u

typedef struct adpcm_dec_config {
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t block_align;
} adpcm_dec_config_t;

/**
 * \brief Open and initialize the context.
 * \param info Parameter info.
 * \return Decoder handle value.
 */
audio_codec_dec_handle_t adpcm_pcm_decode_open(const adpcm_dec_config_t *info);
/**
 * \brief Close the context and release resources.
 * \param handle Parameter handle.
 */
void adpcm_pcm_decode_close(audio_codec_dec_handle_t handle);
/**
 * \brief Decode one input frame into PCM output.
 * \param handle Parameter handle.
 * \param in_frame Parameter in_frame.
 * \param out_frame Parameter out_frame.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t adpcm_pcm_decode_frame(audio_codec_dec_handle_t handle,
                                    audio_codec_dec_in_frame_t *in_frame,
                                    audio_codec_dec_out_frame_t *out_frame);

#ifdef __cplusplus
}
#endif

#endif
