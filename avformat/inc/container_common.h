#ifndef CONTAINER_COMMON_H
#define CONTAINER_COMMON_H

#include "audio_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    avp_io_t *avp_io;
    uint32_t file_size;
    uint32_t stream_offset;
    uint32_t stream_size;
    uint32_t current_offset;
    uint32_t packet_index;
} container_common_t;

#ifdef __cplusplus
}
#endif

#endif