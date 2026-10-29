# Bink Decoder (binkdec)

独立、无依赖的 [FFmpeg](https://ffmpeg.org/) Bink 解复用器 + 视频/音频解码器 C 移植版（LGPL-2.1-or-later）。

A dependency-free, standalone C port of FFmpeg's Bink demuxer, Bink video decoder and Bink audio decoder (LGPL-2.1-or-later).

---

## 特性 / Features

- 纯 C 实现，**零第三方依赖**（库本体只需标准 C 库 + `-lm`）
- Bink 容器（BIK）解复用：多音轨、帧索引、seek 回退关键帧
- Bink 视频解码（BIKi / BIKf 等），输出 YUV420P 平面帧，可转 RGB
- Bink 音频解码（RDFT / DCT 两种模式），输出 float32 PCM
- 与 FFmpeg 逐位一致（视频 PSNR ≈ 168 dB，音频 RMS 误差 < 2e-5）
- Windows / Linux 均可编译（MSYS2 MinGW、MSVC 2022、gcc）
- 附带两个示例程序：CLI 解码器 `bink_test`、SDL2 实时播放器 `bink_player`

- Pure C implementation, **zero third-party dependencies** (the library itself only needs the standard C library + `-lm`)
- Bink container (BIK) demuxing: multiple audio tracks, frame index, seek to previous keyframe
- Bink video decoding (BIKi / BIKf etc.), outputs YUV420P planes, RGB conversion included
- Bink audio decoding (RDFT / DCT), outputs float32 PCM
- Bit-exact vs FFmpeg (video PSNR ≈ 168 dB, audio RMS error < 2e-5)
- Buildable on Windows / Linux (MSYS2 MinGW, MSVC 2022, gcc)
- Two example programs: CLI decoder `bink_test` and SDL2 real-time player `bink_player`

## 支持范围 / Supported scope

| 项目 Item                    | 状态 Status        |
| ---------------------------- | ------------------ |
| Bink1 容器 / Bink1 container | ✅ 支持 Supported  |
| Bink1 视频 / Bink1 video     | ✅ 支持 Supported  |
| Bink1 音频 / Bink1 audio     | ✅ 支持 Supported  |
| Bink2 (BK2)                  | ❌ 不支持（解复用阶段即拒绝）Not supported (rejected at demux) |

> 测试需要自行准备 `.bik` 样本文件。

> You need to provide your own `.bik` sample files for testing.

---

## 构建 / Build

### MSYS2 / MinGW

```sh
# 库 + bink_test + bink_player
make

# 仅库和 bink_test（无 SDL2 依赖）
make bink_test
```

- 要求：`gcc`（C99）、`make`、`libm`
- 播放器 `bink_player` 额外需要 SDL2（如 `mingw-w64-x86_64-SDL2`）；缺 SDL2 时可只构建 `bink_test`
- 编译器/参数可覆盖：`make CC=clang CFLAGS="-O3"`

### Visual Studio 2022

直接打开 [`Bink.sln`](Bink.sln)（`bink_test` 工程，v145 工具集，支持 x64/Win32 × Debug/Release）。播放器因依赖 MSVC 版 SDL2 未纳入工程。

---

## 使用 / Usage

### bink_test（CLI 解码器 / CLI decoder）

```sh
bink_test <file.bik> [out_prefix]
```

解码全部帧，每帧输出一张 RGB PPM 图片（`out_prefix_00000.ppm` …），并把第一条音轨写成 16-bit WAV（`out_prefix_audio.wav`）。

Decodes every frame, writes each frame as an RGB PPM image (`out_prefix_00000.ppm`, ...) and dumps the first audio track to a 16-bit WAV (`out_prefix_audio.wav`).

```sh
bink_test movie.bik out
# 输出 / outputs: out_00000.ppm ... out_00358.ppm, out_audio.wav
```

### bink_player（SDL2 实时播放器 / SDL2 real-time player）

```sh
bink_player <file.bik>
```

实时播放视频 + 第一条音轨，按 60fps 节奏显示。`ESC` 或关闭窗口退出，窗口可拉伸。

Plays the video plus the first audio track in real time, paced at 60 fps. Press `ESC` or close the window to quit; the window is resizable.

---

## 目录结构 / Layout

```
bink_common.h     公共宏/工具（内存、对齐、裁剪、日志） Common macros & helpers
bink_bitstream.c/h 位流读写 + VLC 解码  Bitstream reader + VLC decoding
bink_demux.c/h     容器解复用  Container demuxing
bink_video.c/h     视频解码  Video decoding
bink_audio.c/h     音频解码（RDFT/DCT） Audio decoding (RDFT/DCT)
bink_api.c/h       统一解码 API（open/decode/close） Unified decoding API
bink_data.h        DCT/残差扫描序等数据表  Scan-order data tables
bink_test.c        CLI 示例  CLI example
bink_player.c      SDL2 播放器示例  SDL2 player example
Makefile           MinGW/gcc 构建  Build for MinGW/gcc
Bink.sln           Visual Studio 2022 工程  Visual Studio 2022 solution
```

## 使用 API / Using the API

```c
#include "bink_api.h"

BinkFile *f = NULL;
if (bink_open(&f, "movie.bik") < 0)
    return -1;

BinkFrameResult fr;
while (bink_decode_frame(f, &fr) > 0) {
    /* fr.planes[0..2] 为 YUV420P，fr.audio[0] 为 float32 PCM */
    /* fr.planes[0..2] are YUV420P, fr.audio[0] is float32 PCM */
}
bink_close(&f);
```

---

## 许可证 / License

LGPL-2.1-or-later，请参见 [LICENSE](LICENSE)。

本项目是 FFmpeg Bink 解码器的独立移植版，代码源自 FFmpeg（LGPL-2.1-or-later，作者 Konstantin Shishkov、Peter Ross 等），FFmpeg 版权与许可声明已在各源文件头部保留。FFmpeg 不在本仓库内。

LGPL-2.1-or-later, see [LICENSE](LICENSE).

This project is a standalone port of FFmpeg's Bink decoders. The code derives from FFmpeg (LGPL-2.1-or-later, by Konstantin Shishkov, Peter Ross, et al.); the FFmpeg copyright and license notices are kept in the header of each source file. FFmpeg itself is not bundled in this repository.
