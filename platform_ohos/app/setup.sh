#!/bin/sh
# =============================================================================
#  鸿蒙正式工程的一键准备（在仓库根目录执行：./platform_ohos/app/setup.sh）
#
#  做三件事：
#    1. 把 SDL3 / SDL3_ttf 源码链接到 cpp/（不复制，避免几百 MB 进仓库）
#    2. 把 assets/（字体 + 音效）复制到 resources/rawfile/
#       —— 鸿蒙运行时 SDL_GetBasePath() 返回 "assets://"，映射到 rawfile/
#    3. 校验
# =============================================================================
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"          # 仓库根
APP="$ROOT/platform_ohos/app"
CPP="$APP/entry/src/main/cpp"
RAWFILE="$APP/entry/src/main/resources/rawfile"

echo "[1/3] 链接 SDL3 / SDL3_ttf 源码"
ln -sfn "$ROOT/.tools/sdl3-src"   "$CPP/SDL"
ln -sfn "$ROOT/.tools/sdltff-src" "$CPP/SDL_ttf"

echo "[2/3] 复制资源到 rawfile/"
mkdir -p "$RAWFILE"
cp -R "$ROOT/assets/." "$RAWFILE/"

echo "[3/3] 校验"
for p in "$CPP/SDL/CMakeLists.txt" "$CPP/SDL_ttf/CMakeLists.txt" \
         "$RAWFILE/fonts/NotoSansSC-Regular.ttf" "$RAWFILE/sounds/place.wav"; do
    [ -e "$p" ] && echo "  ✓ $p" || { echo "  ✗ 缺失: $p"; exit 1; }
done
echo "准备完成。下一步：用 DevEco Studio 打开 platform_ohos/app，或命令行构建。"
