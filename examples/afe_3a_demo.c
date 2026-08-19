/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Generate test PCM files with ffmpeg, for example:
 *
 *   ffmpeg -stream_loop -1 -i jinitaimei.wav \
 *     -f lavfi -i "sine=frequency=440:duration=5" \
 *     -f lavfi -i "anoisesrc=color=pink:duration=5:amplitude=0.08" \
 *     -filter_complex "[0:a]aresample=16000,pan=mono|c0=0.5*c0+0.5*c1,atrim=duration=5,asetpts=PTS-STARTPTS[voice];[1:a]asplit=2[far][farecho];[farecho]aecho=0.9:0.95:40|80|120:0.5|0.35|0.2[echoed];[voice][echoed][2:a]amix=inputs=3:duration=first[near]" \
 *     -map "[far]" -ar 16000 -ac 1 -f s16le far.pcm \
 *     -map "[near]" -ar 16000 -ac 1 -f s16le near.pcm
 *
 * Then run:
 *   avp_afe_3a_demo near.pcm far.pcm output.pcm 16000
 */
#include "avp_afe_3a.h"
#include "avp_config.h"
#include "tlsf_port.h"

#include <time.h>

static int parse_u32(const char *text, uint32_t min, uint32_t max, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed;

    if (text == NULL || value == NULL || text[0] == '\0') {
        return -1;
    }

    parsed = strtoul(text, &end, 0);
    if (end == text || *end != '\0' ||
        parsed < min || parsed > max) {
        return -1;
    }

    *value = (uint32_t)parsed;
    return 0;
}

static int is_valid_sample_rate(uint32_t sample_rate)
{
    return sample_rate == 8000u ||
           sample_rate == 16000u ||
           sample_rate == 32000u;
}

static void print_usage(const char *program)
{
    printf("usage: %s <near.pcm> <far.pcm> <output.pcm> "
           "[sample_rate:8000|16000|32000] [frame_count]\n",
           program);
}

static int run_demo(const char *near_path,
                    const char *far_path,
                    const char *output_path,
                    uint32_t sample_rate,
                    uint32_t frame_count)
{
    FILE *near_fp = NULL;
    FILE *far_fp = NULL;
    FILE *out_fp = NULL;
    avp_afe_3a_t *afe = NULL;
    int16_t *near_frame = NULL;
    int16_t *far_frame = NULL;
    int16_t *out_frame = NULL;
    uint32_t frame_samples;
    uint32_t processed_frames = 0u;
    uint32_t processed_samples = 0u;
    uint32_t free_memory;
    int last_vad = -1;
    int last_echo = 0;
    clock_t total_process_ticks = 0;
    avp_afe_3a_config_t config;
    int ret = 1;

    memset(&config, 0, sizeof(config));

    near_fp = fopen(near_path, "rb");
    if (near_fp == NULL) {
        printf("open %s failed\n", near_path);
        goto out;
    }
    far_fp = fopen(far_path, "rb");
    if (far_fp == NULL) {
        printf("open %s failed\n", far_path);
        goto out;
    }
    out_fp = fopen(output_path, "wb");
    if (out_fp == NULL) {
        printf("open %s failed\n", output_path);
        goto out;
    }

    frame_samples = sample_rate * AVP_AFE_3A_FRAME_MS / 1000u;
    near_frame = (int16_t *)avp_malloc((size_t)frame_samples * sizeof(int16_t));
    far_frame = (int16_t *)avp_malloc((size_t)frame_samples * sizeof(int16_t));
    out_frame = (int16_t *)avp_malloc((size_t)frame_samples * sizeof(int16_t));
    if (near_frame == NULL || far_frame == NULL || out_frame == NULL) {
        printf("alloc pcm buffers failed\n");
        goto out;
    }

    free_memory = avp_mem_free_size();
    printf("afe demo start: free memory=%u bytes\n", (unsigned int)free_memory);

    config.sample_rate = sample_rate;
    config.enable_aec = 1u;
    config.enable_ns = 1u;
    config.enable_agc = 1u;
    config.enable_vad = 1u;
    config.aec_nlp_mode = AVP_AFE_3A_AEC_NLP_MODERATE;
    config.ns_policy = AVP_AFE_3A_NS_POLICY_MEDIUM;
    config.agc_mode = AVP_AFE_3A_AGC_MODE_ADAPTIVE_DIGITAL;
    config.agc_config.target_level_dbfs = 3;
    config.agc_config.compression_gain_db = 9;
    config.agc_config.limiter_enable = 1u;
    config.vad_mode = AVP_AFE_3A_VAD_MODE_LOW_BITRATE;
    config.stream_delay_ms = AVP_AFE_3A_STREAM_DELAY_NONE;

    if (avp_afe_3a_open(&config, &afe) != AVP_OK) {
        printf("avp_afe_3a_open failed\n");
        goto out;
    }

    if (avp_afe_3a_get_frame_samples(afe) != frame_samples) {
        printf("avp_afe_3a_get_frame_samples failed\n");
        goto out;
    }

    while (frame_count == 0u || processed_frames < frame_count) {
        size_t near_read;
        size_t far_read;
        const int16_t *far_input = far_frame;
        avp_status_t st;

        near_read = fread(near_frame, sizeof(int16_t), frame_samples, near_fp);
        far_read = fread(far_frame, sizeof(int16_t), frame_samples, far_fp);

        if (near_read == 0u) {
            break;
        }
        if (near_read < frame_samples) {
            printf("near pcm ended with %u/%u samples, stop\n",
                   (unsigned int)near_read,
                   (unsigned int)frame_samples);
            break;
        }

        if (far_read < frame_samples) {
            if (far_read != 0u) {
                printf("far pcm ended with %u/%u samples; "
                       "remaining frames bypass AEC\n",
                       (unsigned int)far_read,
                       (unsigned int)frame_samples);
            }
            far_input = NULL;
        }

        clock_t process_start = clock();
        st = avp_afe_3a_process(afe, near_frame, far_input, out_frame, &last_vad);
        total_process_ticks += clock() - process_start;

        if (st != AVP_OK) {
            printf("avp_afe_3a_process failed at frame %u: %d\n",
                   (unsigned int)processed_frames,
                   (int)st);
            goto out;
        }

        if (fwrite(out_frame, sizeof(int16_t), frame_samples, out_fp) != frame_samples) {
            printf("write %s failed\n", output_path);
            goto out;
        }

        processed_frames++;
        processed_samples += frame_samples;

        if ((processed_frames % 50u) == 0u) {
            avp_afe_3a_control(afe, AVP_AFE_3A_CMD_GET_ECHO_STATUS, &last_echo);
            printf("frames=%u samples=%u vad=%d echo=%d\n",
                   (unsigned int)processed_frames,
                   (unsigned int)processed_samples,
                   last_vad,
                   last_echo);
        }
    }

    avp_afe_3a_control(afe, AVP_AFE_3A_CMD_GET_ECHO_STATUS, &last_echo);

    uint32_t total_audio_ms = processed_frames * AVP_AFE_3A_FRAME_MS;
    double process_time_ms = ((double)total_process_ticks * 1000.0) / (double)1000000;
    double cpu_usage = total_audio_ms == 0u ? 0.0 :
                                              (process_time_ms * 100.0) / (double)total_audio_ms;
    printf("afe demo done: frames=%u samples=%u vad=%d echo=%d "
           "memory_used=%u bytes, "
           "audio_duration=%u ms, "
           "avp_afe_3a_process time=%.2f ms, "
           "cpu_usage=%.2f%%\n",
           (unsigned int)processed_frames,
           (unsigned int)processed_samples,
           last_vad,
           last_echo,
           (unsigned int)(free_memory - avp_mem_free_size()),
           (unsigned int)total_audio_ms,
           process_time_ms,
           cpu_usage);

    ret = 0;

out:
    if (afe != NULL) {
        avp_afe_3a_close(afe);
    }
    if (near_frame != NULL) {
        avp_free(near_frame);
    }
    if (far_frame != NULL) {
        avp_free(far_frame);
    }
    if (out_frame != NULL) {
        avp_free(out_frame);
    }
    if (out_fp != NULL && fclose(out_fp) != 0 && ret == 0) {
        ret = 1;
    }
    if (far_fp != NULL) {
        fclose(far_fp);
    }
    if (near_fp != NULL) {
        fclose(near_fp);
    }

    return ret;
}

int main(int argc, char **argv)
{
    uint32_t sample_rate = 16000u;
    uint32_t frame_count = 0u;

    avp_mem_init();

    if (argc < 4 || argc > 6) {
        print_usage(argv[0]);
        return 1;
    }

    if (argc >= 5 && parse_u32(argv[4], 1u, 32000u, &sample_rate) != 0) {
        print_usage(argv[0]);
        return 1;
    }
    if (!is_valid_sample_rate(sample_rate)) {
        printf("invalid sample rate: %u\n", (unsigned int)sample_rate);
        return 1;
    }

    if (argc >= 6 && parse_u32(argv[5], 0u, UINT32_MAX, &frame_count) != 0) {
        print_usage(argv[0]);
        return 1;
    }

    return run_demo(argv[1], argv[2], argv[3], sample_rate, frame_count);
}
