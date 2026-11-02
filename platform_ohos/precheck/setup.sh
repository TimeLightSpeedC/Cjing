#!/bin/sh
# =============================================================================
#  鸿蒙预检工程的一键准备（在仓库根目录执行）
#
#  做三件事：
#    1. 把 SDL3 / SDL3_ttf 源码链接到工程的 cpp 目录（不复制，避免几百 MB）
#    2. 把中文字体复制到 rawfile/（会随 hap 打包，运行时经 assets:// 访问）
#    3. 校验路径存在
# =============================================================================
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"      # 仓库根
CJ="$ROOT/platform_ohos/precheck/entry/src/main/cpp"

echo "[1/3] 链接 SDL3 / SDL3_ttf 源码"
ln -sfn "$ROOT/.tools/sdl3-src"   "$CJ/SDL"
ln -sfn "$ROOT/.tools/sdltff-src" "$CJ/SDL_ttf"

echo "[2/3] 复制中文字体到 rawfile/"
mkdir -p "$ROOT/platform_ohos/precheck/entry/src/main/resources/rawfile/fonts"
cp -f "$ROOT/assets/fonts/NotoSansSC-Regular.ttf" \
      "$ROOT/platform_ohos/precheck/entry/src/main/resources/rawfile/fonts/"

echo "[3/3] 校验"
for p in "$CJ/SDL/CMakeLists.txt" "$CJ/SDL_ttf/CMakeLists.txt" \
         "$ROOT/platform_ohos/precheck/entry/src/main/resources/rawfile/fonts/NotoSansSC-Regular.ttf"; do
    [ -e "$p" ] && echo "  ✓ $p" || { echo "  ✗ 缺失: $p"; exit 1; }
done
echo "准备完成。下一步：用 DevEco Studio 打开 platform_ohos/precheck"
