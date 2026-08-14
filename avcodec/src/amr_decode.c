/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "amr_codec.h"

#include "amrnb/interf_dec.h"
#include "amrwb/dec_if.h"

typedef struct {
    amr_format_t format;
    void *state;
} amr_opencore_decoder_t;

#define AMR_INVALID_PAYLOAD_SIZE 0xffu

static const uint8_t amr_nb_payload_size_table[16] = {
    12u, 13u, 15u, 17u, 19u, 20u, 26u, 31u,
    5u, AMR_INVALID_PAYLOAD_SIZE, AMR_INVALID_PAYLOAD_SIZE, AMR_INVALID_PAYLOAD_SIZE,
    AMR_INVALID_PAYLOAD_SIZE, AMR_INVALID_PAYLOAD_SIZE, 0u, 0u
};

static const uint8_t amr_wb_payload_size_table[16] = {
    17u, 23u, 32u, 36u, 40u, 46u, 50u, 58u,
    60u, 5u, AMR_INVALID_PAYLOAD_SIZE, AMR_INVALID_PAYLOAD_SIZE,
    AMR_INVALID_PAYLOAD_SIZE, AMR_INVALID_PAYLOAD_SIZE, 0u, 0u
};

static uint8_t amr_payload_size(amr_format_t format, uint8_t frame_type)
{
    if (frame_type >= 16u) {
        return AMR_INVALID_PAYLOAD_SIZE;
    }

    if (format == AMR_FORMAT_NB) {
        return amr_nb_payload_size_table[frame_type];
    }

    if (format == AMR_FORMAT_WB) {
        return amr_wb_payload_size_table[frame_type];
    }

    return AMR_INVALID_PAYLOAD_SIZE;
}

avp_status_t amr_parse_frame_header(amr_format_t format,
                                    const uint8_t *buffer,
                                    uint32_t size,
                                    amr_frame_info_t *frame)
{
    uint8_t header;
    uint8_t frame_type;
    uint8_t payload_size;

    if (buffer == NULL || frame == NULL) {
        return AVP_EINVAL;
    }

    if (size < AMR_MIN_FRAME_HEADER_SIZE) {
        return AVP_ELACKFRAME;
    }

    header = buffer[0];
    frame_type = (uint8_t)((header >> 3) & 0x0fu);
    if ((header & 0x83u) != 0u) {
        return AVP_EBADHEADER;
    }

    payload_size = amr_payload_size(format, frame_type);
    if (payload_size == AMR_INVALID_PAYLOAD_SIZE) {
        return AVP_EBADHEADER;
    }

    memset(frame, 0, sizeof(*frame));
    frame->format = format;
    frame->toc.reserved_high = (header >> 7) & 0x01u;
    frame->toc.frame_type = frame_type;
    frame->toc.quality = (header >> 2) & 0x01u;
    frame->toc.reserved_low = header & 0x03u;
    frame->frame_size = (uint32_t)payload_size + 1u;
    frame->sample_rate = format == AMR_FORMAT_NB ? AMR_NB_SAMPLE_RATE : AMR_WB_SAMPLE_RATE;
    frame->samples_per_channel = format == AMR_FORMAT_NB ? AMR_NB_SAMPLES : AMR_WB_SAMPLES;
    frame->channels = 1u;

    return AVP_OK;
}

const char *amr_format_name(amr_format_t format)
{
    switch (format) {
        case AMR_FORMAT_NB:
            return "AMR-NB";
        case AMR_FORMAT_WB:
            return "AMR-WB";
        default:
            return "unknown";
    }
}

const char *amr_frame_type_name(amr_format_t format, uint8_t frame_type)
{
    static const char *const nb_names[16] = {
        "MR475", "MR515", "MR59", "MR67",
        "MR74", "MR795", "MR102", "MR122",
        "SID", "reserved", "reserved", "reserved",
        "reserved", "reserved", "speech_lost", "no_data"
    };
    static const char *const wb_names[16] = {
        "MD66", "MD885", "MD1265", "MD1425",
        "MD1585", "MD1825", "MD1985", "MD2305",
        "MD2385", "SID", "reserved", "reserved",
        "reserved", "reserved", "speech_lost", "no_data"
    };

    if (frame_type >= 16u) {
        return "unknown";
    }

    if (format == AMR_FORMAT_NB) {
        return nb_names[frame_type];
    }

    if (format == AMR_FORMAT_WB) {
        return wb_names[frame_type];
    }

    return "unknown";
}

audio_codec_dec_handle_t amr_pcm_decode_open(const amr_dec_config_t *config)
{
    amr_opencore_decoder_t *decoder;

    if (config == NULL ||
        (config->format != AMR_FORMAT_NB && config->format != AMR_FORMAT_WB)) {
        return NULL;
    }

    decoder = (amr_opencore_decoder_t *)avp_malloc(sizeof(*decoder));
    if (decoder == NULL) {
        return NULL;
    }

    memset(decoder, 0, sizeof(*decoder));
    decoder->format = config->format;

    if (decoder->format == AMR_FORMAT_NB) {
        decoder->state = Decoder_Interface_init();
    } else if (decoder->format == AMR_FORMAT_WB) {
        decoder->state = D_IF_init();
    } else {
        avp_free(decoder);
        return NULL;
    }

    if (decoder->state == NULL) {
        avp_free(decoder);
        return NULL;
    }

    return (audio_codec_dec_handle_t)decoder;
}

void amr_pcm_decode_close(audio_codec_dec_handle_t handle)
{
    amr_opencore_decoder_t *decoder = (amr_opencore_decoder_t *)handle;

    if (decoder == NULL) {
        return;
    }

    if (decoder->state != NULL) {
        if (decoder->format == AMR_FORMAT_NB) {
            Decoder_Interface_exit(decoder->state);
        } else if (decoder->format == AMR_FORMAT_WB) {
            D_IF_exit(decoder->state);
        }
    }
    avp_free(decoder);
}

avp_status_t amr_pcm_decode_frame(audio_codec_dec_handle_t handle,
                                  audio_codec_dec_in_frame_t *in_frame,
                                  audio_codec_dec_out_frame_t *out_frame)
{
    amr_opencore_decoder_t *decoder = (amr_opencore_decoder_t *)handle;
    int bfi;
    avp_status_t st;
    amr_frame_info_t frame;

    if (decoder == NULL || decoder->state == NULL ||
        in_frame == NULL || out_frame == NULL ||
        in_frame->buffer == NULL || out_frame->buffer == NULL) {
        return AVP_EINVAL;
    }

    out_frame->require_size = 0u;
    out_frame->pcm_size = 0u;
    in_frame->consumed_size = 0u;

    st = amr_parse_frame_header(decoder->format, in_frame->buffer, in_frame->size, &frame);
    if (st != AVP_OK) {
        return st;
    }

    if (frame.frame_size > in_frame->size) {
        return AVP_ELACKFRAME;
    }

    out_frame->require_size = frame.samples_per_channel * 1 * (uint32_t)sizeof(int16_t);

    if (out_frame->size < out_frame->require_size) {
        return AVP_EBUFFER;
    }

    bfi = frame.toc.quality == 0 ? 1 : 0;
    if (decoder->format == AMR_FORMAT_NB) {
        Decoder_Interface_Decode(decoder->state, in_frame->buffer, out_frame->buffer, bfi);
    } else {
        D_IF_decode(decoder->state, in_frame->buffer, out_frame->buffer, bfi);
    }

    out_frame->samples_per_channel = frame.samples_per_channel;
    out_frame->sample_rate = frame.sample_rate;
    out_frame->bitrate = audio_codec_calc_bitrate_kbps(frame.frame_size,
                                                       frame.sample_rate,
                                                       frame.samples_per_channel);
    out_frame->duration_ms = audio_codec_calc_duration_ms(frame.samples_per_channel,
                                                          frame.sample_rate);
    out_frame->channels = 1u;
    out_frame->bits_per_sample = 16u;
    out_frame->pcm_size = frame.samples_per_channel * (uint32_t)sizeof(int16_t);

    in_frame->consumed_size = frame.frame_size;

    return AVP_OK;
}
