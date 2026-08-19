/* Standalone howling-suppression example for raw 44.1 kHz stereo S16LE PCM. */
#include <math.h>
#include <stdlib.h>

#include "tlsf_port.h"
#include "avp_ae_howling.h"

#define PCM_SAMPLE_RATE    44100u
#define PCM_CHANNELS       2u
#define PCM_BUFFER_SAMPLES 4096u
#define DEFAULT_INPUT      "../output/files/jinitaimei.pcm"

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
    float threshold_db = 18.0f;
    float notch_q = 12.0f;
    uint8_t max_notches = AVP_AE_HOWLING_MAX_NOTCHES;
    uint32_t max_blocks = 0u;
    uint32_t parsed;
    uint32_t blocks = 0u;
    uint32_t total_samples = 0u;
    int16_t buffer[PCM_BUFFER_SAMPLES];
    FILE *input = NULL;
    FILE *output = NULL;
    avp_ae_howling_t *howling = NULL;
    avp_ae_howling_config_t config;
    avp_status_t st;
    int result = 1;

    if (argc > 7)
        goto usage;
    if (argc > 1)
        input_path = argv[1];
    if (argc > 2)
        output_path = argv[2];
    if (argc > 3 && parse_float(argv[3], 6.0f, 60.0f, &threshold_db) != 0)
        goto usage;
    if (argc > 4 && parse_float(argv[4], 2.0f, 30.0f, &notch_q) != 0)
        goto usage;
    if (argc > 5) {
        if (parse_u32(argv[5], 1u, AVP_AE_HOWLING_MAX_NOTCHES, &parsed) != 0)
            goto usage;
        max_notches = (uint8_t)parsed;
    }
    if (argc > 6 && parse_u32(argv[6], 0u, UINT32_MAX, &max_blocks) != 0)
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
    config.frame_samples = AVP_AE_HOWLING_DEFAULT_FRAME_SAMPLES;
    config.max_notches = max_notches;
    config.threshold_db = threshold_db;
    config.notch_q = notch_q;
    config.enable = 1u;
    st = avp_ae_howling_open(&config, &howling);
    if (st != AVP_OK) {
        printf("avp_ae_howling_open failed: %d\n", (int)st);
        goto out;
    }

    while (max_blocks == 0u || blocks < max_blocks) {
        size_t samples = fread(buffer, sizeof(buffer[0]), PCM_BUFFER_SAMPLES, input);
        samples -= samples % PCM_CHANNELS;
        if (samples == 0u)
            break;
        st = avp_ae_howling_process(howling, buffer, buffer, (uint32_t)samples);
        if (st != AVP_OK) {
            printf("avp_ae_howling_process failed: %d\n", (int)st);
            goto out;
        }
        if (fwrite(buffer, sizeof(buffer[0]), samples, output) != samples) {
            printf("write PCM output failed\n");
            goto out;
        }
        total_samples += (uint32_t)samples;
        blocks++;
    }

    uint8_t active_notches = 0u;
    st = avp_ae_howling_control(howling, AVP_AE_HOWLING_CMD_GET_ACTIVE_NOTCHES,
                                &active_notches);
    if (st != AVP_OK) {
        printf("avp_ae_howling_control failed: %d\n", (int)st);
        goto out;
    }
    printf("howling done: blocks=%u, samples=%u, sample_rate=%u, channels=%u, active_notches=%u\n",
           (unsigned int)blocks, (unsigned int)total_samples,
           (unsigned int)PCM_SAMPLE_RATE, (unsigned int)PCM_CHANNELS,
           (unsigned int)active_notches);

    result = 0;

out:
    avp_ae_howling_close(howling);
    if (output != NULL && fclose(output) != 0 && result == 0)
        result = 1;
    if (input != NULL)
        fclose(input);
    return result;

usage:
    printf("usage: %s [input.pcm] [output.pcm] [threshold_db] [notch_q] [max_notches] [max_blocks]\n",
           argv[0]);
    return 1;
}
