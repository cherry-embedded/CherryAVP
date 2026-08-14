/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MP3_CODEC_H
#define MP3_CODEC_H

#include "audio_codec_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MP3_ID3V1_SIZE        128u
#define MP3_ID3V2_HEADER_SIZE 10u
#define MP3_MAX_CHANNELS      2u
/*
 * Max decoded PCM samples per MP3 Layer III frame, per channel:
 * MPEG 1 Layer III has 2 granules * 576 samples = 1152 samples.
 * MPEG 2/2.5 Layer III has 1 granule * 576 samples = 576 samples.
 */
#define MP3_MAX_SAMPLES_PER_FRAME 1152u
/*
 * Max compressed MP3 Layer III frame size in bytes, excluding free format:
 * floor(144000 * 320 kbps / 32000 Hz) + 1 padding byte = 1441 bytes.
 */
#define MP3_MAX_FRAME_SIZE 1441u

#define MP3_MIN_FRAME_HEADER_SIZE 4u

#define MP3_PCM_MAX_SAMPLES_PER_FRAME (MP3_MAX_SAMPLES_PER_FRAME * MP3_MAX_CHANNELS)

typedef enum {
    MP3_MPEG_25 = 0,
    MP3_MPEG_RESERVED = 1,
    MP3_MPEG_2 = 2,
    MP3_MPEG_1 = 3
} mp3_mpeg_version_t;

typedef enum {
    MP3_LAYER_RESERVED = 0,
    MP3_LAYER_III = 1,
    MP3_LAYER_II = 2,
    MP3_LAYER_I = 3
} mp3_layer_t;

typedef enum {
    MP3_CHANNEL_STEREO = 0,
    MP3_CHANNEL_JOINT_STEREO = 1,
    MP3_CHANNEL_DUAL_CHANNEL = 2,
    MP3_CHANNEL_SINGLE_CHANNEL = 3
} mp3_channel_mode_t;

typedef struct {
    unsigned int syncword          : 11;
    unsigned int version_id        : 2;
    unsigned int layer             : 2;
    unsigned int protection_bit    : 1;
    unsigned int bitrate_index     : 4;
    unsigned int sample_rate_index : 2;
    unsigned int padding_bit       : 1;
    unsigned int private_bit       : 1;
    unsigned int channel_mode      : 2;
    unsigned int mode_extension    : 2;
    unsigned int copyright         : 1;
    unsigned int original          : 1;
    unsigned int emphasis          : 2;
} mp3_frame_header_t;

typedef struct {
    mp3_frame_header_t header;

    uint16_t bitrate_kbps;

    uint8_t crc_size;
    uint8_t side_info_size;
    uint16_t main_data_offset;
    uint16_t main_data_size;

    uint32_t frame_size;
    uint32_t sample_rate;
    uint32_t samples_per_channel;
    uint8_t channels;
} mp3_frame_info_t;

/**
 * \brief Parse one frame header from the input buffer.
 * \param buffer Parameter buffer.
 * \param size Parameter size.
 * \param frame Parameter frame.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t mp3_parse_frame_header(const uint8_t *buffer,
                                    uint32_t size,
                                    mp3_frame_info_t *frame);
/**
 * \brief Get a human-readable name string.
 * \param version_id Parameter version_id.
 * \return Pointer to a null-terminated string.
 */
const char *mp3_mpeg_version_name(uint8_t version_id);
/**
 * \brief Get a human-readable name string.
 * \param layer Parameter layer.
 * \return Pointer to a null-terminated string.
 */
const char *mp3_layer_name(uint8_t layer);
/**
 * \brief Get a human-readable name string.
 * \param channel_mode Parameter channel_mode.
 * \return Pointer to a null-terminated string.
 */
const char *mp3_channel_mode_name(uint8_t channel_mode);

/**
 * \brief Open and initialize the context.
 * \return Decoder handle value.
 */
audio_codec_dec_handle_t mp3_pcm_decode_open(void);
/**
 * \brief Close the context and release resources.
 * \param handle Parameter handle.
 */
void mp3_pcm_decode_close(audio_codec_dec_handle_t handle);
/**
 * \brief Decode one input frame into PCM output.
 * \param handle Parameter handle.
 * \param in_frame Parameter in_frame.
 * \param out_frame Parameter out_frame.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t mp3_pcm_decode_frame(audio_codec_dec_handle_t handle,
                                  audio_codec_dec_in_frame_t *in_frame,
                                  audio_codec_dec_out_frame_t *out_frame);

#ifdef __cplusplus
}
#endif

#endif
