/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MP3_DSP_OVERRIDE_HPM_H
#define MP3_DSP_OVERRIDE_HPM_H

#include "nds32_intrinsic.h"

#define HAVE_SSE 0

#if defined(__riscv_vector) && (__riscv_vector == 1)
#define HAVE_SIMD 1
typedef float f4 __attribute__((vector_size(16), aligned(4)));
typedef float hpm_mp3_f32x2_t __attribute__((vector_size(8), aligned(4)));
typedef int32_t hpm_mp3_i32x4_t __attribute__((vector_size(16), aligned(4)));
typedef uint32_t hpm_mp3_u32x4_t __attribute__((vector_size(16), aligned(4)));
typedef int16_t int16x4_t __attribute__((vector_size(8), aligned(2)));

static __inline__ __attribute__((always_inline)) f4 hpm_mp3_vld(const float *src)
{
    f4 v;

    memcpy(&v, src, sizeof(v));
    return v;
}

static __inline__ __attribute__((always_inline)) void hpm_mp3_vstore(float *dst, f4 v)
{
    memcpy(dst, &v, sizeof(v));
}

static __inline__ __attribute__((always_inline)) f4 hpm_mp3_vset(float s)
{
    return (f4) { s, s, s, s };
}

static __inline__ __attribute__((always_inline)) f4 hpm_mp3_vrev(f4 x)
{
    return (f4) { x[3], x[2], x[1], x[0] };
}

static __inline__ __attribute__((always_inline)) hpm_mp3_f32x2_t vget_low_f32(f4 x)
{
    return (hpm_mp3_f32x2_t) { x[0], x[1] };
}

static __inline__ __attribute__((always_inline)) void vst1_f32(float *dst, hpm_mp3_f32x2_t v)
{
    dst[0] = v[0];
    dst[1] = v[1];
}

static __inline__ __attribute__((always_inline)) hpm_mp3_u32x4_t vcltq_f32(f4 a, f4 b)
{
    return (hpm_mp3_u32x4_t) {
        a[0] < b[0] ? UINT32_MAX : 0u,
        a[1] < b[1] ? UINT32_MAX : 0u,
        a[2] < b[2] ? UINT32_MAX : 0u,
        a[3] < b[3] ? UINT32_MAX : 0u,
    };
}

static __inline__ __attribute__((always_inline)) hpm_mp3_i32x4_t vreinterpretq_s32_u32(hpm_mp3_u32x4_t x)
{
    return (hpm_mp3_i32x4_t) {
        (int32_t)x[0],
        (int32_t)x[1],
        (int32_t)x[2],
        (int32_t)x[3],
    };
}

static __inline__ __attribute__((always_inline)) int32_t hpm_mp3_float_to_i32(float x)
{
    if (x >= 2147483520.0f) {
        return INT32_MAX;
    }
    if (x <= -2147483648.0f) {
        return INT32_MIN;
    }
    return (int32_t)x;
}

static __inline__ __attribute__((always_inline)) hpm_mp3_i32x4_t vcvtq_s32_f32(f4 x)
{
    return (hpm_mp3_i32x4_t) {
        hpm_mp3_float_to_i32(x[0]),
        hpm_mp3_float_to_i32(x[1]),
        hpm_mp3_float_to_i32(x[2]),
        hpm_mp3_float_to_i32(x[3]),
    };
}

static __inline__ __attribute__((always_inline)) int32_t hpm_mp3_sat_add_i32(int32_t a, int32_t b)
{
    return __nds__kaddw(a, b);
}

static __inline__ __attribute__((always_inline)) hpm_mp3_i32x4_t vqaddq_s32(hpm_mp3_i32x4_t a,
                                                                           hpm_mp3_i32x4_t b)
{
    return (hpm_mp3_i32x4_t) {
        hpm_mp3_sat_add_i32(a[0], b[0]),
        hpm_mp3_sat_add_i32(a[1], b[1]),
        hpm_mp3_sat_add_i32(a[2], b[2]),
        hpm_mp3_sat_add_i32(a[3], b[3]),
    };
}

static __inline__ __attribute__((always_inline)) int16_t hpm_mp3_sat_i16(int32_t x)
{
    return (int16_t)__nds__sclip32(x, 15);
}

static __inline__ __attribute__((always_inline)) int16x4_t vqmovn_s32(hpm_mp3_i32x4_t x)
{
    return (int16x4_t) {
        hpm_mp3_sat_i16(x[0]),
        hpm_mp3_sat_i16(x[1]),
        hpm_mp3_sat_i16(x[2]),
        hpm_mp3_sat_i16(x[3]),
    };
}

static __inline__ __attribute__((always_inline)) void vst1_lane_s16(int16_t *dst,
                                                                    int16x4_t v,
                                                                    const int lane)
{
    *dst = v[lane];
}

static __inline__ __attribute__((always_inline)) void vst1q_lane_f32(float *dst,
                                                                     f4 v,
                                                                     const int lane)
{
    *dst = v[lane];
}

#define VSTORE(dst, v) hpm_mp3_vstore((dst), (v))
#define VLD(src)       hpm_mp3_vld((src))
#define VSET(s)        hpm_mp3_vset((s))
#define VADD(a, b)     ((a) + (b))
#define VSUB(a, b)     ((a) - (b))
#define VMUL(a, b)     ((a) * (b))
#define VMAC(a, x, y)  ((a) + ((x) * (y)))
#define VMSB(a, x, y)  ((a) - ((x) * (y)))
#define VMUL_S(x, s)   ((x) * hpm_mp3_vset((s)))
#define VREV(x)        hpm_mp3_vrev((x))

static int have_simd(void)
{
    return 1;
}

#else
#define HAVE_SIMD 0
#endif

#endif /* MP3_DSP_OVERRIDE_HPM_H */
