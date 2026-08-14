/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "adpcm_codec.h"

static const int8_t adpcm_ima_index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

static const int16_t adpcm_ima_step_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14,
    16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
    7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767
};

typedef struct {
    int32_t predictor;
    int32_t step_index;
} adpcm_ima_channel_state_t;

typedef struct {
    adpcm_dec_config_t config;
    uint32_t samples_per_block;
} adpcm_pcm_decoder_t;

static int16_t adpcm_ima_clip_int16(int32_t sample)
{
    if (sample > 32767) {
        return 32767;
    }

    if (sample < -32768) {
        return -32768;
    }

    return (int16_t)sample;
}

static int16_t adpcm_ima_decode_nibble(adpcm_ima_channel_state_t *state,
                                       uint8_t nibble)
{
    int32_t step;
    int32_t diff;

    step = adpcm_ima_step_table[state->step_index];
    diff = step >> 3;

    if ((nibble & 1u) != 0u) {
        diff += step >> 2;
    }
    if ((nibble & 2u) != 0u) {
        diff += step >> 1;
    }
    if ((nibble & 4u) != 0u) {
        diff += step;
    }

    if ((nibble & 8u) != 0u) {
        state->predictor -= diff;
    } else {
        state->predictor += diff;
    }

    if (state->predictor > 32767) {
        state->predictor = 32767;
    } else if (state->predictor < -32768) {
        state->predictor = -32768;
    }

    state->step_index += adpcm_ima_index_table[nibble & 0x0fu];
    if (state->step_index < 0) {
        state->step_index = 0;
    } else if (state->step_index > 88) {
        state->step_index = 88;
    }

    return (int16_t)state->predictor;
}

static uint32_t adpcm_ima_samples_per_block(uint16_t block_align,
                                            uint16_t channels)
{
    uint32_t header_size;

    if (channels == 0u) {
        return 0u;
    }

    header_size = (uint32_t)channels * 4u;
    if (block_align < header_size) {
        return 0u;
    }

    return (((uint32_t)block_align - header_size) * 2u) /
               (uint32_t)channels +
           1u;
}

audio_codec_dec_handle_t adpcm_pcm_decode_open(const adpcm_dec_config_t *config)
{
    adpcm_pcm_decoder_t *decoder;
    uint32_t samples_per_block;

    if (config == NULL ||
        config->channels == 0u ||
        config->channels > ADPCM_IMA_MAX_CHANNELS ||
        config->block_align == 0u ||
        config->sample_rate == 0u) {
        return NULL;
    }

    samples_per_block = adpcm_ima_samples_per_block(config->block_align,
                                                    config->channels);
    if (samples_per_block == 0u) {
        return NULL;
    }

    decoder = (adpcm_pcm_decoder_t *)avp_malloc(sizeof(*decoder));
    if (decoder == NULL) {
        return NULL;
    }

    decoder->config = *config;
    decoder->samples_per_block = samples_per_block;
    return (audio_codec_dec_handle_t)decoder;
}

void adpcm_pcm_decode_close(audio_codec_dec_handle_t handle)
{
    avp_free(handle);
}

avp_status_t adpcm_pcm_decode_frame(audio_codec_dec_handle_t handle,
                                    audio_codec_dec_in_frame_t *in_frame,
                                    audio_codec_dec_out_frame_t *out_frame)
{
    adpcm_pcm_decoder_t *decoder = (adpcm_pcm_decoder_t *)handle;
    adpcm_ima_channel_state_t state[ADPCM_IMA_MAX_CHANNELS];
    uint32_t channel_samples[ADPCM_IMA_MAX_CHANNELS];
    const adpcm_dec_config_t *config;
    uint32_t samples_per_block;
    uint32_t header_size;
    uint32_t offset;
    uint32_t ch;

    if (decoder == NULL ||
        in_frame == NULL || out_frame == NULL ||
        in_frame->buffer == NULL || out_frame->buffer == NULL) {
        return AVP_EINVAL;
    }

    config = &decoder->config;
    samples_per_block = decoder->samples_per_block;

    out_frame->require_size = 0u;
    out_frame->pcm_size = 0u;
    in_frame->consumed_size = 0u;

    header_size = config->channels * 4u;

    if (config->block_align > in_frame->size || header_size > in_frame->size) {
        return AVP_ELACKFRAME;
    }

    out_frame->require_size = samples_per_block * config->channels * (uint32_t)sizeof(int16_t);
    if (out_frame->size < out_frame->require_size) {
        return AVP_EBUFFER;
    }

    for (ch = 0u; ch < config->channels; ch++) {
        const uint8_t *channel_header = in_frame->buffer + ch * 4u;

        state[ch].predictor = (int16_t)AVP_GET_LE16(channel_header);
        state[ch].step_index = channel_header[2];
        if (state[ch].step_index > 88) {
            return AVP_EBADFRAME;
        }

        out_frame->buffer[ch] = adpcm_ima_clip_int16(state[ch].predictor);
        channel_samples[ch] = 1u;
    }

    offset = header_size;

    while (offset < config->block_align) {
        uint32_t done_channels = 0u;

        for (ch = 0u; ch < config->channels; ch++) {
            if (channel_samples[ch] >= samples_per_block) {
                done_channels++;
            }
        }
        if (done_channels == config->channels) {
            break;
        }

        for (ch = 0u; ch < config->channels && offset < config->block_align; ch++) {
            uint32_t byte_count;

            for (byte_count = 0u;
                 byte_count < 4u && offset < config->block_align && channel_samples[ch] < samples_per_block;
                 byte_count++) {
                uint8_t code = in_frame->buffer[offset++];
                uint32_t sample_index = channel_samples[ch];

                out_frame->buffer[(sample_index * config->channels) + ch] =
                    adpcm_ima_decode_nibble(&state[ch], code & 0x0fu);
                sample_index++;
                if (sample_index < samples_per_block) {
                    out_frame->buffer[(sample_index * config->channels) + ch] =
                        adpcm_ima_decode_nibble(&state[ch], code >> 4);
                    sample_index++;
                }
                channel_samples[ch] = sample_index;
            }
        }
    }

    for (ch = 0u; ch < config->channels; ch++) {
        if (channel_samples[ch] != samples_per_block) {
            return AVP_EBADFRAME;
        }
    }

    out_frame->sample_rate = config->sample_rate;
    out_frame->bitrate = audio_codec_calc_bitrate_kbps(config->block_align,
                                                       config->sample_rate,
                                                       samples_per_block);
    out_frame->samples_per_channel = samples_per_block;
    out_frame->duration_ms = audio_codec_calc_duration_ms(samples_per_block,
                                                          config->sample_rate);
    out_frame->channels = (uint8_t)config->channels;
    out_frame->bits_per_sample = 16u;
    out_frame->pcm_size = samples_per_block * config->channels * (uint32_t)sizeof(int16_t);

    in_frame->consumed_size = config->block_align;

    return AVP_OK;
}
