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

static avp_status_t m4a_next_box(const uint8_t *buffer,
                                 uint32_t buffer_size,
                                 uint32_t parent_start,
                                 uint32_t parent_size,
                                 uint32_t *pos,
                                 m4a_box_t *box)
{
    uint32_t parent_end;
    uint32_t size;
    uint32_t header_size = 8u;

    if (buffer == NULL || pos == NULL || box == NULL ||
        parent_start > buffer_size ||
        parent_size > buffer_size - parent_start) {
        return AVP_EINVAL;
    }

    parent_end = parent_start + parent_size;
    if (*pos >= parent_end) {
        return AVP_ENOENT;
    }
    if (parent_end - *pos < 8u) {
        return AVP_EBADHEADER;
    }

    size = AVP_GET_BE32(buffer + *pos);
    memset(box, 0, sizeof(*box));
    box->start = *pos;
    box->type = AUDIO_CODEC_FOURCC(buffer[*pos + 4u],
                                   buffer[*pos + 5u],
                                   buffer[*pos + 6u],
                                   buffer[*pos + 7u]);

    if (size == 1u) {
        if (parent_end - *pos < 16u) {
            return AVP_EBADHEADER;
        }
        size = AVP_GET_BE64(buffer + *pos + 8u);
        header_size = 16u;
    } else if (size == 0u) {
        size = parent_end - *pos;
    }

    if (size < header_size || size > parent_end - *pos) {
        return AVP_EBADHEADER;
    }

    box->size = size;
    box->header_size = header_size;
    *pos += size;
    return AVP_OK;
}

static avp_status_t m4a_find_child(const uint8_t *buffer,
                                   uint32_t buffer_size,
                                   uint32_t parent_start,
                                   uint32_t parent_size,
                                   uint32_t type,
                                   m4a_box_t *out)
{
    uint32_t pos = parent_start;
    avp_status_t st;

    for (;;) {
        m4a_box_t box;

        st = m4a_next_box(buffer, buffer_size, parent_start, parent_size, &pos, &box);
        if (st == AVP_ENOENT) {
            return AVP_ENOENT;
        }
        if (st != AVP_OK) {
            return st;
        }
        if (box.type == type) {
            *out = box;
            return AVP_OK;
        }
    }
}

static avp_status_t m4a_find_moov_in_probe(const uint8_t *buffer,
                                           uint32_t size,
                                           m4a_box_t *moov)
{
    uint32_t pos;
    m4a_box_t trak;

    if (buffer == NULL || moov == NULL || size < 8u) {
        return AVP_EINVAL;
    }

    /*
     * The probe buffer may be either the file head or the file tail. When it is
     * the tail, it can start in the middle of mdat, so scan for a complete moov
     * box instead of requiring top-level parsing from offset zero.
     */
    for (pos = 0u; pos + 8u <= size; pos++) {
        uint32_t box_size;
        uint32_t header_size = 8u;

        if (AUDIO_CODEC_FOURCC(buffer[pos + 4u],
                               buffer[pos + 5u],
                               buffer[pos + 6u],
                               buffer[pos + 7u]) != M4A_BOX_TYPE('m', 'o', 'o', 'v')) {
            continue;
        }

        box_size = AVP_GET_BE32(buffer + pos);
        if (box_size == 1u) {
            if (pos + 16u > size) {
                continue;
            }
            box_size = AVP_GET_BE64(buffer + pos + 8u);
            header_size = 16u;
        } else if (box_size == 0u) {
            box_size = (uint32_t)size - pos;
        }

        if (box_size < header_size) {
            continue;
        }
        if (box_size > (uint32_t)size - pos) {
            continue;
        }

        if (m4a_find_child(buffer,
                           size,
                           pos + header_size,
                           box_size - header_size,
                           M4A_BOX_TYPE('t', 'r', 'a', 'k'),
                           &trak) != AVP_OK) {
            continue;
        }

        memset(moov, 0, sizeof(*moov));
        moov->start = pos;
        moov->size = box_size;
        moov->header_size = header_size;
        moov->type = M4A_BOX_TYPE('m', 'o', 'o', 'v');
        return AVP_OK;
    }

    return AVP_ELACKFRAME;
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
                                   uint32_t payload_start,
                                   uint32_t payload_size,
                                   m4a_demux_t *m4a,
                                   m4a_track_t *track)
{
    const uint8_t *asc;
    uint32_t asc_size;
    uint8_t object_type = 0u;
    avp_status_t st;

    if (buffer == NULL || m4a == NULL || track == NULL ||
        payload_size < 4u || payload_size > UINT32_MAX) {
        return AVP_EBADHEADER;
    }

    st = m4a_find_decoder_specific(buffer + payload_start + 4u,
                                   (uint32_t)payload_size - 4u,
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
    uint32_t payload_start = entry_start + 8u;
    uint32_t child_start;
    uint32_t child_size;
    uint16_t version;
    avp_status_t st;
    m4a_box_t esds;

    if (buffer == NULL || m4a == NULL || track == NULL ||
        entry_size < 36u ||
        entry_start > buffer_size ||
        entry_size > buffer_size - entry_start) {
        return AVP_EBADHEADER;
    }

    version = AVP_GET_BE16(buffer + payload_start + 8u);
    child_start = payload_start + 28u;
    if (version == 1u) {
        child_start += 16u;
    } else if (version == 2u) {
        child_start += 36u;
    }
    if (child_start > entry_start + entry_size) {
        return AVP_EBADHEADER;
    }
    child_size = entry_start + entry_size - child_start;

    st = m4a_find_child(buffer,
                        buffer_size,
                        child_start,
                        child_size,
                        M4A_BOX_TYPE('e', 's', 'd', 's'),
                        &esds);
    if (st != AVP_OK) {
        return st;
    }

    st = m4a_parse_esds(buffer,
                        esds.start + esds.header_size,
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

    if (buffer == NULL || m4a == NULL || track == NULL ||
        entry_size < 36u ||
        entry_start > buffer_size ||
        entry_size > buffer_size - entry_start) {
        return AVP_EBADHEADER;
    }

    child_start = entry_start + 36u;
    child_size = entry_size - 36u;
    st = m4a_find_child(buffer,
                        buffer_size,
                        child_start,
                        child_size,
                        M4A_BOX_TYPE('a', 'l', 'a', 'c'),
                        &atom);
    if (st != AVP_OK || atom.size < atom.header_size + 4u + sizeof(ALACSpecificConfig)) {
        return st == AVP_ENOENT ? AVP_EUNSUPPORTED : st;
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
                                   uint32_t payload_start,
                                   uint32_t payload_size,
                                   m4a_demux_t *m4a,
                                   m4a_track_t *track)
{
    uint32_t pos;
    uint32_t end;
    uint32_t entry_count;
    uint32_t i;

    if (buffer == NULL || m4a == NULL || track == NULL || payload_size < 8u ||
        payload_start > buffer_size ||
        payload_size > buffer_size - payload_start) {
        return AVP_EBADHEADER;
    }

    entry_count = AVP_GET_BE32(buffer + payload_start + 4u);
    pos = payload_start + 8u;
    end = payload_start + payload_size;

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
                                   uint32_t payload_start,
                                   uint32_t payload_size,
                                   m4a_demux_t *m4a,
                                   m4a_track_t *track)
{
    uint32_t default_size;
    uint32_t sample_count;
    uint32_t i;

    if (buffer == NULL || m4a == NULL || track == NULL || payload_size < 12u ||
        payload_size > UINT32_MAX) {
        return AVP_EBADHEADER;
    }

    default_size = AVP_GET_BE32(buffer + payload_start + 4u);
    sample_count = AVP_GET_BE32(buffer + payload_start + 8u);
    if (sample_count == 0u ||
        sample_count > UINT32_MAX / (uint32_t)sizeof(uint32_t)) {
        return AVP_EBADHEADER;
    }
    if (default_size == 0u &&
        payload_size < 12u + (uint32_t)sample_count * 4u) {
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
                                   AVP_GET_BE32(buffer + payload_start + 12u + (uint32_t)i * 4u);

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
                                   uint32_t payload_start,
                                   uint32_t payload_size,
                                   m4a_track_t *track)
{
    uint32_t entry_count;
    uint32_t i;

    if (buffer == NULL || track == NULL || payload_size < 8u ||
        payload_size > UINT32_MAX) {
        return AVP_EBADHEADER;
    }

    entry_count = AVP_GET_BE32(buffer + payload_start + 4u);
    if (entry_count == 0u ||
        entry_count > UINT32_MAX / (uint32_t)sizeof(m4a_stsc_entry_t) ||
        payload_size < 8u + (uint32_t)entry_count * 12u) {
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
        const uint8_t *entry = buffer + payload_start + 8u + (uint32_t)i * 12u;

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
                                   uint32_t payload_start,
                                   uint32_t payload_size,
                                   uint8_t co64,
                                   m4a_track_t *track)
{
    uint32_t entry_count;
    uint32_t field_size = co64 ? 8u : 4u;
    uint32_t i;

    if (buffer == NULL || track == NULL || payload_size < 8u ||
        payload_size > UINT32_MAX) {
        return AVP_EBADHEADER;
    }

    entry_count = AVP_GET_BE32(buffer + payload_start + 4u);
    if (entry_count == 0u ||
        entry_count > UINT32_MAX / (uint32_t)sizeof(uint32_t) ||
        payload_size < 8u + (uint32_t)entry_count * field_size) {
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
        const uint8_t *entry = buffer + payload_start + 8u + (uint32_t)i * field_size;

        track->chunk_offsets[i] = co64 ? AVP_GET_BE64(entry) : (uint32_t)AVP_GET_BE32(entry);
    }

    return AVP_OK;
}

static avp_status_t m4a_parse_stbl(const uint8_t *buffer,
                                   uint32_t buffer_size,
                                   uint32_t payload_start,
                                   uint32_t payload_size,
                                   m4a_demux_t *m4a,
                                   m4a_track_t *track)
{
    uint32_t pos = payload_start;
    avp_status_t st;

    for (;;) {
        m4a_box_t box;
        uint32_t box_payload = 0u;
        uint32_t box_payload_size = 0u;

        st = m4a_next_box(buffer, buffer_size, payload_start, payload_size, &pos, &box);
        if (st == AVP_ENOENT) {
            break;
        }
        if (st != AVP_OK) {
            return st;
        }

        box_payload = box.start + box.header_size;
        box_payload_size = box.size - box.header_size;

        if (box.type == M4A_BOX_TYPE('s', 't', 's', 'd')) {
            st = m4a_parse_stsd(buffer, buffer_size, box_payload, box_payload_size, m4a, track);
        } else if (box.type == M4A_BOX_TYPE('s', 't', 's', 'z')) {
            st = m4a_parse_stsz(buffer, box_payload, box_payload_size, m4a, track);
        } else if (box.type == M4A_BOX_TYPE('s', 't', 's', 'c')) {
            st = m4a_parse_stsc(buffer, box_payload, box_payload_size, track);
        } else if (box.type == M4A_BOX_TYPE('s', 't', 'c', 'o')) {
            st = m4a_parse_stco(buffer, box_payload, box_payload_size, 0u, track);
        } else if (box.type == M4A_BOX_TYPE('c', 'o', '6', '4')) {
            st = m4a_parse_stco(buffer, box_payload, box_payload_size, 1u, track);
        } else {
            st = AVP_OK;
        }

        if (st != AVP_OK && st != AVP_ENOENT) {
            return st;
        }
    }

    return AVP_OK;
}

static avp_status_t m4a_parse_trak(const uint8_t *buffer,
                                   uint32_t buffer_size,
                                   uint32_t payload_start,
                                   uint32_t payload_size,
                                   m4a_demux_t *m4a,
                                   m4a_track_t *track)
{
    m4a_box_t mdia;
    m4a_box_t hdlr;
    m4a_box_t minf;
    m4a_box_t stbl;
    uint32_t mdia_payload;
    uint32_t mdia_payload_size;
    uint32_t hdlr_payload;
    uint32_t minf_payload;
    uint32_t stbl_payload;
    avp_status_t st;

    st = m4a_find_child(buffer,
                        buffer_size,
                        payload_start,
                        payload_size,
                        M4A_BOX_TYPE('m', 'd', 'i', 'a'),
                        &mdia);
    if (st != AVP_OK) {
        return st;
    }
    mdia_payload = mdia.start + mdia.header_size;
    mdia_payload_size = mdia.size - mdia.header_size;

    st = m4a_find_child(buffer,
                        buffer_size,
                        mdia_payload,
                        mdia_payload_size,
                        M4A_BOX_TYPE('h', 'd', 'l', 'r'),
                        &hdlr);
    if (st != AVP_OK) {
        return st;
    }
    hdlr_payload = hdlr.start + hdlr.header_size;
    if (hdlr.size - hdlr.header_size < 12u) {
        return AVP_EBADHEADER;
    }
    track->is_audio = AUDIO_CODEC_FOURCC(buffer[hdlr_payload + 8u],
                                         buffer[hdlr_payload + 9u],
                                         buffer[hdlr_payload + 10u],
                                         buffer[hdlr_payload + 11u]) ==
                              M4A_BOX_TYPE('s', 'o', 'u', 'n') ?
                          1u :
                          0u;
    if (!track->is_audio) {
        return AVP_EUNSUPPORTED;
    }

    st = m4a_find_child(buffer,
                        buffer_size,
                        mdia_payload,
                        mdia_payload_size,
                        M4A_BOX_TYPE('m', 'i', 'n', 'f'),
                        &minf);
    if (st != AVP_OK) {
        return st;
    }
    minf_payload = minf.start + minf.header_size;

    st = m4a_find_child(buffer,
                        buffer_size,
                        minf_payload,
                        minf.size - minf.header_size,
                        M4A_BOX_TYPE('s', 't', 'b', 'l'),
                        &stbl);
    if (st != AVP_OK) {
        return st;
    }
    stbl_payload = stbl.start + stbl.header_size;

    return m4a_parse_stbl(buffer,
                          buffer_size,
                          stbl_payload,
                          stbl.size - stbl.header_size,
                          m4a,
                          track);
}

static avp_status_t m4a_build_header_from_track(const m4a_track_t *track,
                                                m4a_demux_t *demuxer)
{
    uint32_t stsc_index = 0u;
    uint32_t i;
    uint32_t first_offset;
    uint32_t last_end;
    uint32_t packet_index = 0;

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

static avp_status_t m4a_parse_moov_header(const uint8_t *buffer,
                                          uint32_t size,
                                          m4a_demux_t *demuxer)
{
    m4a_box_t moov;
    uint32_t moov_payload;
    uint32_t moov_payload_size;
    uint32_t pos;
    avp_status_t st;

    if (buffer == NULL || size < 8u || demuxer == NULL ||
        demuxer->common.file_size == 0u || size > demuxer->common.file_size) {
        return AVP_EINVAL;
    }

    st = m4a_find_moov_in_probe(buffer, size, &moov);
    if (st != AVP_OK) {
        return st;
    }

    moov_payload = moov.start + moov.header_size;
    moov_payload_size = moov.size - moov.header_size;
    pos = moov_payload;

    for (;;) {
        m4a_box_t trak;
        m4a_track_t track;

        st = m4a_next_box(buffer, size, moov_payload, moov_payload_size, &pos, &trak);
        if (st == AVP_ENOENT) {
            break;
        }
        if (st != AVP_OK) {
            return st;
        }
        if (trak.type != M4A_BOX_TYPE('t', 'r', 'a', 'k')) {
            continue;
        }

        memset(&track, 0, sizeof(track));
        st = m4a_parse_trak(buffer,
                            size,
                            trak.start + trak.header_size,
                            trak.size - trak.header_size,
                            demuxer,
                            &track);
        if (st == AVP_EUNSUPPORTED) {
            m4a_track_deinit(&track);
            continue;
        }
        if (st != AVP_OK) {
            m4a_track_deinit(&track);
            return st;
        }

        st = m4a_build_header_from_track(&track, demuxer);
        if (st != AVP_OK) {
            m4a_track_deinit(&track);
            return st;
        }

        m4a_track_deinit(&track);

        return AVP_OK;
    }

    return AVP_EUNSUPPORTED;
}

static avp_status_t m4a_read_moov(m4a_demux_t *demuxer,
                                  uint8_t **moov_buffer,
                                  uint32_t *moov_size)
{
    uint32_t offset = 0u;

    if (demuxer == NULL || demuxer->common.avp_io == NULL ||
        moov_buffer == NULL || moov_size == NULL) {
        return AVP_EINVAL;
    }

    *moov_buffer = NULL;
    *moov_size = 0u;

    while (offset + 8u <= demuxer->common.file_size) {
        uint8_t header[16];
        uint64_t box_size;
        uint32_t header_size = 8u;
        uint32_t type;
        avp_status_t st;

        st = avp_io_read_at(demuxer->common.avp_io, offset, header, 8u);
        if (st != AVP_OK) {
            return st;
        }

        box_size = AVP_GET_BE32(header);
        type = M4A_BOX_TYPE(header[4], header[5], header[6], header[7]);
        if (box_size == 1u) {
            if (offset + 16u > demuxer->common.file_size) {
                return AVP_EBADHEADER;
            }
            st = avp_io_read_at(demuxer->common.avp_io, offset + 8u, header + 8u, 8u);
            if (st != AVP_OK) {
                return st;
            }
            box_size = AVP_GET_BE64(header + 8u);
            header_size = 16u;
        } else if (box_size == 0u) {
            box_size = demuxer->common.file_size - offset;
        }

        if (box_size < header_size ||
            box_size > demuxer->common.file_size - offset ||
            box_size > UINT32_MAX) {
            return AVP_EBADHEADER;
        }

        if (type == M4A_BOX_TYPE('m', 'o', 'o', 'v')) {
            uint8_t *buffer = (uint8_t *)avp_malloc((uint32_t)box_size);
            if (buffer == NULL) {
                return AVP_ENOMEM;
            }

            st = avp_io_read_at(demuxer->common.avp_io, offset, buffer, (uint32_t)box_size);
            if (st != AVP_OK) {
                return st;
            }

            *moov_buffer = buffer;
            *moov_size = (uint32_t)box_size;
            return AVP_OK;
        }

        offset += (uint32_t)box_size;
    }

    return AVP_ENOENT;
}

avp_status_t m4a_demux_open(m4a_demux_t *demuxer, avp_io_t *avp_io)
{
    uint8_t *moov_buffer = NULL;
    uint32_t moov_size = 0u;
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

    st = m4a_read_moov(demuxer, &moov_buffer, &moov_size);
    if (st == AVP_OK) {
        st = m4a_parse_moov_header(moov_buffer,
                                   moov_size,
                                   demuxer);
    }
    if (moov_buffer != NULL) {
        avp_free(moov_buffer);
        moov_buffer = NULL;
    }
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
