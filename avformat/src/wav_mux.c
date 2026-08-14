/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "wav_container.h"

static int wav_add_overflow(uint32_t lhs, uint32_t rhs, uint32_t *out)
{
    if (lhs > (UINT32_MAX - rhs)) {
        return 1;
    }

    if (out != NULL) {
        *out = lhs + rhs;
    }
    return 0;
}

static avp_status_t wav_write_exact(wav_mux_t *encoder,
                                    uint32_t offset,
                                    const uint8_t *buffer,
                                    uint32_t size)
{
    uint32_t total = 0u;
    avp_status_t st;

    if (encoder == NULL || encoder->avp_io == NULL || buffer == NULL) {
        return AVP_EINVAL;
    }

    st = avp_io_seek(encoder->avp_io, offset);
    if (st != AVP_OK) {
        return st;
    }

    while (total < size) {
        int actual_size = avp_io_write(encoder->avp_io, buffer + total, size - total);
        if (actual_size < 0) {
            return AVP_IO;
        }

        if ((uint32_t)actual_size > (size - total)) {
            return AVP_IO;
        }

        total += (uint32_t)actual_size;
    }

    return AVP_OK;
}

static avp_status_t wav_write_file_header(wav_mux_t *encoder)
{
    uint8_t header[WAV_HEADER_SIZE];

    if (encoder == NULL) {
        return AVP_EINVAL;
    }

    memset(header, 0, sizeof(header));
    AVP_SET_LE32(header + 0u, encoder->header.wave_header.ChunkID);
    AVP_SET_LE32(header + 4u, encoder->header.wave_header.ChunkSize);
    AVP_SET_LE32(header + 8u, encoder->header.wave_header.Format);
    AVP_SET_LE32(header + 12u, encoder->header.wave_fmt.Subchunk1ID);
    AVP_SET_LE32(header + 16u, encoder->header.wave_fmt.Subchunk1Size);
    AVP_SET_LE16(header + 20u, encoder->header.wave_fmt.AudioFormat);
    AVP_SET_LE16(header + 22u, encoder->header.wave_fmt.NumChannels);
    AVP_SET_LE32(header + 24u, encoder->header.wave_fmt.SampleRate);
    AVP_SET_LE32(header + 28u, encoder->header.wave_fmt.ByteRate);
    AVP_SET_LE16(header + 32u, encoder->header.wave_fmt.BlockAlign);
    AVP_SET_LE16(header + 34u, encoder->header.wave_fmt.BitsPerSample);
    AVP_SET_LE32(header + 36u, encoder->header.wave_data.Subchunk2ID);
    AVP_SET_LE32(header + 40u, encoder->header.wave_data.Subchunk2Size);

    return wav_write_exact(encoder, 0u, header, (uint32_t)sizeof(header));
}

avp_status_t wav_mux_open(wav_mux_t *encoder,
                          avp_io_t *avp_io,
                          uint16_t audio_format,
                          uint16_t num_channels,
                          uint32_t sample_rate,
                          uint16_t bits_per_sample)
{
    uint32_t block_align;
    uint32_t byte_rate;
    avp_status_t st;

    if (encoder == NULL) {
        return AVP_EINVAL;
    }

    if (avp_io == NULL) {
        return AVP_EINVAL;
    }

    encoder->avp_io = avp_io;
    memset(&encoder->header, 0, sizeof(encoder->header));
    encoder->file_size = 0u;
    encoder->offset = 0u;
    encoder->pcm_size = 0u;

    if (num_channels == 0u ||
        sample_rate == 0u ||
        bits_per_sample == 0u ||
        (bits_per_sample % 8u) != 0u) {
        return AVP_EINVAL;
    }

    block_align = (uint32_t)num_channels * ((uint32_t)bits_per_sample / 8u);
    if (block_align == 0u || block_align > UINT16_MAX) {
        return AVP_EINVAL;
    }

    byte_rate = sample_rate * block_align;
    if (byte_rate / sample_rate != block_align) {
        return AVP_EINVAL;
    }

    memset(&encoder->header, 0, sizeof(encoder->header));
    encoder->header.wave_header.ChunkID = WAV_ID_RIFF;
    encoder->header.wave_header.Format = WAV_ID_WAVE;
    encoder->header.wave_fmt.Subchunk1ID = WAV_ID_FMT;
    encoder->header.wave_data.Subchunk2ID = WAV_ID_DATA;

    encoder->header.wave_fmt.Subchunk1Size = 16u;
    encoder->header.wave_fmt.AudioFormat = audio_format;
    encoder->header.wave_fmt.NumChannels = num_channels;
    encoder->header.wave_fmt.SampleRate = sample_rate;
    encoder->header.wave_fmt.BitsPerSample = bits_per_sample;
    encoder->header.wave_fmt.BlockAlign = (uint16_t)block_align;
    encoder->header.wave_fmt.ByteRate = byte_rate;
    encoder->header.wave_fmt.CbSize = 0u;
    encoder->header.wave_fmt.SamplesPerBlock = 0u;

    encoder->header.wave_header.ChunkSize = WAV_HEADER_SIZE - 8u;
    encoder->header.wave_data.Subchunk2Size = 0u;

    encoder->offset = WAV_HEADER_SIZE;
    encoder->file_size = WAV_HEADER_SIZE;
    encoder->pcm_size = 0u;

    st = wav_write_file_header(encoder);
    if (st != AVP_OK) {
        return st;
    }

    return AVP_OK;
}

void wav_mux_close(wav_mux_t *encoder)
{
    memset(encoder, 0, sizeof(*encoder));
}

avp_status_t wav_mux(wav_mux_t *encoder,
                     const void *pcm_data,
                     uint32_t pcm_size)
{
    uint32_t block_align;
    avp_status_t st;

    if (encoder == NULL || pcm_data == NULL || pcm_size == 0u) {
        return AVP_EINVAL;
    }

    if (encoder->avp_io == NULL) {
        return AVP_EINVAL;
    }

    block_align = encoder->header.wave_fmt.BlockAlign;

    if (block_align == 0u) {
        return AVP_EINVAL;
    }

    if ((pcm_size % block_align) != 0u) {
        return AVP_EINVAL;
    }

    if (wav_add_overflow(encoder->offset, pcm_size, NULL)) {
        return AVP_ERANGE;
    }

    if (wav_add_overflow(encoder->header.wave_data.Subchunk2Size, pcm_size, NULL) ||
        wav_add_overflow(encoder->header.wave_header.ChunkSize, pcm_size, NULL)) {
        return AVP_ERANGE;
    }

    st = wav_write_exact(encoder, encoder->offset, pcm_data, pcm_size);
    if (st != AVP_OK) {
        return st;
    }

    encoder->offset += pcm_size;
    encoder->file_size = encoder->offset;
    encoder->header.wave_data.Subchunk2Size += pcm_size;
    encoder->header.wave_header.ChunkSize += pcm_size;
    encoder->pcm_size = encoder->header.wave_data.Subchunk2Size;

    st = wav_write_file_header(encoder);
    if (st != AVP_OK) {
        return st;
    }

    return AVP_OK;
}
