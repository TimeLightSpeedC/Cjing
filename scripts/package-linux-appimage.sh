#!/usr/bin/env bash
# =============================================================================
#  scripts/package-linux-appimage.sh —— GNU/Linux AppImage（建议在 WSL 里跑）
#
#  用法（仓库根目录）：
#    ./scripts/package-linux-appimage.sh
#
#  产物：dist/Cjing-<版本>-linux-x86_64.AppImage
#
#  前置（Ubuntu/Debian）：
#    sudo apt-get install -y build-essential cmake ninja-build curl file \
#      libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev \
#      libxfixes-dev libxss-dev libxtst-dev libwayland-dev libdecor-0-dev \
#      libfribidi-dev libasound2-dev libpulse-dev libdbus-1-dev \
#      libibus-1.0-dev libudev-dev libunwind-dev
#
#  说明：
#    · CJING_STATIC=ON：SDL3/SDL3_ttf 静态链接，AppImage 内不需要额外 .so
#    · 首次运行会下载 appimagetool 到 .tools/（约 3 MB）
# =============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(sed -n 's/^ *VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt" | head -1)"
VERSION="${VERSION:-0.2.0}"
DIST="$ROOT/dist"
BUILD="$ROOT/build-appimage"
APPDIR="$BUILD/Cjing.AppDir"

info() { printf '  %s\n' "$*"; }
die()  { printf '  ✗ %s\n' "$*" >&2; exit 1; }

[ "$(uname -s)" = "Linux" ] || die "此脚本需要 Linux（Windows 请用 WSL）"
mkdir -p "$DIST"

# ---- 构建（静态）----
info "[1/4] 配置并构建"
cmake -S "$ROOT" -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCJING_STATIC=ON \
    -DCJING_BUILD_TESTS=OFF \
    -DCJING_BUILD_EXAMPLES=OFF
cmake --build "$BUILD" --parallel 8

BIN="$BUILD/bin/CjingApp"
[ -x "$BIN" ] || die "找不到可执行文件: $BIN"

# ---- AppDir ----
info "[2/4] 组装 AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" \
         "$APPDIR/usr/share/applications" \
         "$APPDIR/usr/share/icons/hicolor/256x256/apps"

cp "$BIN" "$APPDIR/usr/bin/CjingApp"
cp -R "$ROOT/assets" "$APPDIR/usr/bin/assets"
cp "$ROOT/packaging/linux/cjing.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/cjing.png"
cp "$ROOT/packaging/linux/cjing.png" "$APPDIR/cjing.png"

cat > "$APPDIR/cjing.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=井字棋
Comment=经典井字棋（SDL3，跨平台）
Exec=CjingApp
Icon=cjing
Categories=Game;BoardGame;
Terminal=false
EOF
cp "$APPDIR/cjing.desktop" "$APPDIR/usr/share/applications/cjing.desktop"

cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
exec "$HERE/usr/bin/CjingApp" "$@"
EOF
chmod +x "$APPDIR/AppRun"

# ---- appimagetool ----
info "[3/4] 准备 appimagetool"
TOOL="$ROOT/.tools/appimagetool-x86_64.AppImage"
if [ ! -x "$TOOL" ]; then
    curl -fL --retry 3 -o "$TOOL" \
        "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage" \
        || die "appimagetool 下载失败（可手动放到 $TOOL）"
    chmod +x "$TOOL"
fi

# ---- 生成 AppImage ----
info "[4/4] 生成 AppImage"
OUT="$DIST/Cjing-$VERSION-linux-x86_64.AppImage"
# 无 FUSE 的环境（容器/WSL）用解包运行模式
APPIMAGE_EXTRACT_AND_RUN=1 ARCH=x86_64 "$TOOL" --no-appstream "$APPDIR" "$OUT"
chmod +x "$OUT"

info "产物: $OUT（$(du -h "$OUT" | cut -f1)）"
info "运行: ./$(basename "$OUT")"
