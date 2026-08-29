# WAV 转换为 CherryAVP 音频格式

本文以 `input.wav` 为输入，使用 FFmpeg 生成 CherryAVP 示例中常用的音频格式。

先确认输入 WAV 的参数：

```bash
ffprobe -hide_banner -v error \
  -show_entries stream=codec_name,sample_rate,channels,bits_per_sample \
  -of default=noprint_wrappers=1 input.wav
```

## AFE 3A PCM 测试

生成带噪音和回声的测试 PCM：

```bash
ffmpeg -hide_banner -y \
  -stream_loop -1 -i examples/files/jinitaimei.wav \
  -f lavfi -i "sine=frequency=440:duration=5" \
  -f lavfi -i "anoisesrc=color=pink:duration=5:amplitude=0.08" \
  -filter_complex "[0:a]aresample=16000,pan=mono|c0=0.5*c0+0.5*c1,atrim=duration=5,asetpts=PTS-STARTPTS[voice];[1:a]asplit=2[far][farecho];[farecho]aecho=0.9:0.95:40|80|120:0.5|0.35|0.2[echoed];[voice][echoed][2:a]amix=inputs=3:duration=first[near]" \
  -map "[far]" -ar 16000 -ac 1 -f s16le far.pcm \
  -map "[near]" -ar 16000 -ac 1 -f s16le near.pcm
```

运行 AFE 3A demo，输出处理后的 PCM：

```bash
./avp_afe_3a_demo near.pcm far.pcm output.pcm 16000
```

`near.pcm` 是麦克风输入（jinitaimei 人声 + 440Hz 回声 + 粉红噪声），
`far.pcm` 是扬声器参考信号，输出为 16-bit 单声道 PCM。

## PCM WAV

16-bit little-endian PCM：

```bash
ffmpeg -i input.wav -c:a pcm_s16le output_pcm_s16le.wav
```

其他常用 PCM 位宽：

```bash
ffmpeg -i input.wav -c:a pcm_s8     output_pcm_s8.wav
ffmpeg -i input.wav -c:a pcm_s24le  output_pcm_s24le.wav
ffmpeg -i input.wav -c:a pcm_s32le  output_pcm_s32le.wav
```

## IMA ADPCM

CherryAVP 的 WAV ADPCM 解码器对应 IMA ADPCM WAV：

```bash
ffmpeg -i input.wav -c:a adpcm_ima_wav output_adpcm_ima.wav
```

ADPCM 会按 WAV block 对齐，最终时长可能比原始 PCM 多几个采样点。

## G.711 A-law / u-law

G.711 A-law：

```bash
ffmpeg -i input.wav -c:a pcm_alaw output_g711a.wav
```

G.711 u-law：

```bash
ffmpeg -i input.wav -c:a pcm_mulaw output_g711u.wav
```

如果目标是电话语音，通常使用单声道 8 kHz：

```bash
ffmpeg -i input.wav -ar 8000 -ac 1 -c:a pcm_alaw output_g711a_8k_mono.wav
ffmpeg -i input.wav -ar 8000 -ac 1 -c:a pcm_mulaw output_g711u_8k_mono.wav
```

## G.722

G.722 通常使用单声道 16 kHz、64 kbps：

```bash
ffmpeg -i input.wav -ar 16000 -ac 1 -c:a adpcm_g722 output_g722.wav
```

## AAC

生成 ADTS AAC：

```bash
ffmpeg -i input.wav -c:a aac -b:a 128k -f adts output.aac
```

生成 M4A/MP4 中的 AAC：

```bash
ffmpeg -i input.wav -c:a aac -b:a 128k output.m4a
```

## ALAC

ALAC 通常放在 M4A 容器中：

```bash
ffmpeg -i input.wav -c:a alac output_alac.m4a
```

## FLAC

```bash
ffmpeg -i input.wav -c:a flac output.flac
```

指定压缩级别，`0` 最快，`12` 压缩率最高：

```bash
ffmpeg -i input.wav -c:a flac -compression_level 5 output.flac
```

## MP3

使用 FFmpeg 内置 MP3 编码器：

```bash
ffmpeg -i input.wav -c:a mp3 -b:a 128k output.mp3
```

使用 libmp3lame：

```bash
ffmpeg -i input.wav -c:a libmp3lame -q:a 4 output.mp3
```

## Opus

生成 Ogg Opus：

```bash
ffmpeg -i input.wav -ar 48000 -c:a libopus -b:a 128k output.opus
```

或者显式指定 Ogg 容器：

```bash
ffmpeg -i input.wav -ar 48000 -c:a libopus -b:a 128k -f ogg output_opus.ogg
```

## Vorbis

生成 Ogg Vorbis：

```bash
ffmpeg -i input.wav -c:a libvorbis -q:a 5 -f ogg output_vorbis.ogg
```

## SBC

SBC 是裸码流，没有 WAV 头，使用 `.sbc` 文件保存：

```bash
ffmpeg -i input.wav \
  -ar 44100 -ac 2 \
  -c:a sbc -b:a 128k -f sbc output.sbc
```

SBC 允许的采样率通常为 `16000`、`32000`、`44100` 或 `48000` Hz，声道一般为单声道或双声道。

## AMR-NB

AMR-NB 必须使用单声道 8 kHz。FFmpeg 需要编译时启用 AMR-NB encoder，例如 `libopencore_amrnb`：

```bash
ffmpeg -i input.wav \
  -ar 8000 -ac 1 \
  -c:a libopencore_amrnb -b:a 12.2k \
  -f amr output.amr
```

当前系统的 FFmpeg 如果提示：

```text
Unknown encoder 'libopencore_amrnb'
```

说明该 FFmpeg 没有编译 AMR-NB 编码器，需要安装或重新编译带 AMR encoder 的 FFmpeg。

## AMR-WB

AMR-WB 必须使用单声道 16 kHz。FFmpeg 需要启用 `libvo_amrwbenc`：

```bash
ffmpeg -i input.wav \
  -ar 16000 -ac 1 \
  -c:a libvo_amrwbenc -b:a 23.85k \
  -f amr output.awb
```

常用 AMR-WB 码率包括 `6.60k`、`8.85k`、`12.65k`、`14.25k`、`15.85k`、`18.25k`、`19.85k`、`23.05k` 和 `23.85k`。

## 一次生成常用格式

以下命令以 `jinitaimei.wav` 为例：

```bash
INPUT=custom_demo/sd_fatfs/CherryAVP/examples/files/jinitaimei.wav
OUT=custom_demo/sd_fatfs/CherryAVP/examples/files

ffmpeg -i "$INPUT" -c:a adpcm_ima_wav "$OUT/jinitaimei_adpcm_ima.wav"
ffmpeg -i "$INPUT" -c:a pcm_alaw       "$OUT/jinitaimei_g711a.wav"
ffmpeg -i "$INPUT" -c:a pcm_mulaw      "$OUT/jinitaimei_g711u.wav"
ffmpeg -i "$INPUT" -ar 16000 -ac 1 -c:a adpcm_g722 "$OUT/jinitaimei_g722.wav"
ffmpeg -i "$INPUT" -c:a aac -b:a 128k   "$OUT/jinitaimei_aac.m4a"
ffmpeg -i "$INPUT" -c:a alac            "$OUT/jinitaimei_alac.m4a"
ffmpeg -i "$INPUT" -c:a flac            "$OUT/jinitaimei.flac"
ffmpeg -i "$INPUT" -c:a libmp3lame -q:a 4 "$OUT/jinitaimei.mp3"
ffmpeg -i "$INPUT" -ar 48000 -c:a libopus -b:a 128k "$OUT/jinitaimei.opus"
ffmpeg -i "$INPUT" -c:a libvorbis -q:a 5 -f ogg "$OUT/jinitaimei_vorbis.ogg"
ffmpeg -i "$INPUT" -ar 44100 -ac 2 -c:a sbc -b:a 128k -f sbc "$OUT/jinitaimei.sbc"
```

AMR 命令需要单独设置采样率和声道，不能直接沿用上面的双声道 44.1 kHz 输入参数。

## 检查输出格式

```bash
ffprobe -hide_banner -v error \
  -show_entries format=format_name \
  -show_entries stream=codec_name,sample_rate,channels,bits_per_sample \
  -of default=noprint_wrappers=1 output_file
```

查看本机是否具备某个编码器：

```bash
ffmpeg -hide_banner -encoders | grep -Ei \
  'aac|alac|amr|adpcm|flac|mp3|opus|vorbis|sbc|pcm'
```
