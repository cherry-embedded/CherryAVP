/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "amr_container.h"

avp_status_t amr_demux_open(amr_demux_t *demuxer, avp_io_t *avp_io)
{
    uint8_t header[AMR_WB_MAGIC_SIZE];
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
    st = avp_io_seek(avp_io, 0);
    if (st != AVP_OK) {
        return st;
    }
    st = avp_io_read(avp_io, header, sizeof(header));
    if (st == AVP_OK) {
        if (memcmp(header, AMR_WB_MAGIC, AMR_WB_MAGIC_SIZE) == 0) {
            demuxer->format = AMR_FORMAT_WB;
            demuxer->common.stream_offset = AMR_WB_MAGIC_SIZE;
            demuxer->common.stream_size = demuxer->common.file_size - demuxer->common.stream_offset;
        } else if (memcmp(header, AMR_NB_MAGIC, AMR_NB_MAGIC_SIZE) == 0) {
            demuxer->format = AMR_FORMAT_NB;
            demuxer->common.stream_offset = AMR_NB_MAGIC_SIZE;
            demuxer->common.stream_size = demuxer->common.file_size - demuxer->common.stream_offset;
        } else {
            st = AVP_EBADHEADER;
        }
    }
    if (st == AVP_OK) {
        demuxer->common.current_offset = demuxer->common.stream_offset;
        st = avp_io_seek(avp_io, demuxer->common.stream_offset);
    }
    return st;
}

void amr_demux_close(amr_demux_t *demuxer)
{
    if (demuxer != NULL) {
        memset(demuxer, 0, sizeof(*demuxer));
    }
}

avp_status_t amr_demux_get_audio_stream_config(const amr_demux_t *demuxer,
                                               audio_codec_dec_config_t *config)
{
    if (demuxer == NULL || config == NULL) {
        return AVP_EINVAL;
    }

    memset(config, 0, sizeof(*config));
    config->codec_type = AUDIO_CODEC_ID_AMR;
    config->amr_config.format = demuxer->format;
    return AVP_OK;
}

avp_status_t amr_demux_read_packet(amr_demux_t *demuxer,
                                   avp_packet_t *packet)
{
    uint8_t header[AMR_MIN_FRAME_HEADER_SIZE];
    amr_frame_info_t frame;
    uint32_t stream_end;
    avp_status_t st;

    if (demuxer == NULL || packet == NULL) {
        return AVP_EINVAL;
    }

    stream_end = demuxer->common.stream_offset + demuxer->common.stream_size;
    if (demuxer->common.current_offset >= stream_end) {
        return AVP_ENOENT;
    }

    st = avp_io_read(demuxer->common.avp_io,
                     header,
                     (uint32_t)sizeof(header));
    if (st != AVP_OK) {
        return st;
    }
    st = amr_parse_frame_header(demuxer->format,
                                header,
                                (uint32_t)sizeof(header),
                                &frame);
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
