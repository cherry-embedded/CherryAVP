/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "flac_container.h"

static int flac_add_overflow(uint32_t lhs, uint32_t rhs, uint32_t *out)
{
    if (lhs > (UINT32_MAX - rhs)) {
        return 1;
    }

    if (out != NULL) {
        *out = lhs + rhs;
    }
    return 0;
}

static avp_status_t flac_parse_streaminfo_block(const uint8_t *buffer,
                                                uint32_t size,
                                                flac_streaminfo_t *streaminfo)
{
    uint64_t packed;

    if (streaminfo == NULL || buffer == NULL || size != FLAC_STREAMINFO_SIZE) {
        return AVP_EINVAL;
    }

    memset(streaminfo, 0, sizeof(*streaminfo));
    streaminfo->min_block_size = AVP_GET_BE16(&buffer[0]);
    streaminfo->max_block_size = AVP_GET_BE16(&buffer[2]);
    streaminfo->min_frame_size = AVP_GET_BE24(&buffer[4]);
    streaminfo->max_frame_size = AVP_GET_BE24(&buffer[7]);
    packed = AVP_GET_BE64(&buffer[10]);
    streaminfo->sample_rate = (uint32_t)((packed >> 44) & 0xfffffu);
    streaminfo->channels = (uint8_t)(((packed >> 41) & 0x07u) + 1u);
    streaminfo->bits_per_sample = (uint8_t)(((packed >> 36) & 0x1fu) + 1u);
    streaminfo->total_samples = packed & 0xfffffffffull;
    memcpy(streaminfo->md5, &buffer[18], sizeof(streaminfo->md5));

    if (streaminfo->min_block_size == 0u ||
        streaminfo->max_block_size < streaminfo->min_block_size ||
        streaminfo->sample_rate == 0u) {
        return AVP_EBADHEADER;
    }

    return AVP_OK;
}

static avp_status_t flac_parse_seektable(flac_file_header_info_t *header_info,
                                         const uint8_t *buffer,
                                         uint32_t size)
{
    uint32_t point_count;
    uint32_t i;

    if (header_info == NULL || buffer == NULL ||
        (size % FLAC_SEEKPOINT_SIZE) != 0u) {
        return AVP_EINVAL;
    }

    point_count = size / FLAC_SEEKPOINT_SIZE;
    header_info->seek_point_total = point_count;
    header_info->seek_point_count = 0u;
    memset(header_info->seek_points, 0, sizeof(header_info->seek_points));

    for (i = 0u; i < point_count; i++) {
        flac_seekpoint_t point;
        const uint8_t *seekpoint = buffer + i * FLAC_SEEKPOINT_SIZE;

        point.sample_number = AVP_GET_BE64(&seekpoint[0]);
        point.stream_offset = AVP_GET_BE64(&seekpoint[8]);
        point.frame_samples = AVP_GET_BE16(&seekpoint[16]);
        if (point.sample_number == FLAC_SEEKPOINT_PLACEHOLDER) {
            continue;
        }
        if (header_info->seek_point_count < FLAC_MAX_SEEK_POINTS) {
            header_info->seek_points[header_info->seek_point_count++] = point;
        }
    }
    return AVP_OK;
}

avp_status_t flac_demux_open(flac_demux_t *demuxer, avp_io_t *avp_io)
{
    uint8_t block_header[FLAC_METADATA_HEADER_SIZE];
    uint8_t streaminfo[FLAC_STREAMINFO_SIZE];
    int64_t size;
    uint32_t metadata_offset = FLAC_MARKER_SIZE;
    uint8_t saw_streaminfo = 0u;
    avp_status_t st;

    if (demuxer == NULL || avp_io == NULL) {
        return AVP_EINVAL;
    }
    size = avp_io_get_size(avp_io);
    if (size < 0) {
        return AVP_IO;
    }

    memset(demuxer, 0, sizeof(*demuxer));
    demuxer->avp_io = avp_io;
    demuxer->file_size = (uint32_t)size;

    if (demuxer->file_size < FLAC_MARKER_SIZE ||
        avp_io_read_at(avp_io, 0u, block_header, FLAC_MARKER_SIZE) != AVP_OK ||
        block_header[0] != 'f' || block_header[1] != 'L' ||
        block_header[2] != 'a' || block_header[3] != 'C') {
        return AVP_EBADHEADER;
    }

    memset(&demuxer->header, 0, sizeof(demuxer->header));
    while (1) {
        uint32_t block_size;
        uint32_t payload_offset;
        uint8_t is_last;
        uint8_t block_type;

        if (metadata_offset > demuxer->file_size ||
            demuxer->file_size - metadata_offset < FLAC_METADATA_HEADER_SIZE ||
            avp_io_read_at(avp_io,
                           metadata_offset,
                           block_header,
                           FLAC_METADATA_HEADER_SIZE) != AVP_OK) {
            st = AVP_EBADHEADER;
            goto fail;
        }

        is_last = (block_header[0] >> 7) & 0x01u;
        block_type = block_header[0] & 0x7fu;
        block_size = AVP_GET_BE24(&block_header[1]);
        payload_offset = metadata_offset + FLAC_METADATA_HEADER_SIZE;
        if (block_size > demuxer->file_size - payload_offset) {
            st = AVP_EBADHEADER;
            goto fail;
        }

        if (demuxer->header.metadata_count == 0u &&
            (block_type != FLAC_METADATA_STREAMINFO ||
             block_size != FLAC_STREAMINFO_SIZE)) {
            st = AVP_EBADHEADER;
            goto fail;
        }

        if (block_type == FLAC_METADATA_STREAMINFO) {
            if (block_size != FLAC_STREAMINFO_SIZE ||
                avp_io_read_at(avp_io,
                               payload_offset,
                               streaminfo,
                               FLAC_STREAMINFO_SIZE) != AVP_OK) {
                st = AVP_EBADHEADER;
                goto fail;
            }
            st = flac_parse_streaminfo_block(streaminfo,
                                             FLAC_STREAMINFO_SIZE,
                                             &demuxer->header.streaminfo);
            if (st != AVP_OK) {
                goto fail;
            }
            memcpy(demuxer->header.streaminfo_buf,
                   streaminfo,
                   FLAC_STREAMINFO_SIZE);
            demuxer->header.streaminfo_size = FLAC_STREAMINFO_SIZE;
            saw_streaminfo = 1u;
        } else if (block_type == FLAC_METADATA_SEEKTABLE) {
            uint8_t *seektable;

            if ((block_size % FLAC_SEEKPOINT_SIZE) != 0u) {
                st = AVP_EBADHEADER;
                goto fail;
            }
            if (block_size != 0u) {
                seektable = (uint8_t *)avp_malloc(block_size);
                if (seektable == NULL) {
                    st = AVP_ENOMEM;
                    goto fail;
                }
                st = avp_io_read_at(avp_io,
                                    payload_offset,
                                    seektable,
                                    block_size);
                if (st == AVP_OK) {
                    st = flac_parse_seektable(&demuxer->header,
                                              seektable,
                                              block_size);
                }
                avp_free(seektable);
                if (st != AVP_OK) {
                    goto fail;
                }
            } else {
                demuxer->header.seek_point_total = 0u;
                demuxer->header.seek_point_count = 0u;
            }
        }

        demuxer->header.metadata_count++;
        metadata_offset = payload_offset + block_size;
        if (is_last != 0u) {
            break;
        }
    }

    if (saw_streaminfo == 0u || metadata_offset >= demuxer->file_size) {
        st = AVP_EBADHEADER;
        goto fail;
    }

    demuxer->stream_offset = metadata_offset;
    demuxer->stream_size = demuxer->file_size - metadata_offset;
    demuxer->current_offset = demuxer->stream_offset;
    st = avp_io_seek(avp_io, demuxer->current_offset);
    if (st != AVP_OK) {
        goto fail;
    }
    return AVP_OK;

fail:
    return st;
}

void flac_demux_close(flac_demux_t *demuxer)
{
    if (demuxer != NULL) {
        memset(demuxer, 0, sizeof(*demuxer));
    }
}

avp_status_t flac_demux_get_audio_stream_config(const flac_demux_t *demuxer,
                                                audio_codec_dec_config_t *config)
{
    if (demuxer == NULL || config == NULL) {
        return AVP_EINVAL;
    }
    if (demuxer->avp_io == NULL || demuxer->header.streaminfo.sample_rate == 0u) {
        return AVP_EBADHEADER;
    }

    memset(config, 0, sizeof(*config));
    config->codec_type = AUDIO_CODEC_ID_FLAC;
    memcpy(config->flac_config.streaminfo,
           demuxer->header.streaminfo_buf,
           demuxer->header.streaminfo_size);
    config->flac_config.streaminfo_size = demuxer->header.streaminfo_size;
    return AVP_OK;
}

static int flac_demux_is_sync_pair(const uint8_t *buffer)
{
    return buffer != NULL &&
           buffer[0] == 0xffu &&
           (buffer[1] & 0xfeu) == 0xf8u;
}

static int flac_demux_frame_header_matches(const flac_demux_t *demuxer,
                                           const flac_frame_info_t *frame)
{
    const flac_streaminfo_t *streaminfo;
    uint64_t frame_sample;
    uint64_t remain_samples;

    if (demuxer == NULL || frame == NULL) {
        return 0;
    }

    streaminfo = &demuxer->header.streaminfo;
    if (frame->channels != streaminfo->channels ||
        frame->bits_per_sample != streaminfo->bits_per_sample ||
        frame->sample_rate != streaminfo->sample_rate) {
        return 0;
    }

    if (frame->block_size >= streaminfo->min_block_size &&
        frame->block_size <= streaminfo->max_block_size) {
        return 1;
    }

    if (streaminfo->total_samples == 0u ||
        frame->block_size == 0u ||
        frame->block_size > streaminfo->max_block_size) {
        return 0;
    }

    if (frame->header.blocking_strategy == 0u) {
        frame_sample = frame->frame_or_sample_number * streaminfo->max_block_size;
    } else {
        frame_sample = frame->frame_or_sample_number;
    }
    if (frame_sample >= streaminfo->total_samples) {
        return 0;
    }

    remain_samples = streaminfo->total_samples - frame_sample;
    return remain_samples < streaminfo->min_block_size &&
           frame->block_size == remain_samples;
}

static avp_status_t flac_demux_packet_append(avp_packet_t *packet,
                                             const uint8_t *data,
                                             uint32_t size)
{
    avp_status_t st;

    if (packet == NULL || (data == NULL && size != 0u)) {
        return AVP_EINVAL;
    }
    if (flac_add_overflow(packet->size, size, NULL)) {
        return AVP_ERANGE;
    }
    st = avp_packet_expand(packet, packet->size + size);
    if (st != AVP_OK) {
        return st;
    }
    if (size != 0u) {
        memcpy(packet->buf + packet->size, data, size);
        packet->size += size;
    }
    return AVP_OK;
}

static avp_status_t flac_demux_packet_read(flac_demux_t *demuxer,
                                           avp_packet_t *packet,
                                           uint32_t packet_offset,
                                           uint32_t stream_end,
                                           uint32_t size)
{
    avp_status_t st;

    if (demuxer == NULL || packet == NULL) {
        return AVP_EINVAL;
    }
    if (size > stream_end - packet_offset ||
        packet->size > stream_end - packet_offset - size) {
        return AVP_EBADFRAME;
    }
    if (flac_add_overflow(packet->size, size, NULL)) {
        return AVP_ERANGE;
    }
    st = avp_packet_expand(packet, packet->size + size);
    if (st != AVP_OK) {
        return st;
    }
    st = avp_io_read(demuxer->avp_io, packet->buf + packet->size, size);
    if (st != AVP_OK) {
        return st;
    }
    packet->size += size;
    return AVP_OK;
}

static avp_status_t flac_demux_validate_current_header(flac_demux_t *demuxer,
                                                       avp_packet_t *packet,
                                                       uint32_t packet_offset,
                                                       uint32_t stream_end)
{
    flac_frame_info_t frame;
    avp_status_t st;

    if (demuxer == NULL || packet == NULL) {
        return AVP_EINVAL;
    }

    while (packet->size < FLAC_MIN_FRAME_HEADER_SIZE) {
        if (packet_offset + packet->size >= stream_end) {
            return AVP_EBADFRAME;
        }
        st = flac_demux_packet_read(demuxer,
                                    packet,
                                    packet_offset,
                                    stream_end,
                                    1u);
        if (st != AVP_OK) {
            return st;
        }
    }

    for (;;) {
        uint32_t parse_size = packet->size > FLAC_FRAME_HEADER_MAX_SIZE ?
                                  FLAC_FRAME_HEADER_MAX_SIZE :
                                  packet->size;

        st = flac_parse_frame_header(packet->buf, parse_size, &frame);
        if (st == AVP_OK) {
            return flac_demux_frame_header_matches(demuxer, &frame) ?
                       AVP_OK :
                       AVP_EBADFRAME;
        }
        if (st != AVP_ELACKFRAME ||
            packet->size >= FLAC_FRAME_HEADER_MAX_SIZE ||
            packet_offset + packet->size >= stream_end) {
            return AVP_EBADFRAME;
        }

        st = flac_demux_packet_read(demuxer,
                                    packet,
                                    packet_offset,
                                    stream_end,
                                    1u);
        if (st != AVP_OK) {
            return st;
        }
    }
}

static avp_status_t flac_demux_find_next_header(flac_demux_t *demuxer,
                                                avp_packet_t *packet,
                                                uint32_t packet_offset,
                                                uint32_t stream_end,
                                                uint32_t *frame_size)
{
    uint32_t scan_pos = FLAC_MIN_FRAME_HEADER_SIZE;
    avp_status_t st;

    if (demuxer == NULL || packet == NULL || frame_size == NULL) {
        return AVP_EINVAL;
    }

    for (;;) {
        while (scan_pos + 1u < packet->size) {
            if (flac_demux_is_sync_pair(packet->buf + scan_pos)) {
                for (;;) {
                    flac_frame_info_t frame;
                    uint32_t available = packet->size - scan_pos;
                    uint32_t parse_size;

                    if (available < FLAC_MIN_FRAME_HEADER_SIZE) {
                        if (packet_offset + packet->size >= stream_end) {
                            break;
                        }
                        st = flac_demux_packet_read(demuxer,
                                                    packet,
                                                    packet_offset,
                                                    stream_end,
                                                    1u);
                        if (st != AVP_OK) {
                            return st;
                        }
                        continue;
                    }

                    parse_size = available > FLAC_FRAME_HEADER_MAX_SIZE ?
                                     FLAC_FRAME_HEADER_MAX_SIZE :
                                     available;
                    st = flac_parse_frame_header(packet->buf + scan_pos,
                                                 parse_size,
                                                 &frame);
                    if (st == AVP_OK) {
                        if (flac_demux_frame_header_matches(demuxer, &frame)) {
                            if (available > FLAC_FRAME_HEADER_MAX_SIZE) {
                                return AVP_ERANGE;
                            }
                            memcpy(demuxer->next_data,
                                   packet->buf + scan_pos,
                                   available);
                            demuxer->next_data_size = available;
                            packet->size = scan_pos;
                            *frame_size = scan_pos;
                            return AVP_OK;
                        }
                        break;
                    }
                    if (st == AVP_ELACKFRAME &&
                        available < FLAC_FRAME_HEADER_MAX_SIZE &&
                        packet_offset + packet->size < stream_end) {
                        st = flac_demux_packet_read(demuxer,
                                                    packet,
                                                    packet_offset,
                                                    stream_end,
                                                    1u);
                        if (st != AVP_OK) {
                            return st;
                        }
                        continue;
                    }
                    break;
                }
            }
            scan_pos++;
        }

        if (packet_offset + packet->size >= stream_end) {
            *frame_size = packet->size;
            return AVP_ENOENT;
        }

        st = flac_demux_packet_read(demuxer,
                                    packet,
                                    packet_offset,
                                    stream_end,
                                    1u);
        if (st != AVP_OK) {
            return st;
        }
    }
}

avp_status_t flac_demux_read_packet(flac_demux_t *demuxer,
                                    avp_packet_t *packet)
{
    uint32_t stream_end;
    uint32_t frame_size = 0;
    uint32_t packet_offset;
    avp_status_t st;

    if (demuxer == NULL || packet == NULL) {
        return AVP_EINVAL;
    }

    if (flac_add_overflow(demuxer->stream_offset,
                          demuxer->stream_size,
                          &stream_end)) {
        return AVP_ERANGE;
    }
    if (demuxer->current_offset >= stream_end) {
        return AVP_ENOENT;
    }

    if (demuxer->next_data_size > FLAC_FRAME_HEADER_MAX_SIZE ||
        demuxer->next_data_size > stream_end - demuxer->current_offset) {
        return AVP_EBADFRAME;
    }

    packet_offset = demuxer->current_offset;
    packet->size = 0u;

    if (demuxer->next_data_size != 0u) {
        st = flac_demux_packet_append(packet,
                                      demuxer->next_data,
                                      demuxer->next_data_size);
        if (st != AVP_OK) {
            return st;
        }
        demuxer->next_data_size = 0u;
    }

    st = flac_demux_validate_current_header(demuxer,
                                            packet,
                                            packet_offset,
                                            stream_end);
    if (st != AVP_OK) {
        return st;
    }

    st = flac_demux_find_next_header(demuxer,
                                     packet,
                                     packet_offset,
                                     stream_end,
                                     &frame_size);
    if (st != AVP_OK && st != AVP_ENOENT) {
        return st;
    }
    if (frame_size < FLAC_MIN_FRAME_HEADER_SIZE || packet->size != frame_size) {
        return AVP_EBADFRAME;
    }

    packet->offset = packet_offset;
    packet->index = demuxer->packet_index++;
    packet->type = AVP_PACKET_TYPE_AUDIO;
    demuxer->current_offset = packet_offset + frame_size;
    return AVP_OK;
}
