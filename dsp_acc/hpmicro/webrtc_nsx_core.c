/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#if !defined(CONFIG_AVP_WEBRTC_OVERRIDE)
#error "This file is only used when CONFIG_AVP_WEBRTC_OVERRIDE is defined"
#endif

#include <assert.h>
#include <string.h>

#include "webrtc/modules/audio_processing/ns/noise_suppression_x.h"
#include "webrtc/modules/audio_processing/ns/nsx_core.h"

void WebRtcNsx_Init_custom()
{
    // Initialize function pointers.
    // WebRtcNsx_NoiseEstimation = NoiseEstimationC;
    // WebRtcNsx_PrepareSpectrum = PrepareSpectrumC;
    // WebRtcNsx_SynthesisUpdate = SynthesisUpdateC;
    // WebRtcNsx_AnalysisUpdate = AnalysisUpdateC;
    // WebRtcNsx_Denormalize = DenormalizeC;
    // WebRtcNsx_NormalizeRealBuffer = NormalizeRealBufferC;
}
