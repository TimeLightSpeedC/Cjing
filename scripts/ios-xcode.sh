#!/usr/bin/env bash
# =============================================================================
#  生成 iOS 的 Xcode 工程并打开（用于断点调试 / 模拟器手动测试 / 后续签名）
#
#  背景：本项目用 CMake 统一三平台构建（Ninja 走 CI），
#        但交互式调试用 Xcode 更方便 —— CMake 可以直接生成 Xcode 工程。
#
#  用法：
#      ./scripts/ios-xcode.sh             # 模拟器版本（默认）
#      ./scripts/ios-xcode.sh device      # 真机版本（需要签名配置）
#      ./scripts/ios-xcode.sh simulator --no-open
#
#  注意：
#    * 生成目录与 Ninja 的 build-ios-sim 完全独立，互不影响；
#    * 改了 CMakeLists 后 Xcode 会通过 ZERO_CHECK 自动重跑 cmake；
#    * 真机版本需要在 Xcode 的 Signing & Capabilities 里选 Team。
# =============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

MODE="${1:-simulator}"
OPEN_PROJECT=1
for arg in "$@"; do
    [ "$arg" = "--no-open" ] && OPEN_PROJECT=0
done

case "$MODE" in
    simulator|sim)
        BUILD="$ROOT/build-ios-xcode"
        SYSROOT="iphonesimulator"
        LABEL="模拟器"
        ;;
    device|dev)
        BUILD="$ROOT/build-ios-xcode-device"
        SYSROOT="iphoneos"
        LABEL="真机（需要签名）"
        ;;
    *)
        echo "用法: $0 [simulator|device] [--no-open]"
        exit 1
        ;;
esac

# ---- 源码完整性预检 ----
# CMake 在"生成阶段"才会报源文件缺失，错误信息（Cannot find source file）
# 不提示是仓库被改动过，容易误判成工程配置问题。这里提前拦住。
_missing=0
for rel in core/src/game.cpp core/src/layout.cpp core/src/tap.cpp \
           platform_sdl/app.cpp platform_sdl/sdl_view.cpp; do
    if [ ! -f "$ROOT/$rel" ]; then
        echo "✗ 缺少源码: $rel"
        _missing=1
    fi
done
if [ "$_missing" != "0" ]; then
    echo
    echo "  仓库可能不完整（文件被删除或移动）。先检查："
    echo "    git -C \"$ROOT\" status --short"
    echo "  若显示被删除，恢复："
    echo "    git -C \"$ROOT\" restore -- core platform_sdl"
    exit 1
fi

# ---- 找一个可用的 cmake ----
CMAKE_BIN="${CMAKE_BIN:-}"
if [ -z "$CMAKE_BIN" ]; then
    if command -v cmake > /dev/null 2>&1; then
        CMAKE_BIN="$(command -v cmake)"
    else
        for c in \
            "/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake" \
            "/Applications/CLion.app/Contents/bin/cmake/mac/x64/bin/cmake"; do
            [ -x "$c" ] && CMAKE_BIN="$c" && break
        done
    fi
fi
if [ -z "$CMAKE_BIN" ]; then
    echo "✗ 未找到 cmake：请安装 CMake 或用 CMAKE_BIN=<path> 指定"
    exit 1
fi

echo "cmake  : $CMAKE_BIN"
echo "目标   : ${LABEL}（${SYSROOT}）"
echo "目录   : ${BUILD}"

# ---- 生成器切换保护 ----
# CMake 的生成器绑定在构建目录里，从 Ninja 换成 Xcode 会直接报错。
if [ -f "$BUILD/CMakeCache.txt" ]; then
    if ! grep -q "CMAKE_GENERATOR:INTERNAL=Xcode" "$BUILD/CMakeCache.txt" 2>/dev/null; then
        echo "  该目录已用其它生成器配置过，删除后重建：${BUILD}"
        rm -rf "$BUILD"
    fi
fi

LOG="$ROOT/.tools/ios-xcode-configure.log"
mkdir -p "$(dirname "$LOG")"
if ! "$CMAKE_BIN" -S "$ROOT/ios" -B "$BUILD" -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT="$SYSROOT" \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_XCODE_GENERATE_SCHEME=ON > "$LOG" 2>&1; then
    echo
    echo "✗ CMake 生成 Xcode 工程失败，最后 20 行："
    tail -20 "$LOG" | sed 's/^/    /'
    echo
    echo "  完整日志: ${LOG}"
    echo "  常见原因："
    echo "    1. Xcode 命令行工具未选中 → sudo xcode-select -s /Applications/Xcode.app"
    echo "    2. 首次运行 Xcode 未同意协议 → sudo xcodebuild -license accept"
    echo "    3. 真机版本未配置签名 Team"
    exit 1
fi

PROJ="$BUILD/cjing.xcodeproj"
if [ ! -d "$PROJ" ]; then
    echo "✗ 未生成工程：$PROJ"
    exit 1
fi

echo
echo "✓ 工程已生成：${PROJ}"
echo "  产物位置：${BUILD}/Debug-${SYSROOT}/cjing.app"
echo "  在 Xcode 中：选 scheme「cjing」→ 选设备 → Cmd+R"

if [ "$OPEN_PROJECT" = "1" ]; then
    open "$PROJ"
fi
