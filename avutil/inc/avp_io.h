/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AVP_IO_H
#define AVP_IO_H

#include <stdint.h>
#include <stddef.h>
#include "avp_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct avp_io avp_io_t;

/*
 * A positive return value is the number of bytes transferred. Zero means EOF
 * (read) or no progress (write), and a negative value is an I/O error.
 */
typedef int (*avp_io_read_cb)(avp_io_t *avp_io,
                             uint8_t *buffer,
                             uint32_t size);
typedef int (*avp_io_write_cb)(avp_io_t *avp_io,
                              const uint8_t *buffer,
                              uint32_t size);
typedef int (*avp_io_seek_cb)(avp_io_t *avp_io,
                             uint32_t offset);
typedef int (*avp_io_get_size_cb)(avp_io_t *avp_io);

struct avp_io {
    avp_io_read_cb read;
    avp_io_write_cb write;
    avp_io_seek_cb seek;
    avp_io_get_size_cb get_size;
    void *priv;
};

void avp_io_init(avp_io_t *avp_io,
                 avp_io_read_cb read,
                 avp_io_write_cb write,
                 avp_io_seek_cb seek,
                 avp_io_get_size_cb get_size,
                 void *priv);
int avp_io_read(avp_io_t *avp_io,
                uint8_t *buffer,
                uint32_t size);
int avp_io_write(avp_io_t *avp_io,
                 const uint8_t *buffer,
                 uint32_t size);
int avp_io_seek(avp_io_t *avp_io,
                uint32_t offset);
int avp_io_get_size(avp_io_t *avp_io);

static inline int avp_io_read_at(avp_io_t *avp_io,
                                 uint32_t offset,
                                 uint8_t *buffer,
                                 uint32_t size)
{
    int ret;

    ret = avp_io_seek(avp_io, offset);
    if (ret != AVP_OK) {
        return ret;
    }
    return avp_io_read(avp_io, buffer, size);
}

static inline int avp_io_write_at(avp_io_t *avp_io,
                                  uint32_t offset,
                                  const uint8_t *buffer,
                                  uint32_t size)
{
    int ret;

    ret = avp_io_seek(avp_io, offset);
    if (ret != AVP_OK) {
        return ret;
    }
    return avp_io_write(avp_io, buffer, size);
}

#ifdef __cplusplus
}
#endif

#endif
