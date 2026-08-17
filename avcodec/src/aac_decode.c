/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "aac_codec.h"

#include "pvmp4audiodecoder_api.h"

typedef struct {
    tPVMP4AudioDecoderExternal ext;
    void *mem;
    aac_dec_config_t config;
    uint8_t first_frame;
} aac_opencore_decoder_t;

static const uint32_t aac_sample_rate_table[16] = {
    96000u, 88200u, 64000u, 48000u,
    44100u, 32000u, 24000u, 22050u,
    16000u, 12000u, 11025u, 8000u,
    7350u, 0u, 0u, 0u
};

static uint8_t aac_channel_config_to_channels(uint32_t channel_config)
{
    static const uint8_t channel_count_table[8] = {
        0u, 1u, 2u, 3u, 4u, 5u, 6u, 8u
    };

    if (channel_config >= (sizeof(channel_count_table) / sizeof(channel_count_table[0]))) {
        return 0u;
    }

    return channel_count_table[channel_config];
}

avp_status_t aac_parse_frame_header(const uint8_t *buffer,
                                    uint32_t size,
                                    aac_frame_info_t *frame)
{
    uint16_t frame_size;
    uint16_t header_length;
    uint8_t sf_index;

    if (buffer == NULL || frame == NULL) {
        return AVP_EINVAL;
    }

    if (size < AAC_MIN_FRAME_HEADER_SIZE) {
        return AVP_ELACKFRAME;
    }

    memset(frame, 0, sizeof(*frame));
    frame->adts.syncword = (uint16_t)(((uint16_t)buffer[0] << 4) | (buffer[1] >> 4));
    if (frame->adts.syncword != 0xfffu) {
        return AVP_EBADHEADER;
    }

    frame->adts.id = (buffer[1] >> 3) & 0x01u;
    frame->adts.layer = (buffer[1] >> 1) & 0x03u;
    frame->adts.protection_absent = buffer[1] & 0x01u;
    frame->adts.profile = (buffer[2] >> 6) & 0x03u;
    frame->audio_object_type = (aac_audio_object_type_t)(frame->adts.profile + 1u);
    sf_index = (buffer[2] >> 2) & 0x0fu;
    frame->adts.sampling_frequency_index = sf_index;
    frame->sample_rate = aac_sample_rate_table[sf_index];
    frame->adts.private_bit = (buffer[2] >> 1) & 0x01u;
    frame->adts.channel_configuration = (uint8_t)(((buffer[2] & 0x01u) << 2) | (buffer[3] >> 6));
    frame->channels = aac_channel_config_to_channels(frame->adts.channel_configuration);
    frame->adts.original_copy = (buffer[3] >> 5) & 0x01u;
    frame->adts.home = (buffer[3] >> 4) & 0x01u;
    frame->adts.copyright_identification_bit = (buffer[3] >> 3) & 0x01u;
    frame->adts.copyright_identification_start = (buffer[3] >> 2) & 0x01u;

    frame_size = (uint16_t)(((uint16_t)(buffer[3] & 0x03u) << 11) |
                            ((uint16_t)buffer[4] << 3) |
                            ((uint16_t)buffer[5] >> 5));
    header_length = frame->adts.protection_absent ? AAC_ADTS_HEADER_SIZE :
                                                    (AAC_ADTS_HEADER_SIZE + AAC_ADTS_CRC_SIZE);

    frame->frame_size = frame_size;
    frame->header_length = header_length;
    frame->adts.adts_buffer_fullness = (uint16_t)(((uint16_t)(buffer[5] & 0x1fu) << 6) |
                                                  ((uint16_t)buffer[6] >> 2));
    frame->adts.number_of_raw_data_blocks = buffer[6] & 0x03u;
    frame->samples_per_channel = (uint16_t)(AAC_MAX_SAMPLES * ((uint16_t)frame->adts.number_of_raw_data_blocks + 1u));
    frame->adts.aac_frame_length = frame->frame_size;

    if (frame->adts.layer != 0u ||
        frame->sample_rate == 0u ||
        frame->channels == 0u ||
        frame->channels > AAC_MAX_CHANNELS ||
        frame_size < header_length ||
        frame_size > AAC_MAX_FRAME_SIZE) {
        return AVP_EBADHEADER;
    }

    if (frame->adts.protection_absent == 0u) {
        frame->adts.crc_check = (uint16_t)(((uint16_t)buffer[AAC_ADTS_HEADER_SIZE] << 8) |
                                           ((uint16_t)buffer[AAC_ADTS_HEADER_SIZE + 1u] << 0));
    }

    return AVP_OK;
}

const char *aac_profile_name(uint8_t profile)
{
    switch (profile) {
        case 0u:
            return "Main";
        case 1u:
            return "LC";
        case 2u:
            return "SSR";
        case 3u:
            return "reserved";
        default:
            return "unknown";
    }
}

audio_codec_dec_handle_t aac_pcm_decode_open(const aac_dec_config_t *config)
{
    aac_opencore_decoder_t *decoder;
    UInt32 mem_size;
    Int status;

    if (config == NULL ||
        (config->has_no_adts_header && (config->channels == 0u || config->channels > AAC_MAX_CHANNELS))) {
        return NULL;
    }

    decoder = (aac_opencore_decoder_t *)avp_malloc(sizeof(*decoder));
    if (decoder == NULL) {
        return NULL;
    }

    memset(decoder, 0, sizeof(*decoder));
    decoder->first_frame = 1u;
    decoder->config = *config;

    mem_size = PVMP4AudioDecoderGetMemRequirements();
    decoder->mem = avp_malloc((size_t)mem_size);
    if (decoder->mem == NULL) {
        avp_free(decoder);
        return NULL;
    }
    decoder->ext.aacPlusEnabled = config->aac_plus_enabled ? TRUE : FALSE;
    if (PVMP4AudioDecoderInitLibrary(&decoder->ext, decoder->mem) != 0) {
        avp_free(decoder->mem);
        avp_free(decoder);
        return NULL;
    }

    if (decoder->config.has_no_adts_header) {
        status = PVMP4SetAudioConfig(&decoder->ext,
                                     decoder->mem,
                                     1,
                                     (Int)decoder->config.sample_rate,
                                     decoder->config.channels,
                                     MP4AUDIO_AAC_LC);
        if (status != MP4AUDEC_SUCCESS) {
            avp_free(decoder->mem);
            avp_free(decoder);
            return NULL;
        }
        decoder->first_frame = 0u;
    }

    return (audio_codec_dec_handle_t)decoder;
}

void aac_pcm_decode_close(audio_codec_dec_handle_t handle)
{
    aac_opencore_decoder_t *decoder = (aac_opencore_decoder_t *)handle;

    if (decoder == NULL) {
        return;
    }

    if (decoder->mem != NULL) {
        avp_free(decoder->mem);
    }

    avp_free(decoder);
}

avp_status_t aac_pcm_decode_frame(audio_codec_dec_handle_t handle,
                                  audio_codec_dec_in_frame_t *in_frame,
                                  audio_codec_dec_out_frame_t *out_frame)
{
    aac_opencore_decoder_t *decoder = (aac_opencore_decoder_t *)handle;
    Int status;
    avp_status_t st;
    aac_frame_info_t frame;

    if (decoder == NULL || decoder->mem == NULL ||
        in_frame == NULL || out_frame == NULL ||
        in_frame->buffer == NULL || out_frame->buffer == NULL) {
        return AVP_EINVAL;
    }

    out_frame->require_size = 0u;
    out_frame->pcm_size = 0u;
    in_frame->consumed_size = 0u;

    if (!decoder->config.has_no_adts_header) {
        st = aac_parse_frame_header(in_frame->buffer, in_frame->size, &frame);
        if (st != AVP_OK) {
            return st;
        }

        if (frame.frame_size > in_frame->size) {
            return AVP_ELACKFRAME;
        }
    } else {
        memset(&frame, 0, sizeof(frame));
        frame.frame_size = in_frame->size;
        frame.sample_rate = decoder->config.sample_rate;
        frame.samples_per_channel = AAC_MAX_SAMPLES;
        frame.channels = decoder->config.channels;
        frame.audio_object_type = AAC_AUDIO_OBJECT_TYPE_AAC_LC;
    }

    /*
     * PacketVideo uses pOutputBuffer_plus for AAC+/SBR second-half output.
     * Keep the public output buffer contiguous by reserving a second block.
     */
    out_frame->require_size = frame.samples_per_channel *
                              (uint32_t)frame.channels *
                              2u *
                              (uint32_t)sizeof(int16_t);

    if (out_frame->size < out_frame->require_size) {
        return AVP_EBUFFER;
    }

    decoder->ext.pInputBuffer = (UChar *)in_frame->buffer;
    decoder->ext.inputBufferCurrentLength = (Int)frame.frame_size;
    decoder->ext.inputBufferMaxLength = (Int)frame.frame_size;
    decoder->ext.inputBufferUsedLength = 0;
    decoder->ext.remainderBits = 0;
    decoder->ext.pOutputBuffer = out_frame->buffer;
    decoder->ext.pOutputBuffer_plus = out_frame->buffer +
                                      frame.samples_per_channel * (uint32_t)frame.channels;
    decoder->ext.outputFormat = OUTPUTFORMAT_16PCM_INTERLEAVED;
    decoder->ext.desiredChannels = frame.channels;
    decoder->ext.aacPlusEnabled = decoder->config.aac_plus_enabled ? TRUE : FALSE;
    decoder->ext.repositionFlag = decoder->first_frame != 0u ? TRUE : FALSE;

    if (!decoder->config.has_no_adts_header && decoder->first_frame != 0u) {
        status = PVMP4AudioDecoderConfig(&decoder->ext, decoder->mem);
    } else {
        status = MP4AUDEC_INVALID_FRAME;
    }

    if (status != MP4AUDEC_SUCCESS) {
        decoder->ext.pInputBuffer = (UChar *)in_frame->buffer;
        decoder->ext.inputBufferCurrentLength = (Int)frame.frame_size;
        decoder->ext.inputBufferMaxLength = (Int)frame.frame_size;
        decoder->ext.inputBufferUsedLength = 0;
        decoder->ext.remainderBits = 0;
        decoder->ext.repositionFlag = decoder->first_frame != 0u ? TRUE : FALSE;
        status = PVMP4AudioDecodeFrame(&decoder->ext, decoder->mem);
    }
    if (status != MP4AUDEC_SUCCESS) {
        return AVP_EBADFRAME;
    }
    decoder->first_frame = 0u;

    out_frame->sample_rate = decoder->ext.samplingRate;
    out_frame->samples_per_channel = decoder->ext.frameLength;
    out_frame->duration_ms = audio_codec_calc_duration_ms(out_frame->samples_per_channel,
                                                          out_frame->sample_rate);
    out_frame->channels = decoder->ext.encodedChannels;
    out_frame->bitrate = audio_codec_calc_bitrate_kbps(frame.frame_size,
                                                       out_frame->sample_rate,
                                                       out_frame->samples_per_channel);
    out_frame->bits_per_sample = 16u;
    out_frame->pcm_size = out_frame->samples_per_channel *
                          (uint32_t)out_frame->channels *
                          (uint32_t)sizeof(int16_t);

    in_frame->consumed_size = frame.frame_size;

    return AVP_OK;
}
