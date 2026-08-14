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
endif()