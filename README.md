**English | [Simplified Chinese](README_zh.md)**

<h1 align="center" style="margin: 30px 0 30px; font-weight: bold;">CherryAVP</h1>
<p align="center">
	<a href="https://github.com/cherry-embedded/CherryAVP/releases"><img src="https://img.shields.io/github/release/cherry-embedded/CherryAVP.svg"></a>
	<a href="https://github.com/cherry-embedded/CherryAVP/blob/master/LICENSE"><img src="https://img.shields.io/github/license/cherry-embedded/CherryAVP.svg?style=flat-square"></a>
	<a href="https://github.com/cherry-embedded/CherryAVP/actions/workflows/build.yml"><img src="https://github.com/cherry-embedded/CherryAVP/actions/workflows/build.yml/badge.svg"> </a>
</p>

CherryAVP is a tiny and beautiful, low-memory and high-performance audio and video processing library designed for MCUs.

## Reasons for doing this project

- Most chip companies use open‑source libraries, but they do not open‑source their own modifications or additions. I don't understand this—it's just like toolchain vendors who keep their DSP instructions proprietary.
- There is no unified framework or API, so switching to a different chip requires re‑adaptation of the entire codebase.
- The sources of the open‑source libraries used are often disorganised(we only rely on official libraries with specific commit hashes).
- I want to use it.

## Features

- Support audio codec
- Support video codec
- Support Multiple audio front-end algorithms
- Support Multiple audio effects algorithms
- Support sample-rate, channel, bit-depth, data weaver conversion

## Encoders

### Audio Encoding

#### ADPCM

#### AAC

#### AMR

#### FLAC

#### G711

#### G722

#### MP3

#### Opus

#### Vorbis

### Video Encoding

#### MJPEG

We suggest to use Hardware-based MJPEG encoder.

#### H264

We suggest to use Hardware-based H264 encoder.

### Container Muxing

#### WAV

#### M4A

#### OGG

#### AVI

#### MP4

## Decoders

- Supports parsing file format from containers, extracting audio stream offsets and total lengths, and automatically configuring decoder parameters; see the `audio_stream_demux_open` APIs
- Supports frame-based decoding; the input must be a complete audio frame. See the `audio_codec_stream_dec_frame` APIs

> - Raw Opus, Vorbis and no adts AAC cannot identify complete frames, so a complete frame must be provided when using `audio_codec_stream_dec_frame`

### Audio Decoding

#### ADPCM

- Supports IMA ADPCM block PCM decoding
- Input bit depth: 4-bit ADPCM
- Sample rates: all sample rates
- Channels: [1, 8]
- Output: 16-bit signed interleaved PCM

#### ALAC

- Supports ALAC (Apple Lossless) PCM decoding
- ALAC decoder based on **apple-alac**
- Supports ALAC audio in CAF and M4A/MP4 containers
- Reads the ALAC magic cookie and automatically configures frame length, sample rate, channels, and bit depth
- Input bit depth: 16 / 20 / 24 / 32-bit
- Channels: [1, 8]
- Output: 16-bit signed interleaved PCM

#### AAC

- Supports AAC with or without ADTS headers
- AAC PCM decoder based on **opencore-aac**
- Sample rates (Hz): `96000 / 88200 / 64000 / 48000 / 44100 / 32000 / 24000 / 22050 / 16000 / 12000 / 11025 / 8000 / 7350`
- Channels: [1, 2]
- Output: 16-bit signed interleaved PCM

#### AMR

- Supports AMR-NB / AMR-WB file format parsing
- AMR-NB / AMR-WB PCM decoder based on **opencore-amr**
- Sample rates (Hz): AMR-NB `8000`, AMR-WB `16000`
- Channels: 1
- Output: 16-bit signed mono PCM

#### FLAC

- Supports FLAC file format parsing
- FLAC PCM decoder based on **xiph-flac**
- Sample rates (Hz): `8000 / 16000 / 22050 / 24000 / 32000 / 44100 / 48000 / 88200 / 96000 / 176400 / 192000`
- Channels: [1, 8]
- Input bit depth: 8 / 12 / 16 / 20 / 24-bit
- Output: 16-bit signed interleaved PCM

#### G711

- G711A (A-law) and G711U (u-law) PCM decoders
- Input bit depth: 8-bit companded G711
- Sample rates: all sample rates
- Channels: [1, 255]
- Output: 16-bit signed interleaved PCM

#### G722

- Supports G.722 ADPCM PCM decoding
- Input bit depth: 6 / 7 / 8-bit ADPCM codes
- Sample rates: `16000` Hz wideband PCM output, `8000` Hz low-band output
- Channels: 1
- Output: 16-bit signed mono PCM

#### MP3

- Supports MP3 file format parsing
- MP3 PCM decoder based on **minimp3** or **opencore-mp3**
- Sample rates (Hz): MPEG 1: `44100 / 48000 / 32000`; MPEG 2: `22050 / 24000 / 16000`; MPEG 2.5: `11025 / 12000 / 8000`
- Channels: [1, 2]
- Output: 16-bit signed interleaved PCM

#### Opus

- Opus PCM decoder based on **xiph-opus**
- Sample rates: `8000 / 12000 / 16000 / 24000 / 48000`
- Channels: [1, 2]
- Output: 16-bit signed interleaved PCM

#### Vorbis

- Vorbis PCM decoder based on **xiph-vorbis**
- Sample rates (Hz): `8000 / 11025 / 12000 / 16000 / 22050 / 24000 / 32000 / 44100 / 48000`
- Channels: [1, 2]
- Output: 16-bit signed interleaved PCM

### Video Decoding

#### MJPEG

We suggest to use Hardware-based MJPEG decoder.

#### H264

We suggest to use Hardware-based H264 decoder.

### Container Demuxing

#### WAV

- Supports WAV file format parsing
- Supports PCM, IMA ADPCM, G711A, G711U, and G.722

#### OGG

- Supports Ogg Opus file format parsing: `OpusHead` and `OpusTags`
- Supports Ogg Vorbis file format parsing: the three header packets, `identification`, `comment`, and `setup`
- Automatically combine the segments into complete frame based on the segment tables.

#### M4A

- Supports M4A box parsing
- Automatically parses the AAC configuration from the `mp4a` and `esds` boxes
- Automatically parses the ALAC magic cookie from the `alac` box

#### CAF

- Supports CAF file header and chunk parsing
- Automatically extracts the ALAC stream offset, stream size, and decoder configuration

#### AVI

- Supports AVI file format parsing
- Uses peek/pop to extract complete frames

#### MP4

- Supports MP4 box parsing with MJPEG video and AAC audio tracks
- Uses peek/pop to extract complete frames

## Audio Front-End Algorithms

### 3A Algorithm based on WebRTC

- Input / output: 16-bit signed mono PCM
- Sample rates: `8000 / 16000 / 32000` Hz
- Frame duration: fixed `10 ms`, or `sample_rate * AVP_AFE_3A_FRAME_MS / 1000` samples
- Processing model: single-instance mono processing; `near_in`, `far_in`, and `near_out` must use the same sample rate and frame length
- Processing order: split -> HPF -> AGC Analyze -> NS Analyze -> AEC -> NS Process -> VAD -> AGC Process -> merge

Implemented features:

- High-pass filter (HPF): removes DC offset and low-frequency noise
- Acoustic echo cancellation (AEC): based on WebRTC legacy AEC, with far-end reference input, stream delay, and echo status

> - Advanced AEC options: metrics, delay logging, drift/skew compensation, extended filter, delay agnostic mode, and next generation AEC

- Noise suppression (NS): based on WebRTC float NS, with mild / medium / aggressive policies

> - NS observation data: prior speech probability and noise estimate getters

- Automatic gain control (AGC): based on WebRTC legacy AGC, with fixed digital, adaptive digital, and adaptive analog modes

> - AGC configuration: target level, compression gain, limiter, analog level input / output, and saturation warning

- Voice activity detection (VAD): based on WebRTC VAD, with normal / low bitrate / aggressive / very aggressive modes
- 32 kHz band splitting: 32 kHz input is split into two 16 kHz bands; the low band is used by AEC / NS / AGC / VAD and then merged back to full-band output
- Runtime control: AEC / NS / AGC / VAD / HPF can be enabled or disabled, and AEC, NS, AGC, and VAD parameters can be updated at runtime

### Automatic level control (ALC)
### Dynamic range control (DRC)
### Howling suppression

## Audio Effects Algorithms

### Equalizer (EQ)
### Reverb
### Compressor
### Limiter
### Mixer

### Time and pitch modification (Sonic)

The Sonic-based time and pitch effect is based on **sonic** . It provides streaming speed and pitch modification for speech or music.

- Input / output: 16-bit signed interleaved PCM
- Channels: `1 / 2`
- Sample rate: configured by `avp_ae_sonic_config_t.sample_rate`; input and output keep the same sample rate
- Features: independently controls speed, pitch, rate, volume, chord pitch mode, and quality
- Runtime control: set/get speed, pitch, rate, volume, chord pitch, quality, sample rate, and channels; flush, reset, and query available output samples

### Volume control

The volume effect uses a 256-step dB mapping table for mute, attenuation, and amplification of 16-bit
PCM samples.

- Input / output: 16-bit signed PCM, with in-place processing supported
- Channels: unrestricted; `sample_count` is the total number of `int16_t` samples, including all interleaved channels
- Volume index: `0..255`; index `0` is always mute
- dB mapping: indices `1..255` are converted from `min_db..max_db` to Q14 linear gain factors; the default range is `-60..18 dB`
- Saturation: when gain is greater than 1.0, output samples are saturated to the `int16_t` range to avoid integer overflow
- Runtime control: set/get index, set/get dB range, enable/bypass, and query the current Q14 gain

## How to use

Run with the following command to test all cases.

```
./scripts/test_examples.sh
```

## Third-Party Libraries Used

| Name | Purpose | Source |
| --- | --- | --- |
| minimp3 | MP3 decoding | https://github.com/lieff/minimp3/tree/7b590fdcfa5a79c033e76eacc05d0c3e4c79f536 |
| xiph-flac | FLAC encoding and decoding | https://android.googlesource.com/platform/external/flac https://github.com/xiph/flac |
| xiph-opus | Opus encoding and decoding | https://android.googlesource.com/platform/external/libopus https://github.com/xiph/opus |
| xiph-ogg | Ogg bitstream/container | https://android.googlesource.com/platform/external/libogg https://github.com/xiph/ogg |
| xiph-vorbis | Vorbis encoding and decoding | https://android.googlesource.com/platform/external/libvorbis https://github.com/xiph/vorbis/tree/e3c9861ff096d52378e131ff8c334552e09cdffa |
| opencore-aac | AAC decoding | https://android.googlesource.com/platform/external/opencore/+/61bf9af643abf0011dcf82ae8a436aeb7e8aae97 |
| opencore-amr | AMR-NB / AMR-WB encoding and decoding | https://android.googlesource.com/platform/external/opencore/+/61bf9af643abf0011dcf82ae8a436aeb7e8aae97 https://sourceforge.net/p/opencore-amr/code/ci/master/tree/ |
| opencore-mp3 | MP3 decoding | https://android.googlesource.com/platform/external/opencore/+/61bf9af643abf0011dcf82ae8a436aeb7e8aae97 |
| opencore-sbc | SBC decoding | https://android.googlesource.com/platform/external/opencore/+/61bf9af643abf0011dcf82ae8a436aeb7e8aae97 |
| android-aac | AAC encoding | https://cs.android.com/android/platform/superproject/+/android-latest-release:external/aac/xhe-aac/aacfastenc/src/ |
| android-sbc | SBC encoding and decoding | https://cs.android.com/android/platform/superproject/+/android-latest-release:packages/modules/Bluetooth/system/embdrv/sbc/ https://github.com/nxp-upstream/libsbc |
| android-lc3 | LC3 encoding and decoding | https://android.googlesource.com/platform/external/liblc3/|
| apple-alac | ALAC decoding | https://android.googlesource.com/platform/external/alac/+/3502769fc9053ae2acca2663da1a25f4982023bb |
| webrtc | AEC2/NS/AGC/VAD | https://webrtc.googlesource.com/src/webrtc/+/e00dfb9675737cba8fc74efde80c11043cb57089 |
| xiph-speexdsp | AEC/NS/AGC/VAD | https://github.com/xiph/speexdsp/tree/1b28a0f61bc31162979e1f26f3981fc3637095c8 |
| athena-signal | AEC/NS/AGC/VAD/DOA/MVDR/GSC | https://github.com/athena-team/athena-signal/tree/master/athena_signal/kernels |
