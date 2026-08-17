/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#if !defined(CONFIG_AVP_WEBRTC_OVERRIDE)
#error "This file is only used when CONFIG_AVP_WEBRTC_OVERRIDE is defined"
#endif

#include <math.h>
#include <string.h> // memset

#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/modules/audio_processing/aec/aec_common.h"
#include "webrtc/modules/audio_processing/aec/aec_core_internal.h"
#include "webrtc/modules/audio_processing/aec/aec_rdft.h"

void WebRtcAec_InitAec_custom(void)
{
    // Assembly optimization
    // WebRtcAec_FilterFar = FilterFar;
    // WebRtcAec_ScaleErrorSignal = ScaleErrorSignal;
    // WebRtcAec_FilterAdaptation = FilterAdaptation;
    // WebRtcAec_OverdriveAndSuppress = OverdriveAndSuppress;
    // WebRtcAec_ComfortNoise = ComfortNoise;
    // WebRtcAec_SubbandCoherence = SubbandCoherence;
    // WebRtcAec_StoreAsComplex = StoreAsComplex;
    // WebRtcAec_PartitionDelay = PartitionDelay;
    // WebRtcAec_WindowData = WindowData;
}