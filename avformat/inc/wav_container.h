/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef WAV_CONTAINER_H
#define WAV_CONTAINER_H

#include "container_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WAV_FOURCC(a, b, c, d) AUDIO_CODEC_FOURCC(a, b, c, d)

#define WAV_ID_RIFF WAV_FOURCC('R', 'I', 'F', 'F')
#define WAV_ID_WAVE WAV_FOURCC('W', 'A', 'V', 'E')
#define WAV_ID_FMT  WAV_FOURCC('f', 'm', 't', ' ')
#define WAV_ID_DATA WAV_FOURCC('d', 'a', 't', 'a')

#define WAV_RIFF_HEADER_SIZE      12u
#define WAV_CHUNK_HEADER_SIZE     8u
#define WAV_FMT_BASE_SIZE         16u
#define WAV_HEADER_PARSE_MIN_SIZE (WAV_RIFF_HEADER_SIZE + WAV_CHUNK_HEADER_SIZE + WAV_FMT_BASE_SIZE)
#define WAV_HEADER_SIZE           (WAV_RIFF_HEADER_SIZE + WAV_CHUNK_HEADER_SIZE + WAV_FMT_BASE_SIZE + WAV_CHUNK_HEADER_SIZE)

#define WAV_AUDIO_FORMAT_PCM        1u
#define WAV_AUDIO_FORMAT_MS_ADPCM   2u
#define WAV_AUDIO_FORMAT_IEEE_FLOAT 3u
#define WAV_AUDIO_FORMAT_ALAW       6u
#define WAV_AUDIO_FORMAT_MULAW      7u
#define WAV_AUDIO_FORMAT_IMA_ADPCM  0x0011u
#define WAV_AUDIO_FORMAT_DVI_ADPCM  WAV_AUDIO_FORMAT_IMA_ADPCM
#define WAV_AUDIO_FORMAT_G722       0x028Fu
#define WAV_AUDIO_FORMAT_EXTENSIBLE 0xFFFEu

#define wav_get_wave_size(demuxer)     ((demuxer)->header.wave_header.ChunkSize + 8u)
#define wav_get_data_size(demuxer)     ((demuxer)->header.wave_data.Subchunk2Size)
#define wav_get_pcm_size(demuxer)      ((demuxer)->pcm_size)
#define wav_get_file_size(demuxer)     ((demuxer)->file_size)
#define wav_get_stream_offset(demuxer) ((demuxer)->common.stream_offset)
#define wav_get_stream_size(demuxer)   ((demuxer)->common.stream_size)

typedef struct {
    uint32_t ChunkID;
    uint32_t ChunkSize;
    uint32_t Format;
} wave_header_t;

typedef struct {
    uint32_t Subchunk1ID;
    uint32_t Subchunk1Size;
    uint16_t AudioFormat;
    uint16_t NumChannels;
    uint32_t SampleRate;
    uint32_t ByteRate;
    uint16_t BlockAlign;
    uint16_t BitsPerSample;
    uint16_t CbSize;
    uint16_t SamplesPerBlock;
} wave_fmt_t;

typedef struct {
    uint32_t Subchunk2ID;
    uint32_t Subchunk2Size;
} wave_data_t;

typedef struct {
    wave_header_t wave_header;
    wave_fmt_t wave_fmt;
    wave_data_t wave_data;
} wav_file_header_info_t;

typedef struct wav_demux wav_demux_t;
typedef struct wav_mux wav_mux_t;

struct wav_mux {
    avp_io_t *avp_io;
    wav_file_header_info_t header;
    uint32_t file_size;
    uint32_t offset;
    uint32_t pcm_size;
};

struct wav_demux {
    container_common_t common;

    wav_file_header_info_t header;
};

/**
 * \brief Open and initialize the context.
 * \param muxer Parameter muxer.
 * \param avp_io Parameter avp_io.
 * \param audio_format Parameter audio_format.
 * \param num_channels Parameter num_channels.
 * \param sample_rate Parameter sample_rate.
 * \param bits_per_sample Parameter bits_per_sample.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t wav_mux_open(wav_mux_t *muxer,
                          avp_io_t *avp_io,
                          uint16_t audio_format,
                          uint16_t num_channels,
                          uint32_t sample_rate,
                          uint16_t bits_per_sample);
/**
 * \brief Close the context and release resources.
 * \param muxer Parameter muxer.
 */
void wav_mux_close(wav_mux_t *muxer);
/**
 * \brief Mux PCM data into the container stream.
 * \param muxer Parameter muxer.
 * \param pcm_data Parameter pcm_data.
 * \param pcm_size Parameter pcm_size.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t wav_mux(wav_mux_t *muxer,
                     const void *pcm_data,
                     uint32_t pcm_size);

/**
 * \brief Open and initialize the context.
 * \param demuxer Parameter demuxer.
 * \param avp_io Parameter avp_io.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t wav_demux_open(wav_demux_t *demuxer,
                            avp_io_t *avp_io);
/**
 * \brief Close the context and release resources.
 * \param demuxer Parameter demuxer.
 */
void wav_demux_close(wav_demux_t *demuxer);
/**
 * \brief Get the audio stream decoder configuration.
 * \param demuxer Parameter demuxer.
 * \param config Parameter config.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t wav_demux_get_audio_stream_config(const wav_demux_t *demuxer,
                                               audio_codec_dec_config_t *config);
/**
 * \brief Read one packet from the stream.
 * \param demuxer Parameter demuxer.
 * \param packet Parameter packet.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t wav_demux_read_packet(wav_demux_t *demuxer,
                                   avp_packet_t *packet);
#ifdef __cplusplus
}
#endif

#endif
