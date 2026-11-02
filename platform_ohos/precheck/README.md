# 鸿蒙 Go/No-Go 预检（P2-T23）

这个工程的目的**只有一个**：回答"SDL3 能不能在鸿蒙上显示窗口并渲染中文"。

> 判据（来自路线图）：**Go = 空窗口可显示 且 字体渲染成功**。
> 音频是可选降级项，不作为判据。

工程基于 SDL 官方 `openharmony-project` 模板（`SDL3 源码根目录/openharmony-project`），
只做了三处改动：

| 文件 | 改动 |
|---|---|
| `AppScope/app.json5` | 包名改为 `com.timelightspeed.cjing.precheck` |
| `entry/src/main/cpp/CMakeLists.txt` | 编译 `libmain.so`，同时编译 SDL3 与 SDL3_ttf |
| `entry/src/main/cpp/main.c` | 预检程序：窗口 + 中文 + 触摸计数 |

---

## 一次性准备

在**仓库根目录**执行：

```bash
./platform_ohos/precheck/setup.sh
```

它做三件事：

1. 把 `.tools/sdl3-src`、`.tools/sdltff-src` 软链接到工程的 `cpp/` 下（不复制源码）
2. 把 `assets/fonts/NotoSansSC-Regular.ttf` 复制到 `rawfile/fonts/`
3. 校验三条路径都存在

> 为什么用软链接：SDL3 + SDL3_ttf + HarfBuzz + FreeType 合计约 280MB，
> 复制进工程既浪费又容易与主仓版本不一致。`setup.sh` 每次自动建立。
>
> 这两个链接和字体副本都**不入库**（见 `.gitignore`）。

---

## 在 DevEco Studio 里构建与运行

### 1. 打开工程

DevEco Studio → `File → Open` → 选择 `platform_ohos/precheck`

首次打开会提示同步（hvigor sync），让它跑完。

### 2. 配置签名（需要华为账号）

`File → Project Structure → Signing Configs`

- 勾选 **Automatically generate signature**
- 若未登录，会弹出 Huawei ID 登录框 —— 这就是"何时登录"的答案：
  **只在需要签名安装时才登录**；纯编译不需要。

### 3. 选择运行目标

工具栏设备下拉框：

- **模拟器**：`Tools → Device Manager` 里创建一个 Phone 模拟器（API 24）
- **真机**：手机开启开发者模式 + USB 调试，用 `hdc list targets` 确认识别

### 4. 运行

点绿色三角。首次会自动：

1. hvigor 调用鸿蒙自带 CMake 编译 `libmain.so` + `libSDL3.so` + `libSDL3_ttf.so`
2. 打包 hap（含 `rawfile/fonts/` 里的字体）
3. 安装并启动

---

## 成功/失败怎么判断

### 成功（Go）

屏幕显示：

- 深色背景 + 左上角**暖黄色方块**
- 中间一行中文：**「鸿蒙预检 · 中文渲染正常」**
- 点击屏幕 → 背景颜色变化（触摸事件到达原生层）

日志（DevEco 的 Log 窗口，过滤 `precheck`）：

```
[precheck] 字体已加载
[precheck] 初始化完成：窗口=0x... 渲染器=0x... 字体=0x... 文字=0x...
[precheck] 第一帧已呈现
[precheck] 触摸/点击 #1
```

### 失败时怎么定位

| 现象 | 含义 | 下一步 |
|---|---|---|
| 应用启动即退出，日志有 `SDL_Init 失败` | 视频后端没起来 | 记录完整日志 |
| 有窗口、无文字，日志有 `字体打开失败` | 字体路径或 rawfile 打包问题 | 检查 `rawfile/fonts/` 是否有文件 |
| 有窗口、无文字、无报错 | 文字纹理创建失败 | 查 `TTF_RenderText_Blended` 日志 |
| 完全没有日志 | libmain.so 没被加载 | 检查 CMake 是否产出 `libmain.so`（必须是共享库） |

---

## 已知的路径差异（重要）

鸿蒙上 `SDL_GetBasePath()` 返回的是 **`assets://`**，
对应 hap 内 `entry/src/main/resources/rawfile/` 的内容。

而桌面版的 `SDLAssetIO` 是按 `<base>/assets/<name>` 拼路径的。
**两者规则不同**，所以：

- 本预检**直接**用 `assets://fonts/...`，不经过 `SDLAssetIO`
- 正式的鸿蒙移植（P4）需要给 `SDLAssetIO` 增加 `assets://` 分支

---

## 构建产物（本地验证用）

不用 DevEco 也可以只验证原生侧能否编译：

```bash
SDK=/Applications/DevEco-Studio.app/Contents/sdk/default
"$SDK/openharmony/native/build-tools/cmake/bin/cmake" -G Ninja \
  -DCMAKE_MAKE_PROGRAM="$SDK/openharmony/native/build-tools/cmake/bin/ninja" \
  -DCMAKE_TOOLCHAIN_FILE="$SDK/openharmony/native/build/cmake/ohos.toolchain.cmake" \
  -DCMAKE_BUILD_TYPE=Release -DOHOS_COMPATIBLE_SDK_VERSION=6.1.1.125 \
  -S platform_ohos/precheck/entry/src/main/cpp -B .tools/ohos-precheck-build
"$SDK/openharmony/native/build-tools/cmake/bin/cmake" --build .tools/ohos-precheck-build --parallel
```

> 注意：**必须用鸿蒙自带的 CMake 和 Ninja**，系统/CLion 的版本无法识别该平台。

---

## 命令行打包（已实测通过）

不打开 DevEco 也能打出 hap。三个环境变量缺一不可：

```bash
cd platform_ohos/precheck
D=/Applications/DevEco-Studio.app/Contents

export HOME="$PWD/.hvigor-home"        # hvigor 默认写 ~/.hvigor；重定向避免污染
export PATH="$D/tools/node/bin:$PATH"  # hvigorw 需要自带 node
export DEVECO_SDK_HOME="$D/sdk"        # 指向内置 SDK
export JAVA_HOME="$D/jbr/Contents/Home" # 打包 hap 需要 Java，用 DevEco 自带 JBR

"$D/tools/hvigor/bin/hvigorw" --mode module -p product=default assembleHap --no-daemon
```

产物：

```
entry/build/default/outputs/default/entry-default-unsigned.hap   (27 MB)
```

实测包内容（`unzip -l`）：

| 路径 | 说明 |
|---|---|
| `libs/arm64-v8a/{libmain,libSDL3,libSDL3_ttf,libc++_shared}.so` | 真机架构 |
| `libs/x86_64/` 同名文件 | 模拟器架构 |
| `resources/rawfile/fonts/NotoSansSC-Regular.ttf` | 中文字体随包 |

### 两个坑

1. **`Java Runtime` 找不到** → 必须设 `JAVA_HOME` 指向 `$D/jbr/Contents/Home`。
2. **`EPERM: ~/.hvigor`** → hvigor 默认写用户主目录；在受限环境下重定向 `HOME`。

> 未签名 hap 只能证明"能打包"。**安装到设备需要签名**，那一步必须在 DevEco 里完成
> （登录华为账号 → 自动生成签名）。

---

## 实测结论（2026-11-02，API 24 模拟器）

### 已证实 ✅

| 项 | 证据 |
|---|---|
| 原生编译与打包 | hvigor 打包成功，hap 内含双架构 `libmain/libSDL3/libSDL3_ttf` 与中文字体 |
| 应用启动 | 日志 `SDL_AppInit said continue! We are live!` |
| **中文字体渲染** | TTF 生成文字表面 **504×64，抽样非透明 454 点** —— 汉字确实被画出来了 |
| 渲染循环 | 持续 `eglSwapBuffers`，present 返回 true |
| 触摸/窗口尺寸 | window/output 均为 1280×2566 |

### 未通过 ❌

**内容不上屏**：屏幕上只有清屏色 `(20,24,40)`，方块与文字都不可见。

已排除的假设（都做过实验）：

| 假设 | 实验 | 结果 |
|---|---|---|
| 逻辑呈现映射错误 | 关闭 `SDL_SetRenderLogicalPresentation`，直接用像素坐标 | 无效 |
| viewport 为 0 | 显式 `SDL_SetRenderViewport(0,0,w,h)` | 无效（尺寸日志确认 1280×2566 正确） |
| 软件渲染器可用 | `SDL_CreateRenderer("software")` | 画面变白（present 不到 XComponent 表面） |
| GLES2 后端问题 | 改试 `vulkan` | 创建成功，但仍只有清屏色 |
| 读取时机问题 | `SDL_RenderReadPixels` 读回中心像素 | GLES2 读到清屏色、Vulkan 读到黑色 |

### 最可能的原因

SDL3 官方文档写明：**支持 HarmonyOS 5.0.4 / API 16+，且仅在 Mate 80 上实测过**。
本机 SDK 是 **6.1.1 / API 24**，而官方模板 `build-profile.json5` 的默认目标正是 **`5.0.4(16)`** ——
我们为了能编译改成了 6.1.1(24)。这很可能就是渲染异常的原因。

### 下一步（建议）

1. 在 DevEco 的 SDK Manager 中安装 **HarmonyOS 5.0.4(16)** SDK
2. 把 `build-profile.json5` 的 `targetSdkVersion` / `compatibleSdkVersion` 改回 `"5.0.4(16)"`
3. 重新运行；若仍不显示，改用真机（官方唯一实测过的环境）

> 调试期发现的可复用闭环：`hvigorw assembleHap` + `hdc install`（**未签名包模拟器可直接安装**）
> + `snapshot_display` 截图，全程不需要 DevEco GUI。脚本思路见本文"命令行打包"。
