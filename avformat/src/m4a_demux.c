/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "m4a_container.h"

#include "ALACAudioTypes.h"

#define M4A_BOX_TYPE(a, b, c, d)    AUDIO_CODEC_FOURCC(a, b, c, d)
#define M4A_OBJECT_TYPE_MPEG4_AUDIO 0x40u
#define M4A_OBJECT_TYPE_AAC_MAIN    0x66u
#define M4A_OBJECT_TYPE_AAC_LC      0x67u
#define M4A_OBJECT_TYPE_AAC_SSR     0x68u

typedef struct {
    uint32_t first_chunk;
    uint32_t samples_per_chunk;
    uint32_t sample_description_index;
} m4a_stsc_entry_t;

typedef struct {
    uint32_t start;
    uint32_t size;
    uint32_t header_size;
    uint32_t type;
} m4a_box_t;

typedef struct {
    uint8_t is_audio;
    uint8_t has_mp4a;
    uint8_t has_esds;

    uint32_t stsc_count;
    m4a_stsc_entry_t *stsc;

    uint32_t chunk_count;
    uint32_t *chunk_offsets;
} m4a_track_t;

static void m4a_track_deinit(m4a_track_t *track)
{
    if (track == NULL) {
        return;
    }

    if (track->stsc != NULL) {
        avp_free(track->stsc);
    }
    if (track->chunk_offsets != NULL) {
        avp_free(track->chunk_offsets);
    }
    memset(track, 0, sizeof(*track));
}

static const uint32_t m4a_aac_sample_rate_table[16] = {
    96000u, 88200u, 64000u, 48000u,
    44100u, 32000u, 24000u, 22050u,
    16000u, 12000u, 11025u, 8000u,
    7350u, 0u, 0u, 0u
};

static uint8_t m4a_aac_channel_config_to_channels(uint32_t channel_config)
{
    static const uint8_t channel_count_table[8] = {
        0u, 1u, 2u, 3u, 4u, 5u, 6u, 8u
    };

    if (channel_config >= (sizeof(channel_count_table) / sizeof(channel_count_table[0]))) {
        return 0u;
    }
    return channel_count_table[channel_config];
}

static int m4a_aac_read_audio_object_type(avp_bitreader_t *br, uint32_t *audio_object_type)
{
    uint32_t object_type;

    if (!avp_bitreader_read(br, 5u, &object_type)) {
        return 0;
    }
    if (object_type == 31u) {
        uint32_t extension;

        if (!avp_bitreader_read(br, 6u, &extension)) {
            return 0;
        }
        object_type = 32u + extension;
    }

    *audio_object_type = object_type;
    return 1;
}

static int m4a_aac_read_sample_rate(avp_bitreader_t *br, uint32_t *sample_rate)
{
    uint32_t index;

    if (!avp_bitreader_read(br, 4u, &index)) {
        return 0;
    }
    if (index == 0x0fu) {
        return avp_bitreader_read(br, 24u, sample_rate) && *sample_rate != 0u;
    }

    *sample_rate = m4a_aac_sample_rate_table[index];
    return *sample_rate != 0u;
}

static int m4a_aac_parse_program_config(avp_bitreader_t *br, uint8_t *channels)
{
    uint32_t front_count;
    uint32_t side_count;
    uint32_t back_count;
    uint32_t lfe_count;
    uint32_t assoc_count;
    uint32_t cc_count;
    uint32_t present;
    uint32_t value;
    uint32_t channel_count = 0u;
    uint32_t i;

    if (!avp_bitreader_skip(br, 4u) ||
        !avp_bitreader_skip(br, 2u) ||
        !avp_bitreader_skip(br, 4u) ||
        !avp_bitreader_read(br, 4u, &front_count) ||
        !avp_bitreader_read(br, 4u, &side_count) ||
        !avp_bitreader_read(br, 4u, &back_count) ||
        !avp_bitreader_read(br, 2u, &lfe_count) ||
        !avp_bitreader_read(br, 3u, &assoc_count) ||
        !avp_bitreader_read(br, 4u, &cc_count)) {
        return 0;
    }

    if (!avp_bitreader_read(br, 1u, &present) ||
        (present != 0u && !avp_bitreader_skip(br, 4u)) ||
        !avp_bitreader_read(br, 1u, &present) ||
        (present != 0u && !avp_bitreader_skip(br, 4u)) ||
        !avp_bitreader_read(br, 1u, &present) ||
        (present != 0u && !avp_bitreader_skip(br, 3u))) {
        return 0;
    }

    for (i = 0u; i < front_count + side_count + back_count; i++) {
        if (!avp_bitreader_read(br, 1u, &value) || !avp_bitreader_skip(br, 4u)) {
            return 0;
        }
        channel_count += value != 0u ? 2u : 1u;
    }
    channel_count += lfe_count;

    if (!avp_bitreader_skip(br, lfe_count * 4u) ||
        !avp_bitreader_skip(br, assoc_count * 4u) ||
        !avp_bitreader_skip(br, cc_count * 5u)) {
        return 0;
    }

    if ((br->bitpos & 7u) != 0u && !avp_bitreader_skip(br, 8u - (br->bitpos & 7u))) {
        return 0;
    }
    if (!avp_bitreader_read(br, 8u, &value) || !avp_bitreader_skip(br, value * 8u) ||
        channel_count == 0u || channel_count > UINT8_MAX) {
        return 0;
    }

    *channels = (uint8_t)channel_count;
    return 1;
}

static int m4a_aac_parse_ga_specific_config(avp_bitreader_t *br,
                                            uint32_t audio_object_type,
                                            uint32_t channel_config,
                                            uint8_t *channels)
{
    uint32_t frame_length_flag;
    uint32_t depends_on_core_coder;
    uint32_t extension_flag;

    if (!avp_bitreader_read(br, 1u, &frame_length_flag) ||
        !avp_bitreader_read(br, 1u, &depends_on_core_coder)) {
        return 0;
    }
    /* PVMP4SetAudioConfig currently fixes the AAC frame length at 1024. */
    if (frame_length_flag != 0u ||
        (depends_on_core_coder != 0u && !avp_bitreader_skip(br, 14u)) ||
        !avp_bitreader_read(br, 1u, &extension_flag)) {
        return 0;
    }

    if (channel_config == 0u) {
        if (!m4a_aac_parse_program_config(br, channels)) {
            return 0;
        }
    } else {
        *channels = m4a_aac_channel_config_to_channels(channel_config);
    }

    if ((audio_object_type == AAC_AUDIO_OBJECT_TYPE_AAC_SCALABLE ||
         audio_object_type == AAC_AUDIO_OBJECT_TYPE_ER_AAC_SCALABLE) &&
        !avp_bitreader_skip(br, 3u)) {
        return 0;
    }
    if (extension_flag != 0u) {
        if (audio_object_type == AAC_AUDIO_OBJECT_TYPE_ER_BSAC &&
            !avp_bitreader_skip(br, 16u)) {
            return 0;
        }
        if ((audio_object_type == AAC_AUDIO_OBJECT_TYPE_ER_AAC_LC ||
             audio_object_type == AAC_AUDIO_OBJECT_TYPE_ER_AAC_LTP ||
             audio_object_type == AAC_AUDIO_OBJECT_TYPE_ER_AAC_SCALABLE ||
             audio_object_type == AAC_AUDIO_OBJECT_TYPE_ER_AAC_LD) &&
            !avp_bitreader_skip(br, 3u)) {
            return 0;
        }
        if (!avp_bitreader_skip(br, 1u)) {
            return 0;
        }
    }

    return *channels != 0u;
}

static avp_status_t m4a_aac_get_upsampling_factor(uint32_t core_sample_rate,
                                                  uint32_t output_sample_rate,
                                                  uint8_t *factor)
{
    if (output_sample_rate == core_sample_rate) {
        *factor = 1u;
        return AVP_OK;
    }
    if (core_sample_rate <= UINT32_MAX / 2u &&
        output_sample_rate == core_sample_rate * 2u) {
        *factor = 2u;
        return AVP_OK;
    }
    return AVP_EUNSUPPORTED;
}

static avp_status_t m4a_parse_audio_specific_config(const uint8_t *buffer,
                                                    uint32_t size,
                                                    aac_dec_config_t *config)
{
    avp_bitreader_t br;
    uint32_t declared_audio_object_type;
    uint32_t core_audio_object_type;
    uint32_t effective_audio_object_type;
    uint32_t channel_config;
    uint32_t core_sample_rate;
    uint32_t extension_sample_rate = 0u;
    uint32_t value;
    uint8_t channels;
    uint8_t upsampling_factor = 1u;
    avp_status_t st;

    if (buffer == NULL || size == 0u || config == NULL) {
        return AVP_EINVAL;
    }

    avp_bitreader_init(&br, buffer, size);
    if (!m4a_aac_read_audio_object_type(&br, &declared_audio_object_type) ||
        !m4a_aac_read_sample_rate(&br, &core_sample_rate) ||
        !avp_bitreader_read(&br, 4u, &channel_config)) {
        return AVP_EBADHEADER;
    }

    core_audio_object_type = declared_audio_object_type;
    effective_audio_object_type = declared_audio_object_type;
    if (declared_audio_object_type == AAC_AUDIO_OBJECT_TYPE_SBR ||
        declared_audio_object_type == AAC_AUDIO_OBJECT_TYPE_PS) {
        if (!m4a_aac_read_sample_rate(&br, &extension_sample_rate) ||
            !m4a_aac_read_audio_object_type(&br, &core_audio_object_type)) {
            return AVP_EBADHEADER;
        }
        st = m4a_aac_get_upsampling_factor(core_sample_rate,
                                           extension_sample_rate,
                                           &upsampling_factor);
        if (st != AVP_OK) {
            return st;
        }
    }

    if (core_audio_object_type != AAC_AUDIO_OBJECT_TYPE_AAC_LC) {
        return AVP_EUNSUPPORTED;
    }
    if (!m4a_aac_parse_ga_specific_config(&br,
                                          core_audio_object_type,
                                          channel_config,
                                          &channels)) {
        return AVP_EBADHEADER;
    }

    if (declared_audio_object_type != AAC_AUDIO_OBJECT_TYPE_SBR &&
        declared_audio_object_type != AAC_AUDIO_OBJECT_TYPE_PS &&
        avp_bitreader_bits_left(&br) >= 16u &&
        avp_bitreader_read(&br, 11u, &value) && value == 0x2b7u) {
        uint32_t extension_audio_object_type;
        uint32_t sbr_present;

        if (!m4a_aac_read_audio_object_type(&br, &extension_audio_object_type)) {
            return AVP_EBADHEADER;
        }
        if (extension_audio_object_type == AAC_AUDIO_OBJECT_TYPE_SBR) {
            if (!avp_bitreader_read(&br, 1u, &sbr_present)) {
                return AVP_EBADHEADER;
            }
            if (sbr_present != 0u) {
                if (!m4a_aac_read_sample_rate(&br, &extension_sample_rate)) {
                    return AVP_EBADHEADER;
                }
                st = m4a_aac_get_upsampling_factor(core_sample_rate,
                                                   extension_sample_rate,
                                                   &upsampling_factor);
                if (st != AVP_OK) {
                    return st;
                }
                effective_audio_object_type = AAC_AUDIO_OBJECT_TYPE_SBR;

                if (avp_bitreader_bits_left(&br) >= 12u &&
                    avp_bitreader_read(&br, 11u, &value) && value == 0x548u) {
                    uint32_t ps_present;

                    if (!avp_bitreader_read(&br, 1u, &ps_present)) {
                        return AVP_EBADHEADER;
                    }
                    if (ps_present != 0u) {
                        effective_audio_object_type = AAC_AUDIO_OBJECT_TYPE_PS;
                    }
                }
            }
        }
    }

    if (channels == 0u || channels > 2u ||
        (effective_audio_object_type != AAC_AUDIO_OBJECT_TYPE_AAC_LC &&
         effective_audio_object_type != AAC_AUDIO_OBJECT_TYPE_SBR &&
         effective_audio_object_type != AAC_AUDIO_OBJECT_TYPE_PS)) {
        return AVP_EUNSUPPORTED;
    }

    if (upsampling_factor != 1u) {
        return AVP_EUNSUPPORTED;
    }

    memset(config, 0, sizeof(*config));
    config->has_no_adts_header = true;
    config->sample_rate = core_sample_rate;
    config->channels = channels;
    return AVP_OK;
}

#define M4A_READ_MEMORY_BOX(_buffer, _buffer_size, _current, _end, _box, _status) \
    do {                                                                          \
        const uint8_t *m4a_mem_buffer = (_buffer);                                \
        uint32_t m4a_mem_buffer_size = (_buffer_size);                            \
        uint32_t m4a_current = (_current);                                        \
        uint32_t m4a_end = (_end);                                                \
        m4a_box_t *m4a_box = (_box);                                              \
        uint64_t m4a_size;                                                        \
        uint32_t m4a_header_size = 8u;                                            \
                                                                                  \
        if (m4a_mem_buffer == NULL || m4a_box == NULL ||                          \
            m4a_current > m4a_end || m4a_end > m4a_mem_buffer_size) {             \
            (_status) = AVP_EINVAL;                                               \
        } else if (m4a_end - m4a_current < 8u) {                                  \
            (_status) = AVP_EBADHEADER;                                           \
        } else {                                                                  \
            m4a_size = AVP_GET_BE32(m4a_mem_buffer + m4a_current);                \
            memset(m4a_box, 0, sizeof(*m4a_box));                                 \
            m4a_box->start = m4a_current;                                         \
            m4a_box->type = M4A_BOX_TYPE(m4a_mem_buffer[m4a_current + 4u],        \
                                         m4a_mem_buffer[m4a_current + 5u],        \
                                         m4a_mem_buffer[m4a_current + 6u],        \
                                         m4a_mem_buffer[m4a_current + 7u]);       \
            if (m4a_size == 1u) {                                                 \
                if (m4a_end - m4a_current < 16u) {                                \
                    (_status) = AVP_EBADHEADER;                                   \
                    break;                                                        \
                }                                                                 \
                m4a_size = AVP_GET_BE64(m4a_mem_buffer + m4a_current + 8u);       \
                m4a_header_size = 16u;                                            \
            } else if (m4a_size == 0u) {                                          \
                m4a_size = m4a_end - m4a_current;                                 \
            }                                                                     \
            if (m4a_size < m4a_header_size || m4a_size > m4a_end - m4a_current || \
                m4a_size > UINT32_MAX) {                                          \
                (_status) = AVP_EBADHEADER;                                       \
            } else {                                                              \
                m4a_box->size = (uint32_t)m4a_size;                               \
                m4a_box->header_size = m4a_header_size;                           \
                (_status) = AVP_OK;                                               \
            }                                                                     \
        }                                                                         \
    } while (0)

static avp_status_t m4a_parse_box_header(m4a_demux_t *demuxer,
                                         uint32_t current,
                                         uint32_t end,
                                         m4a_box_t *box)
{
    uint8_t header[16];
    uint64_t box_size;
    uint32_t header_size = 8u;
    avp_status_t st;

    if (demuxer == NULL || demuxer->common.avp_io == NULL ||
        box == NULL || current > end || end > demuxer->common.file_size) {
        return AVP_EINVAL;
    }
    if (end - current < 8u) {
        return AVP_EBADHEADER;
    }

    st = avp_io_read_at(demuxer->common.avp_io, current, header, 8u);
    if (st != AVP_OK) {
        return st;
    }

    box_size = AVP_GET_BE32(header);
    memset(box, 0, sizeof(*box));
    box->start = current;
    box->type = M4A_BOX_TYPE(header[4], header[5], header[6], header[7]);
    if (box_size == 1u) {
        if (end - current < 16u) {
            return AVP_EBADHEADER;
        }
        st = avp_io_read_at(demuxer->common.avp_io, current + 8u, header + 8u, 8u);
        if (st != AVP_OK) {
            return st;
        }
        box_size = AVP_GET_BE64(header + 8u);
        header_size = 16u;
    } else if (box_size == 0u) {
        box_size = end - current;
    }

    if (box_size < header_size || box_size > end - current || box_size > UINT32_MAX) {
        return AVP_EBADHEADER;
    }

    box->size = (uint32_t)box_size;
    box->header_size = header_size;
    return AVP_OK;
}

static avp_status_t m4a_read_descriptor_length(const uint8_t *buffer,
                                               uint32_t size,
                                               uint32_t *pos,
                                               uint32_t *length)
{
    uint32_t value = 0u;
    uint32_t i;

    if (buffer == NULL || pos == NULL || length == NULL) {
        return AVP_EINVAL;
    }

    for (i = 0u; i < 4u; i++) {
        uint8_t b;

        if (*pos >= size) {
            return AVP_EBADHEADER;
        }
        b = buffer[(*pos)++];
        value = (value << 7) | (uint32_t)(b & 0x7fu);
        if ((b & 0x80u) == 0u) {
            *length = value;
            return AVP_OK;
        }
    }

    return AVP_EBADHEADER;
}

static avp_status_t m4a_find_decoder_specific(const uint8_t *buffer,
                                              uint32_t size,
                                              uint8_t *object_type,
                                              const uint8_t **asc,
                                              uint32_t *asc_size)
{
    uint32_t pos = 0u;

    if (buffer == NULL || object_type == NULL || asc == NULL || asc_size == NULL) {
        return AVP_EINVAL;
    }

    while (pos + 2u <= size) {
        const uint8_t *payload;
        uint8_t tag;
        uint32_t length;
        uint32_t payload_pos;
        avp_status_t st;

        tag = buffer[pos++];
        st = m4a_read_descriptor_length(buffer, size, &pos, &length);
        if (st != AVP_OK) {
            return st;
        }
        if (length > size - pos) {
            return AVP_EBADHEADER;
        }

        payload = buffer + pos;
        if (tag == 0x05u) {
            *asc = payload;
            *asc_size = length;
            return AVP_OK;
        }

        payload_pos = 0u;
        if (tag == 0x03u && length >= 3u) {
            uint8_t flags;

            payload_pos = 2u;
            flags = payload[payload_pos++];
            if ((flags & 0x80u) != 0u) {
                payload_pos += 2u;
            }
            if ((flags & 0x40u) != 0u && payload_pos < length) {
                payload_pos += 1u + payload[payload_pos];
            }
            if ((flags & 0x20u) != 0u) {
                payload_pos += 2u;
            }
        } else if (tag == 0x04u && length >= 13u) {
            *object_type = payload[0];
            payload_pos = 13u;
        }

        if (payload_pos < length) {
            st = m4a_find_decoder_specific(payload + payload_pos,
                                           length - payload_pos,
                                           object_type,
                                           asc,
                                           asc_size);
            if (st == AVP_OK) {
                return AVP_OK;
            }
        }

        pos += length;
    }

    return AVP_ENOENT;
}

static avp_status_t m4a_parse_esds(const uint8_t *buffer,
                                   uint32_t buffer_size,
                                   m4a_demux_t *m4a,
                                   m4a_track_t *track)
{
    const uint8_t *asc;
    uint32_t asc_size;
    uint8_t object_type = 0u;
    avp_status_t st;

    if (buffer == NULL || m4a == NULL || track == NULL ||
        buffer_size < 4u) {
        return AVP_EBADHEADER;
    }

    st = m4a_find_decoder_specific(buffer + 4u,
                                   buffer_size - 4u,
                                   &object_type,
                                   &asc,
                                   &asc_size);
    if (st != AVP_OK) {
        return st;
    }
    if (object_type != M4A_OBJECT_TYPE_MPEG4_AUDIO &&
        object_type != M4A_OBJECT_TYPE_AAC_MAIN &&
        object_type != M4A_OBJECT_TYPE_AAC_LC &&
        object_type != M4A_OBJECT_TYPE_AAC_SSR) {
        return AVP_EUNSUPPORTED;
    }
    st = m4a_parse_audio_specific_config(asc, asc_size, &m4a->aac_config);
    if (st != AVP_OK) {
        return st;
    }

    track->has_esds = 1u;
    m4a->codec_type = M4A_AUDIO_CODEC_AAC;
    return AVP_OK;
}

static avp_status_t m4a_parse_mp4a(const uint8_t *buffer,
                                   uint32_t buffer_size,
                                   uint32_t entry_start,
                                   uint32_t entry_size,
                                   m4a_demux_t *m4a,
                                   m4a_track_t *track)
{
    uint32_t payload = entry_start + 8u;
    uint32_t child_start;
    uint32_t child_size;
    uint16_t version;
    avp_status_t st;
    m4a_box_t esds;
    uint64_t current;
    uint64_t end;
    int found_esds = 0;

    if (buffer == NULL || m4a == NULL || track == NULL ||
        entry_size < 36u ||
        entry_start > buffer_size ||
        entry_size > buffer_size - entry_start) {
        return AVP_EBADHEADER;
    }

    version = AVP_GET_BE16(buffer + payload + 8u);
    child_start = payload + 28u;
    if (version == 1u) {
        child_start += 16u;
    } else if (version == 2u) {
        child_start += 36u;
    }
    if (child_start > entry_start + entry_size) {
        return AVP_EBADHEADER;
    }
    child_size = entry_start + entry_size - child_start;

    current = child_start;
    end = (uint64_t)child_start + child_size;
    while (current + 8u <= end) {
        M4A_READ_MEMORY_BOX(buffer,
                            buffer_size,
                            (uint32_t)current,
                            (uint32_t)end,
                            &esds,
                            st);
        if (st != AVP_OK) {
            return st;
        }

        switch (esds.type) {
            case M4A_BOX_TYPE('e', 's', 'd', 's'):
                found_esds = 1;
                break;
            default:
                break;
        }
        if (found_esds != 0) {
            break;
        }
        current = (uint64_t)esds.start + esds.size;
    }
    if (current != end && found_esds == 0) {
        return AVP_EBADHEADER;
    }
    if (found_esds == 0) {
        return AVP_ENOENT;
    }

    st = m4a_parse_esds(buffer + esds.start + esds.header_size,
                        esds.size - esds.header_size,
                        m4a,
                        track);
    if (st != AVP_OK) {
        return st;
    }

    track->has_mp4a = 1u;
    return AVP_OK;
}

static avp_status_t m4a_parse_alac(const uint8_t *buffer,
                                   uint32_t buffer_size,
                                   uint32_t entry_start,
                                   uint32_t entry_size,
                                   m4a_demux_t *m4a,
                                   m4a_track_t *track)
{
    m4a_box_t atom;
    uint32_t child_start;
    uint32_t child_size;
    uint32_t cookie_size;
    avp_status_t st;
    uint64_t current;
    uint64_t end;
    int found_alac = 0;

    if (buffer == NULL || m4a == NULL || track == NULL ||
        entry_size < 36u ||
        entry_start > buffer_size ||
        entry_size > buffer_size - entry_start) {
        return AVP_EBADHEADER;
    }

    child_start = entry_start + 36u;
    child_size = entry_size - 36u;
    current = child_start;
    end = (uint64_t)child_start + child_size;
    while (current + 8u <= end) {
        M4A_READ_MEMORY_BOX(buffer,
                            buffer_size,
                            (uint32_t)current,
                            (uint32_t)end,
                            &atom,
                            st);
        if (st != AVP_OK) {
            return st;
        }

        switch (atom.type) {
            case M4A_BOX_TYPE('a', 'l', 'a', 'c'):
                found_alac = 1;
                break;
            default:
                break;
        }
        if (found_alac != 0) {
            break;
        }
        current = (uint64_t)atom.start + atom.size;
    }
    if (current != end && found_alac == 0) {
        return AVP_EBADHEADER;
    }
    if (found_alac == 0) {
        return AVP_EUNSUPPORTED;
    }
    if (atom.size < atom.header_size + 4u + sizeof(ALACSpecificConfig)) {
        return AVP_EBADHEADER;
    }

    cookie_size = atom.size - atom.header_size - 4u;
    if (cookie_size > ALAC_MAGIC_COOKIE_MAX_SIZE) {
        cookie_size = ALAC_MAGIC_COOKIE_MAX_SIZE;
    }
    if (cookie_size < sizeof(ALACSpecificConfig)) {
        return AVP_EBADHEADER;
    }
    memcpy(m4a->alac_config.magic_cookie,
           buffer + atom.start + atom.header_size + 4u,
           cookie_size);
    m4a->alac_config.magic_cookie_size = cookie_size;
    m4a->codec_type = M4A_AUDIO_CODEC_ALAC;
    track->has_mp4a = 1u;
    track->has_esds = 1u;
    return AVP_OK;
}

static avp_status_t m4a_parse_stsd(const uint8_t *buffer,
                                   uint32_t buffer_size,
                                   m4a_demux_t *m4a,
                                   m4a_track_t *track)
{
    uint32_t pos;
    uint32_t end;
    uint32_t entry_count;
    uint32_t i;

    if (buffer == NULL || m4a == NULL || track == NULL || buffer_size < 8u) {
        return AVP_EBADHEADER;
    }

    entry_count = AVP_GET_BE32(buffer + 4u);
    pos = 8u;
    end = buffer_size;

    for (i = 0u; i < entry_count && pos + 8u <= end; i++) {
        uint32_t entry_size = AVP_GET_BE32(buffer + pos);
        uint32_t entry_type = AUDIO_CODEC_FOURCC(buffer[pos + 4u],
                                                 buffer[pos + 5u],
                                                 buffer[pos + 6u],
                                                 buffer[pos + 7u]);

        if (entry_size < 8u || entry_size > end - pos) {
            return AVP_EBADHEADER;
        }

        if (entry_type == M4A_BOX_TYPE('m', 'p', '4', 'a')) {
            return m4a_parse_mp4a(buffer, buffer_size, pos, entry_size, m4a, track);
        }
        if (entry_type == M4A_BOX_TYPE('a', 'l', 'a', 'c')) {
            return m4a_parse_alac(buffer, buffer_size, pos, entry_size, m4a, track);
        }

        pos += entry_size;
    }

    return AVP_ENOENT;
}

static avp_status_t m4a_parse_stsz(const uint8_t *buffer,
                                   uint32_t buffer_size,
                                   m4a_demux_t *m4a,
                                   m4a_track_t *track)
{
    uint32_t default_size;
    uint32_t sample_count;
    uint32_t i;

    if (buffer == NULL || m4a == NULL || track == NULL || buffer_size < 12u) {
        return AVP_EBADHEADER;
    }

    default_size = AVP_GET_BE32(buffer + 4u);
    sample_count = AVP_GET_BE32(buffer + 8u);
    if (sample_count == 0u ||
        sample_count > UINT32_MAX / (uint32_t)sizeof(uint32_t)) {
        return AVP_EBADHEADER;
    }
    if (default_size == 0u &&
        buffer_size < 12u + (uint32_t)sample_count * 4u) {
        return AVP_EBADHEADER;
    }

    if (m4a->sample_sizes != NULL) {
        return AVP_EBADHEADER;
    }

    m4a->sample_sizes = (uint32_t *)avp_malloc((size_t)sample_count * sizeof(uint32_t));
    if (m4a->sample_sizes == NULL) {
        return AVP_ENOMEM;
    }
    m4a->sample_count = sample_count;
    m4a->max_sample_size = 0u;
    for (i = 0u; i < sample_count; i++) {
        uint32_t sample_size = default_size != 0u ?
                                   default_size :
                                   AVP_GET_BE32(buffer + 12u + (uint32_t)i * 4u);

        if (sample_size == 0u) {
            return AVP_EBADHEADER;
        }
        m4a->sample_sizes[i] = sample_size;
        if (sample_size > m4a->max_sample_size) {
            m4a->max_sample_size = sample_size;
        }
    }

    return AVP_OK;
}

static avp_status_t m4a_parse_stsc(const uint8_t *buffer,
                                   uint32_t buffer_size,
                                   m4a_track_t *track)
{
    uint32_t entry_count;
    uint32_t i;

    if (buffer == NULL || track == NULL || buffer_size < 8u) {
        return AVP_EBADHEADER;
    }

    entry_count = AVP_GET_BE32(buffer + 4u);
    if (entry_count == 0u ||
        entry_count > UINT32_MAX / (uint32_t)sizeof(m4a_stsc_entry_t) ||
        buffer_size < 8u + (uint32_t)entry_count * 12u) {
        return AVP_EBADHEADER;
    }
    if (track->stsc != NULL) {
        return AVP_EBADHEADER;
    }

    track->stsc = (m4a_stsc_entry_t *)avp_malloc((size_t)entry_count * sizeof(*track->stsc));
    if (track->stsc == NULL) {
        return AVP_ENOMEM;
    }
    track->stsc_count = entry_count;
    for (i = 0u; i < entry_count; i++) {
        const uint8_t *entry = buffer + 8u + (uint32_t)i * 12u;

        track->stsc[i].first_chunk = AVP_GET_BE32(entry);
        track->stsc[i].samples_per_chunk = AVP_GET_BE32(entry + 4u);
        track->stsc[i].sample_description_index = AVP_GET_BE32(entry + 8u);
        if (track->stsc[i].first_chunk == 0u ||
            track->stsc[i].samples_per_chunk == 0u ||
            track->stsc[i].sample_description_index == 0u ||
            (i != 0u && track->stsc[i].first_chunk <= track->stsc[i - 1u].first_chunk)) {
            return AVP_EBADHEADER;
        }
    }

    return AVP_OK;
}

static avp_status_t m4a_parse_stco(const uint8_t *buffer,
                                   uint32_t buffer_size,
                                   uint8_t co64,
                                   m4a_track_t *track)
{
    uint32_t entry_count;
    uint32_t field_size = co64 ? 8u : 4u;
    uint32_t i;

    if (buffer == NULL || track == NULL || buffer_size < 8u) {
        return AVP_EBADHEADER;
    }

    entry_count = AVP_GET_BE32(buffer + 4u);
    if (entry_count == 0u ||
        entry_count > UINT32_MAX / (uint32_t)sizeof(uint32_t) ||
        buffer_size < 8u + (uint32_t)entry_count * field_size) {
        return AVP_EBADHEADER;
    }
    if (track->chunk_offsets != NULL) {
        return AVP_EBADHEADER;
    }

    track->chunk_offsets = (uint32_t *)avp_malloc((size_t)entry_count * sizeof(*track->chunk_offsets));
    if (track->chunk_offsets == NULL) {
        return AVP_ENOMEM;
    }
    track->chunk_count = entry_count;
    for (i = 0u; i < entry_count; i++) {
        const uint8_t *entry = buffer + 8u + (uint32_t)i * field_size;

        track->chunk_offsets[i] = co64 ? AVP_GET_BE64(entry) : (uint32_t)AVP_GET_BE32(entry);
    }

    return AVP_OK;
}

static avp_status_t m4a_build_header_from_track(const m4a_track_t *track,
                                                m4a_demux_t *demuxer)
{
    uint32_t stsc_index = 0u;
    uint32_t i;
    uint32_t first_offset;
    uint32_t last_end;
    uint32_t packet_index = 0u;

    if (track == NULL || demuxer == NULL ||
        !track->is_audio || !track->has_mp4a || !track->has_esds ||
        demuxer->sample_sizes == NULL || demuxer->sample_count == 0u ||
        track->stsc == NULL || track->stsc_count == 0u ||
        track->chunk_offsets == NULL || track->chunk_count == 0u) {
        return AVP_EBADHEADER;
    }

    if (demuxer->sample_offsets != NULL) {
        return AVP_EBADHEADER;
    }

    demuxer->sample_offsets = (uint32_t *)avp_malloc((size_t)demuxer->sample_count * sizeof(uint32_t));
    if (demuxer->sample_offsets == NULL) {
        return AVP_ENOMEM;
    }

    first_offset = UINT32_MAX;
    last_end = 0u;

    for (i = 0u; i < track->chunk_count && packet_index < demuxer->sample_count; i++) {
        uint32_t chunk_number = i + 1u;
        uint32_t j;
        uint32_t samples_per_chunk;
        uint32_t current_offset;

        while (stsc_index + 1u < track->stsc_count &&
               chunk_number >= track->stsc[stsc_index + 1u].first_chunk) {
            stsc_index++;
        }

        samples_per_chunk = track->stsc[stsc_index].samples_per_chunk;
        current_offset = track->chunk_offsets[i];
        for (j = 0u; j < samples_per_chunk && packet_index < demuxer->sample_count; j++) {
            uint32_t sample_size = demuxer->sample_sizes[packet_index];
            uint32_t sample_end = current_offset + sample_size;

            if (current_offset > UINT32_MAX || sample_end > demuxer->common.file_size) {
                return AVP_EBADHEADER;
            }

            demuxer->sample_offsets[packet_index] = (uint32_t)current_offset;
            if (current_offset < first_offset) {
                first_offset = current_offset;
            }
            if (sample_end > last_end) {
                last_end = sample_end;
            }
            current_offset = sample_end;
            packet_index++;
        }
    }

    if (packet_index != demuxer->sample_count ||
        last_end < first_offset) {
        return AVP_EBADHEADER;
    }

    for (i = 0u; i < demuxer->sample_count; i++) {
        if ((uint32_t)demuxer->sample_offsets[i] < first_offset) {
            return AVP_EBADHEADER;
        }
        demuxer->sample_offsets[i] -= (uint32_t)first_offset;
    }

    demuxer->common.stream_offset = (uint32_t)first_offset;
    demuxer->common.stream_size = (uint32_t)(last_end - first_offset);

    return AVP_OK;
}

static int m4a_is_nested_box(uint32_t type)
{
    switch (type) {
        case M4A_BOX_TYPE('m', 'o', 'o', 'v'):
        case M4A_BOX_TYPE('t', 'r', 'a', 'k'):
        case M4A_BOX_TYPE('m', 'd', 'i', 'a'):
        case M4A_BOX_TYPE('m', 'i', 'n', 'f'):
        case M4A_BOX_TYPE('s', 't', 'b', 'l'):
        case M4A_BOX_TYPE('e', 'd', 't', 's'):
        case M4A_BOX_TYPE('d', 'i', 'n', 'f'):
        case M4A_BOX_TYPE('u', 'd', 't', 'a'):
        case M4A_BOX_TYPE('m', 'v', 'e', 'x'):
        case M4A_BOX_TYPE('m', 'o', 'o', 'f'):
        case M4A_BOX_TYPE('t', 'r', 'a', 'f'):
        case M4A_BOX_TYPE('m', 'f', 'r', 'a'):
        case M4A_BOX_TYPE('w', 'a', 'v', 'e'):
            return 1;
        default:
            return 0;
    }
}

static avp_status_t m4a_parse_file_boxes(m4a_demux_t *demuxer,
                                         uint32_t start,
                                         uint32_t end,
                                         m4a_track_t *track,
                                         uint32_t depth)
{
    uint64_t current = start;

    if (demuxer == NULL || demuxer->common.avp_io == NULL ||
        start > end || end > demuxer->common.file_size) {
        return AVP_EINVAL;
    }

    while (current + 8u <= end) {
        m4a_box_t box;
        uint32_t box_payload;
        uint32_t box_data_size;
        avp_status_t st;
        int parse_children;

        st = m4a_parse_box_header(demuxer, (uint32_t)current, end, &box);
        if (st != AVP_OK) {
            return st;
        }

        box_payload = box.start + box.header_size;
        box_data_size = box.size - box.header_size;
        parse_children = m4a_is_nested_box(box.type) != 0 && box.size > box.header_size;

        // char box_str[5] = { 0 };
        // avp_fourcc_to_string(box.type, box_str);
        // printf("%*s[%s], offset %u, size %u\n",
        //        depth * 2,
        //        "",
        //        box_str,
        //        box.start,
        //        box.size);

        switch (box.type) {
            case M4A_BOX_TYPE('h', 'd', 'l', 'r'):
                if (track != NULL) {
                    uint8_t handler_buffer[12];

                    if (box_data_size < sizeof(handler_buffer)) {
                        return AVP_EBADHEADER;
                    }
                    st = avp_io_read_at(demuxer->common.avp_io,
                                        box_payload,
                                        handler_buffer,
                                        sizeof(handler_buffer));
                    if (st != AVP_OK) {
                        return st;
                    }
                    track->is_audio = M4A_BOX_TYPE(handler_buffer[8],
                                                   handler_buffer[9],
                                                   handler_buffer[10],
                                                   handler_buffer[11]) ==
                                              M4A_BOX_TYPE('s', 'o', 'u', 'n') ?
                                          1u :
                                          0u;
                }
                break;
            case M4A_BOX_TYPE('s', 't', 's', 'd'):
                if (track != NULL && track->is_audio != 0u) {
                    uint8_t *box_buffer = (uint8_t *)avp_malloc((size_t)box.size);

                    if (box_buffer == NULL) {
                        return AVP_ENOMEM;
                    }
                    st = avp_io_read_at(demuxer->common.avp_io,
                                        box.start,
                                        box_buffer,
                                        box.size);
                    if (st == AVP_OK) {
                        st = m4a_parse_stsd(box_buffer + box.header_size,
                                            box.size - box.header_size,
                                            demuxer,
                                            track);
                    }
                    avp_free(box_buffer);
                    if (st != AVP_OK && st != AVP_ENOENT) {
                        return st;
                    }
                }
                break;
            case M4A_BOX_TYPE('s', 't', 's', 'z'):
                if (track != NULL && track->is_audio != 0u) {
                    uint8_t *box_buffer = (uint8_t *)avp_malloc((size_t)box.size);

                    if (box_buffer == NULL) {
                        return AVP_ENOMEM;
                    }
                    st = avp_io_read_at(demuxer->common.avp_io,
                                        box.start,
                                        box_buffer,
                                        box.size);
                    if (st == AVP_OK) {
                        st = m4a_parse_stsz(box_buffer + box.header_size,
                                            box.size - box.header_size,
                                            demuxer,
                                            track);
                    }
                    avp_free(box_buffer);
                    if (st != AVP_OK) {
                        return st;
                    }
                }
                break;
            case M4A_BOX_TYPE('s', 't', 's', 'c'):
                if (track != NULL && track->is_audio != 0u) {
                    uint8_t *box_buffer = (uint8_t *)avp_malloc((size_t)box.size);

                    if (box_buffer == NULL) {
                        return AVP_ENOMEM;
                    }
                    st = avp_io_read_at(demuxer->common.avp_io,
                                        box.start,
                                        box_buffer,
                                        box.size);
                    if (st == AVP_OK) {
                        st = m4a_parse_stsc(box_buffer + box.header_size,
                                            box.size - box.header_size,
                                            track);
                    }
                    avp_free(box_buffer);
                    if (st != AVP_OK) {
                        return st;
                    }
                }
                break;
            case M4A_BOX_TYPE('s', 't', 'c', 'o'):
                if (track != NULL && track->is_audio != 0u) {
                    uint8_t *box_buffer = (uint8_t *)avp_malloc((size_t)box.size);

                    if (box_buffer == NULL) {
                        return AVP_ENOMEM;
                    }
                    st = avp_io_read_at(demuxer->common.avp_io,
                                        box.start,
                                        box_buffer,
                                        box.size);
                    if (st == AVP_OK) {
                        st = m4a_parse_stco(box_buffer + box.header_size,
                                            box.size - box.header_size,
                                            0u,
                                            track);
                    }
                    avp_free(box_buffer);
                    if (st != AVP_OK) {
                        return st;
                    }
                }
                break;
            case M4A_BOX_TYPE('c', 'o', '6', '4'):
                if (track != NULL && track->is_audio != 0u) {
                    uint8_t *box_buffer = (uint8_t *)avp_malloc((size_t)box.size);

                    if (box_buffer == NULL) {
                        return AVP_ENOMEM;
                    }
                    st = avp_io_read_at(demuxer->common.avp_io,
                                        box.start,
                                        box_buffer,
                                        box.size);
                    if (st == AVP_OK) {
                        st = m4a_parse_stco(box_buffer + box.header_size,
                                            box.size - box.header_size,
                                            1u,
                                            track);
                    }
                    avp_free(box_buffer);
                    if (st != AVP_OK) {
                        return st;
                    }
                }
                break;
            default:
                break;
        }

        if (parse_children) {
            st = m4a_parse_file_boxes(demuxer,
                                      box_payload,
                                      box.start + box.size,
                                      track,
                                      depth + 1);
            if (st != AVP_OK) {
                return st;
            }
            if (track != NULL &&
                box.type == M4A_BOX_TYPE('t', 'r', 'a', 'k') &&
                track->is_audio != 0u) {
                return AVP_OK;
            }
        }

        current = (uint64_t)box.start + box.size;
    }

    return current == end ? AVP_OK : AVP_EBADHEADER;
}

avp_status_t m4a_demux_open(m4a_demux_t *demuxer, avp_io_t *avp_io)
{
    int64_t size;
    m4a_track_t track;
    avp_status_t st;

    if (demuxer == NULL || avp_io == NULL) {
        return AVP_EINVAL;
    }

    size = avp_io_get_size(avp_io);
    if (size < 0) {
        return AVP_IO;
    }

    memset(demuxer, 0, sizeof(*demuxer));
    memset(&track, 0, sizeof(track));
    demuxer->common.avp_io = avp_io;
    demuxer->common.file_size = (uint32_t)size;

    st = m4a_parse_file_boxes(demuxer,
                              0u,
                              demuxer->common.file_size,
                              &track,
                              0);
    if (st != AVP_OK) {
        goto fail;
    }

    st = m4a_build_header_from_track(&track, demuxer);
    m4a_track_deinit(&track);
    if (st != AVP_OK) {
        goto fail;
    }

    demuxer->common.current_offset = demuxer->common.stream_offset;
    st = avp_io_seek(avp_io, demuxer->common.stream_offset);
    if (st != AVP_OK) {
        goto fail;
    }
    return AVP_OK;
fail:
    m4a_track_deinit(&track);
    if (demuxer->sample_sizes != NULL) {
        avp_free(demuxer->sample_sizes);
    }
    if (demuxer->sample_offsets != NULL) {
        avp_free(demuxer->sample_offsets);
    }
    memset(demuxer, 0, sizeof(*demuxer));
    return st;
}

void m4a_demux_close(m4a_demux_t *demuxer)
{
    if (demuxer == NULL) {
        return;
    }

    if (demuxer->sample_sizes != NULL) {
        avp_free(demuxer->sample_sizes);
    }
    if (demuxer->sample_offsets != NULL) {
        avp_free(demuxer->sample_offsets);
    }
    memset(demuxer, 0, sizeof(*demuxer));
}

avp_status_t m4a_demux_get_audio_stream_config(const m4a_demux_t *demuxer,
                                               audio_codec_dec_config_t *config)
{
    if (demuxer == NULL || config == NULL) {
        return AVP_EINVAL;
    }

    if (demuxer->codec_type != M4A_AUDIO_CODEC_AAC &&
        demuxer->codec_type != M4A_AUDIO_CODEC_ALAC) {
        return AVP_EUNSUPPORTED;
    }

    memset(config, 0, sizeof(*config));
    if (demuxer->codec_type == M4A_AUDIO_CODEC_AAC) {
        config->codec_type = AUDIO_CODEC_ID_AAC;
        config->aac_config = demuxer->aac_config;
    } else {
        config->codec_type = AUDIO_CODEC_ID_ALAC;
        config->alac_config = demuxer->alac_config;
    }
    return AVP_OK;
}

avp_status_t m4a_demux_read_packet(m4a_demux_t *demuxer,
                                   avp_packet_t *packet)
{
    uint32_t packet_size;
    uint32_t packet_offset;
    uint32_t stream_end;
    avp_status_t st;

    if (demuxer == NULL || packet == NULL) {
        return AVP_EINVAL;
    }

    stream_end = demuxer->common.stream_offset + demuxer->common.stream_size;
    if (demuxer->common.current_offset >= stream_end ||
        demuxer->common.packet_index >= demuxer->sample_count) {
        return AVP_ENOENT;
    }

    if (demuxer->common.packet_index >= demuxer->sample_count) {
        return AVP_ENOENT;
    }

    packet_size = demuxer->sample_sizes[demuxer->common.packet_index];
    packet_offset = demuxer->common.stream_offset +
                    demuxer->sample_offsets[demuxer->common.packet_index];

    if (packet_offset != demuxer->common.current_offset) {
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
