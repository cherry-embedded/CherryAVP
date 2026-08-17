/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#if !defined(CONFIG_AVP_WEBRTC_OVERRIDE)
#error "This file is only used when CONFIG_AVP_WEBRTC_OVERRIDE is defined"
#endif

#include "hpm_math.h"
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"

int16_t WebRtcSpl_MaxAbsValueW16_hpm(const int16_t *vector, size_t length)
{
    return hpm_dsp_absmax_q15((const q15_t *)vector, (uint32_t)length, NULL);
}

int32_t WebRtcSpl_MaxAbsValueW32_hpm(const int32_t *vector, size_t length)
{
    return hpm_dsp_absmax_q31((const q31_t *)vector, (uint32_t)length, NULL);
}

int16_t WebRtcSpl_MaxValueW16_hpm(const int16_t *vector, size_t length)
{
    return hpm_dsp_max_q15((const q15_t *)vector, (uint32_t)length, NULL);
}

int32_t WebRtcSpl_MaxValueW32_hpm(const int32_t *vector, size_t length)
{
    return hpm_dsp_max_q31((const q31_t *)vector, (uint32_t)length, NULL);
}

int16_t WebRtcSpl_MinValueW16_hpm(const int16_t *vector, size_t length)
{
    return hpm_dsp_min_q15((const q15_t *)vector, (uint32_t)length, NULL);
}

int32_t WebRtcSpl_MinValueW32_hpm(const int32_t *vector, size_t length)
{
    return hpm_dsp_min_q31((const q31_t *)vector, (uint32_t)length, NULL);
}

size_t WebRtcSpl_MaxAbsIndexW16_hpm(const int16_t *vector, size_t length)
{
    uint32_t index = 0;

    (void)hpm_dsp_absmax_q15((const q15_t *)vector, (uint32_t)length, &index);
    return index;
}

size_t WebRtcSpl_MaxIndexW16_hpm(const int16_t *vector, size_t length)
{
    uint32_t index = 0;

    (void)hpm_dsp_max_q15((const q15_t *)vector, (uint32_t)length, &index);
    return index;
}

size_t WebRtcSpl_MaxIndexW32_hpm(const int32_t *vector, size_t length)
{
    uint32_t index = 0;

    (void)hpm_dsp_max_q31((const q31_t *)vector, (uint32_t)length, &index);
    return index;
}

size_t WebRtcSpl_MinIndexW16_hpm(const int16_t *vector, size_t length)
{
    uint32_t index = 0;

    (void)hpm_dsp_min_q15((const q15_t *)vector, (uint32_t)length, &index);
    return index;
}

size_t WebRtcSpl_MinIndexW32_hpm(const int32_t *vector, size_t length)
{
    uint32_t index = 0;

    (void)hpm_dsp_min_q31((const q31_t *)vector, (uint32_t)length, &index);
    return index;
}

void WebRtcSpl_Init(void)
{
    // WebRtcSpl_MaxAbsValueW16 = WebRtcSpl_MaxAbsValueW16_hpm;
    // WebRtcSpl_MaxAbsValueW32 = WebRtcSpl_MaxAbsValueW32_hpm;
    // WebRtcSpl_MaxValueW16 = WebRtcSpl_MaxValueW16_hpm;
    // WebRtcSpl_MaxValueW32 = WebRtcSpl_MaxValueW32_hpm;
    // WebRtcSpl_MinValueW16 = WebRtcSpl_MinValueW16_hpm;
    // WebRtcSpl_MinValueW32 = WebRtcSpl_MinValueW32_hpm;
    // WebRtcSpl_CrossCorrelation = WebRtcSpl_CrossCorrelation_hpm;
    // WebRtcSpl_DownsampleFast = WebRtcSpl_DownsampleFast_hpm;
    // WebRtcSpl_ScaleAndAddVectorsWithRound =
    //     WebRtcSpl_ScaleAndAddVectorsWithRound_hpm;
}