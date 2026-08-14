/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "avp_io.h"

void avp_io_init(avp_io_t *avp_io,
                 avp_io_read_cb read,
                 avp_io_write_cb write,
                 avp_io_seek_cb seek,
                 avp_io_get_size_cb get_size,
                 void *priv)
{
    if (avp_io == NULL) {
        return;
    }

    avp_io->read = read;
    avp_io->write = write;
    avp_io->seek = seek;
    avp_io->get_size = get_size;
    avp_io->priv = priv;
}

int avp_io_read(avp_io_t *avp_io,
                uint8_t *buffer,
                uint32_t size)
{
    int ret;
    if (avp_io == NULL || avp_io->read == NULL ||
        (buffer == NULL && size != 0u)) {
        return AVP_EINVAL;
    }

    if (size == 0u) {
        return 0;
    }

    ret = avp_io->read(avp_io, buffer, size);
    if (ret < 0) {
        return AVP_IO;
    }
    if ((uint32_t)ret < size) {
        return AVP_ENOENT;
    } else {
        return AVP_OK;
    }
}

int avp_io_write(avp_io_t *avp_io,
                 const uint8_t *buffer,
                 uint32_t size)
{
    int ret;
    if (avp_io == NULL || avp_io->write == NULL || buffer == NULL) {
        return AVP_EINVAL;
    }

    ret = avp_io->write(avp_io, buffer, size);
    if (ret < 0) {
        return AVP_IO;
    }

    return ret;
}

int avp_io_seek(avp_io_t *avp_io,
                uint32_t offset)
{
    if (avp_io == NULL || avp_io->seek == NULL) {
        return AVP_EINVAL;
    }

    return avp_io->seek(avp_io, offset) < 0 ? AVP_IO : AVP_OK;
}

int avp_io_get_size(avp_io_t *avp_io)
{
    if (avp_io == NULL || avp_io->get_size == NULL) {
        return AVP_EINVAL;
    }

    int ret = avp_io->get_size(avp_io);

    return ret <= 0 ? AVP_IO : ret;
}
