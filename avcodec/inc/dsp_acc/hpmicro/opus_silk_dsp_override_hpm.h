/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef OPUS_SILK_DSP_OVERRIDE_HPM_H
#define OPUS_SILK_DSP_OVERRIDE_HPM_H

#include "nds32_intrinsic.h"

#ifdef __cplusplus
extern "C" {
#endif

static OPUS_INLINE opus_int32 hpm_opus_silk_smulwb(opus_int32 a32, opus_int32 b32)
{
   return (opus_int32)__nds__smmwb(a32, b32);
}

static OPUS_INLINE opus_int32 hpm_opus_silk_smlawb(opus_int32 a32, opus_int32 b32, opus_int32 c32)
{
   return a32 + hpm_opus_silk_smulwb(b32, c32);
}

static OPUS_INLINE opus_int32 hpm_opus_silk_smulwt(opus_int32 a32, opus_int32 b32)
{
   return (opus_int32)__nds__smmwt(a32, b32);
}

static OPUS_INLINE opus_int32 hpm_opus_silk_smlawt(opus_int32 a32, opus_int32 b32, opus_int32 c32)
{
   return a32 + hpm_opus_silk_smulwt(b32, c32);
}

static OPUS_INLINE opus_int32 hpm_opus_silk_smulbb(opus_int32 a32, opus_int32 b32)
{
   return (opus_int32)__nds__smbb16(a32, b32);
}

static OPUS_INLINE opus_int32 hpm_opus_silk_smlabb(opus_int32 a32, opus_int32 b32, opus_int32 c32)
{
   return a32 + hpm_opus_silk_smulbb(b32, c32);
}

static OPUS_INLINE opus_int32 hpm_opus_silk_smulbt(opus_int32 a32, opus_int32 b32)
{
   return (opus_int32)__nds__smbt16(a32, b32);
}

static OPUS_INLINE opus_int32 hpm_opus_silk_smlabt(opus_int32 a32, opus_int32 b32, opus_int32 c32)
{
   return a32 + hpm_opus_silk_smulbt(b32, c32);
}

static OPUS_INLINE opus_int32 hpm_opus_silk_add_sat32(opus_int32 a32, opus_int32 b32)
{
   return (opus_int32)__nds__kaddw(a32, b32);
}

static OPUS_INLINE opus_int32 hpm_opus_silk_sub_sat32(opus_int32 a32, opus_int32 b32)
{
   return (opus_int32)__nds__ksubw(a32, b32);
}

static OPUS_INLINE opus_int32 silk_CLZ16(opus_int16 in16)
{
   return (opus_int32)__nds__clz32(((opus_int32)((opus_uint32)(opus_uint16)in16 << 16)) | 0x8000);
}

static OPUS_INLINE opus_int32 silk_CLZ32(opus_int32 in32)
{
   return in32 ? (opus_int32)__nds__clz32(in32) : 32;
}

#undef silk_SMULWB
#define silk_SMULWB(a32, b32) (hpm_opus_silk_smulwb((a32), (b32)))

#undef silk_SMLAWB
#define silk_SMLAWB(a32, b32, c32) (hpm_opus_silk_smlawb((a32), (b32), (c32)))

#undef silk_SMULWT
#define silk_SMULWT(a32, b32) (hpm_opus_silk_smulwt((a32), (b32)))

#undef silk_SMLAWT
#define silk_SMLAWT(a32, b32, c32) (hpm_opus_silk_smlawt((a32), (b32), (c32)))

#undef silk_SMULBB
#define silk_SMULBB(a32, b32) (hpm_opus_silk_smulbb((a32), (b32)))

#undef silk_SMLABB
#define silk_SMLABB(a32, b32, c32) (hpm_opus_silk_smlabb((a32), (b32), (c32)))

#undef silk_SMULBT
#define silk_SMULBT(a32, b32) (hpm_opus_silk_smulbt((a32), (b32)))

#undef silk_SMLABT
#define silk_SMLABT(a32, b32, c32) (hpm_opus_silk_smlabt((a32), (b32), (c32)))

#undef silk_ADD_SAT32
#define silk_ADD_SAT32(a32, b32) (hpm_opus_silk_add_sat32((a32), (b32)))

#undef silk_SUB_SAT32
#define silk_SUB_SAT32(a32, b32) (hpm_opus_silk_sub_sat32((a32), (b32)))

#define OVERRIDE_silk_CLZ16
#define OVERRIDE_silk_CLZ32

#ifdef __cplusplus
}
#endif

#endif /* OPUS_SILK_DSP_OVERRIDE_HPM_H */
