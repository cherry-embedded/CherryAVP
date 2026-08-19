/* Standalone mixer example for two raw 44.1 kHz stereo S16LE PCM inputs. */
#include <math.h>
#include <stdlib.h>

#include "tlsf_port.h"
#include "avp_ae_mixer.h"

#define PCM_SAMPLE_RATE    44100u
#define PCM_CHANNELS       2u
#define PCM_BUFFER_SAMPLES 4096u
#define DEFAULT_INPUT      "../output/files/jinitaimei.pcm"

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
    const char *input_a_path = DEFAULT_INPUT;
    const char *input_b_path = DEFAULT_INPUT;
    const char *output_path = "mixer_out.pcm";
    float gain_a = 0.75f;
    float gain_b = 0.50f;
    uint32_t max_blocks = 0u;
    uint32_t blocks = 0u;
    uint32_t total_samples = 0u;
    int16_t buffer_a[PCM_BUFFER_SAMPLES];
    int16_t buffer_b[PCM_BUFFER_SAMPLES];
    int16_t output_buffer[PCM_BUFFER_SAMPLES];
    const int16_t *inputs[2] = { buffer_a, buffer_b };
    FILE *input_a = NULL;
    FILE *input_b = NULL;
    FILE *output = NULL;
    avp_ae_mixer_t *mixer = NULL;
    avp_ae_mixer_config_t config;
    avp_status_t st;
    int result = 1;

    if (argc > 7)
        goto usage;
    if (argc > 1)
        input_a_path = argv[1];
    if (argc > 2)
        input_b_path = argv[2];
    if (argc > 3)
        output_path = argv[3];
    if (argc > 4 && parse_float(argv[4], 0.0f, 4.0f, &gain_a) != 0)
        goto usage;
    if (argc > 5 && parse_float(argv[5], 0.0f, 4.0f, &gain_b) != 0)
        goto usage;
    if (argc > 6 && parse_u32(argv[6], &max_blocks) != 0)
        goto usage;

    avp_mem_init();
    input_a = fopen(input_a_path, "rb");
    if (input_a == NULL) {
        printf("open first PCM input failed: %s\n", input_a_path);
        goto out;
    }
    input_b = fopen(input_b_path, "rb");
    if (input_b == NULL) {
        printf("open second PCM input failed: %s\n", input_b_path);
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
    config.input_count = 2u;
    config.gains[0] = gain_a;
    config.gains[1] = gain_b;
    config.enable = 1u;
    st = avp_ae_mixer_open(&config, &mixer);
    if (st != AVP_OK) {
        printf("avp_ae_mixer_open failed: %d\n", (int)st);
        goto out;
    }

    while (max_blocks == 0u || blocks < max_blocks) {
        size_t samples_a = fread(buffer_a, sizeof(buffer_a[0]), PCM_BUFFER_SAMPLES, input_a);
        size_t samples_b = fread(buffer_b, sizeof(buffer_b[0]), PCM_BUFFER_SAMPLES, input_b);
        size_t samples = samples_a < samples_b ? samples_a : samples_b;
        samples -= samples % PCM_CHANNELS;
        if (samples == 0u)
            break;
        st = avp_ae_mixer_process(mixer, inputs, output_buffer, (uint32_t)samples);
        if (st != AVP_OK) {
            printf("avp_ae_mixer_process failed: %d\n", (int)st);
            goto out;
        }
        if (fwrite(output_buffer, sizeof(output_buffer[0]), samples, output) != samples) {
            printf("write PCM output failed\n");
            goto out;
        }
        total_samples += (uint32_t)samples;
        blocks++;
    }

    avp_ae_mixer_gain_t gain = { 1u, 0.0f };
    st = avp_ae_mixer_control(mixer, AVP_AE_MIXER_CMD_GET_INPUT_GAIN, &gain);
    if (st != AVP_OK) {
        printf("avp_ae_mixer_control failed: %d\n", (int)st);
        goto out;
    }
    printf("mixer done: blocks=%u, samples=%u, sample_rate=%u, channels=%u, gains=%.2f/%.2f\n",
           (unsigned int)blocks, (unsigned int)total_samples,
           (unsigned int)PCM_SAMPLE_RATE, (unsigned int)PCM_CHANNELS,
           (double)gain_a, (double)gain.gain);

    result = 0;

out:
    avp_ae_mixer_close(mixer);
    if (output != NULL && fclose(output) != 0 && result == 0)
        result = 1;
    if (input_b != NULL)
        fclose(input_b);
    if (input_a != NULL)
        fclose(input_a);
    return result;

usage:
    printf("usage: %s [input_a.pcm] [input_b.pcm] [output.pcm] [gain_a] [gain_b] [max_blocks]\n",
           argv[0]);
    return 1;
}
