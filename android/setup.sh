#!/bin/sh
# =============================================================================
#  Android 工程的一键准备（在仓库根或任意位置执行均可）
#
#  做三件事：
#    1. 把 SDL3 / SDL3_ttf 源码链接到 app/jni（不复制，约 280MB）
#    2. 把游戏资源复制到 app/src/main/assets（APK 会打包，运行时经 assets:// 访问）
#    3. 校验路径
# =============================================================================
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"          # 仓库根
JNI="$ROOT/android/app/jni"
ASSETS="$ROOT/android/app/src/main/assets"

echo "[1/3] 链接 SDL3 / SDL3_ttf 源码"
ln -sfn "$ROOT/.tools/sdl3-src"   "$JNI/SDL"
ln -sfn "$ROOT/.tools/sdltff-src" "$JNI/SDL_ttf"

echo "[2/3] 复制资源到 app/src/main/assets"
mkdir -p "$ASSETS"
cp -R "$ROOT/assets/." "$ASSETS/"

echo "[3/3] 校验"
for p in "$JNI/SDL/CMakeLists.txt" "$JNI/SDL_ttf/CMakeLists.txt" \
         "$ASSETS/fonts/NotoSansSC-Regular.ttf" "$ASSETS/sounds/place.wav"; do
    [ -e "$p" ] && echo "  ✓ $p" || { echo "  ✗ 缺失: $p"; exit 1; }
done

echo
echo "准备完成。构建方式（需 Android SDK + NDK 28.2.13676358）："
echo "  cd android && ./gradlew assembleDebug"
echo "产物: android/app/build/outputs/apk/debug/app-debug.apk"
