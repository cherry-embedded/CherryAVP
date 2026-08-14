/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ogg_container.h"

#define OGG_PAGE_HEADER_MIN_SIZE 27u
#define OGG_PAGE_CAPTURE_SIZE    4u
#define OGG_PAGE_SEGMENT_END     255u

static avp_status_t ogg_read_packet_raw(ogg_demux_t *demuxer,
                                        avp_packet_t *packet,
                                        uint32_t *packet_offset);
static avp_status_t ogg_read_page(ogg_demux_t *demuxer);

static avp_status_t ogg_parse_comment_packet(const uint8_t *packet,
                                             uint32_t size,
                                             uint32_t prefix_size,
                                             char *vendor,
                                             uint32_t vendor_size,
                                             uint32_t *comment_count)
{
    uint32_t pos;
    uint32_t vendor_length;
    uint32_t copy_size;

    if (packet == NULL || vendor == NULL || vendor_size == 0u ||
        comment_count == NULL || size < prefix_size + 8u) {
        return AVP_EBADHEADER;
    }

    pos = prefix_size;
    vendor_length = AVP_GET_LE32(packet + pos);
    pos += 4u;
    if (vendor_length > size - pos) {
        return AVP_EBADHEADER;
    }

    copy_size = vendor_length;
    if (copy_size >= vendor_size) {
        copy_size = vendor_size - 1u;
    }
    memcpy(vendor, packet + pos, copy_size);
    vendor[copy_size] = '\0';
    pos += vendor_length;

    if (size - pos < 4u) {
        return AVP_EBADHEADER;
    }

    *comment_count = AVP_GET_LE32(packet + pos);
    return AVP_OK;
}

#if defined(CONFIG_CHERRYAVP_OPUS)
static int ogg_opus_is_header(const uint8_t *packet, uint32_t size)
{
    return packet != NULL &&
           size >= OPUS_CODEC_MAGIC_SIZE &&
           memcmp(packet, OPUS_CODEC_MAGIC_HEAD, OPUS_CODEC_MAGIC_SIZE) == 0;
}

static int ogg_opus_is_tags(const uint8_t *packet, uint32_t size)
{
    return packet != NULL &&
           size >= OPUS_CODEC_MAGIC_SIZE &&
           memcmp(packet, OPUS_CODEC_MAGIC_TAGS, OPUS_CODEC_MAGIC_SIZE) == 0;
}

static avp_status_t ogg_opus_parse_header_packet(const uint8_t *packet,
                                                 uint32_t size,
                                                 opus_header_info_t *info)
{
    uint8_t channels;
    uint8_t mapping_family;
    uint8_t stream_count;
    uint8_t coupled_count;
    uint32_t i;

    if (packet == NULL || info == NULL) {
        return AVP_EINVAL;
    }

    if (!ogg_opus_is_header(packet, size) || size < OPUS_DECODE_HEAD_SIZE) {
        return AVP_EBADHEADER;
    }

    if ((packet[8] & 0xf0u) != 0u) {
        return AVP_EUNSUPPORTED;
    }

    channels = packet[9];
    if (channels == 0u) {
        return AVP_EBADHEADER;
    }

    memset(info, 0, sizeof(*info));
    info->channels = channels;
    info->pre_skip = AVP_GET_LE16(&packet[10]);
    info->sample_rate = AVP_GET_LE32(&packet[12]);
    if (info->sample_rate == 0u) {
        info->sample_rate = OPUS_CODEC_GRANULE_SAMPLE_RATE;
    }
    mapping_family = packet[18];
    info->mapping_family = mapping_family;

    if (mapping_family != 0u) {
        return AVP_EUNSUPPORTED;
    }

    if (mapping_family == 0u) {
        if (channels > 2u) {
            return AVP_EBADHEADER;
        }
        info->stream_count = 1u;
        info->coupled_count = channels == 2u ? 1u : 0u;
        info->mapping[0] = 0u;
        if (channels == 2u) {
            info->mapping[1] = 1u;
        }
        return AVP_OK;
    }

    if (size < OPUS_DECODE_HEAD_SIZE + 2u + (uint32_t)channels) {
        return AVP_EBADHEADER;
    }

    stream_count = packet[19];
    coupled_count = packet[20];
    if (stream_count == 0u || coupled_count > stream_count ||
        (uint32_t)stream_count + coupled_count > 255u) {
        return AVP_EBADHEADER;
    }

    info->stream_count = stream_count;
    info->coupled_count = coupled_count;
    for (i = 0u; i < (uint32_t)channels; i++) {
        uint8_t map = packet[21u + i];

        if (map != 255u && map >= (uint8_t)(stream_count + coupled_count)) {
            return AVP_EBADHEADER;
        }
        info->mapping[i] = map;
    }

    return AVP_OK;
}
#endif

#if defined(CONFIG_CHERRYAVP_VORBIS)
static int ogg_vorbis_is_header(const uint8_t *packet,
                                uint32_t size,
                                uint8_t header_type)
{
    return packet != NULL &&
           size >= VORBIS_DECODE_HEADER_SIZE &&
           packet[0] == header_type &&
           memcmp(packet + 1, VORBIS_CODEC_MAGIC, VORBIS_CODEC_MAGIC_SIZE) == 0;
}

static avp_status_t ogg_vorbis_parse_identification_packet(const uint8_t *packet,
                                                           uint32_t size,
                                                           vorbis_header_info_t *info)
{
    uint32_t version;

    if (packet == NULL || info == NULL) {
        return AVP_EINVAL;
    }

    if (!ogg_vorbis_is_header(packet, size, 0x01u) || size < 30u) {
        return AVP_EBADHEADER;
    }

    version = AVP_GET_LE32(packet + 7u);
    if (version != 0u || packet[11] == 0u ||
        AVP_GET_LE32(packet + 12u) == 0u ||
        (packet[29] & 0x01u) == 0u) {
        return AVP_EBADHEADER;
    }

    memset(info, 0, sizeof(*info));
    info->channels = packet[11];
    info->sample_rate = AVP_GET_LE32(packet + 12u);
    info->bitrate_upper = (int32_t)AVP_GET_LE32(packet + 16u);
    info->bitrate_nominal = (int32_t)AVP_GET_LE32(packet + 20u);
    info->bitrate_lower = (int32_t)AVP_GET_LE32(packet + 24u);
    return AVP_OK;
}

static uint8_t *ogg_vorbis_dup_packet(const uint8_t *packet, uint32_t size)
{
    uint8_t *copy;

    if (packet == NULL || size == 0u) {
        return NULL;
    }

    copy = (uint8_t *)avp_malloc(size);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, packet, size);
    return copy;
}
#endif

static avp_status_t ogg_demux_set_stream_bounds(ogg_demux_t *demuxer)
{
    avp_status_t st;

    if (demuxer == NULL) {
        return AVP_EINVAL;
    }

    if (!demuxer->page_loaded ||
        demuxer->segment_index >= demuxer->segment_count) {
        st = ogg_read_page(demuxer);
        if (st != AVP_OK) {
            return st == AVP_ENOENT ? AVP_EBADHEADER : st;
        }
    }

    if (demuxer->body_pos > demuxer->body_size ||
        demuxer->body_offset > demuxer->common.file_size ||
        demuxer->body_pos > demuxer->common.file_size - demuxer->body_offset) {
        return AVP_EBADHEADER;
    }

    demuxer->common.stream_offset = demuxer->body_offset + demuxer->body_pos;
    demuxer->common.stream_size = demuxer->common.file_size - demuxer->common.stream_offset;
    demuxer->common.current_offset = demuxer->common.stream_offset;
    return AVP_OK;
}

static avp_status_t ogg_demux_parse_file_headers(ogg_demux_t *demuxer)
{
    avp_packet_t *packet;
    uint32_t packet_offset;
    avp_status_t st;

    if (demuxer == NULL) {
        return AVP_EINVAL;
    }

    packet = avp_packet_alloc(4096u);
    if (packet == NULL) {
        return AVP_ENOMEM;
    }

    while (!demuxer->header_parsed) {
        packet->size = 0u;
        packet_offset = 0u;
        st = ogg_read_packet_raw(demuxer, packet, &packet_offset);
        if (st != AVP_OK) {
            goto out;
        }

        if (demuxer->header_parse_step == 0u) {
#if defined(CONFIG_CHERRYAVP_OPUS)
            if (ogg_opus_is_header(packet->buf, packet->size)) {
                demuxer->format = OGG_FORMAT_OPUS;
                demuxer->header_packet_count = 2u;
                st = ogg_opus_parse_header_packet(packet->buf, packet->size, &demuxer->opus);
                if (st != AVP_OK) {
                    goto out;
                }
                demuxer->header_parse_step = 1u;
#endif
#if defined(CONFIG_CHERRYAVP_VORBIS)
            } else if (ogg_vorbis_is_header(packet->buf, packet->size, 0x01u)) {
                demuxer->format = OGG_FORMAT_VORBIS;
                demuxer->header_packet_count = 3u;
                st = ogg_vorbis_parse_identification_packet(packet->buf,
                                                            packet->size,
                                                            &demuxer->vorbis);
                if (st != AVP_OK) {
                    goto out;
                }
                demuxer->vorbis_headers.identification = ogg_vorbis_dup_packet(packet->buf,
                                                                               packet->size);
                demuxer->vorbis_headers.identification_size = packet->size;
                if (demuxer->vorbis_headers.identification == NULL) {
                    st = AVP_ENOMEM;
                    goto out;
                }
                demuxer->header_parse_step = 1u;
#endif
            } else {
                st = AVP_EUNSUPPORTED;
                goto out;
            }
            continue;
        }

#if defined(CONFIG_CHERRYAVP_OPUS)
        if (demuxer->format == OGG_FORMAT_OPUS) {
            if (demuxer->header_parse_step != 1u ||
                !ogg_opus_is_tags(packet->buf, packet->size)) {
                st = AVP_EBADHEADER;
                goto out;
            }

            st = ogg_parse_comment_packet(packet->buf,
                                          packet->size,
                                          OPUS_CODEC_MAGIC_SIZE,
                                          demuxer->vendor,
                                          sizeof(demuxer->vendor),
                                          &demuxer->comment_count);
            if (st != AVP_OK) {
                goto out;
            }
            demuxer->header_parsed = 1u;
            break;
        }
#endif

#if defined(CONFIG_CHERRYAVP_VORBIS)
        if (demuxer->format == OGG_FORMAT_VORBIS) {
            if (demuxer->header_parse_step == 1u) {
                if (!ogg_vorbis_is_header(packet->buf, packet->size, 0x03u)) {
                    st = AVP_EBADHEADER;
                    goto out;
                }
                demuxer->vorbis_headers.comment = ogg_vorbis_dup_packet(packet->buf,
                                                                        packet->size);
                demuxer->vorbis_headers.comment_size = packet->size;
                if (demuxer->vorbis_headers.comment == NULL) {
                    st = AVP_ENOMEM;
                    goto out;
                }

                st = ogg_parse_comment_packet(packet->buf,
                                              packet->size,
                                              VORBIS_DECODE_HEADER_SIZE,
                                              demuxer->vendor,
                                              sizeof(demuxer->vendor),
                                              &demuxer->comment_count);
                if (st != AVP_OK) {
                    goto out;
                }
                demuxer->header_parse_step = 2u;
                continue;
            }

            if (demuxer->header_parse_step != 2u ||
                !ogg_vorbis_is_header(packet->buf, packet->size, 0x05u)) {
                st = AVP_EBADHEADER;
                goto out;
            }
            demuxer->vorbis_headers.setup = ogg_vorbis_dup_packet(packet->buf,
                                                                  packet->size);
            demuxer->vorbis_headers.setup_size = packet->size;
            if (demuxer->vorbis_headers.setup == NULL) {
                st = AVP_ENOMEM;
                goto out;
            }
            demuxer->header_parsed = 1u;
            break;
        }
#endif

        st = AVP_EUNSUPPORTED;
        goto out;
    }

    demuxer->packet_count = demuxer->header_packet_count;
    demuxer->common.packet_index = 0u;
    st = ogg_demux_set_stream_bounds(demuxer);

out:
    avp_packet_free(packet);
    return st;
}

static avp_status_t ogg_read_page(ogg_demux_t *demuxer)
{
    uint8_t header[OGG_PAGE_HEADER_MIN_SIZE + OGG_MAX_PAGE_SEGMENTS];
    uint32_t segment_count;
    uint32_t header_size;
    uint32_t body_size;
    uint32_t serial;
    uint32_t i;
    avp_status_t st;

    if (demuxer == NULL || demuxer->common.avp_io == NULL) {
        return AVP_EINVAL;
    }

    for (;;) {
        if (demuxer->next_offset >= demuxer->common.file_size) {
            return AVP_ENOENT;
        }
        if (demuxer->common.file_size - demuxer->next_offset < OGG_PAGE_HEADER_MIN_SIZE) {
            return AVP_EBADHEADER;
        }

        st = avp_io_read(demuxer->common.avp_io, header, OGG_PAGE_HEADER_MIN_SIZE);
        if (st != AVP_OK) {
            return st;
        }
        if (memcmp(header, "OggS", OGG_PAGE_CAPTURE_SIZE) != 0 || header[4] != 0u) {
            return AVP_EBADHEADER;
        }

        segment_count = header[26];
        header_size = OGG_PAGE_HEADER_MIN_SIZE + segment_count;
        if (demuxer->common.file_size - demuxer->next_offset < header_size) {
            return AVP_EBADHEADER;
        }
        if (segment_count != 0u) {
            st = avp_io_read(demuxer->common.avp_io,
                             &header[OGG_PAGE_HEADER_MIN_SIZE],
                             segment_count);
            if (st != AVP_OK) {
                return st;
            }
        }

        body_size = 0u;
        for (i = 0u; i < segment_count; i++) {
            body_size += header[OGG_PAGE_HEADER_MIN_SIZE + i];
        }
        if (demuxer->common.file_size - demuxer->next_offset < header_size ||
            demuxer->common.file_size - demuxer->next_offset - header_size < body_size) {
            return AVP_EBADHEADER;
        }

        serial = AVP_GET_LE32(&header[14]);
        if (!demuxer->stream_serial_valid) {
            demuxer->stream_serial = serial;
            demuxer->stream_serial_valid = 1u;
        } else if (serial != demuxer->stream_serial) {
            return AVP_EUNSUPPORTED;
        }

        if (body_size > demuxer->page_body_capacity) {
            uint8_t *body = (uint8_t *)avp_realloc(demuxer->page_body, body_size);

            if (body == NULL) {
                return AVP_ENOMEM;
            }
            demuxer->page_body = body;
            demuxer->page_body_capacity = body_size;
        }

        if (body_size != 0u) {
            st = avp_io_read(demuxer->common.avp_io, demuxer->page_body, body_size);
            if (st != AVP_OK) {
                return st;
            }
        }

        demuxer->page_offset = demuxer->next_offset;
        demuxer->body_offset = demuxer->next_offset + header_size;
        demuxer->body_size = body_size;
        demuxer->body_pos = 0u;
        demuxer->segment_count = (uint8_t)segment_count;
        demuxer->segment_index = 0u;
        demuxer->page_loaded = 1u;
        memcpy(demuxer->segments,
               &header[OGG_PAGE_HEADER_MIN_SIZE],
               segment_count);
        demuxer->next_offset = demuxer->body_offset + body_size;
        demuxer->page_count++;

        if (body_size != 0u) {
            return AVP_OK;
        }
    }
}

static avp_status_t ogg_read_packet_raw(ogg_demux_t *demuxer,
                                        avp_packet_t *packet,
                                        uint32_t *packet_offset)
{
    uint32_t first_offset = 0u;
    uint8_t have_segment = 0u;
    avp_status_t st;

    if (demuxer == NULL || packet == NULL || packet_offset == NULL) {
        return AVP_EINVAL;
    }

    packet->size = 0u;
    *packet_offset = 0u;

    for (;;) {
        uint32_t segment_size;

        if (!demuxer->page_loaded ||
            demuxer->segment_index >= demuxer->segment_count) {
            st = ogg_read_page(demuxer);
            if (st != AVP_OK) {
                return have_segment ? AVP_EBADHEADER : st;
            }
        }

        segment_size = demuxer->segments[demuxer->segment_index];
        if (!have_segment) {
            first_offset = demuxer->body_offset + demuxer->body_pos;
            have_segment = 1u;
        }

        if (segment_size > demuxer->body_size - demuxer->body_pos) {
            return AVP_EBADHEADER;
        }
        if (segment_size != 0u) {
            if (packet->size > UINT32_MAX - segment_size) {
                return AVP_ERANGE;
            }
            st = avp_packet_expand(packet, packet->size + segment_size);
            if (st != AVP_OK) {
                return st;
            }
            memcpy(packet->buf + packet->size,
                   demuxer->page_body + demuxer->body_pos,
                   segment_size);
        }

        packet->size += segment_size;
        demuxer->body_pos += segment_size;
        demuxer->segment_index++;

        if (segment_size < OGG_PAGE_SEGMENT_END) {
            if (packet->size == 0u) {
                have_segment = 0u;
                first_offset = 0u;
                continue;
            }

            demuxer->packet_offset = first_offset;
            *packet_offset = first_offset;
            return AVP_OK;
        }
    }
}

avp_status_t ogg_demux_open(ogg_demux_t *demuxer,
                            avp_io_t *avp_io)
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

    st = avp_io_seek(avp_io, 0u);
    if (st != AVP_OK) {
        return st;
    }
    st = ogg_demux_parse_file_headers(demuxer);
    if (st != AVP_OK) {
        goto fail;
    }
    demuxer->packet_count = demuxer->header_packet_count;
    demuxer->common.packet_index = 0u;

    return AVP_OK;
fail:
#if defined(CONFIG_CHERRYAVP_VORBIS)
    if (demuxer->vorbis_headers.identification != NULL) {
        avp_free((void *)demuxer->vorbis_headers.identification);
    }
    if (demuxer->vorbis_headers.comment != NULL) {
        avp_free((void *)demuxer->vorbis_headers.comment);
    }
    if (demuxer->vorbis_headers.setup != NULL) {
        avp_free((void *)demuxer->vorbis_headers.setup);
    }
#endif
    if (demuxer->page_body != NULL) {
        avp_free(demuxer->page_body);
    }
    return st;
}

void ogg_demux_close(ogg_demux_t *demuxer)
{
    if (demuxer == NULL) {
        return;
    }

#if defined(CONFIG_CHERRYAVP_VORBIS)
    if (demuxer->vorbis_headers.identification != NULL) {
        avp_free((void *)demuxer->vorbis_headers.identification);
    }
    if (demuxer->vorbis_headers.comment != NULL) {
        avp_free((void *)demuxer->vorbis_headers.comment);
    }
    if (demuxer->vorbis_headers.setup != NULL) {
        avp_free((void *)demuxer->vorbis_headers.setup);
    }
#endif
    if (demuxer->page_body != NULL) {
        avp_free(demuxer->page_body);
    }
    memset(demuxer, 0, sizeof(*demuxer));
}

avp_status_t ogg_demux_get_audio_stream_config(const ogg_demux_t *demuxer,
                                               audio_codec_dec_config_t *config)
{
    if (demuxer == NULL || config == NULL) {
        return AVP_EINVAL;
    }

#if defined(CONFIG_CHERRYAVP_OPUS)
    if (demuxer->format == OGG_FORMAT_OPUS) {
        config->codec_type = AUDIO_CODEC_ID_OPUS;
        config->opus_config.sample_rate = demuxer->opus.sample_rate;
        config->opus_config.channels = demuxer->opus.channels;
        return AVP_OK;
    }
#endif

#if defined(CONFIG_CHERRYAVP_VORBIS)
    if (demuxer->format == OGG_FORMAT_VORBIS) {
        if (demuxer->vorbis_headers.identification == NULL ||
            demuxer->vorbis_headers.comment == NULL ||
            demuxer->vorbis_headers.setup == NULL) {
            return AVP_EBADHEADER;
        }
        config->codec_type = AUDIO_CODEC_ID_VORBIS;
        config->vorbis_config.identification = demuxer->vorbis_headers.identification;
        config->vorbis_config.identification_size = demuxer->vorbis_headers.identification_size;
        config->vorbis_config.comment = demuxer->vorbis_headers.comment;
        config->vorbis_config.comment_size = demuxer->vorbis_headers.comment_size;
        config->vorbis_config.setup = demuxer->vorbis_headers.setup;
        config->vorbis_config.setup_size = demuxer->vorbis_headers.setup_size;
        return AVP_OK;
    }
#endif

    return AVP_EUNSUPPORTED;
}

avp_status_t ogg_demux_read_packet(ogg_demux_t *demuxer,
                                   avp_packet_t *packet)
{
    uint32_t packet_offset;
    avp_status_t st;

    if (demuxer == NULL || packet == NULL) {
        return AVP_EINVAL;
    }

    st = ogg_read_packet_raw(demuxer, packet, &packet_offset);
    if (st != AVP_OK) {
        return st;
    }

    packet->offset = packet_offset;
    packet->index = demuxer->common.packet_index++;
    packet->type = AVP_PACKET_TYPE_AUDIO;
    demuxer->common.current_offset = demuxer->next_offset;
    demuxer->packet_count++;
    return AVP_OK;
}

const char *ogg_format_name(ogg_format_t format)
{
    switch (format) {
        case OGG_FORMAT_OPUS:
            return "Opus";
        case OGG_FORMAT_VORBIS:
            return "Vorbis";
        case OGG_FORMAT_FLAC:
            return "FLAC";
        default:
            return "unknown";
    }
}
