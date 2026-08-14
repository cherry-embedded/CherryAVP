/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef OPUS_CELT_DSP_OVERRIDE_HPM_H
#define OPUS_CELT_DSP_OVERRIDE_HPM_H

#include "nds32_intrinsic.h"

#ifdef __cplusplus
extern "C" {
#endif

static OPUS_INLINE opus_val32 hpm_opus_celt_mult16_32_q16(opus_val16 a, opus_val32 b)
{
   return (opus_val32)__nds__smmwb((opus_int32)b, (opus_int32)a);
}

static OPUS_INLINE opus_val32 hpm_opus_celt_mult16_32_p16(opus_val16 a, opus_val32 b)
{
   return (opus_val32)__nds__smmwb_u((opus_int32)b, (opus_int32)a);
}

static OPUS_INLINE opus_val32 hpm_opus_celt_mult16_32_q15(opus_val16 a, opus_val32 b)
{
   return (opus_val32)__nds__smmwb(SHL32(b, 1), (opus_int32)a);
}

static OPUS_INLINE opus_int64 hpm_opus_celt_smul64(opus_val32 a, opus_val32 b)
{
   return (opus_int64)__nds__smar64((opus_int64)0, (opus_int32)a, (opus_int32)b);
}

static OPUS_INLINE opus_val32 hpm_opus_celt_mult32_32_q16(opus_val32 a, opus_val32 b)
{
   return (opus_val32)(hpm_opus_celt_smul64(a, b) >> 16);
}

static OPUS_INLINE opus_val32 hpm_opus_celt_mult32_32_q31(opus_val32 a, opus_val32 b)
{
   return (opus_val32)(hpm_opus_celt_smul64(a, b) >> 31);
}

static OPUS_INLINE opus_val32 hpm_opus_celt_mult32_32_p31(opus_val32 a, opus_val32 b)
{
   return (opus_val32)((hpm_opus_celt_smul64(a, b) + 1073741824) >> 31);
}

static OPUS_INLINE opus_val32 hpm_opus_celt_mult32_32_q32(opus_val32 a, opus_val32 b)
{
   return (opus_val32)(hpm_opus_celt_smul64(a, b) >> 32);
}

static OPUS_INLINE opus_val32 hpm_opus_celt_mult32_32_32(opus_val32 a, opus_val32 b)
{
   return (opus_val32)hpm_opus_celt_smul64(a, b);
}

static OPUS_INLINE opus_val32 hpm_opus_celt_mult16_16su(opus_val16 a, opus_uint16 b)
{
   return (opus_val32)__nds__smar64((opus_int64)0, (opus_int32)a, (opus_int32)b);
}

static OPUS_INLINE opus_val32 hpm_opus_celt_mult16_16(opus_val16 a, opus_val16 b)
{
   return (opus_val32)__nds__smbb16((opus_int32)a, (opus_int32)b);
}

static OPUS_INLINE opus_val16 hpm_opus_celt_mult16_16_16(opus_val16 a, opus_val16 b)
{
   return EXTRACT16(hpm_opus_celt_mult16_16(a, b));
}

static OPUS_INLINE opus_val32 hpm_opus_celt_mult16_16_q(opus_val16 a, opus_val16 b, int shift)
{
   return (opus_val32)(hpm_opus_celt_mult16_16(a, b) >> shift);
}

static OPUS_INLINE opus_val32 hpm_opus_celt_mult16_16_p(opus_val16 a, opus_val16 b, int shift)
{
   return (opus_val32)((hpm_opus_celt_mult16_16(a, b) + (1 << (shift - 1))) >> shift);
}

static OPUS_INLINE opus_val32 hpm_opus_celt_mac16_16(opus_val32 c, opus_val16 a, opus_val16 b)
{
   return ADD32(c, hpm_opus_celt_mult16_16(a, b));
}

static OPUS_INLINE opus_val32 hpm_opus_celt_mac16_32_q15(opus_val32 c, opus_val16 a, opus_val32 b)
{
   return ADD32(c, __nds__smmwb(SHL32(b, 1), (opus_int32)a));
}

static OPUS_INLINE opus_val32 hpm_opus_celt_mac16_32_q16(opus_val32 c, opus_val16 a, opus_val32 b)
{
   return ADD32(c, __nds__smmwb((opus_int32)b, (opus_int32)a));
}

static OPUS_INLINE opus_val16 hpm_opus_celt_sig2word16(celt_sig x)
{
   return EXTRACT16(__nds__sclip32(PSHR32(x, SIG_SHIFT), 15));
}

static OPUS_INLINE opus_val16 hpm_opus_celt_saturate16(opus_val32 x)
{
   return EXTRACT16(__nds__sclip32(x, 15));
}

static OPUS_INLINE opus_val16 hpm_opus_celt_sround16(opus_val32 x, int shift)
{
   return EXTRACT16(__nds__sclip32(PSHR32(x, shift), 15));
}

#undef MULT16_16SU
#define MULT16_16SU(a, b) (hpm_opus_celt_mult16_16su((a), (b)))

#undef MULT16_32_Q16
#define MULT16_32_Q16(a, b) (hpm_opus_celt_mult16_32_q16((a), (b)))

#undef MULT16_32_P16
#define MULT16_32_P16(a, b) (hpm_opus_celt_mult16_32_p16((a), (b)))

#undef MULT16_32_Q15
#define MULT16_32_Q15(a, b) (hpm_opus_celt_mult16_32_q15((a), (b)))

#undef MULT32_32_Q16
#define MULT32_32_Q16(a, b) (hpm_opus_celt_mult32_32_q16((a), (b)))

#undef MULT32_32_Q31
#define MULT32_32_Q31(a, b) (hpm_opus_celt_mult32_32_q31((a), (b)))

#undef MULT32_32_P31
#define MULT32_32_P31(a, b) (hpm_opus_celt_mult32_32_p31((a), (b)))

#undef MULT32_32_P31_ovflw
#define MULT32_32_P31_ovflw(a, b) (hpm_opus_celt_mult32_32_p31((a), (b)))

#undef MULT32_32_Q32
#define MULT32_32_Q32(a, b) (hpm_opus_celt_mult32_32_q32((a), (b)))

#undef MULT32_32_32
#define MULT32_32_32(a, b) (hpm_opus_celt_mult32_32_32((a), (b)))

#undef MULT16_16_16
#define MULT16_16_16(a, b) (hpm_opus_celt_mult16_16_16((a), (b)))

#undef MULT16_16
#define MULT16_16(a, b) (hpm_opus_celt_mult16_16((a), (b)))

#undef MAC16_16
#define MAC16_16(c, a, b) (hpm_opus_celt_mac16_16((c), (a), (b)))

#undef MAC16_32_Q15
#define MAC16_32_Q15(c, a, b) (hpm_opus_celt_mac16_32_q15((c), (a), (b)))

#undef MAC16_32_Q16
#define MAC16_32_Q16(c, a, b) (hpm_opus_celt_mac16_32_q16((c), (a), (b)))

#undef MULT16_16_Q11_32
#define MULT16_16_Q11_32(a, b) (hpm_opus_celt_mult16_16_q((a), (b), 11))

#undef MULT16_16_Q11
#define MULT16_16_Q11(a, b) (hpm_opus_celt_mult16_16_q((a), (b), 11))

#undef MULT16_16_Q13
#define MULT16_16_Q13(a, b) (hpm_opus_celt_mult16_16_q((a), (b), 13))

#undef MULT16_16_Q14
#define MULT16_16_Q14(a, b) (hpm_opus_celt_mult16_16_q((a), (b), 14))

#undef MULT16_16_Q15
#define MULT16_16_Q15(a, b) (hpm_opus_celt_mult16_16_q((a), (b), 15))

#undef MULT16_16_P13
#define MULT16_16_P13(a, b) (hpm_opus_celt_mult16_16_p((a), (b), 13))

#undef MULT16_16_P14
#define MULT16_16_P14(a, b) (hpm_opus_celt_mult16_16_p((a), (b), 14))

#undef MULT16_16_P15
#define MULT16_16_P15(a, b) (hpm_opus_celt_mult16_16_p((a), (b), 15))

#undef SATURATE16
#define SATURATE16(x) (hpm_opus_celt_saturate16(x))

#undef SROUND16
#define SROUND16(x, a) (hpm_opus_celt_sround16((x), (a)))

#undef SIG2WORD16
#define SIG2WORD16(x) (hpm_opus_celt_sig2word16(x))

#ifdef __cplusplus
}
#endif

#endif /* OPUS_CELT_DSP_OVERRIDE_HPM_H */
