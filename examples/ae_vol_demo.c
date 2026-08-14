/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "avp_ae_vol.h"
#include "wav_container.h"
#include "tlsf_port.h"

#define DEFAULT_INPUT_WAV  "../examples/files/jinitaimei.wav"
#define DEFAULT_OUTPUT_WAV "vol_out.wav"
#define DEFAULT_VOL_INDEX  192u
#define DEFAULT_MIN_DB     AVP_AE_VOL_DEFAULT_MIN_DB
#define DEFAULT_MAX_DB     AVP_AE_VOL_DEFAULT_MAX_DB

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

static int parse_u32(const char *text,
                     uint32_t min,
                     uint32_t max,
                     uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed;

    if (text == NULL || value == NULL) {
        return -1;
    }

    parsed = strtoul(text, &end, 0);
    if (end == text ||
        *end != '\0' ||
        parsed < min ||
        parsed > max) {
        return -1;
    }

    *value = (uint32_t)parsed;
    return 0;
}

static int parse_i32(const char *text, int min, int max, int *value)
{
    char *end = NULL;
    long parsed;

    if (text == NULL || value == NULL) {
        return -1;
    }

    parsed = strtol(text, &end, 0);
    if (end == text ||
        *end != '\0' ||
        parsed < min ||
        parsed > max) {
        return -1;
    }

    *value = (int)parsed;
    return 0;
}

static void print_usage(const char *program)
{
    printf("usage: %s [input.wav] [output.wav] [index] [min_db] [max_db] [max_packets]\n",
           program);
    printf("       default: %s %s %u %d %d 0\n",
           DEFAULT_INPUT_WAV,
           DEFAULT_OUTPUT_WAV,
           (unsigned int)DEFAULT_VOL_INDEX,
           DEFAULT_MIN_DB,
           DEFAULT_MAX_DB);
}

static int run_vol(const char *input_path,
                   const char *output_path,
                   uint8_t index,
                   int min_db,
                   int max_db,
                   uint32_t max_packets)
{
    FILE *input_fp = NULL;
    FILE *output_fp = NULL;
    avp_io_t input_io;
    avp_io_t output_io;
    wav_demux_t demuxer;
    wav_mux_t muxer;
    avp_packet_t *packet = NULL;
    avp_ae_vol_t *vol = NULL;
    uint32_t packet_count = 0u;
    uint32_t sample_count_total = 0u;
    uint16_t block_align;
    int32_t gain_q14 = 0;
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
        demuxer.header.wave_fmt.NumChannels == 0u) {
        FAIL("input must be 16-bit PCM WAV\n");
    }
    block_align = demuxer.header.wave_fmt.BlockAlign;

    packet = avp_packet_alloc(demuxer.header.wave_fmt.ByteRate / 100u);
    if (packet == NULL) {
        FAIL("alloc packet failed\n");
    }

    {
        avp_ae_vol_config_t config;

        memset(&config, 0, sizeof(config));
        config.min_db = min_db;
        config.max_db = max_db;
        config.index = index;
        config.enable = 1u;
        st = avp_ae_vol_open(&config, &vol);
        if (st != AVP_OK) {
            FAIL("avp_ae_vol_open failed: %d\n", (int)st);
        }
    }

    st = avp_ae_vol_control(vol, AVP_AE_VOL_CMD_GET_GAIN_Q14, &gain_q14);
    if (st != AVP_OK) {
        FAIL("get gain failed: %d\n", (int)st);
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
        uint32_t sample_count;

        st = wav_demux_read_packet(&demuxer, packet);
        if (st == AVP_ENOENT) {
            break;
        }
        if (st != AVP_OK) {
            FAIL("wav_demux_read_packet failed: %d\n", (int)st);
        }
        if ((packet->size % block_align) != 0u ||
            (packet->size % sizeof(int16_t)) != 0u) {
            FAIL("unaligned PCM packet: %u bytes\n", (unsigned int)packet->size);
        }

        sample_count = packet->size / sizeof(int16_t);
        st = avp_ae_vol_process(vol,
                                (const int16_t *)packet->buf,
                                (int16_t *)packet->buf,
                                sample_count);
        if (st != AVP_OK) {
            FAIL("avp_ae_vol_process failed: %d\n", (int)st);
        }

        st = wav_mux(&muxer, packet->buf, packet->size);
        if (st != AVP_OK) {
            FAIL("wav_mux failed: %d\n", (int)st);
        }

        sample_count_total += sample_count;
        packet_count++;
    }

    printf("vol done: packets=%u, samples=%u, sample_rate=%u, channels=%u, "
           "index=%u, range=%d..%d dB, gain_q14=%d\n",
           (unsigned int)packet_count,
           (unsigned int)sample_count_total,
           (unsigned int)demuxer.header.wave_fmt.SampleRate,
           (unsigned int)demuxer.header.wave_fmt.NumChannels,
           (unsigned int)index,
           min_db,
           max_db,
           (int)gain_q14);
    ret = 0;

out:
    if (mux_opened != 0) {
        wav_mux_close(&muxer);
    }
    if (demux_opened != 0) {
        wav_demux_close(&demuxer);
    }
    avp_ae_vol_close(vol);
    avp_packet_free(packet);
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
    uint32_t parsed;
    uint32_t max_packets = 0u;
    uint8_t index = (uint8_t)DEFAULT_VOL_INDEX;
    int min_db = DEFAULT_MIN_DB;
    int max_db = DEFAULT_MAX_DB;

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
    if (argc > 3) {
        if (parse_u32(argv[3], 0u, 255u, &parsed) != 0) {
            print_usage(argv[0]);
            return 1;
        }
        index = (uint8_t)parsed;
    }
    if (argc > 4 && parse_i32(argv[4], -120, 60, &min_db) != 0) {
        print_usage(argv[0]);
        return 1;
    }
    if (argc > 5 && parse_i32(argv[5], -120, 60, &max_db) != 0) {
        print_usage(argv[0]);
        return 1;
    }
    if (argc > 6 && parse_u32(argv[6], 0u, UINT32_MAX, &max_packets) != 0) {
        print_usage(argv[0]);
        return 1;
    }

    return run_vol(input_path, output_path, index, min_db, max_db, max_packets);
}
