/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AVP_BITREADER_H
#define AVP_BITREADER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const uint8_t *buffer;
    uint32_t size;
    uint32_t bitpos;
} avp_bitreader_t;

static inline void avp_bitreader_init(avp_bitreader_t *br,
                                      const uint8_t *buffer,
                                      uint32_t size)
{
    br->buffer = buffer;
    br->size = size;
    br->bitpos = 0u;
}

static inline uint32_t avp_bitreader_bits_left(const avp_bitreader_t *br)
{
    uint64_t total_bits;

    if (br == NULL) {
        return 0u;
    }

    total_bits = (uint64_t)br->size * 8u;
    if ((uint64_t)br->bitpos >= total_bits) {
        return 0u;
    }

    total_bits -= br->bitpos;
    return total_bits > UINT32_MAX ? UINT32_MAX : (uint32_t)total_bits;
}

static inline int avp_bitreader_read(avp_bitreader_t *br, uint8_t bits, uint32_t *out)
{
    uint32_t value = 0u;
    uint8_t i;

    if (br == NULL || out == NULL || bits > 32u) {
        return 0;
    }

    if ((uint64_t)br->bitpos + bits > (uint64_t)br->size * 8u) {
        return 0;
    }

    for (i = 0u; i < bits; i++) {
        uint32_t byte_index = br->bitpos >> 3;
        uint8_t bit_index = (uint8_t)(7u - (br->bitpos & 7u));

        value = (value << 1) | ((br->buffer[byte_index] >> bit_index) & 1u);
        br->bitpos++;
    }

    *out = value;
    return 1;
}

static inline int avp_bitreader_read_signed(avp_bitreader_t *br,
                                            uint8_t bits,
                                            int32_t *out)
{
    uint32_t value;
    uint32_t sign_bit;

    if (out == NULL || bits == 0u || bits > 31u) {
        return 0;
    }

    if (!avp_bitreader_read(br, bits, &value)) {
        return 0;
    }

    sign_bit = 1u << (bits - 1u);
    if ((value & sign_bit) != 0u) {
        value |= ~((1u << bits) - 1u);
    }

    *out = (int32_t)value;
    return 1;
}

static inline int avp_bitreader_read_unary(avp_bitreader_t *br, uint32_t *out)
{
    uint32_t count = 0u;
    uint32_t bit;

    if (br == NULL || out == NULL) {
        return 0;
    }

    while (1) {
        if (!avp_bitreader_read(br, 1u, &bit)) {
            return 0;
        }
        if (bit != 0u) {
            break;
        }
        count++;
    }

    *out = count;
    return 1;
}

static inline int avp_bitreader_skip(avp_bitreader_t *br, uint32_t bits)
{
    if (br == NULL) {
        return 0;
    }

    if ((uint64_t)br->bitpos + bits > (uint64_t)br->size * 8u) {
        return 0;
    }

    br->bitpos += bits;
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif
