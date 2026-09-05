/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tlsf_port.h"
#include "avp_mfcc.h"

#define DEFAULT_INPUT      "jinitaimei_mfcc_16k.pcm"
#define DEFAULT_OUTPUT     "mfcc_output.bin"
#define PCM_SAMPLE_RATE    16000u
#define PCM_BUFFER_SAMPLES 4096u
#define MFCC_NUM_FRAMES    49u
#define MFCC_NUM_COEFFS    13u

static void print_usage(const char *program)
{
    printf("usage: %s [input.pcm] [output.bin]\n", program);
    printf("output: raw float32, row-major [%u][%u]\n",
           (unsigned int)MFCC_NUM_FRAMES, (unsigned int)MFCC_NUM_COEFFS);
}

static int save_mfcc_output(FILE *output,
                            const float mfcc[MFCC_NUM_FRAMES][MFCC_NUM_COEFFS])
{
    size_t count;

    if (output == NULL || mfcc == NULL) {
        return -1;
    }
    count = fwrite(mfcc, sizeof(float), MFCC_NUM_FRAMES * MFCC_NUM_COEFFS, output);
    return count == MFCC_NUM_FRAMES * MFCC_NUM_COEFFS ? 0 : -1;
}

int main(int argc, char **argv)
{
    const char *input_path = DEFAULT_INPUT;
    const char *output_path = DEFAULT_OUTPUT;
    FILE *input_fp = NULL;
    FILE *output_fp = NULL;
    int16_t pcm_buffer[PCM_BUFFER_SAMPLES];
    avp_mfcc_t *mfcc = NULL;
    avp_mfcc_config_t config;
    avp_mfcc_frame_t frame;
    float mfcc_output[MFCC_NUM_FRAMES][MFCC_NUM_COEFFS];
    uint32_t step_samples;
    uint32_t sample_read = 0u;
    uint32_t mfcc_count = 0;
    uint32_t input_samples = 0u;
    int st;
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
    config.window.size_ms = 30;
    config.window.step_size_ms = 20;
    config.filterbank.num_channels = 40u;
    config.filterbank.lower_band_limit = 125u;
    config.filterbank.upper_band_limit = 7500u;
    config.num_coefficients = MFCC_NUM_COEFFS;

    avp_mem_init();

    if (avp_mfcc_open(&config, &mfcc) != AVP_OK) {
        printf("avp_mfcc_open failed\n");
        return 1;
    }
    step_samples = avp_mfcc_get_step_samples(mfcc);
    if (step_samples == 0u || step_samples > PCM_BUFFER_SAMPLES) {
        printf("invalid mfcc step samples: %u\n", (unsigned int)step_samples);
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

    memset(mfcc_output, 0, sizeof(mfcc_output));

    while (mfcc_count < MFCC_NUM_FRAMES) {
        size_t read_samples = fread(pcm_buffer, sizeof(pcm_buffer[0]),
                                    step_samples, input_fp);
        if (read_samples == 0) {
            break;
        }
        input_samples += (uint32_t)read_samples;
        st = avp_mfcc_process(mfcc, pcm_buffer, step_samples,
                              &sample_read, &frame);
        if (st != AVP_OK) {
            printf("avp_mfcc_process failed: %d\n", st);
            goto out;
        }
        if (sample_read != step_samples) {
            printf("unexpected sample_read: %u/%u\n",
                   (unsigned int)sample_read, (unsigned int)step_samples);
            goto out;
        }
        if (frame.mfcc_size > 0u) {
            if (frame.mfcc_size != MFCC_NUM_COEFFS) {
                printf("unexpected mfcc size: %u/%u\n",
                       (unsigned int)frame.mfcc_size,
                       (unsigned int)MFCC_NUM_COEFFS);
                goto out;
            }
            memcpy(mfcc_output[mfcc_count], frame.mfcc_value,
                   MFCC_NUM_COEFFS * sizeof(mfcc_output[0][0]));
            mfcc_count++;
        }
    }

    if (mfcc_count != MFCC_NUM_FRAMES) {
        printf("unexpected mfcc_count: %u/%u\n",
               (unsigned int)mfcc_count, (unsigned int)MFCC_NUM_FRAMES);
        goto out;
    }

    if (save_mfcc_output(output_fp, mfcc_output) != 0) {
        printf("write output failed\n");
        goto out;
    }

    printf("mfcc done: %u frames x %u coeffs, input=%u samples, output=%s\n",
           (unsigned int)MFCC_NUM_FRAMES, (unsigned int)MFCC_NUM_COEFFS,
           (unsigned int)input_samples, output_path);

    for (uint32_t i = 0; i < mfcc_count; i++) {
        printf("frame %2u: ", (unsigned int)i);
        for (uint32_t j = 0; j < MFCC_NUM_COEFFS; j++) {
            printf("%8.4f ", mfcc_output[i][j]);
        }
        printf("\n");
    }

    ret = 0;

out:
    if (output_fp != NULL) {
        fclose(output_fp);
    }
    if (input_fp != NULL) {
        fclose(input_fp);
    }
    avp_mfcc_close(mfcc);
    return ret;
}
