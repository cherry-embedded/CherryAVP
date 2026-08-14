/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MP4_CONTAINER_H
#define MP4_CONTAINER_H

#include "container_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t mp4_fourcc_t;

#define MP4_FOURCC(a, b, c, d)                                \
    ((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) | \
     ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24))

typedef enum {
    MP4_VIDEO_CODEC_UNKNOWN = 0,
    MP4_VIDEO_CODEC_RAW,
    MP4_VIDEO_CODEC_MJPEG,
    MP4_VIDEO_CODEC_PNG,
    MP4_VIDEO_CODEC_H263,
    MP4_VIDEO_CODEC_MPEG1,
    MP4_VIDEO_CODEC_MPEG2,
    MP4_VIDEO_CODEC_MPEG4,
    MP4_VIDEO_CODEC_H264,
    MP4_VIDEO_CODEC_HEVC,
    MP4_VIDEO_CODEC_VVC,
    MP4_VIDEO_CODEC_VP8,
    MP4_VIDEO_CODEC_VP9,
    MP4_VIDEO_CODEC_AV1,
    MP4_VIDEO_CODEC_DV,
    MP4_VIDEO_CODEC_PRORES
} mp4_video_codec_t;

typedef enum {
    MP4_AUDIO_CODEC_UNKNOWN = 0,
    MP4_AUDIO_CODEC_PCM,
    MP4_AUDIO_CODEC_AAC,
    MP4_AUDIO_CODEC_MP2,
    MP4_AUDIO_CODEC_MP3,
    MP4_AUDIO_CODEC_ALAC,
    MP4_AUDIO_CODEC_FLAC,
    MP4_AUDIO_CODEC_OPUS,
    MP4_AUDIO_CODEC_VORBIS,
    MP4_AUDIO_CODEC_AC3,
    MP4_AUDIO_CODEC_EAC3,
    MP4_AUDIO_CODEC_AC4,
    MP4_AUDIO_CODEC_DTS,
    MP4_AUDIO_CODEC_AMR_NB,
    MP4_AUDIO_CODEC_AMR_WB,
    MP4_AUDIO_CODEC_G711_ALAW,
    MP4_AUDIO_CODEC_G711_MULAW
} mp4_audio_codec_t;

typedef struct {
    uint32_t stream_index;
    mp4_video_codec_t codec_type;
    mp4_fourcc_t sample_entry;
    uint32_t width;
    uint32_t height;
    uint32_t timescale;
    uint64_t duration;
    uint32_t sample_count;
    uint32_t max_sample_size;
    uint32_t *sample_sizes;
    uint32_t *sample_offsets;
    uint32_t sample_index;
} mp4_video_stream_t;

typedef struct {
    uint32_t stream_index;
    mp4_audio_codec_t codec_type;
    mp4_fourcc_t sample_entry;
    uint32_t timescale;
    uint64_t duration;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    uint32_t sample_count;
    uint32_t max_sample_size;
    uint32_t *sample_sizes;
    uint32_t *sample_offsets;
    uint32_t sample_index;
    aac_dec_config_t aac_config;
    alac_dec_config_t alac_config;
} mp4_audio_stream_t;

typedef struct {
    container_common_t common;
    uint32_t stream_count;
    uint32_t has_video;
    uint32_t has_audio;
    mp4_video_stream_t video;
    mp4_audio_stream_t audio;
} mp4_demux_t;

#define mp4_get_file_size(demuxer)          ((demuxer)->common.file_size)
#define mp4_get_stream_count(demuxer)       ((demuxer)->stream_count)
#define mp4_get_video_sample_count(demuxer) ((demuxer)->video.sample_count)
#define mp4_get_audio_sample_count(demuxer) ((demuxer)->audio.sample_count)

/**
 * \brief Open and initialize the context.
 * \param demuxer Parameter demuxer.
 * \param avp_io Parameter avp_io.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t mp4_demux_open(mp4_demux_t *demuxer, avp_io_t *avp_io);
/**
 * \brief Close the context and release resources.
 * \param demuxer Parameter demuxer.
 */
void mp4_demux_close(mp4_demux_t *demuxer);
/**
 * \brief Get the audio stream decoder configuration.
 * \param demuxer Parameter demuxer.
 * \param config Parameter config.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t mp4_demux_get_audio_stream_config(const mp4_demux_t *demuxer,
                                               audio_codec_dec_config_t *config);
/**
 * \brief Peek one packet without consuming it.
 * \param demuxer Parameter demuxer.
 * \param packet Parameter packet.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t mp4_demux_peek_packet(mp4_demux_t *demuxer, avp_packet_t *packet);
/**
 * \brief Pop and consume one packet from the stream.
 * \param demuxer Parameter demuxer.
 * \param packet Parameter packet.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t mp4_demux_pop_packet(mp4_demux_t *demuxer, avp_packet_t *packet);
/**
 * \brief Rewind the stream position to the beginning.
 * \param demuxer Parameter demuxer.
 */
void mp4_demux_rewind(mp4_demux_t *demuxer);

/**
 * \brief Get a human-readable name string.
 * \param codec Parameter codec.
 * \return Pointer to a null-terminated string.
 */
const char *mp4_video_codec_name(mp4_video_codec_t codec);
/**
 * \brief Get a human-readable name string.
 * \param codec Parameter codec.
 * \return Pointer to a null-terminated string.
 */
const char *mp4_audio_codec_name(mp4_audio_codec_t codec);

#ifdef __cplusplus
}
#endif

#endif
