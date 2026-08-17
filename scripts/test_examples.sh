#!/usr/bin/env bash

set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
OUTPUT_ROOT="${OUTPUT_ROOT:-output}"
RUN_ID="${RUN_ID:-$(date +%Y%m%d-%H%M%S)}"
OUT_DIR="${OUTPUT_ROOT}/${RUN_ID}"
LOG_DIR="${OUT_DIR}/logs"
TEST_FILES_DIR="${OUT_DIR}/test_files"
EXAMPLE_FILES_DIR="${ROOT_DIR}/examples/files"
FRAME_COUNT="${FRAME_COUNT:-0}"

pass_count=0
fail_count=0
skip_count=0

mkdir -p "${LOG_DIR}"

note()
{
    printf '%s\n' "$*"
}

run_case()
{
    local name="$1"
    local artifact="$2"
    local exe="$3"
    local log="${LOG_DIR}/${name}.log"
    local status
    local summary

    shift 3

    if [ ! -x "${exe}" ]; then
        printf '[SKIP] %-24s missing executable: %s\n' "${name}" "${exe}"
        skip_count=$((skip_count + 1))
        return
    fi

    printf '[RUN ] %-24s ' "${name}"
    "${exe}" "$@" > "${log}" 2>&1
    status=$?
    summary="$(tail -n 1 "${log}" 2>/dev/null || true)"

    if [ ${status} -eq 0 ]; then
        printf 'ok   %s  %s\n' "${summary}"
        pass_count=$((pass_count + 1))
    else
        printf 'fail status=%d  log=%s\n' "${status}" "${log}"
        tail -n 8 "${log}" || true
        fail_count=$((fail_count + 1))
    fi
}

run_stream_case()
{
    local name="$1"
    local codec="$2"
    local input="$3"
    local output="${OUT_DIR}/${name}.pcm"

    run_case "${name}" "${output}" \
        "${BUILD_DIR}/examples/audio_codec_stream_demo" \
        "${codec}" "${input}" "${output}" "${FRAME_COUNT}"
}

run_resample_case()
{
    local name="$1"
    local rate="$2"
    local channels="$3"
    local bits="$4"
    local layout="$5"
    local output="${OUT_DIR}/resample_dump/${name}.pcm"
    local artifact="${output}"

    if [ "${layout}" = "p" ] || [ "${layout}" = "planar" ]; then
        artifact="${output}.ch0.pcm"
    fi

    run_case "${name}" "${artifact}" \
        "${BUILD_DIR}/examples/resample_demo" \
        "${EXAMPLE_FILES_DIR}/jinitaimei.wav" "${output}" \
        "${rate}" "${channels}" "${bits}" "${layout}" "${FRAME_COUNT}"
}

generate_test_files()
{
    local input_wav="$1"
    local outdir="$2"
    local out

    mkdir -p "${outdir}"

    if ! command -v ffmpeg >/dev/null 2>&1; then
        note "ffmpeg is required to generate test files"
        exit 1
    fi

    note "Generating ffmpeg test files into ${outdir}..."

    note "Generating aac ..."
    out="${outdir}/jinitaimei.aac"
    ffmpeg -hide_banner -loglevel error -y \
        -i "${input_wav}" -c:a aac -b:a 128k -f adts "${out}" ||
        note "failed: ${out}"

    note "Generating amr ..."
    out="${outdir}/jinitaimei.amr"
    if ! ffmpeg -hide_banner -loglevel error -y \
        -i "${input_wav}" -ar 8000 -ac 1 \
        -c:a libopencore_amrnb -b:a 12.2k -f amr "${out}"; then
        note "ffmpeg has no AMR-NB encoder, copying existing AMR sample"
        cp -f "${EXAMPLE_FILES_DIR}/jinitaimei.amr" "${out}" ||
            note "failed: ${out}"
    fi

    note "Generating flac ..."
    out="${outdir}/jinitaimei.flac"
    ffmpeg -hide_banner -loglevel error -y \
        -i "${input_wav}" -c:a flac "${out}" ||
        note "failed: ${out}"

    note "Generating m4a ..."
    out="${outdir}/jinitaimei.m4a"
    ffmpeg -hide_banner -loglevel error -y \
        -i "${input_wav}" -c:a aac -b:a 128k "${out}" ||
        note "failed: ${out}"

    note "Generating mp3 ..."
    out="${outdir}/jinitaimei.mp3"
    ffmpeg -hide_banner -loglevel error -y \
        -i "${input_wav}" -c:a libmp3lame -q:a 4 "${out}" ||
        note "failed: ${out}"

    note "Generating ogg-opus ..."
    out="${outdir}/jinitaimei_opus.ogg"
    ffmpeg -hide_banner -loglevel error -y \
        -i "${input_wav}" -ar 48000 -c:a libopus -b:a 128k \
        -f ogg "${out}" ||
        note "failed: ${out}"

    note "Generating ogg-vorbis ..."
    out="${outdir}/jinitaimei_vorbis.ogg"
    ffmpeg -hide_banner -loglevel error -y \
        -i "${input_wav}" -c:a libvorbis -q:a 5 -f ogg "${out}" ||
        note "failed: ${out}"

    note "Generating g711a ..."
    out="${outdir}/jinitaimei_g711a.wav"
    ffmpeg -hide_banner -loglevel error -y \
        -i "${input_wav}" -c:a pcm_alaw "${out}" ||
        note "failed: ${out}"

    note "Generating g711u ..."
    out="${outdir}/jinitaimei_g711u.wav"
    ffmpeg -hide_banner -loglevel error -y \
        -i "${input_wav}" -c:a pcm_mulaw "${out}" ||
        note "failed: ${out}"

    note "Generating g722 ..."
    out="${outdir}/jinitaimei_g722.wav"
    ffmpeg -hide_banner -loglevel error -y \
        -i "${input_wav}" -ar 16000 -ac 1 -c:a adpcm_g722 "${out}" ||
        note "failed: ${out}"

    note "Generating adpcm_ima ..."
    out="${outdir}/jinitaimei_adpcm_ima.wav"
    ffmpeg -hide_banner -loglevel error -y \
        -i "${input_wav}" -c:a adpcm_ima_wav "${out}" ||
        note "failed: ${out}"

    note "Generating caf-alac ..."
    out="${outdir}/jinitaimei_alac.caf"
    ffmpeg -hide_banner -loglevel error -y \
        -i "${input_wav}" -c:a alac -f caf "${out}" ||
        note "failed: ${out}"

    note "Generating m4a-alac ..."
    out="${outdir}/jinitaimei_alac.m4a"
    ffmpeg -hide_banner -loglevel error -y \
        -i "${input_wav}" -c:a alac "${out}" ||
        note "failed: ${out}"

    note "Generating 24-bit caf-alac ..."
    out="${outdir}/jinitaimei_alac_24bit.caf"
    ffmpeg -hide_banner -loglevel error -y \
        -i "${input_wav}" -sample_fmt s32p -bits_per_raw_sample 24 \
        -c:a alac -f caf "${out}" ||
        note "failed: ${out}"

    note "Generating AFE test PCM files..."
    ffmpeg -hide_banner -loglevel error -y \
        -stream_loop -1 -i "${EXAMPLE_FILES_DIR}/jinitaimei.wav" \
        -f lavfi -i "sine=frequency=440:duration=5" \
        -f lavfi -i "anoisesrc=color=pink:duration=5:amplitude=0.08" \
        -filter_complex "[0:a]aresample=16000,pan=mono|c0=0.5*c0+0.5*c1,atrim=duration=5,asetpts=PTS-STARTPTS[voice];[1:a]asplit=2[far][farecho];[farecho]aecho=0.9:0.95:40|80|120:0.5|0.35|0.2[echoed];[voice][echoed][2:a]amix=inputs=3:duration=first[near]" \
        -map "[far]" -ar 16000 -ac 1 -f s16le "${TEST_FILES_DIR}/afe_far.pcm" \
        -map "[near]" -ar 16000 -ac 1 -f s16le "${TEST_FILES_DIR}/afe_near.pcm"
}

note "CherryAVC example test"
note "  root       : ${ROOT_DIR}"
note "  build_dir  : ${BUILD_DIR}"
note "  out_dir    : ${OUT_DIR}"
note "  frames     : ${FRAME_COUNT} (0 means all)"
note ""

note "Configuring build directory..."
cmake -S "${ROOT_DIR}/examples" -B "${BUILD_DIR}"

note "Building examples..."
cmake --build "${BUILD_DIR}"
note ""

generate_test_files "${EXAMPLE_FILES_DIR}/jinitaimei.wav" "${TEST_FILES_DIR}"
note ""

note "Dedicated demos"
mkdir -p "${OUT_DIR}/avi_dump"
run_case "avi_demo" "${OUT_DIR}/avi_dump" \
    "${BUILD_DIR}/examples/avi_demo" \
    "${EXAMPLE_FILES_DIR}/jinitaimei_480x272.avi" "${OUT_DIR}/avi_dump" "${FRAME_COUNT}"

mkdir -p "${OUT_DIR}/mp4_dump"
run_case "mp4_demo" "${OUT_DIR}/mp4_dump" \
    "${BUILD_DIR}/examples/mp4_demo" \
    "${EXAMPLE_FILES_DIR}/jinitaimei_480x272.mp4" "${OUT_DIR}/mp4_dump" "${FRAME_COUNT}"

note ""
note "AFE 3A demo"
run_case "afe_3a_demo" "${OUT_DIR}/afe_3a_demo.pcm" \
    "${BUILD_DIR}/examples/afe_3a_demo" \
    "${TEST_FILES_DIR}/afe_near.pcm" "${TEST_FILES_DIR}/afe_far.pcm" \
    "${OUT_DIR}/afe_3a_demo.pcm" 16000 "${FRAME_COUNT}"

note ""
note "AE Sonic demo"
run_case "sonic_speed" "${OUT_DIR}/sonic_speed.wav" \
    "${BUILD_DIR}/examples/ae_sonic_demo" \
    "${EXAMPLE_FILES_DIR}/jinitaimei.wav" "${OUT_DIR}/sonic_speed.wav" \
    1.35 1.00 1.00 "${FRAME_COUNT}"
run_case "sonic_pitch" "${OUT_DIR}/sonic_pitch.wav" \
    "${BUILD_DIR}/examples/ae_sonic_demo" \
    "${EXAMPLE_FILES_DIR}/jinitaimei.wav" "${OUT_DIR}/sonic_pitch.wav" \
    1.00 1.20 1.00 "${FRAME_COUNT}"

note ""
note "AE Volume demo"
run_case "ae_vol_down" "${OUT_DIR}/ae_vol_down.wav" \
    "${BUILD_DIR}/examples/ae_vol_demo" \
    "${EXAMPLE_FILES_DIR}/jinitaimei.wav" "${OUT_DIR}/ae_vol_down.wav" \
    128 -60 18 "${FRAME_COUNT}"
run_case "ae_vol_up" "${OUT_DIR}/ae_vol_up.wav" \
    "${BUILD_DIR}/examples/ae_vol_demo" \
    "${EXAMPLE_FILES_DIR}/jinitaimei.wav" "${OUT_DIR}/ae_vol_up.wav" \
    240 -60 18 "${FRAME_COUNT}"

note ""
note "audio_codec_stream_demo"
run_stream_case "stream_aac" aac "${TEST_FILES_DIR}/jinitaimei.aac"
run_stream_case "stream_amr" amr "${TEST_FILES_DIR}/jinitaimei.amr"
run_stream_case "stream_amr_wb" amr "${EXAMPLE_FILES_DIR}/amr_wb_min.awb"
run_stream_case "stream_flac" flac "${TEST_FILES_DIR}/jinitaimei.flac"
run_stream_case "stream_mp3" mp3 "${TEST_FILES_DIR}/jinitaimei.mp3"
run_stream_case "stream_alac" alac "${TEST_FILES_DIR}/jinitaimei_alac.caf"
run_stream_case "stream_alac_24bit" alac "${TEST_FILES_DIR}/jinitaimei_alac_24bit.caf"
run_stream_case "stream_m4a" m4a "${TEST_FILES_DIR}/jinitaimei.m4a"
run_stream_case "stream_ogg_opus" ogg "${TEST_FILES_DIR}/jinitaimei_opus.ogg"
run_stream_case "stream_ogg_vorbis" ogg "${TEST_FILES_DIR}/jinitaimei_vorbis.ogg"
run_stream_case "stream_wav_pcm" wav "${EXAMPLE_FILES_DIR}/jinitaimei.wav"
run_stream_case "stream_wav_adpcm_ima" wav "${TEST_FILES_DIR}/jinitaimei_adpcm_ima.wav"
run_stream_case "stream_wav_g711a" wav "${TEST_FILES_DIR}/jinitaimei_g711a.wav"
run_stream_case "stream_wav_g711u" wav "${TEST_FILES_DIR}/jinitaimei_g711u.wav"
run_stream_case "stream_wav_g722" wav "${TEST_FILES_DIR}/jinitaimei_g722.wav"

run_case "wav_encode_demo" "${OUT_DIR}/wav_encode_demo.wav" \
    "${BUILD_DIR}/examples/wav_encode_demo" \
    "${OUT_DIR}/stream_wav_pcm.pcm" "${OUT_DIR}/wav_encode_demo.wav" 44100 16 2

mkdir -p "${OUT_DIR}/resample_dump"
run_resample_case "resample_44k_mono_u8_i" 44100 1 8 i
run_resample_case "resample_44k_mono_u8_p" 44100 1 8 p
run_resample_case "resample_44k_mono_s16_i" 44100 1 16 i
run_resample_case "resample_44k_mono_s16_p" 44100 1 16 p
run_resample_case "resample_44k_mono_s32_i" 44100 1 32 i
run_resample_case "resample_44k_mono_s32_p" 44100 1 32 p
run_resample_case "resample_44k_stereo_u8_i" 44100 2 8 i
run_resample_case "resample_44k_stereo_u8_p" 44100 2 8 p
run_resample_case "resample_44k_stereo_s16_i" 44100 2 16 i
run_resample_case "resample_44k_stereo_s16_p" 44100 2 16 p
run_resample_case "resample_44k_stereo_s32_i" 44100 2 32 i
run_resample_case "resample_44k_stereo_s32_p" 44100 2 32 p
run_resample_case "resample_44k_3ch_u8_i" 44100 3 8 i
run_resample_case "resample_44k_3ch_u8_p" 44100 3 8 p
run_resample_case "resample_44k_3ch_s16_i" 44100 3 16 i
run_resample_case "resample_44k_3ch_s16_p" 44100 3 16 p
run_resample_case "resample_44k_3ch_s32_i" 44100 3 32 i
run_resample_case "resample_44k_3ch_s32_p" 44100 3 32 p

run_resample_case "resample_48k_mono_u8_i" 32000 1 8 i
run_resample_case "resample_48k_mono_u8_p" 48000 1 8 p
run_resample_case "resample_48k_mono_s16_i" 48000 1 16 i
run_resample_case "resample_48k_mono_s16_p" 48000 1 16 p
run_resample_case "resample_48k_mono_s32_i" 48000 1 32 i
run_resample_case "resample_48k_mono_s32_p" 48000 1 32 p
run_resample_case "resample_48k_stereo_u8_i" 48000 2 8 i
run_resample_case "resample_48k_stereo_u8_p" 48000 2 8 p
run_resample_case "resample_48k_stereo_s16_i" 48000 2 16 i
run_resample_case "resample_48k_stereo_s16_p" 48000 2 16 p
run_resample_case "resample_48k_stereo_s32_i" 48000 2 32 i
run_resample_case "resample_48k_stereo_s32_p" 48000 2 32 p
run_resample_case "resample_48k_3ch_u8_i" 48000 3 8 i
run_resample_case "resample_48k_3ch_u8_p" 48000 3 8 p
run_resample_case "resample_48k_3ch_s16_i" 48000 3 16 i
run_resample_case "resample_48k_3ch_s16_p" 48000 3 16 p
run_resample_case "resample_48k_3ch_s32_i" 48000 3 32 i
run_resample_case "resample_48k_3ch_s32_p" 48000 3 32 p

run_resample_case "resample_32k_mono_u8_i" 32000 1 8 i
run_resample_case "resample_32k_mono_u8_p" 32000 1 8 p
run_resample_case "resample_32k_mono_s16_i" 32000 1 16 i
run_resample_case "resample_32k_mono_s16_p" 32000 1 16 p
run_resample_case "resample_32k_mono_s32_i" 32000 1 32 i
run_resample_case "resample_32k_mono_s32_p" 32000 1 32 p
run_resample_case "resample_32k_stereo_u8_i" 32000 2 8 i
run_resample_case "resample_32k_stereo_u8_p" 32000 2 8 p
run_resample_case "resample_32k_stereo_s16_i" 32000 2 16 i
run_resample_case "resample_32k_stereo_s16_p" 32000 2 16 p
run_resample_case "resample_32k_stereo_s32_i" 32000 2 32 i
run_resample_case "resample_32k_stereo_s32_p" 32000 2 32 p
run_resample_case "resample_32k_3ch_u8_i" 32000 3 8 i
run_resample_case "resample_32k_3ch_u8_p" 32000 3 8 p
run_resample_case "resample_32k_3ch_s16_i" 32000 3 16 i
run_resample_case "resample_32k_3ch_s16_p" 32000 3 16 p
run_resample_case "resample_32k_3ch_s32_i" 32000 3 32 i
run_resample_case "resample_32k_3ch_s32_p" 32000 3 32 p

run_resample_case "resample_16k_mono_u8_i" 16000 1 8 i
run_resample_case "resample_16k_mono_u8_p" 16000 1 8 p
run_resample_case "resample_16k_mono_s16_i" 16000 1 16 i
run_resample_case "resample_16k_mono_s16_p" 16000 1 16 p
run_resample_case "resample_16k_mono_s32_i" 16000 1 32 i
run_resample_case "resample_16k_mono_s32_p" 16000 1 32 p
run_resample_case "resample_16k_stereo_u8_i" 16000 2 8 i
run_resample_case "resample_16k_stereo_u8_p" 16000 2 8 p
run_resample_case "resample_16k_stereo_s16_i" 16000 2 16 i
run_resample_case "resample_16k_stereo_s16_p" 16000 2 16 p
run_resample_case "resample_16k_stereo_s32_i" 16000 2 32 i
run_resample_case "resample_16k_stereo_s32_p" 16000 2 32 p
run_resample_case "resample_16k_3ch_u8_i" 16000 3 8 i
run_resample_case "resample_16k_3ch_u8_p" 16000 3 8 p
run_resample_case "resample_16k_3ch_s16_i" 16000 3 16 i
run_resample_case "resample_16k_3ch_s16_p" 16000 3 16 p
run_resample_case "resample_16k_3ch_s32_i" 16000 3 32 i
run_resample_case "resample_16k_3ch_s32_p" 16000 3 32 p

run_resample_case "resample_8k_mono_u8_i" 8000 1 8 i
run_resample_case "resample_8k_mono_u8_p" 8000 1 8 p
run_resample_case "resample_8k_mono_s16_i" 8000 1 16 i
run_resample_case "resample_8k_mono_s16_p" 8000 1 16 p
run_resample_case "resample_8k_mono_s32_i" 8000 1 32 i
run_resample_case "resample_8k_mono_s32_p" 8000 1 32 p
run_resample_case "resample_8k_stereo_u8_i" 8000 2 8 i
run_resample_case "resample_8k_stereo_u8_p" 8000 2 8 p
run_resample_case "resample_8k_stereo_s16_i" 8000 2 16 i
run_resample_case "resample_8k_stereo_s16_p" 8000 2 16 p
run_resample_case "resample_8k_stereo_s32_i" 8000 2 32 i
run_resample_case "resample_8k_stereo_s32_p" 8000 2 32 p
run_resample_case "resample_8k_3ch_u8_i" 8000 3 8 i
run_resample_case "resample_8k_3ch_u8_p" 8000 3 8 p
run_resample_case "resample_8k_3ch_s16_i" 8000 3 16 i
run_resample_case "resample_8k_3ch_s16_p" 8000 3 16 p
run_resample_case "resample_8k_3ch_s32_i" 8000 3 32 i
run_resample_case "resample_8k_3ch_s32_p" 8000 3 32 p

note ""
note "Summary"
note "  passed : ${pass_count}"
note "  failed : ${fail_count}"
note "  skipped: ${skip_count}"
note "  logs   : ${LOG_DIR}"

if [ ${fail_count} -ne 0 ]; then
    exit 1
fi

exit 0
