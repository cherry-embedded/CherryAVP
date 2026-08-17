/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "alac_codec.h"

#include "ALACBitUtilities.h"
#include "ALACDecoder.h"

typedef struct {
    ALACDecoder *decoder;
    uint32_t frame_length;
    uint8_t bit_depth;
    uint8_t channels;
    uint32_t sample_rate;
    uint8_t *decode_buffer;
    uint32_t decode_buffer_size;
} alac_decoder_t;

static uint32_t alac_bytes_per_sample(uint8_t bit_depth)
{
    switch (bit_depth) {
        case 16u:
            return 2u;
        case 20u:
        case 24u:
            return 3u;
        case 32u:
            return 4u;
        default:
            return 0u;
    }
}

static int16_t alac_sample_to_s16(const uint8_t *sample,
                                  uint8_t bit_depth)
{
    int16_t value16;
    int32_t value;

    if (bit_depth == 16u) {
        memcpy(&value16, sample, sizeof(value16));
        return value16;
    }

    if (bit_depth == 32u) {
        memcpy(&value, sample, sizeof(value));
        return (int16_t)(value >> 16);
    }

#if TARGET_RT_BIG_ENDIAN
    value = ((int32_t)sample[0] << 16) |
            ((int32_t)sample[1] << 8) |
            (int32_t)sample[2];
#else
    value = ((int32_t)sample[2] << 16) |
            ((int32_t)sample[1] << 8) |
            (int32_t)sample[0];
#endif
    if ((value & 0x00800000) != 0) {
        value |= (int32_t)0xff000000;
    }
    return (int16_t)(value >> 8);
}

audio_codec_dec_handle_t alac_pcm_decode_open(const alac_dec_config_t *config)
{
    alac_decoder_t *decoder;
    ALACSpecificConfig *specific;

    if (config == NULL ||
        config->magic_cookie_size < sizeof(ALACSpecificConfig) ||
        config->magic_cookie_size > ALAC_MAGIC_COOKIE_MAX_SIZE) {
        return NULL;
    }

    decoder = (alac_decoder_t *)avp_calloc(1u, sizeof(*decoder));
    if (decoder == NULL) {
        return NULL;
    }

    decoder->decoder = new ALACDecoder();
    if (decoder->decoder == NULL ||
        decoder->decoder->Init((void *)config->magic_cookie,
                               config->magic_cookie_size) != ALAC_noErr) {
        delete decoder->decoder;
        avp_free(decoder);
        return NULL;
    }

    specific = &decoder->decoder->mConfig;
    decoder->frame_length = specific->frameLength;
    decoder->bit_depth = specific->bitDepth;
    decoder->channels = specific->numChannels;
    decoder->sample_rate = specific->sampleRate;
    if (decoder->frame_length == 0u ||
        decoder->channels == 0u ||
        decoder->channels > kALACMaxChannels ||
        alac_bytes_per_sample(decoder->bit_depth) == 0u ||
        decoder->sample_rate == 0u) {
        delete decoder->decoder;
        avp_free(decoder);
        return NULL;
    }

    return (audio_codec_dec_handle_t)decoder;
}

void alac_pcm_decode_close(audio_codec_dec_handle_t handle)
{
    alac_decoder_t *decoder = (alac_decoder_t *)handle;

    if (decoder == NULL) {
        return;
    }
    delete decoder->decoder;
    if (decoder->decode_buffer != NULL) {
        avp_free(decoder->decode_buffer);
    }
    avp_free(decoder);
}

avp_status_t alac_pcm_decode_frame(audio_codec_dec_handle_t handle,
                                   audio_codec_dec_in_frame_t *in_frame,
                                   audio_codec_dec_out_frame_t *out_frame)
{
    alac_decoder_t *decoder = (alac_decoder_t *)handle;
    BitBuffer bits;
    uint32_t decoded_samples = 0u;
    uint32_t consumed_bits;
    uint32_t consumed_bytes;
    uint32_t bytes_per_sample;
    uint64_t required_size;
    uint8_t *decoded_buffer;
    uint32_t i;
    int32_t status;

    if (decoder == NULL || decoder->decoder == NULL ||
        in_frame == NULL || out_frame == NULL ||
        in_frame->buffer == NULL || out_frame->buffer == NULL ||
        in_frame->size == 0u) {
        return AVP_EINVAL;
    }

    in_frame->consumed_size = 0u;
    out_frame->require_size = 0u;
    out_frame->pcm_size = 0u;

    bytes_per_sample = alac_bytes_per_sample(decoder->bit_depth);
    required_size = (uint64_t)decoder->frame_length * decoder->channels *
                    sizeof(int16_t);

    out_frame->require_size = (uint32_t)required_size;
    if (out_frame->size < out_frame->require_size) {
        return AVP_EBUFFER;
    }

    if (decoder->bit_depth == 16u) {
        decoded_buffer = (uint8_t *)out_frame->buffer;
    } else {
        uint64_t decode_size = (uint64_t)decoder->frame_length *
                               decoder->channels *
                               bytes_per_sample;

        if (decoder->decode_buffer_size < (uint32_t)decode_size) {
            uint8_t *new_buffer = (uint8_t *)avp_realloc(decoder->decode_buffer,
                                                         (uint32_t)decode_size);
            if (new_buffer == NULL) {
                return AVP_ENOMEM;
            }
            decoder->decode_buffer = new_buffer;
            decoder->decode_buffer_size = (uint32_t)decode_size;
        }
        decoded_buffer = decoder->decode_buffer;
    }

    BitBufferInit(&bits, in_frame->buffer, in_frame->size);
    status = decoder->decoder->Decode(&bits,
                                      decoded_buffer,
                                      decoder->frame_length,
                                      decoder->channels,
                                      &decoded_samples);
    consumed_bits = BitBufferGetPosition(&bits);
    consumed_bytes = (consumed_bits + 7u) / 8u;

    if (status != ALAC_noErr ||
        decoded_samples == 0u ||
        decoded_samples > decoder->frame_length ||
        consumed_bytes == 0u ||
        consumed_bytes > in_frame->size) {
        return AVP_EBADFRAME;
    }

    if (decoder->bit_depth != 16u) {
        uint32_t sample_count = decoded_samples * decoder->channels;

        for (i = 0u; i < sample_count; i++) {
            out_frame->buffer[i] = alac_sample_to_s16(
                decoded_buffer + i * bytes_per_sample,
                decoder->bit_depth);
        }
    }

    out_frame->sample_rate = decoder->sample_rate;
    out_frame->bitrate = audio_codec_calc_bitrate_kbps(consumed_bytes,
                                                       decoder->sample_rate,
                                                       decoded_samples);
    out_frame->samples_per_channel = decoded_samples;
    out_frame->duration_ms = audio_codec_calc_duration_ms(decoded_samples,
                                                          decoder->sample_rate);
    out_frame->channels = decoder->channels;
    out_frame->bits_per_sample = 16u;
    out_frame->pcm_size = decoded_samples * decoder->channels *
                          (uint32_t)sizeof(int16_t);
    in_frame->consumed_size = consumed_bytes;
    return AVP_OK;
}
