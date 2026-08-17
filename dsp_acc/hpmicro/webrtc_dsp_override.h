/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef WEBRTC_DSP_OVERRIDE_H
#define WEBRTC_DSP_OVERRIDE_H

#include "nds32_intrinsic.h"

#undef WEBRTC_SPL_MUL_16_16
#undef WEBRTC_SPL_MUL_16_32_RSFT16
static __inline int32_t WEBRTC_SPL_MUL_16_16(int16_t a, int16_t b)
{
    return __nds__smbb16((int32_t)a, (int32_t)b);
}

static __inline int32_t WEBRTC_SPL_MUL_16_32_RSFT16(int16_t a, int32_t b)
{
    return __nds__smmwb(b, (int32_t)a);
}

static __inline int32_t WebRtc_MulAccumW16(int16_t a, int16_t b, int32_t c)
{
    return (int32_t)__nds__smalbb((int64_t)c, (int32_t)a, (int32_t)b);
}

static __inline int16_t WebRtcSpl_SatW32ToW16(int32_t value32)
{
    return (int16_t)__nds__sclip32(value32, 15);
}

static __inline int32_t WebRtcSpl_AddSatW32(int32_t l_var1, int32_t l_var2)
{
    return __nds__kaddw(l_var1, l_var2);
}

static __inline int32_t WebRtcSpl_SubSatW32(int32_t l_var1, int32_t l_var2)
{
    return __nds__ksubw(l_var1, l_var2);
}

static __inline int16_t WebRtcSpl_AddSatW16(int16_t a, int16_t b)
{
    return (int16_t)__nds__kaddh((int32_t)a, (int32_t)b);
}

static __inline int16_t WebRtcSpl_SubSatW16(int16_t var1, int16_t var2)
{
    return (int16_t)__nds__ksubh((int32_t)var1, (int32_t)var2);
}

static __inline int16_t WebRtcSpl_GetSizeInBits(uint32_t n)
{
    return n == 0 ? 0 : (int16_t)(32 - __builtin_clz(n));
}

static __inline int16_t WebRtcSpl_NormW32(int32_t a)
{
    uint32_t value;

    if (a == 0) {
        return 0;
    }

    value = (uint32_t)(a < 0 ? ~a : a);
    return (int16_t)(__builtin_clz(value) - 1);
}

static __inline int16_t WebRtcSpl_NormU32(uint32_t a)
{
    return a == 0 ? 0 : (int16_t)__builtin_clz(a);
}

static __inline int16_t WebRtcSpl_NormW16(int16_t a)
{
    int32_t value;

    if (a == 0) {
        return 0;
    }

    value = a < 0 ? ~(int32_t)a : (int32_t)a;
    return (int16_t)(__builtin_clz((uint32_t)value) - 17);
}

#endif /* WEBRTC_DSP_OVERRIDE_H */
