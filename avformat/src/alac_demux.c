/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "alac_container.h"

#include "ALACAudioTypes.h"

#define ALAC_CAF_HEADER_SIZE       8u
#define ALAC_CAF_CHUNK_HEADER_SIZE 12u
#define ALAC_CAF_DESC_SIZE         32u
#define ALAC_CAF_DATA_PREFIX_SIZE  4u
#define ALAC_CAF_COOKIE_MAX_SIZE   ALAC_MAGIC_COOKIE_MAX_SIZE
#define ALAC_CAF_PAKT_HEADER_SIZE  24u

#define ALAC_CAF_FOURCC(a, b, c, d)   \
    (((uint32_t)(uint8_t)(a) << 24) | \
     ((uint32_t)(uint8_t)(b) << 16) | \
     ((uint32_t)(uint8_t)(c) << 8) |  \
     (uint32_t)(uint8_t)(d))

#define ALAC_CAF_ID_CAFF ALAC_CAF_FOURCC('c', 'a', 'f', 'f')
#define ALAC_CAF_ID_DESC ALAC_CAF_FOURCC('d', 'e', 's', 'c')
#define ALAC_CAF_ID_KUKI ALAC_CAF_FOURCC('k', 'u', 'k', 'i')
#define ALAC_CAF_ID_DATA ALAC_CAF_FOURCC('d', 'a', 't', 'a')
#define ALAC_CAF_ID_PAKT ALAC_CAF_FOURCC('p', 'a', 'k', 't')
#define ALAC_CAF_ID_ALAC ALAC_CAF_FOURCC('a', 'l', 'a', 'c')

static uint32_t alac_caf_fourcc(const uint8_t *buffer)
{
    return ALAC_CAF_FOURCC(buffer[0], buffer[1], buffer[2], buffer[3]);
}

static avp_status_t alac_caf_read_chunk_header(alac_demux_t *demuxer,
                                               uint32_t *type,
                                               int64_t *size)
{
    uint8_t header[ALAC_CAF_CHUNK_HEADER_SIZE];

    if (demuxer == NULL || demuxer->common.avp_io == NULL ||
        type == NULL || size == NULL) {
        return AVP_EBADHEADER;
    }
    if (avp_io_read(demuxer->common.avp_io, header, sizeof(header)) != AVP_OK) {
        return AVP_IO;
    }

    *type = alac_caf_fourcc(header);
    *size = (int64_t)AVP_GET_BE64(&header[4]);
    return AVP_OK;
}

static avp_status_t alac_caf_parse_ber_uint(const uint8_t *buffer,
                                            uint32_t size,
                                            uint32_t *pos,
                                            uint32_t *value)
{
    uint32_t parsed = 0u;
    uint32_t i;

    if (buffer == NULL || pos == NULL || value == NULL || *pos >= size) {
        return AVP_EBADHEADER;
    }

    for (i = 0u; i < 5u; i++) {
        uint8_t byte;

        if (*pos >= size) {
            return AVP_EBADHEADER;
        }
        byte = buffer[(*pos)++];
        if (parsed > (UINT32_MAX >> 7)) {
            return AVP_ERANGE;
        }
        parsed = (parsed << 7) | (uint32_t)(byte & 0x7fu);
        if ((byte & 0x80u) == 0u) {
            *value = parsed;
            return AVP_OK;
        }
    }

    return AVP_ERANGE;
}

static avp_status_t alac_caf_parse_packet_table(alac_demux_t *demuxer,
                                                const uint8_t *buffer,
                                                uint32_t size,
                                                uint32_t *packet_size_sum)
{
    uint32_t packet_count;
    uint32_t pos;
    uint32_t sum = 0u;
    uint64_t count64;
    uint32_t i;
    avp_status_t st;

    if (demuxer == NULL || buffer == NULL || packet_size_sum == NULL ||
        size < ALAC_CAF_PAKT_HEADER_SIZE) {
        return AVP_EBADHEADER;
    }

    count64 = AVP_GET_BE64(&buffer[0]);
    if (count64 == 0u || count64 > UINT32_MAX) {
        return AVP_EBADHEADER;
    }
    packet_count = (uint32_t)count64;
    demuxer->packet_count = packet_count;

    if (demuxer->bytes_per_packet != 0u) {
        *packet_size_sum = 0u;
        return AVP_OK;
    }

    if (demuxer->packet_sizes != NULL) {
        return AVP_EBADHEADER;
    }

    demuxer->packet_sizes = (uint32_t *)avp_malloc((size_t)packet_count * sizeof(uint32_t));
    if (demuxer->packet_sizes == NULL) {
        return AVP_ENOMEM;
    }

    pos = ALAC_CAF_PAKT_HEADER_SIZE;
    for (i = 0u; i < packet_count; i++) {
        st = alac_caf_parse_ber_uint(buffer, size, &pos, &demuxer->packet_sizes[i]);
        if (st != AVP_OK) {
            return st;
        }
        sum += demuxer->packet_sizes[i];
    }

    if (pos != size && demuxer->frames_per_packet == 0u) {
        uint32_t pair_pos = ALAC_CAF_PAKT_HEADER_SIZE;

        sum = 0u;
        for (i = 0u; i < packet_count; i++) {
            uint32_t frame_count;

            st = alac_caf_parse_ber_uint(buffer, size, &pair_pos, &demuxer->packet_sizes[i]);
            if (st != AVP_OK) {
                return st;
            }
            st = alac_caf_parse_ber_uint(buffer, size, &pair_pos, &frame_count);
            if (st != AVP_OK) {
                return st;
            }
            sum += demuxer->packet_sizes[i];
        }
        pos = pair_pos;
    }

    if (pos != size) {
        return AVP_EBADHEADER;
    }

    *packet_size_sum = sum;
    return AVP_OK;
}

avp_status_t alac_demux_open(alac_demux_t *demuxer, avp_io_t *avp_io)
{
    uint8_t header[ALAC_CAF_HEADER_SIZE];
    uint8_t desc[ALAC_CAF_DESC_SIZE];
    uint8_t cookie[ALAC_CAF_COOKIE_MAX_SIZE];
    uint32_t offset = ALAC_CAF_HEADER_SIZE;
    uint32_t chunk_type;
    uint32_t cookie_size = 0u;
    uint32_t data_offset = 0u;
    uint32_t data_size = 0u;
    uint32_t desc_seen = 0u;
    uint32_t cookie_seen = 0u;
    uint32_t data_seen = 0u;
    uint32_t pakt_seen = 0u;
    uint32_t pakt_size_sum = 0u;
    int64_t chunk_size;
    int64_t file_size;
    avp_status_t st;

    if (demuxer == NULL || avp_io == NULL) {
        return AVP_EINVAL;
    }
    file_size = avp_io_get_size(avp_io);
    if (file_size < (int64_t)ALAC_CAF_HEADER_SIZE || file_size > UINT32_MAX) {
        return AVP_IO;
    }

    memset(demuxer, 0, sizeof(*demuxer));
    demuxer->common.avp_io = avp_io;
    demuxer->common.file_size = (uint32_t)file_size;
    st = avp_io_seek(avp_io, 0);
    if (st != AVP_OK) {
        return st;
    }

    if (avp_io_read(avp_io, header, sizeof(header)) != AVP_OK ||
        alac_caf_fourcc(header) != ALAC_CAF_ID_CAFF ||
        AVP_GET_BE16(&header[4]) != 1u) {
        return AVP_EBADHEADER;
    }

    while (offset < demuxer->common.file_size) {
        uint64_t end;
        uint32_t payload_offset;
        uint32_t payload_size;

        if (demuxer->common.file_size - offset < ALAC_CAF_CHUNK_HEADER_SIZE) {
            st = AVP_EBADHEADER;
            goto fail;
        }
        st = alac_caf_read_chunk_header(demuxer, &chunk_type, &chunk_size);
        if (st != AVP_OK) {
            goto fail;
        }
        payload_offset = offset + ALAC_CAF_CHUNK_HEADER_SIZE;
        if (chunk_size < 0) {
            end = demuxer->common.file_size;
        } else {
            end = (uint64_t)payload_offset + (uint64_t)chunk_size;
        }
        if (end > demuxer->common.file_size || end < payload_offset) {
            st = AVP_EBADHEADER;
            goto fail;
        }
        payload_size = (uint32_t)(end - payload_offset);

        switch (chunk_type) {
            case ALAC_CAF_ID_DESC: {
                double sample_rate;
                uint8_t sample_rate_bytes[sizeof(sample_rate)];
                uint32_t i;

                if (desc_seen != 0u || payload_size != ALAC_CAF_DESC_SIZE ||
                    avp_io_read(avp_io, desc, sizeof(desc)) != AVP_OK) {
                    st = AVP_EBADHEADER;
                    goto fail;
                }
                for (i = 0u; i < sizeof(sample_rate_bytes); i++) {
                    sample_rate_bytes[i] = desc[sizeof(sample_rate_bytes) - 1u - i];
                }
                memcpy(&sample_rate, sample_rate_bytes, sizeof(sample_rate));
                if (alac_caf_fourcc(&desc[8]) != ALAC_CAF_ID_ALAC ||
                    sample_rate <= 0.0 ||
                    sample_rate > UINT32_MAX ||
                    AVP_GET_BE32(&desc[24]) == 0u ||
                    AVP_GET_BE32(&desc[24]) > UINT8_MAX ||
                    AVP_GET_BE32(&desc[28]) > 32u) {
                    st = AVP_EBADHEADER;
                    goto fail;
                }
                demuxer->sample_rate = (uint32_t)sample_rate;
                demuxer->bytes_per_packet = AVP_GET_BE32(&desc[16]);
                demuxer->frames_per_packet = AVP_GET_BE32(&desc[20]);
                demuxer->channels = AVP_GET_BE32(&desc[24]);
                demuxer->bits_per_sample = AVP_GET_BE32(&desc[28]);
                desc_seen = 1u;
                break;
            }

            case ALAC_CAF_ID_KUKI:
                if (cookie_seen != 0u || payload_size < sizeof(ALACSpecificConfig) ||
                    payload_size > ALAC_CAF_COOKIE_MAX_SIZE ||
                    avp_io_read(avp_io, cookie, payload_size) != AVP_OK) {
                    st = AVP_EBADHEADER;
                    goto fail;
                }
                memcpy(demuxer->alac_config.magic_cookie, cookie, payload_size);
                demuxer->alac_config.magic_cookie_size = payload_size;
                cookie_size = payload_size;
                cookie_seen = 1u;
                break;

            case ALAC_CAF_ID_DATA:
                if (data_seen != 0u || payload_size < ALAC_CAF_DATA_PREFIX_SIZE) {
                    st = AVP_EBADHEADER;
                    goto fail;
                }
                data_offset = payload_offset + ALAC_CAF_DATA_PREFIX_SIZE;
                data_size = payload_size - ALAC_CAF_DATA_PREFIX_SIZE;
                data_seen = 1u;
                break;

            case ALAC_CAF_ID_PAKT: {
                uint8_t *pakt;

                if (pakt_seen != 0u || payload_size < ALAC_CAF_PAKT_HEADER_SIZE) {
                    st = AVP_EBADHEADER;
                    goto fail;
                }

                pakt = (uint8_t *)avp_malloc(payload_size);
                if (pakt == NULL) {
                    st = AVP_ENOMEM;
                    goto fail;
                }
                st = avp_io_read(avp_io, pakt, payload_size);
                if (st == AVP_OK) {
                    st = alac_caf_parse_packet_table(demuxer,
                                                     pakt,
                                                     payload_size,
                                                     &pakt_size_sum);
                }
                avp_free(pakt);
                if (st != AVP_OK) {
                    goto fail;
                }
                pakt_seen = 1u;
                break;
            }

            default:
                break;
        }

        if (end > UINT32_MAX) {
            st = AVP_ERANGE;
            goto fail;
        }
        offset = (uint32_t)end;
        st = avp_io_seek(avp_io, offset);
        if (st != AVP_OK) {
            goto fail;
        }
    }

    if (desc_seen == 0u || cookie_seen == 0u || cookie_size == 0u ||
        data_seen == 0u || data_size == 0u) {
        st = AVP_EBADHEADER;
        goto fail;
    }
    if (demuxer->bytes_per_packet == 0u) {
        if (pakt_seen == 0u || demuxer->packet_sizes == NULL ||
            pakt_size_sum != data_size) {
            st = AVP_EBADHEADER;
            goto fail;
        }
    } else if (data_size % demuxer->bytes_per_packet != 0u) {
        st = AVP_EBADHEADER;
        goto fail;
    } else {
        demuxer->packet_count = data_size / demuxer->bytes_per_packet;
    }
    if (demuxer->packet_count == 0u) {
        st = AVP_EBADHEADER;
        goto fail;
    }
    demuxer->common.stream_offset = data_offset;
    demuxer->common.stream_size = data_size;
    demuxer->common.current_offset = demuxer->common.stream_offset;
    st = avp_io_seek(avp_io, demuxer->common.stream_offset);
    if (st != AVP_OK) {
        goto fail;
    }
    return AVP_OK;

fail:
    if (demuxer->packet_sizes != NULL) {
        avp_free(demuxer->packet_sizes);
        demuxer->packet_sizes = NULL;
    }
    return st;
}

void alac_demux_close(alac_demux_t *demuxer)
{
    if (demuxer != NULL) {
        if (demuxer->packet_sizes != NULL) {
            avp_free(demuxer->packet_sizes);
        }
        memset(demuxer, 0, sizeof(*demuxer));
    }
}

avp_status_t alac_demux_get_audio_stream_config(const alac_demux_t *demuxer,
                                                audio_codec_dec_config_t *config)
{
    if (demuxer == NULL || config == NULL) {
        return AVP_EINVAL;
    }

    memset(config, 0, sizeof(*config));
    config->codec_type = AUDIO_CODEC_ID_ALAC;
    config->alac_config = demuxer->alac_config;
    return AVP_OK;
}

avp_status_t alac_demux_read_packet(alac_demux_t *demuxer,
                                    avp_packet_t *packet)
{
    uint32_t stream_end;
    uint32_t packet_size;
    avp_status_t st;

    if (demuxer == NULL || packet == NULL) {
        return AVP_EINVAL;
    }

    stream_end = demuxer->common.stream_offset + demuxer->common.stream_size;
    if (demuxer->common.current_offset >= stream_end ||
        demuxer->common.packet_index >= demuxer->packet_count) {
        return AVP_ENOENT;
    }

    packet_size = demuxer->packet_sizes != NULL ?
                      demuxer->packet_sizes[demuxer->common.packet_index] :
                      demuxer->bytes_per_packet;
    if (packet_size == 0u || packet_size > stream_end - demuxer->common.current_offset) {
        return AVP_EBADFRAME;
    }
    st = avp_packet_expand(packet, packet_size);
    if (st != AVP_OK) {
        return st;
    }
    st = avp_io_read(demuxer->common.avp_io,
                     packet->buf,
                     packet_size);
    if (st != AVP_OK) {
        return st;
    }

    packet->size = packet_size;
    packet->offset = demuxer->common.current_offset;
    packet->index = demuxer->common.packet_index++;
    packet->type = AVP_PACKET_TYPE_AUDIO;
    demuxer->common.current_offset += packet_size;
    return AVP_OK;
}
