#!/usr/bin/env bash
# =============================================================================
#  安装 Android SDK（不经过 Java，规避 sdkmanager 的 TLS 握手失败）
#
#  背景：部分网络下 `sdkmanager` 会报
#        javax.net.ssl.SSLHandshakeException: Remote host terminated the handshake
#        但同一台机器上 curl 访问 dl.google.com 完全正常。
#        因此这里直接用 curl 下载官方 zip，再解压到 SDK 目录。
#
#  用法：
#        ./scripts/install-android-sdk.sh                 # 只装构建所需（约 800MB）
#        ./scripts/install-android-sdk.sh --with-emulator # 额外装模拟器+系统镜像
#
#  安装位置：${ANDROID_HOME:-$HOME/Library/Android/sdk}
# =============================================================================
set -euo pipefail

# 默认位置：优先 --sdk-dir，其次 $ANDROID_HOME，最后用户目录
SDK="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
WITH_EMULATOR=0
_args=("$@")
for ((i=0; i<${#_args[@]}; i++)); do
  case "${_args[i]}" in
    --with-emulator) WITH_EMULATOR=1 ;;
    --sdk-dir)       SDK="${_args[i+1]}"; i=$((i+1)) ;;
  esac
done
BASE="https://dl.google.com/android/repository"
SYSIMG_BASE="$BASE/sys-img/google_apis"

mkdir -p "$SDK"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

fetch() {
  local name="$1"
  [ -f "$TMP/$name" ] && return 0
  echo "↓ $name"
  curl -fL --retry 3 --retry-delay 2 --progress-bar -o "$TMP/$name" "$BASE/$name"
}

# 解压 zip 中指定顶层目录到 dest（必要时改名）
#
# 注意：**绝不删除已存在的目标目录**。
# 原因：在受限环境（如带沙箱的编辑器）里，对 SDK 目录的删除可能被拒绝，
# 而"创建新目录"是允许的。已存在的组件直接跳过即可。
install_dir() { # <zip 文件名> <zip 内顶层目录> <目标目录>
  local zip="$1" src="$2" dest="$3"
  if [ -e "$dest" ]; then
    echo "  已存在，跳过: $dest"
    return 0
  fi
  mkdir -p "$(dirname "$dest")" "$TMP/x"
  unzip -q "$TMP/$zip" "$src/*" -d "$TMP/x"
  mv "$TMP/x/$src" "$dest"
  rm -rf "$TMP/x"
}

echo "SDK 目录: $SDK"

if [ -x "$SDK/platform-tools/adb" ]; then
  echo "  已存在，跳过: platform-tools"
else
  fetch platform-tools_r37.0.1-darwin.zip
  install_dir platform-tools_r37.0.1-darwin.zip platform-tools "$SDK/platform-tools"
fi

fetch build-tools_r35_macosx.zip
install_dir build-tools_r35_macosx.zip android-15 "$SDK/build-tools/35.0.0"

fetch platform-35_r02.zip
install_dir platform-35_r02.zip android-35 "$SDK/platforms/android-35"

fetch android-ndk-r28c-darwin.zip
install_dir android-ndk-r28c-darwin.zip android-ndk-r28c "$SDK/ndk/28.2.13676358"

fetch commandlinetools-mac-11076708_latest.zip
install_dir commandlinetools-mac-11076708_latest.zip cmdline-tools "$SDK/cmdline-tools/latest"

if [ "$WITH_EMULATOR" = "1" ]; then
  fetch emulator-darwin_aarch64-16740729.zip
  install_dir emulator-darwin_aarch64-16740729.zip emulator "$SDK/emulator"

  # 系统镜像在 sys-img/google_apis/ 子路径下，不能用 repository/ 根路径
  if [ ! -f "$TMP/arm64-v8a-35_r09.zip" ]; then
    echo "↓ arm64-v8a-35_r09.zip (sys-img)"
    curl -fL --retry 3 --retry-delay 2 --progress-bar \
      -o "$TMP/arm64-v8a-35_r09.zip" "$SYSIMG_BASE/arm64-v8a-35_r09.zip"
  fi
  IMG="$SDK/system-images/android-35/google_apis/arm64-v8a"
  rm -rf "$IMG"; mkdir -p "$IMG"
  unzip -q "$TMP/arm64-v8a-35_r09.zip" -d "$IMG"
fi

echo
echo "安装完成，SDK 内容："
ls "$SDK" | sed 's/^/  /'
echo
echo "adb: $("$SDK/platform-tools/adb" version 2>/dev/null | head -1)"
