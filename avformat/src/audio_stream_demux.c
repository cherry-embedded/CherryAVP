/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "audio_stream_demux.h"

static void audio_stream_demux_container_close(audio_stream_demux_t *demuxer)
{
    if (demuxer == NULL) {
        return;
    }
    switch (demuxer->kind) {
#if defined(CONFIG_CHERRYAVP_MP3)
        case AUDIO_STREAM_DEMUX_MP3:
            mp3_demux_close(&demuxer->container.mp3);
            break;
#endif
#if defined(CONFIG_CHERRYAVP_AAC)
        case AUDIO_STREAM_DEMUX_AAC:
            aac_demux_close(&demuxer->container.aac);
            break;
#endif
#if defined(CONFIG_CHERRYAVP_AMR)
        case AUDIO_STREAM_DEMUX_AMR:
            amr_demux_close(&demuxer->container.amr);
            break;
#endif
#if defined(CONFIG_CHERRYAVP_FLAC)
        case AUDIO_STREAM_DEMUX_FLAC:
            flac_demux_close(&demuxer->container.flac);
            break;
#endif
#if defined(CONFIG_CHERRYAVP_ALAC)
        case AUDIO_STREAM_DEMUX_ALAC:
            alac_demux_close(&demuxer->container.alac);
            break;
#endif
#if defined(CONFIG_CHERRYAVP_WAV)
        case AUDIO_STREAM_DEMUX_WAV:
            wav_demux_close(&demuxer->container.wav);
            break;
#endif
#if defined(CONFIG_CHERRYAVP_M4A)
        case AUDIO_STREAM_DEMUX_M4A:
            m4a_demux_close(&demuxer->container.m4a);
            break;
#endif
#if defined(CONFIG_CHERRYAVP_OGG)
        case AUDIO_STREAM_DEMUX_OGG:
            ogg_demux_close(&demuxer->container.ogg);
            break;
#endif
        default:
            break;
    }
}

static avp_status_t audio_stream_demux_get_info(audio_stream_demux_t *demuxer,
                                                audio_stream_demux_info_t *info)
{
    if (demuxer == NULL || info == NULL) {
        return AVP_EINVAL;
    }
    memset(info, 0, sizeof(*info));

    switch (demuxer->kind) {
#if defined(CONFIG_CHERRYAVP_MP3)
        case AUDIO_STREAM_DEMUX_MP3:
            info->stream_offset = demuxer->container.mp3.common.stream_offset;
            info->stream_size = demuxer->container.mp3.common.stream_size;
            return mp3_demux_get_audio_stream_config(&demuxer->container.mp3,
                                                     &info->config);
#endif
#if defined(CONFIG_CHERRYAVP_AAC)
        case AUDIO_STREAM_DEMUX_AAC:
            info->stream_offset = demuxer->container.aac.common.stream_offset;
            info->stream_size = demuxer->container.aac.common.stream_size;
            return aac_demux_get_audio_stream_config(&demuxer->container.aac,
                                                     &info->config);
#endif
#if defined(CONFIG_CHERRYAVP_AMR)
        case AUDIO_STREAM_DEMUX_AMR:
            info->stream_offset = demuxer->container.amr.common.stream_offset;
            info->stream_size = demuxer->container.amr.common.stream_size;
            return amr_demux_get_audio_stream_config(&demuxer->container.amr,
                                                     &info->config);
#endif
#if defined(CONFIG_CHERRYAVP_FLAC)
        case AUDIO_STREAM_DEMUX_FLAC:
            info->stream_offset = demuxer->container.flac.stream_offset;
            info->stream_size = demuxer->container.flac.stream_size;
            return flac_demux_get_audio_stream_config(&demuxer->container.flac,
                                                      &info->config);
#endif
#if defined(CONFIG_CHERRYAVP_ALAC)
        case AUDIO_STREAM_DEMUX_ALAC:
            info->stream_offset = demuxer->container.alac.common.stream_offset;
            info->stream_size = demuxer->container.alac.common.stream_size;
            return alac_demux_get_audio_stream_config(&demuxer->container.alac,
                                                      &info->config);
#endif
#if defined(CONFIG_CHERRYAVP_WAV)
        case AUDIO_STREAM_DEMUX_WAV:
            info->stream_offset = demuxer->container.wav.common.stream_offset;
            info->stream_size = demuxer->container.wav.common.stream_size;
            return wav_demux_get_audio_stream_config(&demuxer->container.wav,
                                                     &info->config);
#endif
#if defined(CONFIG_CHERRYAVP_M4A)
        case AUDIO_STREAM_DEMUX_M4A:
            info->stream_offset = demuxer->container.m4a.common.stream_offset;
            info->stream_size = demuxer->container.m4a.common.stream_size;
            return m4a_demux_get_audio_stream_config(&demuxer->container.m4a,
                                                     &info->config);
#endif
#if defined(CONFIG_CHERRYAVP_OGG)
        case AUDIO_STREAM_DEMUX_OGG:
            info->stream_offset = demuxer->container.ogg.common.stream_offset;
            info->stream_size = demuxer->container.ogg.common.stream_size;
            return ogg_demux_get_audio_stream_config(&demuxer->container.ogg,
                                                     &info->config);
#endif
        default:
            return AVP_EUNSUPPORTED;
    }
}

static int audio_stream_demux_select(const char *format,
                                     audio_stream_demux_kind_t *kind)
{
    if (format == NULL || kind == NULL) {
        return 0;
    }
    if (strcmp(format, "mp3") == 0) {
        *kind = AUDIO_STREAM_DEMUX_MP3;
    } else if (strcmp(format, "aac") == 0) {
        *kind = AUDIO_STREAM_DEMUX_AAC;
    } else if (strcmp(format, "amr") == 0) {
        *kind = AUDIO_STREAM_DEMUX_AMR;
    } else if (strcmp(format, "flac") == 0) {
        *kind = AUDIO_STREAM_DEMUX_FLAC;
    } else if (strcmp(format, "alac") == 0 || strcmp(format, "caf") == 0) {
        *kind = AUDIO_STREAM_DEMUX_ALAC;
    } else if (strcmp(format, "wav") == 0) {
        *kind = AUDIO_STREAM_DEMUX_WAV;
    } else if (strcmp(format, "m4a") == 0) {
        *kind = AUDIO_STREAM_DEMUX_M4A;
    } else if (strcmp(format, "ogg") == 0 || strcmp(format, "opus") == 0 ||
               strcmp(format, "vorbis") == 0) {
        *kind = AUDIO_STREAM_DEMUX_OGG;
    } else {
        return 0;
    }
    return 1;
}

avp_status_t audio_stream_demux_open(const char *format,
                                     audio_stream_demux_t *demuxer,
                                     avp_io_t *avp_io,
                                     audio_stream_demux_info_t *info)
{
    avp_status_t st;

    if (format == NULL || demuxer == NULL || avp_io == NULL || info == NULL) {
        return AVP_EINVAL;
    }
    memset(info, 0, sizeof(*info));
    memset(demuxer, 0, sizeof(*demuxer));
    demuxer->avp_io = avp_io;
    if (!audio_stream_demux_select(format, &demuxer->kind)) {
        return AVP_EUNSUPPORTED;
    }

    switch (demuxer->kind) {
#if defined(CONFIG_CHERRYAVP_MP3)
        case AUDIO_STREAM_DEMUX_MP3:
            st = mp3_demux_open(&demuxer->container.mp3, avp_io);
            break;
#endif
#if defined(CONFIG_CHERRYAVP_AAC)
        case AUDIO_STREAM_DEMUX_AAC:
            st = aac_demux_open(&demuxer->container.aac, avp_io);
            break;
#endif
#if defined(CONFIG_CHERRYAVP_AMR)
        case AUDIO_STREAM_DEMUX_AMR:
            st = amr_demux_open(&demuxer->container.amr, avp_io);
            break;
#endif
#if defined(CONFIG_CHERRYAVP_FLAC)
        case AUDIO_STREAM_DEMUX_FLAC:
            st = flac_demux_open(&demuxer->container.flac, avp_io);
            break;
#endif
#if defined(CONFIG_CHERRYAVP_ALAC)
        case AUDIO_STREAM_DEMUX_ALAC:
            st = alac_demux_open(&demuxer->container.alac, avp_io);
            break;
#endif
#if defined(CONFIG_CHERRYAVP_WAV)
        case AUDIO_STREAM_DEMUX_WAV:
            st = wav_demux_open(&demuxer->container.wav, avp_io);
            break;
#endif
#if defined(CONFIG_CHERRYAVP_M4A)
        case AUDIO_STREAM_DEMUX_M4A:
            st = m4a_demux_open(&demuxer->container.m4a, avp_io);
            break;
#endif
#if defined(CONFIG_CHERRYAVP_OGG)
        case AUDIO_STREAM_DEMUX_OGG:
            st = ogg_demux_open(&demuxer->container.ogg, avp_io);
            break;
#endif
        default:
            st = AVP_EUNSUPPORTED;
            break;
    }
    if (st != AVP_OK) {
        return st;
    }
    st = audio_stream_demux_get_info(demuxer, &demuxer->info);
    if (st != AVP_OK) {
        return st;
    }
    *info = demuxer->info;
    return AVP_OK;
}

avp_status_t audio_stream_demux_read_packet(audio_stream_demux_t *demuxer,
                                            avp_packet_t *packet)
{
    if (demuxer == NULL || packet == NULL) {
        return AVP_EINVAL;
    }

    switch (demuxer->kind) {
#if defined(CONFIG_CHERRYAVP_MP3)
        case AUDIO_STREAM_DEMUX_MP3:
            return mp3_demux_read_packet(&demuxer->container.mp3, packet);
#endif
#if defined(CONFIG_CHERRYAVP_AAC)
        case AUDIO_STREAM_DEMUX_AAC:
            return aac_demux_read_packet(&demuxer->container.aac, packet);
#endif
#if defined(CONFIG_CHERRYAVP_AMR)
        case AUDIO_STREAM_DEMUX_AMR:
            return amr_demux_read_packet(&demuxer->container.amr, packet);
#endif
#if defined(CONFIG_CHERRYAVP_FLAC)
        case AUDIO_STREAM_DEMUX_FLAC:
            return flac_demux_read_packet(&demuxer->container.flac, packet);
#endif
#if defined(CONFIG_CHERRYAVP_ALAC)
        case AUDIO_STREAM_DEMUX_ALAC:
            return alac_demux_read_packet(&demuxer->container.alac, packet);
#endif
#if defined(CONFIG_CHERRYAVP_WAV)
        case AUDIO_STREAM_DEMUX_WAV:
            return wav_demux_read_packet(&demuxer->container.wav, packet);
#endif
#if defined(CONFIG_CHERRYAVP_M4A)
        case AUDIO_STREAM_DEMUX_M4A:
            return m4a_demux_read_packet(&demuxer->container.m4a, packet);
#endif
#if defined(CONFIG_CHERRYAVP_OGG)
        case AUDIO_STREAM_DEMUX_OGG:
            return ogg_demux_read_packet(&demuxer->container.ogg, packet);
#endif
        default:
            return AVP_EUNSUPPORTED;
    }
}

void audio_stream_demux_close(audio_stream_demux_t *demuxer)
{
    if (demuxer == NULL) {
        return;
    }
    audio_stream_demux_container_close(demuxer);
}
