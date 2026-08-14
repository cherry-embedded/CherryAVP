/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AVI_CONTAINER_H
#define AVI_CONTAINER_H

#include "container_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t avi_fourcc_t;

#define AVI_FOURCC(a, b, c, d)                                \
    ((uint32_t)(uint8_t)(a) | ((uint32_t)(uint8_t)(b) << 8) | \
     ((uint32_t)(uint8_t)(c) << 16) | ((uint32_t)(uint8_t)(d) << 24))

#define AVI_ID_RIFF AVI_FOURCC('R', 'I', 'F', 'F')
#define AVI_ID_LIST AVI_FOURCC('L', 'I', 'S', 'T')
#define AVI_ID_AVI  AVI_FOURCC('A', 'V', 'I', ' ')
#define AVI_ID_HDRL AVI_FOURCC('h', 'd', 'r', 'l')
#define AVI_ID_STRL AVI_FOURCC('s', 't', 'r', 'l')
#define AVI_ID_MOVI AVI_FOURCC('m', 'o', 'v', 'i')
#define AVI_ID_REC  AVI_FOURCC('r', 'e', 'c', ' ')

#define AVI_ID_AVIH AVI_FOURCC('a', 'v', 'i', 'h')
#define AVI_ID_STRH AVI_FOURCC('s', 't', 'r', 'h')
#define AVI_ID_STRF AVI_FOURCC('s', 't', 'r', 'f')
#define AVI_ID_IDX1 AVI_FOURCC('i', 'd', 'x', '1')

#define AVI_STREAM_VIDS AVI_FOURCC('v', 'i', 'd', 's')
#define AVI_STREAM_AUDS AVI_FOURCC('a', 'u', 'd', 's')

#define AVI_CHUNK_00DB AVI_FOURCC('0', '0', 'd', 'b')
#define AVI_CHUNK_00DC AVI_FOURCC('0', '0', 'd', 'c')
#define AVI_CHUNK_01WB AVI_FOURCC('0', '1', 'w', 'b')

#define AVIIF_LIST       0x00000001u
#define AVIIF_KEYFRAME   0x00000010u
#define AVIIF_NO_TIME    0x00000100u
#define AVIIF_COMPRESSOR 0x0fff0000u

#define AVI_MAIN_HEADER_SIZE        56u
#define AVI_STREAM_HEADER_SIZE      56u
#define AVI_BITMAP_INFO_HEADER_SIZE 40u
#define AVI_WAVE_FORMAT_SIZE        16u
#define AVI_WAVE_FORMAT_EX_SIZE     18u
#define AVI_WAVE_FORMAT_EXT_SIZE    40u
#define AVI_INDEX_ENTRY_SIZE        16u

#define avi_get_file_size(demuxer)    ((demuxer)->common.file_size)
#define avi_get_width(demuxer)        ((demuxer)->main_header.dwWidth)
#define avi_get_height(demuxer)       ((demuxer)->main_header.dwHeight)
#define avi_get_frame_count(demuxer)  ((demuxer)->main_header.dwTotalFrames)
#define avi_get_stream_count(demuxer) ((demuxer)->main_header.dwStreams)
#define avi_get_index_count(demuxer)  ((demuxer)->index_count)

typedef enum {
    AVI_CHUNK_TYPE_UNKNOWN = 0,
    AVI_CHUNK_TYPE_VIDEO = 1,
    AVI_CHUNK_TYPE_AUDIO = 2,
    AVI_CHUNK_TYPE_PALETTE = 3
} avi_chunk_type_t;

typedef enum {
    AVI_VIDEO_CODEC_UNKNOWN = 0,
    AVI_VIDEO_CODEC_RAW,
    AVI_VIDEO_CODEC_RLE,
    AVI_VIDEO_CODEC_MJPEG,
    AVI_VIDEO_CODEC_PNG,
    AVI_VIDEO_CODEC_MPEG1,
    AVI_VIDEO_CODEC_MPEG2,
    AVI_VIDEO_CODEC_MPEG4,
    AVI_VIDEO_CODEC_H264,
    AVI_VIDEO_CODEC_HEVC,
    AVI_VIDEO_CODEC_VP8,
    AVI_VIDEO_CODEC_VP9,
    AVI_VIDEO_CODEC_AV1,
    AVI_VIDEO_CODEC_THEORA,
    AVI_VIDEO_CODEC_DV,
    AVI_VIDEO_CODEC_HUFFYUV,
    AVI_VIDEO_CODEC_FFV1
} avi_video_codec_t;

typedef enum {
    AVI_AUDIO_CODEC_UNKNOWN = 0,
    AVI_AUDIO_CODEC_PCM,
    AVI_AUDIO_CODEC_IEEE_FLOAT,
    AVI_AUDIO_CODEC_MS_ADPCM,
    AVI_AUDIO_CODEC_IMA_ADPCM,
    AVI_AUDIO_CODEC_G711_ALAW,
    AVI_AUDIO_CODEC_G711_MULAW,
    AVI_AUDIO_CODEC_G722,
    AVI_AUDIO_CODEC_MP2,
    AVI_AUDIO_CODEC_MP3,
    AVI_AUDIO_CODEC_AAC,
    AVI_AUDIO_CODEC_AC3,
    AVI_AUDIO_CODEC_EAC3,
    AVI_AUDIO_CODEC_DTS,
    AVI_AUDIO_CODEC_WMA,
    AVI_AUDIO_CODEC_FLAC,
    AVI_AUDIO_CODEC_VORBIS,
    AVI_AUDIO_CODEC_OPUS,
    AVI_AUDIO_CODEC_EXTENSIBLE
} avi_audio_codec_t;

typedef struct {
    avi_fourcc_t id;
    uint32_t size;
    avi_fourcc_t type;
} avi_riff_header_t;

typedef struct {
    avi_fourcc_t id;
    uint32_t size;
} avi_chunk_header_t;

typedef struct {
    avi_fourcc_t id;
    uint32_t size;
    avi_fourcc_t type;
} avi_list_header_t;

typedef struct {
    uint32_t dwMicroSecPerFrame;
    uint32_t dwMaxBytesPerSec;
    uint32_t dwPaddingGranularity;
    uint32_t dwFlags;
    uint32_t dwTotalFrames;
    uint32_t dwInitialFrames;
    uint32_t dwStreams;
    uint32_t dwSuggestedBufferSize;
    uint32_t dwWidth;
    uint32_t dwHeight;
    uint32_t dwReserved[4];
} avi_main_header_t;

typedef struct {
    int16_t left;
    int16_t top;
    int16_t right;
    int16_t bottom;
} avi_rect_t;

typedef struct {
    avi_fourcc_t fccType;
    avi_fourcc_t fccHandler;
    uint32_t dwFlags;
    uint16_t wPriority;
    uint16_t wLanguage;
    uint32_t dwInitialFrames;
    uint32_t dwScale;
    uint32_t dwRate;
    uint32_t dwStart;
    uint32_t dwLength;
    uint32_t dwSuggestedBufferSize;
    uint32_t dwQuality;
    uint32_t dwSampleSize;
    avi_rect_t rcFrame;
} avi_stream_header_t;

typedef struct {
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    avi_fourcc_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} avi_bitmap_info_header_t;

typedef struct {
    uint16_t wFormatTag;
    uint16_t nChannels;
    uint32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;
    uint16_t cbSize;
    uint16_t wValidBitsPerSample;
    uint32_t dwChannelMask;
    uint16_t wSubFormatTag;
} avi_wave_format_ex_t;

typedef struct {
    avi_fourcc_t dwChunkId;
    uint32_t dwFlags;
    uint32_t dwOffset;
    uint32_t dwSize;
} avi_index_entry_t;

typedef struct {
    avi_stream_header_t stream;
    avi_bitmap_info_header_t format;
    avi_video_codec_t codec_type;
    uint32_t stream_index;
    uint32_t data_size;
} avi_video_stream_t;

typedef struct {
    avi_stream_header_t stream;
    avi_wave_format_ex_t format;
    avi_audio_codec_t codec_type;
    uint32_t stream_index;
    uint32_t block_count;
    uint32_t data_size;
} avi_audio_stream_t;

typedef struct avi_demux avi_demux_t;

struct avi_demux {
    container_common_t common;

    avi_riff_header_t riff;
    avi_main_header_t main_header;
    avi_video_stream_t video;
    avi_audio_stream_t audio;

    uint32_t stream_count;
    uint32_t has_video;
    uint32_t has_audio;
    uint32_t has_index;

    uint32_t hdrl_offset;
    uint32_t hdrl_size;
    uint32_t movi_offset;
    uint32_t movi_data_offset;
    uint32_t movi_size;
    uint32_t idx1_offset;
    uint32_t idx1_size;
    uint32_t index_count;
};

/**
 * \brief Open and initialize the context.
 * \param demuxer Parameter demuxer.
 * \param avp_io Parameter avp_io.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t avi_demux_open(avi_demux_t *demuxer,
                            avp_io_t *avp_io);
/**
 * \brief Close the context and release resources.
 * \param demuxer Parameter demuxer.
 */
void avi_demux_close(avi_demux_t *demuxer);
/**
 * \brief Get the audio stream decoder configuration.
 * \param demuxer Parameter demuxer.
 * \param config Parameter config.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t avi_demux_get_audio_stream_config(const avi_demux_t *demuxer,
                                               audio_codec_dec_config_t *config);
/**
 * \brief Peek one packet without consuming it.
 * \param demuxer Parameter demuxer.
 * \param packet Parameter packet.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t avi_demux_peek_packet(avi_demux_t *demuxer,
                                   avp_packet_t *packet);
/**
 * \brief Perform this API operation.
 * \param demuxer Parameter demuxer.
 * \param index Parameter index.
 * \param packet Parameter packet.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t avi_demux_peek_packet_by_idx(avi_demux_t *demuxer,
                                          uint32_t index,
                                          avp_packet_t *packet);
/**
 * \brief Pop and consume one packet from the stream.
 * \param demuxer Parameter demuxer.
 * \param packet Parameter packet.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t avi_demux_pop_packet(avi_demux_t *demuxer,
                                  avp_packet_t *packet);
/**
 * \brief Rewind the stream position to the beginning.
 * \param demuxer Parameter demuxer.
 */
void avi_demux_rewind(avi_demux_t *demuxer);

/**
 * \brief Get a human-readable name string.
 * \param type Parameter type.
 * \return Pointer to a null-terminated string.
 */
const char *avi_chunk_type_name(avi_chunk_type_t type);
/**
 * \brief Get a human-readable name string.
 * \param codec Parameter codec.
 * \return Pointer to a null-terminated string.
 */
const char *avi_video_codec_name(avi_video_codec_t codec);
/**
 * \brief Get a human-readable name string.
 * \param codec Parameter codec.
 * \return Pointer to a null-terminated string.
 */
const char *avi_audio_codec_name(avi_audio_codec_t codec);

#ifdef __cplusplus
}
#endif

#endif
