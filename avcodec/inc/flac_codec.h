/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef FLAC_CODEC_H
#define FLAC_CODEC_H

#include "audio_codec_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLAC_MARKER_SIZE           4u
#define FLAC_METADATA_HEADER_SIZE  4u
#define FLAC_STREAMINFO_SIZE       34u
#define FLAC_FRAME_HEADER_MAX_SIZE 16u
#define FLAC_SEEKPOINT_SIZE        18u

#define FLAC_BLOCK_SIZE_192   192u
#define FLAC_BLOCK_SIZE_256   256u
#define FLAC_BLOCK_SIZE_512   512u
#define FLAC_BLOCK_SIZE_576   576u
#define FLAC_BLOCK_SIZE_1024  1024u
#define FLAC_BLOCK_SIZE_1152  1152u
#define FLAC_BLOCK_SIZE_2048  2048u
#define FLAC_BLOCK_SIZE_2304  2304u
#define FLAC_BLOCK_SIZE_4096  4096u
#define FLAC_BLOCK_SIZE_4608  4608u
#define FLAC_BLOCK_SIZE_8192  8192u
#define FLAC_BLOCK_SIZE_16384 16384u
#define FLAC_BLOCK_SIZE_32768 32768u

#define FLAC_BLOCK_SIZE_CODE_RESERVED 0u
#define FLAC_BLOCK_SIZE_CODE_192      1u
#define FLAC_BLOCK_SIZE_CODE_576      2u
#define FLAC_BLOCK_SIZE_CODE_1152     3u
#define FLAC_BLOCK_SIZE_CODE_2304     4u
#define FLAC_BLOCK_SIZE_CODE_4608     5u
#define FLAC_BLOCK_SIZE_CODE_EXTRA8   6u
#define FLAC_BLOCK_SIZE_CODE_EXTRA16  7u
#define FLAC_BLOCK_SIZE_CODE_256      8u
#define FLAC_BLOCK_SIZE_CODE_512      9u
#define FLAC_BLOCK_SIZE_CODE_1024     10u
#define FLAC_BLOCK_SIZE_CODE_2048     11u
#define FLAC_BLOCK_SIZE_CODE_4096     12u
#define FLAC_BLOCK_SIZE_CODE_8192     13u
#define FLAC_BLOCK_SIZE_CODE_16384    14u
#define FLAC_BLOCK_SIZE_CODE_32768    15u

/* FLAC channel assignment values above 7 use stereo decorrelation. */
#define FLAC_CHANNEL_ASSIGNMENT_INDEPENDENT_MAX 7u
#define FLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE       8u
#define FLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE      9u
#define FLAC_CHANNEL_ASSIGNMENT_MID_SIDE        10u

/*
 * STREAMINFO min_frame_size/max_frame_size are 24-bit fields.
 * A STREAMINFO max_frame_size value of 0 still means unknown.
 */
#define FLAC_MAX_FRAME_SIZE        0x00ffffffu
#define FLAC_SEEKPOINT_PLACEHOLDER 0xffffffffffffffffull

#ifndef FLAC_MAX_SEEK_POINTS
#define FLAC_MAX_SEEK_POINTS 64u
#endif

#define FLAC_MIN_FRAME_HEADER_SIZE 6

typedef enum {
    FLAC_METADATA_STREAMINFO = 0,
    FLAC_METADATA_PADDING = 1,
    FLAC_METADATA_APPLICATION = 2,
    FLAC_METADATA_SEEKTABLE = 3,
    FLAC_METADATA_VORBIS_COMMENT = 4,
    FLAC_METADATA_CUESHEET = 5,
    FLAC_METADATA_PICTURE = 6
} flac_metadata_type_t;

typedef struct {
    uint8_t is_last;
    uint8_t block_type;
    uint32_t length;
    uint32_t offset;
} flac_metadata_block_t;

typedef struct {
    uint16_t min_block_size;
    uint16_t max_block_size;
    uint32_t min_frame_size;
    uint32_t max_frame_size;
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
    uint64_t total_samples;
    uint8_t md5[16];
} flac_streaminfo_t;

typedef struct {
    uint64_t sample_number;
    uint64_t stream_offset;
    uint16_t frame_samples;
} flac_seekpoint_t;

typedef struct {
    unsigned int sync_code          : 14;
    unsigned int reserved           : 1;
    unsigned int blocking_strategy  : 1;
    unsigned int block_size_code    : 4;
    unsigned int sample_rate_code   : 4;
    unsigned int channel_assignment : 4;
    unsigned int sample_size_code   : 3;
    unsigned int reserved2          : 1;
} flac_frame_header_t;

typedef struct {
    flac_frame_header_t header;
    uint64_t frame_or_sample_number;
    uint32_t block_size;
    uint8_t header_size;
    uint8_t crc8;

    uint32_t frame_size;
    uint32_t sample_rate;
    uint32_t samples_per_channel;
    uint8_t channels;
    uint8_t bits_per_sample;
} flac_frame_info_t;

typedef struct {
    uint64_t target_sample;
    uint64_t frame_sample;
    uint32_t frame_offset;
    uint32_t skip_samples;
    uint8_t used_seektable;
} flac_seek_result_t;

typedef struct {
    uint8_t streaminfo[FLAC_STREAMINFO_SIZE];
    uint32_t streaminfo_size;
} flac_dec_config_t;

/**
 * \brief Parse one frame header from the input buffer.
 * \param buffer Parameter buffer.
 * \param size Parameter size.
 * \param frame Parameter frame.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t flac_parse_frame_header(const uint8_t *buffer,
                                     uint32_t size,
                                     flac_frame_info_t *frame);
/**
 * \brief Get a human-readable name string.
 * \param type Parameter type.
 * \return Pointer to a null-terminated string.
 */
const char *flac_metadata_type_name(uint8_t type);

/**
 * \brief Open and initialize the context.
 * \param config Parameter config.
 * \return Decoder handle value.
 */
audio_codec_dec_handle_t flac_pcm_decode_open(const flac_dec_config_t *config);
/**
 * \brief Close the context and release resources.
 * \param handle Parameter handle.
 */
void flac_pcm_decode_close(audio_codec_dec_handle_t handle);
/**
 * \brief Decode one input frame into PCM output.
 * \param handle Parameter handle.
 * \param in_frame Parameter in_frame.
 * \param out_frame Parameter out_frame.
 * \return Status code. See avp_status_t for details.
 */
avp_status_t flac_pcm_decode_frame(audio_codec_dec_handle_t handle,
                                   audio_codec_dec_in_frame_t *in_frame,
                                   audio_codec_dec_out_frame_t *out_frame);

#ifdef __cplusplus
}
#endif

#endif
