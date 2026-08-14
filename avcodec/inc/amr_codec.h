/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AMR_CODEC_H
#define AMR_CODEC_H

#include "audio_codec_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AMR_NB_MAGIC          "#!AMR\n"
#define AMR_WB_MAGIC          "#!AMR-WB\n"
#define AMR_NB_MAGIC_SIZE     6u
#define AMR_WB_MAGIC_SIZE     9u
#define AMR_FRAME_DURATION_MS 20u
#define AMR_NB_SAMPLE_RATE    8000u
#define AMR_WB_SAMPLE_RATE    16000u
#define AMR_NB_SAMPLES        160u
#define AMR_WB_SAMPLES        320u
#define AMR_MAX_FRAME_SIZE    61u

#define AMR_MIN_FRAME_HEADER_SIZE 1

typedef enum {
    AMR_FORMAT_UNKNOWN = 0,
    AMR_FORMAT_NB,
    AMR_FORMAT_WB
} amr_format_t;

typedef struct {
    unsigned int reserved_high : 1;
    unsigned int frame_type    : 4;
    unsigned int quality       : 1;
    unsigned int reserved_low  : 2;
} amr_toc_header_t;

typedef struct {
    amr_format_t format;
    amr_toc_header_t toc;

    uint32_t frame_size;
    uint32_t sample_rate;
    uint32_t samples_per_channel;
    uint8_t channels;
} amr_frame_info_t;

typedef struct {
    amr_format_t format;
} amr_dec_config_t;

/**
 * \brief Parse one frame header from the input buffer.
 * \param format Parameter format.
 * \param buffer Parameter buffer.
 * \param size Parameter size.
 * \param frame Parameter frame.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t amr_parse_frame_header(amr_format_t format,
                                    const uint8_t *buffer,
                                    uint32_t size,
                                    amr_frame_info_t *frame);
/**
 * \brief Get a human-readable name string.
 * \param format Parameter format.
 * \return Pointer to a null-terminated string.
 */
const char *amr_format_name(amr_format_t format);
/**
 * \brief Get a human-readable name string.
 * \param format Parameter format.
 * \param frame_type Parameter frame_type.
 * \return Pointer to a null-terminated string.
 */
const char *amr_frame_type_name(amr_format_t format, uint8_t frame_type);

/**
 * \brief Open and initialize the context.
 * \param config Parameter config.
 * \return Decoder handle value.
 */
audio_codec_dec_handle_t amr_pcm_decode_open(const amr_dec_config_t *config);
/**
 * \brief Close the context and release resources.
 * \param handle Parameter handle.
 */
void amr_pcm_decode_close(audio_codec_dec_handle_t handle);
/**
 * \brief Decode one input frame into PCM output.
 * \param handle Parameter handle.
 * \param in_frame Parameter in_frame.
 * \param out_frame Parameter out_frame.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t amr_pcm_decode_frame(audio_codec_dec_handle_t handle,
                                  audio_codec_dec_in_frame_t *in_frame,
                                  audio_codec_dec_out_frame_t *out_frame);

#ifdef __cplusplus
}
#endif

#endif
