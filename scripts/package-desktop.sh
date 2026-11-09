#!/usr/bin/env bash
# =============================================================================
#  scripts/package-desktop.sh —— 桌面三平台分发包（P5-T9/T10/T11）
#
#  在【当前操作系统】上构建并打包：
#    macOS   → Cjing.app（ad-hoc 签名）+ .zip，有 hdiutil 时再出 .dmg
#    Linux   → 目录 + .tar.gz（静态链接 SDL，无外部 .so 依赖）
#    Windows → 目录 + .zip（静态链接 SDL，无 DLL 依赖）
#
#  用法：
#    ./scripts/package-desktop.sh            # 产物写到 dist/
#    ./scripts/package-desktop.sh --no-build # 复用已有 build-dist
#
#  说明：
#    · 使用 CJING_STATIC=ON：SDL3/SDL3_ttf 全部静态链接，包内只有可执行文件 + assets
#    · 资源目录必须与可执行文件同级（SDL_GetBasePath()/assets）
#    · 未做代码签名/公证：macOS 收件人需在「隐私与安全性」里放行，或自行签名
# =============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="0.2.0"
BUILD="$ROOT/build-dist"
DIST="$ROOT/dist"
STAGE="$ROOT/build-dist/package"
DO_BUILD=1

for arg in "$@"; do
    case "$arg" in
        --no-build) DO_BUILD=0 ;;
        -h|--help)  sed -n '2,22p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "未知参数: $arg" >&2; exit 1 ;;
    esac
done

OS="$(uname -s)"
mkdir -p "$DIST"

info() { printf '  %s\n' "$*"; }

# ---- 找 cmake / ninja（CI 在 PATH；本机可能只有 CLion 自带）----
CMAKE_BIN="${CMAKE:-}"
if [ -z "$CMAKE_BIN" ]; then
    if command -v cmake >/dev/null 2>&1; then
        CMAKE_BIN="$(command -v cmake)"
    elif [ -x "/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake" ]; then
        CMAKE_BIN="/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake"
    else
        echo "找不到 cmake：请安装或设置 CMAKE 环境变量" >&2
        exit 1
    fi
fi

GEN_ARGS=()
if command -v ninja >/dev/null 2>&1; then
    GEN_ARGS=(-G Ninja)
elif [ -x "/Applications/CLion.app/Contents/bin/ninja/mac/aarch64/ninja" ]; then
    GEN_ARGS=(-G Ninja -DCMAKE_MAKE_PROGRAM=/Applications/CLion.app/Contents/bin/ninja/mac/aarch64/ninja)
fi
info "cmake: $CMAKE_BIN ${GEN_ARGS[*]:-（默认生成器）}"

# ---- 构建（静态链接，Release）----
# macOS 的 Universal 不能一次编译：SDL_ttf 内置的 harfbuzz 会按主机（arm64）
# 给 x86_64 切片也加上 -march=armv8-a+sve2，clang 直接报 unknown target CPU。
# 因此分架构各编一次，再用 lipo 合并。
build_one() {
    local arch="$1" dir="$2"
    info "构建 $arch → $dir"
    mkdir -p "$dir"
    "$CMAKE_BIN" -S "$ROOT" -B "$dir" "${GEN_ARGS[@]}" \
        -DCMAKE_OSX_ARCHITECTURES="$arch" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCJING_STATIC=ON \
        -DCJING_BUILD_TESTS=OFF \
        -DCJING_BUILD_EXAMPLES=OFF > "$dir/configure.log" 2>&1
    "$CMAKE_BIN" --build "$dir" --config Release --parallel 8 > "$dir/build.log" 2>&1
}

if [ "$DO_BUILD" -eq 1 ]; then
    info "配置并构建（CJING_STATIC=ON, Release）"
    if [ "$OS" = "Darwin" ]; then
        rm -rf "$ROOT/build-dist-arm64" "$ROOT/build-dist-x86_64"
        build_one arm64  "$ROOT/build-dist-arm64"
        build_one x86_64 "$ROOT/build-dist-x86_64"
        mkdir -p "$BUILD"
        lipo -create \
            "$ROOT/build-dist-arm64/bin/CjingApp" \
            "$ROOT/build-dist-x86_64/bin/CjingApp" \
            -output "$BUILD/CjingApp"
        info "已合并 Universal: $(lipo -archs "$BUILD/CjingApp")"
    else
        rm -rf "$BUILD"
        mkdir -p "$BUILD"
        "$CMAKE_BIN" -S "$ROOT" -B "$BUILD" "${GEN_ARGS[@]}" \
            -DCMAKE_BUILD_TYPE=Release \
            -DCJING_STATIC=ON \
            -DCJING_BUILD_TESTS=OFF \
            -DCJING_BUILD_EXAMPLES=OFF > "$BUILD/configure.log" 2>&1
        "$CMAKE_BIN" --build "$BUILD" --config Release --parallel 8 > "$BUILD/build.log" 2>&1
    fi
fi

# ---- 找可执行文件（兼容 VS 的 Release/ 子目录）----
if [ "$OS" = "Windows_NT" ] || [ "${OS#MINGW}" != "$OS" ] || [ "${OS#MSYS}" != "$OS" ]; then
    BIN="$(find "$BUILD" -name 'CjingApp.exe' -type f | head -1)"
    PLATFORM="windows"
else
    BIN="$(find "$BUILD" -maxdepth 2 -name 'CjingApp' -type f -perm -u+x | head -1)"
    if [ "$OS" = "Darwin" ]; then PLATFORM="macos"; else PLATFORM="linux"; fi
fi
[ -n "${BIN:-}" ] && [ -f "$BIN" ] || { echo "找不到 CjingApp 可执行文件（构建失败？见 $BUILD.build.log）" >&2; exit 1; }
info "可执行文件: $BIN"

# ---- 组装目录：可执行文件 + assets 同级 ----
rm -rf "$STAGE"
mkdir -p "$STAGE/assets"
cp "$BIN" "$STAGE/"
cp -R "$ROOT/assets/." "$STAGE/assets/"

cat > "$STAGE/README.txt" <<EOF
井字棋 Cjing ${VERSION}（${PLATFORM}，静态链接 SDL3）

运行方式：直接双击（macOS 为 Cjing.app）或执行 CjingApp
资源目录 assets/ 必须与可执行文件同级。

本包未做商业代码签名：
- macOS：首次打开需右键 → 打开；或在「系统设置 → 隐私与安全性」中放行
- Windows：SmartScreen 可能提示，选择「仍要运行」
EOF

# ---- 平台打包 ----
case "$PLATFORM" in
    macos)
        APP="$DIST/Cjing.app"
        rm -rf "$APP"
        mkdir -p "$APP/Contents/MacOS"
        cp "$BIN" "$APP/Contents/MacOS/CjingApp"
        cp -R "$ROOT/assets" "$APP/Contents/MacOS/assets"
        cat > "$APP/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>井字棋</string>
    <key>CFBundleDisplayName</key><string>井字棋</string>
    <key>CFBundleIdentifier</key><string>com.timelightspeed.cjing</string>
    <key>CFBundleExecutable</key><string>CjingApp</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>0.2.0</string>
    <key>CFBundleVersion</key><string>0.2.0</string>
    <key>LSMinimumSystemVersion</key><string>12.0</string>
    <key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
PLIST
        echo "APPL????" > "$APP/Contents/PkgInfo"
        # Apple Silicon 上未签名二进制无法运行：用 ad-hoc 签名（不需要开发者证书）
        codesign --force --deep --sign - "$APP" >/dev/null 2>&1 || true

        rm -f "$DIST/Cjing-$VERSION-macos-universal.zip"
        ditto -c -k --sequesterRsrc --keepParent "$APP" "$DIST/Cjing-$VERSION-macos-universal.zip"
        info "已生成 dist/Cjing-$VERSION-macos-universal.zip"

        # DMG 需要挂载卷（沙箱/CI 权限不同）；失败不影响 zip
        if hdiutil create -volname "Cjing" -srcfolder "$APP" -ov -format UDZO \
                "$DIST/Cjing-$VERSION-macos-universal.dmg" >/dev/null 2>&1; then
            info "已生成 dist/Cjing-$VERSION-macos-universal.dmg"
        else
            info "（DMG 生成失败，已跳过；zip 可用）"
        fi
        ;;
    linux)
        tar -C "$STAGE" -czf "$DIST/Cjing-$VERSION-linux-x86_64.tar.gz" .
        info "已生成 dist/Cjing-$VERSION-linux-x86_64.tar.gz"
        ;;
    windows)
        if command -v powershell.exe >/dev/null 2>&1; then
            powershell.exe -NoProfile -Command \
                "Compress-Archive -Path '$STAGE\\*' -DestinationPath '$DIST\\Cjing-$VERSION-windows-x64.zip' -Force"
        else
            (cd "$STAGE" && zip -qry "$DIST/Cjing-$VERSION-windows-x64.zip" .)
        fi
        info "已生成 dist/Cjing-$VERSION-windows-x64.zip"
        ;;
esac
