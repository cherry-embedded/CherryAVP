#!/usr/bin/env bash

set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
OUTPUT_ROOT="${OUTPUT_ROOT:-output}"
RUN_ID="${RUN_ID:-$(date +%Y%m%d-%H%M%S)}"
OUT_DIR="${OUTPUT_ROOT}/${RUN_ID}"
LOG_DIR="${OUT_DIR}/logs"
TEST_FILES_DIR="${OUTPUT_ROOT}/files"
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
    local mode="$2"
    local value="$3"
    local output="${OUT_DIR}/${name}.pcm"

    run_case "${name}" "${output}" \
        "${BUILD_DIR}/examples/resample_demo" \
        "${TEST_FILES_DIR}/jinitaimei.pcm" "${output}" \
        "${mode}" "${value}" "${FRAME_COUNT}"
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

    note "Generating raw PCM input ..."
    out="${outdir}/jinitaimei.pcm"
    ffmpeg -hide_banner -loglevel error -y \
        -i "${input_wav}" -ar 44100 -ac 2 -f s16le "${out}" ||
        note "failed: ${out}"

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

    note "Generating 3A test PCM..."
    ffmpeg -hide_banner -loglevel error -y \
        -stream_loop -1 -i "${input_wav}" \
        -f lavfi -i "sine=frequency=440:duration=5" \
        -f lavfi -i "anoisesrc=color=pink:duration=5:amplitude=0.08" \
        -filter_complex "[0:a]aresample=16000,pan=mono|c0=0.5*c0+0.5*c1,atrim=duration=5,asetpts=PTS-STARTPTS[voice];[1:a]asplit=2[far][farecho];[farecho]aecho=0.9:0.95:40|80|120:0.5|0.35|0.2[echoed];[voice][echoed][2:a]amix=inputs=3:duration=first[near]" \
        -map "[far]" -ar 16000 -ac 1 -f s16le "${TEST_FILES_DIR}/jinitaimei_afe_3a_far.pcm" \
        -map "[near]" -ar 16000 -ac 1 -f s16le "${TEST_FILES_DIR}/jinitaimei_afe_3a_near.pcm"

    note "Generating howling test PCM..."
    out="${outdir}/jinitaimei_howling.pcm"
    ffmpeg -hide_banner -loglevel error -y \
        -stream_loop -1 -i "${input_wav}" \
        -f lavfi -i "sine=frequency=2600:sample_rate=44100:duration=8" \
        -filter_complex "[0:a]atrim=duration=8,asetpts=PTS-STARTPTS[voice];[1:a]volume=0.8[howl];[voice][howl]amix=inputs=2:duration=first:normalize=0[mix]" \
        -map "[mix]" -ar 44100 -ac 2 -f s16le "${out}" ||
        note "failed: ${out}"

    note "Generating filter low/mid/high test PCM..."
    out="${outdir}/ae_filter_low_mid_high_tones.pcm"
    ffmpeg -hide_banner -loglevel error -y \
        -f lavfi -i "sine=frequency=500:sample_rate=44100:duration=8" \
        -f lavfi -i "sine=frequency=6000:sample_rate=44100:duration=8" \
        -f lavfi -i "sine=frequency=10000:sample_rate=44100:duration=8" \
        -filter_complex "[0:a]volume=0.45[low];[1:a]volume=0.35[mid];[2:a]volume=0.25[high];[low][mid][high]amix=inputs=3:duration=first:normalize=0[mix]" \
        -map "[mix]" -ar 44100 -ac 2 -f s16le "${out}" ||
        note "failed: ${out}"

    note "Generating mixer second input PCM (220 Hz sine)..."
    duration=$(ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 "${input_wav}" 2>/dev/null || echo "5")
    out="${outdir}/ae_mixer_sin220.pcm"
    ffmpeg -hide_banner -loglevel error -y \
        -f lavfi -i "sine=frequency=220:sample_rate=44100" \
        -map 0:a \
        -ac 2 -ar 44100 -f s16le -t "$duration" "${out}" ||
        note "failed: ${out}"

    note "Generating limiter & compressor test PCM..."
    out="${outdir}/jinitaimei_limter_compressor.pcm"
    ffmpeg -hide_banner -loglevel error -y \
        -stream_loop -1 -i "${input_wav}" \
        -f lavfi -i "sine=frequency=440:sample_rate=44100:duration=8" \
        -filter_complex "[0:a]atrim=duration=8,volume=2.5[voice];[1:a]volume=1.6[tone];[voice][tone]amix=inputs=2:duration=first:normalize=0[mix]" \
        -map "[mix]" -ar 44100 -ac 2 -f s16le "${out}" ||
        note "failed: ${out}"
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
    "${TEST_FILES_DIR}/jinitaimei_afe_3a_near.pcm" "${TEST_FILES_DIR}/jinitaimei_afe_3a_far.pcm" \
    "${OUT_DIR}/afe_3a_demo.pcm" 16000 "${FRAME_COUNT}"

note ""
note "AFE Howling demo"
run_case "afe_howling" "${OUT_DIR}/afe_howling.pcm" \
    "${BUILD_DIR}/examples/afe_howling_demo" \
    "${TEST_FILES_DIR}/jinitaimei_howling.pcm" "${OUT_DIR}/afe_howling.pcm" \
    8 6 6 12 4 "${FRAME_COUNT}"

note ""
note "AE Sonic demo"
run_case "sonic_speed" "${OUT_DIR}/sonic_speed.pcm" \
    "${BUILD_DIR}/examples/ae_sonic_demo" \
    "${TEST_FILES_DIR}/jinitaimei.pcm" "${OUT_DIR}/sonic_speed.pcm" \
    1.35 1.00 "${FRAME_COUNT}"
run_case "sonic_pitch" "${OUT_DIR}/sonic_pitch.pcm" \
    "${BUILD_DIR}/examples/ae_sonic_demo" \
    "${TEST_FILES_DIR}/jinitaimei.pcm" "${OUT_DIR}/sonic_pitch.pcm" \
    1.00 1.20 "${FRAME_COUNT}"

note ""
note "AE Volume demo"
run_case "ae_vol_down" "${OUT_DIR}/ae_vol_down.pcm" \
    "${BUILD_DIR}/examples/ae_vol_demo" \
    "${TEST_FILES_DIR}/jinitaimei.pcm" "${OUT_DIR}/ae_vol_down.pcm" \
    128 -60 18 "${FRAME_COUNT}"
run_case "ae_vol_up" "${OUT_DIR}/ae_vol_up.pcm" \
    "${BUILD_DIR}/examples/ae_vol_demo" \
    "${TEST_FILES_DIR}/jinitaimei.pcm" "${OUT_DIR}/ae_vol_up.pcm" \
    240 -60 18 "${FRAME_COUNT}"

note ""
note "AE Mixer demo"
run_case "ae_mixer" "${OUT_DIR}/ae_mixer.pcm" \
    "${BUILD_DIR}/examples/ae_mixer_demo" \
    "${TEST_FILES_DIR}/jinitaimei.pcm" "${TEST_FILES_DIR}/ae_mixer_sin220.pcm" \
    "${OUT_DIR}/ae_mixer.pcm" 0.75 0.50 "${FRAME_COUNT}"

note ""
note "AE Reverb demo"
run_case "ae_reverb" "${OUT_DIR}/ae_reverb.pcm" \
    "${BUILD_DIR}/examples/ae_reverb_demo" \
    "${TEST_FILES_DIR}/jinitaimei.pcm" "${OUT_DIR}/ae_reverb.pcm" "${FRAME_COUNT}"

note ""
note "AE Compressor demo"
run_case "ae_compressor" "${OUT_DIR}/ae_compressor.pcm" \
    "${BUILD_DIR}/examples/ae_compressor_demo" \
    "${TEST_FILES_DIR}/jinitaimei_limter_compressor.pcm" "${OUT_DIR}/ae_compressor.pcm" "${FRAME_COUNT}"

note ""
note "AE Limiter demo"
run_case "ae_limiter" "${OUT_DIR}/ae_limiter.pcm" \
    "${BUILD_DIR}/examples/ae_limiter_demo" \
    "${TEST_FILES_DIR}/jinitaimei_limter_compressor.pcm" "${OUT_DIR}/ae_limiter.pcm" "${FRAME_COUNT}"

note ""
note "AE EQ demo"
run_case "ae_eq" "${OUT_DIR}/ae_eq.pcm" \
    "${BUILD_DIR}/examples/ae_eq_demo" \
    "${TEST_FILES_DIR}/jinitaimei.pcm" "${OUT_DIR}/ae_eq.pcm" "${FRAME_COUNT}"

note ""
note "AE Filter demo"
filter_types=(
    low_pass
    high_pass
    band_pass
    band_stop
    all_pass
    peaking
    low_shelf
    high_shelf
)
filter_inputs=(
    ae_filter_low_mid_high_tones.pcm
    ae_filter_low_mid_high_tones.pcm
    ae_filter_low_mid_high_tones.pcm
    ae_filter_low_mid_high_tones.pcm
    ae_filter_low_mid_high_tones.pcm
    ae_filter_low_mid_high_tones.pcm
    ae_filter_low_mid_high_tones.pcm
    ae_filter_low_mid_high_tones.pcm
)
for filter_type in "${!filter_types[@]}"; do
    filter_name="${filter_types[${filter_type}]}"
    filter_input="${filter_inputs[${filter_type}]}"
    run_case "ae_filter_${filter_name}" "${OUT_DIR}/ae_filter_${filter_name}.pcm" \
        "${BUILD_DIR}/examples/ae_filter_demo" \
        "${TEST_FILES_DIR}/${filter_input}" \
        "${OUT_DIR}/ae_filter_${filter_name}.pcm" \
        "${FRAME_COUNT}" "${filter_type}"
done

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

run_resample_case "resample_rate_8k" rate 8000
run_resample_case "resample_rate_16k" rate 16000
run_resample_case "resample_rate_32k" rate 32000
run_resample_case "resample_rate_48k" rate 48000
run_resample_case "resample_bits_8" bit 8
run_resample_case "resample_bits_24" bit 24
run_resample_case "resample_bits_32" bit 32
run_resample_case "resample_ch_1" channel 1
run_resample_case "resample_ch_3" channel 3
run_resample_case "resample_ch_4" channel 4

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
