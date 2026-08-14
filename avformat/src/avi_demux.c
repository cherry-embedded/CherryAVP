/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "avi_container.h"

typedef struct {
    avi_fourcc_t id;
    avi_chunk_type_t type;
    uint32_t stream_index;
    uint32_t flags;
    uint32_t chunk_offset;
    uint32_t data_offset;
    uint32_t size;
} avi_chunk_info_t;

typedef struct {
    uint32_t end;
    uint32_t next;
    avi_fourcc_t type;
    uint32_t stream_index;
} avi_open_list_t;

#define AVI_OPEN_LIST_DEPTH 8u

static uint32_t avi_align2(uint32_t size)
{
    return size + (size & 1u);
}

static int avi_add_overflow(uint32_t lhs, uint32_t rhs, uint32_t *out)
{
    if (out == NULL) {
        return 1;
    }

    if (lhs > (UINT32_MAX - rhs)) {
        return 1;
    }

    *out = lhs + rhs;
    return 0;
}

static avp_status_t avi_read_chunk_header(avi_demux_t *demuxer,
                                          uint32_t offset,
                                          avi_chunk_header_t *header)
{
    uint8_t buffer[8];
    avp_status_t st;

    if (header == NULL) {
        return AVP_EINVAL;
    }

    st = avp_io_read_at(demuxer->common.avp_io, offset, buffer, (uint32_t)sizeof(buffer));
    if (st != AVP_OK) {
        return st;
    }

    header->id = AVP_GET_LE32(&buffer[0]);
    header->size = AVP_GET_LE32(&buffer[4]);
    return AVP_OK;
}

static avp_status_t avi_read_fourcc(avi_demux_t *demuxer,
                                    uint32_t offset,
                                    avi_fourcc_t *id)
{
    uint8_t buffer[4];
    avp_status_t st;

    if (id == NULL) {
        return AVP_EINVAL;
    }

    st = avp_io_read_at(demuxer->common.avp_io, offset, buffer, (uint32_t)sizeof(buffer));
    if (st != AVP_OK) {
        return st;
    }

    *id = AVP_GET_LE32(buffer);
    return AVP_OK;
}

static avp_status_t avi_chunk_next(uint32_t chunk_offset,
                                   uint32_t chunk_size,
                                   uint32_t limit,
                                   uint32_t *next_offset)
{
    uint32_t payload_offset;
    uint32_t aligned_size;
    uint32_t next;

    if (next_offset == NULL) {
        return AVP_EINVAL;
    }

    if (avi_add_overflow(chunk_offset, 8u, &payload_offset)) {
        return AVP_ERANGE;
    }

    aligned_size = avi_align2(chunk_size);
    if (aligned_size < chunk_size) {
        return AVP_ERANGE;
    }

    if (avi_add_overflow(payload_offset, aligned_size, &next)) {
        return AVP_ERANGE;
    }

    if (next > limit) {
        return AVP_EBADHEADER;
    }

    *next_offset = next;
    return AVP_OK;
}

static void avi_parse_main_header(const uint8_t *buffer, avi_main_header_t *header)
{
    header->dwMicroSecPerFrame = AVP_GET_LE32(&buffer[0]);
    header->dwMaxBytesPerSec = AVP_GET_LE32(&buffer[4]);
    header->dwPaddingGranularity = AVP_GET_LE32(&buffer[8]);
    header->dwFlags = AVP_GET_LE32(&buffer[12]);
    header->dwTotalFrames = AVP_GET_LE32(&buffer[16]);
    header->dwInitialFrames = AVP_GET_LE32(&buffer[20]);
    header->dwStreams = AVP_GET_LE32(&buffer[24]);
    header->dwSuggestedBufferSize = AVP_GET_LE32(&buffer[28]);
    header->dwWidth = AVP_GET_LE32(&buffer[32]);
    header->dwHeight = AVP_GET_LE32(&buffer[36]);
    header->dwReserved[0] = AVP_GET_LE32(&buffer[40]);
    header->dwReserved[1] = AVP_GET_LE32(&buffer[44]);
    header->dwReserved[2] = AVP_GET_LE32(&buffer[48]);
    header->dwReserved[3] = AVP_GET_LE32(&buffer[52]);
}

static void avi_parse_stream_header(const uint8_t *buffer, avi_stream_header_t *header)
{
    header->fccType = AVP_GET_LE32(&buffer[0]);
    header->fccHandler = AVP_GET_LE32(&buffer[4]);
    header->dwFlags = AVP_GET_LE32(&buffer[8]);
    header->wPriority = AVP_GET_LE16(&buffer[12]);
    header->wLanguage = AVP_GET_LE16(&buffer[14]);
    header->dwInitialFrames = AVP_GET_LE32(&buffer[16]);
    header->dwScale = AVP_GET_LE32(&buffer[20]);
    header->dwRate = AVP_GET_LE32(&buffer[24]);
    header->dwStart = AVP_GET_LE32(&buffer[28]);
    header->dwLength = AVP_GET_LE32(&buffer[32]);
    header->dwSuggestedBufferSize = AVP_GET_LE32(&buffer[36]);
    header->dwQuality = AVP_GET_LE32(&buffer[40]);
    header->dwSampleSize = AVP_GET_LE32(&buffer[44]);
    header->rcFrame.left = (int16_t)AVP_GET_LE16(&buffer[48]);
    header->rcFrame.top = (int16_t)AVP_GET_LE16(&buffer[50]);
    header->rcFrame.right = (int16_t)AVP_GET_LE16(&buffer[52]);
    header->rcFrame.bottom = (int16_t)AVP_GET_LE16(&buffer[54]);
}

static void avi_parse_bitmap_info(const uint8_t *buffer, avi_bitmap_info_header_t *format)
{
    format->biSize = AVP_GET_LE32(&buffer[0]);
    format->biWidth = (int32_t)AVP_GET_LE32(&buffer[4]);
    format->biHeight = (int32_t)AVP_GET_LE32(&buffer[8]);
    format->biPlanes = (int16_t)AVP_GET_LE16(&buffer[12]);
    format->biBitCount = (int16_t)AVP_GET_LE16(&buffer[14]);
    format->biCompression = AVP_GET_LE32(&buffer[16]);
    format->biSizeImage = AVP_GET_LE32(&buffer[20]);
    format->biXPelsPerMeter = (int32_t)AVP_GET_LE32(&buffer[24]);
    format->biYPelsPerMeter = (int32_t)AVP_GET_LE32(&buffer[28]);
    format->biClrUsed = AVP_GET_LE32(&buffer[32]);
    format->biClrImportant = AVP_GET_LE32(&buffer[36]);
}

static void avi_parse_wave_format(const uint8_t *buffer,
                                  uint32_t size,
                                  avi_wave_format_ex_t *format)
{
    format->wFormatTag = AVP_GET_LE16(&buffer[0]);
    format->nChannels = AVP_GET_LE16(&buffer[2]);
    format->nSamplesPerSec = AVP_GET_LE32(&buffer[4]);
    format->nAvgBytesPerSec = AVP_GET_LE32(&buffer[8]);
    format->nBlockAlign = AVP_GET_LE16(&buffer[12]);
    format->wBitsPerSample = AVP_GET_LE16(&buffer[14]);
    format->cbSize = 0u;
    format->wValidBitsPerSample = 0u;
    format->dwChannelMask = 0u;
    format->wSubFormatTag = 0u;

    if (size >= AVI_WAVE_FORMAT_EX_SIZE) {
        format->cbSize = AVP_GET_LE16(&buffer[16]);
    }
    if (size >= AVI_WAVE_FORMAT_EXT_SIZE && format->cbSize >= 22u) {
        format->wValidBitsPerSample = AVP_GET_LE16(&buffer[18]);
        format->dwChannelMask = AVP_GET_LE32(&buffer[20]);
        format->wSubFormatTag = AVP_GET_LE16(&buffer[24]);
    }
}

static avi_video_codec_t avi_video_codec_from_fourcc(avi_fourcc_t fourcc)
{
    switch (fourcc) {
        case AVI_FOURCC('D', 'I', 'B', ' '):
        case AVI_FOURCC('R', 'G', 'B', ' '):
        case AVI_FOURCC('R', 'A', 'W', ' '):
        case AVI_FOURCC('I', '4', '2', '0'):
        case AVI_FOURCC('I', 'Y', 'U', 'V'):
        case AVI_FOURCC('Y', 'U', 'Y', '2'):
        case AVI_FOURCC('U', 'Y', 'V', 'Y'):
            return AVI_VIDEO_CODEC_RAW;
        case AVI_FOURCC('M', 'J', 'P', 'G'):
        case AVI_FOURCC('J', 'P', 'E', 'G'):
        case AVI_FOURCC('m', 'j', 'p', 'g'):
        case AVI_FOURCC('m', 'j', 'p', 'a'):
        case AVI_FOURCC('m', 'j', 'p', 'b'):
            return AVI_VIDEO_CODEC_MJPEG;
        case AVI_FOURCC('P', 'N', 'G', '1'):
        case AVI_FOURCC('M', 'P', 'N', 'G'):
            return AVI_VIDEO_CODEC_PNG;
        case AVI_FOURCC('M', 'P', 'G', '1'):
        case AVI_FOURCC('P', 'I', 'M', '1'):
            return AVI_VIDEO_CODEC_MPEG1;
        case AVI_FOURCC('M', 'P', 'G', '2'):
        case AVI_FOURCC('M', 'P', 'E', 'G'):
        case AVI_FOURCC('P', 'I', 'M', '2'):
            return AVI_VIDEO_CODEC_MPEG2;
        case AVI_FOURCC('M', 'P', '4', 'V'):
        case AVI_FOURCC('F', 'M', 'P', '4'):
        case AVI_FOURCC('D', 'I', 'V', 'X'):
        case AVI_FOURCC('D', 'X', '5', '0'):
        case AVI_FOURCC('X', 'V', 'I', 'D'):
        case AVI_FOURCC('M', '4', 'S', '2'):
            return AVI_VIDEO_CODEC_MPEG4;
        case AVI_FOURCC('H', '2', '6', '4'):
        case AVI_FOURCC('X', '2', '6', '4'):
        case AVI_FOURCC('A', 'V', 'C', '1'):
        case AVI_FOURCC('a', 'v', 'c', '1'):
            return AVI_VIDEO_CODEC_H264;
        case AVI_FOURCC('H', 'E', 'V', 'C'):
        case AVI_FOURCC('H', '2', '6', '5'):
        case AVI_FOURCC('h', 'v', 'c', '1'):
        case AVI_FOURCC('h', 'e', 'v', '1'):
            return AVI_VIDEO_CODEC_HEVC;
        case AVI_FOURCC('V', 'P', '8', '0'):
            return AVI_VIDEO_CODEC_VP8;
        case AVI_FOURCC('V', 'P', '9', '0'):
            return AVI_VIDEO_CODEC_VP9;
        case AVI_FOURCC('A', 'V', '0', '1'):
        case AVI_FOURCC('a', 'v', '0', '1'):
            return AVI_VIDEO_CODEC_AV1;
        case AVI_FOURCC('T', 'H', 'E', 'O'):
        case AVI_FOURCC('t', 'h', 'e', 'o'):
            return AVI_VIDEO_CODEC_THEORA;
        case AVI_FOURCC('D', 'V', 'S', 'D'):
        case AVI_FOURCC('d', 'v', 's', 'd'):
        case AVI_FOURCC('d', 'v', '2', '5'):
        case AVI_FOURCC('d', 'v', '5', '0'):
            return AVI_VIDEO_CODEC_DV;
        case AVI_FOURCC('H', 'F', 'Y', 'U'):
            return AVI_VIDEO_CODEC_HUFFYUV;
        case AVI_FOURCC('F', 'F', 'V', '1'):
            return AVI_VIDEO_CODEC_FFV1;
        default:
            return AVI_VIDEO_CODEC_UNKNOWN;
    }
}

static avi_video_codec_t avi_detect_video_codec(const avi_stream_header_t *stream,
                                                const avi_bitmap_info_header_t *format)
{
    avi_video_codec_t codec;

    switch (format->biCompression) {
        case 0u:
        case 3u:
            return AVI_VIDEO_CODEC_RAW;
        case 1u:
        case 2u:
            return AVI_VIDEO_CODEC_RLE;
        case 4u:
            return AVI_VIDEO_CODEC_MJPEG;
        case 5u:
            return AVI_VIDEO_CODEC_PNG;
        default:
            break;
    }

    codec = avi_video_codec_from_fourcc(format->biCompression);
    if (codec == AVI_VIDEO_CODEC_UNKNOWN) {
        codec = avi_video_codec_from_fourcc(stream->fccHandler);
    }
    return codec;
}

static avi_audio_codec_t avi_audio_codec_from_format_tag(uint16_t format_tag)
{
    switch (format_tag) {
        case 0x0001u:
            return AVI_AUDIO_CODEC_PCM;
        case 0x0002u:
            return AVI_AUDIO_CODEC_MS_ADPCM;
        case 0x0003u:
            return AVI_AUDIO_CODEC_IEEE_FLOAT;
        case 0x0006u:
            return AVI_AUDIO_CODEC_G711_ALAW;
        case 0x0007u:
            return AVI_AUDIO_CODEC_G711_MULAW;
        case 0x028fu:
            return AVI_AUDIO_CODEC_G722;
        case 0x0008u:
        case 0x2001u:
            return AVI_AUDIO_CODEC_DTS;
        case 0x0011u:
            return AVI_AUDIO_CODEC_IMA_ADPCM;
        case 0x0050u:
            return AVI_AUDIO_CODEC_MP2;
        case 0x0055u:
            return AVI_AUDIO_CODEC_MP3;
        case 0x00ffu:
        case 0x1600u:
        case 0x1601u:
        case 0x1602u:
            return AVI_AUDIO_CODEC_AAC;
        case 0x0161u:
        case 0x0162u:
            return AVI_AUDIO_CODEC_WMA;
        case 0x2000u:
            return AVI_AUDIO_CODEC_AC3;
        case 0x2002u:
            return AVI_AUDIO_CODEC_EAC3;
        case 0x674fu:
        case 0x6750u:
        case 0x6751u:
        case 0x676fu:
        case 0x6770u:
        case 0x6771u:
            return AVI_AUDIO_CODEC_VORBIS;
        case 0x704fu:
            return AVI_AUDIO_CODEC_OPUS;
        case 0xf1acu:
            return AVI_AUDIO_CODEC_FLAC;
        case 0xfffeu:
            return AVI_AUDIO_CODEC_EXTENSIBLE;
        default:
            return AVI_AUDIO_CODEC_UNKNOWN;
    }
}

static avi_audio_codec_t avi_detect_audio_codec(const avi_wave_format_ex_t *format)
{
    avi_audio_codec_t codec;

    codec = avi_audio_codec_from_format_tag(format->wFormatTag);
    if (codec == AVI_AUDIO_CODEC_EXTENSIBLE && format->wSubFormatTag != 0u) {
        codec = avi_audio_codec_from_format_tag(format->wSubFormatTag);
        if (codec == AVI_AUDIO_CODEC_UNKNOWN) {
            codec = AVI_AUDIO_CODEC_EXTENSIBLE;
        }
    }
    return codec;
}

static avi_chunk_type_t avi_classify_chunk(avi_fourcc_t id, uint32_t *stream_index)
{
    uint8_t c0 = (uint8_t)(id & 0xffu);
    uint8_t c1 = (uint8_t)((id >> 8) & 0xffu);
    uint8_t c2 = (uint8_t)((id >> 16) & 0xffu);
    uint8_t c3 = (uint8_t)((id >> 24) & 0xffu);

    if (stream_index != NULL) {
        *stream_index = 0u;
    }

    if (c0 < (uint8_t)'0' || c0 > (uint8_t)'9' ||
        c1 < (uint8_t)'0' || c1 > (uint8_t)'9') {
        return AVI_CHUNK_TYPE_UNKNOWN;
    }

    if (stream_index != NULL) {
        *stream_index = ((uint32_t)(c0 - (uint8_t)'0') * 10u) +
                        (uint32_t)(c1 - (uint8_t)'0');
    }

    if (c2 == (uint8_t)'d' && (c3 == (uint8_t)'b' || c3 == (uint8_t)'c')) {
        return AVI_CHUNK_TYPE_VIDEO;
    }

    if (c2 == (uint8_t)'w' && c3 == (uint8_t)'b') {
        return AVI_CHUNK_TYPE_AUDIO;
    }

    if (c2 == (uint8_t)'p' && c3 == (uint8_t)'c') {
        return AVI_CHUNK_TYPE_PALETTE;
    }

    return AVI_CHUNK_TYPE_UNKNOWN;
}

static void avi_fill_chunk_info(avi_chunk_info_t *chunk,
                                avi_fourcc_t id,
                                uint32_t flags,
                                uint32_t chunk_offset,
                                uint32_t size)
{
    uint32_t stream_index = 0u;

    chunk->id = id;
    chunk->type = avi_classify_chunk(id, &stream_index);
    chunk->stream_index = stream_index;
    chunk->flags = flags;
    chunk->chunk_offset = chunk_offset;
    chunk->data_offset = chunk_offset + 8u;
    chunk->size = size;
}

static avp_status_t avi_parse_avih(avi_demux_t *demuxer,
                                   uint32_t offset,
                                   uint32_t size)
{
    uint8_t buffer[AVI_MAIN_HEADER_SIZE];
    avp_status_t st;

    if (demuxer == NULL) {
        return AVP_EINVAL;
    }

    if (size < AVI_MAIN_HEADER_SIZE) {
        return AVP_EBADHEADER;
    }

    st = avp_io_read_at(demuxer->common.avp_io, offset, buffer, AVI_MAIN_HEADER_SIZE);
    if (st != AVP_OK) {
        return st;
    }

    avi_parse_main_header(buffer, &demuxer->main_header);
    return AVP_OK;
}

static avp_status_t avi_parse_strf(avi_demux_t *demuxer,
                                   const avi_stream_header_t *stream,
                                   uint32_t stream_index,
                                   uint32_t offset,
                                   uint32_t size)
{
    uint8_t buffer[AVI_BITMAP_INFO_HEADER_SIZE];
    avp_status_t st;

    if (demuxer == NULL || stream == NULL) {
        return AVP_EINVAL;
    }

    if (stream->fccType == AVI_STREAM_VIDS) {
        if (size < AVI_BITMAP_INFO_HEADER_SIZE) {
            return AVP_EBADHEADER;
        }

        st = avp_io_read_at(demuxer->common.avp_io, offset, buffer, AVI_BITMAP_INFO_HEADER_SIZE);
        if (st != AVP_OK) {
            return st;
        }

        if (demuxer->has_video == 0u) {
            demuxer->video.stream = *stream;
            demuxer->video.stream_index = stream_index;
            avi_parse_bitmap_info(buffer, &demuxer->video.format);
            demuxer->video.codec_type = avi_detect_video_codec(stream,
                                                               &demuxer->video.format);
            demuxer->has_video = 1u;
        }

        return AVP_OK;
    }

    if (stream->fccType == AVI_STREAM_AUDS) {
        uint32_t read_size;

        if (size < AVI_WAVE_FORMAT_SIZE) {
            return AVP_EBADHEADER;
        }

        memset(buffer, 0, sizeof(buffer));
        if (size >= AVI_WAVE_FORMAT_EXT_SIZE) {
            read_size = AVI_WAVE_FORMAT_EXT_SIZE;
        } else if (size >= AVI_WAVE_FORMAT_EX_SIZE) {
            read_size = AVI_WAVE_FORMAT_EX_SIZE;
        } else {
            read_size = AVI_WAVE_FORMAT_SIZE;
        }
        st = avp_io_read_at(demuxer->common.avp_io, offset, buffer, read_size);
        if (st != AVP_OK) {
            return st;
        }

        if (demuxer->has_audio == 0u) {
            demuxer->audio.stream = *stream;
            demuxer->audio.stream_index = stream_index;
            demuxer->audio.block_count = stream->dwLength;
            avi_parse_wave_format(buffer, size, &demuxer->audio.format);
            demuxer->audio.codec_type = avi_detect_audio_codec(&demuxer->audio.format);
            demuxer->has_audio = 1u;
        }

        return AVP_OK;
    }

    return AVP_OK;
}

static avp_status_t avi_validate_index_candidate(avi_demux_t *demuxer,
                                                 uint32_t candidate,
                                                 avi_fourcc_t chunk_id,
                                                 uint32_t chunk_size)
{
    avi_chunk_header_t header;
    avp_status_t st;
    uint32_t data_offset;
    uint32_t data_end;

    if (demuxer == NULL) {
        return AVP_EINVAL;
    }

    if (avi_add_overflow(candidate, 8u, &data_offset)) {
        return AVP_ERANGE;
    }

    if (avi_add_overflow(data_offset, chunk_size, &data_end)) {
        return AVP_ERANGE;
    }

    if (data_end > demuxer->common.file_size) {
        return AVP_ERANGE;
    }

    st = avi_read_chunk_header(demuxer, candidate, &header);
    if (st != AVP_OK) {
        return st;
    }

    if (header.id != chunk_id || header.size != chunk_size) {
        return AVP_EBADHEADER;
    }

    return AVP_OK;
}

static int avi_push_candidate(uint32_t *candidates,
                              uint32_t *count,
                              uint32_t candidate)
{
    uint32_t i;

    if (candidates == NULL || count == NULL || *count >= 4u) {
        return 0;
    }

    for (i = 0u; i < *count; i++) {
        if (candidates[i] == candidate) {
            return 1;
        }
    }

    candidates[*count] = candidate;
    (*count)++;
    return 1;
}

static avp_status_t avi_resolve_index_offset(avi_demux_t *demuxer,
                                             const avi_index_entry_t *entry,
                                             uint32_t *chunk_offset)
{
    uint32_t candidates[4];
    uint32_t count = 0u;
    uint32_t candidate;
    uint32_t movi_type_offset;
    uint32_t i;

    if (demuxer == NULL || entry == NULL || chunk_offset == NULL) {
        return AVP_EINVAL;
    }

    avi_push_candidate(candidates, &count, entry->dwOffset);

    if (!avi_add_overflow(demuxer->movi_data_offset, entry->dwOffset, &candidate)) {
        avi_push_candidate(candidates, &count, candidate);
    }

    if (!avi_add_overflow(demuxer->movi_offset, 8u, &movi_type_offset)) {
        if (!avi_add_overflow(movi_type_offset, entry->dwOffset, &candidate)) {
            avi_push_candidate(candidates, &count, candidate);
        }
    }

    if (!avi_add_overflow(demuxer->movi_offset, entry->dwOffset, &candidate)) {
        avi_push_candidate(candidates, &count, candidate);
    }

    for (i = 0u; i < count; i++) {
        if (avi_validate_index_candidate(demuxer,
                                         candidates[i],
                                         entry->dwChunkId,
                                         entry->dwSize) == AVP_OK) {
            *chunk_offset = candidates[i];
            return AVP_OK;
        }
    }

    return AVP_EINVAL;
}

avp_status_t avi_demux_open(avi_demux_t *demuxer,
                            avp_io_t *avp_io)
{
    uint8_t riff_buffer[12];
    uint32_t current;
    uint32_t riff_end;
    uint32_t riff_file_size;
    avi_open_list_t stack[AVI_OPEN_LIST_DEPTH];
    uint32_t stack_depth;
    avi_stream_header_t current_stream;
    uint32_t current_stream_index;
    uint8_t stream_header_valid;
    int64_t avp_io_size;
    uint32_t file_size;
    avp_status_t st;

    if (demuxer == NULL || avp_io == NULL) {
        return AVP_EINVAL;
    }

    avp_io_size = avp_io_get_size(avp_io);
    if (avp_io_size < 0) {
        return AVP_IO;
    }
    if (avp_io_size < 12) {
        return AVP_ERANGE;
    }
    file_size = (uint32_t)avp_io_size;

    memset(demuxer, 0, sizeof(*demuxer));
    demuxer->common.avp_io = avp_io;

    st = avp_io_read_at(demuxer->common.avp_io, 0u, riff_buffer, (uint32_t)sizeof(riff_buffer));
    if (st != AVP_OK) {
        return st;
    }

    memset(&demuxer->riff, 0, sizeof(demuxer->riff));
    memset(&demuxer->main_header, 0, sizeof(demuxer->main_header));
    memset(&demuxer->video, 0, sizeof(demuxer->video));
    memset(&demuxer->audio, 0, sizeof(demuxer->audio));

    demuxer->riff.id = AVP_GET_LE32(&riff_buffer[0]);
    demuxer->riff.size = AVP_GET_LE32(&riff_buffer[4]);
    demuxer->riff.type = AVP_GET_LE32(&riff_buffer[8]);

    if (demuxer->riff.id != AVI_ID_RIFF || demuxer->riff.type != AVI_ID_AVI) {
        return AVP_EBADHEADER;
    }

    if (avi_add_overflow(demuxer->riff.size, 8u, &riff_file_size) ||
        riff_file_size < 12u ||
        riff_file_size > file_size) {
        return AVP_EBADHEADER;
    }

    demuxer->common.file_size = file_size;

    demuxer->stream_count = 0u;
    demuxer->has_video = 0u;
    demuxer->has_audio = 0u;
    demuxer->has_index = 0u;
    demuxer->hdrl_offset = 0u;
    demuxer->hdrl_size = 0u;
    demuxer->movi_offset = 0u;
    demuxer->movi_data_offset = 0u;
    demuxer->movi_size = 0u;
    demuxer->idx1_offset = 0u;
    demuxer->idx1_size = 0u;
    demuxer->index_count = 0u;
    demuxer->common.stream_offset = 0u;
    demuxer->common.stream_size = 0u;
    demuxer->common.current_offset = 0u;
    demuxer->common.packet_index = 0u;

    current = 12u;
    riff_end = riff_file_size;
    stack_depth = 0u;
    current_stream_index = 0u;
    stream_header_valid = 0u;
    memset(&current_stream, 0, sizeof(current_stream));

    while (current < riff_end) {
        avi_chunk_header_t header;
        avi_fourcc_t tag;
        uint8_t is_list;
        uint32_t payload_offset;
        uint32_t payload_size;
        uint32_t payload_end;
        uint32_t list_payload_offset;
        uint32_t list_payload_size;
        uint32_t limit;
        uint32_t next;

        while (stack_depth > 0u && current >= stack[stack_depth - 1u].end) {
            avi_open_list_t done = stack[stack_depth - 1u];

            stack_depth--;
            if (done.type == AVI_ID_STRL) {
                stream_header_valid = 0u;
                memset(&current_stream, 0, sizeof(current_stream));
            }
            if (current < done.next) {
                current = done.next;
            }
        }

        limit = stack_depth > 0u ? stack[stack_depth - 1u].end : riff_end;
        if (current >= limit) {
            continue;
        }

        if ((limit - current) < sizeof(avi_chunk_header_t)) {
            return AVP_EBADHEADER;
        }

        st = avi_read_chunk_header(demuxer, current, &header);
        if (st != AVP_OK) {
            return st;
        }

        st = avi_chunk_next(current, header.size, limit, &next);
        if (st != AVP_OK) {
            return st;
        }

        tag = header.id;
        is_list = tag == AVI_ID_LIST ? 1u : 0u;
        payload_offset = current + (uint32_t)sizeof(avi_chunk_header_t);
        payload_size = header.size;
        if (avi_add_overflow(payload_offset, payload_size, &payload_end) ||
            payload_end > limit) {
            return AVP_EBADHEADER;
        }
        list_payload_offset = payload_offset;
        list_payload_size = payload_size;

        if (is_list != 0u) {
            if (header.size < 4u) {
                return AVP_EBADHEADER;
            }

            st = avi_read_fourcc(demuxer,
                                 payload_offset,
                                 &tag);
            if (st != AVP_OK) {
                return st;
            }
            list_payload_offset = payload_offset + 4u;
            list_payload_size = header.size - 4u;
        }

        switch (tag) {
            case AVI_ID_HDRL:
                if (is_list == 0u) {
                    break;
                }
                if (stack_depth >= AVI_OPEN_LIST_DEPTH) {
                    return AVP_ERANGE;
                }

                demuxer->hdrl_offset = current;
                demuxer->hdrl_size = list_payload_size;
                stack[stack_depth].end = payload_end;
                stack[stack_depth].next = next;
                stack[stack_depth].type = AVI_ID_HDRL;
                stack[stack_depth].stream_index = UINT32_MAX;
                stack_depth++;
                current = list_payload_offset;
                continue;

            case AVI_ID_STRL:
                if (is_list == 0u) {
                    break;
                }
                if (stack_depth >= AVI_OPEN_LIST_DEPTH) {
                    return AVP_ERANGE;
                }

                current_stream_index = demuxer->stream_count;
                demuxer->stream_count++;
                stream_header_valid = 0u;
                memset(&current_stream, 0, sizeof(current_stream));
                stack[stack_depth].end = payload_end;
                stack[stack_depth].next = next;
                stack[stack_depth].type = AVI_ID_STRL;
                stack[stack_depth].stream_index = current_stream_index;
                stack_depth++;
                current = list_payload_offset;
                continue;

            case AVI_ID_MOVI:
                if (is_list == 0u) {
                    break;
                }
                demuxer->movi_offset = current;
                demuxer->movi_data_offset = list_payload_offset;
                demuxer->movi_size = list_payload_size;
                demuxer->common.current_offset = demuxer->movi_data_offset;
                break;

            case AVI_ID_AVIH:
                st = avi_parse_avih(demuxer, payload_offset, payload_size);
                if (st != AVP_OK) {
                    return st;
                }
                break;

            case AVI_ID_STRH:
                if (stack_depth == 0u ||
                    stack[stack_depth - 1u].type != AVI_ID_STRL) {
                    break;
                }
                if (payload_size < AVI_STREAM_HEADER_SIZE) {
                    return AVP_EBADHEADER;
                }

                uint8_t buffer[AVI_STREAM_HEADER_SIZE];

                st = avp_io_read_at(demuxer->common.avp_io,
                                    payload_offset,
                                    buffer,
                                    AVI_STREAM_HEADER_SIZE);
                if (st != AVP_OK) {
                    return st;
                }

                avi_parse_stream_header(buffer, &current_stream);
                current_stream_index = stack[stack_depth - 1u].stream_index;
                stream_header_valid = 1u;
                break;

            case AVI_ID_STRF:
                if (stack_depth == 0u ||
                    stack[stack_depth - 1u].type != AVI_ID_STRL ||
                    stream_header_valid == 0u) {
                    break;
                }

                st = avi_parse_strf(demuxer,
                                    &current_stream,
                                    current_stream_index,
                                    payload_offset,
                                    payload_size);
                if (st != AVP_OK) {
                    return st;
                }
                break;

            case AVI_ID_IDX1:
                demuxer->idx1_offset = current + 8u;
                demuxer->idx1_size = header.size;
                demuxer->index_count = header.size / AVI_INDEX_ENTRY_SIZE;
                demuxer->has_index = demuxer->index_count > 0u ? 1u : 0u;
                break;

            case AVI_ID_REC:
                if (is_list != 0u && stack_depth < AVI_OPEN_LIST_DEPTH) {
                    stack[stack_depth].end = payload_end;
                    stack[stack_depth].next = next;
                    stack[stack_depth].type = AVI_ID_REC;
                    stack[stack_depth].stream_index = UINT32_MAX;
                    stack_depth++;
                    current = list_payload_offset;
                    continue;
                }
                break;

            default:
                break;
        }

        current = next;
    }

    if (demuxer->hdrl_offset == 0u || demuxer->movi_offset == 0u) {
        return AVP_EBADHEADER;
    }

    if (demuxer->has_video == 0u && demuxer->has_audio == 0u) {
        return AVP_EUNSUPPORTED;
    }

    demuxer->common.stream_offset = demuxer->movi_data_offset;
    demuxer->common.stream_size = demuxer->movi_size;
    demuxer->common.current_offset = demuxer->common.stream_offset;
    st = avp_io_seek(demuxer->common.avp_io, demuxer->common.stream_offset);
    if (st != AVP_OK) {
        return st;
    }

    return AVP_OK;
}

void avi_demux_close(avi_demux_t *demuxer)
{
    if (demuxer == NULL) {
        return;
    }

    memset(demuxer, 0, sizeof(*demuxer));
}

avp_status_t avi_demux_get_audio_stream_config(const avi_demux_t *demuxer,
                                               audio_codec_dec_config_t *config)
{
    if (demuxer == NULL || config == NULL || demuxer->has_audio == 0u) {
        return AVP_EINVAL;
    }

    memset(config, 0, sizeof(*config));
    switch (demuxer->audio.codec_type) {
        case AVI_AUDIO_CODEC_PCM:
            config->codec_type = AUDIO_CODEC_ID_PCM;
            config->pcm_config.channels = demuxer->audio.format.nChannels;
            config->pcm_config.bits_per_sample = demuxer->audio.format.wBitsPerSample;
            config->pcm_config.sample_rate = demuxer->audio.format.nSamplesPerSec;
            return AVP_OK;
#if defined(CONFIG_CHERRYAVP_ADPCM)
        case AVI_AUDIO_CODEC_IMA_ADPCM:
            config->codec_type = AUDIO_CODEC_ID_IMA_ADPCM;
            config->adpcm_config.channels = demuxer->audio.format.nChannels;
            config->adpcm_config.block_align = demuxer->audio.format.nBlockAlign;
            config->adpcm_config.sample_rate = demuxer->audio.format.nSamplesPerSec;
            return AVP_OK;
#endif
#if defined(CONFIG_CHERRYAVP_G711)
        case AVI_AUDIO_CODEC_G711_ALAW:
        case AVI_AUDIO_CODEC_G711_MULAW:
            if (demuxer->audio.format.nChannels > UINT8_MAX) {
                return AVP_EUNSUPPORTED;
            }
            config->codec_type = demuxer->audio.codec_type == AVI_AUDIO_CODEC_G711_ALAW ?
                                     AUDIO_CODEC_ID_G711_ALAW :
                                     AUDIO_CODEC_ID_G711_ULAW;
            config->g711_config.format = demuxer->audio.codec_type == AVI_AUDIO_CODEC_G711_ALAW ?
                                             G711_FORMAT_ALAW :
                                             G711_FORMAT_MULAW;
            config->g711_config.channels = (uint8_t)demuxer->audio.format.nChannels;
            config->g711_config.sample_rate = demuxer->audio.format.nSamplesPerSec;
            return AVP_OK;
#endif
#if defined(CONFIG_CHERRYAVP_G722)
        case AVI_AUDIO_CODEC_G722:
            if (demuxer->audio.format.nChannels != 1u) {
                return AVP_EUNSUPPORTED;
            }
            config->codec_type = AUDIO_CODEC_ID_G722;
            config->g722_config.channels = (uint8_t)demuxer->audio.format.nChannels;
            config->g722_config.sample_rate = demuxer->audio.format.nSamplesPerSec;
            config->g722_config.bitrate = demuxer->audio.format.nSamplesPerSec *
                                          demuxer->audio.format.wBitsPerSample;
            config->g722_config.packed = 0u;
            return AVP_OK;
#endif
        default:
            return AVP_EUNSUPPORTED;
    }
}

static avp_status_t avi_read_next_chunk_info(avi_demux_t *demuxer,
                                             avi_chunk_info_t *chunk)
{
    uint32_t movi_end;
    uint32_t current;

    if (demuxer == NULL || chunk == NULL || demuxer->common.avp_io == NULL) {
        return AVP_EINVAL;
    }

    if (demuxer->movi_data_offset == 0u || demuxer->movi_size == 0u) {
        return AVP_EBADHEADER;
    }

    if (avi_add_overflow(demuxer->movi_data_offset, demuxer->movi_size, &movi_end)) {
        return AVP_ERANGE;
    }

    current = demuxer->common.current_offset;
    if (current < demuxer->movi_data_offset) {
        current = demuxer->movi_data_offset;
    }

    while (current < movi_end) {
        avi_chunk_header_t header;
        avi_fourcc_t tag;
        uint8_t is_list;
        avp_status_t st;
        uint32_t next;

        if ((movi_end - current) < 8u) {
            return AVP_ENOENT;
        }

        st = avi_read_chunk_header(demuxer, current, &header);
        if (st != AVP_OK) {
            return st;
        }

        st = avi_chunk_next(current, header.size, movi_end, &next);
        if (st != AVP_OK) {
            return st;
        }

        tag = header.id;
        is_list = tag == AVI_ID_LIST ? 1u : 0u;
        if (is_list != 0u) {
            if (header.size < 4u) {
                return AVP_EBADHEADER;
            }

            st = avi_read_fourcc(demuxer, current + 8u, &tag);
            if (st != AVP_OK) {
                return st;
            }
        }

        switch (tag) {
            case AVI_ID_REC:
                if (is_list != 0u) {
                    current += 12u;
                    continue;
                }
                break;

            default:
                if (is_list == 0u && header.size != 0u) {
                    avi_chunk_type_t type = avi_classify_chunk(header.id, NULL);

                    if (type != AVI_CHUNK_TYPE_UNKNOWN) {
                        avi_fill_chunk_info(chunk, header.id, 0u, current, header.size);
                        return AVP_OK;
                    }
                }
                break;
        }

        current = next;
    }

    return AVP_ENOENT;
}

static avp_status_t avi_read_index_chunk_info(avi_demux_t *demuxer,
                                              uint32_t index,
                                              avi_chunk_info_t *chunk)
{
    uint8_t buffer[AVI_INDEX_ENTRY_SIZE];
    avi_index_entry_t entry;
    avp_status_t st;
    uint32_t entry_offset;
    uint32_t chunk_offset;

    if (demuxer == NULL || chunk == NULL || demuxer->common.avp_io == NULL) {
        return AVP_EINVAL;
    }

    if (demuxer->has_index == 0u || demuxer->idx1_offset == 0u) {
        return AVP_ENOENT;
    }

    if (index >= demuxer->index_count) {
        return AVP_ERANGE;
    }

    if (avi_add_overflow(demuxer->idx1_offset, index * AVI_INDEX_ENTRY_SIZE, &entry_offset)) {
        return AVP_ERANGE;
    }

    st = avp_io_read_at(demuxer->common.avp_io, entry_offset, buffer, AVI_INDEX_ENTRY_SIZE);
    if (st != AVP_OK) {
        return st;
    }

    entry.dwChunkId = AVP_GET_LE32(&buffer[0]);
    entry.dwFlags = AVP_GET_LE32(&buffer[4]);
    entry.dwOffset = AVP_GET_LE32(&buffer[8]);
    entry.dwSize = AVP_GET_LE32(&buffer[12]);

    if (entry.dwSize == 0u) {
        return AVP_ENOENT;
    }

    st = avi_resolve_index_offset(demuxer, &entry, &chunk_offset);
    if (st != AVP_OK) {
        return st;
    }

    avi_fill_chunk_info(chunk, entry.dwChunkId, entry.dwFlags, chunk_offset, entry.dwSize);
    return AVP_OK;
}

static void avi_fill_packet(const avi_chunk_info_t *chunk, avp_packet_t *packet)
{
    packet->size = chunk->size;
    packet->offset = chunk->data_offset;
    packet->index = chunk->stream_index;
    packet->type = chunk->type == AVI_CHUNK_TYPE_VIDEO ?
                       AVP_PACKET_TYPE_VIDEO :
                   chunk->type == AVI_CHUNK_TYPE_AUDIO ?
                       AVP_PACKET_TYPE_AUDIO :
                       AVP_PACKET_TYPE_UNKNOWN;
}

avp_status_t avi_demux_peek_packet(avi_demux_t *demuxer,
                                   avp_packet_t *packet)
{
    avi_chunk_info_t chunk;
    avp_status_t st;

    if (demuxer == NULL || packet == NULL) {
        return AVP_EINVAL;
    }

    st = avi_read_next_chunk_info(demuxer, &chunk);
    if (st != AVP_OK) {
        return st;
    }

    avi_fill_packet(&chunk, packet);
    return AVP_OK;
}

avp_status_t avi_demux_peek_packet_by_idx(avi_demux_t *demuxer,
                                          uint32_t index,
                                          avp_packet_t *packet)
{
    avi_chunk_info_t chunk;
    avp_status_t st;

    if (demuxer == NULL || packet == NULL) {
        return AVP_EINVAL;
    }

    st = avi_read_index_chunk_info(demuxer, index, &chunk);
    if (st != AVP_OK) {
        return st;
    }

    avi_fill_packet(&chunk, packet);
    return AVP_OK;
}

avp_status_t avi_demux_pop_packet(avi_demux_t *demuxer,
                                  avp_packet_t *packet)
{
    avp_status_t st;
    uint32_t chunk_offset;
    uint32_t next_offset;

    if (demuxer == NULL || packet == NULL || packet->buf == NULL ||
        packet->size == 0u) {
        return AVP_EINVAL;
    }

    if (packet->offset < 8u) {
        return AVP_EINVAL;
    }

    st = avp_io_read_at(demuxer->common.avp_io,
                        packet->offset,
                        packet->buf,
                        packet->size);
    if (st != AVP_OK) {
        return st;
    }

    chunk_offset = packet->offset - 8u;
    if (avi_chunk_next(chunk_offset, packet->size, demuxer->common.file_size, &next_offset)) {
        return AVP_ERANGE;
    }
    demuxer->common.current_offset = next_offset;
    demuxer->common.packet_index++;
    return AVP_OK;
}

void avi_demux_rewind(avi_demux_t *demuxer)
{
    if (demuxer == NULL) {
        return;
    }

    demuxer->common.current_offset = demuxer->common.stream_offset;
    demuxer->common.packet_index = 0u;
    avp_io_seek(demuxer->common.avp_io, demuxer->common.stream_offset);
}

const char *avi_chunk_type_name(avi_chunk_type_t type)
{
    switch (type) {
        case AVI_CHUNK_TYPE_VIDEO:
            return "video";
        case AVI_CHUNK_TYPE_AUDIO:
            return "audio";
        case AVI_CHUNK_TYPE_PALETTE:
            return "palette";
        default:
            return "unknown";
    }
}

const char *avi_video_codec_name(avi_video_codec_t codec)
{
    switch (codec) {
        case AVI_VIDEO_CODEC_RAW:
            return "raw video";
        case AVI_VIDEO_CODEC_RLE:
            return "RLE";
        case AVI_VIDEO_CODEC_MJPEG:
            return "MJPEG";
        case AVI_VIDEO_CODEC_PNG:
            return "PNG";
        case AVI_VIDEO_CODEC_MPEG1:
            return "MPEG-1 Video";
        case AVI_VIDEO_CODEC_MPEG2:
            return "MPEG-2 Video";
        case AVI_VIDEO_CODEC_MPEG4:
            return "MPEG-4 Visual";
        case AVI_VIDEO_CODEC_H264:
            return "H.264";
        case AVI_VIDEO_CODEC_HEVC:
            return "HEVC";
        case AVI_VIDEO_CODEC_VP8:
            return "VP8";
        case AVI_VIDEO_CODEC_VP9:
            return "VP9";
        case AVI_VIDEO_CODEC_AV1:
            return "AV1";
        case AVI_VIDEO_CODEC_THEORA:
            return "Theora";
        case AVI_VIDEO_CODEC_DV:
            return "DV";
        case AVI_VIDEO_CODEC_HUFFYUV:
            return "HuffYUV";
        case AVI_VIDEO_CODEC_FFV1:
            return "FFV1";
        default:
            return "unknown";
    }
}

const char *avi_audio_codec_name(avi_audio_codec_t codec)
{
    switch (codec) {
        case AVI_AUDIO_CODEC_PCM:
            return "PCM";
        case AVI_AUDIO_CODEC_IEEE_FLOAT:
            return "IEEE float PCM";
        case AVI_AUDIO_CODEC_MS_ADPCM:
            return "MS ADPCM";
        case AVI_AUDIO_CODEC_IMA_ADPCM:
            return "IMA ADPCM";
        case AVI_AUDIO_CODEC_G711_ALAW:
            return "G.711 A-law";
        case AVI_AUDIO_CODEC_G711_MULAW:
            return "G.711 mu-law";
        case AVI_AUDIO_CODEC_G722:
            return "G.722";
        case AVI_AUDIO_CODEC_MP2:
            return "MP2";
        case AVI_AUDIO_CODEC_MP3:
            return "MP3";
        case AVI_AUDIO_CODEC_AAC:
            return "AAC";
        case AVI_AUDIO_CODEC_AC3:
            return "AC-3";
        case AVI_AUDIO_CODEC_EAC3:
            return "E-AC-3";
        case AVI_AUDIO_CODEC_DTS:
            return "DTS";
        case AVI_AUDIO_CODEC_WMA:
            return "WMA";
        case AVI_AUDIO_CODEC_FLAC:
            return "FLAC";
        case AVI_AUDIO_CODEC_VORBIS:
            return "Vorbis";
        case AVI_AUDIO_CODEC_OPUS:
            return "Opus";
        case AVI_AUDIO_CODEC_EXTENSIBLE:
            return "WAVE_FORMAT_EXTENSIBLE";
        default:
            return "unknown";
    }
}
