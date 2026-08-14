/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AUDIO_STREAM_DEMUX_H
#define AUDIO_STREAM_DEMUX_H

#if defined(CONFIG_CHERRYAVP_AAC)
#include "aac_container.h"
#endif
#if defined(CONFIG_CHERRYAVP_AMR)
#include "amr_container.h"
#endif
#if defined(CONFIG_CHERRYAVP_FLAC)
#include "flac_container.h"
#endif
#if defined(CONFIG_CHERRYAVP_ALAC)
#include "alac_container.h"
#endif
#if defined(CONFIG_CHERRYAVP_MP3)
#include "mp3_container.h"
#endif
#if defined(CONFIG_CHERRYAVP_WAV)
#include "wav_container.h"
#endif
#if defined(CONFIG_CHERRYAVP_M4A)
#include "m4a_container.h"
#endif
#if defined(CONFIG_CHERRYAVP_OGG)
#include "ogg_container.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AUDIO_STREAM_DEMUX_NONE = 0,
    AUDIO_STREAM_DEMUX_MP3,
    AUDIO_STREAM_DEMUX_AAC,
    AUDIO_STREAM_DEMUX_AMR,
    AUDIO_STREAM_DEMUX_FLAC,
    AUDIO_STREAM_DEMUX_ALAC,
    AUDIO_STREAM_DEMUX_WAV,
    AUDIO_STREAM_DEMUX_M4A,
    AUDIO_STREAM_DEMUX_OGG,
} audio_stream_demux_kind_t;

typedef struct {
    uint32_t stream_offset;
    uint32_t stream_size;
    audio_codec_dec_config_t config;
} audio_stream_demux_info_t;

typedef struct {
    avp_io_t *avp_io;
    audio_stream_demux_kind_t kind;
    audio_codec_dec_handle_t decoder;
    audio_stream_demux_info_t info;
    union {
#if defined(CONFIG_CHERRYAVP_MP3)
        mp3_demux_t mp3;
#endif
#if defined(CONFIG_CHERRYAVP_AAC)
        aac_demux_t aac;
#endif
#if defined(CONFIG_CHERRYAVP_AMR)
        amr_demux_t amr;
#endif
#if defined(CONFIG_CHERRYAVP_FLAC)
        flac_demux_t flac;
#endif
#if defined(CONFIG_CHERRYAVP_ALAC)
        alac_demux_t alac;
#endif
#if defined(CONFIG_CHERRYAVP_WAV)
        wav_demux_t wav;
#endif
#if defined(CONFIG_CHERRYAVP_M4A)
        m4a_demux_t m4a;
#endif
#if defined(CONFIG_CHERRYAVP_OGG)
        ogg_demux_t ogg;
#endif
    } container;
} audio_stream_demux_t;

/**
 * \brief Open and initialize the context.
 * \param format Parameter format.
 * \param demuxer Parameter demuxer.
 * \param avp_io Parameter avp_io.
 * \param info Parameter info.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t audio_stream_demux_open(const char *format,
                                     audio_stream_demux_t *demuxer,
                                     avp_io_t *avp_io,
                                     audio_stream_demux_info_t *info);
/**
 * \brief Read one packet from the stream.
 * \param demuxer Parameter demuxer.
 * \param packet Parameter packet.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t audio_stream_demux_read_packet(audio_stream_demux_t *demuxer,
                                            avp_packet_t *packet);
/**
 * \brief Close the context and release resources.
 * \param demuxer Parameter demuxer.
 */
void audio_stream_demux_close(audio_stream_demux_t *demuxer);

#ifdef __cplusplus
}
#endif

#endif
