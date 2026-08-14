/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "g711_codec.h"

#define ULAW_BIAS 0x84

typedef struct {
    g711_dec_config_t config;
} g711_pcm_decoder_t;

/*
 * A-law is basically as follows:
 *
 *      Linear Input Code        Compressed Code
 *      -----------------        ---------------
 *      0000000wxyza             000wxyz
 *      0000001wxyza             001wxyz
 *      000001wxyzab             010wxyz
 *      00001wxyzabc             011wxyz
 *      0001wxyzabcd             100wxyz
 *      001wxyzabcde             101wxyz
 *      01wxyzabcdef             110wxyz
 *      1wxyzabcdefg             111wxyz
 *
 * For further information see John C. Bellamy's Digital Telephony, 1982,
 * John Wiley & Sons, pps 98-111 and 472-476.
 */

int16_t g711_alaw_to_linear(uint8_t alaw)
{
    int16_t pcm;
    uint8_t segment;

    alaw ^= 0x55u;
    pcm = (int16_t)((alaw & 0x0fu) << 4);
    segment = (uint8_t)((alaw & 0x70u) >> 4);

    if (segment == 0u) {
        pcm = (int16_t)(pcm + 8);
    } else {
        pcm = (int16_t)(pcm + 0x108);
        if (segment > 1u) {
            pcm = (int16_t)(pcm << (segment - 1u));
        }
    }

    return (alaw & 0x80u) != 0u ? pcm : (int16_t)-pcm;
}

/*
 * Mu-law is basically as follows:
 *
 *      Biased Linear Input Code        Compressed Code
 *      ------------------------        ---------------
 *      00000001wxyza                   000wxyz
 *      0000001wxyzab                   001wxyz
 *      000001wxyzabc                   010wxyz
 *      00001wxyzabcd                   011wxyz
 *      0001wxyzabcde                   100wxyz
 *      001wxyzabcdef                   101wxyz
 *      01wxyzabcdefg                   110wxyz
 *      1wxyzabcdefgh                   111wxyz
 *
 * Each biased linear code has a leading 1 which identifies the segment
 * number. The value of the segment number is equal to 7 minus the number
 * of leading 0's. The quantization interval is directly available as the
 * four bits wxyz.  * The trailing bits (a - h) are ignored.
 *
 * Ordinarily the complement of the resulting code word is used for
 * transmission, and so the code word is complemented before it is returned.
 *
 * For further information see John C. Bellamy's Digital Telephony, 1982,
 * John Wiley & Sons, pps 98-111 and 472-476.
 */

int16_t g711_mulaw_to_linear(uint8_t mulaw)
{
    int t;

    /* Complement to obtain normal u-law value. */
    mulaw = ~mulaw;
    /*
   * Extract and bias the quantization bits. Then
   * shift up by the segment number and subtract out the bias.
   */
    t = (((mulaw & 0x0F) << 3) + ULAW_BIAS) << (((int)mulaw & 0x70) >> 4);
    return (int16_t)((mulaw & 0x80) ? (ULAW_BIAS - t) : (t - ULAW_BIAS));
}

audio_codec_dec_handle_t g711_pcm_decode_open(const g711_dec_config_t *config)
{
    g711_pcm_decoder_t *decoder;

    if (config == NULL ||
        config->sample_rate == 0u ||
        config->channels == 0u ||
        (config->format != G711_FORMAT_ALAW && config->format != G711_FORMAT_MULAW)) {
        return NULL;
    }

    decoder = (g711_pcm_decoder_t *)avp_malloc(sizeof(*decoder));
    if (decoder == NULL) {
        return NULL;
    }

    memset(decoder, 0, sizeof(*decoder));
    decoder->config = *config;
    return (audio_codec_dec_handle_t)decoder;
}

void g711_pcm_decode_close(audio_codec_dec_handle_t handle)
{
    avp_free(handle);
}

avp_status_t g711_pcm_decode_frame(audio_codec_dec_handle_t handle,
                                   audio_codec_dec_in_frame_t *in_frame,
                                   audio_codec_dec_out_frame_t *out_frame)
{
    g711_pcm_decoder_t *decoder = (g711_pcm_decoder_t *)handle;
    uint32_t samples;
    uint32_t require_size;
    uint32_t i;

    if (decoder == NULL ||
        in_frame == NULL || out_frame == NULL ||
        in_frame->buffer == NULL || out_frame->buffer == NULL) {
        return AVP_EINVAL;
    }

    in_frame->consumed_size = 0u;
    out_frame->require_size = 0u;
    out_frame->pcm_size = 0u;

    samples = in_frame->size;
    require_size = samples * (uint32_t)sizeof(int16_t);
    if (out_frame->size < require_size) {
        out_frame->require_size = require_size;
        return AVP_EBUFFER;
    }

    if (decoder->config.format == G711_FORMAT_ALAW) {
        for (i = 0u; i < samples; i++) {
            out_frame->buffer[i] = g711_alaw_to_linear(in_frame->buffer[i]);
        }
    } else if (decoder->config.format == G711_FORMAT_MULAW) {
        for (i = 0u; i < samples; i++) {
            out_frame->buffer[i] = g711_mulaw_to_linear(in_frame->buffer[i]);
        }
    } else {
        return AVP_EINVAL;
    }

    out_frame->sample_rate = decoder->config.sample_rate;
    out_frame->bitrate = audio_codec_calc_bitrate_kbps((uint32_t)decoder->config.channels,
                                                       decoder->config.sample_rate,
                                                       1u);
    out_frame->samples_per_channel = samples / (uint32_t)decoder->config.channels;
    out_frame->duration_ms = audio_codec_calc_duration_ms(out_frame->samples_per_channel,
                                                          decoder->config.sample_rate);
    out_frame->channels = decoder->config.channels;
    out_frame->bits_per_sample = 16u;
    out_frame->pcm_size = require_size;
    in_frame->consumed_size = in_frame->size;

    return AVP_OK;
}
