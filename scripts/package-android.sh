#!/usr/bin/env bash
# =============================================================================
#  scripts/package-android.sh —— Android release APK（可分发）
#
#  用法（仓库根目录）：
#    ./scripts/package-android.sh                       # 用 debug keystore 签名
#    CJING_ANDROID_KEYSTORE=~/my.jks \
#    CJING_ANDROID_KS_PASS=**** CJING_ANDROID_KEY_ALIAS=key \
#    CJING_ANDROID_KEY_PASS=**** ./scripts/package-android.sh   # 正式签名
#
#  产物：dist/Cjing-<版本>-android.apk
#
#  前置：
#    · Android SDK（含 build-tools）—— 可用 ANDROID_HOME 指定
#    · JDK —— 默认用 Android Studio 自带 JBR，也可 JAVA_HOME
# =============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(sed -n 's/^ *VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt" | head -1)"
VERSION="${VERSION:-0.2.0}"
DIST="$ROOT/dist"
mkdir -p "$DIST"

info() { printf '  %s\n' "$*"; }
die()  { printf '  ✗ %s\n' "$*" >&2; exit 1; }

# ---- JDK ----
if [ -z "${JAVA_HOME:-}" ]; then
    for j in "/Applications/Android Studio.app/Contents/jbr/Contents/Home" \
             "/Applications/DevEco-Studio.app/Contents/jbr/Contents/Home"; do
        [ -x "$j/bin/java" ] && JAVA_HOME="$j" && break
    done
fi
[ -n "${JAVA_HOME:-}" ] || die "找不到 JDK：请设置 JAVA_HOME"
export JAVA_HOME
export PATH="$JAVA_HOME/bin:$PATH"
info "JDK: $JAVA_HOME"

# ---- Android SDK ----
ANDROID_HOME="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
if [ -z "$ANDROID_HOME" ] && [ -f "$ROOT/android/local.properties" ]; then
    ANDROID_HOME="$(sed -n 's/^sdk.dir=//p' "$ROOT/android/local.properties" | head -1)"
fi
[ -n "$ANDROID_HOME" ] && [ -d "$ANDROID_HOME" ] || die "找不到 Android SDK：请设置 ANDROID_HOME"
export ANDROID_HOME
info "SDK: $ANDROID_HOME"

BUILD_TOOLS="$(ls -d "$ANDROID_HOME"/build-tools/* 2>/dev/null | sort -V | tail -1)"
[ -n "$BUILD_TOOLS" ] || die "找不到 build-tools"
info "build-tools: $(basename "$BUILD_TOOLS")"

# ---- 构建 ----
info "gradle assembleRelease"
(
    cd "$ROOT/android"
    ./gradlew assembleRelease \
        -x lintVitalRelease -x lintVitalAnalyzeRelease \
        --console=plain
)

UNSIGNED="$ROOT/android/app/build/outputs/apk/release/app-release-unsigned.apk"
[ -f "$UNSIGNED" ] || die "构建完成但找不到 $UNSIGNED"

# ---- 签名 ----
KS="${CJING_ANDROID_KEYSTORE:-$HOME/.android/debug.keystore}"
KS_PASS="${CJING_ANDROID_KS_PASS:-android}"
KEY_ALIAS="${CJING_ANDROID_KEY_ALIAS:-androiddebugkey}"
KEY_PASS="${CJING_ANDROID_KEY_PASS:-android}"
[ -f "$KS" ] || die "找不到 keystore: $KS"

ALIGNED="$ROOT/android/app/build/outputs/apk/release/app-release-aligned.apk"
OUT="$DIST/Cjing-$VERSION-android.apk"

"$BUILD_TOOLS/zipalign" -p -f 4 "$UNSIGNED" "$ALIGNED"
"$BUILD_TOOLS/apksigner" sign \
    --ks "$KS" --ks-key-alias "$KEY_ALIAS" \
    --ks-pass "pass:$KS_PASS" --key-pass "pass:$KEY_PASS" \
    --v4-signing-enabled false \
    --out "$OUT" "$ALIGNED"
"$BUILD_TOOLS/apksigner" verify --print-certs "$OUT" | head -2

info "产物: ${OUT}（$(du -h "$OUT" | cut -f1)）"
info "签名证书: $(basename "$KS")（${KEY_ALIAS}）"
