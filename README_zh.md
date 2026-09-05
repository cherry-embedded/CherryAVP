**[English](README.md) | 简体中文**

<h1 align="center" style="margin: 30px 0 30px; font-weight: bold;">CherryAVP</h1>
<p align="center">
	<a href="https://github.com/cherry-embedded/CherryAVP/releases"><img src="https://img.shields.io/github/release/cherry-embedded/CherryAVP.svg"></a>
	<a href="https://github.com/cherry-embedded/CherryAVP/blob/master/LICENSE"><img src="https://img.shields.io/github/license/cherry-embedded/CherryAVP.svg?style=flat-square"></a>
	<a href="https://github.com/cherry-embedded/CherryAVP/actions/workflows/build.yml"><img src="https://github.com/cherry-embedded/CherryAVP/actions/workflows/build.yml/badge.svg"> </a>
</p>

CherryAVP 是一个小而美，低内存，高性能的专为 MCU 设计的 Audio & Video 处理库。

## 做这个项目的原因

- 大部分芯片公司都是使用的开源库，但是他们自己不开源，我不理解，就像做工具链的不开源 DSP 指令一样
- 没有统一的框架和 API，换芯片要重新适配
- 使用的开源库来源混乱（我们只使用官方库 + 对应 commit 版本）
- 我要用

## 特性

- 支持音频编解码
- 支持视频编解码
- 支持多种音频前端算法
- 支持多种音频效果算法
- 支持采样率转换，声道转换，位深转换，数据交织和解交织
- 支持 MFCC (split frame -> window -> FFT -> energy -> mel filterbank -> log -> DCT)

## 编码器

### 音频编码

#### ADPCM

#### AAC

#### AMR

#### FLAC

#### G711

#### MP3

#### Opus

#### Vorbis

### 视频编码

#### MJPEG

我们推荐使用硬件 MJPEG 编码器

#### H264

我们推荐使用硬件 H264 编码器

### 容器封装

#### WAV

#### M4A

#### OGG

#### AVI

## 解码器

- 支持按文件/容器解析，提取音频流偏移和总长度，自动配置各类解码器参数，对应 `audio_stream_demux_open` 相关 API
- 支持按帧解码，要求输入必须是完整的音频帧，对应 `audio_codec_stream_dec_frame` 相关 API

> - 原始 opus 和 vorbis, 无 adts AAC 无法识别完整帧，因此如果使用 `audio_codec_stream_dec_frame` 时必须输入完整帧

### 音频解码

#### ADPCM

- 支持 IMA ADPCM 块 PCM 解码
- 输入位深：4-bit ADPCM
- 采样率：所有采样率
- 声道数：[1, 2]
- 输出：16-bit signed interleaved PCM

#### ALAC

- 支持 ALAC（Apple Lossless）PCM 解码
- 基于 **apple-alac** 的 ALAC 解码器
- 支持 CAF、M4A/MP4 中的 ALAC 音频
- 支持读取 ALAC magic cookie，自动配置帧长度、采样率、声道数和位深
- 输入位深：16 / 20 / 24 / 32-bit
- 声道数：[1, 8]
- 输出：16-bit signed interleaved PCM

#### AAC

- 支持有 / 无 ADTS 头文件
- 基于 **opencore-aac** 的 AAC PCM 解码器
- 采样率 (Hz)：`96000 / 88200 / 64000 / 48000 / 44100 / 32000 / 24000 / 22050 / 16000 / 12000 / 11025 / 8000 / 7350`
- 声道数：[1, 2]
- 输出：16-bit signed interleaved PCM

#### AMR

- 支持 AMR-NB / AMR-WB 文件格式解析
- 基于 **opencore-amr** 的 AMR-NB / AMR-WB PCM 解码器
- 采样率 (Hz)：AMR-NB `8000`，AMR-WB `16000`
- 声道数：1
- 输出：16-bit signed mono PCM

#### FLAC

- 支持 FLAC 文件格式解析
- 基于 **xiph-flac** 的 flac PCM 解码器
- 采样率 (Hz)：`8000 / 16000 / 22050 / 24000 / 32000 / 44100 / 48000 / 88200 / 96000 / 176400 / 192000`
- 声道数：[1, 8]
- 输入位深：8 / 12 / 16 / 20 / 24-bit
- 输出：16-bit signed interleaved PCM

#### G711

- G711A（A-law）和 G711U（u-law）PCM 解码器
- 输入位深：8-bit companded G711
- 采样率：所有采样率
- 声道数：[1, 255]
- 输出：16-bit signed interleaved PCM

#### G722

- 支持 G.722 ADPCM PCM 解码
- 输入位深：6 / 7 / 8-bit ADPCM code
- 采样率：`16000` Hz 宽带 PCM 输出，`8000` Hz low-band 输出
- 声道数：1
- 输出：16-bit signed mono PCM

#### MP3

- 支持 MP3 文件格式解析
- 基于 **minimp3** 或者 **opencore-mp3** 的 MP3 PCM 解码器
- 采样率 (Hz)：MPEG 1：`44100 / 48000 / 32000`；MPEG 2：`22050 / 24000 / 16000`；MPEG 2.5：`11025 / 12000 / 8000`
- 声道数：[1, 2]
- 输出：16-bit signed interleaved PCM

#### Opus

- 基于 **xiph-opus** 的 Opus PCM 解码器
- 采样率：`8000 / 12000 / 16000 / 24000 / 48000`
- 声道数：[1, 2]
- 输出：16-bit signed interleaved PCM

#### Vorbis

- 基于 **xiph-vorbis** 的 Vorbis PCM 解码器
- 采样率 (Hz)：`8000 / 11025 / 12000 / 16000 / 22050 / 24000 / 32000 / 44100 / 48000`
- 声道数：[1, 2]
- 输出：16-bit signed interleaved PCM

### 视频解码

#### MJPEG

我们推荐使用硬件 MJPEG 解码器

#### H264

我们推荐使用硬件 H264 解码器

### 容器解封装

#### WAV

- 支持 WAV 文件格式解析
- 支持 PCM、IMA ADPCM、G711A、G711U 和 G.722

#### OGG

- 支持 Ogg Opus 文件格式解析：OpusHead、OpusTags
- 支持 Ogg Vorbis 文件格式解析：identification/comment/setup 三个 header packet
- 内部自动根据 segment table 组成完整帧

#### M4A

- 支持 M4A box 解析
- 根据 `mp4a` 和 `esds` box 自动解析 AAC 配置
- 根据 `alac` 自动解析 ALAC magic cookie

#### CAF

- 支持 CAF 文件头和 chunk 解析
- 自动提取 ALAC 音频流偏移、长度和 decoder 配置

#### AVI

- 支持 AVI 文件格式解析
- 使用 peek/pop 方式提取完整帧

#### MP4

- 支持包含 MJPEG 视频轨和 AAC 音频轨的 MP4 box 解析
- 使用 peek/pop 方式提取完整帧

## 音频前端算法

### 基于 WebRTC 的 3A 算法

- 输入 / 输出：16-bit signed mono PCM
- 采样率：`8000 / 16000 / 32000` Hz
- 帧长：固定 `10 ms`，即 `sample_rate * AVP_AFE_3A_FRAME_MS / 1000` 个采样点
- 处理方式：单实例单声道处理，要求 `near_in`、`far_in` 和 `near_out` 使用相同采样率和相同帧长
- 处理流程：split -> HPF -> AGC Analyze -> NS Analyze -> AEC -> NS Process -> VAD -> AGC Process -> merge

支持以下功能：

- 高通滤波 (HPF)：去除直流和低频噪声
- 回声消除 (AEC)：基于 WebRTC legacy AEC，支持 far-end reference 输入、stream delay、echo status

> - AEC 高级能力：支持 metrics、delay logging、drift/skew compensation、extended filter、delay agnostic、next generation AEC

- 噪声抑制 (NS)：基于 WebRTC float NS，支持 mild / medium / aggressive 三档策略

> - NS 观测信息：支持获取 prior speech probability 和 noise estimate

- 自动增益控制 (AGC)：基于 WebRTC legacy AGC，支持 fixed digital、adaptive digital、adaptive analog 三种模式

> - AGC 配置：支持 target level、compression gain、limiter、analog level 输入 / 输出、saturation warning

- 语音活动检测 (VAD)：基于 WebRTC VAD，支持 normal / low bitrate / aggressive / very aggressive 四档模式
- 32 kHz 分频处理：32 kHz 输入会拆成两个 16 kHz 频带，低频带用于 AEC / NS / AGC / VAD，处理完成后合成为全带输出
- 支持运行时控制：支持开关 AEC / NS / AGC / VAD / HPF，并支持动态调整 AEC、NS、AGC、VAD 相关配置

### 自动电平控制 (ALC)
### 动态范围控制 (DRC)
### 啸叫抑制 (Howling)

自适应啸叫抑制器使用 radix-2 FFT 按短帧分析信号，并结合 PAPR、PHPR 和 PNPR 三个判据检测窄带峰值，再使用最多 4 个 IIR 陷波器跟踪和抑制啸叫频点，每个通道独立维护陷波器状态。

- 帧长：按 `sample_rate` 自动推导，目标 `10 ms`，向下取 2 的幂并限制在 `64..1024`，可通过 `avp_afe_howling_get_frame_samples()` 获取
- 处理粒度：每次调用处理一帧 FFT（`avp_afe_howling_get_frame_samples(handle) * channels` 个交错采样）
- 检测阈值：PAPR (-10..20 dB)、PHPR (0..100 dB)、PNPR (0..100 dB)
- FFT：默认内置 float radix-2 FFT，也可注入 `void fft(float *src, uint32_t m)` 或 `void fft(int16_t *src, uint32_t m)` 形式的外部回调

## 音频效果算法

### 滤波器 (Filter)

支持低通、高通、带通、带阻 / 陷波、全通、峰值、低频搁架和高频搁架滤波器。

### 压缩器 (Compressor)

支持 16-bit signed interleaved PCM 输入，使用峰值包络跟踪，支持配置 threshold、ratio、attack、release 和 makeup gain。

### 均衡器 (EQ)

参数均衡器基于 RBJ 双二阶滤波器级联实现，支持 peaking、低频/高频搁架、低通和高通频段。

- 最多支持 8 个频段和 8 个交错声道
- 每个频段可配置类型、频率、dB 增益和 Q 值/斜率

### 限制器 (Limiter)

支持 16-bit signed interleaved PCM 输入，输出限制在配置的 ceiling 内，并使用 attack/release 平滑降低削波和瞬态峰值。

### 混音器 (Mixer)

支持最多 8 路 16-bit signed interleaved PCM，每个输入通道都可以独立设置权重（音量），并支持平滑的淡入淡出过渡。

### 混响 (Reverb)

流式混响使用 4 路并行反馈延时线，支持调节 room size、damping、wet 和 dry 混合比例。

- 输入 / 输出：16-bit signed interleaved PCM，支持多声道
- 支持运行时控制

### 变速/变调 (Sonic)

支持 16-bit signed interleaved PCM 的变速变调控制。

### 音量控制 (Volume Control)

基于 256 档 dB 映射表实现，支持 16-bit signed interleaved PCM 的静音、衰减和放大。

## 如何使用

执行下面脚本，一键测试所有例程。

```
./scripts/test_examples.sh
```

## 使用到的第三方库

| 名称 | 用途 | 源码地址 |
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
| kissfft | FFT | https://github.com/mborgerding/kissfft/commit/7bce4153c6bc8aba2db0e889e576f9d00505cbe1 |
| sonic | sonic | https://github.com/waywardgeek/sonic |