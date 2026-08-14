/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef OGG_CONTAINER_H
#define OGG_CONTAINER_H

#include "container_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OGG_MAX_PAGE_SEGMENTS 255u

typedef enum {
    OGG_FORMAT_UNKNOWN = 0,
    OGG_FORMAT_OPUS,
    OGG_FORMAT_VORBIS,
    OGG_FORMAT_FLAC
} ogg_format_t;

typedef struct ogg_demux {
    container_common_t common;
    uint32_t page_count;
    uint32_t packet_count;
    uint32_t stream_serial;
    uint8_t stream_serial_valid;

    ogg_format_t format;
    uint32_t header_packet_count;
#if defined(CONFIG_CHERRYAVP_OPUS)
    opus_header_info_t opus;
#endif
#if defined(CONFIG_CHERRYAVP_VORBIS)
    vorbis_header_info_t vorbis;
#endif
    char vendor[64];
    uint32_t comment_count;
    uint8_t header_parsed;
    uint8_t header_parse_step;

    uint32_t next_offset;
    uint32_t page_offset;
    uint32_t body_offset;
    uint32_t body_size;
    uint32_t body_pos;
    uint32_t packet_offset;
    uint8_t segments[OGG_MAX_PAGE_SEGMENTS];
    uint8_t segment_count;
    uint8_t segment_index;
    uint8_t page_loaded;
    uint8_t *page_body;
    uint32_t page_body_capacity;

#if defined(CONFIG_CHERRYAVP_VORBIS)
    vorbis_dec_config_t vorbis_headers;
#endif
} ogg_demux_t;

/**
 * \brief Open and initialize the context.
 * \param demuxer Parameter demuxer.
 * \param avp_io Parameter avp_io.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t ogg_demux_open(ogg_demux_t *demuxer, avp_io_t *avp_io);
/**
 * \brief Close the context and release resources.
 * \param demuxer Parameter demuxer.
 */
void ogg_demux_close(ogg_demux_t *demuxer);
/**
 * \brief Get the audio stream decoder configuration.
 * \param demuxer Parameter demuxer.
 * \param config Parameter config.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t ogg_demux_get_audio_stream_config(const ogg_demux_t *demuxer,
                                               audio_codec_dec_config_t *config);
/**
 * \brief Read one packet from the stream.
 * \param demuxer Parameter demuxer.
 * \param packet Parameter packet.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t ogg_demux_read_packet(ogg_demux_t *demuxer,
                                   avp_packet_t *packet);

/**
 * \brief Get a human-readable name string.
 * \param format Parameter format.
 * \return Pointer to a null-terminated string.
 */
const char *ogg_format_name(ogg_format_t format);

#ifdef __cplusplus
}
#endif

#endif
