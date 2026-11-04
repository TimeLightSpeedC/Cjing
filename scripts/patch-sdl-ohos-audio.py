#!/usr/bin/env python3
"""给 SDL3 的鸿蒙音频后端打补丁：显式设置回调帧数，降低音效延迟。

背景
    SDL 的 ohaudio 后端在打开设备后执行：
        device->sample_frames = OH_AudioRenderer_GetFrameSizeInCallback(...)
    也就是**直接采纳**系统给的帧数（实测 4096 帧 ≈ 93ms）。
    它从不调用 OH_AudioStreamBuilder_SetFrameSizeInCallback，
    所以 SDL 的 SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES 在这条路径上无效。

    该文件里有一段 AAudio 时代的缓冲设置，被 `#if 0` 停用了，
    且用的是 AAudio 接口，对 OHAudio 不成立。

补丁
    在设置延迟模式之后插入一行 OHAudio 的 SetFrameSizeInCallback，
    帧数由编译宏 CJING_OHOS_AUDIO_FRAMES 传入（默认由构建系统给 512）。

    只影响鸿蒙构建（宏只在鸿蒙 CMake 里定义），其它平台编译结果不变。
    幂等：已打过则跳过。
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / ".tools/sdl3-src/src/audio/ohaudio/SDL_ohaudio.c"

MARKER = "CJING_OHOS_AUDIO_FRAMES"
ANCHOR = "ctx.OH_AudioStreamBuilder_SetLatencyMode(builder, AUDIOSTREAM_LATENCY_MODE_FAST);"

if not SRC.exists():
    print(f"  ✗ 找不到 {SRC}（先运行 setup 拉取 SDL 源码）")
    sys.exit(1)

text = SRC.read_text(encoding="utf-8")
if MARKER in text:
    print("  已打过补丁，跳过")
    sys.exit(0)

if ANCHOR not in text:
    print("  ✗ 锚点未找到（SDL 版本可能已变化）")
    sys.exit(1)

patch = ANCHOR + """

#ifdef CJING_OHOS_AUDIO_FRAMES
    // [Cjing] 显式指定回调帧数，避免系统默认的 4096 帧（≈93ms）带来的延迟。
    // 没有这一行，SDL 会在建流后采纳系统的帧数，hint 在该路径上无效。
    // 直接调用 OHAudio 接口（SDL 的 ctx 函数表里没有这一项），
    // 因此鸿蒙构建需要额外链接 ohaudio（见 CMakeLists）。
    OH_AudioStreamBuilder_SetFrameSizeInCallback(
        builder, (int32_t)CJING_OHOS_AUDIO_FRAMES);
#endif"""

SRC.write_text(text.replace(ANCHOR, patch, 1), encoding="utf-8")
print(f"  ✓ 已给 {SRC.name} 打补丁（帧数由 CJING_OHOS_AUDIO_FRAMES 指定）")
