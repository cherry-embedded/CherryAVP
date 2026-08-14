/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mp4_container.h"
#include "m4a_container.h"

#include "ALACAudioTypes.h"
#include "avp_bitreader.h"

#define MP4_OBJECT_TYPE_MPEG4_VIDEO     0x20u
#define MP4_OBJECT_TYPE_MPEG2_VIDEO_MIN 0x60u
#define MP4_OBJECT_TYPE_MPEG2_VIDEO_MAX 0x65u
#define MP4_OBJECT_TYPE_AAC             0x40u
#define MP4_OBJECT_TYPE_JPEG            0x6cu

typedef struct {
    uint32_t start;
    uint32_t size;
    uint32_t header_size;
    mp4_fourcc_t type;
} mp4_box_t;

typedef struct {
    uint32_t first_chunk;
    uint32_t samples_per_chunk;
    uint32_t sample_description_index;
} mp4_stsc_entry_t;

typedef struct {
    avp_packet_type_t type;
    uint32_t stream_index;
    uint32_t timescale;
    uint64_t duration;
    mp4_fourcc_t sample_entry;
    mp4_video_codec_t video_codec;
    mp4_audio_codec_t audio_codec;
    uint32_t width;
    uint32_t height;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    uint32_t sample_count;
    uint32_t max_sample_size;
    uint32_t *sample_sizes;
    uint32_t *sample_offsets;
    uint32_t stsc_count;
    mp4_stsc_entry_t *stsc;
    uint32_t chunk_count;
    uint32_t *chunk_offsets;
    aac_dec_config_t aac_config;
    alac_dec_config_t alac_config;
} mp4_track_t;

static void mp4_track_deinit(mp4_track_t *track)
{
    if (track == NULL) {
        return;
    }

    if (track->sample_sizes != NULL) {
        avp_free(track->sample_sizes);
    }
    if (track->sample_offsets != NULL) {
        avp_free(track->sample_offsets);
    }
    if (track->stsc != NULL) {
        avp_free(track->stsc);
    }
    if (track->chunk_offsets != NULL) {
        avp_free(track->chunk_offsets);
    }
    memset(track, 0, sizeof(*track));
}

static const uint32_t mp4_aac_sample_rate_table[16] = {
    96000u, 88200u, 64000u, 48000u,
    44100u, 32000u, 24000u, 22050u,
    16000u, 12000u, 11025u, 8000u,
    7350u, 0u, 0u, 0u
};

static uint8_t mp4_aac_channel_config_to_channels(uint32_t channel_config)
{
    static const uint8_t channel_count_table[8] = {
        0u, 1u, 2u, 3u, 4u, 5u, 6u, 8u
    };

    if (channel_config >= (sizeof(channel_count_table) / sizeof(channel_count_table[0]))) {
        return 0u;
    }
    return channel_count_table[channel_config];
}

static int mp4_aac_read_audio_object_type(avp_bitreader_t *br, uint32_t *audio_object_type)
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

static int mp4_aac_read_sample_rate(avp_bitreader_t *br, uint32_t *sample_rate)
{
    uint32_t index;

    if (!avp_bitreader_read(br, 4u, &index)) {
        return 0;
    }
    if (index == 0x0fu) {
        return avp_bitreader_read(br, 24u, sample_rate) && *sample_rate != 0u;
    }

    *sample_rate = mp4_aac_sample_rate_table[index];
    return *sample_rate != 0u;
}

static int mp4_aac_parse_program_config(avp_bitreader_t *br, uint8_t *channels)
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

static int mp4_aac_parse_ga_specific_config(avp_bitreader_t *br,
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
    if (frame_length_flag != 0u ||
        (depends_on_core_coder != 0u && !avp_bitreader_skip(br, 14u)) ||
        !avp_bitreader_read(br, 1u, &extension_flag)) {
        return 0;
    }

    if (channel_config == 0u) {
        if (!mp4_aac_parse_program_config(br, channels)) {
            return 0;
        }
    } else {
        *channels = mp4_aac_channel_config_to_channels(channel_config);
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

static avp_status_t mp4_aac_get_upsampling_factor(uint32_t core_sample_rate,
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

static avp_status_t mp4_parse_audio_specific_config(const uint8_t *buffer,
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
    if (!mp4_aac_read_audio_object_type(&br, &declared_audio_object_type) ||
        !mp4_aac_read_sample_rate(&br, &core_sample_rate) ||
        !avp_bitreader_read(&br, 4u, &channel_config)) {
        return AVP_EBADHEADER;
    }

    core_audio_object_type = declared_audio_object_type;
    effective_audio_object_type = declared_audio_object_type;
    if (declared_audio_object_type == AAC_AUDIO_OBJECT_TYPE_SBR ||
        declared_audio_object_type == AAC_AUDIO_OBJECT_TYPE_PS) {
        if (!mp4_aac_read_sample_rate(&br, &extension_sample_rate) ||
            !mp4_aac_read_audio_object_type(&br, &core_audio_object_type)) {
            return AVP_EBADHEADER;
        }
        st = mp4_aac_get_upsampling_factor(core_sample_rate,
                                           extension_sample_rate,
                                           &upsampling_factor);
        if (st != AVP_OK) {
            return st;
        }
    }

    if (core_audio_object_type != AAC_AUDIO_OBJECT_TYPE_AAC_LC) {
        return AVP_EUNSUPPORTED;
    }
    if (!mp4_aac_parse_ga_specific_config(&br,
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

        if (!mp4_aac_read_audio_object_type(&br, &extension_audio_object_type)) {
            return AVP_EBADHEADER;
        }
        if (extension_audio_object_type == AAC_AUDIO_OBJECT_TYPE_SBR) {
            if (!avp_bitreader_read(&br, 1u, &sbr_present)) {
                return AVP_EBADHEADER;
            }
            if (sbr_present != 0u) {
                if (!mp4_aac_read_sample_rate(&br, &extension_sample_rate)) {
                    return AVP_EBADHEADER;
                }
                st = mp4_aac_get_upsampling_factor(core_sample_rate,
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

    if (channels == 0u || channels > AAC_MAX_CHANNELS ||
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

static avp_status_t mp4_next_box(const uint8_t *buffer,
                                 uint32_t buffer_size,
                                 uint32_t parent_start,
                                 uint32_t parent_size,
                                 uint32_t *pos,
                                 mp4_box_t *box)
{
    uint32_t parent_end;
    uint64_t size;
    uint32_t header_size = 8u;

    if (buffer == NULL || pos == NULL || box == NULL ||
        parent_start > buffer_size || parent_size > buffer_size - parent_start) {
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
    box->type = MP4_FOURCC(buffer[*pos + 4u],
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

    if (size < header_size || size > parent_end - *pos || size > UINT32_MAX) {
        return AVP_EBADHEADER;
    }

    box->size = (uint32_t)size;
    box->header_size = header_size;
    *pos += (uint32_t)size;
    return AVP_OK;
}

static avp_status_t mp4_find_child(const uint8_t *buffer,
                                   uint32_t buffer_size,
                                   uint32_t parent_start,
                                   uint32_t parent_size,
                                   mp4_fourcc_t type,
                                   mp4_box_t *out)
{
    uint32_t pos = parent_start;

    for (;;) {
        mp4_box_t box;
        avp_status_t st;

        st = mp4_next_box(buffer, buffer_size, parent_start, parent_size, &pos, &box);
        if (st != AVP_OK) {
            return st;
        }
        if (box.type == type) {
            *out = box;
            return AVP_OK;
        }
    }
}

static avp_status_t mp4_read_moov(mp4_demux_t *demuxer,
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
        mp4_fourcc_t type;
        avp_status_t st;

        st = avp_io_read_at(demuxer->common.avp_io, offset, header, 8u);
        if (st != AVP_OK) {
            return st;
        }

        box_size = AVP_GET_BE32(header);
        type = MP4_FOURCC(header[4], header[5], header[6], header[7]);
        if (box_size == 1u) {
            if (demuxer->common.file_size - offset < 16u) {
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

        if (box_size < header_size || box_size > demuxer->common.file_size - offset ||
            box_size > UINT32_MAX) {
            return AVP_EBADHEADER;
        }

        if (type == MP4_FOURCC('m', 'o', 'o', 'v')) {
            uint8_t *buffer = (uint8_t *)avp_malloc((size_t)box_size);

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

static avp_status_t mp4_read_descriptor_length(const uint8_t *buffer,
                                               uint32_t size,
                                               uint32_t *pos,
                                               uint32_t *length)
{
    uint32_t value = 0u;
    uint32_t i;

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

static avp_status_t mp4_find_decoder_object_type(const uint8_t *buffer,
                                                 uint32_t size,
                                                 uint8_t *object_type)
{
    uint32_t pos = 0u;

    if (buffer == NULL || object_type == NULL) {
        return AVP_EINVAL;
    }

    while (pos + 2u <= size) {
        const uint8_t *payload;
        uint8_t tag = buffer[pos++];
        uint32_t length;
        uint32_t payload_pos = 0u;
        avp_status_t st;

        st = mp4_read_descriptor_length(buffer, size, &pos, &length);
        if (st != AVP_OK || length > size - pos) {
            return AVP_EBADHEADER;
        }

        payload = buffer + pos;
        if (tag == 0x04u && length >= 1u) {
            *object_type = payload[0];
            return AVP_OK;
        }

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
            if (payload_pos > length) {
                return AVP_EBADHEADER;
            }
        }

        if (payload_pos < length) {
            st = mp4_find_decoder_object_type(payload + payload_pos,
                                              length - payload_pos,
                                              object_type);
            if (st == AVP_OK) {
                return AVP_OK;
            }
        }
        pos += length;
    }

    return AVP_ENOENT;
}

static avp_status_t mp4_find_decoder_specific(const uint8_t *buffer,
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
        uint32_t payload_pos = 0u;
        avp_status_t st;

        tag = buffer[pos++];
        st = mp4_read_descriptor_length(buffer, size, &pos, &length);
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
            if (payload_pos > length) {
                return AVP_EBADHEADER;
            }
        } else if (tag == 0x04u && length >= 13u) {
            *object_type = payload[0];
            payload_pos = 13u;
        }

        if (payload_pos < length) {
            st = mp4_find_decoder_specific(payload + payload_pos,
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

static mp4_video_codec_t mp4_video_codec_from_sample_entry(mp4_fourcc_t type)
{
    switch (type) {
        case MP4_FOURCC('r', 'a', 'w', ' '):
        case MP4_FOURCC('y', 'u', 'v', '2'):
        case MP4_FOURCC('2', 'v', 'u', 'y'):
            return MP4_VIDEO_CODEC_RAW;
        case MP4_FOURCC('j', 'p', 'e', 'g'):
        case MP4_FOURCC('m', 'j', 'p', 'a'):
        case MP4_FOURCC('m', 'j', 'p', 'b'):
        case MP4_FOURCC('m', 'j', 'p', 'g'):
        case MP4_FOURCC('A', 'V', 'D', 'J'):
            return MP4_VIDEO_CODEC_MJPEG;
        case MP4_FOURCC('p', 'n', 'g', ' '):
            return MP4_VIDEO_CODEC_PNG;
        case MP4_FOURCC('h', '2', '6', '3'):
        case MP4_FOURCC('s', '2', '6', '3'):
            return MP4_VIDEO_CODEC_H263;
        case MP4_FOURCC('a', 'v', 'c', '1'):
        case MP4_FOURCC('a', 'v', 'c', '2'):
        case MP4_FOURCC('a', 'v', 'c', '3'):
        case MP4_FOURCC('a', 'v', 'c', '4'):
            return MP4_VIDEO_CODEC_H264;
        case MP4_FOURCC('h', 'v', 'c', '1'):
        case MP4_FOURCC('h', 'e', 'v', '1'):
            return MP4_VIDEO_CODEC_HEVC;
        case MP4_FOURCC('v', 'v', 'c', '1'):
        case MP4_FOURCC('v', 'v', 'i', '1'):
            return MP4_VIDEO_CODEC_VVC;
        case MP4_FOURCC('v', 'p', '0', '8'):
            return MP4_VIDEO_CODEC_VP8;
        case MP4_FOURCC('v', 'p', '0', '9'):
            return MP4_VIDEO_CODEC_VP9;
        case MP4_FOURCC('a', 'v', '0', '1'):
            return MP4_VIDEO_CODEC_AV1;
        case MP4_FOURCC('d', 'v', 'c', ' '):
        case MP4_FOURCC('d', 'v', 'c', 'p'):
        case MP4_FOURCC('d', 'v', 'p', 'p'):
        case MP4_FOURCC('d', 'v', '5', 'n'):
        case MP4_FOURCC('d', 'v', '5', 'p'):
            return MP4_VIDEO_CODEC_DV;
        case MP4_FOURCC('a', 'p', 'c', 'h'):
        case MP4_FOURCC('a', 'p', 'c', 'n'):
        case MP4_FOURCC('a', 'p', 'c', 's'):
        case MP4_FOURCC('a', 'p', 'c', 'o'):
        case MP4_FOURCC('a', 'p', '4', 'h'):
        case MP4_FOURCC('a', 'p', '4', 'x'):
            return MP4_VIDEO_CODEC_PRORES;
        default:
            return MP4_VIDEO_CODEC_UNKNOWN;
    }
}

static mp4_audio_codec_t mp4_audio_codec_from_sample_entry(mp4_fourcc_t type)
{
    switch (type) {
        case MP4_FOURCC('r', 'a', 'w', ' '):
        case MP4_FOURCC('t', 'w', 'o', 's'):
        case MP4_FOURCC('s', 'o', 'w', 't'):
        case MP4_FOURCC('l', 'p', 'c', 'm'):
        case MP4_FOURCC('i', 'n', '2', '4'):
        case MP4_FOURCC('i', 'n', '3', '2'):
        case MP4_FOURCC('f', 'l', '3', '2'):
        case MP4_FOURCC('f', 'l', '6', '4'):
        case MP4_FOURCC('i', 'p', 'c', 'm'):
        case MP4_FOURCC('f', 'p', 'c', 'm'):
            return MP4_AUDIO_CODEC_PCM;
        case MP4_FOURCC('.', 'm', 'p', '2'):
        case MP4_FOURCC('m', 'p', '2', ' '):
            return MP4_AUDIO_CODEC_MP2;
        case MP4_FOURCC('.', 'm', 'p', '3'):
        case MP4_FOURCC('m', 'p', '3', ' '):
            return MP4_AUDIO_CODEC_MP3;
        case MP4_FOURCC('a', 'l', 'a', 'c'):
            return MP4_AUDIO_CODEC_ALAC;
        case MP4_FOURCC('f', 'L', 'a', 'C'):
            return MP4_AUDIO_CODEC_FLAC;
        case MP4_FOURCC('O', 'p', 'u', 's'):
            return MP4_AUDIO_CODEC_OPUS;
        case MP4_FOURCC('v', 'r', 'b', 's'):
            return MP4_AUDIO_CODEC_VORBIS;
        case MP4_FOURCC('a', 'c', '-', '3'):
            return MP4_AUDIO_CODEC_AC3;
        case MP4_FOURCC('e', 'c', '-', '3'):
            return MP4_AUDIO_CODEC_EAC3;
        case MP4_FOURCC('a', 'c', '-', '4'):
            return MP4_AUDIO_CODEC_AC4;
        case MP4_FOURCC('d', 't', 's', 'c'):
        case MP4_FOURCC('d', 't', 's', 'e'):
        case MP4_FOURCC('d', 't', 's', 'h'):
        case MP4_FOURCC('d', 't', 's', 'l'):
            return MP4_AUDIO_CODEC_DTS;
        case MP4_FOURCC('s', 'a', 'm', 'r'):
            return MP4_AUDIO_CODEC_AMR_NB;
        case MP4_FOURCC('s', 'a', 'w', 'b'):
            return MP4_AUDIO_CODEC_AMR_WB;
        case MP4_FOURCC('a', 'l', 'a', 'w'):
            return MP4_AUDIO_CODEC_G711_ALAW;
        case MP4_FOURCC('u', 'l', 'a', 'w'):
            return MP4_AUDIO_CODEC_G711_MULAW;
        default:
            return MP4_AUDIO_CODEC_UNKNOWN;
    }
}

static avp_status_t mp4_parse_video_sample_entry(const uint8_t *buffer,
                                                 uint32_t buffer_size,
                                                 uint32_t entry_start,
                                                 uint32_t entry_size,
                                                 mp4_track_t *track)
{
    mp4_fourcc_t type;

    if (entry_size < 86u || entry_start > buffer_size ||
        entry_size > buffer_size - entry_start) {
        return AVP_EBADHEADER;
    }

    type = MP4_FOURCC(buffer[entry_start + 4u],
                      buffer[entry_start + 5u],
                      buffer[entry_start + 6u],
                      buffer[entry_start + 7u]);
    track->sample_entry = type;
    track->width = AVP_GET_BE16(buffer + entry_start + 32u);
    track->height = AVP_GET_BE16(buffer + entry_start + 34u);
    if (track->width == 0u || track->height == 0u) {
        return AVP_EBADHEADER;
    }

    track->video_codec = mp4_video_codec_from_sample_entry(type);
    if (track->video_codec != MP4_VIDEO_CODEC_UNKNOWN) {
        return AVP_OK;
    }

    if (type == MP4_FOURCC('m', 'p', '4', 'v')) {
        mp4_box_t esds;
        uint8_t object_type;
        avp_status_t st;

        /* MP4 stores MJPEG under mp4v with JPEG object type indication. */
        st = mp4_find_child(buffer,
                            buffer_size,
                            entry_start + 86u,
                            entry_size - 86u,
                            MP4_FOURCC('e', 's', 'd', 's'),
                            &esds);
        if (st != AVP_OK || esds.size - esds.header_size < 5u) {
            return AVP_EUNSUPPORTED;
        }
        st = mp4_find_decoder_object_type(buffer + esds.start + esds.header_size + 4u,
                                          esds.size - esds.header_size - 4u,
                                          &object_type);
        if (st != AVP_OK) {
            return AVP_EUNSUPPORTED;
        }

        if (object_type == MP4_OBJECT_TYPE_MPEG4_VIDEO) {
            track->video_codec = MP4_VIDEO_CODEC_MPEG4;
        } else if (object_type >= MP4_OBJECT_TYPE_MPEG2_VIDEO_MIN &&
                   object_type <= MP4_OBJECT_TYPE_MPEG2_VIDEO_MAX) {
            track->video_codec = MP4_VIDEO_CODEC_MPEG2;
        } else if (object_type == 0x6au) {
            track->video_codec = MP4_VIDEO_CODEC_MPEG1;
        } else if (object_type == MP4_OBJECT_TYPE_JPEG) {
            track->video_codec = MP4_VIDEO_CODEC_MJPEG;
        } else {
            return AVP_EUNSUPPORTED;
        }
        return AVP_OK;
    }

    return AVP_EUNSUPPORTED;
}

static avp_status_t mp4_find_audio_esds(const uint8_t *buffer,
                                        uint32_t buffer_size,
                                        uint32_t child_start,
                                        uint32_t child_size,
                                        mp4_box_t *esds)
{
    mp4_box_t wave;
    avp_status_t st;

    st = mp4_find_child(buffer,
                        buffer_size,
                        child_start,
                        child_size,
                        MP4_FOURCC('e', 's', 'd', 's'),
                        esds);
    if (st == AVP_OK) {
        return AVP_OK;
    }

    st = mp4_find_child(buffer,
                        buffer_size,
                        child_start,
                        child_size,
                        MP4_FOURCC('w', 'a', 'v', 'e'),
                        &wave);
    if (st != AVP_OK) {
        return st;
    }
    return mp4_find_child(buffer,
                          buffer_size,
                          wave.start + wave.header_size,
                          wave.size - wave.header_size,
                          MP4_FOURCC('e', 's', 'd', 's'),
                          esds);
}

static avp_status_t mp4_parse_alac_config(const uint8_t *buffer,
                                          uint32_t buffer_size,
                                          uint32_t entry_start,
                                          uint32_t entry_size,
                                          alac_dec_config_t *config)
{
    mp4_box_t atom;
    uint32_t child_start;
    uint32_t child_size;
    uint32_t cookie_size;
    avp_status_t st;

    if (buffer == NULL || config == NULL ||
        entry_size < 36u ||
        entry_start > buffer_size ||
        entry_size > buffer_size - entry_start) {
        return AVP_EBADHEADER;
    }

    child_start = entry_start + 36u;
    child_size = entry_size - 36u;
    st = mp4_find_child(buffer,
                        buffer_size,
                        child_start,
                        child_size,
                        MP4_FOURCC('a', 'l', 'a', 'c'),
                        &atom);
    if (st != AVP_OK) {
        mp4_box_t wave;

        st = mp4_find_child(buffer,
                            buffer_size,
                            child_start,
                            child_size,
                            MP4_FOURCC('w', 'a', 'v', 'e'),
                            &wave);
        if (st == AVP_OK) {
            st = mp4_find_child(buffer,
                                buffer_size,
                                wave.start + wave.header_size,
                                wave.size - wave.header_size,
                                MP4_FOURCC('a', 'l', 'a', 'c'),
                                &atom);
        }
    }
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

    memset(config, 0, sizeof(*config));
    memcpy(config->magic_cookie,
           buffer + atom.start + atom.header_size + 4u,
           cookie_size);
    config->magic_cookie_size = cookie_size;
    return AVP_OK;
}

static avp_status_t mp4_parse_audio_sample_entry(const uint8_t *buffer,
                                                 uint32_t buffer_size,
                                                 uint32_t entry_start,
                                                 uint32_t entry_size,
                                                 mp4_track_t *track)
{
    mp4_fourcc_t type;
    uint32_t child_offset;
    uint16_t version;

    if (entry_size < 36u || entry_start > buffer_size ||
        entry_size > buffer_size - entry_start) {
        return AVP_EBADHEADER;
    }

    type = MP4_FOURCC(buffer[entry_start + 4u],
                      buffer[entry_start + 5u],
                      buffer[entry_start + 6u],
                      buffer[entry_start + 7u]);
    version = AVP_GET_BE16(buffer + entry_start + 16u);
    if (version == 0u) {
        child_offset = 36u;
    } else if (version == 1u) {
        child_offset = 52u;
    } else if (version == 2u) {
        child_offset = 72u;
    } else {
        return AVP_EUNSUPPORTED;
    }
    if (entry_size < child_offset) {
        return AVP_EBADHEADER;
    }

    track->sample_entry = type;
    if (version == 2u) {
        uint32_t channels = AVP_GET_BE32(buffer + entry_start + 48u);
        uint32_t bits_per_sample = AVP_GET_BE32(buffer + entry_start + 56u);

        track->channels = (uint16_t)channels;
        track->bits_per_sample = (uint16_t)bits_per_sample;
        track->sample_rate = track->timescale;
    } else {
        track->channels = AVP_GET_BE16(buffer + entry_start + 24u);
        track->bits_per_sample = AVP_GET_BE16(buffer + entry_start + 26u);
        track->sample_rate = AVP_GET_BE32(buffer + entry_start + 32u) >> 16;
        if (track->sample_rate == 0u) {
            track->sample_rate = track->timescale;
        }
    }
    track->audio_codec = mp4_audio_codec_from_sample_entry(type);
    if (track->audio_codec != MP4_AUDIO_CODEC_UNKNOWN) {
        if (track->audio_codec == MP4_AUDIO_CODEC_ALAC) {
            return mp4_parse_alac_config(buffer,
                                         buffer_size,
                                         entry_start,
                                         entry_size,
                                         &track->alac_config);
        }
        return AVP_OK;
    }

    if (type == MP4_FOURCC('m', 'p', '4', 'a')) {
        mp4_box_t esds;
        const uint8_t *asc;
        uint32_t asc_size;
        uint8_t object_type;
        avp_status_t st;

        st = mp4_parse_alac_config(buffer,
                                   buffer_size,
                                   entry_start,
                                   entry_size,
                                   &track->alac_config);
        if (st == AVP_OK) {
            track->audio_codec = MP4_AUDIO_CODEC_ALAC;
            return AVP_OK;
        }

        st = mp4_find_audio_esds(buffer,
                                 buffer_size,
                                 entry_start + child_offset,
                                 entry_size - child_offset,
                                 &esds);
        if (st != AVP_OK || esds.size - esds.header_size < 5u) {
            return AVP_EUNSUPPORTED;
        }
        object_type = 0u;
        asc = NULL;
        asc_size = 0u;
        st = mp4_find_decoder_specific(buffer + esds.start + esds.header_size + 4u,
                                       esds.size - esds.header_size - 4u,
                                       &object_type,
                                       &asc,
                                       &asc_size);
        if (st != AVP_OK) {
            return AVP_EUNSUPPORTED;
        }

        if (object_type == MP4_OBJECT_TYPE_AAC ||
            (object_type >= 0x66u && object_type <= 0x68u)) {
            st = mp4_parse_audio_specific_config(asc, asc_size, &track->aac_config);
            if (st != AVP_OK) {
                return st;
            }
            track->sample_rate = track->aac_config.sample_rate;
            track->channels = track->aac_config.channels;
            track->audio_codec = MP4_AUDIO_CODEC_AAC;
        } else if (object_type == 0x69u || object_type == 0x6bu) {
            track->audio_codec = MP4_AUDIO_CODEC_MP3;
        } else {
            return AVP_EUNSUPPORTED;
        }
        return AVP_OK;
    }

    return AVP_EUNSUPPORTED;
}

static avp_status_t mp4_parse_stsd(const uint8_t *buffer,
                                   uint32_t buffer_size,
                                   uint32_t payload_start,
                                   uint32_t payload_size,
                                   mp4_track_t *track)
{
    uint32_t entry_count;
    uint32_t pos;
    uint32_t end;
    uint32_t i;

    if (payload_size < 8u || payload_start > buffer_size ||
        payload_size > buffer_size - payload_start) {
        return AVP_EBADHEADER;
    }

    entry_count = AVP_GET_BE32(buffer + payload_start + 4u);
    pos = payload_start + 8u;
    end = payload_start + payload_size;
    for (i = 0u; i < entry_count; i++) {
        uint32_t entry_size;
        avp_status_t st;

        if (end - pos < 8u) {
            return AVP_EBADHEADER;
        }
        entry_size = AVP_GET_BE32(buffer + pos);
        if (entry_size < 8u || entry_size > end - pos) {
            return AVP_EBADHEADER;
        }

        if (track->type == AVP_PACKET_TYPE_VIDEO) {
            st = mp4_parse_video_sample_entry(buffer,
                                              buffer_size,
                                              pos,
                                              entry_size,
                                              track);
        } else if (track->type == AVP_PACKET_TYPE_AUDIO) {
            st = mp4_parse_audio_sample_entry(buffer,
                                              buffer_size,
                                              pos,
                                              entry_size,
                                              track);
        } else {
            return AVP_EUNSUPPORTED;
        }
        if (st == AVP_OK) {
            return AVP_OK;
        }
        if (st != AVP_EUNSUPPORTED) {
            return st;
        }
        pos += entry_size;
    }

    return AVP_EUNSUPPORTED;
}

static avp_status_t mp4_parse_stsz(const uint8_t *buffer,
                                   uint32_t payload_start,
                                   uint32_t payload_size,
                                   mp4_track_t *track)
{
    uint32_t default_size;
    uint32_t sample_count;
    uint32_t i;

    if (payload_size < 12u) {
        return AVP_EBADHEADER;
    }

    default_size = AVP_GET_BE32(buffer + payload_start + 4u);
    sample_count = AVP_GET_BE32(buffer + payload_start + 8u);
    if (sample_count == 0u ||
        sample_count > UINT32_MAX / (uint32_t)sizeof(uint32_t) ||
        (default_size == 0u && sample_count > (payload_size - 12u) / 4u)) {
        return AVP_EBADHEADER;
    }
    if (track->sample_sizes != NULL) {
        return AVP_EBADHEADER;
    }

    track->sample_sizes = (uint32_t *)avp_malloc((size_t)sample_count * sizeof(uint32_t));
    if (track->sample_sizes == NULL) {
        return AVP_ENOMEM;
    }
    track->sample_count = sample_count;
    track->max_sample_size = 0u;
    for (i = 0u; i < sample_count; i++) {
        uint32_t sample_size = default_size != 0u ?
                                   default_size :
                                   AVP_GET_BE32(buffer + payload_start + 12u + i * 4u);

        if (sample_size == 0u) {
            return AVP_EBADHEADER;
        }
        track->sample_sizes[i] = sample_size;
        if (sample_size > track->max_sample_size) {
            track->max_sample_size = sample_size;
        }
    }

    return AVP_OK;
}

static avp_status_t mp4_parse_stsc(const uint8_t *buffer,
                                   uint32_t payload_start,
                                   uint32_t payload_size,
                                   mp4_track_t *track)
{
    uint32_t entry_count;
    uint32_t i;

    if (payload_size < 8u) {
        return AVP_EBADHEADER;
    }

    entry_count = AVP_GET_BE32(buffer + payload_start + 4u);
    if (entry_count == 0u ||
        entry_count > UINT32_MAX / (uint32_t)sizeof(mp4_stsc_entry_t) ||
        entry_count > (payload_size - 8u) / 12u) {
        return AVP_EBADHEADER;
    }
    if (track->stsc != NULL) {
        return AVP_EBADHEADER;
    }

    track->stsc = (mp4_stsc_entry_t *)avp_malloc((size_t)entry_count * sizeof(*track->stsc));
    if (track->stsc == NULL) {
        return AVP_ENOMEM;
    }
    track->stsc_count = entry_count;
    for (i = 0u; i < entry_count; i++) {
        const uint8_t *entry = buffer + payload_start + 8u + i * 12u;

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

    if (track->stsc[0].first_chunk != 1u) {
        return AVP_EBADHEADER;
    }
    return AVP_OK;
}

static avp_status_t mp4_parse_stco(const uint8_t *buffer,
                                   uint32_t payload_start,
                                   uint32_t payload_size,
                                   uint8_t co64,
                                   mp4_track_t *track)
{
    uint32_t field_size = co64 != 0u ? 8u : 4u;
    uint32_t entry_count;
    uint32_t i;

    if (payload_size < 8u) {
        return AVP_EBADHEADER;
    }

    entry_count = AVP_GET_BE32(buffer + payload_start + 4u);
    if (entry_count == 0u ||
        entry_count > UINT32_MAX / (uint32_t)sizeof(uint32_t) ||
        entry_count > (payload_size - 8u) / field_size) {
        return AVP_EBADHEADER;
    }
    if (track->chunk_offsets != NULL) {
        return AVP_EBADHEADER;
    }

    track->chunk_offsets = (uint32_t *)avp_malloc((size_t)entry_count * sizeof(uint32_t));
    if (track->chunk_offsets == NULL) {
        return AVP_ENOMEM;
    }
    track->chunk_count = entry_count;
    for (i = 0u; i < entry_count; i++) {
        const uint8_t *entry = buffer + payload_start + 8u + i * field_size;
        uint64_t offset = co64 != 0u ? AVP_GET_BE64(entry) : AVP_GET_BE32(entry);

        if (offset > UINT32_MAX) {
            return AVP_ERANGE;
        }
        track->chunk_offsets[i] = (uint32_t)offset;
    }

    return AVP_OK;
}

static avp_status_t mp4_parse_mdhd(const uint8_t *buffer,
                                   uint32_t payload_start,
                                   uint32_t payload_size,
                                   mp4_track_t *track)
{
    uint8_t version;

    if (payload_size < 20u) {
        return AVP_EBADHEADER;
    }

    version = buffer[payload_start];
    if (version == 0u) {
        track->timescale = AVP_GET_BE32(buffer + payload_start + 12u);
        track->duration = AVP_GET_BE32(buffer + payload_start + 16u);
    } else if (version == 1u) {
        if (payload_size < 32u) {
            return AVP_EBADHEADER;
        }
        track->timescale = AVP_GET_BE32(buffer + payload_start + 20u);
        track->duration = AVP_GET_BE64(buffer + payload_start + 24u);
    } else {
        return AVP_EUNSUPPORTED;
    }

    return track->timescale != 0u ? AVP_OK : AVP_EBADHEADER;
}

static avp_status_t mp4_get_trak_media(const uint8_t *buffer,
                                       uint32_t buffer_size,
                                       uint32_t payload_start,
                                       uint32_t payload_size,
                                       mp4_box_t *mdia,
                                       avp_packet_type_t *type)
{
    mp4_box_t hdlr;
    uint32_t handler_start;
    mp4_fourcc_t handler;
    avp_status_t st;

    st = mp4_find_child(buffer,
                        buffer_size,
                        payload_start,
                        payload_size,
                        MP4_FOURCC('m', 'd', 'i', 'a'),
                        mdia);
    if (st != AVP_OK) {
        return st;
    }
    st = mp4_find_child(buffer,
                        buffer_size,
                        mdia->start + mdia->header_size,
                        mdia->size - mdia->header_size,
                        MP4_FOURCC('h', 'd', 'l', 'r'),
                        &hdlr);
    if (st != AVP_OK || hdlr.size - hdlr.header_size < 12u) {
        return AVP_EBADHEADER;
    }

    handler_start = hdlr.start + hdlr.header_size;
    handler = MP4_FOURCC(buffer[handler_start + 8u],
                         buffer[handler_start + 9u],
                         buffer[handler_start + 10u],
                         buffer[handler_start + 11u]);
    if (handler == MP4_FOURCC('v', 'i', 'd', 'e')) {
        *type = AVP_PACKET_TYPE_VIDEO;
    } else if (handler == MP4_FOURCC('s', 'o', 'u', 'n')) {
        *type = AVP_PACKET_TYPE_AUDIO;
    } else {
        *type = AVP_PACKET_TYPE_UNKNOWN;
    }
    return AVP_OK;
}

static avp_status_t mp4_parse_stbl(const uint8_t *buffer,
                                   uint32_t buffer_size,
                                   uint32_t payload_start,
                                   uint32_t payload_size,
                                   mp4_track_t *track)
{
    uint32_t pos = payload_start;

    for (;;) {
        mp4_box_t box;
        uint32_t box_payload;
        uint32_t box_payload_size;
        avp_status_t st;

        st = mp4_next_box(buffer, buffer_size, payload_start, payload_size, &pos, &box);
        if (st == AVP_ENOENT) {
            break;
        }
        if (st != AVP_OK) {
            return st;
        }

        box_payload = box.start + box.header_size;
        box_payload_size = box.size - box.header_size;
        if (box.type == MP4_FOURCC('s', 't', 's', 'd')) {
            st = mp4_parse_stsd(buffer, buffer_size, box_payload, box_payload_size, track);
        } else if (box.type == MP4_FOURCC('s', 't', 's', 'z')) {
            st = mp4_parse_stsz(buffer, box_payload, box_payload_size, track);
        } else if (box.type == MP4_FOURCC('s', 't', 's', 'c')) {
            st = mp4_parse_stsc(buffer, box_payload, box_payload_size, track);
        } else if (box.type == MP4_FOURCC('s', 't', 'c', 'o')) {
            st = mp4_parse_stco(buffer, box_payload, box_payload_size, 0u, track);
        } else if (box.type == MP4_FOURCC('c', 'o', '6', '4')) {
            st = mp4_parse_stco(buffer, box_payload, box_payload_size, 1u, track);
        } else {
            st = AVP_OK;
        }

        if (st != AVP_OK) {
            return st;
        }
    }

    if ((track->type == AVP_PACKET_TYPE_VIDEO &&
         track->video_codec == MP4_VIDEO_CODEC_UNKNOWN) ||
        (track->type == AVP_PACKET_TYPE_AUDIO &&
         track->audio_codec == MP4_AUDIO_CODEC_UNKNOWN)) {
        return AVP_EUNSUPPORTED;
    }
    if (track->sample_sizes == NULL || track->stsc == NULL ||
        track->chunk_offsets == NULL) {
        return AVP_EBADHEADER;
    }
    return AVP_OK;
}

static avp_status_t mp4_parse_trak(const uint8_t *buffer,
                                   uint32_t buffer_size,
                                   const mp4_box_t *trak,
                                   uint32_t stream_index,
                                   mp4_track_t *track)
{
    mp4_box_t mdia;
    mp4_box_t mdhd;
    mp4_box_t minf;
    mp4_box_t stbl;
    avp_packet_type_t type;
    uint32_t mdia_start;
    uint32_t mdia_size;
    avp_status_t st;

    memset(track, 0, sizeof(*track));
    st = mp4_get_trak_media(buffer,
                            buffer_size,
                            trak->start + trak->header_size,
                            trak->size - trak->header_size,
                            &mdia,
                            &type);
    if (st != AVP_OK) {
        return st;
    }
    if (type != AVP_PACKET_TYPE_VIDEO && type != AVP_PACKET_TYPE_AUDIO) {
        return AVP_ENOENT;
    }

    track->type = type;
    track->stream_index = stream_index;
    mdia_start = mdia.start + mdia.header_size;
    mdia_size = mdia.size - mdia.header_size;
    st = mp4_find_child(buffer,
                        buffer_size,
                        mdia_start,
                        mdia_size,
                        MP4_FOURCC('m', 'd', 'h', 'd'),
                        &mdhd);
    if (st != AVP_OK) {
        return st;
    }
    st = mp4_parse_mdhd(buffer,
                        mdhd.start + mdhd.header_size,
                        mdhd.size - mdhd.header_size,
                        track);
    if (st != AVP_OK) {
        return st;
    }

    st = mp4_find_child(buffer,
                        buffer_size,
                        mdia_start,
                        mdia_size,
                        MP4_FOURCC('m', 'i', 'n', 'f'),
                        &minf);
    if (st != AVP_OK) {
        return st;
    }
    st = mp4_find_child(buffer,
                        buffer_size,
                        minf.start + minf.header_size,
                        minf.size - minf.header_size,
                        MP4_FOURCC('s', 't', 'b', 'l'),
                        &stbl);
    if (st != AVP_OK) {
        return st;
    }

    return mp4_parse_stbl(buffer,
                          buffer_size,
                          stbl.start + stbl.header_size,
                          stbl.size - stbl.header_size,
                          track);
}

static avp_status_t mp4_build_sample_offsets(mp4_track_t *track, uint32_t file_size)
{
    uint32_t sample_index = 0u;
    uint32_t stsc_index = 0u;
    uint32_t i;

    if (track == NULL || track->sample_count == 0u || track->sample_sizes == NULL ||
        track->stsc_count == 0u || track->stsc == NULL ||
        track->chunk_count == 0u || track->chunk_offsets == NULL) {
        return AVP_EBADHEADER;
    }

    track->sample_offsets = (uint32_t *)avp_malloc((size_t)track->sample_count * sizeof(uint32_t));
    if (track->sample_offsets == NULL) {
        return AVP_ENOMEM;
    }

    for (i = 0u; i < track->chunk_count && sample_index < track->sample_count; i++) {
        uint32_t chunk_number = i + 1u;
        uint32_t current_offset = track->chunk_offsets[i];
        uint32_t samples_per_chunk;
        uint32_t j;

        while (stsc_index + 1u < track->stsc_count &&
               chunk_number >= track->stsc[stsc_index + 1u].first_chunk) {
            stsc_index++;
        }
        samples_per_chunk = track->stsc[stsc_index].samples_per_chunk;
        for (j = 0u; j < samples_per_chunk && sample_index < track->sample_count; j++) {
            uint32_t sample_size = track->sample_sizes[sample_index];

            if (current_offset > file_size || sample_size > file_size - current_offset) {
                return AVP_EBADHEADER;
            }
            track->sample_offsets[sample_index] = current_offset;
            current_offset += sample_size;
            sample_index++;
        }
    }

    if (sample_index != track->sample_count) {
        return AVP_EBADHEADER;
    }

    return AVP_OK;
}

static void mp4_take_video_track(mp4_demux_t *demuxer, mp4_track_t *track)
{
    demuxer->video.stream_index = track->stream_index;
    demuxer->video.codec_type = track->video_codec;
    demuxer->video.sample_entry = track->sample_entry;
    demuxer->video.width = track->width;
    demuxer->video.height = track->height;
    demuxer->video.timescale = track->timescale;
    demuxer->video.duration = track->duration;
    demuxer->video.sample_count = track->sample_count;
    demuxer->video.max_sample_size = track->max_sample_size;
    demuxer->video.sample_sizes = track->sample_sizes;
    demuxer->video.sample_offsets = track->sample_offsets;
    demuxer->has_video = 1u;

    track->sample_sizes = NULL;
    track->sample_offsets = NULL;
}

static void mp4_take_audio_track(mp4_demux_t *demuxer, mp4_track_t *track)
{
    demuxer->audio.stream_index = track->stream_index;
    demuxer->audio.codec_type = track->audio_codec;
    demuxer->audio.sample_entry = track->sample_entry;
    demuxer->audio.timescale = track->timescale;
    demuxer->audio.duration = track->duration;
    demuxer->audio.sample_rate = track->sample_rate;
    demuxer->audio.channels = track->channels;
    demuxer->audio.bits_per_sample = track->bits_per_sample;
    demuxer->audio.sample_count = track->sample_count;
    demuxer->audio.max_sample_size = track->max_sample_size;
    demuxer->audio.sample_sizes = track->sample_sizes;
    demuxer->audio.sample_offsets = track->sample_offsets;
    if (track->audio_codec == MP4_AUDIO_CODEC_AAC) {
        demuxer->audio.aac_config = track->aac_config;
    } else if (track->audio_codec == MP4_AUDIO_CODEC_ALAC) {
        demuxer->audio.alac_config = track->alac_config;
    }
    demuxer->has_audio = 1u;

    track->sample_sizes = NULL;
    track->sample_offsets = NULL;
}

static avp_status_t mp4_parse_moov(const uint8_t *buffer,
                                   uint32_t buffer_size,
                                   mp4_demux_t *demuxer)
{
    mp4_box_t moov;
    uint64_t declared_size;
    uint32_t pos;
    uint32_t stream_index = 0u;
    avp_status_t st;

    if (buffer == NULL || buffer_size < 8u || demuxer == NULL) {
        return AVP_EINVAL;
    }
    declared_size = AVP_GET_BE32(buffer);
    if (declared_size == 1u) {
        if (buffer_size < 16u) {
            return AVP_EBADHEADER;
        }
        declared_size = AVP_GET_BE64(buffer + 8u);
    } else if (declared_size == 0u) {
        declared_size = buffer_size;
    }
    if (declared_size != buffer_size ||
        MP4_FOURCC(buffer[4], buffer[5], buffer[6], buffer[7]) !=
            MP4_FOURCC('m', 'o', 'o', 'v')) {
        return AVP_EBADHEADER;
    }

    memset(&moov, 0, sizeof(moov));
    moov.start = 0u;
    moov.size = buffer_size;
    moov.header_size = AVP_GET_BE32(buffer) == 1u ? 16u : 8u;
    moov.type = MP4_FOURCC('m', 'o', 'o', 'v');
    pos = moov.header_size;

    for (;;) {
        mp4_box_t trak;

        st = mp4_next_box(buffer,
                          buffer_size,
                          moov.header_size,
                          moov.size - moov.header_size,
                          &pos,
                          &trak);
        if (st == AVP_ENOENT) {
            break;
        }
        if (st != AVP_OK) {
            return st;
        }
        if (trak.type != MP4_FOURCC('t', 'r', 'a', 'k')) {
            continue;
        }

        mp4_track_t track;

        st = mp4_parse_trak(buffer, buffer_size, &trak, stream_index, &track);
        if (st == AVP_OK) {
            if ((track.type == AVP_PACKET_TYPE_VIDEO && demuxer->has_video == 0u) ||
                (track.type == AVP_PACKET_TYPE_AUDIO && demuxer->has_audio == 0u)) {
                st = mp4_build_sample_offsets(&track, demuxer->common.file_size);
                if (st == AVP_OK) {
                    if (track.type == AVP_PACKET_TYPE_VIDEO) {
                        mp4_take_video_track(demuxer, &track);
                    } else {
                        mp4_take_audio_track(demuxer, &track);
                    }
                }
            }
        }
        mp4_track_deinit(&track);
        if (st != AVP_OK && st != AVP_ENOENT && st != AVP_EUNSUPPORTED) {
            return st;
        }

        stream_index++;
    }

    demuxer->stream_count = stream_index;
    return stream_index != 0u ? AVP_OK : AVP_EUNSUPPORTED;
}

avp_status_t mp4_demux_open(mp4_demux_t *demuxer, avp_io_t *avp_io)
{
    uint8_t *moov_buffer = NULL;
    uint32_t moov_size = 0u;
    int size;
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
    st = mp4_read_moov(demuxer, &moov_buffer, &moov_size);
    if (st != AVP_OK) {
        goto fail;
    }
    st = mp4_parse_moov(moov_buffer, moov_size, demuxer);
    if (st != AVP_OK) {
        goto fail;
    }
    if (moov_buffer != NULL) {
        avp_free(moov_buffer);
    }
    if (demuxer->has_video == 0u && demuxer->has_audio == 0u) {
        st = AVP_EUNSUPPORTED;
        goto fail;
    }

    avp_packet_t first_packet;

    memset(&first_packet, 0, sizeof(first_packet));
    st = mp4_demux_peek_packet(demuxer, &first_packet);
    if (st != AVP_OK) {
        goto fail;
    }

    demuxer->common.stream_offset = first_packet.offset;
    demuxer->common.stream_size = demuxer->common.file_size > first_packet.offset ?
                                      demuxer->common.file_size - first_packet.offset :
                                      0u;
    demuxer->common.current_offset = first_packet.offset;
    demuxer->common.packet_index = 0u;
    st = avp_io_seek(avp_io, demuxer->common.stream_offset);
    if (st != AVP_OK) {
        goto fail;
    }

    return AVP_OK;
fail:
    if (demuxer->video.sample_sizes != NULL) {
        avp_free(demuxer->video.sample_sizes);
    }
    if (demuxer->video.sample_offsets != NULL) {
        avp_free(demuxer->video.sample_offsets);
    }
    if (demuxer->audio.sample_sizes != NULL) {
        avp_free(demuxer->audio.sample_sizes);
    }
    if (demuxer->audio.sample_offsets != NULL) {
        avp_free(demuxer->audio.sample_offsets);
    }
    return st;
}

void mp4_demux_close(mp4_demux_t *demuxer)
{
    if (demuxer == NULL) {
        return;
    }

    if (demuxer->video.sample_sizes != NULL) {
        avp_free(demuxer->video.sample_sizes);
    }
    if (demuxer->video.sample_offsets != NULL) {
        avp_free(demuxer->video.sample_offsets);
    }
    if (demuxer->audio.sample_sizes != NULL) {
        avp_free(demuxer->audio.sample_sizes);
    }
    if (demuxer->audio.sample_offsets != NULL) {
        avp_free(demuxer->audio.sample_offsets);
    }
    memset(demuxer, 0, sizeof(*demuxer));
}

avp_status_t mp4_demux_get_audio_stream_config(const mp4_demux_t *demuxer,
                                               audio_codec_dec_config_t *config)
{
    if (demuxer == NULL || config == NULL) {
        return AVP_EINVAL;
    }
    if (demuxer->has_audio == 0u) {
        return AVP_ENOENT;
    }
    if (demuxer->audio.codec_type != MP4_AUDIO_CODEC_AAC &&
        demuxer->audio.codec_type != MP4_AUDIO_CODEC_ALAC) {
        return AVP_EUNSUPPORTED;
    }

    if (demuxer->audio.codec_type == MP4_AUDIO_CODEC_AAC) {
        config->codec_type = AUDIO_CODEC_ID_AAC;
        config->aac_config = demuxer->audio.aac_config;
    } else if (demuxer->audio.codec_type == MP4_AUDIO_CODEC_ALAC) {
        config->codec_type = AUDIO_CODEC_ID_ALAC;
        config->alac_config = demuxer->audio.alac_config;
    }
    return AVP_OK;
}

static void mp4_fill_packet(avp_packet_t *packet,
                            uint32_t offset,
                            uint32_t size,
                            uint32_t stream_index,
                            avp_packet_type_t type)
{
    packet->offset = offset;
    packet->size = size;
    packet->index = stream_index;
    packet->type = type;
}

avp_status_t mp4_demux_peek_packet(mp4_demux_t *demuxer, avp_packet_t *packet)
{
    uint8_t video_available;
    uint8_t audio_available;

    if (demuxer == NULL || packet == NULL) {
        return AVP_EINVAL;
    }

    video_available = demuxer->has_video != 0u &&
                      demuxer->video.sample_index < demuxer->video.sample_count;
    audio_available = demuxer->has_audio != 0u &&
                      demuxer->audio.sample_index < demuxer->audio.sample_count;
    if (video_available == 0u && audio_available == 0u) {
        return AVP_ENOENT;
    }

    /* Chunk offsets retain the muxer's interleaved packet order. */
    if (video_available != 0u &&
        (audio_available == 0u ||
         demuxer->video.sample_offsets[demuxer->video.sample_index] <=
             demuxer->audio.sample_offsets[demuxer->audio.sample_index])) {
        mp4_fill_packet(packet,
                        demuxer->video.sample_offsets[demuxer->video.sample_index],
                        demuxer->video.sample_sizes[demuxer->video.sample_index],
                        demuxer->video.stream_index,
                        AVP_PACKET_TYPE_VIDEO);
    } else {
        mp4_fill_packet(packet,
                        demuxer->audio.sample_offsets[demuxer->audio.sample_index],
                        demuxer->audio.sample_sizes[demuxer->audio.sample_index],
                        demuxer->audio.stream_index,
                        AVP_PACKET_TYPE_AUDIO);
    }

    return AVP_OK;
}

avp_status_t mp4_demux_pop_packet(mp4_demux_t *demuxer, avp_packet_t *packet)
{
    avp_status_t st;

    if (demuxer == NULL || packet == NULL || packet->buf == NULL ||
        packet->size == 0u) {
        return AVP_EINVAL;
    }

    st = avp_io_read_at(demuxer->common.avp_io,
                        packet->offset,
                        packet->buf,
                        packet->size);

    if (packet->type == AVP_PACKET_TYPE_VIDEO) {
        demuxer->video.sample_index++;
    } else {
        demuxer->audio.sample_index++;
    }
    demuxer->common.current_offset = packet->offset + packet->size;
    demuxer->common.packet_index++;
    return st;
}

void mp4_demux_rewind(mp4_demux_t *demuxer)
{
    if (demuxer == NULL) {
        return;
    }

    demuxer->video.sample_index = 0u;
    demuxer->audio.sample_index = 0u;
    demuxer->common.current_offset = demuxer->common.stream_offset;
    demuxer->common.packet_index = 0u;
    avp_io_seek(demuxer->common.avp_io, demuxer->common.stream_offset);
}

const char *mp4_video_codec_name(mp4_video_codec_t codec)
{
    switch (codec) {
        case MP4_VIDEO_CODEC_RAW:
            return "raw video";
        case MP4_VIDEO_CODEC_MJPEG:
            return "MJPEG";
        case MP4_VIDEO_CODEC_PNG:
            return "PNG";
        case MP4_VIDEO_CODEC_H263:
            return "H.263";
        case MP4_VIDEO_CODEC_MPEG1:
            return "MPEG-1 Video";
        case MP4_VIDEO_CODEC_MPEG2:
            return "MPEG-2 Video";
        case MP4_VIDEO_CODEC_MPEG4:
            return "MPEG-4 Visual";
        case MP4_VIDEO_CODEC_H264:
            return "H.264";
        case MP4_VIDEO_CODEC_HEVC:
            return "HEVC";
        case MP4_VIDEO_CODEC_VVC:
            return "VVC";
        case MP4_VIDEO_CODEC_VP8:
            return "VP8";
        case MP4_VIDEO_CODEC_VP9:
            return "VP9";
        case MP4_VIDEO_CODEC_AV1:
            return "AV1";
        case MP4_VIDEO_CODEC_DV:
            return "DV";
        case MP4_VIDEO_CODEC_PRORES:
            return "ProRes";
        default:
            return "unknown";
    }
}

const char *mp4_audio_codec_name(mp4_audio_codec_t codec)
{
    switch (codec) {
        case MP4_AUDIO_CODEC_PCM:
            return "PCM";
        case MP4_AUDIO_CODEC_AAC:
            return "AAC";
        case MP4_AUDIO_CODEC_MP2:
            return "MP2";
        case MP4_AUDIO_CODEC_MP3:
            return "MP3";
        case MP4_AUDIO_CODEC_ALAC:
            return "ALAC";
        case MP4_AUDIO_CODEC_FLAC:
            return "FLAC";
        case MP4_AUDIO_CODEC_OPUS:
            return "Opus";
        case MP4_AUDIO_CODEC_VORBIS:
            return "Vorbis";
        case MP4_AUDIO_CODEC_AC3:
            return "AC-3";
        case MP4_AUDIO_CODEC_EAC3:
            return "E-AC-3";
        case MP4_AUDIO_CODEC_AC4:
            return "AC-4";
        case MP4_AUDIO_CODEC_DTS:
            return "DTS";
        case MP4_AUDIO_CODEC_AMR_NB:
            return "AMR-NB";
        case MP4_AUDIO_CODEC_AMR_WB:
            return "AMR-WB";
        case MP4_AUDIO_CODEC_G711_ALAW:
            return "G.711 A-law";
        case MP4_AUDIO_CODEC_G711_MULAW:
            return "G.711 mu-law";
        default:
            return "unknown";
    }
}
