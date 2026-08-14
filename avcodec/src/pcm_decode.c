/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "pcm_codec.h"

typedef struct {
    pcm_dec_config_t config;
} pcm_decoder_t;

audio_codec_dec_handle_t pcm_decode_open(const pcm_dec_config_t *config)
{
    pcm_decoder_t *decoder;

    if (config == NULL || config->channels == 0u || config->sample_rate == 0u ||
        (config->bits_per_sample != 8u && config->bits_per_sample != 16u &&
         config->bits_per_sample != 24u && config->bits_per_sample != 32u)) {
        return NULL;
    }
    decoder = (pcm_decoder_t *)avp_malloc(sizeof(*decoder));
    if (decoder != NULL) {
        decoder->config = *config;
    }
    return (audio_codec_dec_handle_t)decoder;
}

void pcm_decode_close(audio_codec_dec_handle_t handle)
{
    avp_free(handle);
}

static int32_t pcm_read_sample(const uint8_t *sample, uint16_t bits)
{
    int32_t value;

    if (bits == 8u) {
        return ((int32_t)sample[0] - 128) << 8;
    }
    if (bits == 16u) {
        value = (int32_t)((uint32_t)sample[0] | ((uint32_t)sample[1] << 8));
        return (int16_t)value;
    }
    if (bits == 24u) {
        value = (int32_t)((uint32_t)sample[0] | ((uint32_t)sample[1] << 8) |
                          ((uint32_t)sample[2] << 16));
        if ((value & 0x00800000) != 0) {
            value |= (int32_t)0xff000000;
        }
        return value >> 8;
    }
    value = (int32_t)((uint32_t)sample[0] | ((uint32_t)sample[1] << 8) |
                      ((uint32_t)sample[2] << 16) | ((uint32_t)sample[3] << 24));
    return value >> 16;
}

avp_status_t pcm_decode_frame(audio_codec_dec_handle_t handle,
                              audio_codec_dec_in_frame_t *in_frame,
                              audio_codec_dec_out_frame_t *out_frame)
{
    pcm_decoder_t *decoder = (pcm_decoder_t *)handle;
    uint32_t bytes_per_sample;
    uint32_t block_size;
    uint32_t samples;
    uint32_t i;

    if (decoder == NULL || in_frame == NULL || out_frame == NULL ||
        in_frame->buffer == NULL || in_frame->size == 0u || out_frame->buffer == NULL) {
        return AVP_EINVAL;
    }
    bytes_per_sample = decoder->config.bits_per_sample / 8u;
    block_size = bytes_per_sample * decoder->config.channels;
    samples = in_frame->size / block_size;

    out_frame->require_size = samples * decoder->config.channels * sizeof(int16_t);
    out_frame->pcm_size = out_frame->require_size;
    in_frame->consumed_size = samples * block_size;
    if (out_frame->size < out_frame->pcm_size) {
        return AVP_EBUFFER;
    }
    for (i = 0u; i < samples * decoder->config.channels; i++) {
        int32_t value = pcm_read_sample(in_frame->buffer + i * bytes_per_sample,
                                        decoder->config.bits_per_sample);
        if (value > INT16_MAX) {
            value = INT16_MAX;
        } else if (value < INT16_MIN) {
            value = INT16_MIN;
        }
        out_frame->buffer[i] = (int16_t)value;
    }
    out_frame->sample_rate = decoder->config.sample_rate;
    out_frame->channels = (uint8_t)decoder->config.channels;
    out_frame->bits_per_sample = 16u;
    out_frame->samples_per_channel = samples;
    out_frame->duration_ms = audio_codec_calc_duration_ms(samples, decoder->config.sample_rate);
    out_frame->bitrate = audio_codec_calc_bitrate_kbps(in_frame->consumed_size,
                                                       decoder->config.sample_rate,
                                                       samples);
    return AVP_OK;
}
