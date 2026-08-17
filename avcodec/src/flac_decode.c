/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "flac_codec.h"

#include "FLAC/stream_decoder.h"

static uint8_t flac_crc8(const uint8_t *buffer, uint32_t size)
{
    uint8_t crc = 0u;
    uint32_t i;

    for (i = 0u; i < size; i++) {
        uint8_t bit;

        crc ^= buffer[i];
        for (bit = 0u; bit < 8u; bit++) {
            crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ 0x07u) :
                                  (uint8_t)(crc << 1);
        }
    }

    return crc;
}

static avp_status_t flac_parse_utf8_uint_buffer(const uint8_t *buffer,
                                                uint32_t buffer_size,
                                                uint32_t *value,
                                                uint8_t *size)
{
    uint8_t first;
    uint8_t bytes;
    uint8_t i;
    uint32_t v;

    if (buffer == NULL || value == NULL || size == NULL || buffer_size == 0u) {
        return AVP_EINVAL;
    }

    first = buffer[0];
    if ((first & 0x80u) == 0u) {
        *value = first;
        *size = 1u;
        return AVP_OK;
    }

    if ((first & 0xe0u) == 0xc0u) {
        bytes = 2u;
        v = first & 0x1fu;
    } else if ((first & 0xf0u) == 0xe0u) {
        bytes = 3u;
        v = first & 0x0fu;
    } else if ((first & 0xf8u) == 0xf0u) {
        bytes = 4u;
        v = first & 0x07u;
    } else if ((first & 0xfcu) == 0xf8u) {
        bytes = 5u;
        v = first & 0x03u;
    } else if ((first & 0xfeu) == 0xfcu) {
        bytes = 6u;
        v = first & 0x01u;
    } else {
        return AVP_EBADHEADER;
    }

    if (bytes > buffer_size) {
        return AVP_ELACKFRAME;
    }

    for (i = 1u; i < bytes; i++) {
        uint8_t next = buffer[i];

        if ((next & 0xc0u) != 0x80u) {
            return AVP_EBADHEADER;
        }
        v = (v << 6) | (next & 0x3fu);
    }

    *value = v;
    *size = bytes;
    return AVP_OK;
}

static uint32_t flac_block_size_from_code(uint8_t code,
                                          uint8_t extra8,
                                          uint16_t extra16)
{
    switch (code) {
        case FLAC_BLOCK_SIZE_CODE_192:
            return FLAC_BLOCK_SIZE_192;
        case FLAC_BLOCK_SIZE_CODE_576:
        case FLAC_BLOCK_SIZE_CODE_1152:
        case FLAC_BLOCK_SIZE_CODE_2304:
        case FLAC_BLOCK_SIZE_CODE_4608:
            return FLAC_BLOCK_SIZE_576 << (code - FLAC_BLOCK_SIZE_CODE_576);
        case FLAC_BLOCK_SIZE_CODE_EXTRA8:
            return (uint32_t)extra8 + 1u;
        case FLAC_BLOCK_SIZE_CODE_EXTRA16:
            return (uint32_t)extra16 + 1u;
        case FLAC_BLOCK_SIZE_CODE_256:
        case FLAC_BLOCK_SIZE_CODE_512:
        case FLAC_BLOCK_SIZE_CODE_1024:
        case FLAC_BLOCK_SIZE_CODE_2048:
        case FLAC_BLOCK_SIZE_CODE_4096:
        case FLAC_BLOCK_SIZE_CODE_8192:
        case FLAC_BLOCK_SIZE_CODE_16384:
        case FLAC_BLOCK_SIZE_CODE_32768:
            return FLAC_BLOCK_SIZE_256 << (code - FLAC_BLOCK_SIZE_CODE_256);
        default:
            return 0u;
    }
}

static uint32_t flac_sample_rate_from_code(uint8_t code,
                                           uint8_t extra8,
                                           uint16_t extra16)
{
    switch (code) {
        case 1u:
            return 88200u;
        case 2u:
            return 176400u;
        case 3u:
            return 192000u;
        case 4u:
            return 8000u;
        case 5u:
            return 16000u;
        case 6u:
            return 22050u;
        case 7u:
            return 24000u;
        case 8u:
            return 32000u;
        case 9u:
            return 44100u;
        case 10u:
            return 48000u;
        case 11u:
            return 96000u;
        case 12u:
            return (uint32_t)extra8 * 1000u;
        case 13u:
            return extra16;
        case 14u:
            return (uint32_t)extra16 * 10u;
        default:
            return 0u;
    }
}

static uint8_t flac_bits_from_code(uint8_t code)
{
    switch (code) {
        case 1u:
            return 8u;
        case 2u:
            return 12u;
        case 4u:
            return 16u;
        case 5u:
            return 20u;
        case 6u:
            return 24u;
        default:
            return 0u;
    }
}

static int flac_channel_assignment_is_valid(uint8_t channel_assignment)
{
    if (channel_assignment <= FLAC_CHANNEL_ASSIGNMENT_INDEPENDENT_MAX) {
        return 1;
    }

    return channel_assignment >= FLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE &&
           channel_assignment <= FLAC_CHANNEL_ASSIGNMENT_MID_SIDE;
}

static uint8_t flac_channels_from_assignment(uint8_t channel_assignment)
{
    if (channel_assignment <= FLAC_CHANNEL_ASSIGNMENT_INDEPENDENT_MAX) {
        return (uint8_t)(channel_assignment + 1u);
    }

    if (flac_channel_assignment_is_valid(channel_assignment)) {
        return 2u;
    }

    return 0u;
}

static avp_status_t flac_parse_frame_header_fields(const uint8_t *buffer,
                                                   uint32_t size,
                                                   flac_frame_info_t *frame)
{
    uint8_t utf8_size;
    uint8_t block_size_code;
    uint8_t sample_rate_code;
    uint8_t channel_assignment;
    uint8_t sample_size_code;
    uint8_t reserved;
    uint8_t block_extra8 = 0u;
    uint16_t block_extra16 = 0u;
    uint8_t rate_extra8 = 0u;
    uint16_t rate_extra16 = 0u;
    uint32_t number;
    uint32_t pos;
    uint32_t block_size;
    uint32_t sample_rate;
    uint8_t bits_per_sample;
    uint8_t channels;
    uint32_t header_size;
    avp_status_t st;

    if (buffer == NULL) {
        return AVP_EINVAL;
    }

    if (size < FLAC_MIN_FRAME_HEADER_SIZE) {
        return AVP_ELACKFRAME;
    }

    if (buffer[0] != 0xffu ||
        (buffer[1] & 0xfeu) != 0xf8u) {
        return AVP_EBADHEADER;
    }

    reserved = (buffer[1] >> 1) & 0x01u;
    block_size_code = (buffer[2] >> 4) & 0x0fu;
    sample_rate_code = buffer[2] & 0x0fu;
    channel_assignment = (buffer[3] >> 4) & 0x0fu;
    sample_size_code = (buffer[3] >> 1) & 0x07u;

    if (reserved != 0u ||
        block_size_code == 0u ||
        sample_rate_code == 0u ||
        sample_rate_code == 15u ||
        !flac_channel_assignment_is_valid(channel_assignment) ||
        sample_size_code == 0u ||
        sample_size_code == 3u ||
        sample_size_code == 7u) {
        return AVP_EBADHEADER;
    }

    st = flac_parse_utf8_uint_buffer(buffer + 4u, size - 4u, &number, &utf8_size);
    if (st != AVP_OK) {
        return st;
    }

    pos = 4u + utf8_size;
    if (pos >= size) {
        return AVP_ELACKFRAME;
    }

    if (block_size_code == 6u) {
        block_extra8 = buffer[pos];
        pos += 1u;
    } else if (block_size_code == 7u) {
        if (pos + 1u >= size) {
            return AVP_ELACKFRAME;
        }
        block_extra16 = AVP_GET_BE16(&buffer[pos]);
        pos += 2u;
    }

    if (sample_rate_code == 12u) {
        if (pos >= size) {
            return AVP_ELACKFRAME;
        }
        rate_extra8 = buffer[pos];
        pos += 1u;
    } else if (sample_rate_code == 13u || sample_rate_code == 14u) {
        if (pos + 1u >= size) {
            return AVP_ELACKFRAME;
        }
        rate_extra16 = AVP_GET_BE16(&buffer[pos]);
        pos += 2u;
    }

    if (pos >= size) {
        return AVP_ELACKFRAME;
    }

    header_size = pos + 1u;
    if (flac_crc8(buffer, header_size - 1u) != buffer[header_size - 1u]) {
        return AVP_EBADHEADER;
    }

    block_size = flac_block_size_from_code(block_size_code, block_extra8, block_extra16);
    sample_rate = flac_sample_rate_from_code(sample_rate_code, rate_extra8, rate_extra16);
    bits_per_sample = flac_bits_from_code(sample_size_code);
    channels = flac_channels_from_assignment(channel_assignment);

    if (block_size == 0u ||
        sample_rate == 0u ||
        bits_per_sample == 0u ||
        channels == 0u) {
        return AVP_EBADHEADER;
    }

    if (frame != NULL) {
        memset(frame, 0, sizeof(*frame));
        frame->header.sync_code = (uint16_t)(((uint16_t)buffer[0] << 6) | (buffer[1] >> 2));
        frame->header.reserved = reserved;
        frame->header.blocking_strategy = buffer[1] & 0x01u;
        frame->header.block_size_code = block_size_code;
        frame->header.sample_rate_code = sample_rate_code;
        frame->header.channel_assignment = channel_assignment;
        frame->header.sample_size_code = sample_size_code;
        frame->header.reserved2 = buffer[3] & 0x01u;
        frame->frame_or_sample_number = number;
        frame->block_size = block_size;
        frame->samples_per_channel = block_size;
        frame->sample_rate = sample_rate;
        frame->bits_per_sample = bits_per_sample;
        frame->channels = channels;
        frame->header_size = (uint8_t)header_size;
        frame->crc8 = buffer[header_size - 1u];
    }

    return AVP_OK;
}

avp_status_t flac_parse_frame_header(const uint8_t *buffer,
                                     uint32_t size,
                                     flac_frame_info_t *frame)
{
    flac_frame_info_t local_frame;
    avp_status_t st;

    if (frame == NULL) {
        return AVP_EINVAL;
    }

    st = flac_parse_frame_header_fields(buffer,
                                        size,
                                        &local_frame);
    if (st != AVP_OK) {
        return st;
    }

    local_frame.frame_size = 0u;

    if (local_frame.bits_per_sample > 24u ||
        local_frame.channels > 8u ||
        !flac_channel_assignment_is_valid(local_frame.header.channel_assignment)) {
        return AVP_EBADHEADER;
    }

    if (local_frame.header.channel_assignment >= 8u && local_frame.channels != 2u) {
        return AVP_EBADHEADER;
    }

    *frame = local_frame;
    return AVP_OK;
}

const char *flac_metadata_type_name(uint8_t type)
{
    switch (type) {
        case FLAC_METADATA_STREAMINFO:
            return "STREAMINFO";
        case FLAC_METADATA_PADDING:
            return "PADDING";
        case FLAC_METADATA_APPLICATION:
            return "APPLICATION";
        case FLAC_METADATA_SEEKTABLE:
            return "SEEKTABLE";
        case FLAC_METADATA_VORBIS_COMMENT:
            return "VORBIS_COMMENT";
        case FLAC_METADATA_CUESHEET:
            return "CUESHEET";
        case FLAC_METADATA_PICTURE:
            return "PICTURE";
        default:
            return "unknown";
    }
}

#define FLAC_NATIVE_STREAMINFO_SIZE (FLAC_MARKER_SIZE + FLAC_METADATA_HEADER_SIZE + FLAC_STREAMINFO_SIZE)

typedef struct {
    FLAC__StreamDecoder *stream_decoder;
    uint8_t stream_meta[FLAC_NATIVE_STREAMINFO_SIZE];
    const uint8_t *input;
    uint32_t input_size;
    uint32_t input_pos;
    uint32_t stream_meta_pos;
    audio_codec_dec_out_frame_t *out_frame;
    uint32_t frame_bytes;
    uint8_t write_called;
    uint8_t error_seen;
} flac_pcm_decoder_t;

static avp_status_t flac_build_native_stream_meta(uint8_t stream_meta[FLAC_NATIVE_STREAMINFO_SIZE],
                                                  const uint8_t *streaminfo,
                                                  uint32_t streaminfo_size)
{
    if (stream_meta == NULL ||
        streaminfo == NULL ||
        streaminfo_size != FLAC_STREAMINFO_SIZE) {
        return AVP_EINVAL;
    }

    memset(stream_meta, 0, FLAC_NATIVE_STREAMINFO_SIZE);
    stream_meta[0] = 'f';
    stream_meta[1] = 'L';
    stream_meta[2] = 'a';
    stream_meta[3] = 'C';
    stream_meta[4] = 0x80u | FLAC_METADATA_STREAMINFO;
    stream_meta[7] = FLAC_STREAMINFO_SIZE;
    memcpy(&stream_meta[FLAC_MARKER_SIZE + FLAC_METADATA_HEADER_SIZE],
           streaminfo,
           FLAC_STREAMINFO_SIZE);
    return AVP_OK;
}

static int16_t flac_libflac_to_pcm16(FLAC__int32 sample, uint32_t bits_per_sample)
{
    FLAC__int32 value;

    if (bits_per_sample > 16u) {
        value = sample >> (bits_per_sample - 16u);
    } else {
        value = sample << (16u - bits_per_sample);
    }

    if (value > 32767) {
        value = 32767;
    } else if (value < -32768) {
        value = -32768;
    }

    return (int16_t)value;
}

static FLAC__StreamDecoderReadStatus flac_libflac_read_cb(const FLAC__StreamDecoder *stream_decoder,
                                                          FLAC__byte buffer[],
                                                          size_t *bytes,
                                                          void *client_data)
{
    flac_pcm_decoder_t *decoder = (flac_pcm_decoder_t *)client_data;
    size_t copied = 0u;

    ARG_UNUSED(stream_decoder);
    if (decoder == NULL || buffer == NULL || bytes == NULL || *bytes == 0u) {
        if (bytes != NULL) {
            *bytes = 0u;
        }
        return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    }

    while (copied < *bytes) {
        uint32_t remaining;
        size_t copy_size;

        if (decoder->stream_meta_pos < FLAC_NATIVE_STREAMINFO_SIZE) {
            remaining = FLAC_NATIVE_STREAMINFO_SIZE - decoder->stream_meta_pos;
            copy_size = *bytes - copied;
            if (copy_size > remaining) {
                copy_size = remaining;
            }
            memcpy(buffer + copied,
                   decoder->stream_meta + decoder->stream_meta_pos,
                   copy_size);
            decoder->stream_meta_pos += (uint32_t)copy_size;
            copied += copy_size;
            continue;
        }

        if (decoder->input_pos >= decoder->input_size) {
            break;
        }

        remaining = decoder->input_size - decoder->input_pos;
        copy_size = *bytes - copied;
        if (copy_size > remaining) {
            copy_size = remaining;
        }
        memcpy(buffer + copied, decoder->input + decoder->input_pos, copy_size);
        decoder->input_pos += (uint32_t)copy_size;
        copied += copy_size;
    }

    *bytes = copied;
    return copied == 0u ? FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM :
                          FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

static FLAC__bool flac_libflac_eof_cb(const FLAC__StreamDecoder *stream_decoder,
                                      void *client_data)
{
    flac_pcm_decoder_t *decoder = (flac_pcm_decoder_t *)client_data;

    ARG_UNUSED(stream_decoder);
    return decoder == NULL ||
           (decoder->stream_meta_pos >= FLAC_NATIVE_STREAMINFO_SIZE &&
            decoder->input_pos >= decoder->input_size);
}

static FLAC__StreamDecoderWriteStatus flac_libflac_write_cb(const FLAC__StreamDecoder *stream_decoder,
                                                            const FLAC__Frame *frame,
                                                            const FLAC__int32 *const buffer[],
                                                            void *client_data)
{
    flac_pcm_decoder_t *decoder = (flac_pcm_decoder_t *)client_data;
    audio_codec_dec_out_frame_t *out_frame;
    uint32_t samples;
    uint32_t channels;
    uint32_t bits_per_sample;
    uint32_t required_size;
    uint32_t i;
    uint32_t ch;

    ARG_UNUSED(stream_decoder);
    if (decoder == NULL || frame == NULL || buffer == NULL || decoder->out_frame == NULL) {
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    out_frame = decoder->out_frame;
    samples = frame->header.blocksize;
    channels = frame->header.channels;
    bits_per_sample = frame->header.bits_per_sample;
    if (samples == 0u || channels == 0u || channels > FLAC__MAX_CHANNELS ||
        bits_per_sample == 0u || bits_per_sample > 32u) {
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    required_size = samples * channels * (uint32_t)sizeof(int16_t);
    out_frame->require_size = required_size;
    if (out_frame->buffer == NULL || out_frame->size < required_size) {
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
    }

    for (i = 0u; i < samples; i++) {
        for (ch = 0u; ch < channels; ch++) {
            out_frame->buffer[i * channels + ch] = flac_libflac_to_pcm16(buffer[ch][i], bits_per_sample);
        }
    }

    out_frame->sample_rate = frame->header.sample_rate;
    out_frame->bitrate = audio_codec_calc_bitrate_kbps(decoder->frame_bytes,
                                                       frame->header.sample_rate,
                                                       samples);
    out_frame->samples_per_channel = samples;
    out_frame->duration_ms = audio_codec_calc_duration_ms(samples, frame->header.sample_rate);
    out_frame->channels = (uint8_t)channels;
    out_frame->bits_per_sample = 16u;
    out_frame->pcm_size = required_size;
    decoder->write_called = 1u;
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void flac_libflac_metadata_cb(const FLAC__StreamDecoder *stream_decoder,
                                     const FLAC__StreamMetadata *metadata,
                                     void *client_data)
{
    ARG_UNUSED(stream_decoder);
    ARG_UNUSED(metadata);
    ARG_UNUSED(client_data);
}

static void flac_libflac_error_cb(const FLAC__StreamDecoder *stream_decoder,
                                  FLAC__StreamDecoderErrorStatus status,
                                  void *client_data)
{
    flac_pcm_decoder_t *decoder = (flac_pcm_decoder_t *)client_data;

    ARG_UNUSED(stream_decoder);
    ARG_UNUSED(status);
    if (decoder != NULL) {
        decoder->error_seen = 1u;
    }
}

audio_codec_dec_handle_t flac_pcm_decode_open(const flac_dec_config_t *config)
{
    flac_pcm_decoder_t *decoder;
    FLAC__StreamDecoderInitStatus init_status;
    avp_status_t st;

    if (config == NULL || config->streaminfo_size != FLAC_STREAMINFO_SIZE) {
        return NULL;
    }

    decoder = (flac_pcm_decoder_t *)avp_malloc(sizeof(*decoder));
    if (decoder == NULL) {
        return NULL;
    }

    memset(decoder, 0, sizeof(*decoder));
    decoder->stream_decoder = FLAC__stream_decoder_new();
    if (decoder->stream_decoder == NULL) {
        avp_free(decoder);
        return NULL;
    }

    st = flac_build_native_stream_meta(decoder->stream_meta,
                                       config->streaminfo,
                                       config->streaminfo_size);
    if (st != AVP_OK) {
        FLAC__stream_decoder_delete(decoder->stream_decoder);
        avp_free(decoder);
        return NULL;
    }

    if (!FLAC__stream_decoder_set_metadata_ignore_all(decoder->stream_decoder)) {
        FLAC__stream_decoder_delete(decoder->stream_decoder);
        avp_free(decoder);
        return NULL;
    }

    init_status = FLAC__stream_decoder_init_stream(decoder->stream_decoder,
                                                   flac_libflac_read_cb,
                                                   NULL,
                                                   NULL,
                                                   NULL,
                                                   flac_libflac_eof_cb,
                                                   flac_libflac_write_cb,
                                                   flac_libflac_metadata_cb,
                                                   flac_libflac_error_cb,
                                                   decoder);
    if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        FLAC__stream_decoder_delete(decoder->stream_decoder);
        avp_free(decoder);
        return NULL;
    }

    if (!FLAC__stream_decoder_process_until_end_of_metadata(decoder->stream_decoder) ||
        decoder->error_seen != 0u ||
        decoder->stream_meta_pos < FLAC_NATIVE_STREAMINFO_SIZE ||
        FLAC__stream_decoder_get_state(decoder->stream_decoder) !=
            FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC) {
        FLAC__stream_decoder_finish(decoder->stream_decoder);
        FLAC__stream_decoder_delete(decoder->stream_decoder);
        avp_free(decoder);
        return NULL;
    }

    return (audio_codec_dec_handle_t)decoder;
}

void flac_pcm_decode_close(audio_codec_dec_handle_t handle)
{
    flac_pcm_decoder_t *decoder = (flac_pcm_decoder_t *)handle;

    if (decoder == NULL) {
        return;
    }

    if (decoder->stream_decoder != NULL) {
        FLAC__stream_decoder_finish(decoder->stream_decoder);
        FLAC__stream_decoder_delete(decoder->stream_decoder);
    }
    avp_free(decoder);
}

avp_status_t flac_pcm_decode_frame(audio_codec_dec_handle_t handle,
                                   audio_codec_dec_in_frame_t *in_frame,
                                   audio_codec_dec_out_frame_t *out_frame)
{
    flac_pcm_decoder_t *decoder = (flac_pcm_decoder_t *)handle;
    flac_frame_info_t frame;
    avp_status_t st;

    if (decoder == NULL ||
        in_frame == NULL || out_frame == NULL ||
        in_frame->buffer == NULL || out_frame->buffer == NULL) {
        return AVP_EINVAL;
    }

    out_frame->require_size = 0u;
    out_frame->pcm_size = 0u;
    in_frame->consumed_size = 0u;

    st = flac_parse_frame_header(in_frame->buffer,
                                 in_frame->size,
                                 &frame);
    if (st != AVP_OK) {
        if (st == AVP_ELACKFRAME && in_frame->size >= FLAC_MIN_FRAME_HEADER_SIZE) {
            return AVP_EBADFRAME;
        }
        return st;
    }

    out_frame->require_size = frame.block_size *
                              (uint32_t)frame.channels *
                              (uint32_t)sizeof(int16_t);
    if (out_frame->size < out_frame->require_size) {
        return AVP_EBUFFER;
    }

    decoder->input = in_frame->buffer;
    decoder->input_size = in_frame->size;
    decoder->input_pos = 0u;
    decoder->out_frame = out_frame;
    decoder->frame_bytes = in_frame->size;
    decoder->write_called = 0u;
    decoder->error_seen = 0u;

    while (decoder->write_called == 0u && decoder->error_seen == 0u) {
        if (!FLAC__stream_decoder_process_single(decoder->stream_decoder)) {
            decoder->out_frame = NULL;
            return AVP_EBADFRAME;
        }
        if (decoder->stream_meta_pos >= FLAC_NATIVE_STREAMINFO_SIZE &&
            decoder->input_pos >= decoder->input_size &&
            decoder->write_called == 0u) {
            decoder->out_frame = NULL;
            return AVP_EBADFRAME;
        }
    }

    decoder->out_frame = NULL;
    if (decoder->error_seen != 0u || decoder->write_called == 0u) {
        return AVP_EBADFRAME;
    }

    in_frame->consumed_size = in_frame->size;
    return AVP_OK;
}
