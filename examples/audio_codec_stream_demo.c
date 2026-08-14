/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "audio_stream_demux.h"
#include "tlsf_port.h"

#include <time.h>

#define OUTPUT_BUFFER_SIZE (8u * 1024u)

#define FAIL(...)            \
    do {                     \
        printf(__VA_ARGS__); \
        goto out;            \
    } while (0)

static int file_io_read(avp_io_t *io,
                        uint8_t *buffer,
                        uint32_t size)
{
    FILE *fp = (FILE *)io->priv;

    if (fp == NULL || buffer == NULL) {
        return -1;
    }
    return (int)fread(buffer, 1u, size, fp);
}

static int file_io_seek(avp_io_t *io, uint32_t offset)
{
    FILE *fp = (FILE *)io->priv;

    if (fp == NULL) {
        return -1;
    }
    return fseek(fp, (long)offset, SEEK_SET);
}

static int file_io_get_size(avp_io_t *io)
{
    FILE *fp = (FILE *)io->priv;
    int current;
    int size;

    if (fp == NULL) {
        return 0u;
    }
    current = ftell(fp);
    if (current < 0L || fseek(fp, 0L, SEEK_END) != 0) {
        return 0u;
    }
    size = ftell(fp);
    if (size < 0L || fseek(fp, current, SEEK_SET) != 0) {
        return 0u;
    }
    return (uint32_t)size;
}

static int parse_u32(const char *text, uint32_t *out)
{
    char *end = NULL;
    unsigned int value;

    if (text == NULL || out == NULL || text[0] == '\0') {
        return 0;
    }
    value = strtoul(text, &end, 0);
    if (end == text || *end != '\0' || value > UINT32_MAX) {
        return 0;
    }
    *out = (uint32_t)value;
    return 1;
}

static void print_usage(const char *program)
{
    printf("usage: %s <aac|mp3|amr|wav|flac|alac|caf|m4a|ogg> <input> <output.pcm> "
           "[chunk_size] [frame_count]\n",
           program);
}

static int write_pcm(FILE *out_fp, const audio_codec_dec_out_frame_t *out_frame)
{
    size_t samples;

    if (out_fp == NULL || out_frame == NULL || out_frame->pcm_size == 0u) {
        return 0;
    }
    samples = out_frame->pcm_size / sizeof(int16_t);
    return fwrite(out_frame->buffer, sizeof(int16_t), samples, out_fp) == samples ? 0 : -1;
}

static int handle_decoded_frame(FILE *out_fp,
                                const audio_codec_dec_out_frame_t *out_frame,
                                uint32_t frame_index,
                                uint32_t frame_offset,
                                uint32_t frame_size,
                                uint32_t *pcm_samples)
{
    if (write_pcm(out_fp, out_frame) != 0) {
        return -1;
    }
    *pcm_samples += (uint32_t)out_frame->pcm_size / sizeof(int16_t);
    printf("\nFrame #%u:\n", (unsigned int)frame_index);
    printf("  offset             : %u\n", (unsigned int)frame_offset);
    printf("  frame_size         : %u\n", (unsigned int)frame_size);
    printf("  pcm_size           : %u\n", (unsigned int)out_frame->pcm_size);
    printf("  PCM:\n");
    printf("    samples_per_channel: %u\n", (unsigned int)out_frame->samples_per_channel);
    printf("    duration_ms        : %u\n", (unsigned int)out_frame->duration_ms);
    printf("    channels           : %u\n", (unsigned int)out_frame->channels);
    printf("    bits_per_sample    : %u\n", (unsigned int)out_frame->bits_per_sample);
    printf("    sample_rate        : %u Hz\n", (unsigned int)out_frame->sample_rate);
    printf("    bitrate            : %u kbps\n", (unsigned int)out_frame->bitrate);
    return 0;
}

static int run_decode(const char *codec_name,
                      const char *input_path,
                      const char *output_path,
                      uint32_t frame_count)
{
    FILE *in_fp = NULL;
    FILE *out_fp = NULL;
    audio_stream_demux_t demuxer;
    audio_codec_dec_handle_t decoder_handle = NULL;
    avp_packet_t *packet = NULL;
    int16_t *pcm_buffer = NULL;
    uint32_t pcm_buffer_size = OUTPUT_BUFFER_SIZE;
    uint32_t frame_index = 0u;
    uint32_t pcm_samples = 0u;
    uint32_t free_memory;
    uint32_t total_audio_duration_ms = 0u;
    clock_t total_decode_ticks = 0;
    avp_io_t avp_io;
    audio_stream_demux_info_t demux_info;
    avp_status_t st;
    int ret = 1;

    memset(&demuxer, 0, sizeof(demuxer));

    in_fp = fopen(input_path, "rb");
    if (in_fp == NULL) {
        FAIL("open %s failed\n", input_path);
    }
    out_fp = fopen(output_path, "wb");
    if (out_fp == NULL) {
        FAIL("open %s failed\n", output_path);
    }

    pcm_buffer = (int16_t *)avp_malloc(pcm_buffer_size);
    if (pcm_buffer == NULL) {
        FAIL("alloc buffer failed\n");
    }

    packet = avp_packet_alloc(4096u);
    if (packet == NULL) {
        FAIL("init packet failed\n");
    }

    free_memory = avp_mem_free_size();
    printf("stream free memory: %u bytes\n", (unsigned int)free_memory);

    avp_io_init(&avp_io, file_io_read, NULL, file_io_seek, file_io_get_size, in_fp);
    st = audio_stream_demux_open(codec_name, &demuxer, &avp_io, &demux_info);
    if (st != AVP_OK) {
        FAIL("open %s demuxer failed: %d\n", codec_name, (int)st);
    }

    printf("input_path %s stream_offset: %u, stream_size: %u\n",
           input_path,
           (unsigned int)demux_info.stream_offset,
           (unsigned int)demux_info.stream_size);

    decoder_handle = audio_codec_dec_open(&demux_info.config);
    if (decoder_handle == NULL) {
        FAIL("open audio stream decoder failed\n");
    }

    while (frame_count == 0u || frame_index < frame_count) {
        audio_codec_dec_in_frame_t in_frame;
        audio_codec_dec_out_frame_t out_frame;

        st = audio_stream_demux_read_packet(&demuxer, packet);
        if (st == AVP_ENOENT) {
            break;
        }
        if (st != AVP_OK) {
            FAIL("read packet failed at frame %u: %d\n",
                 (unsigned int)frame_index, (int)st);
        }

        memset(&in_frame, 0, sizeof(in_frame));
        in_frame.buffer = packet->buf;
        in_frame.size = packet->size;

        for (;;) {
            memset(&out_frame, 0, sizeof(out_frame));
            out_frame.buffer = pcm_buffer;
            out_frame.size = pcm_buffer_size;

            clock_t decode_start = clock();
            st = audio_codec_dec_frame(decoder_handle, &in_frame, &out_frame);
            total_decode_ticks += clock() - decode_start;
            if (st != AVP_EBUFFER) {
                break;
            }

            int16_t *new_pcm_buffer = (int16_t *)avp_realloc(pcm_buffer,
                                                             out_frame.require_size);
            if (new_pcm_buffer == NULL) {
                FAIL("grow pcm buffer to %u failed\n",
                     (unsigned int)out_frame.require_size);
            }
            pcm_buffer = new_pcm_buffer;
            pcm_buffer_size = out_frame.require_size;
        }

        if (st != AVP_OK ||
            handle_decoded_frame(out_fp,
                                 &out_frame,
                                 frame_index,
                                 packet->offset,
                                 packet->size,
                                 &pcm_samples) != 0) {
            FAIL("decode failed at frame %u: %d\n",
                 (unsigned int)frame_index, (int)st);
        }
        total_audio_duration_ms += out_frame.duration_ms;
        frame_index++;
    }

    double decode_time_ms = ((double)total_decode_ticks * 1000.0) / (double)1000000;
    double cpu_usage = total_audio_duration_ms == 0u ? 0.0 :
                                                       (decode_time_ms * 100.0) / (double)total_audio_duration_ms;

    printf("decode done: %u frames, "
           "pcm write %u bytes, "
           "cost memory: %u bytes, "
           "audio duration=%u ms, "
           "audio_codec_dec_frame time=%.2f ms, "
           "cpu usage=%.2f%%\n",
           (unsigned int)frame_index,
           (unsigned int)(pcm_samples * sizeof(*pcm_buffer)),
           (unsigned int)(free_memory - avp_mem_free_size()),
           (unsigned int)total_audio_duration_ms,
           decode_time_ms,
           cpu_usage);
    ret = 0;

out:
    if (decoder_handle != NULL) {
        audio_codec_dec_close(decoder_handle);
    }
    audio_stream_demux_close(&demuxer);
    if (packet != NULL) {
        avp_packet_free(packet);
    }
    if (pcm_buffer != NULL) {
        avp_free(pcm_buffer);
    }
    if (out_fp != NULL && fclose(out_fp) != 0 && ret == 0) {
        ret = 1;
    }
    if (in_fp != NULL) {
        fclose(in_fp);
    }
    return ret;
}

int main(int argc, char **argv)
{
    uint32_t frame_count = 0u;

    avp_mem_init();
    if (argc < 4 || argc > 6) {
        print_usage(argv[0]);
        return 1;
    }
    if (argc >= 5 && !parse_u32(argv[4], &frame_count)) {
        print_usage(argv[0]);
        return 1;
    }
    return run_decode(argv[1], argv[2], argv[3], frame_count);
}
