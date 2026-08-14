/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AVP_PACKET_H
#define AVP_PACKET_H

#include "avp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AVP_PACKET_TYPE_UNKNOWN = 0,
    AVP_PACKET_TYPE_VIDEO = 1,
    AVP_PACKET_TYPE_AUDIO = 2
} avp_packet_type_t;

typedef struct {
    avp_packet_type_t type;
    uint8_t *buf;
    uint32_t size;
    uint32_t capacity;
    uint32_t offset;
    uint32_t index;
    bool eof;
} avp_packet_t;

avp_packet_t *avp_packet_alloc(uint32_t capacity);
avp_status_t avp_packet_expand(avp_packet_t *packet, uint32_t capacity);
void avp_packet_free(avp_packet_t *packet);

#ifdef __cplusplus
}
#endif

#endif
