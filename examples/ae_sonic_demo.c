/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tlsf_port.h"
#include "avp_ae_sonic.h"

#define PCM_SAMPLE_RATE 44100u
#define PCM_CHANNELS 2u
#define PCM_BUFFER_SAMPLES 4096u
#define SONIC_OUTPUT_FRAMES 8820u
#define DEFAULT_INPUT "jinitaimei.pcm"

static int parse_u32(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed = strtoul(text, &end, 0);

    if (end == text || *end != '\0' || parsed > UINT32_MAX)
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
    const char *output_path = "sonic_out.pcm";
    float speed = 1.35f;
    float pitch = 1.0f;
    uint32_t max_blocks = 0u;
    uint32_t blocks = 0u;
    uint32_t input_frames = 0u;
    uint32_t output_frames = 0u;
    int16_t input_buffer[PCM_BUFFER_SAMPLES];
    int16_t *output_buffer = NULL;
    FILE *input = NULL;
    FILE *output = NULL;
    avp_ae_sonic_t *sonic = NULL;
    avp_ae_sonic_config_t config;
    avp_status_t st;
    int result = 1;

    if (argc > 6)
        goto usage;
    if (argc > 1)
        input_path = argv[1];
    if (argc > 2)
        output_path = argv[2];
    if (argc > 3 && parse_float(argv[3], 0.1f, 8.0f, &speed) != 0)
        goto usage;
    if (argc > 4 && parse_float(argv[4], 0.1f, 8.0f, &pitch) != 0)
        goto usage;
    if (argc > 5 && parse_u32(argv[5], &max_blocks) != 0)
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
    output_buffer = (int16_t *)avp_malloc(
        (size_t)SONIC_OUTPUT_FRAMES * PCM_CHANNELS * sizeof(*output_buffer));
    if (output_buffer == NULL) {
        printf("allocate Sonic output buffer failed\n");
        goto out;
    }

    memset(&config, 0, sizeof(config));
    config.sample_rate = PCM_SAMPLE_RATE;
    config.channels = PCM_CHANNELS;
    config.speed = speed;
    config.pitch = pitch;
    st = avp_ae_sonic_open(&config, &sonic);
    if (st != AVP_OK) {
        printf("avp_ae_sonic_open failed: %d\n", (int)st);
        goto out;
    }

    while (max_blocks == 0u || blocks < max_blocks) {
        size_t samples = fread(input_buffer, sizeof(input_buffer[0]),
                               PCM_BUFFER_SAMPLES, input);
        uint32_t frames;
        uint32_t produced = 0u;

        samples -= samples % PCM_CHANNELS;
        if (samples == 0u)
            break;
        frames = (uint32_t)(samples / PCM_CHANNELS);
        st = avp_ae_sonic_process(sonic, input_buffer, frames, output_buffer,
                                  SONIC_OUTPUT_FRAMES, &produced);
        if (st != AVP_OK) {
            printf("avp_ae_sonic_process failed: %d\n", (int)st);
            goto out;
        }
        if (produced > 0u &&
            fwrite(output_buffer, PCM_CHANNELS * sizeof(output_buffer[0]),
                   produced, output) != produced) {
            printf("write PCM output failed\n");
            goto out;
        }
        input_frames += frames;
        output_frames += produced;
        blocks++;
    }

    st = avp_ae_sonic_control(sonic, AVP_AE_SONIC_CMD_FLUSH, NULL);
    if (st != AVP_OK) {
        printf("avp_ae_sonic_control flush failed: %d\n", (int)st);
        goto out;
    }
    for (;;) {
        uint32_t produced = 0u;
        st = avp_ae_sonic_process(sonic, NULL, 0u, output_buffer,
                                  SONIC_OUTPUT_FRAMES, &produced);
        if (st != AVP_OK) {
            printf("avp_ae_sonic_process drain failed: %d\n", (int)st);
            goto out;
        }
        if (produced == 0u)
            break;
        if (fwrite(output_buffer, PCM_CHANNELS * sizeof(output_buffer[0]),
                   produced, output) != produced) {
            printf("write PCM output failed\n");
            goto out;
        }
        output_frames += produced;
    }

    printf("sonic done: blocks=%u, in_frames=%u, out_frames=%u, sample_rate=%u, channels=%u, speed=%.3f, pitch=%.3f\n",
           (unsigned int)blocks, (unsigned int)input_frames,
           (unsigned int)output_frames, (unsigned int)PCM_SAMPLE_RATE,
           (unsigned int)PCM_CHANNELS, (double)speed, (double)pitch);
    result = 0;

out:
    avp_ae_sonic_close(sonic);
    avp_free(output_buffer);
    if (output != NULL && fclose(output) != 0 && result == 0)
        result = 1;
    if (input != NULL)
        fclose(input);
    return result;

usage:
    printf("usage: %s [input.pcm] [output.pcm] [speed] [pitch] [rate] [max_blocks]\n",
           argv[0]);
    return 1;
}
