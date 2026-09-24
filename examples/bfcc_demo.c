/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tlsf_port.h"
#include "avp_bfcc.h"

#define DEFAULT_INPUT      "jinitaimei_bfcc_16k.pcm"
#define DEFAULT_OUTPUT     "bfcc_output.bin"
#define PCM_SAMPLE_RATE    16000u
#define PCM_BUFFER_SAMPLES 4096u
#define BFCC_NUM_COEFFS    22u

static void print_usage(const char *program)
{
    printf("usage: %s [input.pcm] [output.bin]\n", program);
    printf("output: raw float32, one row per frame [%u coeffs]\n",
           (unsigned int)BFCC_NUM_COEFFS);
}

int main(int argc, char **argv)
{
    const char *input_path = DEFAULT_INPUT;
    const char *output_path = DEFAULT_OUTPUT;
    FILE *input_fp = NULL;
    FILE *output_fp = NULL;
    int16_t pcm_buffer[PCM_BUFFER_SAMPLES];
    avp_bfcc_t *bfcc = NULL;
    avp_bfcc_config_t config;
    avp_bfcc_frame_t frame;
    uint32_t step_samples;
    uint32_t sample_read;
    uint32_t frame_count;
    uint32_t processed_frames = 0u;
    uint32_t processed_samples = 0u;
    int ret = 1;

    if (argc > 3) {
        print_usage(argv[0]);
        return 1;
    }
    if (argc > 1) {
        input_path = argv[1];
    }
    if (argc > 2) {
        output_path = argv[2];
    }

    memset(&config, 0, sizeof(config));
    config.sample_rate = PCM_SAMPLE_RATE;
    config.window.size_ms = 10u;
    config.window.step_size_ms = 10u;
    config.filterbank.num_channels = 22u;
    config.filterbank.lower_band_limit = 80u;
    config.filterbank.upper_band_limit = 7600u;
    config.num_coefficients = BFCC_NUM_COEFFS;
    frame_count = avp_bfcc_get_1s_frame_count(&config);
    if (frame_count == 0u) {
        printf("invalid BFCC frame configuration\n");
        return 1;
    }

    avp_mem_init();
    if (avp_bfcc_open(&config, &bfcc) != AVP_OK) {
        printf("avp_bfcc_open failed\n");
        return 1;
    }

    step_samples = avp_bfcc_get_step_samples(bfcc);
    if (step_samples == 0u || step_samples > PCM_BUFFER_SAMPLES) {
        printf("invalid bfcc step samples: %u\n", (unsigned int)step_samples);
        goto out;
    }

    input_fp = fopen(input_path, "rb");
    if (input_fp == NULL) {
        printf("open input failed: %s\n", input_path);
        goto out;
    }
    output_fp = fopen(output_path, "wb");
    if (output_fp == NULL) {
        printf("open output failed: %s\n", output_path);
        goto out;
    }

    while (processed_samples < config.sample_rate) {
        uint32_t samples_left = config.sample_rate - processed_samples;
        uint32_t read_limit = samples_left > step_samples ? step_samples : samples_left;
        size_t read_samples = fread(pcm_buffer, sizeof(pcm_buffer[0]),
                                    read_limit, input_fp);
        if (read_samples == 0u) {
            break;
        }
        processed_samples += (uint32_t)read_samples;
        if (avp_bfcc_process(bfcc, pcm_buffer, (uint32_t)read_samples,
                             &sample_read, &frame) != AVP_OK ||
            sample_read != (uint32_t)read_samples) {
            printf("avp_bfcc_process failed\n");
            goto out;
        }
        if (frame.bfcc_size > 0u) {
            if (frame.bfcc_size != BFCC_NUM_COEFFS) {
                printf("unexpected bfcc size: %u/%u\n",
                       (unsigned int)frame.bfcc_size,
                       (unsigned int)BFCC_NUM_COEFFS);
                goto out;
            }
            if (fwrite(frame.bfcc_value, sizeof(frame.bfcc_value[0]),
                       frame.bfcc_size, output_fp) != frame.bfcc_size) {
                printf("write output failed\n");
                goto out;
            }
            processed_frames++;
        }
    }

    if (processed_samples != config.sample_rate || processed_frames != frame_count) {
        printf("unexpected BFCC result: %u frames/%u, %u samples/%u\n",
               (unsigned int)processed_frames, (unsigned int)frame_count,
               (unsigned int)processed_samples, (unsigned int)config.sample_rate);
        goto out;
    }

    printf("bfcc done: %u frames x %u coeffs, input=%u samples, output=%s\n",
           (unsigned int)frame_count, (unsigned int)BFCC_NUM_COEFFS,
           (unsigned int)processed_samples, output_path);
    ret = 0;

out:
    if (output_fp != NULL) {
        fclose(output_fp);
    }
    if (input_fp != NULL) {
        fclose(input_fp);
    }
    avp_bfcc_close(bfcc);
    return ret;
}
