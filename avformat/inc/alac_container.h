/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ALAC_CONTAINER_H
#define ALAC_CONTAINER_H

#include "container_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    container_common_t common;

    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bits_per_sample;
    uint32_t frames_per_packet;
    uint32_t bytes_per_packet;
    uint32_t packet_count;
    uint32_t *packet_sizes;
    alac_dec_config_t alac_config;
} alac_demux_t;

/**
 * \brief Open and initialize the context.
 * \param demuxer Parameter demuxer.
 * \param avp_io Parameter avp_io.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t alac_demux_open(alac_demux_t *demuxer, avp_io_t *avp_io);
/**
 * \brief Close the context and release resources.
 * \param demuxer Parameter demuxer.
 */
void alac_demux_close(alac_demux_t *demuxer);
/**
 * \brief Get the audio stream decoder configuration.
 * \param demuxer Parameter demuxer.
 * \param config Parameter config.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t alac_demux_get_audio_stream_config(const alac_demux_t *demuxer,
                                                audio_codec_dec_config_t *config);
/**
 * \brief Read one packet from the stream.
 * \param demuxer Parameter demuxer.
 * \param packet Parameter packet.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t alac_demux_read_packet(alac_demux_t *demuxer,
                                    avp_packet_t *packet);

#ifdef __cplusplus
}
#endif

#endif
