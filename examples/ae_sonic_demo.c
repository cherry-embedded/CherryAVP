/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "avp_ae_sonic.h"
#include "wav_container.h"
#include "tlsf_port.h"

#define DEFAULT_INPUT_WAV          "../examples/files/jinitaimei.wav"
#define DEFAULT_OUTPUT_WAV         "sonic_out.wav"
#define DEFAULT_SPEED              1.35f
#define DEFAULT_PITCH              1.00f
#define DEFAULT_RATE               1.00f
#define DEFAULT_VOLUME             1.00f
#define SONIC_DEMO_MIN_OUT_SAMPLES 4096u

#define FAIL(...)            \
    do {                     \
        printf(__VA_ARGS__); \
        goto out;            \
    } while (0)

static int file_read_cb(avp_io_t *avp_io, uint8_t *buffer, uint32_t size)
{
    FILE *fp = (FILE *)avp_io->priv;

    if (fp == NULL || (buffer == NULL && size != 0u)) {
        return -1;
    }
    return (int)fread(buffer, 1u, size, fp);
}

static int file_write_cb(avp_io_t *avp_io,
                         const uint8_t *buffer,
                         uint32_t size)
{
    FILE *fp = (FILE *)avp_io->priv;

    if (fp == NULL || (buffer == NULL && size != 0u)) {
        return -1;
    }
    return (int)fwrite(buffer, 1u, size, fp);
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

static int parse_float(const char *text, float min, float max, float *value)
{
    char *end = NULL;
    float parsed;

    if (text == NULL || value == NULL) {
        return -1;
    }

    parsed = strtof(text, &end);
    if (end == text ||
        *end != '\0' ||
        parsed < min ||
        parsed > max) {
        return -1;
    }

    *value = parsed;
    return 0;
}

static int parse_u32(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed;

    if (text == NULL || value == NULL) {
        return -1;
    }

    parsed = strtoul(text, &end, 0);
    if (end == text || *end != '\0' || parsed > UINT32_MAX) {
        return -1;
    }

    *value = (uint32_t)parsed;
    return 0;
}

static void print_usage(const char *program)
{
    printf("usage: %s [input.wav] [output.wav] [speed] [pitch] [rate] [max_packets]\n",
           program);
    printf("       default: %s %s %.2f %.2f %.2f 0\n",
           DEFAULT_INPUT_WAV,
           DEFAULT_OUTPUT_WAV,
           DEFAULT_SPEED,
           DEFAULT_PITCH,
           DEFAULT_RATE);
}

static avp_status_t write_sonic_output(avp_ae_sonic_t *sonic,
                                       wav_mux_t *muxer,
                                       int16_t *out_buffer,
                                       uint32_t out_capacity,
                                       uint16_t block_align,
                                       uint32_t *out_samples_total)
{
    avp_status_t st;
    uint32_t available = 0u;

    do {
        uint32_t out_samples = 0u;

        st = avp_ae_sonic_process(sonic,
                               NULL,
                               0u,
                               out_buffer,
                               out_capacity,
                               &out_samples);
        if (st != AVP_OK) {
            return st;
        }
        if (out_samples > 0u) {
            st = wav_mux(muxer,
                         out_buffer,
                         out_samples * (uint32_t)block_align);
            if (st != AVP_OK) {
                return st;
            }
            *out_samples_total += out_samples;
        }

        st = avp_ae_sonic_control(sonic, AVP_AE_SONIC_CMD_GET_AVAILABLE, &available);
        if (st != AVP_OK) {
            return st;
        }
    } while (available > 0u);

    return AVP_OK;
}

static int run_sonic(const char *input_path,
                     const char *output_path,
                     float speed,
                     float pitch,
                     float rate,
                     uint32_t max_packets)
{
    FILE *input_fp = NULL;
    FILE *output_fp = NULL;
    avp_io_t input_io;
    avp_io_t output_io;
    wav_demux_t demuxer;
    wav_mux_t muxer;
    avp_packet_t *packet = NULL;
    avp_ae_sonic_t *sonic = NULL;
    int16_t *out_buffer = NULL;
    uint32_t out_capacity;
    uint32_t input_samples_total = 0u;
    uint32_t output_samples_total = 0u;
    uint32_t packet_count = 0u;
    uint16_t block_align;
    avp_status_t st;
    int demux_opened = 0;
    int mux_opened = 0;
    int ret = 1;

    memset(&input_io, 0, sizeof(input_io));
    memset(&output_io, 0, sizeof(output_io));
    memset(&demuxer, 0, sizeof(demuxer));
    memset(&muxer, 0, sizeof(muxer));

    input_fp = fopen(input_path, "rb");
    if (input_fp == NULL) {
        FAIL("open input failed: %s\n", input_path);
    }
    avp_io_init(&input_io, file_read_cb, NULL, file_seek_cb, file_get_size_cb, input_fp);

    st = wav_demux_open(&demuxer, &input_io);
    if (st != AVP_OK) {
        FAIL("wav_demux_open failed: %d\n", (int)st);
    }
    demux_opened = 1;

    if (demuxer.header.wave_fmt.AudioFormat != WAV_AUDIO_FORMAT_PCM ||
        demuxer.header.wave_fmt.BitsPerSample != 16u ||
        demuxer.header.wave_fmt.NumChannels == 0u ||
        demuxer.header.wave_fmt.NumChannels > AVP_AE_SONIC_MAX_CHANNELS) {
        FAIL("input must be 16-bit PCM WAV with 1-%u channels\n",
             (unsigned int)AVP_AE_SONIC_MAX_CHANNELS);
    }

    block_align = demuxer.header.wave_fmt.BlockAlign;
    packet = avp_packet_alloc(demuxer.header.wave_fmt.ByteRate / 100u);
    if (packet == NULL) {
        FAIL("alloc packet failed\n");
    }

    avp_ae_sonic_config_t config;

    memset(&config, 0, sizeof(config));
    config.sample_rate = demuxer.header.wave_fmt.SampleRate;
    config.channels = (uint8_t)demuxer.header.wave_fmt.NumChannels;
    config.speed = speed;
    config.pitch = pitch;
    config.rate = rate;
    config.volume = DEFAULT_VOLUME;
    config.quality = 0u;
    st = avp_ae_sonic_open(&config, &sonic);
    if (st != AVP_OK) {
        FAIL("avp_ae_sonic_open failed: %d\n", (int)st);
    }

    out_capacity = demuxer.header.wave_fmt.SampleRate / 5u;
    if (out_capacity < SONIC_DEMO_MIN_OUT_SAMPLES) {
        out_capacity = SONIC_DEMO_MIN_OUT_SAMPLES;
    }
    out_buffer = (int16_t *)avp_malloc((size_t)out_capacity * block_align);
    if (out_buffer == NULL) {
        FAIL("alloc output buffer failed\n");
    }

    output_fp = fopen(output_path, "wb");
    if (output_fp == NULL) {
        FAIL("open output failed: %s\n", output_path);
    }
    avp_io_init(&output_io, NULL, file_write_cb, file_seek_cb, file_get_size_cb, output_fp);
    st = wav_mux_open(&muxer,
                      &output_io,
                      WAV_AUDIO_FORMAT_PCM,
                      demuxer.header.wave_fmt.NumChannels,
                      demuxer.header.wave_fmt.SampleRate,
                      demuxer.header.wave_fmt.BitsPerSample);
    if (st != AVP_OK) {
        FAIL("wav_mux_open failed: %d\n", (int)st);
    }
    mux_opened = 1;

    while (max_packets == 0u || packet_count < max_packets) {
        uint32_t in_samples;

        st = wav_demux_read_packet(&demuxer, packet);
        if (st == AVP_ENOENT) {
            break;
        }
        if (st != AVP_OK) {
            FAIL("wav_demux_read_packet failed: %d\n", (int)st);
        }
        if ((packet->size % block_align) != 0u) {
            FAIL("unaligned PCM packet: %u bytes\n", (unsigned int)packet->size);
        }

        in_samples = packet->size / block_align;
        st = avp_ae_sonic_process(sonic,
                               (const int16_t *)packet->buf,
                               in_samples,
                               NULL,
                               0u,
                               NULL);
        if (st != AVP_OK) {
            FAIL("avp_ae_sonic_process failed: %d\n", (int)st);
        }

        st = write_sonic_output(sonic,
                                &muxer,
                                out_buffer,
                                out_capacity,
                                block_align,
                                &output_samples_total);
        if (st != AVP_OK) {
            FAIL("write sonic output failed: %d\n", (int)st);
        }

        input_samples_total += in_samples;
        packet_count++;
    }

    st = avp_ae_sonic_control(sonic, AVP_AE_SONIC_CMD_FLUSH, NULL);
    if (st != AVP_OK) {
        FAIL("avp_ae_sonic flush failed: %d\n", (int)st);
    }
    st = write_sonic_output(sonic,
                            &muxer,
                            out_buffer,
                            out_capacity,
                            block_align,
                            &output_samples_total);
    if (st != AVP_OK) {
        FAIL("drain sonic output failed: %d\n", (int)st);
    }

    printf("sonic done: packets=%u, in_samples=%u, out_samples=%u, "
           "sample_rate=%u, channels=%u, speed=%.3f, pitch=%.3f, rate=%.3f\n",
           (unsigned int)packet_count,
           (unsigned int)input_samples_total,
           (unsigned int)output_samples_total,
           (unsigned int)demuxer.header.wave_fmt.SampleRate,
           (unsigned int)demuxer.header.wave_fmt.NumChannels,
           (double)speed,
           (double)pitch,
           (double)rate);
    ret = 0;

out:
    if (mux_opened != 0) {
        wav_mux_close(&muxer);
    }
    if (demux_opened != 0) {
        wav_demux_close(&demuxer);
    }
    avp_ae_sonic_close(sonic);
    avp_packet_free(packet);
    avp_free(out_buffer);
    if (output_fp != NULL && fclose(output_fp) != 0 && ret == 0) {
        ret = 1;
    }
    if (input_fp != NULL) {
        fclose(input_fp);
    }
    return ret;
}

int main(int argc, char **argv)
{
    const char *input_path = DEFAULT_INPUT_WAV;
    const char *output_path = DEFAULT_OUTPUT_WAV;
    float speed = DEFAULT_SPEED;
    float pitch = DEFAULT_PITCH;
    float rate = DEFAULT_RATE;
    uint32_t max_packets = 0u;

    avp_mem_init();

    if (argc > 7) {
        print_usage(argv[0]);
        return 1;
    }
    if (argc > 1) {
        input_path = argv[1];
    }
    if (argc > 2) {
        output_path = argv[2];
    }
    if (argc > 3 && parse_float(argv[3], 0.1f, 8.0f, &speed) != 0) {
        print_usage(argv[0]);
        return 1;
    }
    if (argc > 4 && parse_float(argv[4], 0.1f, 8.0f, &pitch) != 0) {
        print_usage(argv[0]);
        return 1;
    }
    if (argc > 5 && parse_float(argv[5], 0.1f, 8.0f, &rate) != 0) {
        print_usage(argv[0]);
        return 1;
    }
    if (argc > 6 && parse_u32(argv[6], &max_packets) != 0) {
        print_usage(argv[0]);
        return 1;
    }

    return run_sonic(input_path, output_path, speed, pitch, rate, max_packets);
}
