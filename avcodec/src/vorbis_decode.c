/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "vorbis_codec.h"

#include <vorbis/codec.h>

typedef struct {
    vorbis_info info;
    vorbis_comment comment;
    vorbis_dsp_state dsp;
    vorbis_block block;
    uint32_t packetno;
    uint8_t info_initialized;
    uint8_t comment_initialized;
    uint8_t dsp_initialized;
    uint8_t block_initialized;
} vorbis_pcm_decoder_t;

static avp_status_t vorbis_status_from_error(int error)
{
    if (error == 0) {
        return AVP_OK;
    }

    if (error == OV_ENOTAUDIO || error == OV_EBADPACKET) {
        return AVP_EBADFRAME;
    }

    if (error == OV_EBADHEADER || error == OV_ENOTVORBIS || error == OV_EVERSION) {
        return AVP_EBADHEADER;
    }

    return AVP_EUNSUPPORTED;
}

static void vorbis_fill_packet(ogg_packet *packet,
                               const uint8_t *buffer,
                               uint32_t size,
                               uint32_t packetno)
{
    memset(packet, 0, sizeof(*packet));
    packet->packet = (unsigned char *)buffer;
    packet->bytes = (int)size;
    packet->b_o_s = packetno == 0u ? 1L : 0L;
    packet->packetno = (ogg_int64_t)packetno;
}

audio_codec_dec_handle_t vorbis_pcm_decode_open(const vorbis_dec_config_t *config)
{
    vorbis_pcm_decoder_t *decoder;
    ogg_packet packet;
    int ret;

    if (config == NULL ||
        config->identification == NULL ||
        config->comment == NULL ||
        config->setup == NULL) {
        return NULL;
    }

    decoder = (vorbis_pcm_decoder_t *)avp_malloc(sizeof(*decoder));
    if (decoder == NULL) {
        return NULL;
    }
    memset(decoder, 0, sizeof(*decoder));

    vorbis_info_init(&decoder->info);
    decoder->info_initialized = 1u;
    vorbis_comment_init(&decoder->comment);
    decoder->comment_initialized = 1u;

    vorbis_fill_packet(&packet, config->identification, config->identification_size, 0u);
    ret = vorbis_synthesis_headerin(&decoder->info, &decoder->comment, &packet);
    if (ret == 0) {
        vorbis_fill_packet(&packet, config->comment, config->comment_size, 1u);
        ret = vorbis_synthesis_headerin(&decoder->info, &decoder->comment, &packet);
    }
    if (ret == 0) {
        vorbis_fill_packet(&packet, config->setup, config->setup_size, 2u);
        ret = vorbis_synthesis_headerin(&decoder->info, &decoder->comment, &packet);
    }
    if (ret != 0 ||
        vorbis_synthesis_init(&decoder->dsp, &decoder->info) != 0) {
        vorbis_pcm_decode_close((audio_codec_dec_handle_t)decoder);
        return NULL;
    }
    decoder->dsp_initialized = 1u;

    if (vorbis_block_init(&decoder->dsp, &decoder->block) != 0) {
        vorbis_pcm_decode_close((audio_codec_dec_handle_t)decoder);
        return NULL;
    }
    decoder->block_initialized = 1u;
    decoder->packetno = 3u;
    return (audio_codec_dec_handle_t)decoder;
}

void vorbis_pcm_decode_close(audio_codec_dec_handle_t handle)
{
    vorbis_pcm_decoder_t *decoder = (vorbis_pcm_decoder_t *)handle;

    if (decoder == NULL) {
        return;
    }

    if (decoder->block_initialized) {
        vorbis_block_clear(&decoder->block);
    }
    if (decoder->dsp_initialized) {
        vorbis_dsp_clear(&decoder->dsp);
    }
    if (decoder->comment_initialized) {
        vorbis_comment_clear(&decoder->comment);
    }
    if (decoder->info_initialized) {
        vorbis_info_clear(&decoder->info);
    }
    avp_free(decoder);
}

avp_status_t vorbis_pcm_decode_frame(audio_codec_dec_handle_t handle,
                                     audio_codec_dec_in_frame_t *in_frame,
                                     audio_codec_dec_out_frame_t *out_frame)
{
    vorbis_pcm_decoder_t *decoder = (vorbis_pcm_decoder_t *)handle;
    ogg_packet packet;
    float **pcm;
    int samples;
    int ret;
    uint32_t channels;
    uint32_t require_size;
    uint32_t i;
    uint32_t ch;

    if (decoder == NULL ||
        in_frame == NULL || out_frame == NULL ||
        in_frame->buffer == NULL || out_frame->buffer == NULL) {
        return AVP_EINVAL;
    }

    out_frame->require_size = 0u;
    out_frame->pcm_size = 0u;
    in_frame->consumed_size = 0u;

    vorbis_fill_packet(&packet, in_frame->buffer, in_frame->size, decoder->packetno++);
    ret = vorbis_synthesis(&decoder->block, &packet);
    if (ret != 0) {
        return vorbis_status_from_error(ret);
    }

    ret = vorbis_synthesis_blockin(&decoder->dsp, &decoder->block);
    if (ret != 0) {
        return vorbis_status_from_error(ret);
    }

    samples = vorbis_synthesis_pcmout(&decoder->dsp, &pcm);
    if (samples < 0) {
        return AVP_EBADFRAME;
    }

    channels = (uint32_t)decoder->info.channels;
    require_size = (uint32_t)samples * channels * (uint32_t)sizeof(int16_t);
    out_frame->require_size = require_size;
    if (out_frame->size < require_size) {
        return AVP_EBUFFER;
    }

    for (i = 0u; i < (uint32_t)samples; i++) {
        for (ch = 0u; ch < channels; ch++) {
            float value = pcm[ch][i] * 32767.0f;
            int sample;

            if (value > 32767.0f) {
                sample = 32767;
            } else if (value < -32768.0f) {
                sample = -32768;
            } else {
                sample = (int)value;
            }
            out_frame->buffer[i * channels + ch] = (int16_t)sample;
        }
    }

    vorbis_synthesis_read(&decoder->dsp, samples);

    out_frame->pcm_size = require_size;
    out_frame->sample_rate = (uint32_t)decoder->info.rate;
    out_frame->bitrate = decoder->info.bitrate_nominal > 0 ?
                             (uint32_t)decoder->info.bitrate_nominal / 1000u :
                             audio_codec_calc_bitrate_kbps(in_frame->size,
                                                           (uint32_t)decoder->info.rate,
                                                           (uint32_t)samples);
    out_frame->samples_per_channel = (uint32_t)samples;
    out_frame->duration_ms = audio_codec_calc_duration_ms((uint32_t)samples,
                                                          (uint32_t)decoder->info.rate);
    out_frame->channels = (uint8_t)channels;
    out_frame->bits_per_sample = 16u;

    in_frame->consumed_size = in_frame->size;

    return AVP_OK;
}
