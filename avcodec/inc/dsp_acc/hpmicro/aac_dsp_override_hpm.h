/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef AAC_DSP_OVERRIDE_HPM_H
#define AAC_DSP_OVERRIDE_HPM_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "pv_audio_type_defs.h"

#include "nds32_intrinsic.h"

#define preload_cache(a)

static inline Int32 hpm_aac_mul64_shift(const Int32 a, const Int32 b, const int shift)
{
    return (Int32)(((int64)__nds__smar64((int64)0, a, b)) >> shift);
}

static inline Int32 hpm_aac_sext16(const Int32 x)
{
    return (Int32)((Int16)x);
}

static inline Int32 hpm_aac_hi16(const Int32 x)
{
    return x >> 16;
}

static inline Int32 shft_lft_1(Int32 L_var1)
{
    return __nds__kaddw(L_var1, L_var1);
}

static inline Int32 fxp_mul_16_by_16bb(Int32 L_var1, Int32 L_var2)
{
    return hpm_aac_sext16(L_var1) * hpm_aac_sext16(L_var2);
}

#define fxp_mul_16_by_16(a, b) fxp_mul_16_by_16bb((a), (b))

static inline Int32 fxp_mul_16_by_16tb(Int32 L_var1, Int32 L_var2)
{
    return hpm_aac_hi16(L_var1) * hpm_aac_sext16(L_var2);
}

static inline Int32 fxp_mul_16_by_16bt(Int32 L_var1, Int32 L_var2)
{
    return hpm_aac_sext16(L_var1) * hpm_aac_hi16(L_var2);
}

static inline Int32 fxp_mul_16_by_16tt(Int32 L_var1, Int32 L_var2)
{
    return hpm_aac_hi16(L_var1) * hpm_aac_hi16(L_var2);
}

static inline Int32 fxp_mac_16_by_16(Int16 L_var1, Int16 L_var2, Int32 L_add)
{
    return L_add + (Int32)L_var1 * L_var2;
}

static inline Int32 fxp_mac_16_by_16_bb(Int16 L_var1, Int32 L_var2, Int32 L_add)
{
    return L_add + (Int32)L_var1 * hpm_aac_sext16(L_var2);
}

static inline Int32 fxp_mac_16_by_16_bt(Int16 L_var1, Int32 L_var2, Int32 L_add)
{
    return L_add + (Int32)L_var1 * hpm_aac_hi16(L_var2);
}

static inline Int32 cmplx_mul32_by_16(Int32 x, const Int32 y, Int32 exp_jw)
{
    return __nds__smmwt(x, exp_jw) + __nds__smmwb(y, exp_jw);
}

static inline Int32 fxp_mul32_by_16(Int32 L_var1, const Int32 L_var2)
{
    return __nds__smmwb(L_var1, L_var2);
}

#define fxp_mul32_by_16b(a, b) fxp_mul32_by_16((a), (b))

static inline Int32 fxp_mul32_by_16t(Int32 L_var1, const Int32 L_var2)
{
    return __nds__smmwt(L_var1, L_var2);
}

static inline Int32 fxp_mac32_by_16(const Int32 L_var1, const Int32 L_var2, Int32 L_add)
{
    return L_add + fxp_mul32_by_16(L_var1, L_var2);
}

static inline int64 fxp_mac64_Q31(int64 sum, const Int32 L_var1, const Int32 L_var2)
{
    return (int64)__nds__smar64(sum, L_var1, L_var2);
}

static inline Int32 fxp_mul32_Q31(const Int32 a, const Int32 b)
{
    return __nds__smmul(a, b);
}

static inline Int32 fxp_mac32_Q31(Int32 L_add, const Int32 a, const Int32 b)
{
    return L_add + fxp_mul32_Q31(a, b);
}

static inline Int32 fxp_msu32_Q31(Int32 L_sub, const Int32 a, const Int32 b)
{
    return L_sub - fxp_mul32_Q31(a, b);
}

static inline Int32 fxp_mul32_Q30(const Int32 a, const Int32 b)
{
    return hpm_aac_mul64_shift(a, b, 30);
}

static inline Int32 fxp_mac32_Q30(const Int32 a, const Int32 b, Int32 L_add)
{
    return L_add + fxp_mul32_Q30(a, b);
}

static inline Int32 fxp_mul32_Q29(const Int32 a, const Int32 b)
{
    return hpm_aac_mul64_shift(a, b, 29);
}

static inline Int32 fxp_mac32_Q29(const Int32 a, const Int32 b, Int32 L_add)
{
    return L_add + fxp_mul32_Q29(a, b);
}

static inline Int32 fxp_msu32_Q29(const Int32 a, const Int32 b, Int32 L_sub)
{
    return L_sub - fxp_mul32_Q29(a, b);
}

static inline Int32 fxp_mul32_Q28(const Int32 a, const Int32 b)
{
    return hpm_aac_mul64_shift(a, b, 28);
}

static inline Int32 fxp_mul32_Q27(const Int32 a, const Int32 b)
{
    return hpm_aac_mul64_shift(a, b, 27);
}

static inline Int32 fxp_mul32_Q26(const Int32 a, const Int32 b)
{
    return hpm_aac_mul64_shift(a, b, 26);
}

static inline Int32 fxp_mul32_Q20(const Int32 a, const Int32 b)
{
    return hpm_aac_mul64_shift(a, b, 20);
}

static inline Int32 fxp_mul32_Q15(const Int32 a, const Int32 b)
{
    return hpm_aac_mul64_shift(a, b, 15);
}

static inline Int32 fxp_mul32_Q14(const Int32 a, const Int32 b)
{
    return hpm_aac_mul64_shift(a, b, 14);
}

#ifdef __cplusplus
}
#endif

#endif /* AAC_DSP_OVERRIDE_HPM_H */
