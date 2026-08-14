/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mp3_codec.h"

#include "pvmp3_framedecoder.h"

typedef struct {
    tPVMP3DecoderExternal ext;
    void *mem;
} mp3_opencore_decoder_t;

static const uint32_t mp3_sample_rate_table[4][3] = {
    { 11025u, 12000u, 8000u },
    { 0u, 0u, 0u },
    { 22050u, 24000u, 16000u },
    { 44100u, 48000u, 32000u },
};

static const uint16_t mp3_bitrate_table[4][16] = {
    { 0u, 8u, 16u, 24u, 32u, 40u, 48u, 56u, 64u, 80u, 96u, 112u, 128u, 144u, 160u, 0u },
    { 0u },
    { 0u, 8u, 16u, 24u, 32u, 40u, 48u, 56u, 64u, 80u, 96u, 112u, 128u, 144u, 160u, 0u },
    { 0u, 32u, 40u, 48u, 56u, 64u, 80u, 96u, 112u, 128u, 160u, 192u, 224u, 256u, 320u, 0u },
};

avp_status_t mp3_parse_frame_header(const uint8_t *buffer,
                                    uint32_t size,
                                    mp3_frame_info_t *frame)
{
    uint32_t raw;
    uint8_t version_id;
    uint8_t layer;
    uint8_t bitrate_index;
    uint8_t sample_rate_index;
    uint8_t channel_mode;
    uint16_t bitrate_kbps;
    uint32_t sample_rate;
    uint32_t frame_size;
    uint16_t samples_per_channel;
    uint8_t channels;
    uint8_t crc_size;
    uint8_t side_info_size;

    if (buffer == NULL || frame == NULL) {
        return AVP_EINVAL;
    }

    if (size < MP3_MIN_FRAME_HEADER_SIZE) {
        return AVP_ELACKFRAME;
    }

    raw = ((uint32_t)buffer[0] << 24) |
          ((uint32_t)buffer[1] << 16) |
          ((uint32_t)buffer[2] << 8) |
          ((uint32_t)buffer[3] << 0);

    if ((raw & 0xffe00000u) != 0xffe00000u) {
        return AVP_EBADHEADER;
    }

    version_id = (uint8_t)((raw >> 19) & 0x3u);
    layer = (uint8_t)((raw >> 17) & 0x3u);
    bitrate_index = (uint8_t)((raw >> 12) & 0xfu);
    sample_rate_index = (uint8_t)((raw >> 10) & 0x3u);
    channel_mode = (uint8_t)((raw >> 6) & 0x3u);

    if (version_id == MP3_MPEG_RESERVED ||
        layer != MP3_LAYER_III ||
        bitrate_index == 0u ||
        bitrate_index == 0xfu ||
        sample_rate_index == 0x3u ||
        ((raw >> 0) & 0x3u) == 0x2u) {
        return AVP_EBADHEADER;
    }

    bitrate_kbps = mp3_bitrate_table[version_id][bitrate_index];
    sample_rate = mp3_sample_rate_table[version_id][sample_rate_index];
    if (bitrate_kbps == 0u || sample_rate == 0u) {
        return AVP_EBADHEADER;
    }

    samples_per_channel = version_id == MP3_MPEG_1 ? 1152u : 576u;
    if (version_id == MP3_MPEG_1) {
        frame_size = (144000u * (uint32_t)bitrate_kbps) / sample_rate;
    } else {
        frame_size = (72000u * (uint32_t)bitrate_kbps) / sample_rate;
    }
    frame_size += (raw >> 9) & 0x1u;

    channels = channel_mode == MP3_CHANNEL_SINGLE_CHANNEL ? 1u : 2u;
    crc_size = ((raw >> 16) & 0x1u) == 0u ? 2u : 0u;
    if (version_id == MP3_MPEG_1) {
        side_info_size = channels == 1u ? 17u : 32u;
    } else {
        side_info_size = channels == 1u ? 9u : 17u;
    }

    if (frame_size < MP3_MIN_FRAME_HEADER_SIZE + crc_size + side_info_size) {
        return AVP_EBADHEADER;
    }

    memset(frame, 0, sizeof(*frame));
    frame->header.syncword = (raw >> 21) & 0x7ffu;
    frame->header.version_id = version_id;
    frame->header.layer = layer;
    frame->header.protection_bit = (raw >> 16) & 0x1u;
    frame->header.bitrate_index = bitrate_index;
    frame->header.sample_rate_index = sample_rate_index;
    frame->header.padding_bit = (raw >> 9) & 0x1u;
    frame->header.private_bit = (raw >> 8) & 0x1u;
    frame->header.channel_mode = channel_mode;
    frame->header.mode_extension = (raw >> 4) & 0x3u;
    frame->header.copyright = (raw >> 3) & 0x1u;
    frame->header.original = (raw >> 2) & 0x1u;
    frame->header.emphasis = (raw >> 0) & 0x3u;
    frame->bitrate_kbps = bitrate_kbps;
    frame->sample_rate = sample_rate;
    frame->frame_size = (uint16_t)frame_size;
    frame->samples_per_channel = samples_per_channel;
    frame->channels = channels;
    frame->crc_size = crc_size;
    frame->side_info_size = side_info_size;
    frame->main_data_offset = (uint16_t)(MP3_MIN_FRAME_HEADER_SIZE + crc_size + side_info_size);
    frame->main_data_size = (uint16_t)(frame_size - frame->main_data_offset);

    return AVP_OK;
}

const char *mp3_mpeg_version_name(uint8_t version_id)
{
    switch (version_id) {
        case MP3_MPEG_1:
            return "MPEG 1";
        case MP3_MPEG_2:
            return "MPEG 2";
        case MP3_MPEG_25:
            return "MPEG 2.5";
        default:
            return "reserved";
    }
}

const char *mp3_layer_name(uint8_t layer)
{
    switch (layer) {
        case MP3_LAYER_I:
            return "Layer I";
        case MP3_LAYER_II:
            return "Layer II";
        case MP3_LAYER_III:
            return "Layer III";
        default:
            return "reserved";
    }
}

const char *mp3_channel_mode_name(uint8_t channel_mode)
{
    switch (channel_mode) {
        case MP3_CHANNEL_STEREO:
            return "stereo";
        case MP3_CHANNEL_JOINT_STEREO:
            return "joint stereo";
        case MP3_CHANNEL_DUAL_CHANNEL:
            return "dual channel";
        case MP3_CHANNEL_SINGLE_CHANNEL:
            return "mono";
        default:
            return "unknown";
    }
}

static avp_status_t mp3_opencore_status_to_avp(ERROR_CODE status)
{
    switch (status) {
        case NO_DECODING_ERROR:
            return AVP_OK;
        case NO_ENOUGH_MAIN_DATA_ERROR:
            return AVP_ELACKFRAME;
        case OUTPUT_BUFFER_TOO_SMALL:
            return AVP_EBUFFER;
        case SYNCH_LOST_ERROR:
            return AVP_EBADHEADER;
        default:
            return AVP_EBADFRAME;
    }
}

audio_codec_dec_handle_t mp3_pcm_decode_open(void)
{
    mp3_opencore_decoder_t *decoder;
    uint32 mem_size;

    decoder = (mp3_opencore_decoder_t *)avp_malloc(sizeof(*decoder));
    if (decoder == NULL) {
        return NULL;
    }

    memset(decoder, 0, sizeof(*decoder));

    mem_size = pvmp3_decoderMemRequirements();
    decoder->mem = avp_malloc((size_t)mem_size);
    if (decoder->mem == NULL) {
        avp_free(decoder);
        return NULL;
    }

    memset(&decoder->ext, 0, sizeof(decoder->ext));
    decoder->ext.equalizerType = flat;
    decoder->ext.crcEnabled = 0;
    pvmp3_InitDecoder(&decoder->ext, decoder->mem);

    return (audio_codec_dec_handle_t)decoder;
}

void mp3_pcm_decode_close(audio_codec_dec_handle_t handle)
{
    mp3_opencore_decoder_t *decoder = (mp3_opencore_decoder_t *)handle;

    if (decoder == NULL) {
        return;
    }

    if (decoder->mem != NULL) {
        avp_free(decoder->mem);
    }

    avp_free(decoder);
}

avp_status_t mp3_pcm_decode_frame(audio_codec_dec_handle_t handle,
                                  audio_codec_dec_in_frame_t *in_frame,
                                  audio_codec_dec_out_frame_t *out_frame)
{
    mp3_opencore_decoder_t *decoder = (mp3_opencore_decoder_t *)handle;
    ERROR_CODE status;
    avp_status_t st;
    mp3_frame_info_t frame;
    uint32_t samples_per_channel;
    uint32_t total_samples;
    uint32_t output_words;

    if (decoder == NULL || decoder->mem == NULL ||
        in_frame == NULL || out_frame == NULL ||
        in_frame->buffer == NULL || out_frame->buffer == NULL) {
        return AVP_EINVAL;
    }

    out_frame->require_size = 0u;
    out_frame->pcm_size = 0u;
    in_frame->consumed_size = 0u;

    st = mp3_parse_frame_header(in_frame->buffer, in_frame->size, &frame);
    if (st != AVP_OK) {
        return st;
    }

    if (frame.frame_size > in_frame->size) {
        return AVP_ELACKFRAME;
    }

    out_frame->require_size = frame.samples_per_channel *
                              (uint32_t)frame.channels *
                              (uint32_t)sizeof(int16_t);
    if (out_frame->size < out_frame->require_size) {
        return AVP_EBUFFER;
    }

    output_words = out_frame->size / (uint32_t)sizeof(int16_t);

    decoder->ext.pInputBuffer = (uint8 *)in_frame->buffer;
    decoder->ext.inputBufferCurrentLength = (int32)frame.frame_size;
    decoder->ext.inputBufferMaxLength = (int32)frame.frame_size;
    decoder->ext.inputBufferUsedLength = 0;
    decoder->ext.pOutputBuffer = out_frame->buffer;
    decoder->ext.outputFrameSize = (int32)output_words;
    decoder->ext.equalizerType = flat;
    decoder->ext.crcEnabled = 0;

    status = pvmp3_framedecoder(&decoder->ext, decoder->mem);
    if (status != NO_DECODING_ERROR) {
        return mp3_opencore_status_to_avp(status);
    }

    if (decoder->ext.num_channels <= 0 ||
        decoder->ext.num_channels > (int16)MP3_MAX_CHANNELS ||
        decoder->ext.outputFrameSize <= 0) {
        pvmp3_resetDecoder(decoder->mem);
        return AVP_EBADFRAME;
    }

    total_samples = (uint32_t)decoder->ext.outputFrameSize;
    samples_per_channel = total_samples / (uint32_t)decoder->ext.num_channels;
    if (samples_per_channel == 0u ||
        samples_per_channel > MP3_MAX_SAMPLES_PER_FRAME ||
        total_samples != samples_per_channel * (uint32_t)decoder->ext.num_channels) {
        pvmp3_resetDecoder(decoder->mem);
        return AVP_EBADFRAME;
    }

    out_frame->sample_rate = decoder->ext.samplingRate > 0 ? (uint32_t)decoder->ext.samplingRate : frame.sample_rate;
    out_frame->bitrate = decoder->ext.bitRate > 0 ? (uint32_t)decoder->ext.bitRate / 1000u : frame.bitrate_kbps;
    out_frame->samples_per_channel = samples_per_channel;
    out_frame->duration_ms = audio_codec_calc_duration_ms(samples_per_channel,
                                                          out_frame->sample_rate);
    out_frame->channels = (uint8_t)decoder->ext.num_channels;
    out_frame->bits_per_sample = 16u;
    out_frame->pcm_size = total_samples * (uint32_t)sizeof(int16_t);

    in_frame->consumed_size = frame.frame_size;

    return AVP_OK;
}
