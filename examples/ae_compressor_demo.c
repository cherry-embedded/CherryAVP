/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tlsf_port.h"
#include "avp_ae_compressor.h"

#define PCM_SAMPLE_RATE    44100u
#define PCM_CHANNELS       2u
#define PCM_BUFFER_SAMPLES 4096u
#define DEFAULT_INPUT      "jinitaimei.pcm"

static int parse_u32(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed = strtoul(text, &end, 0);

    if (end == text || *end != '\0' || parsed > UINT32_MAX)
        return -1;
    *value = (uint32_t)parsed;
    return 0;
}

int main(int argc, char **argv)
{
    const char *input_path = DEFAULT_INPUT;
    const char *output_path = "compressor_out.pcm";
    uint32_t max_blocks = 0u;
    uint32_t blocks = 0u;
    uint32_t total_samples = 0u;
    int16_t buffer[PCM_BUFFER_SAMPLES];
    FILE *input = NULL;
    FILE *output = NULL;
    avp_ae_compressor_t *compressor = NULL;
    avp_ae_compressor_config_t config;
    avp_status_t st;
    int result = 1;

    if (argc > 4)
        goto usage;
    if (argc > 1)
        input_path = argv[1];
    if (argc > 2)
        output_path = argv[2];
    if (argc > 3 && parse_u32(argv[3], &max_blocks) != 0)
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
    config.threshold_db = -18.0f;
    config.ratio = 4.0f;
    config.attack_ms = 8.0f;
    config.release_ms = 120.0f;
    config.makeup_db = 3.0f;
    config.enable = 1u;
    st = avp_ae_compressor_open(&config, &compressor);
    if (st != AVP_OK) {
        printf("avp_ae_compressor_open failed: %d\n", (int)st);
        goto out;
    }

    while (max_blocks == 0u || blocks < max_blocks) {
        size_t samples = fread(buffer, sizeof(buffer[0]), PCM_BUFFER_SAMPLES, input);
        samples -= samples % PCM_CHANNELS;
        if (samples == 0u)
            break;
        st = avp_ae_compressor_process(compressor, buffer, buffer, (uint32_t)samples);
        if (st != AVP_OK) {
            printf("avp_ae_compressor_process failed: %d\n", (int)st);
            goto out;
        }
        if (fwrite(buffer, sizeof(buffer[0]), samples, output) != samples) {
            printf("write PCM output failed\n");
            goto out;
        }
        total_samples += (uint32_t)samples;
        blocks++;
    }

    float threshold_db = 0.0f;
    st = avp_ae_compressor_control(compressor,
                                   AVP_AE_COMPRESSOR_CMD_GET_THRESHOLD_DB,
                                   &threshold_db);
    if (st != AVP_OK) {
        printf("avp_ae_compressor_control failed: %d\n", (int)st);
        goto out;
    }
    printf("compressor done: blocks=%u, samples=%u, sample_rate=%u, channels=%u, threshold=%.1f dB\n",
           (unsigned int)blocks, (unsigned int)total_samples,
           (unsigned int)PCM_SAMPLE_RATE, (unsigned int)PCM_CHANNELS,
           (double)threshold_db);
    result = 0;

out:
    avp_ae_compressor_close(compressor);
    if (output != NULL && fclose(output) != 0 && result == 0)
        result = 1;
    if (input != NULL)
        fclose(input);
    return result;

usage:
    printf("usage: %s [input.pcm] [output.pcm] [max_blocks]\n", argv[0]);
    return 1;
}
