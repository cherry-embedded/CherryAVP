/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "avp_packet.h"

#define AVP_PACKET_ALIGN_SIZE 1024u

avp_status_t avp_packet_expand(avp_packet_t *packet, uint32_t capacity)
{
    uint8_t *buffer;
    uint32_t new_capacity;

    if (packet == NULL) {
        return AVP_EINVAL;
    }

    if (capacity <= packet->capacity && packet->buf != NULL) {
        return AVP_OK;
    }

    new_capacity = (capacity + AVP_PACKET_ALIGN_SIZE - 1u) & ~(AVP_PACKET_ALIGN_SIZE - 1u);
    buffer = (uint8_t *)avp_realloc(packet->buf, new_capacity);
    if (buffer == NULL) {
        return AVP_ENOMEM;
    }

    packet->buf = buffer;
    packet->capacity = new_capacity;
    return AVP_OK;
}

avp_packet_t *avp_packet_alloc(uint32_t capacity)
{
    avp_packet_t *packet;
    avp_status_t st;

    packet = (avp_packet_t *)avp_malloc(sizeof(*packet));
    if (packet == NULL) {
        return NULL;
    }
    memset(packet, 0, sizeof(*packet));

    st = avp_packet_expand(packet, capacity);
    if (st != AVP_OK) {
        avp_free(packet->buf);
        avp_free(packet);
        return NULL;
    }

    return packet;
}

void avp_packet_free(avp_packet_t *packet)
{
    if (packet == NULL) {
        return;
    }

    avp_free(packet->buf);
    avp_free(packet);
}
