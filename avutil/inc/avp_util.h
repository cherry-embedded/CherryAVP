#ifndef AVP_UTIL_H
#define AVP_UTIL_H

#ifndef ARG_UNUSED
#define ARG_UNUSED(x) (void)(x)
#endif

#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define AVP_SET_LE16(buffer, value)                     \
    do {                                                \
        (buffer)[0] = (uint8_t)((value)&0xFF);          \
        (buffer)[1] = (uint8_t)(((value) >> 8) & 0xFF); \
    } while (0)

#define AVP_SET_LE32(buffer, value)                      \
    do {                                                 \
        (buffer)[0] = (uint8_t)((value)&0xFF);           \
        (buffer)[1] = (uint8_t)(((value) >> 8) & 0xFF);  \
        (buffer)[2] = (uint8_t)(((value) >> 16) & 0xFF); \
        (buffer)[3] = (uint8_t)(((value) >> 24) & 0xFF); \
    } while (0)

#define AVP_GET_LE16(buffer) \
    ((uint16_t)(buffer)[0] | ((uint16_t)(buffer)[1] << 8))

#define AVP_GET_LE32(buffer) \
    ((uint32_t)(buffer)[0] | ((uint32_t)(buffer)[1] << 8) | ((uint32_t)(buffer)[2] << 16) | ((uint32_t)(buffer)[3] << 24))

#define AVP_SET_BE16(field, value)            \
    do {                                      \
        (field)[0] = (uint8_t)((value) >> 8); \
        (field)[1] = (uint8_t)((value) >> 0); \
    } while (0)

#define AVP_SET_BE32(field, value)             \
    do {                                       \
        (field)[0] = (uint8_t)((value) >> 24); \
        (field)[1] = (uint8_t)((value) >> 16); \
        (field)[2] = (uint8_t)((value) >> 8);  \
        (field)[3] = (uint8_t)((value) >> 0);  \
    } while (0)

#define AVP_GET_BE16(field) \
    (((uint16_t)(field)[0] << 8) | ((uint16_t)(field)[1]))

#define AVP_GET_BE24(field) \
    (((uint32_t)(field)[0] << 16) | ((uint32_t)(field)[1] << 8) | ((uint32_t)(field)[2]))

#define AVP_GET_BE32(field) \
    (((uint32_t)(field)[0] << 24) | ((uint32_t)(field)[1] << 16) | ((uint32_t)(field)[2] << 8) | ((uint32_t)(field)[3] << 0))

#define AVP_GET_BE64(field)                                                                                                      \
    (((uint64_t)(field)[0] << 56) | ((uint64_t)(field)[1] << 48) | ((uint64_t)(field)[2] << 40) | ((uint64_t)(field)[3] << 32) | \
     ((uint64_t)(field)[4] << 24) | ((uint64_t)(field)[5] << 16) | ((uint64_t)(field)[6] << 8) | ((uint64_t)(field)[7]))

static inline void avp_fourcc_to_string(uint32_t id, char out[5])
{
    if (out == NULL) {
        return;
    }

    out[0] = (char)(id & 0xffu);
    out[1] = (char)((id >> 8) & 0xffu);
    out[2] = (char)((id >> 16) & 0xffu);
    out[3] = (char)((id >> 24) & 0xffu);
    out[4] = '\0';
}
#endif // AVP_UTIL_H
