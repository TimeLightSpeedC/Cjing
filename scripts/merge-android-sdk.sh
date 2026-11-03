#!/usr/bin/env bash
# =============================================================================
#  把工作区里装好的 SDK 组件合并进 Android Studio 的默认 SDK
#
#  背景：本机 sdkmanager 因 Java TLS 握手失败无法直接装包，
#        组件是用 curl 下载官方 zip 后解压到 .tools/android-sdk 的。
#        Android Studio 只认 ~/Library/Android/sdk，因此需要合并一次。
#
#  安全性：全部是 Google 官方 zip，只是复制，不删除任何东西；
#          已存在的组件（platform-tools / emulator）会跳过。
#
#  用法：./scripts/merge-android-sdk.sh
# =============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/.tools/android-sdk"
DST="$HOME/Library/Android/sdk"

[ -d "$SRC" ] || { echo "✗ 源 SDK 不存在: $SRC"; exit 1; }
mkdir -p "$DST"

echo "源: $SRC"
echo "目标: $DST"
echo

# 需要合并的组件（platform-tools 与 emulator 默认 SDK 已有且可用，跳过）
COMPONENTS="
build-tools/35.0.0
platforms/android-35
ndk/28.2.13676358
cmake/3.22.1
system-images/android-35
cmdline-tools/latest
"

for rel in $COMPONENTS; do
    if [ ! -d "$SRC/$rel" ]; then
        echo "  跳过（源不存在）: $rel"
        continue
    fi
    if [ -e "$DST/$rel" ]; then
        echo "  跳过（目标已存在）: $rel"
        continue
    fi
    echo "  复制: $rel"
    mkdir -p "$(dirname "$DST/$rel")"
    # ditto 保留权限位与符号链接（NDK 里有可执行文件）
    ditto "$SRC/$rel" "$DST/$rel"
done

# ---- 让项目指向默认 SDK，消除 IDE 与命令行的分歧 ----
cat > "$ROOT/android/local.properties" <<PROP
# 由 scripts/merge-android-sdk.sh 生成：统一使用 Android Studio 的默认 SDK
sdk.dir=$DST
PROP
echo
echo "✓ android/local.properties 已指向默认 SDK"

# ---- 把手工创建的 AVD 搬到标准位置，Android Studio 才能看到 ----
AVD_SRC="$ROOT/.tools/android-avd"
AVD_DST="$HOME/.android/avd"
if [ -d "$AVD_SRC/cjing-api35.avd" ]; then
    mkdir -p "$AVD_DST"
    if [ -e "$AVD_DST/cjing-api35.avd" ]; then
        echo "  跳过（AVD 已存在）: $AVD_DST/cjing-api35.avd"
    else
        echo "  复制 AVD: cjing-api35"
        ditto "$AVD_SRC/cjing-api35.avd" "$AVD_DST/cjing-api35.avd"
    fi
    cat > "$AVD_DST/cjing-api35.ini" <<INI
avd.ini.encoding=UTF-8
path=$AVD_DST/cjing-api35.avd
path.rel=avd/cjing-api35.avd
target=android-35
INI
    echo "✓ AVD 已放到 $AVD_DST"
fi

echo
echo "=== 校验 ==="
for p in build-tools/35.0.0/aapt2 platforms/android-35/android.jar \
         ndk/28.2.13676358/source.properties cmake/3.22.1/bin/cmake \
         system-images/android-35/google_apis/arm64-v8a/system.img; do
    [ -e "$DST/$p" ] && echo "  ✓ $p" || echo "  ✗ $p（缺失）"
done

echo
echo "=== sdkmanager 识别情况 ==="
"$DST/cmdline-tools/latest/bin/sdkmanager" --list_installed 2>/dev/null \
  | grep -E "build-tools;35|platforms;android-35|system-images|ndk;28|cmake;3.22" \
  | sed 's/^/  /' || true

echo
echo "完成。下一步："
echo "  1. 回到 Android Studio，提示冲突时选 [1] Use Android Studio's default SDK"
echo "  2. 或者直接重新打开工程（local.properties 已指向默认 SDK）"
echo "  3. 确认无误后可删除 .tools/android-sdk 回收约 3.9GB："
echo "       rm -rf $SRC"
