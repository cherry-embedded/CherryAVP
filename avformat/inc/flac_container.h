/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef FLAC_CONTAINER_H
#define FLAC_CONTAINER_H

#include "audio_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t metadata_count;
    uint32_t seek_point_total;
    uint32_t seek_point_count;
    flac_seekpoint_t seek_points[FLAC_MAX_SEEK_POINTS];
    flac_streaminfo_t streaminfo;
    uint8_t streaminfo_buf[FLAC_STREAMINFO_SIZE];
    uint32_t streaminfo_size;
} flac_file_header_info_t;

typedef struct {
    avp_io_t *avp_io;
    uint32_t file_size;
    uint32_t stream_offset;
    uint32_t stream_size;
    uint32_t current_offset;
    uint32_t packet_index;
    uint8_t next_data[FLAC_FRAME_HEADER_MAX_SIZE];
    uint32_t next_data_size;
    flac_file_header_info_t header;
} flac_demux_t;

/**
 * \brief Open and initialize the context.
 * \param demuxer Parameter demuxer.
 * \param avp_io Parameter avp_io.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t flac_demux_open(flac_demux_t *demuxer, avp_io_t *avp_io);
/**
 * \brief Close the context and release resources.
 * \param demuxer Parameter demuxer.
 */
void flac_demux_close(flac_demux_t *demuxer);
/**
 * \brief Get the audio stream decoder configuration.
 * \param demuxer Parameter demuxer.
 * \param config Parameter config.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t flac_demux_get_audio_stream_config(const flac_demux_t *demuxer,
                                                audio_codec_dec_config_t *config);
/**
 * \brief Read one packet from the stream.
 * \param demuxer Parameter demuxer.
 * \param packet Parameter packet.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t flac_demux_read_packet(flac_demux_t *demuxer,
                                    avp_packet_t *packet);

#ifdef __cplusplus
}
#endif

#endif
