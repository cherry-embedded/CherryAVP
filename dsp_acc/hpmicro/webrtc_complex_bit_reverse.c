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

void WebRtcSpl_ComplexBitReverse(int16_t *__restrict complex_data, int stages)
{
}
