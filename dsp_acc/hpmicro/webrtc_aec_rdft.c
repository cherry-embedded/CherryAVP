/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#if !defined(CONFIG_AVP_WEBRTC_OVERRIDE)
#error "This file is only used when CONFIG_AVP_WEBRTC_OVERRIDE is defined"
#endif

#include "webrtc/modules/audio_processing/aec/aec_rdft.h"

void aec_rdft_init_custom(void)
{
    // cft1st_128 = cft1st_128_C;
    // cftmdl_128 = cftmdl_128_C;
    // rftfsub_128 = rftfsub_128_C;
    // rftbsub_128 = rftbsub_128_C;
    // cftfsub_128 = cftfsub_128_C;
    // cftbsub_128 = cftbsub_128_C;
    // bitrv2_128 = bitrv2_128_C;
}