#
# Copyright (c) 2026, sakumisu
#
# SPDX-License-Identifier: Apache-2.0
#
cmake_minimum_required(VERSION 3.16)

if(HPM_SDK_BASE)
include(${CMAKE_CURRENT_LIST_DIR}/cherryavp.cmake)

if(TARGET CherryAVP)
    set_target_properties(CherryAVP PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

sdk_src(${CHERRYAVP_TARGET_SRC})
sdk_compile_definitions(${CHERRYAVP_TARGET_PUBLIC_DEFINITION} ${CHERRYAVP_TARGET_PRIVATE_DEFINITION})
sdk_inc(${CHERRYAVP_TARGET_PUBLIC_INCLUDE} ${CHERRYAVP_TARGET_PRIVATE_INCLUDE})

sdk_compile_options(${CHERRYAVP_TARGET_PRIVATE_OPTIONS})

if(CONFIG_CHERRYAVP_WEBRTC_OVERRIDE)
sdk_src(
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp_acc/hpmicro/webrtc_aec_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp_acc/hpmicro/webrtc_aec_rdft.c
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp_acc/hpmicro/webrtc_nsx_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp_acc/hpmicro/webrtc_complex_bit_reverse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp_acc/hpmicro/webrtc_complex_fft.c
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp_acc/hpmicro/webrtc_min_max_operations.c
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp_acc/hpmicro/webrtc_spl_sqrt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp_acc/hpmicro/webrtc_spl_sqrt_floor.c
)
endif()

endif()