/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "g722_codec.h"

/*
 * The SB-ADPCM tables and state update flow follow the public-domain
 * SpanDSP/WebRTC G.722 decoder by Steve Underwood, based in part on the
 * CMU single-channel G.722 codec.
 */

typedef struct {
    int32_t s;
    int32_t sp;
    int32_t sz;
    int32_t r[3];
    int32_t a[3];
    int32_t ap[3];
    int32_t p[3];
    int32_t d[7];
    int32_t b[7];
    int32_t bp[7];
    int32_t sg[7];
    int32_t nb;
    int32_t det;
} g722_band_state_t;

typedef struct {
    uint32_t sample_rate;
    uint32_t bitrate;
    uint8_t bits_per_code;
    uint8_t packed;
    uint8_t eight_k;
    int32_t x[24];
    g722_band_state_t band[2];
    uint32_t in_buffer;
    uint8_t in_bits;
} g722_decoder_state_t;

typedef struct {
    g722_dec_config_t config;
    g722_decoder_state_t state;
} g722_pcm_decoder_t;

static const int32_t g722_wl[8] = {
    -60, -30, 58, 172, 334, 538, 1198, 3042
};

static const int32_t g722_rl42[16] = {
    0, 7, 6, 5, 4, 3, 2, 1,
    7, 6, 5, 4, 3, 2, 1, 0
};

static const int32_t g722_ilb[32] = {
    2048, 2093, 2139, 2186, 2233, 2282, 2332,
    2383, 2435, 2489, 2543, 2599, 2656, 2714,
    2774, 2834, 2896, 2960, 3025, 3091, 3158,
    3228, 3298, 3371, 3444, 3520, 3597, 3676,
    3756, 3838, 3922, 4008
};

static const int32_t g722_wh[3] = {
    0, -214, 798
};

static const int32_t g722_rh2[4] = {
    2, 1, 2, 1
};

static const int32_t g722_qm2[4] = {
    -7408, -1616, 7408, 1616
};

static const int32_t g722_qm4[16] = {
    0, -20456, -12896, -8968,
    -6288, -4240, -2584, -1200,
    20456, 12896, 8968, 6288,
    4240, 2584, 1200, 0
};

static const int32_t g722_qm5[32] = {
    -280, -280, -23352, -17560,
    -14120, -11664, -9752, -8184,
    -6864, -5712, -4696, -3784,
    -2960, -2208, -1520, -880,
    23352, 17560, 14120, 11664,
    9752, 8184, 6864, 5712,
    4696, 3784, 2960, 2208,
    1520, 880, 280, -280
};

static const int32_t g722_qm6[64] = {
    -136, -136, -136, -136,
    -24808, -21904, -19008, -16704,
    -14984, -13512, -12280, -11192,
    -10232, -9360, -8576, -7856,
    -7192, -6576, -6000, -5456,
    -4944, -4464, -4008, -3576,
    -3168, -2776, -2400, -2032,
    -1688, -1360, -1040, -728,
    24808, 21904, 19008, 16704,
    14984, 13512, 12280, 11192,
    10232, 9360, 8576, 7856,
    7192, 6576, 6000, 5456,
    4944, 4464, 4008, 3576,
    3168, 2776, 2400, 2032,
    1688, 1360, 1040, 728,
    432, 136, -432, -136
};

static const int32_t g722_qmf_coeffs[12] = {
    3, -11, 12, 32, -210, 951, 3876, -805, 362, -156, 53, -11
};

static int16_t g722_saturate_int16(int32_t sample)
{
    if (sample > 32767) {
        return 32767;
    }
    if (sample < -32768) {
        return -32768;
    }
    return (int16_t)sample;
}

static void g722_block4(g722_decoder_state_t *state, uint32_t band, int32_t d)
{
    g722_band_state_t *b = &state->band[band];
    int32_t wd1;
    int32_t wd2;
    int32_t wd3;
    int32_t i;

    b->d[0] = d;
    b->r[0] = g722_saturate_int16(b->s + d);
    b->p[0] = g722_saturate_int16(b->sz + d);

    for (i = 0; i < 3; i++) {
        b->sg[i] = b->p[i] >> 15;
    }
    wd1 = g722_saturate_int16(b->a[1] * 4);
    wd2 = b->sg[0] == b->sg[1] ? -wd1 : wd1;
    if (wd2 > 32767) {
        wd2 = 32767;
    }
    wd3 = b->sg[0] == b->sg[2] ? 128 : -128;
    wd3 += wd2 >> 7;
    wd3 += (b->a[2] * 32512) >> 15;
    if (wd3 > 12288) {
        wd3 = 12288;
    } else if (wd3 < -12288) {
        wd3 = -12288;
    }
    b->ap[2] = wd3;

    b->sg[0] = b->p[0] >> 15;
    b->sg[1] = b->p[1] >> 15;
    wd1 = b->sg[0] == b->sg[1] ? 192 : -192;
    wd2 = (b->a[1] * 32640) >> 15;
    b->ap[1] = g722_saturate_int16(wd1 + wd2);
    wd3 = g722_saturate_int16(15360 - b->ap[2]);
    if (b->ap[1] > wd3) {
        b->ap[1] = wd3;
    } else if (b->ap[1] < -wd3) {
        b->ap[1] = -wd3;
    }

    wd1 = d == 0 ? 0 : 128;
    b->sg[0] = d >> 15;
    for (i = 1; i < 7; i++) {
        b->sg[i] = b->d[i] >> 15;
        wd2 = b->sg[i] == b->sg[0] ? wd1 : -wd1;
        wd3 = (b->b[i] * 32640) >> 15;
        b->bp[i] = g722_saturate_int16(wd2 + wd3);
    }

    for (i = 6; i > 0; i--) {
        b->d[i] = b->d[i - 1];
        b->b[i] = b->bp[i];
    }
    for (i = 2; i > 0; i--) {
        b->r[i] = b->r[i - 1];
        b->p[i] = b->p[i - 1];
        b->a[i] = b->ap[i];
    }

    wd1 = g722_saturate_int16(b->r[1] + b->r[1]);
    wd1 = (b->a[1] * wd1) >> 15;
    wd2 = g722_saturate_int16(b->r[2] + b->r[2]);
    wd2 = (b->a[2] * wd2) >> 15;
    b->sp = g722_saturate_int16(wd1 + wd2);

    b->sz = 0;
    for (i = 6; i > 0; i--) {
        wd1 = g722_saturate_int16(b->d[i] + b->d[i]);
        b->sz += (b->b[i] * wd1) >> 15;
    }
    b->sz = g722_saturate_int16(b->sz);

    b->s = g722_saturate_int16(b->sp + b->sz);
}

static void g722_decode_init(g722_decoder_state_t *state,
                             uint32_t sample_rate,
                             uint32_t bitrate,
                             uint8_t packed)
{
    memset(state, 0, sizeof(*state));

    state->sample_rate = sample_rate;
    state->bitrate = bitrate;
    if (bitrate == 48000u) {
        state->bits_per_code = 6u;
    } else if (bitrate == 56000u) {
        state->bits_per_code = 7u;
    } else {
        state->bits_per_code = 8u;
        state->bitrate = 64000u;
    }
    state->eight_k = sample_rate == 8000u ? 1u : 0u;
    state->packed = packed != 0u && state->bits_per_code != 8u ? 1u : 0u;
    state->band[0].det = 32;
    state->band[1].det = 8;
}

static uint32_t g722_decode_sample_count(const g722_decoder_state_t *state,
                                         uint32_t input_size)
{
    uint32_t code_count;

    if (state->packed != 0u) {
        code_count = ((uint32_t)state->in_bits + input_size * 8u) /
                     (uint32_t)state->bits_per_code;
    } else {
        code_count = input_size;
    }

    return state->eight_k != 0u ? code_count : code_count * 2u;
}

static uint32_t g722_decode_samples(g722_decoder_state_t *state,
                                    int16_t *pcm,
                                    const uint8_t *input,
                                    uint32_t input_size)
{
    int32_t dlowt;
    int32_t rlow;
    int32_t ihigh;
    int32_t dhigh;
    int32_t rhigh = 0;
    int32_t xout1;
    int32_t xout2;
    int32_t wd1;
    int32_t wd2;
    int32_t wd3;
    int32_t code;
    uint32_t outlen = 0u;
    uint32_t i;
    uint32_t j;

    for (j = 0u; j < input_size;) {
        if (state->packed != 0u) {
            if (state->in_bits < state->bits_per_code) {
                state->in_buffer |= (uint32_t)input[j++] << state->in_bits;
                state->in_bits = (uint8_t)(state->in_bits + 8u);
            }
            if (state->in_bits < state->bits_per_code) {
                break;
            }
            code = (int32_t)(state->in_buffer & ((1u << state->bits_per_code) - 1u));
            state->in_buffer >>= state->bits_per_code;
            state->in_bits = (uint8_t)(state->in_bits - state->bits_per_code);
        } else {
            code = input[j++];
        }

        switch (state->bits_per_code) {
            default:
            case 8u:
                wd1 = code & 0x3f;
                ihigh = (code >> 6) & 0x03;
                wd2 = g722_qm6[wd1];
                wd1 >>= 2;
                break;
            case 7u:
                wd1 = code & 0x1f;
                ihigh = (code >> 5) & 0x03;
                wd2 = g722_qm5[wd1];
                wd1 >>= 1;
                break;
            case 6u:
                wd1 = code & 0x0f;
                ihigh = (code >> 4) & 0x03;
                wd2 = g722_qm4[wd1];
                break;
        }

        wd2 = (state->band[0].det * wd2) >> 15;
        rlow = state->band[0].s + wd2;
        if (rlow > 16383) {
            rlow = 16383;
        } else if (rlow < -16384) {
            rlow = -16384;
        }

        wd2 = g722_qm4[wd1];
        dlowt = (state->band[0].det * wd2) >> 15;

        wd2 = g722_rl42[wd1];
        wd1 = (state->band[0].nb * 127) >> 7;
        wd1 += g722_wl[wd2];
        if (wd1 < 0) {
            wd1 = 0;
        } else if (wd1 > 18432) {
            wd1 = 18432;
        }
        state->band[0].nb = wd1;

        wd1 = (state->band[0].nb >> 6) & 31;
        wd2 = 8 - (state->band[0].nb >> 11);
        wd3 = wd2 < 0 ? (g722_ilb[wd1] << -wd2) : (g722_ilb[wd1] >> wd2);
        state->band[0].det = wd3 << 2;

        g722_block4(state, 0u, dlowt);

        if (state->eight_k == 0u) {
            wd2 = g722_qm2[ihigh];
            dhigh = (state->band[1].det * wd2) >> 15;
            rhigh = dhigh + state->band[1].s;
            if (rhigh > 16383) {
                rhigh = 16383;
            } else if (rhigh < -16384) {
                rhigh = -16384;
            }

            wd2 = g722_rh2[ihigh];
            wd1 = (state->band[1].nb * 127) >> 7;
            wd1 += g722_wh[wd2];
            if (wd1 < 0) {
                wd1 = 0;
            } else if (wd1 > 22528) {
                wd1 = 22528;
            }
            state->band[1].nb = wd1;

            wd1 = (state->band[1].nb >> 6) & 31;
            wd2 = 10 - (state->band[1].nb >> 11);
            wd3 = wd2 < 0 ? (g722_ilb[wd1] << -wd2) : (g722_ilb[wd1] >> wd2);
            state->band[1].det = wd3 << 2;

            g722_block4(state, 1u, dhigh);
        }

        if (state->eight_k != 0u) {
            pcm[outlen++] = g722_saturate_int16(rlow * 2);
        } else {
            for (i = 0u; i < 22u; i++) {
                state->x[i] = state->x[i + 2u];
            }
            state->x[22] = rlow + rhigh;
            state->x[23] = rlow - rhigh;

            xout1 = 0;
            xout2 = 0;
            for (i = 0u; i < 12u; i++) {
                xout2 += state->x[2u * i] * g722_qmf_coeffs[i];
                xout1 += state->x[2u * i + 1u] * g722_qmf_coeffs[11u - i];
            }
            pcm[outlen++] = g722_saturate_int16(xout1 >> 11);
            pcm[outlen++] = g722_saturate_int16(xout2 >> 11);
        }
    }

    return outlen;
}

audio_codec_dec_handle_t g722_pcm_decode_open(const g722_dec_config_t *config)
{
    g722_pcm_decoder_t *decoder;
    uint32_t bitrate;

    if (config == NULL ||
        config->channels != 1u ||
        (config->sample_rate != 8000u && config->sample_rate != 16000u)) {
        return NULL;
    }

    bitrate = config->bitrate == 0u ? 64000u : config->bitrate;
    if (bitrate != 48000u && bitrate != 56000u && bitrate != 64000u) {
        return NULL;
    }

    decoder = (g722_pcm_decoder_t *)avp_malloc(sizeof(*decoder));
    if (decoder == NULL) {
        return NULL;
    }

    memset(decoder, 0, sizeof(*decoder));
    decoder->config = *config;
    decoder->config.bitrate = bitrate;
    g722_decode_init(&decoder->state,
                     config->sample_rate,
                     bitrate,
                     config->packed);

    return (audio_codec_dec_handle_t)decoder;
}

void g722_pcm_decode_close(audio_codec_dec_handle_t handle)
{
    avp_free(handle);
}

avp_status_t g722_pcm_decode_frame(audio_codec_dec_handle_t handle,
                                   audio_codec_dec_in_frame_t *in_frame,
                                   audio_codec_dec_out_frame_t *out_frame)
{
    g722_pcm_decoder_t *decoder = (g722_pcm_decoder_t *)handle;
    uint32_t samples;
    uint32_t require_size;

    if (decoder == NULL ||
        in_frame == NULL || out_frame == NULL ||
        in_frame->buffer == NULL || out_frame->buffer == NULL) {
        return AVP_EINVAL;
    }

    in_frame->consumed_size = 0u;
    out_frame->require_size = 0u;
    out_frame->pcm_size = 0u;

    samples = g722_decode_sample_count(&decoder->state, in_frame->size);
    require_size = samples * (uint32_t)sizeof(int16_t);
    if (out_frame->size < require_size) {
        out_frame->require_size = require_size;
        return AVP_EBUFFER;
    }

    samples = g722_decode_samples(&decoder->state,
                                  out_frame->buffer,
                                  in_frame->buffer,
                                  in_frame->size);

    out_frame->sample_rate = decoder->state.sample_rate;
    out_frame->bitrate = decoder->state.bitrate / 1000u;
    out_frame->samples_per_channel = samples;
    out_frame->duration_ms = audio_codec_calc_duration_ms(samples,
                                                          decoder->state.sample_rate);
    out_frame->channels = 1u;
    out_frame->bits_per_sample = 16u;
    out_frame->pcm_size = samples * (uint32_t)sizeof(int16_t);
    in_frame->consumed_size = in_frame->size;

    return AVP_OK;
}
