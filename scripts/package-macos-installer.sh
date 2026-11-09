#!/usr/bin/env bash
# =============================================================================
#  scripts/package-macos-installer.sh —— macOS 官方安装包（.pkg）
#
#  用法（仓库根目录）：
#    ./scripts/package-macos-installer.sh              # 构建并打包
#    ./scripts/package-macos-installer.sh --no-build   # 复用 dist/Cjing.app
#
#  产物：
#    dist/Cjing-<版本>-macos.pkg     安装到 /Applications（Installer）
#    dist/Cjing-<版本>-macos-universal.dmg   （可选，需能挂载磁盘卷）
#
#  说明：未做 Developer ID 签名与公证，收件人首次运行需在
#        「系统设置 → 隐私与安全性」放行（见 docs/分发说明.md）。
# =============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(sed -n 's/^ *VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt" | head -1)"
VERSION="${VERSION:-0.2.0}"
DIST="$ROOT/dist"

info() { printf '  %s\n' "$*"; }
die()  { printf '  ✗ %s\n' "$*" >&2; exit 1; }

[ "$(uname -s)" = "Darwin" ] || die "此脚本只能在 macOS 上运行"

# 1) 复用桌面打包脚本（Universal .app + zip）
"$ROOT/scripts/package-desktop.sh" "$@"

APP="$DIST/Cjing.app"
[ -d "$APP" ] || die "找不到 $APP"

# 2) 组装 pkg 根：安装到 /Applications
STAGE="$ROOT/build-dist/pkgroot"
rm -rf "$STAGE"
mkdir -p "$STAGE/Applications"
cp -R "$APP" "$STAGE/Applications/"

PKG="$DIST/Cjing-$VERSION-macos.pkg"
info "pkgbuild → $PKG"
pkgbuild \
    --root "$STAGE" \
    --identifier "com.timelightspeed.cjing" \
    --version "$VERSION" \
    --install-location "/" \
    "$PKG"

# 3) DMG（可选；在受限环境里挂载卷会失败，失败不影响 pkg）
DMG="$DIST/Cjing-$VERSION-macos-universal.dmg"
if hdiutil create -volname "Cjing" -srcfolder "$APP" -ov -format UDZO "$DMG" >/dev/null 2>&1; then
    info "dmg  → $DMG"
else
    info "（DMG 生成失败，已跳过；可在普通终端重试）"
fi

info "产物:"
ls -la "$DIST"/Cjing-"$VERSION"-macos* | sed 's/^/    /'
