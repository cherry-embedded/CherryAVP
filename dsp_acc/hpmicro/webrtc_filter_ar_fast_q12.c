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

void WebRtcSpl_FilterARFastQ12(const int16_t *data_in,
                               int16_t *data_out,
                               const int16_t *__restrict coefficients,
                               size_t coefficients_length,
                               size_t data_length)
{
}
