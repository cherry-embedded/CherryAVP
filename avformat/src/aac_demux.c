/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "aac_container.h"

avp_status_t aac_demux_open(aac_demux_t *demuxer, avp_io_t *avp_io)
{
    int64_t size;
    avp_status_t st;

    if (demuxer == NULL || avp_io == NULL) {
        return AVP_EINVAL;
    }
    size = avp_io_get_size(avp_io);
    if (size < 0) {
        return AVP_IO;
    }

    memset(demuxer, 0, sizeof(*demuxer));
    demuxer->common.avp_io = avp_io;
    demuxer->common.file_size = (uint32_t)size;
    demuxer->common.stream_offset = 0u;
    demuxer->common.stream_size = demuxer->common.file_size;
    demuxer->common.current_offset = demuxer->common.stream_offset;
    st = avp_io_seek(avp_io, demuxer->common.stream_offset);
    return st;
}

void aac_demux_close(aac_demux_t *demuxer)
{
    if (demuxer != NULL) {
        memset(demuxer, 0, sizeof(*demuxer));
    }
}

avp_status_t aac_demux_get_audio_stream_config(const aac_demux_t *demuxer,
                                               audio_codec_dec_config_t *config)
{
    if (demuxer == NULL || config == NULL) {
        return AVP_EINVAL;
    }

    memset(config, 0, sizeof(*config));
    config->codec_type = AUDIO_CODEC_ID_AAC;
    config->aac_config.has_no_adts_header = false;
    return AVP_OK;
}

avp_status_t aac_demux_read_packet(aac_demux_t *demuxer,
                                   avp_packet_t *packet)
{
    uint8_t header[AAC_MIN_FRAME_HEADER_SIZE];
    aac_frame_info_t frame;
    uint32_t stream_end;
    avp_status_t st;

    if (demuxer == NULL || packet == NULL) {
        return AVP_EINVAL;
    }

    stream_end = demuxer->common.stream_offset + demuxer->common.stream_size;
    if (demuxer->common.current_offset >= stream_end) {
        return AVP_ENOENT;
    }
    if (stream_end - demuxer->common.current_offset < AAC_MIN_FRAME_HEADER_SIZE) {
        return AVP_EBADFRAME;
    }

    st = avp_io_read(demuxer->common.avp_io,
                     header,
                     (uint32_t)sizeof(header));
    if (st != AVP_OK) {
        return st;
    }
    st = aac_parse_frame_header(header, (uint32_t)sizeof(header), &frame);
    if (st != AVP_OK) {
        return st;
    }
    if (frame.frame_size > stream_end - demuxer->common.current_offset) {
        return AVP_EBADFRAME;
    }

    st = avp_packet_expand(packet, frame.frame_size);
    if (st != AVP_OK) {
        return st;
    }
    memcpy(packet->buf, header, sizeof(header));
    if (frame.frame_size > sizeof(header)) {
        st = avp_io_read(demuxer->common.avp_io,
                         packet->buf + sizeof(header),
                         frame.frame_size - (uint32_t)sizeof(header));
        if (st != AVP_OK) {
            return st;
        }
    }

    packet->size = frame.frame_size;
    packet->offset = demuxer->common.current_offset;
    packet->index = demuxer->common.packet_index++;
    packet->type = AVP_PACKET_TYPE_AUDIO;
    demuxer->common.current_offset += frame.frame_size;
    return AVP_OK;
}
