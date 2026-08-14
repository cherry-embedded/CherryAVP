/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mp3_container.h"

static uint32_t mp3_syncsafe32(const uint8_t *buffer)
{
    return ((uint32_t)(buffer[0] & 0x7fu) << 21) |
           ((uint32_t)(buffer[1] & 0x7fu) << 14) |
           ((uint32_t)(buffer[2] & 0x7fu) << 7) |
           ((uint32_t)(buffer[3] & 0x7fu) << 0);
}

static int mp3_add_overflow(uint32_t lhs, uint32_t rhs, uint32_t *out)
{
    if (lhs > UINT32_MAX - rhs) {
        return 1;
    }

    if (out != NULL) {
        *out = lhs + rhs;
    }

    return 0;
}

static uint32_t mp3_get_id3v1_size(const uint8_t *buffer, uint32_t size)
{
    if (buffer != NULL && size >= MP3_ID3V1_SIZE &&
        buffer[0] == 'T' && buffer[1] == 'A' && buffer[2] == 'G') {
        return MP3_ID3V1_SIZE;
    }
    return 0u;
}

avp_status_t mp3_demux_open(mp3_demux_t *demuxer, avp_io_t *avp_io)
{
    uint8_t header[MP3_ID3V2_HEADER_SIZE];
    int64_t size;
    uint32_t parse_size;
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
    parse_size = demuxer->common.file_size < sizeof(header) ? demuxer->common.file_size : sizeof(header);
    st = avp_io_read_at(avp_io, 0u, header, parse_size);
    if (st != AVP_OK) {
        return st;
    }
    demuxer->id3v2_size = 0u;
    demuxer->id3v1_size = 0u;
    demuxer->common.stream_offset = 0u;
    demuxer->common.stream_size = demuxer->common.file_size;
    if (parse_size < MP3_ID3V2_HEADER_SIZE) {
        st = AVP_EBADHEADER;
    } else if (header[0] == 'I' && header[1] == 'D' && header[2] == '3') {
        uint32_t payload_size;
        uint32_t total_size;
        uint32_t footer_size = 0u;

        if ((header[6] & 0x80u) != 0u ||
            (header[7] & 0x80u) != 0u ||
            (header[8] & 0x80u) != 0u ||
            (header[9] & 0x80u) != 0u) {
            st = AVP_EBADHEADER;
        } else {
            payload_size = mp3_syncsafe32(&header[6]);
            if (header[3] == 4u && (header[5] & 0x10u) != 0u) {
                footer_size = 10u;
            }
            if (mp3_add_overflow(MP3_ID3V2_HEADER_SIZE, payload_size, &total_size) ||
                mp3_add_overflow(total_size, footer_size, &total_size) ||
                total_size > demuxer->common.file_size) {
                st = AVP_EBADHEADER;
            } else {
                demuxer->id3v2_size = total_size;
                demuxer->common.stream_offset = total_size;
                demuxer->common.stream_size = demuxer->common.file_size - total_size;
                st = AVP_OK;
            }
        }
    } else {
        st = AVP_OK;
    }
    if (st == AVP_OK && demuxer->common.stream_size >= MP3_ID3V1_SIZE) {
        uint8_t tag[MP3_ID3V1_SIZE];
        uint32_t tag_size;

        st = avp_io_read_at(avp_io,
                            demuxer->common.stream_offset + demuxer->common.stream_size - MP3_ID3V1_SIZE,
                            tag,
                            sizeof(tag));
        if (st == AVP_OK) {
            tag_size = mp3_get_id3v1_size(tag, sizeof(tag));
            if (tag_size != 0u) {
                demuxer->id3v1_size = tag_size;
                demuxer->common.stream_size -= tag_size;
            }
        }
    }
    if (st == AVP_OK) {
        demuxer->common.current_offset = demuxer->common.stream_offset;
        st = avp_io_seek(avp_io, demuxer->common.current_offset);
    }
    return st;
}

void mp3_demux_close(mp3_demux_t *demuxer)
{
    if (demuxer != NULL) {
        memset(demuxer, 0, sizeof(*demuxer));
    }
}

avp_status_t mp3_demux_get_audio_stream_config(const mp3_demux_t *demuxer,
                                               audio_codec_dec_config_t *config)
{
    if (demuxer == NULL || config == NULL) {
        return AVP_EINVAL;
    }
    if (demuxer->common.avp_io == NULL || demuxer->common.stream_size == 0u) {
        return AVP_EBADHEADER;
    }

    memset(config, 0, sizeof(*config));
    config->codec_type = AUDIO_CODEC_ID_MP3;
    return AVP_OK;
}

avp_status_t mp3_demux_read_packet(mp3_demux_t *demuxer,
                                   avp_packet_t *packet)
{
    uint8_t header[MP3_MIN_FRAME_HEADER_SIZE];
    mp3_frame_info_t frame;
    uint32_t stream_end;
    avp_status_t st;

    if (demuxer == NULL || packet == NULL) {
        return AVP_EINVAL;
    }

    stream_end = demuxer->common.stream_offset + demuxer->common.stream_size;
    if (demuxer->common.current_offset >= stream_end) {
        return AVP_ENOENT;
    }
    if (stream_end - demuxer->common.current_offset < MP3_MIN_FRAME_HEADER_SIZE) {
        return AVP_EBADFRAME;
    }

    st = avp_io_read(demuxer->common.avp_io,
                     header,
                     (uint32_t)sizeof(header));
    if (st != AVP_OK) {
        return st;
    }
    st = mp3_parse_frame_header(header, (uint32_t)sizeof(header), &frame);
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
