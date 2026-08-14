/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "opus_codec.h"

#include <opus_multistream.h>

typedef struct {
    OpusDecoder *decoder;
    uint32_t sample_rate;
    uint8_t channels;
} opus_pcm_decoder_t;

audio_codec_dec_handle_t opus_pcm_decode_open(const opus_dec_config_t *config)
{
    opus_pcm_decoder_t *decoder;
    int error;

    if (config == NULL ||
        config->channels == 0u ||
        config->channels > 2u ||
        config->sample_rate == 0u) {
        return NULL;
    }

    decoder = (opus_pcm_decoder_t *)avp_malloc(sizeof(*decoder));
    if (decoder == NULL) {
        return NULL;
    }
    memset(decoder, 0, sizeof(*decoder));

    decoder->decoder = opus_decoder_create((opus_int32)config->sample_rate,
                                           config->channels,
                                           &error);
    if (decoder->decoder == NULL || error != OPUS_OK) {
        opus_pcm_decode_close((audio_codec_dec_handle_t)decoder);
        return NULL;
    }

    decoder->sample_rate = config->sample_rate;
    decoder->channels = config->channels;
    return (audio_codec_dec_handle_t)decoder;
}

void opus_pcm_decode_close(audio_codec_dec_handle_t handle)
{
    opus_pcm_decoder_t *decoder = (opus_pcm_decoder_t *)handle;

    if (decoder == NULL) {
        return;
    }

    if (decoder->decoder != NULL) {
        opus_decoder_destroy(decoder->decoder);
    }
    avp_free(decoder);
}

avp_status_t opus_pcm_decode_frame(audio_codec_dec_handle_t handle,
                                   audio_codec_dec_in_frame_t *in_frame,
                                   audio_codec_dec_out_frame_t *out_frame)
{
    opus_pcm_decoder_t *decoder = (opus_pcm_decoder_t *)handle;
    uint32_t channels;
    int samples;
    int packet_samples;
    uint32_t require_size;
    int decoded;

    if (decoder == NULL ||
        in_frame == NULL || out_frame == NULL ||
        in_frame->buffer == NULL || out_frame->buffer == NULL) {
        return AVP_EINVAL;
    }

    out_frame->require_size = 0u;
    out_frame->pcm_size = 0u;
    in_frame->consumed_size = 0u;

    channels = decoder->channels;
    packet_samples = opus_packet_get_nb_samples(in_frame->buffer,
                                                (opus_int32)in_frame->size,
                                                (opus_int32)decoder->sample_rate);
    if (packet_samples < 0) {
        return AVP_EBADHEADER;
    }

    require_size = (uint32_t)packet_samples * channels * (uint32_t)sizeof(int16_t);
    out_frame->require_size = require_size;
    if (out_frame->size < require_size) {
        return AVP_EBUFFER;
    }

    decoded = opus_decode(decoder->decoder,
                          in_frame->buffer,
                          (opus_int32)in_frame->size,
                          out_frame->buffer,
                          packet_samples,
                          0);
    if (decoded < 0) {
        return AVP_EBADFRAME;
    }

    samples = (uint32_t)decoded;

    out_frame->sample_rate = decoder->sample_rate;
    out_frame->bitrate = audio_codec_calc_bitrate_kbps(in_frame->size,
                                                       decoder->sample_rate,
                                                       (uint32_t)samples);
    out_frame->samples_per_channel = samples;
    out_frame->duration_ms = audio_codec_calc_duration_ms(samples,
                                                          decoder->sample_rate);
    out_frame->channels = (uint8_t)channels;
    out_frame->bits_per_sample = 16u;
    out_frame->pcm_size = samples * channels * (uint32_t)sizeof(int16_t);

    in_frame->consumed_size = in_frame->size;

    return AVP_OK;
}
