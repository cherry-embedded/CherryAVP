/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mp4_container.h"
#include "tlsf_port.h"

#define PACKET_BUFFER_SIZE (16u * 1024u)
#define OUTPUT_BUFFER_SIZE (8u * 1024u)

#define FAIL(...)            \
    do {                     \
        printf(__VA_ARGS__); \
        goto out;            \
    } while (0)

static int file_io_read(avp_io_t *io,
                        uint8_t *buffer,
                        uint32_t size)
{
    FILE *fp = (FILE *)io->priv;

    if (fp == NULL || buffer == NULL) {
        return -1;
    }
    return (int)fread(buffer, 1u, size, fp);
}

static int file_io_seek(avp_io_t *io, uint32_t offset)
{
    FILE *fp = (FILE *)io->priv;

    if (fp == NULL) {
        return -1;
    }
    return fseek(fp, (long)offset, SEEK_SET);
}

static int file_io_get_size(avp_io_t *io)
{
    FILE *fp = (FILE *)io->priv;
    int current;
    int size;

    if (fp == NULL) {
        return 0u;
    }
    current = ftell(fp);
    if (current < 0L || fseek(fp, 0L, SEEK_END) != 0) {
        return 0u;
    }
    size = ftell(fp);
    if (size < 0L || fseek(fp, current, SEEK_SET) != 0) {
        return 0u;
    }
    return (uint32_t)size;
}

static int parse_u32(const char *text, uint32_t *out)
{
    char *end = NULL;
    unsigned long value;

    if (text == NULL || out == NULL || text[0] == '\0') {
        return 0;
    }

    value = strtoul(text, &end, 0);
    if (end == text || *end != '\0' || value > UINT32_MAX) {
        return 0;
    }
    *out = (uint32_t)value;
    return 1;
}

static void print_usage(const char *program)
{
    printf("usage: %s <input.mp4> <output_dir> [packet_count]\n", program);
}

static void print_stream_info(const mp4_demux_t *demuxer)
{
    char sample_entry[5];

    printf("MP4 File:\n");
    printf("  file_size  : %u\n", (unsigned int)mp4_get_file_size(demuxer));
    printf("  streams    : %u\n", (unsigned int)demuxer->stream_count);
    if (demuxer->has_video != 0u) {
        avp_fourcc_to_string(demuxer->video.sample_entry, sample_entry);
        printf("Video Stream:\n");
        printf("  stream_index: %u\n", (unsigned int)demuxer->video.stream_index);
        printf("  codec       : %s (%s)\n",
               mp4_video_codec_name(demuxer->video.codec_type),
               sample_entry);
        printf("  size        : %ux%u\n",
               (unsigned int)demuxer->video.width,
               (unsigned int)demuxer->video.height);
        printf("  samples     : %u\n", (unsigned int)demuxer->video.sample_count);
    }
    if (demuxer->has_audio != 0u) {
        avp_fourcc_to_string(demuxer->audio.sample_entry, sample_entry);
        printf("Audio Stream:\n");
        printf("  stream_index: %u\n", (unsigned int)demuxer->audio.stream_index);
        printf("  codec       : %s (%s)\n",
               mp4_audio_codec_name(demuxer->audio.codec_type),
               sample_entry);
        printf("  sample_rate : %u Hz\n", (unsigned int)demuxer->audio.sample_rate);
        printf("  channels    : %u\n", (unsigned int)demuxer->audio.channels);
        printf("  bits        : %u\n", (unsigned int)demuxer->audio.bits_per_sample);
        printf("  samples     : %u\n", (unsigned int)demuxer->audio.sample_count);
    }
}

static const char *packet_type_name(avp_packet_type_t type)
{
    switch (type) {
        case AVP_PACKET_TYPE_VIDEO:
            return "video";
        case AVP_PACKET_TYPE_AUDIO:
            return "audio";
        default:
            return "unknown";
    }
}

static void print_packet(const avp_packet_t *packet, uint32_t packet_index)
{
    printf("packet[%u]: type=%s size=%u stream=%u offset=%u\n",
           (unsigned int)packet_index,
           packet_type_name(packet->type),
           (unsigned int)packet->size,
           (unsigned int)packet->index,
           (unsigned int)packet->offset);
}

static int write_video_frame(const char *output_dir,
                             uint32_t frame_index,
                             const avp_packet_t *packet)
{
    char path[512];
    FILE *fp;
    size_t actual_size;
    int length;

    length = snprintf(path,
                      sizeof(path),
                      "%s/video_%06u_s%u.jpg",
                      output_dir,
                      (unsigned int)frame_index,
                      (unsigned int)packet->index);
    if (length <= 0 || (size_t)length >= sizeof(path)) {
        return -1;
    }

    fp = fopen(path, "wb");
    if (fp == NULL) {
        return -1;
    }
    actual_size = fwrite(packet->buf, 1, packet->size, fp);
    if (fclose(fp) != 0 || actual_size != packet->size) {
        return -1;
    }
    return 0;
}

static int write_pcm(FILE *fp, const audio_codec_dec_out_frame_t *frame)
{
    size_t samples;

    if (fp == NULL || frame == NULL || frame->pcm_size == 0u) {
        return 0;
    }
    samples = frame->pcm_size / sizeof(int16_t);
    return fwrite(frame->buffer, sizeof(int16_t), samples, fp) == samples ? 0 : -1;
}

static int run_demo(const char *input_path,
                    const char *output_dir,
                    uint32_t packet_limit)
{
    FILE *in_fp = NULL;
    FILE *pcm_fp = NULL;
    avp_io_t avp_io;
    mp4_demux_t demuxer;
    audio_codec_dec_handle_t audio_decoder = NULL;
    audio_codec_dec_config_t audio_config;
    uint8_t *packet_buffer = NULL;
    uint32_t packet_capacity = PACKET_BUFFER_SIZE;
    int16_t *pcm_buffer = NULL;
    uint32_t packet_count = 0u;
    uint32_t video_count = 0u;
    uint32_t audio_count = 0u;
    uint32_t video_bytes = 0u;
    uint32_t audio_bytes = 0u;
    uint32_t pcm_bytes = 0u;
    avp_status_t st = AVP_OK;
    int ret = 1;

    memset(&avp_io, 0, sizeof(avp_io));
    memset(&demuxer, 0, sizeof(demuxer));
    memset(&audio_config, 0, sizeof(audio_config));

    in_fp = fopen(input_path, "rb");
    if (in_fp == NULL) {
        FAIL("open %s failed\n", input_path);
    }
    packet_buffer = avp_malloc(PACKET_BUFFER_SIZE);
    if (packet_buffer == NULL) {
        FAIL("alloc buffer failed\n");
    }

    avp_io_init(&avp_io, file_io_read, NULL, file_io_seek, file_io_get_size, in_fp);

    st = mp4_demux_open(&demuxer, &avp_io);
    if (st != AVP_OK) {
        FAIL("open mp4 failed: %d\n", (int)st);
    }
    print_stream_info(&demuxer);
    printf("  output_dir : %s\n", output_dir);

    if (demuxer.has_audio != 0u) {
        st = mp4_demux_get_audio_stream_config(&demuxer, &audio_config);
        if (st != AVP_OK && st != AVP_EUNSUPPORTED) {
            FAIL("get mp4 audio config failed: %d\n", (int)st);
        }
        if (st == AVP_OK) {
            char path[512];
            int length;

            audio_decoder = audio_codec_dec_open(&audio_config);
            if (audio_decoder == NULL) {
                FAIL("open %s decoder failed\n",
                     mp4_audio_codec_name(demuxer.audio.codec_type));
            }
            length = snprintf(path, sizeof(path), "%s/audio.pcm", output_dir);

            if (length <= 0 || (size_t)length >= sizeof(path)) {
                FAIL("audio output path is too long\n");
            }
            pcm_fp = fopen(path, "wb");
            if (pcm_fp == NULL) {
                FAIL("open %s failed\n", path);
            }
        }
    }

    if (audio_decoder != NULL) {
        pcm_buffer = (int16_t *)avp_malloc(OUTPUT_BUFFER_SIZE);
        if (pcm_buffer == NULL) {
            FAIL("alloc PCM buffer failed\n");
        }
    }

    while (packet_limit == 0u || packet_count < packet_limit) {
        avp_packet_t packet;
        uint8_t *new_buffer;

        st = mp4_demux_peek_packet(&demuxer, &packet);
        if (st == AVP_ENOENT) {
            break;
        }
        if (st != AVP_OK) {
            FAIL("peek mp4 packet failed: %d\n", (int)st);
        }
        print_packet(&packet, packet_count);

        if (packet.size > packet_capacity) {
            new_buffer = (uint8_t *)avp_realloc(packet_buffer, packet.size);
            if (new_buffer == NULL) {
                FAIL("alloc packet buffer failed\n");
            }
            packet_buffer = new_buffer;
            packet_capacity = packet.size;
        }
        packet.buf = packet_buffer;
        st = mp4_demux_pop_packet(&demuxer, &packet);
        if (st != AVP_OK) {
            FAIL("pop mp4 packet failed: %d\n", (int)st);
        }

        if (packet.type == AVP_PACKET_TYPE_VIDEO) {
            if (demuxer.video.codec_type == MP4_VIDEO_CODEC_MJPEG &&
                write_video_frame(output_dir, video_count, &packet) != 0) {
                FAIL("write video frame %u failed\n", (unsigned int)video_count);
            }
            video_count++;
            video_bytes += packet.size;
        } else if (packet.type == AVP_PACKET_TYPE_AUDIO) {
            if (audio_decoder != NULL) {
                audio_codec_dec_in_frame_t in_frame;
                audio_codec_dec_out_frame_t out_frame;

                memset(&in_frame, 0, sizeof(in_frame));
                in_frame.buffer = packet.buf;
                in_frame.size = packet.size;
                memset(&out_frame, 0, sizeof(out_frame));
                out_frame.buffer = pcm_buffer;
                out_frame.size = OUTPUT_BUFFER_SIZE;
                st = audio_codec_dec_frame(audio_decoder, &in_frame, &out_frame);
                if (st != AVP_OK) {
                    FAIL("decode %s packet %u failed: %d\n",
                         mp4_audio_codec_name(demuxer.audio.codec_type),
                         (unsigned int)audio_count,
                         (int)st);
                }
                if (write_pcm(pcm_fp, &out_frame) != 0) {
                    FAIL("write audio PCM failed\n");
                }
                pcm_bytes += out_frame.pcm_size;
            }
            audio_count++;
            audio_bytes += packet.size;
        }
        packet_count++;
    }

    printf("demuxer done: %u packets, video=%u (%u bytes), audio=%u (%u bytes), pcm=%u bytes\n",
           (unsigned int)packet_count,
           (unsigned int)video_count,
           (unsigned int)video_bytes,
           (unsigned int)audio_count,
           (unsigned int)audio_bytes,
           (unsigned int)pcm_bytes);
    ret = 0;

out:
    audio_codec_dec_close(audio_decoder);
    mp4_demux_close(&demuxer);
    if (packet_buffer != NULL) {
        avp_free(packet_buffer);
    }
    if (pcm_buffer != NULL) {
        avp_free(pcm_buffer);
    }
    if (pcm_fp != NULL) {
        fclose(pcm_fp);
    }
    if (in_fp != NULL) {
        fclose(in_fp);
    }
    return ret;
}

int main(int argc, char **argv)
{
    uint32_t packet_limit = 0u;

    avp_mem_init();
    if (argc < 3 || argc > 4) {
        print_usage(argv[0]);
        return 1;
    }
    if (argc == 4 && !parse_u32(argv[3], &packet_limit)) {
        printf("invalid packet_count: %s\n", argv[3]);
        return 1;
    }
    return run_demo(argv[1], argv[2], packet_limit);
}
