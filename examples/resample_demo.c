/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "avp_swr.h"
#include "tlsf_port.h"

#include <time.h>

#define DEFAULT_INPUT_PCM       "jinitaimei.pcm"
#define DEFAULT_OUTPUT_PCM      "resample_out.pcm"
#define DEFAULT_OUT_SAMPLE_RATE 16000u

#define INPUT_SAMPLE_RATE 44100u
#define INPUT_CHANNELS    2u
#define INPUT_BITS        16u
#define INPUT_FRAMES_PER_READ 4096u

typedef enum {
    RESAMPLE_MODE_RATE = 0,
    RESAMPLE_MODE_BITS,
    RESAMPLE_MODE_CHANNELS,
} resample_mode_t;

static int parse_u32(const char *text, uint32_t min, uint32_t max, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed;

    if (text == NULL || value == NULL) {
        return -1;
    }

    parsed = strtoul(text, &end, 0);
    if (end == text || *end != '\0' || parsed < min || parsed > max) {
        return -1;
    }

    *value = (uint32_t)parsed;
    return 0;
}

static int parse_mode(const char *text, resample_mode_t *mode)
{
    if (text == NULL || mode == NULL) {
        return -1;
    }

    if (strcmp(text, "r") == 0 || strcmp(text, "rate") == 0) {
        *mode = RESAMPLE_MODE_RATE;
        return 0;
    }
    if (strcmp(text, "b") == 0 || strcmp(text, "bit") == 0 || strcmp(text, "bits") == 0) {
        *mode = RESAMPLE_MODE_BITS;
        return 0;
    }
    if (strcmp(text, "c") == 0 || strcmp(text, "ch") == 0 ||
        strcmp(text, "channel") == 0 || strcmp(text, "channels") == 0) {
        *mode = RESAMPLE_MODE_CHANNELS;
        return 0;
    }

    return -1;
}

static const char *mode_name(resample_mode_t mode)
{
    switch (mode) {
        case RESAMPLE_MODE_RATE:
            return "rate";
        case RESAMPLE_MODE_BITS:
            return "bit";
        case RESAMPLE_MODE_CHANNELS:
            return "channel";
        default:
            return "unknown";
    }
}

static int validate_output_value(resample_mode_t mode, uint32_t value)
{
    switch (mode) {
        case RESAMPLE_MODE_RATE:
            return value == 8000u || value == 16000u ||
                   value == 32000u || value == 48000u;
        case RESAMPLE_MODE_BITS:
            return value == 8u || value == 24u || value == 32u;
        case RESAMPLE_MODE_CHANNELS:
            return value == 1u || value == 3u || value == 4u;
        default:
            return 0;
    }
}

static int convert_once(const avp_audio_format_t *in_fmt,
                        const avp_audio_format_t *out_fmt,
                        avp_sw_sample_t in[],
                        uint32_t in_samples,
                        avp_sw_sample_t out[],
                        uint32_t *out_samples)
{
    uint8_t out_bytes;

    if (in_fmt == NULL || out_fmt == NULL || in == NULL || out == NULL || out_samples == NULL) {
        return -1;
    }

    out_bytes = avp_swr_get_sample_bytes(out_fmt->bits_per_sample);
    if (out_bytes == 0u) {
        return -1;
    }

    if (in_fmt->sample_rate != out_fmt->sample_rate) {
        if (avp_sample_rate_convert_interleaved(in_fmt->sample_rate,
                                                out_fmt->sample_rate,
                                                in_fmt->channels,
                                                in_fmt->bits_per_sample,
                                                in[0],
                                                in_samples,
                                                out[0],
                                                out_samples) != AVP_OK) {
            return -1;
        }
        return 0;
    }

    if (in_fmt->bits_per_sample != out_fmt->bits_per_sample) {
        if (avp_bits_convert_interleaved(in_fmt->bits_per_sample,
                                         out_fmt->bits_per_sample,
                                         in_fmt->channels,
                                         in[0],
                                         out[0],
                                         in_samples) != AVP_OK) {
            return -1;
        }
        *out_samples = in_samples;
        return 0;
    }

    if (in_fmt->channels != out_fmt->channels) {
        if (avp_channel_convert_interleaved(in_fmt->channels,
                                            out_fmt->channels,
                                            in_fmt->bits_per_sample,
                                            in[0],
                                            out[0],
                                            in_samples) != AVP_OK) {
            return -1;
        }
        *out_samples = in_samples;
        return 0;
    }

    memcpy(out[0], in[0], (size_t)in_samples * in_fmt->channels * out_bytes);
    *out_samples = in_samples;
    return 0;
}

static void print_usage(const char *program)
{
    printf("usage: %s [input.pcm] [output.pcm] [mode:r|b|c] [value] [max_input_frames]\n",
           program);
    printf("input format: %u Hz, %u-bit signed interleaved PCM, %u channels\n",
           (unsigned int)INPUT_SAMPLE_RATE,
           (unsigned int)INPUT_BITS,
           (unsigned int)INPUT_CHANNELS);
    printf("rate: 8000, 16000, 32000, 48000\n");
    printf("bit : 8, 24, 32\n");
    printf("ch  : 1, 3, 4\n");
}

int main(int argc, char **argv)
{
    const char *input_path = DEFAULT_INPUT_PCM;
    const char *output_path = DEFAULT_OUTPUT_PCM;
    resample_mode_t mode = RESAMPLE_MODE_RATE;
    uint32_t out_value = DEFAULT_OUT_SAMPLE_RATE;
    avp_audio_format_t in_fmt;
    avp_audio_format_t out_fmt;
    avp_sw_sample_t input_planes[1];
    avp_sw_sample_t output_planes[1];
    uint8_t input_buffer[INPUT_FRAMES_PER_READ * INPUT_CHANNELS * (INPUT_BITS / 8u)];
    uint32_t output_capacity_samples;
    uint8_t output_bytes_per_frame;
    uint32_t max_input_frames = 0u;
    uint32_t total_in_samples = 0u;
    uint32_t total_out_samples = 0u;
    uint32_t block_count = 0u;
    uint32_t free_memory = 0u;
    clock_t total_convert_ticks = 0;
    FILE *input_fp = NULL;
    FILE *output_fp = NULL;
    int ret = 1;

    if (argc > 6) {
        print_usage(argv[0]);
        return 1;
    }
    if (argc >= 2) {
        input_path = argv[1];
    }
    if (argc >= 3) {
        output_path = argv[2];
    }
    if (argc >= 4 && parse_mode(argv[3], &mode) != 0) {
        printf("invalid mode: %s\n", argv[3]);
        return 1;
    }
    if (argc >= 5 && parse_u32(argv[4], 1u, UINT32_MAX, &out_value) != 0) {
        printf("invalid value: %s\n", argv[4]);
        return 1;
    }
    if (argc >= 6 && parse_u32(argv[5], 0u, UINT32_MAX, &max_input_frames) != 0) {
        printf("invalid max_input_frames: %s\n", argv[5]);
        return 1;
    }

    if (!validate_output_value(mode, out_value)) {
        printf("unsupported %s value: %u\n", mode_name(mode), (unsigned int)out_value);
        return 1;
    }

    memset(&in_fmt, 0, sizeof(in_fmt));
    memset(&out_fmt, 0, sizeof(out_fmt));
    memset(input_planes, 0, sizeof(input_planes));
    memset(output_planes, 0, sizeof(output_planes));

    in_fmt.sample_rate = INPUT_SAMPLE_RATE;
    in_fmt.channels = INPUT_CHANNELS;
    in_fmt.bits_per_sample = INPUT_BITS;
    in_fmt.sample_layout = AVP_SAMPLE_LAYOUT_INTERLEAVED;
    out_fmt = in_fmt;
    switch (mode) {
        case RESAMPLE_MODE_RATE:
            out_fmt.sample_rate = out_value;
            break;
        case RESAMPLE_MODE_BITS:
            out_fmt.bits_per_sample = (uint16_t)out_value;
            break;
        case RESAMPLE_MODE_CHANNELS:
            out_fmt.channels = (uint8_t)out_value;
            break;
        default:
            return 1;
    }

    output_bytes_per_frame = (uint8_t)(out_fmt.channels * avp_swr_get_sample_bytes(out_fmt.bits_per_sample));
    output_capacity_samples = (mode == RESAMPLE_MODE_RATE)
                              ? avp_swr_get_out_samples(INPUT_SAMPLE_RATE,
                                                        out_fmt.sample_rate,
                                                        INPUT_FRAMES_PER_READ)
                              : INPUT_FRAMES_PER_READ;
    if (output_capacity_samples == 0u || output_bytes_per_frame == 0u) {
        printf("invalid output format\n");
        return 1;
    }

    avp_mem_init();
    output_planes[0] = avp_malloc((size_t)output_capacity_samples * output_bytes_per_frame);
    if (output_planes[0] == NULL) {
        printf("allocate output buffer failed\n");
        goto out;
    }

    input_fp = fopen(input_path, "rb");
    if (input_fp == NULL) {
        printf("open %s failed\n", input_path);
        goto out;
    }

    output_fp = fopen(output_path, "wb");
    if (output_fp == NULL) {
        printf("open %s failed\n", output_path);
        goto out;
    }

    free_memory = avp_mem_free_size();
    printf("resample demo start: free memory=%u bytes\n", (unsigned int)free_memory);
    printf("input: %s, %u Hz, %u bit, %u ch, interleaved PCM\n",
           input_path,
           (unsigned int)in_fmt.sample_rate,
           (unsigned int)in_fmt.bits_per_sample,
           (unsigned int)in_fmt.channels);
    printf("output: %s, mode=%s, value=%u, %u Hz, %u bit, %u ch, interleaved PCM\n",
           output_path,
           mode_name(mode),
           (unsigned int)out_value,
           (unsigned int)out_fmt.sample_rate,
           (unsigned int)out_fmt.bits_per_sample,
           (unsigned int)out_fmt.channels);

    for (;;) {
        size_t bytes_read;
        uint32_t in_samples;
        uint32_t out_samples;
        clock_t convert_start;

        bytes_read = fread(input_buffer, 1u, sizeof(input_buffer), input_fp);
        bytes_read -= bytes_read % (INPUT_CHANNELS * (INPUT_BITS / 8u));
        if (bytes_read == 0u) {
            break;
        }

        in_samples = (uint32_t)(bytes_read / (INPUT_CHANNELS * (INPUT_BITS / 8u)));
        if (max_input_frames != 0u && total_in_samples + in_samples > max_input_frames) {
            in_samples = max_input_frames - total_in_samples;
        }
        if (in_samples == 0u) {
            break;
        }

        input_planes[0] = input_buffer;
        convert_start = clock();
        if (convert_once(&in_fmt, &out_fmt, input_planes, in_samples,
                         output_planes, &out_samples) != 0) {
            printf("convert block %u failed\n", (unsigned int)block_count);
            goto out;
        }
        total_convert_ticks += clock() - convert_start;

        if (fwrite(output_planes[0], 1u,
                   (size_t)out_samples * output_bytes_per_frame, output_fp) !=
            (size_t)out_samples * output_bytes_per_frame) {
            printf("write output block %u failed\n", (unsigned int)block_count);
            goto out;
        }

        total_in_samples += in_samples;
        total_out_samples += out_samples;
        block_count++;
        if (max_input_frames != 0u && total_in_samples >= max_input_frames) {
            break;
        }
    }

    {
        uint32_t audio_duration_ms = total_in_samples * 1000u / in_fmt.sample_rate;
        double process_time_ms = ((double)total_convert_ticks * 1000.0) / (double)CLOCKS_PER_SEC;
        double cpu_usage = audio_duration_ms == 0u ? 0.0 :
                                                     (process_time_ms * 100.0) /
                                                         (double)audio_duration_ms;

        printf("resample done: blocks=%u in_frames=%u out_frames=%u memory_used=%u bytes, "
               "audio_duration=%u ms, convert time=%.2f ms, cpu_usage=%.2f%%\n",
               (unsigned int)block_count,
               (unsigned int)total_in_samples,
               (unsigned int)total_out_samples,
               (unsigned int)(free_memory - avp_mem_free_size()),
               (unsigned int)audio_duration_ms,
               process_time_ms,
               cpu_usage);
    }

    ret = 0;

out:
    if (output_fp != NULL) {
        fclose(output_fp);
    }
    if (input_fp != NULL) {
        fclose(input_fp);
    }
    if (output_planes[0] != NULL) {
        avp_free(output_planes[0]);
    }
    return ret;
}
