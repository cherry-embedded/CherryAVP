/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#if !defined(CONFIG_CHERRYAVP_WEBRTC_OVERRIDE)
#error "This file is only used when CONFIG_CHERRYAVP_WEBRTC_OVERRIDE is defined"
#endif

#include <assert.h>
#include <string.h>

#include "hpm_math.h"
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"

int16_t WebRtcSpl_MaxAbsValueW16C(const int16_t *vector, size_t length)
{
    return hpm_dsp_absmax_q15((const q15_t *)vector, (uint32_t)length, NULL);
}

int32_t WebRtcSpl_MaxAbsValueW32C(const int32_t *vector, size_t length)
{
    return hpm_dsp_absmax_q31((const q31_t *)vector, (uint32_t)length, NULL);
}

int16_t WebRtcSpl_MaxValueW16C(const int16_t *vector, size_t length)
{
    return hpm_dsp_max_q15((const q15_t *)vector, (uint32_t)length, NULL);
}

int32_t WebRtcSpl_MaxValueW32C(const int32_t *vector, size_t length)
{
    return hpm_dsp_max_q31((const q31_t *)vector, (uint32_t)length, NULL);
}

int16_t WebRtcSpl_MinValueW16C(const int16_t *vector, size_t length)
{
    return hpm_dsp_min_q15((const q15_t *)vector, (uint32_t)length, NULL);
}

int32_t WebRtcSpl_MinValueW32C(const int32_t *vector, size_t length)
{
    return hpm_dsp_min_q31((const q31_t *)vector, (uint32_t)length, NULL);
}

size_t WebRtcSpl_MaxAbsIndexW16(const int16_t *vector, size_t length)
{
    uint32_t index = 0;

    (void)hpm_dsp_absmax_q15((const q15_t *)vector, (uint32_t)length, &index);
    return index;
}

size_t WebRtcSpl_MaxIndexW16(const int16_t *vector, size_t length)
{
    uint32_t index = 0;

    (void)hpm_dsp_max_q15((const q15_t *)vector, (uint32_t)length, &index);
    return index;
}

size_t WebRtcSpl_MaxIndexW32(const int32_t *vector, size_t length)
{
    uint32_t index = 0;

    (void)hpm_dsp_max_q31((const q31_t *)vector, (uint32_t)length, &index);
    return index;
}

size_t WebRtcSpl_MinIndexW16(const int16_t *vector, size_t length)
{
    uint32_t index = 0;

    (void)hpm_dsp_min_q15((const q15_t *)vector, (uint32_t)length, &index);
    return index;
}

size_t WebRtcSpl_MinIndexW32(const int32_t *vector, size_t length)
{
    uint32_t index = 0;

    (void)hpm_dsp_min_q31((const q31_t *)vector, (uint32_t)length, &index);
    return index;
}
