/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "wav_container.h"
#include "tlsf_port.h"

#define PCM_COPY_CHUNK_SIZE 32768u

#define FAIL(...)            \
    do {                     \
        printf(__VA_ARGS__); \
        goto out;            \
    } while (0)

static int file_write_cb(avp_io_t *avp_io,
                         const uint8_t *buffer,
                         uint32_t size)
{
    FILE *fp;
    size_t actual_size;

    if (avp_io == NULL || avp_io->priv == NULL ||
        (buffer == NULL && size != 0u)) {
        return -1;
    }

    fp = (FILE *)avp_io->priv;
    actual_size = fwrite(buffer, 1, size, fp);
    if (actual_size == 0u && size != 0u && ferror(fp)) {
        return -1;
    }
    return (int)actual_size;
}

static int file_seek_cb(avp_io_t *avp_io, uint32_t offset)
{
    FILE *fp;

    if (avp_io == NULL || avp_io->priv == NULL) {
        return -1;
    }

    fp = (FILE *)avp_io->priv;
    return fseek(fp, (long)offset, SEEK_SET);
}

static int file_get_size_cb(avp_io_t *avp_io)
{
    FILE *fp;
    long current;
    long size;

    if (avp_io == NULL || avp_io->priv == NULL) {
        return -1;
    }

    fp = (FILE *)avp_io->priv;
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

static int parse_u32(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed;

    if (text == NULL || value == NULL || text[0] == '\0') {
        return 0;
    }

    parsed = strtoul(text, &end, 0);
    if (end == text || *end != '\0' || parsed > UINT32_MAX) {
        return 0;
    }
    *value = (uint32_t)parsed;
    return 1;
}

static void print_usage(const char *program)
{
    printf("usage: %s <input.pcm> <output.wav> <sample_rate> "
           "<bits_per_sample> <channels>\n",
           program);
}

static int run_encode(const char *input_path,
                      const char *output_path,
                      uint32_t sample_rate,
                      uint32_t bits_per_sample,
                      uint32_t channels)
{
    FILE *input_fp = NULL;
    FILE *output_fp = NULL;
    avp_io_t input_io;
    avp_io_t output_io;
    wav_mux_t mux;
    uint8_t *buffer = NULL;
    uint32_t input_size;
    uint32_t input_offset = 0u;
    uint32_t buffer_size;
    uint32_t block_align;
    uint32_t pcm_size = 0u;
    uint32_t wav_size = 0u;
    uint32_t parsed_size;
    int input_size_result;
    avp_status_t st = AVP_OK;
    int mux_opened = 0;
    int ret = 1;

    memset(&input_io, 0, sizeof(input_io));
    memset(&output_io, 0, sizeof(output_io));
    memset(&mux, 0, sizeof(mux));

    if (sample_rate == 0u || bits_per_sample == 0u ||
        bits_per_sample > UINT16_MAX || (bits_per_sample % 8u) != 0u ||
        channels == 0u || channels > UINT16_MAX) {
        FAIL("invalid WAV parameters\n");
    }

    block_align = channels * (bits_per_sample / 8u);
    if (block_align == 0u || block_align > UINT16_MAX ||
        block_align > UINT32_MAX / PCM_COPY_CHUNK_SIZE) {
        FAIL("WAV block alignment is out of range\n");
    }

    input_fp = fopen(input_path, "rb");
    if (input_fp == NULL) {
        FAIL("open %s failed\n", input_path);
    }
    avp_io_init(&input_io, NULL, NULL, NULL, file_get_size_cb, input_fp);
    input_size_result = file_get_size_cb(&input_io);
    if (input_size_result < 0) {
        FAIL("get size for %s failed\n", input_path);
    }
    parsed_size = (uint32_t)input_size_result;
    input_size = parsed_size;
    if ((input_size % block_align) != 0u) {
        FAIL("PCM size %u is not aligned to %u bytes\n",
             (unsigned int)input_size,
             (unsigned int)block_align);
    }

    output_fp = fopen(output_path, "wb");
    if (output_fp == NULL) {
        FAIL("open %s failed\n", output_path);
    }
    avp_io_init(&output_io, NULL, file_write_cb, file_seek_cb, file_get_size_cb, output_fp);
    st = wav_mux_open(&mux,
                      &output_io,
                      WAV_AUDIO_FORMAT_PCM,
                      (uint16_t)channels,
                      sample_rate,
                      (uint16_t)bits_per_sample);
    if (st != AVP_OK) {
        FAIL("open WAV mux failed: %d\n", (int)st);
    }
    mux_opened = 1;

    buffer_size = PCM_COPY_CHUNK_SIZE - (PCM_COPY_CHUNK_SIZE % block_align);
    if (buffer_size == 0u) {
        buffer_size = block_align;
    }
    buffer = (uint8_t *)avp_malloc(buffer_size);
    if (buffer == NULL) {
        FAIL("alloc PCM buffer of %u bytes failed\n", (unsigned int)buffer_size);
    }

    while (input_offset < input_size) {
        uint32_t request = input_size - input_offset;
        size_t actual_size;

        if (request > buffer_size) {
            request = buffer_size;
        }
        request -= request % block_align;
        if (request == 0u) {
            FAIL("PCM input ended with an incomplete sample\n");
        }

        actual_size = fread(buffer, 1, request, input_fp);
        if (actual_size == 0u) {
            FAIL("read %s failed\n", input_path);
        }
        if ((actual_size % block_align) != 0u || actual_size > request) {
            FAIL("read an unaligned PCM block from %s\n", input_path);
        }

        st = wav_mux(&mux, buffer, (uint32_t)actual_size);
        if (st != AVP_OK) {
            FAIL("write WAV PCM failed: %d\n", (int)st);
        }
        input_offset += (uint32_t)actual_size;
    }

    pcm_size = wav_get_pcm_size(&mux);
    wav_size = wav_get_file_size(&mux);
    printf("encode done: %u PCM bytes -> %u WAV bytes, "
           "sample_rate=%u, bits=%u, channels=%u\n",
           (unsigned int)pcm_size,
           (unsigned int)wav_size,
           (unsigned int)sample_rate,
           (unsigned int)bits_per_sample,
           (unsigned int)channels);
    ret = 0;

out:
    if (mux_opened != 0) {
        wav_mux_close(&mux);
    }
    avp_free(buffer);
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
    uint32_t sample_rate;
    uint32_t bits_per_sample;
    uint32_t channels;

    avp_mem_init();
    if (argc != 6) {
        print_usage(argv[0]);
        return 1;
    }
    if (!parse_u32(argv[3], &sample_rate) ||
        !parse_u32(argv[4], &bits_per_sample) ||
        !parse_u32(argv[5], &channels)) {
        printf("invalid WAV parameters\n");
        return 1;
    }
    return run_encode(argv[1], argv[2], sample_rate, bits_per_sample, channels);
}
