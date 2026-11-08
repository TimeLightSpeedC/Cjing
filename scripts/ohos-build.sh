#!/usr/bin/env bash
# =============================================================================
#  scripts/ohos-build.sh —— 鸿蒙 HAP 一键构建（路线图 P4-T17）
#
#  把 P4 手工做过的步骤固化下来：
#    setup.sh（源码软链/资源/音频补丁）→ hvigorw assembleHap → 产物定位
#    可选：安装到设备、启动、看日志
#
#  用法：
#    ./scripts/ohos-build.sh                    # debug 构建
#    ./scripts/ohos-build.sh --release          # release 构建
#    ./scripts/ohos-build.sh --install          # 构建后安装到已连接设备
#    ./scripts/ohos-build.sh --install --run    # 安装并启动
#    ./scripts/ohos-build.sh --install --log    # 安装后跟踪 hilog
#    ./scripts/ohos-build.sh --device <id>      # 指定 hdc 目标（多设备时）
#    ./scripts/ohos-build.sh --clean            # 先清构建目录（含 .cxx）
#    ./scripts/ohos-build.sh --no-setup         # 跳过 setup.sh
#    ./scripts/ohos-build.sh --signing <file>   # 临时使用本地签名配置（构建后自动还原）
#    ./scripts/ohos-build.sh --project precheck --release   # 构建预检工程的 release 包
#
#  环境变量：
#    DEVECO_HOME           DevEco Studio 路径（默认 /Applications/DevEco-Studio.app）
#    CJING_OHOS_FAKEHOME   设置后把 HOME 指到该目录（沙箱/CI 环境用）
#
#  签名说明：
#    真机安装需要签名。开启方式见 platform_ohos/app/README.md「签名配置的坑」：
#      1) DevEco 生成材料（写到 ~/.ohos/config/）
#      2) build-profile.json5 的 products[0] 加 "signingConfig": "default"
#    未开启时产物是 unsigned HAP，仅模拟器可装。
# =============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEVECO_HOME="${DEVECO_HOME:-/Applications/DevEco-Studio.app}"

MODE="debug"
DO_SETUP=1
DO_CLEAN=0
DO_INSTALL=0
DO_RUN=0
DO_LOG=0
TARGET=""
SIGNING_FILE=""
PROJECT="app"

info()  { printf '  %s\n' "$*"; }
warn()  { printf '  ⚠ %s\n' "$*" >&2; }
die()   { printf '  ✗ %s\n' "$*" >&2; exit 1; }

usage() {
    cat <<'EOF'
用法: ./scripts/ohos-build.sh [选项]

  --release        release 构建（默认 debug）
  --install        构建后安装到已连接设备
  --run            安装并启动（隐含 --install）
  --log            启动后跟踪 hilog（Ctrl+C 退出）
  --device <id>    指定 hdc 目标（多设备时）
  --clean          先清理 entry/build、entry/.cxx、build
  --no-setup       跳过 setup.sh
  -h, --help       显示本帮助

环境变量:
  DEVECO_HOME          DevEco Studio 路径（默认 /Applications/DevEco-Studio.app）
  CJING_OHOS_FAKEHOME  把 HOME 指到该目录（沙箱/CI 用）

签名:
  真机安装需要签名，见 platform_ohos/app/README.md「签名配置的坑」。
  未配置时产物为 unsigned HAP，仅模拟器可装。
EOF
    exit 0
}

# ---- 参数 ----
while [ $# -gt 0 ]; do
    case "$1" in
        --release)   MODE="release" ;;
        --debug)     MODE="debug" ;;
        --install)   DO_INSTALL=1 ;;
        --run)       DO_RUN=1; DO_INSTALL=1 ;;
        --log)       DO_LOG=1 ;;
        --clean)     DO_CLEAN=1 ;;
        --no-setup)  DO_SETUP=0 ;;
        --device)    shift; TARGET="${1:-}"; [ -n "$TARGET" ] || die "--device 需要一个目标 id" ;;
        --signing)   shift; SIGNING_FILE="${1:-}"; [ -n "$SIGNING_FILE" ] || die "--signing 需要一个文件路径" ;;
        --project)   shift; PROJECT="${1:-}"; [ -n "$PROJECT" ] || die "--project 需要 app 或 precheck" ;;
        -h|--help)   usage ;;
        *)           die "未知参数: $1（--help 查看用法）" ;;
    esac
    shift
done

APP_DIR="$ROOT/platform_ohos/${PROJECT}"

case "$PROJECT" in app|precheck) ;; *) die "--project 只支持 app 或 precheck" ;; esac
[ -d "$APP_DIR" ] || die "找不到鸿蒙工程目录: $APP_DIR"

# ---- 工具链 ----
# 官方要求必须用 DevEco 自带的 hvigor（不要用系统装的其他版本）
HVIGOR="$DEVECO_HOME/Contents/tools/hvigor/bin/hvigorw"
JBR="$DEVECO_HOME/Contents/jbr/Contents/Home"
SDK="$DEVECO_HOME/Contents/sdk"
NODE_BIN="$DEVECO_HOME/Contents/tools/node/bin"
HDC="$SDK/default/openharmony/toolchains/hdc"

[ -d "$DEVECO_HOME" ]   || die "找不到 DevEco Studio: $DEVECO_HOME（可设 DEVECO_HOME 覆盖）"
[ -x "$HVIGOR" ]        || die "找不到 hvigorw: $HVIGOR"
[ -d "$JBR" ]           || die "找不到 JDK: $JBR"
[ -d "$SDK" ]           || die "找不到 SDK: $SDK"

export JAVA_HOME="$JBR"
export DEVECO_SDK_HOME="$SDK"
export PATH="$NODE_BIN:$PATH"

# 沙箱/CI：把 HOME 指到工作区内，避免写 ~/.hvigor、~/.ohpm 被拒
if [ -n "${CJING_OHOS_FAKEHOME:-}" ]; then
    mkdir -p "$CJING_OHOS_FAKEHOME"
    export HOME="$CJING_OHOS_FAKEHOME"
    info "HOME 重定向: $HOME"
fi

info "DevEco : $DEVECO_HOME"
info "模式   : $MODE"

# ---- 临时签名配置（--signing：用本地文件替换，构建后自动还原）----
PROFILE="$APP_DIR/build-profile.json5"
if [ -n "$SIGNING_FILE" ]; then
    [ -f "$SIGNING_FILE" ] || die "找不到签名配置: $SIGNING_FILE"
    cp "$PROFILE" "$PROFILE.cjng-backup"
    # shellcheck disable=SC2064
    trap 'mv -f "$PROFILE.cjng-backup" "$PROFILE"' EXIT
    cp "$SIGNING_FILE" "$PROFILE"
    info "临时使用签名配置: $SIGNING_FILE"
fi

# ---- 签名状态预检 ----
SIGNED_CONFIG=0
if grep -qE '^[[:space:]]+"signingConfig"[[:space:]]*:[[:space:]]*"default"' \
        "$APP_DIR/build-profile.json5" 2>/dev/null; then
    SIGNED_CONFIG=1
fi
if [ "$SIGNED_CONFIG" -eq 0 ]; then
    warn "build-profile.json5 未引用签名配置 → 产物将是 unsigned HAP（仅模拟器可装）"
fi

# ---- 清理 ----
if [ "$DO_CLEAN" -eq 1 ]; then
    info "[清理] entry/build、entry/.cxx、build"
    rm -rf "$APP_DIR/entry/build" "$APP_DIR/entry/.cxx" "$APP_DIR/build"
fi

# ---- 准备（软链 / rawfile / 音频补丁）----
if [ "$DO_SETUP" -eq 1 ]; then
    info "[1/3] setup.sh"
    "$APP_DIR/setup.sh" >/dev/null
else
    info "[1/3] 跳过 setup.sh"
fi

# ---- 构建 ----
info "[2/3] hvigorw assembleHap"
START=$(date +%s)
(
    cd "$APP_DIR"
    "$HVIGOR" assembleHap \
        --mode module \
        -p product=default \
        -p "buildMode=$MODE" \
        --no-daemon
) || die "构建失败（完整日志见上方输出）"
ELAPSED=$(( $(date +%s) - START ))

# ---- 产物定位 ----
OUT_DIR="$APP_DIR/entry/build/default/outputs/default"
if [ "$SIGNED_CONFIG" -eq 1 ]; then
    HAP="$OUT_DIR/entry-default-signed.hap"
    KIND="signed（真机可装）"
else
    HAP="$OUT_DIR/entry-default-unsigned.hap"
    KIND="unsigned（仅模拟器）"
fi
[ -f "$HAP" ] || die "构建完成但找不到产物: $HAP"

SIZE=$(du -h "$HAP" | cut -f1 | tr -d ' ')
info "[3/3] 产物: ${HAP}（${SIZE}，${KIND}）"
info "构建耗时: ${ELAPSED}s"

# ---- 安装 / 启动 / 日志 ----
if [ "$DO_INSTALL" -eq 1 ]; then
    [ -x "$HDC" ] || die "找不到 hdc: $HDC"
    HDC_TARGET=()
    [ -n "$TARGET" ] && HDC_TARGET=(-t "$TARGET")

    info "安装到设备${TARGET:+ $TARGET}"
    "$HDC" "${HDC_TARGET[@]}" install -r "$HAP" || die "安装失败（真机需签名；设备需解锁并允许调试）"
fi

BUNDLE=$(sed -n 's/.*"bundleName"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' \
         "$APP_DIR/AppScope/app.json5" | head -1)
[ -n "$BUNDLE" ] || die "无法从 AppScope/app.json5 读取 bundleName"

if [ "$DO_RUN" -eq 1 ]; then
    info "启动 $BUNDLE"
    "$HDC" "${HDC_TARGET[@]}" shell aa start -a EntryAbility -b "$BUNDLE"
fi

if [ "$DO_LOG" -eq 1 ]; then
    info "hilog（Ctrl+C 退出）"
    "$HDC" "${HDC_TARGET[@]}" shell hilog -x | grep -E '\[Cjing\]|SDL' || true
fi
