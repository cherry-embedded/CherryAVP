cmake_minimum_required(VERSION 3.16)

project(CherryAVExamples C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wundef -Wno-format -fomit-frame-pointer -fno-builtin -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wundef -Wno-format -fomit-frame-pointer -fno-builtin -ffunction-sections -fdata-sections")

set(CONFIG_CHERRYAVP_WAV 1)
set(CONFIG_CHERRYAVP_AVI 1)
set(CONFIG_CHERRYAVP_FLAC 1)
set(CONFIG_CHERRYAVP_ALAC 1)
set(CONFIG_CHERRYAVP_MP3 1)
set(CONFIG_CHERRYAVP_AAC 1)
set(CONFIG_CHERRYAVP_M4A 1)
set(CONFIG_CHERRYAVP_MP4 1)
set(CONFIG_CHERRYAVP_AMR 1)
set(CONFIG_CHERRYAVP_ADPCM 1)
set(CONFIG_CHERRYAVP_G711 1)
set(CONFIG_CHERRYAVP_G722 1)
set(CONFIG_CHERRYAVP_OPUS 1)
set(CONFIG_CHERRYAVP_VORBIS 1)
set(CONFIG_CHERRYAVP_OGG 1)
set(CONFIG_CHERRYAVP_SWR 1)
set(CONFIG_CHERRYAVP_AFE_3A 1)
set(CONFIG_CHERRYAVP_AE_SONIC 1)
set(CONFIG_CHERRYAVP_AE_VOL 1)
set(CONFIG_CHERRYAVP_AFE_HOWLING 1)
set(CONFIG_CHERRYAVP_AE_EQ 1)
set(CONFIG_CHERRYAVP_AE_FILTER 1)
set(CONFIG_CHERRYAVP_AE_MIXER 1)
set(CONFIG_CHERRYAVP_AE_REVERB 1)
set(CONFIG_CHERRYAVP_AE_COMPRESSOR 1)
set(CONFIG_CHERRYAVP_AE_LIMITER 1)
set(CONFIG_CHERRYAVP_MFCC 1)
#set(CONFIG_CHERRYAVP_MP3_DECODER_OPENCORE 1)
#set(CONFIG_CHERRYAVP_AAC_PLUS 1)
#set(CONFIG_CHERRYAVP_AFE_3A_NS_FIXED 1)

include(${CMAKE_CURRENT_LIST_DIR}/../cherryavp.cmake)

function(cherryavp_example target_name)
    add_executable(${target_name} ${ARGN} tlsf/tlsf.c tlsf/tlsf_port.c)
    target_link_libraries(${target_name} PRIVATE CherryAVP)
    target_include_directories(${target_name} PRIVATE tlsf)
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    )
endfunction()

cherryavp_example(audio_codec_stream_demo audio_codec_stream_demo.c)
cherryavp_example(resample_demo resample_demo.c)

if(CONFIG_CHERRYAVP_AFE_3A)
cherryavp_example(afe_3a_demo afe_3a_demo.c)
endif()

if(CONFIG_CHERRYAVP_AE_SONIC)
cherryavp_example(ae_sonic_demo ae_sonic_demo.c)
endif()

if(CONFIG_CHERRYAVP_AE_VOL)
cherryavp_example(ae_vol_demo ae_vol_demo.c)
endif()

if(CONFIG_CHERRYAVP_AFE_HOWLING)
cherryavp_example(afe_howling_demo afe_howling_demo.c)
endif()

if(CONFIG_CHERRYAVP_AE_EQ)
cherryavp_example(ae_eq_demo ae_eq_demo.c)
endif()

if(CONFIG_CHERRYAVP_AE_FILTER)
cherryavp_example(ae_filter_demo ae_filter_demo.c)
endif()

if(CONFIG_CHERRYAVP_AE_MIXER)
cherryavp_example(ae_mixer_demo ae_mixer_demo.c)
endif()

if(CONFIG_CHERRYAVP_AE_REVERB)
cherryavp_example(ae_reverb_demo ae_reverb_demo.c)
endif()

if(CONFIG_CHERRYAVP_AE_COMPRESSOR)
cherryavp_example(ae_compressor_demo ae_compressor_demo.c)
endif()

if(CONFIG_CHERRYAVP_AE_LIMITER)
cherryavp_example(ae_limiter_demo ae_limiter_demo.c)
endif()

if(CONFIG_CHERRYAVP_WAV)
    cherryavp_example(wav_encode_demo wav_encode_demo.c)
endif()

if(CONFIG_CHERRYAVP_MP4)
    cherryavp_example(mp4_demo mp4_demo.c)
endif()

if(CONFIG_CHERRYAVP_AVI)
    cherryavp_example(avi_demo avi_demo.c)
endif()

if(CONFIG_CHERRYAVP_MFCC)
    cherryavp_example(mfcc_demo mfcc_demo.c)
endif()
