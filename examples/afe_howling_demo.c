/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tlsf_port.h"
#include "avp_afe_howling.h"

#define PCM_SAMPLE_RATE    44100u
#define PCM_CHANNELS       2u
#define DEFAULT_INPUT      "jinitaimei.pcm"

static int parse_u32(const char *text, uint32_t min, uint32_t max, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed = strtoul(text, &end, 0);

    if (end == text || *end != '\0' || parsed < min || parsed > max)
        return -1;
    *value = (uint32_t)parsed;
    return 0;
}

static int parse_float(const char *text, float min, float max, float *value)
{
    char *end = NULL;
    float parsed = strtof(text, &end);

    if (end == text || *end != '\0' || !isfinite(parsed) || parsed < min || parsed > max)
        return -1;
    *value = parsed;
    return 0;
}

int main(int argc, char **argv)
{
    const char *input_path = DEFAULT_INPUT;
    const char *output_path = "howling_out.pcm";
    float papr_th = 8.0f;
    float phpr_th = 6.0f;
    float pnpr_th = 6.0f;
    float notch_q = 12.0f;
    uint8_t max_notches = AVP_AFE_HOWLING_MAX_NOTCHES;
    uint32_t max_blocks = 0u;
    uint32_t parsed;
    uint32_t blocks = 0u;
    uint32_t total_samples = 0u;
    uint32_t frame_samples = 0u;
    uint32_t pcm_frame_samples = 0u;
    int16_t *buffer = NULL;
    FILE *input = NULL;
    FILE *output = NULL;
    avp_afe_howling_t *howling = NULL;
    avp_afe_howling_config_t config;
    avp_status_t st;
    int result = 1;

    if (argc > 9)
        goto usage;
    if (argc > 1)
        input_path = argv[1];
    if (argc > 2)
        output_path = argv[2];
    if (argc > 3 && parse_float(argv[3], -10.0f, 20.0f, &papr_th) != 0)
        goto usage;
    if (argc > 4 && parse_float(argv[4], 0.0f, 100.0f, &phpr_th) != 0)
        goto usage;
    if (argc > 5 && parse_float(argv[5], 0.0f, 100.0f, &pnpr_th) != 0)
        goto usage;
    if (argc > 6 && parse_float(argv[6], 2.0f, 30.0f, &notch_q) != 0)
        goto usage;
    if (argc > 7) {
        if (parse_u32(argv[7], 1u, AVP_AFE_HOWLING_MAX_NOTCHES, &parsed) != 0)
            goto usage;
        max_notches = (uint8_t)parsed;
    }
    if (argc > 8 && parse_u32(argv[8], 0u, UINT32_MAX, &max_blocks) != 0)
        goto usage;

    avp_mem_init();
    input = fopen(input_path, "rb");
    if (input == NULL) {
        printf("open PCM input failed: %s\n", input_path);
        goto out;
    }
    output = fopen(output_path, "wb");
    if (output == NULL) {
        printf("open PCM output failed: %s\n", output_path);
        goto out;
    }

    memset(&config, 0, sizeof(config));
    config.sample_rate = PCM_SAMPLE_RATE;
    config.channels = PCM_CHANNELS;
    config.max_notches = max_notches;
    config.papr_th = papr_th;
    config.phpr_th = phpr_th;
    config.pnpr_th = pnpr_th;
    config.notch_q = notch_q;
    config.enable = 1u;
    st = avp_afe_howling_open(&config, &howling);
    if (st != AVP_OK) {
        printf("avp_afe_howling_open failed: %d\n", (int)st);
        goto out;
    }
    frame_samples = avp_afe_howling_get_frame_samples(howling);
    pcm_frame_samples = frame_samples * PCM_CHANNELS;
    buffer = (int16_t *)avp_malloc(pcm_frame_samples * sizeof(buffer[0]));
    if (buffer == NULL) {
        printf("allocate PCM buffer failed\n");
        goto out;
    }

    while (max_blocks == 0u || blocks < max_blocks) {
        size_t samples = fread(buffer, sizeof(buffer[0]), pcm_frame_samples, input);
        if (samples < pcm_frame_samples)
            break;
        st = avp_afe_howling_process(howling, buffer, buffer, pcm_frame_samples);
        if (st != AVP_OK) {
            printf("avp_afe_howling_process failed: %d\n", (int)st);
            goto out;
        }
        if (fwrite(buffer, sizeof(buffer[0]), pcm_frame_samples, output) != pcm_frame_samples) {
            printf("write PCM output failed\n");
            goto out;
        }
        total_samples += pcm_frame_samples;
        blocks++;
    }

    uint8_t active_notches = 0u;
    st = avp_afe_howling_control(howling, AVP_AFE_HOWLING_CMD_GET_ACTIVE_NOTCHES,
                                &active_notches);
    if (st != AVP_OK) {
        printf("avp_afe_howling_control failed: %d\n", (int)st);
        goto out;
    }
    printf("howling done: blocks=%u, samples=%u, sample_rate=%u, channels=%u, active_notches=%u\n",
           (unsigned int)blocks, (unsigned int)total_samples,
           (unsigned int)PCM_SAMPLE_RATE, (unsigned int)PCM_CHANNELS,
           (unsigned int)active_notches);

    result = 0;

out:
    avp_free(buffer);
    avp_afe_howling_close(howling);
    if (output != NULL && fclose(output) != 0 && result == 0)
        result = 1;
    if (input != NULL)
        fclose(input);
    return result;

usage:
    printf("usage: %s [input.pcm] [output.pcm] [papr_th] [phpr_th] [pnpr_th] [notch_q] [max_notches] [max_blocks]\n",
           argv[0]);
    return 1;
}
