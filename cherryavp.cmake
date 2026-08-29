#
# Copyright (c) 2026, sakumisu
#
# SPDX-License-Identifier: Apache-2.0
#
if(CONFIG_CHERRYAVP_M4A OR CONFIG_CHERRYAVP_MP4)
    set(CONFIG_CHERRYAVP_AAC ON CACHE BOOL "Build CherryAVC AAC codec" FORCE)
    set(CONFIG_CHERRYAVP_ALAC ON CACHE BOOL "Build CherryAVP ALAC codec" FORCE)
endif()

set(CHERRYAVP_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})
set(CHERRYAVP_AVUTIL_DIR ${CHERRYAVP_ROOT_DIR}/avutil)
set(CHERRYAVP_AVFORMAT_DIR ${CHERRYAVP_ROOT_DIR}/avformat)
set(CHERRYAVP_AVCODEC_DIR ${CHERRYAVP_ROOT_DIR}/avcodec)
set(CHERRYAVP_AVFILTER_DIR ${CHERRYAVP_ROOT_DIR}/avfilter)
set(CHERRYAVP_AVSWRESAMPLE_DIR ${CHERRYAVP_ROOT_DIR}/avswresample)
set(CHERRYAVP_AVUTIL_INCLUDE_DIR ${CHERRYAVP_AVUTIL_DIR}/inc)
set(CHERRYAVP_AVFORMAT_INCLUDE_DIR ${CHERRYAVP_AVFORMAT_DIR}/inc)
set(CHERRYAVP_AVCODEC_INCLUDE_DIR ${CHERRYAVP_AVCODEC_DIR}/inc)
set(CHERRYAVP_AVFILTER_INCLUDE_DIR ${CHERRYAVP_AVFILTER_DIR}/inc)
set(CHERRYAVP_AVSWRESAMPLE_INCLUDE_DIR ${CHERRYAVP_AVSWRESAMPLE_DIR}/inc)
set(CHERRYAVP_AVUTIL_SRC_DIR ${CHERRYAVP_AVUTIL_DIR}/src)
set(CHERRYAVP_AVFORMAT_SRC_DIR ${CHERRYAVP_AVFORMAT_DIR}/src)
set(CHERRYAVP_AVCODEC_SRC_DIR ${CHERRYAVP_AVCODEC_DIR}/src)
set(CHERRYAVP_AVFILTER_SRC_DIR ${CHERRYAVP_AVFILTER_DIR}/src)
set(CHERRYAVP_AVSWRESAMPLE_SRC_DIR ${CHERRYAVP_AVSWRESAMPLE_DIR}/src)
set(CHERRYAVP_THIRD_PARTY_DIR ${CHERRYAVP_ROOT_DIR}/third_party)

find_library(CHERRYAVP_MATH_LIBRARY m)

set(CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVUTIL_SRC_DIR}/avp_io.c
    ${CHERRYAVP_AVUTIL_SRC_DIR}/avp_packet.c
    ${CHERRYAVP_AVUTIL_SRC_DIR}/avp_mem.cpp
    ${CHERRYAVP_AVCODEC_SRC_DIR}/audio_decode.c
    ${CHERRYAVP_AVCODEC_SRC_DIR}/pcm_decode.c
)
set(CHERRYAVP_TARGET_PUBLIC_INCLUDE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/dsp_acc
    ${CHERRYAVP_AVUTIL_INCLUDE_DIR}
    ${CHERRYAVP_AVFORMAT_INCLUDE_DIR}
    ${CHERRYAVP_AVCODEC_INCLUDE_DIR}
    ${CHERRYAVP_AVFILTER_INCLUDE_DIR}
    ${CHERRYAVP_AVSWRESAMPLE_INCLUDE_DIR}
)
set(CHERRYAVP_TARGET_PRIVATE_INCLUDE)
set(CHERRYAVP_TARGET_PUBLIC_DEFINITION)
set(CHERRYAVP_TARGET_PRIVATE_DEFINITION)
set(CHERRYAVP_TARGET_PRIVATE_OPTIONS)

set(OPENCORE_DIR ${CHERRYAVP_THIRD_PARTY_DIR}/opencore)
set(OPENCORE_PATCH_DIR ${CHERRYAVP_THIRD_PARTY_DIR}/opencore_patch/oscl_compat)
set(LIBOGG_DIR ${CHERRYAVP_THIRD_PARTY_DIR}/xiph-ogg)
set(LIBOGG_PATCH_DIR ${CHERRYAVP_THIRD_PARTY_DIR}/xiph-ogg_patch)
set(LIBOPUS_DIR ${CHERRYAVP_THIRD_PARTY_DIR}/xiph-opus)
set(LIBOPUS_PATCH_DIR ${CHERRYAVP_THIRD_PARTY_DIR}/xiph-opus_patch)
set(LIBVORBIS_DIR ${CHERRYAVP_THIRD_PARTY_DIR}/xiph-vorbis)
set(LIBFLAC_DIR ${CHERRYAVP_THIRD_PARTY_DIR}/xiph-flac)
set(LIBFLAC_SRC_DIR ${LIBFLAC_DIR}/src/libFLAC)
set(LIBSPEEXDSP_DIR ${CHERRYAVP_THIRD_PARTY_DIR}/xiph-speexdsp)
set(MINIMP3_DIR ${CHERRYAVP_THIRD_PARTY_DIR}/minimp3)
set(ALAC_DIR ${CHERRYAVP_THIRD_PARTY_DIR}/apple-alac/codec)

if(CONFIG_CHERRYAVP_SWR)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVSWRESAMPLE_SRC_DIR}/avp_swr.c
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_SWR)
endif()

# ---------------------------------------------------------------------------
# Audio effect volume filter
# ---------------------------------------------------------------------------

if(CONFIG_CHERRYAVP_AE_VOL)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVFILTER_SRC_DIR}/avp_ae_vol.c
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_AE_VOL)
endif()

# ---------------------------------------------------------------------------
# Adaptive howling suppression filter
# ---------------------------------------------------------------------------

if(CONFIG_CHERRYAVP_AFE_HOWLING)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVFILTER_SRC_DIR}/avp_afe_howling.c
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_AFE_HOWLING)
endif()

# ---------------------------------------------------------------------------
# Parametric equalizer
# ---------------------------------------------------------------------------

if(CONFIG_CHERRYAVP_AE_EQ)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVFILTER_SRC_DIR}/avp_ae_eq.c
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_AE_EQ)
endif()

# ---------------------------------------------------------------------------
# Generic biquad filter
# ---------------------------------------------------------------------------

if(CONFIG_CHERRYAVP_AE_FILTER)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVFILTER_SRC_DIR}/avp_ae_filter.c
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_AE_FILTER)
endif()

# ---------------------------------------------------------------------------
# PCM mixer
# ---------------------------------------------------------------------------

if(CONFIG_CHERRYAVP_AE_MIXER)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVFILTER_SRC_DIR}/avp_ae_mixer.c
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_AE_MIXER)
endif()

# ---------------------------------------------------------------------------
# Reverb, compressor, and limiter
# ---------------------------------------------------------------------------

if(CONFIG_CHERRYAVP_AE_REVERB)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVFILTER_SRC_DIR}/avp_ae_reverb.c
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_AE_REVERB)
endif()

if(CONFIG_CHERRYAVP_AE_COMPRESSOR)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVFILTER_SRC_DIR}/avp_ae_compressor.c
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_AE_COMPRESSOR)
endif()

if(CONFIG_CHERRYAVP_AE_LIMITER)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVFILTER_SRC_DIR}/avp_ae_limiter.c
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_AE_LIMITER)
endif()

# ---------------------------------------------------------------------------
# Sonic time/pitch audio filter
# ---------------------------------------------------------------------------

if(CONFIG_CHERRYAVP_AE_SONIC)
set(CHERRYAVP_AE_SONIC_DIR ${CHERRYAVP_THIRD_PARTY_DIR}/sonic)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVFILTER_SRC_DIR}/avp_ae_sonic.c
    ${CHERRYAVP_AE_SONIC_DIR}/sonic.c
)
list(APPEND CHERRYAVP_TARGET_PRIVATE_INCLUDE
    ${CHERRYAVP_AE_SONIC_DIR}
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_AE_SONIC)
endif()

# ---------------------------------------------------------------------------
# WebRTC audio front-end (AEC / NS / AGC / VAD)
# ---------------------------------------------------------------------------

if(CONFIG_CHERRYAVP_AFE_3A)
set(WEBRTC_DIR ${CHERRYAVP_THIRD_PARTY_DIR}/webrtc)

set(WEBRTC_AFE_SRC
    ${WEBRTC_DIR}/common_audio/ring_buffer.c
    ${WEBRTC_DIR}/common_audio/fft4g.c
    # ${WEBRTC_DIR}/common_audio/signal_processing/auto_corr_to_refl_coef.c
    # ${WEBRTC_DIR}/common_audio/signal_processing/auto_correlation.c
    ${WEBRTC_DIR}/common_audio/signal_processing/complex_bit_reverse.c
    ${WEBRTC_DIR}/common_audio/signal_processing/complex_fft.c
    ${WEBRTC_DIR}/common_audio/signal_processing/copy_set_operations.c
    # ${WEBRTC_DIR}/common_audio/signal_processing/cross_correlation.c
    ${WEBRTC_DIR}/common_audio/signal_processing/division_operations.c
    ${WEBRTC_DIR}/common_audio/signal_processing/dot_product_with_scale.c
    # ${WEBRTC_DIR}/common_audio/signal_processing/downsample_fast.c
    ${WEBRTC_DIR}/common_audio/signal_processing/energy.c
    # ${WEBRTC_DIR}/common_audio/signal_processing/filter_ar.c
    # ${WEBRTC_DIR}/common_audio/signal_processing/filter_ar_fast_q12.c
    # ${WEBRTC_DIR}/common_audio/signal_processing/filter_ma_fast_q12.c
    # ${WEBRTC_DIR}/common_audio/signal_processing/get_hanning_window.c
    ${WEBRTC_DIR}/common_audio/signal_processing/get_scaling_square.c
    # ${WEBRTC_DIR}/common_audio/signal_processing/ilbc_specific_functions.c
    # ${WEBRTC_DIR}/common_audio/signal_processing/levinson_durbin.c
    # ${WEBRTC_DIR}/common_audio/signal_processing/lpc_to_refl_coef.c
    ${WEBRTC_DIR}/common_audio/signal_processing/min_max_operations.c
    ${WEBRTC_DIR}/common_audio/signal_processing/randomization_functions.c
    ${WEBRTC_DIR}/common_audio/signal_processing/real_fft.c
    # ${WEBRTC_DIR}/common_audio/signal_processing/refl_coef_to_lpc.c
    ${WEBRTC_DIR}/common_audio/signal_processing/resample.c
    ${WEBRTC_DIR}/common_audio/signal_processing/resample_48khz.c
    ${WEBRTC_DIR}/common_audio/signal_processing/resample_by_2.c
    ${WEBRTC_DIR}/common_audio/signal_processing/resample_by_2_internal.c
    ${WEBRTC_DIR}/common_audio/signal_processing/resample_fractional.c
    ${WEBRTC_DIR}/common_audio/signal_processing/spl_init.c
    ${WEBRTC_DIR}/common_audio/signal_processing/spl_sqrt.c
    ${WEBRTC_DIR}/common_audio/signal_processing/spl_sqrt_floor.c
    ${WEBRTC_DIR}/common_audio/signal_processing/splitting_filter.c
    # ${WEBRTC_DIR}/common_audio/signal_processing/sqrt_of_one_minus_x_squared.c
    # ${WEBRTC_DIR}/common_audio/signal_processing/vector_scaling_operations.c
    ${WEBRTC_DIR}/common_audio/vad/webrtc_vad.c
    ${WEBRTC_DIR}/common_audio/vad/vad_core.c
    ${WEBRTC_DIR}/common_audio/vad/vad_filterbank.c
    ${WEBRTC_DIR}/common_audio/vad/vad_gmm.c
    ${WEBRTC_DIR}/common_audio/vad/vad_sp.c
    ${WEBRTC_DIR}/modules/audio_processing/aec/aec_core.c
    ${WEBRTC_DIR}/modules/audio_processing/aec/aec_rdft.c
    ${WEBRTC_DIR}/modules/audio_processing/aec/aec_resampler.c
    ${WEBRTC_DIR}/modules/audio_processing/aec/echo_cancellation.c
    ${WEBRTC_DIR}/modules/audio_processing/utility/delay_estimator.c
    ${WEBRTC_DIR}/modules/audio_processing/utility/delay_estimator_wrapper.c
    ${WEBRTC_DIR}/modules/audio_processing/agc/legacy/analog_agc.c
    ${WEBRTC_DIR}/modules/audio_processing/agc/legacy/digital_agc.c
)

if(CONFIG_CHERRYAVP_WEBRTC_OVERRIDE)
    list(REMOVE_ITEM WEBRTC_AFE_SRC
        ${WEBRTC_DIR}/common_audio/signal_processing/complex_bit_reverse.c
        ${WEBRTC_DIR}/common_audio/signal_processing/complex_fft.c
        ${WEBRTC_DIR}/common_audio/signal_processing/min_max_operations.c
        ${WEBRTC_DIR}/common_audio/signal_processing/spl_sqrt.c
        ${WEBRTC_DIR}/common_audio/signal_processing/spl_sqrt_floor.c
    )
endif()

if(CONFIG_CHERRYAVP_AFE_3A_NS_FIXED)
    list(APPEND WEBRTC_AFE_SRC
        ${WEBRTC_DIR}/modules/audio_processing/ns/noise_suppression_x.c
        ${WEBRTC_DIR}/modules/audio_processing/ns/nsx_core.c
        ${WEBRTC_DIR}/modules/audio_processing/ns/nsx_core_c.c
    )
else()
    list(APPEND WEBRTC_AFE_SRC
        ${WEBRTC_DIR}/modules/audio_processing/ns/noise_suppression.c
        ${WEBRTC_DIR}/modules/audio_processing/ns/ns_core.c
    )
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64|AMD64|i.86)")
    list(APPEND WEBRTC_AFE_SRC
        ${WEBRTC_DIR}/modules/audio_processing/aec/aec_core_sse2.c
        ${WEBRTC_DIR}/modules/audio_processing/aec/aec_rdft_sse2.c
        ${WEBRTC_DIR}/system_wrappers/source/cpu_features.cc
    )
    set_source_files_properties(
        ${WEBRTC_DIR}/modules/audio_processing/aec/aec_core_sse2.c
        ${WEBRTC_DIR}/modules/audio_processing/aec/aec_rdft_sse2.c
        PROPERTIES COMPILE_OPTIONS "-msse2"
    )
endif()

list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVFILTER_SRC_DIR}/avp_afe_3a.c
    ${WEBRTC_AFE_SRC}
)
list(APPEND CHERRYAVP_TARGET_PRIVATE_INCLUDE
    ${CHERRYAVP_THIRD_PARTY_DIR}
)
if(CONFIG_CHERRYAVP_WEBRTC_OVERRIDE)
    list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_WEBRTC_OVERRIDE)
endif()
if(CONFIG_CHERRYAVP_AFE_3A_NS_FIXED)
    list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_AFE_3A_NS_FIXED)
endif()
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_AFE_3A)
endif()

# ---------------------------------------------------------------------------
# OpenCORE AAC
# ---------------------------------------------------------------------------

if(CONFIG_CHERRYAVP_AAC)
set(OPENCORE_AAC_DEC_DIR ${OPENCORE_DIR}/codecs_v2/audio/aac/dec)
set(OPENCORE_AAC_DEC_SRC_DIR ${OPENCORE_AAC_DEC_DIR}/src)

set(OPENCORE_AAC_DEC_SRC
    ${OPENCORE_AAC_DEC_SRC_DIR}/analysis_sub_band.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/apply_ms_synt.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/apply_tns.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/buf_getbits.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/byte_align.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/calc_auto_corr.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/calc_gsfb_table.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/calc_sbr_anafilterbank.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/calc_sbr_envelope.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/calc_sbr_synfilterbank.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/check_crc.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/dct16.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/dct64.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/decode_huff_cw_binary.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/decode_noise_floorlevels.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/deinterleave.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/digit_reversal_tables.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/dst16.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/dst32.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/dst8.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/esc_iquant_scaling.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/extractframeinfo.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/fft_rx4_long.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/fft_rx4_short.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/fft_rx4_tables_fxp.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/find_adts_syncword.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/fwd_long_complex_rot.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/fwd_short_complex_rot.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/gen_rand_vector.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/get_adif_header.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/get_adts_header.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/get_audio_specific_config.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/get_dse.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/get_ele_list.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/get_ga_specific_config.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/get_ics_info.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/get_prog_config.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/get_pulse_data.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/get_sbr_bitstream.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/get_sbr_startfreq.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/get_sbr_stopfreq.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/get_tns.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/getfill.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/getgroup.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/getics.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/getmask.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/hcbtables_binary.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/huffcb.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/huffdecode.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/hufffac.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/huffspec_fxp.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/idct16.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/idct32.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/idct8.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/imdct_fxp.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/infoinit.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/init_sbr_dec.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/intensity_right.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/inv_long_complex_rot.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/inv_short_complex_rot.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/iquant_table.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/long_term_prediction.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/long_term_synthesis.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/lt_decode.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/mdct_fxp.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/mdct_tables_fxp.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/mdst.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/mix_radix_fft.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/ms_synt.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/pns_corr.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/pns_intensity_right.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/pns_left.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/ps_all_pass_filter_coeff.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/ps_all_pass_fract_delay_filter.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/ps_allocate_decoder.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/ps_applied.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/ps_bstr_decoding.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/ps_channel_filtering.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/ps_decode_bs_utils.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/ps_decorrelate.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/ps_fft_rx8.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/ps_hybrid_analysis.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/ps_hybrid_filter_bank_allocation.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/ps_hybrid_synthesis.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/ps_init_stereo_mixing.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/ps_pwr_transient_detection.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/ps_read_data.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/ps_stereo_processing.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/pulse_nc.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/pv_div.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/pv_log2.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/pv_normalize.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/pv_pow2.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/pv_sine.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/pv_sqrt.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/pvmp4audiodecoderconfig.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/pvmp4audiodecoderframe.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/pvmp4audiodecodergetmemrequirements.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/pvmp4audiodecoderinitlibrary.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/pvmp4audiodecoderresetbuffer.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/pvmp4setaudioconfig.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/q_normalize.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/qmf_filterbank_coeff.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_aliasing_reduction.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_applied.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_code_book_envlevel.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_crc_check.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_create_limiter_bands.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_dec.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_decode_envelope.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_decode_huff_cw.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_downsample_lo_res.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_envelope_calc_tbl.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_envelope_unmapping.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_extract_extended_data.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_find_start_andstop_band.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_generate_high_freq.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_get_additional_data.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_get_cpe.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_get_dir_control_data.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_get_envelope.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_get_header_data.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_get_noise_floor_data.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_get_sce.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_inv_filt_levelemphasis.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_open.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_read_data.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_requantize_envelope_data.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_reset_dec.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sbr_update_freq_scale.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/set_mc_info.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/sfb.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/shellsort.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/synthesis_sub_band.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/tns_ar_filter.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/tns_decode_coef.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/tns_inv_filter.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/trans4m_freq_2_time_fxp.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/trans4m_time_2_freq_fxp.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/unpack_idx.cpp
    ${OPENCORE_AAC_DEC_SRC_DIR}/window_tables_fxp.cpp
)
list(APPEND CHERRYAVP_TARGET_SRC
    ${OPENCORE_AAC_DEC_SRC}
    ${CHERRYAVP_AVCODEC_SRC_DIR}/aac_decode.c
)
list(APPEND CHERRYAVP_TARGET_PRIVATE_INCLUDE
    ${OPENCORE_PATCH_DIR}
    ${OPENCORE_AAC_DEC_DIR}/include
    ${OPENCORE_AAC_DEC_SRC_DIR}
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_AAC)

if(CONFIG_CHERRYAVP_AAC_PLUS)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_AAC_PLUS)

list(APPEND CHERRYAVP_TARGET_PRIVATE_DEFINITION
    AAC_PLUS
    HQ_SBR
    PARAMETRICSTEREO
)
endif()

endif()

if(CONFIG_CHERRYAVP_M4A OR CONFIG_CHERRYAVP_MP4)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVFORMAT_SRC_DIR}/m4a_demux.c
)
endif()

if(CONFIG_CHERRYAVP_M4A)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_M4A)
endif()

if(CONFIG_CHERRYAVP_MP4)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVFORMAT_SRC_DIR}/mp4_demux.c
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_MP4)
endif()

# ---------------------------------------------------------------------------
# OpenCORE AMR
# ---------------------------------------------------------------------------

if(CONFIG_CHERRYAVP_AMR)
set(OPENCORE_AMR_NB_DEC_DIR ${OPENCORE_DIR}/codecs_v2/audio/gsm_amr/amr_nb/dec)
set(OPENCORE_AMR_NB_DEC_SRC_DIR ${OPENCORE_AMR_NB_DEC_DIR}/src)
set(OPENCORE_AMR_NB_COMMON_DIR ${OPENCORE_DIR}/codecs_v2/audio/gsm_amr/amr_nb/common)
set(OPENCORE_AMR_NB_COMMON_SRC_DIR ${OPENCORE_AMR_NB_COMMON_DIR}/src)
set(OPENCORE_AMR_NB_ENC_SRC_DIR ${OPENCORE_DIR}/codecs_v2/audio/gsm_amr/amr_nb/enc/src)
set(OPENCORE_AMR_WB_DEC_DIR ${OPENCORE_DIR}/codecs_v2/audio/gsm_amr/amr_wb/dec)
set(OPENCORE_AMR_WB_DEC_SRC_DIR ${OPENCORE_AMR_WB_DEC_DIR}/src)
set(OPENCORE_AMR_COMMON_DEC_INC ${OPENCORE_DIR}/codecs_v2/audio/gsm_amr/common/dec/include)

set(OPENCORE_AMR_NB_DEC_SRC
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/a_refl.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/agc.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/amrdecode.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/b_cn_cod.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/bgnscd.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/c_g_aver.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/d1035pf.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/d2_11pf.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/d2_9pf.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/d3_14pf.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/d4_17pf.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/d8_31pf.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/d_gain_c.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/d_gain_p.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/d_plsf.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/d_plsf_3.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/d_plsf_5.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/dec_amr.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/dec_gain.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/dec_input_format_tab.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/dec_lag3.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/dec_lag6.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/dtx_dec.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/ec_gains.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/ex_ctrl.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/if2_to_ets.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/int_lsf.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/lsp_avg.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/ph_disp.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/post_pro.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/preemph.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/pstfilt.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/qgain475_tab.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/sp_dec.cpp
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}/wmf_to_ets.cpp
)

set(OPENCORE_AMR_NB_COMMON_SRC
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/add.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/az_lsp.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/bitno_tab.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/bitreorder_tab.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/bits2prm.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/bytesused.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/c2_9pf_tab.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/copy.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/div_s.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/extract_h.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/extract_l.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/gains_tbl.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/gc_pred.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/gmed_n.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/gray_tbl.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/grid_tbl.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/int_lpc.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/inv_sqrt.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/inv_sqrt_tbl.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/l_abs.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/l_deposit_h.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/l_deposit_l.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/l_shr_r.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/log2.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/log2_norm.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/log2_tbl.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/lsfwt.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/lsp.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/lsp_az.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/lsp_lsf.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/lsp_lsf_tbl.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/lsp_tab.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/mult_r.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/negate.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/norm_l.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/norm_s.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/overflow_tbl.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/ph_disp_tab.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/pow2.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/pow2_tbl.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/pred_lt.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/q_plsf.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/q_plsf_3.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/q_plsf_3_tbl.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/q_plsf_5.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/q_plsf_5_tbl.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/qua_gain_tbl.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/reorder.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/residu.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/round.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/set_zero.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/shr.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/shr_r.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/sqrt_l.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/sqrt_l_tbl.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/sub.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/syn_filt.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/weight_a.cpp
    ${OPENCORE_AMR_NB_COMMON_SRC_DIR}/window_tab.cpp
)

set(OPENCORE_AMR_WB_DEC_SRC
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/agc2_amr_wb.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/band_pass_6k_7k.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/dec_acelp_2p_in_64.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/dec_acelp_4p_in_64.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/dec_alg_codebook.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/dec_gain2_amr_wb.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/deemphasis_32.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/dtx_decoder_amr_wb.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/get_amr_wb_bits.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/highpass_400hz_at_12k8.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/highpass_50hz_at_12k8.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/homing_amr_wb_dec.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/interpolate_isp.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/isf_extrapolation.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/isp_az.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/isp_isf.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/lagconceal.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/low_pass_filt_7k.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/median5.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/mime_io.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/noise_gen_amrwb.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/normalize_amr_wb.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/oversamp_12k8_to_16k.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/phase_dispersion.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/pit_shrp.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/pred_lt4.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/preemph_amrwb_dec.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/pvamrwb_math_op.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/pvamrwbdecoder.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/q_gain2_tab.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/qisf_ns.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/qisf_ns_tab.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/qpisf_2s.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/qpisf_2s_tab.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/scale_signal.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/synthesis_amr_wb.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/voice_factor.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/wb_syn_filt.cpp
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}/weight_amrwb_lpc.cpp
)

list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_THIRD_PARTY_DIR}/opencore_patch/amrnb/wrapper.cpp
    ${OPENCORE_AMR_NB_DEC_SRC}
    ${OPENCORE_AMR_NB_COMMON_SRC}
    ${CHERRYAVP_THIRD_PARTY_DIR}/opencore_patch/amrwb/wrapper.cpp
    ${OPENCORE_AMR_WB_DEC_SRC}
    ${CHERRYAVP_AVCODEC_SRC_DIR}/amr_decode.c
)
list(APPEND CHERRYAVP_TARGET_PRIVATE_INCLUDE
    ${OPENCORE_PATCH_DIR}
    ${OPENCORE_AMR_NB_DEC_SRC_DIR}
    ${OPENCORE_AMR_NB_DEC_DIR}/include
    ${OPENCORE_AMR_NB_COMMON_DIR}/include
    ${OPENCORE_AMR_NB_ENC_SRC_DIR}
    ${OPENCORE_AMR_WB_DEC_SRC_DIR}
    ${OPENCORE_AMR_WB_DEC_DIR}/include
    ${OPENCORE_AMR_COMMON_DEC_INC}
    ${OPENCORE_DIR}
    ${CHERRYAVP_THIRD_PARTY_DIR}/opencore_patch
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_AMR)
list(APPEND CHERRYAVP_TARGET_PRIVATE_DEFINITION
DISABLE_AMRNB_ENCODER
WMOPS=0
)
endif()

# ---------------------------------------------------------------------------
# Ogg / Opus / Vorbis third-party sources
# ---------------------------------------------------------------------------

if(CONFIG_CHERRYAVP_OGG OR CONFIG_CHERRYAVP_VORBIS)
list(APPEND CHERRYAVP_TARGET_SRC
    ${LIBOGG_DIR}/src/bitwise.c
    ${LIBOGG_DIR}/src/framing.c
)
list(APPEND CHERRYAVP_TARGET_PRIVATE_INCLUDE
    ${LIBOGG_PATCH_DIR}
    ${LIBOGG_DIR}/include
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_INCLUDE
    ${LIBOGG_PATCH_DIR}
    ${LIBOGG_DIR}/include
)
endif()

if(CONFIG_CHERRYAVP_OPUS)
list(APPEND CHERRYAVP_TARGET_SRC
    ${LIBOPUS_DIR}/src/opus.c
    ${LIBOPUS_DIR}/src/opus_decoder.c
    ${LIBOPUS_DIR}/src/opus_multistream.c
    ${LIBOPUS_DIR}/src/opus_multistream_decoder.c
    ${LIBOPUS_DIR}/src/extensions.c
    ${LIBOPUS_DIR}/celt/bands.c
    ${LIBOPUS_DIR}/celt/celt.c
    ${LIBOPUS_DIR}/celt/celt_decoder.c
    ${LIBOPUS_DIR}/celt/cwrs.c
    ${LIBOPUS_DIR}/celt/entcode.c
    ${LIBOPUS_DIR}/celt/entdec.c
    ${LIBOPUS_DIR}/celt/entenc.c
    ${LIBOPUS_DIR}/celt/kiss_fft.c
    ${LIBOPUS_DIR}/celt/laplace.c
    ${LIBOPUS_DIR}/celt/mathops.c
    ${LIBOPUS_DIR}/celt/mdct.c
    ${LIBOPUS_DIR}/celt/modes.c
    ${LIBOPUS_DIR}/celt/pitch.c
    ${LIBOPUS_DIR}/celt/celt_lpc.c
    ${LIBOPUS_DIR}/celt/quant_bands.c
    ${LIBOPUS_DIR}/celt/rate.c
    ${LIBOPUS_DIR}/celt/vq.c
    ${LIBOPUS_DIR}/silk/CNG.c
    ${LIBOPUS_DIR}/silk/code_signs.c
    ${LIBOPUS_DIR}/silk/init_decoder.c
    ${LIBOPUS_DIR}/silk/decode_core.c
    ${LIBOPUS_DIR}/silk/decode_frame.c
    ${LIBOPUS_DIR}/silk/decode_parameters.c
    ${LIBOPUS_DIR}/silk/decode_indices.c
    ${LIBOPUS_DIR}/silk/decode_pulses.c
    ${LIBOPUS_DIR}/silk/decoder_set_fs.c
    ${LIBOPUS_DIR}/silk/dec_API.c
    ${LIBOPUS_DIR}/silk/gain_quant.c
    ${LIBOPUS_DIR}/silk/interpolate.c
    ${LIBOPUS_DIR}/silk/LP_variable_cutoff.c
    ${LIBOPUS_DIR}/silk/NLSF_decode.c
    ${LIBOPUS_DIR}/silk/PLC.c
    ${LIBOPUS_DIR}/silk/shell_coder.c
    ${LIBOPUS_DIR}/silk/tables_gain.c
    ${LIBOPUS_DIR}/silk/tables_LTP.c
    ${LIBOPUS_DIR}/silk/tables_NLSF_CB_NB_MB.c
    ${LIBOPUS_DIR}/silk/tables_NLSF_CB_WB.c
    ${LIBOPUS_DIR}/silk/tables_other.c
    ${LIBOPUS_DIR}/silk/tables_pitch_lag.c
    ${LIBOPUS_DIR}/silk/tables_pulses_per_block.c
    ${LIBOPUS_DIR}/silk/HP_variable_cutoff.c
    ${LIBOPUS_DIR}/silk/NLSF_unpack.c
    ${LIBOPUS_DIR}/silk/NLSF_del_dec_quant.c
    ${LIBOPUS_DIR}/silk/process_NLSFs.c
    ${LIBOPUS_DIR}/silk/stereo_MS_to_LR.c
    ${LIBOPUS_DIR}/silk/check_control_input.c
    ${LIBOPUS_DIR}/silk/control_codec.c
    ${LIBOPUS_DIR}/silk/biquad_alt.c
    ${LIBOPUS_DIR}/silk/bwexpander.c
    ${LIBOPUS_DIR}/silk/bwexpander_32.c
    ${LIBOPUS_DIR}/silk/debug.c
    ${LIBOPUS_DIR}/silk/decode_pitch.c
    ${LIBOPUS_DIR}/silk/inner_prod_aligned.c
    ${LIBOPUS_DIR}/silk/lin2log.c
    ${LIBOPUS_DIR}/silk/log2lin.c
    ${LIBOPUS_DIR}/silk/LPC_analysis_filter.c
    ${LIBOPUS_DIR}/silk/LPC_fit.c
    ${LIBOPUS_DIR}/silk/LPC_inv_pred_gain.c
    ${LIBOPUS_DIR}/silk/table_LSF_cos.c
    ${LIBOPUS_DIR}/silk/NLSF2A.c
    ${LIBOPUS_DIR}/silk/NLSF_stabilize.c
    ${LIBOPUS_DIR}/silk/NLSF_VQ_weights_laroia.c
    ${LIBOPUS_DIR}/silk/pitch_est_tables.c
    ${LIBOPUS_DIR}/silk/resampler.c
    ${LIBOPUS_DIR}/silk/resampler_down2.c
    ${LIBOPUS_DIR}/silk/resampler_down2_3.c
    ${LIBOPUS_DIR}/silk/resampler_private_AR2.c
    ${LIBOPUS_DIR}/silk/resampler_private_down_FIR.c
    ${LIBOPUS_DIR}/silk/resampler_private_IIR_FIR.c
    ${LIBOPUS_DIR}/silk/resampler_private_up2_HQ.c
    ${LIBOPUS_DIR}/silk/resampler_rom.c
    ${LIBOPUS_DIR}/silk/sigm_Q15.c
    ${LIBOPUS_DIR}/silk/sort.c
    ${LIBOPUS_DIR}/silk/sum_sqr_shift.c
    ${LIBOPUS_DIR}/silk/stereo_decode_pred.c
    ${CHERRYAVP_AVCODEC_SRC_DIR}/opus_decode.c
)
list(APPEND CHERRYAVP_TARGET_PRIVATE_INCLUDE
    ${LIBOPUS_DIR}/include
    ${LIBOPUS_DIR}/silk
    ${LIBOPUS_DIR}/celt
    ${LIBOPUS_DIR}/silk/fixed
    ${LIBOPUS_PATCH_DIR}
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_OPUS)
list(APPEND CHERRYAVP_TARGET_PRIVATE_DEFINITION
    CUSTOM_SUPPORT
    OPUS_BUILD
    PACKAGE_VERSION="1.5.2"
    EMBEDDED_ARM=1
    __OPTIMIZE__
    HAVE_LRINT
    # HAVE_LRINTF
    VAR_ARRAYS
    FIXED_POINT  # Use fixed-point arithmetic for embedded
    DISABLE_FLOAT_API # Disable floating point API
    CUSTOM_MODES # Enable custom CELT modes
)
endif()

if(CONFIG_CHERRYAVP_VORBIS)
list(APPEND CHERRYAVP_TARGET_SRC
    ${LIBVORBIS_DIR}/lib/mdct.c
    ${LIBVORBIS_DIR}/lib/smallft.c
    ${LIBVORBIS_DIR}/lib/block.c
    ${LIBVORBIS_DIR}/lib/envelope.c
    ${LIBVORBIS_DIR}/lib/window.c
    ${LIBVORBIS_DIR}/lib/lsp.c
    ${LIBVORBIS_DIR}/lib/lpc.c
    ${LIBVORBIS_DIR}/lib/analysis.c
    ${LIBVORBIS_DIR}/lib/synthesis.c
    ${LIBVORBIS_DIR}/lib/psy.c
    ${LIBVORBIS_DIR}/lib/info.c
    ${LIBVORBIS_DIR}/lib/floor1.c
    ${LIBVORBIS_DIR}/lib/floor0.c
    ${LIBVORBIS_DIR}/lib/res0.c
    ${LIBVORBIS_DIR}/lib/mapping0.c
    ${LIBVORBIS_DIR}/lib/registry.c
    ${LIBVORBIS_DIR}/lib/codebook.c
    ${LIBVORBIS_DIR}/lib/sharedbook.c
    ${LIBVORBIS_DIR}/lib/lookup.c
    ${LIBVORBIS_DIR}/lib/bitrate.c
    ${CHERRYAVP_AVCODEC_SRC_DIR}/vorbis_decode.c
)
list(APPEND CHERRYAVP_TARGET_PRIVATE_INCLUDE
    ${LIBOGG_PATCH_DIR}
    ${LIBOGG_DIR}/include
    ${LIBVORBIS_DIR}/include
    ${LIBVORBIS_DIR}/lib
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_INCLUDE
    ${LIBOGG_PATCH_DIR}
    ${LIBOGG_DIR}/include
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_VORBIS)
endif()

# ---------------------------------------------------------------------------
# CherryAVC codec sources
# ---------------------------------------------------------------------------

if(CONFIG_CHERRYAVP_ADPCM)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVCODEC_SRC_DIR}/adpcm_decode.c
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_ADPCM)
endif()

if(CONFIG_CHERRYAVP_G711)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVCODEC_SRC_DIR}/g711_decode.c
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_G711)
endif()

if(CONFIG_CHERRYAVP_G722)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVCODEC_SRC_DIR}/g722_decode.c
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_G722)
endif()

if(CONFIG_CHERRYAVP_WAV)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVFORMAT_SRC_DIR}/wav_demux.c
    ${CHERRYAVP_AVFORMAT_SRC_DIR}/wav_mux.c
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_WAV)
endif()

if(CONFIG_CHERRYAVP_AVI)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVFORMAT_SRC_DIR}/avi_demux.c
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_AVI)
endif()

if(CONFIG_CHERRYAVP_FLAC)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVCODEC_SRC_DIR}/flac_decode.c
    ${LIBFLAC_SRC_DIR}/bitmath.c
    ${LIBFLAC_SRC_DIR}/bitreader.c
    ${LIBFLAC_SRC_DIR}/cpu.c
    ${LIBFLAC_SRC_DIR}/crc.c
    ${LIBFLAC_SRC_DIR}/fixed.c
    ${LIBFLAC_SRC_DIR}/float.c
    ${LIBFLAC_SRC_DIR}/format.c
    ${LIBFLAC_SRC_DIR}/lpc.c
    ${LIBFLAC_SRC_DIR}/md5.c
    ${LIBFLAC_SRC_DIR}/memory.c
    ${LIBFLAC_SRC_DIR}/stream_decoder.c
)
list(APPEND CHERRYAVP_TARGET_PRIVATE_INCLUDE
    ${LIBFLAC_DIR}/include
    ${LIBFLAC_DIR}/include/share
    ${LIBFLAC_SRC_DIR}/include
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_INCLUDE
    ${LIBFLAC_DIR}/include
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_FLAC)
list(APPEND CHERRYAVP_TARGET_PRIVATE_DEFINITION
    FLAC__NO_DLL
    FLAC__NO_ASM
    FLAC__HAS_OGG=0
    FLAC__HAS_FILE=0
    FLAC__INTEGER_ONLY_LIBRARY
)
endif()

if(CONFIG_CHERRYAVP_ALAC)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVCODEC_SRC_DIR}/alac_decode.cpp
    ${ALAC_DIR}/ALACDecoder.cpp
    ${ALAC_DIR}/ALACBitUtilities.c
    ${ALAC_DIR}/EndianPortable.c
    ${ALAC_DIR}/ag_dec.c
    ${ALAC_DIR}/dp_dec.c
    ${ALAC_DIR}/matrix_dec.c
    ${CHERRYAVP_AVFORMAT_SRC_DIR}/alac_demux.c
)
list(APPEND CHERRYAVP_TARGET_PRIVATE_INCLUDE
    ${ALAC_DIR}
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_ALAC)
list(APPEND CHERRYAVP_TARGET_PRIVATE_DEFINITION
FLAC__HAS_X86INTRIN=0
CPU_IS_BIG_ENDIAN=0
ENABLE_64_BIT_WORDS=0
WORDS_BIGENDIAN=0
PRAGMA_ONCE=0
PRAGMA_STRUCT_ALIGN=0
PRAGMA_STRUCT_PACKPUSH=0
PRAGMA_STRUCT_PACK=0
PRAGMA_MARK=0
TARGET_RT_LITTLE_ENDIAN=1
TARGET_CPU_PPC=0
TARGET_RT_BIG_ENDIAN=0
TARGET_OS_MAC=0
NDEBUG
)
endif()

if(CONFIG_CHERRYAVP_MP3)
if(CONFIG_CHERRYAVP_MP3_DECODER_OPENCORE)
set(OPENCORE_MP3_DEC_DIR ${OPENCORE_DIR}/codecs_v2/audio/mp3/dec)
set(OPENCORE_MP3_DEC_SRC_DIR ${OPENCORE_MP3_DEC_DIR}/src)

set(OPENCORE_MP3_DEC_SRC
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_normalize.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_alias_reduction.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_crc.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_decode_header.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_decode_huff_cw.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_getbits.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_dequantize_sample.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_framedecoder.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_get_main_data_size.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_get_side_info.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_get_scale_factors.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_mpeg2_get_scale_data.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_mpeg2_get_scale_factors.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_mpeg2_stereo_proc.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_huffman_decoding.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_huffman_parsing.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_tables.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_imdct_synth.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_mdct_6.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_dct_6.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_poly_phase_synthesis.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_equalizer.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_seek_synch.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_stereo_proc.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_reorder.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_polyphase_filter_window.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_mdct_18.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_dct_9.cpp
    ${OPENCORE_MP3_DEC_SRC_DIR}/pvmp3_dct_16.cpp
)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVCODEC_SRC_DIR}/mp3_decode_opencore.c
    ${OPENCORE_MP3_DEC_SRC}
)
list(APPEND CHERRYAVP_TARGET_PRIVATE_INCLUDE
    ${MINIMP3_DIR}
    ${OPENCORE_PATCH_DIR}
    ${OPENCORE_MP3_DEC_DIR}/include
    ${OPENCORE_MP3_DEC_SRC_DIR}
)
else()
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVCODEC_SRC_DIR}/mp3_decode.c
)
list(APPEND CHERRYAVP_TARGET_PRIVATE_INCLUDE
    ${MINIMP3_DIR}
)
endif()
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_MP3)
endif()

if(CONFIG_CHERRYAVP_OGG)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVFORMAT_SRC_DIR}/ogg_demux.c
)
list(APPEND CHERRYAVP_TARGET_PRIVATE_INCLUDE
    ${LIBOGG_PATCH_DIR}
    ${LIBOGG_DIR}/include
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_INCLUDE
    ${LIBOGG_PATCH_DIR}
    ${LIBOGG_DIR}/include
)
list(APPEND CHERRYAVP_TARGET_PUBLIC_DEFINITION CONFIG_CHERRYAVP_OGG)
endif()

list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVFORMAT_SRC_DIR}/audio_stream_demux.c
)

if(CONFIG_CHERRYAVP_MP3)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVFORMAT_SRC_DIR}/mp3_demux.c
)
endif()

if(CONFIG_CHERRYAVP_AAC)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVFORMAT_SRC_DIR}/aac_demux.c
)
endif()

if(CONFIG_CHERRYAVP_AMR)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVFORMAT_SRC_DIR}/amr_demux.c
)
endif()

if(CONFIG_CHERRYAVP_FLAC)
list(APPEND CHERRYAVP_TARGET_SRC
    ${CHERRYAVP_AVFORMAT_SRC_DIR}/flac_demux.c
)
endif()

list(REMOVE_DUPLICATES CHERRYAVP_TARGET_SRC)
list(REMOVE_DUPLICATES CHERRYAVP_TARGET_PUBLIC_INCLUDE)
list(REMOVE_DUPLICATES CHERRYAVP_TARGET_PRIVATE_INCLUDE)
list(REMOVE_DUPLICATES CHERRYAVP_TARGET_PUBLIC_DEFINITION)
list(REMOVE_DUPLICATES CHERRYAVP_TARGET_PRIVATE_DEFINITION)

list(APPEND CHERRYAVP_TARGET_PRIVATE_OPTIONS
-Wno-narrowing -Wno-implicit-fallthrough -Wno-multichar
)

add_library(CherryAVP STATIC ${CHERRYAVP_TARGET_SRC})
target_include_directories(CherryAVP
    PUBLIC ${CHERRYAVP_TARGET_PUBLIC_INCLUDE}
    PRIVATE ${CHERRYAVP_TARGET_PRIVATE_INCLUDE}
)
target_compile_definitions(CherryAVP
    PUBLIC ${CHERRYAVP_TARGET_PUBLIC_DEFINITION}
    PRIVATE ${CHERRYAVP_TARGET_PRIVATE_DEFINITION}
)
target_compile_options(CherryAVP
    PRIVATE ${CHERRYAVP_TARGET_PRIVATE_OPTIONS}
)

if(CHERRYAVP_MATH_LIBRARY)
    target_link_libraries(CherryAVP PUBLIC ${CHERRYAVP_MATH_LIBRARY})
endif()
