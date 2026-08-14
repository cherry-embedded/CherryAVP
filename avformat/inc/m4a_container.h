/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef M4A_CONTAINER_H
#define M4A_CONTAINER_H

#include "container_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    M4A_AUDIO_CODEC_UNKNOWN = 0,
    M4A_AUDIO_CODEC_AAC,
    M4A_AUDIO_CODEC_ALAC
} m4a_audio_codec_t;

struct m4a_demux {
    container_common_t common;
    m4a_audio_codec_t codec_type;
    aac_dec_config_t aac_config;
    alac_dec_config_t alac_config;

    uint32_t sample_count;
    uint32_t *sample_sizes;
    uint32_t *sample_offsets;
    uint32_t max_sample_size;
};

typedef struct m4a_demux m4a_demux_t;

/**
 * \brief Open and initialize the context.
 * \param demuxer Parameter demuxer.
 * \param avp_io Parameter avp_io.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t m4a_demux_open(m4a_demux_t *demuxer,
                            avp_io_t *avp_io);
/**
 * \brief Close the context and release resources.
 * \param demuxer Parameter demuxer.
 */
void m4a_demux_close(m4a_demux_t *demuxer);
/**
 * \brief Get the audio stream decoder configuration.
 * \param demuxer Parameter demuxer.
 * \param config Parameter config.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t m4a_demux_get_audio_stream_config(const m4a_demux_t *demuxer,
                                               audio_codec_dec_config_t *config);
/**
 * \brief Read one packet from the stream.
 * \param demuxer Parameter demuxer.
 * \param packet Parameter packet.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t m4a_demux_read_packet(m4a_demux_t *demuxer,
                                   avp_packet_t *packet);

#ifdef __cplusplus
}
#endif

#endif
