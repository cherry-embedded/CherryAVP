/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AUDIO_CODEC_H
#define AUDIO_CODEC_H

#include "audio_codec_common.h"
#include "pcm_codec.h"

#if defined(CONFIG_CHERRYAVP_AAC)
#include "aac_codec.h"
#endif
#if defined(CONFIG_CHERRYAVP_AMR)
#include "amr_codec.h"
#endif
#if defined(CONFIG_CHERRYAVP_FLAC)
#include "flac_codec.h"
#endif
#if defined(CONFIG_CHERRYAVP_ALAC)
#include "alac_codec.h"
#endif
#if defined(CONFIG_CHERRYAVP_MP3)
#include "mp3_codec.h"
#endif
#if defined(CONFIG_CHERRYAVP_OPUS)
#include "opus_codec.h"
#endif
#if defined(CONFIG_CHERRYAVP_VORBIS)
#include "vorbis_codec.h"
#endif
#if defined(CONFIG_CHERRYAVP_ADPCM)
#include "adpcm_codec.h"
#endif
#if defined(CONFIG_CHERRYAVP_G711)
#include "g711_codec.h"
#endif
#if defined(CONFIG_CHERRYAVP_G722)
#include "g722_codec.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t codec_type;
    pcm_dec_config_t pcm_config;
#if defined(CONFIG_CHERRYAVP_AAC)
    aac_dec_config_t aac_config;
#endif
#if defined(CONFIG_CHERRYAVP_AMR)
    amr_dec_config_t amr_config;
#endif
#if defined(CONFIG_CHERRYAVP_FLAC)
    flac_dec_config_t flac_config;
#endif
#if defined(CONFIG_CHERRYAVP_OPUS)
    opus_dec_config_t opus_config;
#endif
#if defined(CONFIG_CHERRYAVP_VORBIS)
    vorbis_dec_config_t vorbis_config;
#endif
#if defined(CONFIG_CHERRYAVP_ADPCM)
    adpcm_dec_config_t adpcm_config;
#endif
#if defined(CONFIG_CHERRYAVP_G711)
    g711_dec_config_t g711_config;
#endif
#if defined(CONFIG_CHERRYAVP_G722)
    g722_dec_config_t g722_config;
#endif
#if defined(CONFIG_CHERRYAVP_ALAC)
    alac_dec_config_t alac_config;
#endif
} audio_codec_dec_config_t;

typedef audio_codec_dec_in_frame_t audio_codec_dec_in_frame_t;
typedef audio_codec_dec_out_frame_t audio_codec_dec_out_frame_t;
typedef audio_codec_dec_handle_t audio_codec_dec_handle_t;

/**
 * \brief Open and initialize the context.
 * \param config Parameter config.
 * \return Decoder handle value.
 */
audio_codec_dec_handle_t audio_codec_dec_open(const audio_codec_dec_config_t *config);
/**
 * \brief Close the context and release resources.
 * \param handle Parameter handle.
 */
void audio_codec_dec_close(audio_codec_dec_handle_t handle);
/**
 * \brief Decode one input frame into PCM output.
 * \param handle Parameter handle.
 * \param in_frame Parameter in_frame.
 * \param out_frame Parameter out_frame.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t audio_codec_dec_frame(audio_codec_dec_handle_t handle,
                                          audio_codec_dec_in_frame_t *in_frame,
                                          audio_codec_dec_out_frame_t *out_frame);

#ifdef __cplusplus
}
#endif

#endif
