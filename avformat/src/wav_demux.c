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

static avp_status_t wav_read_forward(avp_io_t *avp_io,
                                     uint32_t *current_offset,
                                     uint8_t *buffer,
                                     uint32_t size)
{
    avp_status_t st;

    st = avp_io_read(avp_io, buffer, size);
    if (st != AVP_OK) {
        return st;
    }
    if (wav_add_overflow(*current_offset, size, current_offset)) {
        return AVP_ERANGE;
    }
    return AVP_OK;
}

static avp_status_t wav_skip_forward(avp_io_t *avp_io,
                                     uint32_t *current_offset,
                                     uint32_t size)
{
    uint8_t buffer[64];

    while (size != 0u) {
        uint32_t read_size = size > (uint32_t)sizeof(buffer) ?
                                 (uint32_t)sizeof(buffer) :
                                 size;
        avp_status_t st;

        st = wav_read_forward(avp_io, current_offset, buffer, read_size);
        if (st != AVP_OK) {
            return st;
        }
        size -= read_size;
    }

    return AVP_OK;
}

static avp_status_t wav_validate_file_header(const wav_file_header_info_t *header,
                                             uint32_t wav_size,
                                             uint32_t data_offset)
{
    uint32_t block_align;
    uint32_t byte_rate;
    uint32_t data_size;
    uint32_t data_end;

    if (header == NULL) {
        return AVP_EINVAL;
    }

    if (wav_size < WAV_RIFF_HEADER_SIZE) {
        return AVP_EBADHEADER;
    }

    if (header->wave_header.ChunkID != WAV_ID_RIFF ||
        header->wave_header.Format != WAV_ID_WAVE ||
        header->wave_fmt.Subchunk1ID != WAV_ID_FMT ||
        header->wave_data.Subchunk2ID != WAV_ID_DATA) {
        return AVP_EBADHEADER;
    }

    if (header->wave_fmt.NumChannels == 0u ||
        header->wave_fmt.SampleRate == 0u ||
        header->wave_fmt.BlockAlign == 0u) {
        return AVP_EBADHEADER;
    }

    data_size = header->wave_data.Subchunk2Size;

    if (header->wave_fmt.AudioFormat == WAV_AUDIO_FORMAT_PCM) {
        if (header->wave_fmt.BitsPerSample == 0u ||
            (header->wave_fmt.BitsPerSample % 8u) != 0u) {
            return AVP_EBADHEADER;
        }

        block_align = (uint32_t)header->wave_fmt.NumChannels * ((uint32_t)header->wave_fmt.BitsPerSample / 8u);
        byte_rate = header->wave_fmt.SampleRate * block_align;

        if (block_align == 0u || block_align > UINT16_MAX) {
            return AVP_EBADHEADER;
        }

        if (header->wave_fmt.SampleRate != 0u && byte_rate / header->wave_fmt.SampleRate != block_align) {
            return AVP_EBADHEADER;
        }

        if (header->wave_fmt.BlockAlign != (uint16_t)block_align) {
            return AVP_EBADHEADER;
        }

        if (header->wave_fmt.ByteRate != byte_rate) {
            return AVP_EBADHEADER;
        }

        if ((data_size % block_align) != 0u) {
            return AVP_EBADHEADER;
        }

    } else if (header->wave_fmt.AudioFormat == WAV_AUDIO_FORMAT_ALAW ||
               header->wave_fmt.AudioFormat == WAV_AUDIO_FORMAT_MULAW) {
        uint32_t decoded_size;

        if (header->wave_fmt.NumChannels > UINT8_MAX ||
            header->wave_fmt.BitsPerSample != 8u) {
            return AVP_EBADHEADER;
        }

        block_align = header->wave_fmt.NumChannels;
        byte_rate = header->wave_fmt.SampleRate * block_align;

        if (byte_rate / header->wave_fmt.SampleRate != block_align) {
            return AVP_EBADHEADER;
        }

        if (header->wave_fmt.BlockAlign != (uint16_t)block_align) {
            return AVP_EBADHEADER;
        }

        if (header->wave_fmt.ByteRate != byte_rate) {
            return AVP_EBADHEADER;
        }

        if ((data_size % block_align) != 0u) {
            return AVP_EBADHEADER;
        }

        decoded_size = (uint32_t)data_size * sizeof(int16_t);
        if (decoded_size > UINT32_MAX) {
            return AVP_EBADHEADER;
        }

    } else if (header->wave_fmt.AudioFormat == WAV_AUDIO_FORMAT_IMA_ADPCM) {
        uint32_t header_size;
        uint32_t expected_samples_per_block;

        if (header->wave_fmt.NumChannels > ADPCM_IMA_MAX_CHANNELS ||
            header->wave_fmt.BitsPerSample != 4u ||
            header->wave_fmt.SamplesPerBlock == 0u) {
            return AVP_EBADHEADER;
        }

        header_size = (uint32_t)header->wave_fmt.NumChannels * 4u;
        block_align = header->wave_fmt.BlockAlign;
        if (block_align < header_size || (data_size % block_align) != 0u) {
            return AVP_EBADHEADER;
        }

        expected_samples_per_block = (((block_align - header_size) * 2u) /
                                      (uint32_t)header->wave_fmt.NumChannels) +
                                     1u;
        if (header->wave_fmt.SamplesPerBlock != expected_samples_per_block) {
            return AVP_EBADHEADER;
        }
    } else if (header->wave_fmt.AudioFormat == WAV_AUDIO_FORMAT_G722) {
        if (header->wave_fmt.NumChannels != 1u ||
            header->wave_fmt.SampleRate != 16000u ||
            header->wave_fmt.BlockAlign == 0u ||
            header->wave_fmt.BitsPerSample != 4u) {
            return AVP_EBADHEADER;
        }

        if ((data_size % header->wave_fmt.BlockAlign) != 0u) {
            return AVP_EBADHEADER;
        }
    } else {
        return AVP_EBADHEADER;
    }

    if (wav_add_overflow(data_offset, data_size, &data_end) ||
        data_end > wav_size) {
        return AVP_EBADHEADER;
    }

    if (header->wave_fmt.Subchunk1Size < 16u) {
        return AVP_EBADHEADER;
    }

    return AVP_OK;
}

avp_status_t wav_demux_open(wav_demux_t *demuxer,
                            avp_io_t *avp_io)
{
    uint32_t wav_size;
    uint32_t current_offset = 0u;
    uint8_t riff_buffer[WAV_RIFF_HEADER_SIZE];
    uint8_t fmt_found = 0u;
    uint8_t data_found = 0u;
    int64_t size;
    avp_status_t st;

    if (demuxer == NULL || avp_io == NULL) {
        return AVP_EINVAL;
    }

    size = avp_io_get_size(avp_io);
    if (size < 0) {
        return AVP_IO;
    }

    memset(demuxer, 0, sizeof(*demuxer));
    demuxer->common.avp_io = avp_io;
    demuxer->common.file_size = (uint32_t)size;

    st = wav_read_forward(avp_io,
                          &current_offset,
                          riff_buffer,
                          (uint32_t)sizeof(riff_buffer));
    if (st != AVP_OK) {
        return st;
    }

    demuxer->header.wave_header.ChunkID = AVP_GET_LE32(riff_buffer + 0u);
    demuxer->header.wave_header.ChunkSize = AVP_GET_LE32(riff_buffer + 4u);
    demuxer->header.wave_header.Format = AVP_GET_LE32(riff_buffer + 8u);
    if (wav_add_overflow(demuxer->header.wave_header.ChunkSize, 8u, &wav_size) ||
        demuxer->header.wave_header.ChunkID != WAV_ID_RIFF ||
        demuxer->header.wave_header.Format != WAV_ID_WAVE ||
        wav_size < WAV_RIFF_HEADER_SIZE ||
        wav_size > demuxer->common.file_size) {
        return AVP_EBADHEADER;
    }

    while (current_offset + WAV_CHUNK_HEADER_SIZE <= wav_size) {
        uint8_t chunk_header[WAV_CHUNK_HEADER_SIZE];
        uint32_t chunk_id;
        uint32_t chunk_size;
        uint32_t chunk_data_offset;
        uint32_t padded_size;
        uint32_t chunk_end;

        st = wav_read_forward(avp_io,
                              &current_offset,
                              chunk_header,
                              (uint32_t)sizeof(chunk_header));
        if (st != AVP_OK) {
            return st;
        }

        chunk_id = AVP_GET_LE32(chunk_header + 0u);
        chunk_size = AVP_GET_LE32(chunk_header + 4u);
        chunk_data_offset = current_offset;
        if (chunk_data_offset > wav_size ||
            chunk_size > wav_size - chunk_data_offset) {
            return AVP_EBADHEADER;
        }

        if (chunk_id == WAV_ID_FMT) {
            uint8_t fmt[20];
            uint32_t fmt_read_size;

            if (chunk_size < 16u) {
                return AVP_EBADHEADER;
            }

            memset(fmt, 0, sizeof(fmt));
            fmt_read_size = chunk_size > (uint32_t)sizeof(fmt) ?
                                (uint32_t)sizeof(fmt) :
                                chunk_size;
            st = wav_read_forward(avp_io,
                                  &current_offset,
                                  fmt,
                                  fmt_read_size);
            if (st != AVP_OK) {
                return st;
            }

            demuxer->header.wave_fmt.Subchunk1ID = chunk_id;
            demuxer->header.wave_fmt.Subchunk1Size = chunk_size;
            demuxer->header.wave_fmt.AudioFormat = AVP_GET_LE16(fmt + 0u);
            demuxer->header.wave_fmt.NumChannels = AVP_GET_LE16(fmt + 2u);
            demuxer->header.wave_fmt.SampleRate = AVP_GET_LE32(fmt + 4u);
            demuxer->header.wave_fmt.ByteRate = AVP_GET_LE32(fmt + 8u);
            demuxer->header.wave_fmt.BlockAlign = AVP_GET_LE16(fmt + 12u);
            demuxer->header.wave_fmt.BitsPerSample = AVP_GET_LE16(fmt + 14u);
            demuxer->header.wave_fmt.CbSize = 0u;
            if (chunk_size >= 18u) {
                demuxer->header.wave_fmt.CbSize = AVP_GET_LE16(fmt + 16u);
            }
            demuxer->header.wave_fmt.SamplesPerBlock = 0u;
            if (demuxer->header.wave_fmt.AudioFormat == WAV_AUDIO_FORMAT_IMA_ADPCM) {
                if (chunk_size < 20u || demuxer->header.wave_fmt.CbSize < 2u) {
                    return AVP_EBADHEADER;
                }
                demuxer->header.wave_fmt.SamplesPerBlock = AVP_GET_LE16(fmt + 18u);
            }
            fmt_found = 1u;
        } else if (chunk_id == WAV_ID_DATA) {
            if (fmt_found == 0u) {
                return AVP_EBADHEADER;
            }
            demuxer->header.wave_data.Subchunk2ID = chunk_id;
            demuxer->header.wave_data.Subchunk2Size = chunk_size;
            demuxer->common.stream_offset = chunk_data_offset;
            demuxer->common.stream_size = chunk_size;
            demuxer->common.current_offset = demuxer->common.stream_offset;
            demuxer->common.packet_index = 0u;
            data_found = 1u;
        }

        if (fmt_found && data_found) {
            break;
        }

        padded_size = chunk_size + (chunk_size & 1u);
        if (padded_size < chunk_size ||
            wav_add_overflow(chunk_data_offset, padded_size, &chunk_end) ||
            current_offset > chunk_end) {
            return AVP_EBADHEADER;
        }
        st = wav_skip_forward(avp_io,
                              &current_offset,
                              chunk_end - current_offset);
        if (st != AVP_OK) {
            return st;
        }
    }

    if (!fmt_found || !data_found) {
        return AVP_EBADHEADER;
    }

    st = wav_validate_file_header(&demuxer->header,
                                  wav_size,
                                  demuxer->common.stream_offset);
    if (st != AVP_OK) {
        return st;
    }

    return AVP_OK;
}

void wav_demux_close(wav_demux_t *demuxer)
{
    if (demuxer == NULL) {
        return;
    }

    memset(demuxer, 0, sizeof(*demuxer));
}

avp_status_t wav_demux_get_audio_stream_config(const wav_demux_t *demuxer,
                                               audio_codec_dec_config_t *config)
{
    if (demuxer == NULL || config == NULL) {
        return AVP_EINVAL;
    }
    if (demuxer->common.avp_io == NULL || demuxer->common.stream_size == 0u) {
        return AVP_EBADHEADER;
    }

    memset(config, 0, sizeof(*config));
    switch (demuxer->header.wave_fmt.AudioFormat) {
        case WAV_AUDIO_FORMAT_PCM:
            config->codec_type = AUDIO_CODEC_ID_PCM;
            config->pcm_config.channels = demuxer->header.wave_fmt.NumChannels;
            config->pcm_config.bits_per_sample = demuxer->header.wave_fmt.BitsPerSample;
            config->pcm_config.sample_rate = demuxer->header.wave_fmt.SampleRate;
            return AVP_OK;
#if defined(CONFIG_CHERRYAVP_ADPCM)
        case WAV_AUDIO_FORMAT_IMA_ADPCM:
            config->codec_type = AUDIO_CODEC_ID_IMA_ADPCM;
            config->adpcm_config.channels = demuxer->header.wave_fmt.NumChannels;
            config->adpcm_config.block_align = demuxer->header.wave_fmt.BlockAlign;
            config->adpcm_config.sample_rate = demuxer->header.wave_fmt.SampleRate;
            return AVP_OK;
#endif
#if defined(CONFIG_CHERRYAVP_G711)
        case WAV_AUDIO_FORMAT_ALAW:
        case WAV_AUDIO_FORMAT_MULAW:
            if (demuxer->header.wave_fmt.NumChannels > UINT8_MAX) {
                return AVP_EUNSUPPORTED;
            }
            config->codec_type = demuxer->header.wave_fmt.AudioFormat == WAV_AUDIO_FORMAT_ALAW ?
                                     AUDIO_CODEC_ID_G711_ALAW :
                                     AUDIO_CODEC_ID_G711_ULAW;
            config->g711_config.format = demuxer->header.wave_fmt.AudioFormat == WAV_AUDIO_FORMAT_ALAW ?
                                             G711_FORMAT_ALAW :
                                             G711_FORMAT_MULAW;
            config->g711_config.channels = (uint8_t)demuxer->header.wave_fmt.NumChannels;
            config->g711_config.sample_rate = demuxer->header.wave_fmt.SampleRate;
            return AVP_OK;
#endif
#if defined(CONFIG_CHERRYAVP_G722)
        case WAV_AUDIO_FORMAT_G722:
            config->codec_type = AUDIO_CODEC_ID_G722;
            config->g722_config.channels = (uint8_t)demuxer->header.wave_fmt.NumChannels;
            config->g722_config.sample_rate = demuxer->header.wave_fmt.SampleRate;
            config->g722_config.bitrate = demuxer->header.wave_fmt.SampleRate *
                                          demuxer->header.wave_fmt.BitsPerSample;
            config->g722_config.packed = 0u;
            return AVP_OK;
#endif
        default:
            return AVP_EUNSUPPORTED;
    }
}

avp_status_t wav_demux_read_packet(wav_demux_t *demuxer,
                                   avp_packet_t *packet)
{
    uint32_t stream_end;
    uint32_t block_align;
    uint32_t packet_size;
    avp_status_t st;

    if (demuxer == NULL || packet == NULL) {
        return AVP_EINVAL;
    }

    if (wav_add_overflow(demuxer->common.stream_offset,
                         demuxer->common.stream_size,
                         &stream_end)) {
        return AVP_ERANGE;
    }
    if (demuxer->common.current_offset >= stream_end) {
        return AVP_ENOENT;
    }

    block_align = demuxer->header.wave_fmt.BlockAlign;
    if (demuxer->header.wave_fmt.AudioFormat == WAV_AUDIO_FORMAT_IMA_ADPCM) {
        packet_size = block_align;
    } else {
        packet_size = MIN(demuxer->header.wave_fmt.ByteRate / 100,
                          stream_end - demuxer->common.current_offset); // 10ms
        packet_size -= packet_size % block_align;
        if (packet_size == 0u) {
            packet_size = stream_end - demuxer->common.current_offset;
        }
    }
    if (packet_size > stream_end - demuxer->common.current_offset) {
        return AVP_EBADFRAME;
    }

    st = avp_packet_expand(packet, packet_size);
    if (st != AVP_OK) {
        return st;
    }
    st = avp_io_read(demuxer->common.avp_io,
                     packet->buf,
                     packet_size);
    if (st != AVP_OK) {
        return st;
    }

    packet->size = packet_size;
    packet->offset = demuxer->common.current_offset;
    packet->index = demuxer->common.packet_index++;
    packet->type = AVP_PACKET_TYPE_AUDIO;
    demuxer->common.current_offset += packet_size;
    return AVP_OK;
}
