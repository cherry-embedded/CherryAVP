/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AAC_CODEC_H
#define AAC_CODEC_H

#include "audio_codec_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AAC_ADTS_HEADER_SIZE 7u
#define AAC_ADTS_CRC_SIZE    2u
/*
 * ADTS aac_frame_size is a 13-bit field and stores the whole ADTS frame
 * size, including the ADTS header and AAC raw data block: (1 << 13) - 1.
 */
#define AAC_MAX_FRAME_SIZE 8191u
#define AAC_MAX_SAMPLES    1024u
#define AAC_MAX_CHANNELS   2u

#define AAC_MIN_FRAME_HEADER_SIZE AAC_ADTS_HEADER_SIZE

/* ISO/IEC 14496-3 Audio Object Type values. */
typedef enum {
    AAC_AUDIO_OBJECT_TYPE_NULL = 0,
    AAC_AUDIO_OBJECT_TYPE_AAC_MAIN = 1,
    AAC_AUDIO_OBJECT_TYPE_AAC_LC = 2,
    AAC_AUDIO_OBJECT_TYPE_AAC_SSR = 3,
    AAC_AUDIO_OBJECT_TYPE_LTP = 4,
    AAC_AUDIO_OBJECT_TYPE_SBR = 5,
    AAC_AUDIO_OBJECT_TYPE_AAC_SCALABLE = 6,
    AAC_AUDIO_OBJECT_TYPE_TWINVQ = 7,
    AAC_AUDIO_OBJECT_TYPE_CELP = 8,
    AAC_AUDIO_OBJECT_TYPE_HVXC = 9,
    AAC_AUDIO_OBJECT_TYPE_TTSI = 12,
    AAC_AUDIO_OBJECT_TYPE_ER_AAC_LC = 17,
    AAC_AUDIO_OBJECT_TYPE_ER_AAC_LTP = 19,
    AAC_AUDIO_OBJECT_TYPE_ER_AAC_SCALABLE = 20,
    AAC_AUDIO_OBJECT_TYPE_ER_TWINVQ = 21,
    AAC_AUDIO_OBJECT_TYPE_ER_BSAC = 22,
    AAC_AUDIO_OBJECT_TYPE_ER_AAC_LD = 23,
    AAC_AUDIO_OBJECT_TYPE_ER_CELP = 24,
    AAC_AUDIO_OBJECT_TYPE_ER_HVXC = 25,
    AAC_AUDIO_OBJECT_TYPE_ER_HILN = 26,
    AAC_AUDIO_OBJECT_TYPE_PARAMETRIC = 27,
    AAC_AUDIO_OBJECT_TYPE_PS = 29
} aac_audio_object_type_t;

typedef struct {
    unsigned int syncword                       : 12;
    unsigned int id                             : 1;
    unsigned int layer                          : 2;
    unsigned int protection_absent              : 1;
    unsigned int profile                        : 2;
    unsigned int sampling_frequency_index       : 4;
    unsigned int private_bit                    : 1;
    unsigned int channel_configuration          : 3;
    unsigned int original_copy                  : 1;
    unsigned int home                           : 1;
    unsigned int copyright_identification_bit   : 1;
    unsigned int copyright_identification_start : 1;
    unsigned int aac_frame_length               : 13;
    unsigned int adts_buffer_fullness           : 11;
    unsigned int number_of_raw_data_blocks      : 2;
    unsigned int crc_check                      : 16;
} aac_adts_header_t;

typedef struct {
    aac_adts_header_t adts;
    uint16_t header_length;

    uint32_t frame_size;
    uint32_t sample_rate;
    uint32_t samples_per_channel;
    aac_audio_object_type_t audio_object_type;
    uint8_t channels;
} aac_frame_info_t;

typedef struct {
    bool has_no_adts_header; /* true if the input stream has no adts header */
    bool aac_plus_enabled;   /* true if the input stream is AAC+ (SBR) */
    uint32_t sample_rate;    /* set if has_no_adts_header is true */
    uint8_t channels;        /* set if has_no_adts_header is true */
} aac_dec_config_t;

/**
 * \brief Parse one frame header from the input buffer.
 * \param buffer Parameter buffer.
 * \param size Parameter size.
 * \param frame Parameter frame.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t aac_parse_frame_header(const uint8_t *buffer,
                                    uint32_t size,
                                    aac_frame_info_t *frame);
/**
 * \brief Get a human-readable name string.
 * \param profile Parameter profile.
 * \return Pointer to a null-terminated string.
 */
const char *aac_profile_name(uint8_t profile);

/**
 * \brief Open and initialize the context.
 * \param config Parameter config.
 * \return Decoder handle value.
 */
audio_codec_dec_handle_t aac_pcm_decode_open(const aac_dec_config_t *config);
/**
 * \brief Close the context and release resources.
 * \param handle Parameter handle.
 */
void aac_pcm_decode_close(audio_codec_dec_handle_t handle);
/**
 * \brief Decode one input frame into PCM output.
 * \param handle Parameter handle.
 * \param in_frame Parameter in_frame.
 * \param out_frame Parameter out_frame.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t aac_pcm_decode_frame(audio_codec_dec_handle_t handle,
                                  audio_codec_dec_in_frame_t *in_frame,
                                  audio_codec_dec_out_frame_t *out_frame);
#ifdef __cplusplus
}
#endif

#endif
