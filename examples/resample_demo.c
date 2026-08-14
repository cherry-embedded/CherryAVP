/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "avp_swr.h"
#include "wav_container.h"
#include "tlsf_port.h"

#include <time.h>

#define DEFAULT_INPUT_WAV       "../examples/files/jinitaimei.wav"
#define DEFAULT_OUTPUT_PCM      "resample_out.pcm"
#define DEFAULT_OUT_SAMPLE_RATE 16000u
#define DEFAULT_OUT_CHANNELS    1u
#define DEFAULT_OUT_BITS        16u
#define DEFAULT_OUT_LAYOUT      AVP_SAMPLE_LAYOUT_INTERLEAVED

#define RESAMPLE_DEMO_MAX_PLANES 8u
#define RESAMPLE_DEMO_PATH_SIZE  512u

typedef struct {
    avp_audio_format_t fmt;
    uint8_t *planes[RESAMPLE_DEMO_MAX_PLANES];
    uint32_t capacity_samples;
} pcm_view_t;

typedef struct {
    FILE *planes[RESAMPLE_DEMO_MAX_PLANES];
    uint8_t plane_count;
} pcm_writer_t;

static int file_read_cb(avp_io_t *avp_io, uint8_t *buffer, uint32_t size)
{
    FILE *fp = (FILE *)avp_io->priv;

    if (fp == NULL || buffer == NULL) {
        return -1;
    }
    return (int)fread(buffer, 1u, size, fp);
}

static int file_seek_cb(avp_io_t *avp_io, uint32_t offset)
{
    FILE *fp = (FILE *)avp_io->priv;

    if (fp == NULL) {
        return -1;
    }
    return fseek(fp, (long)offset, SEEK_SET);
}

static int file_get_size_cb(avp_io_t *avp_io)
{
    FILE *fp = (FILE *)avp_io->priv;
    long current;
    long size;

    if (fp == NULL) {
        return -1;
    }

    current = ftell(fp);
    if (current < 0L || fseek(fp, 0L, SEEK_END) != 0) {
        return -1;
    }
    size = ftell(fp);
    if (size < 0L || fseek(fp, current, SEEK_SET) != 0) {
        return -1;
    }
    return (int)size;
}

static const char *layout_name(avp_sample_layout_t layout)
{
    return layout == AVP_SAMPLE_LAYOUT_PLANAR ? "planar" : "interleaved";
}

static int parse_u32(const char *text, uint32_t min, uint32_t max, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed;

    if (text == NULL || value == NULL) {
        return -1;
    }

    parsed = strtoul(text, &end, 0);
    if (end == text ||
        *end != '\0' ||
        parsed < min ||
        parsed > max) {
        return -1;
    }

    *value = (uint32_t)parsed;
    return 0;
}

static int parse_u8(const char *text, uint8_t min, uint8_t max, uint8_t *value)
{
    uint32_t parsed;

    if (parse_u32(text, min, max, &parsed) != 0) {
        return -1;
    }

    *value = (uint8_t)parsed;
    return 0;
}

static int parse_layout(const char *text, avp_sample_layout_t *layout)
{
    if (text == NULL || layout == NULL) {
        return -1;
    }

    if (strcmp(text, "i") == 0 ||
        strcmp(text, "interleaved") == 0) {
        *layout = AVP_SAMPLE_LAYOUT_INTERLEAVED;
        return 0;
    }
    if (strcmp(text, "p") == 0 ||
        strcmp(text, "planar") == 0) {
        *layout = AVP_SAMPLE_LAYOUT_PLANAR;
        return 0;
    }

    return -1;
}

static void print_usage(const char *program)
{
    printf("usage: %s [input.wav] [output.pcm] [out_rate] [out_channels] [out_bits] [layout:i|p] [max_input_frames]\n",
           program);
}

static uint8_t audio_format_plane_count(const avp_audio_format_t *fmt)
{
    return fmt->sample_layout == AVP_SAMPLE_LAYOUT_PLANAR ? fmt->channels : 1u;
}

static void destroy_view(pcm_view_t *view)
{
    uint8_t ch;

    if (view == NULL) {
        return;
    }

    for (ch = 0u; ch < RESAMPLE_DEMO_MAX_PLANES; ch++) {
        if (view->planes[ch] != NULL) {
            avp_free(view->planes[ch]);
            view->planes[ch] = NULL;
        }
    }
    memset(view, 0, sizeof(*view));
}

static int reserve_view(pcm_view_t *view,
                        const avp_audio_format_t *fmt,
                        uint32_t samples)
{
    uint8_t plane_count;
    uint8_t bytes;
    uint8_t ch;

    if (view == NULL ||
        fmt == NULL ||
        fmt->channels == 0u ||
        fmt->channels > RESAMPLE_DEMO_MAX_PLANES) {
        return -1;
    }

    bytes = avp_swr_get_sample_bytes(fmt->bits_per_sample);
    if (bytes == 0u) {
        return -1;
    }

    plane_count = audio_format_plane_count(fmt);
    if (view->capacity_samples >= samples &&
        view->fmt.sample_rate == fmt->sample_rate &&
        view->fmt.bits_per_sample == fmt->bits_per_sample &&
        view->fmt.channels == fmt->channels &&
        view->fmt.sample_layout == fmt->sample_layout) {
        return 0;
    }

    destroy_view(view);
    view->fmt = *fmt;
    view->capacity_samples = samples;

    for (ch = 0u; ch < plane_count; ch++) {
        size_t plane_size;

        if (fmt->sample_layout == AVP_SAMPLE_LAYOUT_INTERLEAVED) {
            if (samples > UINT32_MAX / fmt->channels ||
                samples * fmt->channels > UINT32_MAX / bytes) {
                destroy_view(view);
                return -1;
            }
            plane_size = (size_t)samples * fmt->channels * bytes;
        } else {
            if (samples > UINT32_MAX / bytes) {
                destroy_view(view);
                return -1;
            }
            plane_size = (size_t)samples * bytes;
        }

        view->planes[ch] = (uint8_t *)avp_malloc(plane_size);
        if (view->planes[ch] == NULL) {
            destroy_view(view);
            return -1;
        }
    }

    return 0;
}

static void close_writer(pcm_writer_t *writer)
{
    uint8_t i;

    if (writer == NULL) {
        return;
    }

    for (i = 0u; i < RESAMPLE_DEMO_MAX_PLANES; i++) {
        if (writer->planes[i] != NULL) {
            fclose(writer->planes[i]);
        }
    }
    memset(writer, 0, sizeof(*writer));
}

static int open_writer(pcm_writer_t *writer,
                       const char *path,
                       const avp_audio_format_t *fmt)
{
    uint8_t ch;

    if (writer == NULL ||
        path == NULL ||
        fmt == NULL ||
        fmt->channels == 0u ||
        fmt->channels > RESAMPLE_DEMO_MAX_PLANES) {
        return -1;
    }

    memset(writer, 0, sizeof(*writer));
    writer->plane_count = audio_format_plane_count(fmt);

    if (fmt->sample_layout == AVP_SAMPLE_LAYOUT_INTERLEAVED) {
        writer->planes[0] = fopen(path, "wb");
        return writer->planes[0] == NULL ? -1 : 0;
    }

    for (ch = 0u; ch < fmt->channels; ch++) {
        char plane_path[RESAMPLE_DEMO_PATH_SIZE];
        int ret;

        ret = snprintf(plane_path,
                       sizeof(plane_path),
                       "%s.ch%u.pcm",
                       path,
                       (unsigned int)ch);
        if (ret < 0 || (size_t)ret >= sizeof(plane_path)) {
            close_writer(writer);
            return -1;
        }

        writer->planes[ch] = fopen(plane_path, "wb");
        if (writer->planes[ch] == NULL) {
            close_writer(writer);
            return -1;
        }
    }

    return 0;
}

static int write_output_packet(pcm_writer_t *writer,
                               const pcm_view_t *view,
                               uint32_t samples)
{
    uint8_t bytes;
    uint8_t ch;

    if (writer == NULL || view == NULL) {
        return -1;
    }

    bytes = avp_swr_get_sample_bytes(view->fmt.bits_per_sample);
    if (bytes == 0u) {
        return -1;
    }

    if (view->fmt.sample_layout == AVP_SAMPLE_LAYOUT_INTERLEAVED) {
        size_t size = (size_t)samples * view->fmt.channels * bytes;

        if (fwrite(view->planes[0], 1u, size, writer->planes[0]) != size) {
            return -1;
        }
    } else {
        size_t size = (size_t)samples * bytes;

        for (ch = 0u; ch < view->fmt.channels; ch++) {
            if (fwrite(view->planes[ch], 1u, size, writer->planes[ch]) != size) {
                return -1;
            }
        }
    }

    return 0;
}

static uint32_t get_packet_samples(const wav_demux_t *demuxer,
                                   const avp_packet_t *packet)
{
    uint16_t block_align;

    if (demuxer == NULL || packet == NULL) {
        return 0u;
    }

    block_align = demuxer->header.wave_fmt.BlockAlign;
    if (block_align == 0u) {
        return 0u;
    }

    return packet->size / block_align;
}

static int flush_resampler(avp_swr_t *swr,
                           const avp_audio_format_t *out_fmt,
                           pcm_view_t *output,
                           pcm_writer_t *writer,
                           uint32_t *total_out_samples,
                           clock_t *ticks)
{
    int converted;
    uint32_t flush_capacity;

    flush_capacity = avp_swr_get_out_samples(out_fmt->sample_rate,
                                             out_fmt->sample_rate,
                                             1024u);
    if (flush_capacity == 0u) {
        flush_capacity = 1024u;
    }

    if (reserve_view(output, out_fmt, flush_capacity) != 0) {
        printf("alloc flush output failed: %u frames, free %u\n",
               (unsigned int)flush_capacity,
               (unsigned int)avp_mem_free_size());
        return -1;
    }

    for (;;) {
        clock_t flush_start = clock();
        converted = avp_swr_convert(swr,
                                    NULL,
                                    0u,
                                    (void *const *)output->planes,
                                    flush_capacity);
        *ticks += clock() - flush_start;
        if (converted < 0) {
            printf("flush failed: %d\n", converted);
            return -1;
        }
        if (converted == 0) {
            break;
        }
        if (write_output_packet(writer, output, (uint32_t)converted) != 0) {
            printf("write flush output failed\n");
            return -1;
        }
        *total_out_samples += (uint32_t)converted;
    }

    return 0;
}

int main(int argc, char **argv)
{
    const char *input_path = DEFAULT_INPUT_WAV;
    const char *output_path = DEFAULT_OUTPUT_PCM;
    avp_audio_format_t in_fmt;
    avp_audio_format_t out_fmt;
    uint32_t max_input_frames = 0u;
    FILE *fp = NULL;
    avp_io_t io;
    wav_demux_t demuxer;
    avp_packet_t *packet = NULL;
    avp_swr_t *swr = NULL;
    pcm_view_t output;
    pcm_writer_t writer;
    uint32_t total_in_samples = 0u;
    uint32_t total_out_samples = 0u;
    uint32_t packet_count = 0u;
    uint32_t free_memory = 0u;
    clock_t total_convert_ticks = 0;
    avp_status_t st;
    int ret = 1;

    memset(&io, 0, sizeof(io));
    memset(&demuxer, 0, sizeof(demuxer));
    memset(&in_fmt, 0, sizeof(in_fmt));
    memset(&out_fmt, 0, sizeof(out_fmt));
    memset(&output, 0, sizeof(output));
    memset(&writer, 0, sizeof(writer));

    out_fmt.sample_rate = DEFAULT_OUT_SAMPLE_RATE;
    out_fmt.channels = DEFAULT_OUT_CHANNELS;
    out_fmt.bits_per_sample = DEFAULT_OUT_BITS;
    out_fmt.sample_layout = DEFAULT_OUT_LAYOUT;

    avp_mem_init();

    if (argc > 8) {
        print_usage(argv[0]);
        return 1;
    }
    if (argc >= 2) {
        input_path = argv[1];
    }
    if (argc >= 3) {
        output_path = argv[2];
    }
    if (argc >= 4 &&
        parse_u32(argv[3], 1u, UINT32_MAX, &out_fmt.sample_rate) != 0) {
        printf("invalid out_rate: %s\n", argv[3]);
        return 1;
    }
    if (argc >= 5 &&
        parse_u8(argv[4], 1u, RESAMPLE_DEMO_MAX_PLANES, &out_fmt.channels) != 0) {
        printf("invalid out_channels: %s\n", argv[4]);
        return 1;
    }
    if (argc >= 6) {
        uint32_t out_bits;

        if (parse_u32(argv[5], 8u, 32u, &out_bits) != 0) {
            printf("invalid out_bits: %s\n", argv[5]);
            return 1;
        }
        out_fmt.bits_per_sample = (uint16_t)out_bits;
    }
    if (avp_swr_get_sample_bytes(out_fmt.bits_per_sample) == 0u) {
        printf("unsupported out_bits: %u\n", (unsigned int)out_fmt.bits_per_sample);
        return 1;
    }
    if (argc >= 7 &&
        parse_layout(argv[6], &out_fmt.sample_layout) != 0) {
        printf("invalid layout: %s\n", argv[6]);
        return 1;
    }
    if (argc >= 8 &&
        parse_u32(argv[7], 0u, UINT32_MAX, &max_input_frames) != 0) {
        printf("invalid max_input_frames: %s\n", argv[7]);
        return 1;
    }

    fp = fopen(input_path, "rb");
    if (fp == NULL) {
        printf("open %s failed\n", input_path);
        goto out;
    }

    avp_io_init(&io, file_read_cb, NULL, file_seek_cb, file_get_size_cb, fp);
    st = wav_demux_open(&demuxer, &io);
    if (st != AVP_OK) {
        printf("wav_demux_open failed: %d\n", (int)st);
        goto out;
    }

    if (demuxer.header.wave_fmt.AudioFormat != WAV_AUDIO_FORMAT_PCM ||
        demuxer.header.wave_fmt.BlockAlign == 0u ||
        demuxer.header.wave_fmt.NumChannels == 0u ||
        demuxer.header.wave_fmt.NumChannels > RESAMPLE_DEMO_MAX_PLANES ||
        avp_swr_get_sample_bytes(demuxer.header.wave_fmt.BitsPerSample) == 0u) {
        printf("input must be 8/16/32-bit PCM WAV with 1-%u channels\n",
               (unsigned int)RESAMPLE_DEMO_MAX_PLANES);
        goto out;
    }

    in_fmt.sample_rate = demuxer.header.wave_fmt.SampleRate;
    in_fmt.channels = (uint8_t)demuxer.header.wave_fmt.NumChannels;
    in_fmt.bits_per_sample = demuxer.header.wave_fmt.BitsPerSample;
    in_fmt.sample_layout = AVP_SAMPLE_LAYOUT_INTERLEAVED;

    free_memory = avp_mem_free_size();
    printf("resample demo start: free memory=%u bytes\n", (unsigned int)free_memory);

    swr = avp_swr_open(&in_fmt, &out_fmt);
    if (swr == NULL) {
        printf("avp_swr_open failed\n");
        goto out;
    }

    if (open_writer(&writer, output_path, &out_fmt) != 0) {
        printf("open output %s failed\n", output_path);
        goto out;
    }

    packet = avp_packet_alloc(demuxer.header.wave_fmt.ByteRate / 100u);
    if (packet == NULL) {
        printf("alloc packet failed, free %u\n", (unsigned int)avp_mem_free_size());
        goto out;
    }

    printf("input: %s, %u Hz, %u bit, %u ch, interleaved\n",
           input_path,
           (unsigned int)in_fmt.sample_rate,
           (unsigned int)in_fmt.bits_per_sample,
           (unsigned int)in_fmt.channels);
    printf("output: %s, %u Hz, %u bit, %u ch, %s\n",
           output_path,
           (unsigned int)out_fmt.sample_rate,
           (unsigned int)out_fmt.bits_per_sample,
           (unsigned int)out_fmt.channels,
           layout_name(out_fmt.sample_layout));

    for (;;) {
        const void *in_planes[1];
        uint32_t in_samples;
        uint32_t out_capacity;
        int converted;

        st = wav_demux_read_packet(&demuxer, packet);
        if (st == AVP_ENOENT) {
            break;
        }
        if (st != AVP_OK) {
            printf("wav_demux_read_packet failed: %d\n", (int)st);
            goto out;
        }

        in_samples = get_packet_samples(&demuxer, packet);
        if (in_samples == 0u) {
            continue;
        }

        if (max_input_frames != 0u &&
            total_in_samples + in_samples > max_input_frames) {
            in_samples = max_input_frames - total_in_samples;
            packet->size = in_samples * demuxer.header.wave_fmt.BlockAlign;
        }

        out_capacity = avp_swr_get_out_samples(in_fmt.sample_rate,
                                               out_fmt.sample_rate,
                                               in_samples);
        if (out_capacity == 0u ||
            reserve_view(&output, &out_fmt, out_capacity) != 0) {
            printf("alloc output failed: %u frames, free %u\n",
                   (unsigned int)out_capacity,
                   (unsigned int)avp_mem_free_size());
            goto out;
        }

        in_planes[0] = packet->buf;
        {
            clock_t convert_start = clock();
            converted = avp_swr_convert(swr,
                                        in_planes,
                                        in_samples,
                                        (void *const *)output.planes,
                                        out_capacity);
            total_convert_ticks += clock() - convert_start;
        }
        if (converted < 0 || (uint32_t)converted > out_capacity) {
            printf("convert packet %u failed: %d max %u\n",
                   (unsigned int)packet->index,
                   converted,
                   (unsigned int)out_capacity);
            goto out;
        }

        if (write_output_packet(&writer, &output, (uint32_t)converted) != 0) {
            printf("write output packet %u failed\n", (unsigned int)packet->index);
            goto out;
        }

        total_in_samples += in_samples;
        total_out_samples += (uint32_t)converted;
        packet_count++;

        if (max_input_frames != 0u && total_in_samples >= max_input_frames) {
            break;
        }
    }

    if (flush_resampler(swr, &out_fmt, &output, &writer, &total_out_samples, &total_convert_ticks) != 0) {
        goto out;
    }

    {
        uint32_t audio_duration_ms = (in_fmt.sample_rate > 0u) ?
                                     (total_in_samples * 1000u / in_fmt.sample_rate) : 0u;
        double process_time_ms = ((double)total_convert_ticks * 1000.0) / (double)1000000;
        double cpu_usage = audio_duration_ms == 0u ? 0.0 :
                           (process_time_ms * 100.0) / (double)audio_duration_ms;
        printf("resample done: packets=%u in_frames=%u out_frames=%u "
               "memory_used=%u bytes, "
               "audio_duration=%u ms, "
               "avp_swr_convert time=%.2f ms, "
               "cpu_usage=%.2f%%\n",
               (unsigned int)packet_count,
               (unsigned int)total_in_samples,
               (unsigned int)total_out_samples,
               (unsigned int)(free_memory - avp_mem_free_size()),
               (unsigned int)audio_duration_ms,
               process_time_ms,
               cpu_usage);
    }
    ret = 0;

out:
    close_writer(&writer);
    destroy_view(&output);
    avp_packet_free(packet);
    if (swr != NULL) {
        avp_swr_close(swr);
    }
    wav_demux_close(&demuxer);
    if (fp != NULL) {
        fclose(fp);
    }
    return ret;
}
