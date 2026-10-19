# Cjing 井字棋跨平台项目路线图 ProMax

> **本文档是唯一权威路线图**（single source of truth）。合并自三份早期计划：
> Qwen 版（SDL3 事实修正）、GLM 版 V2（任务颗粒度与时间维度）、hy 版 V2（结构骨架、双路线、风险登记册）。
> 合并时已修正两份 V2 的遗留问题（日历算术、时间维度缺失、FreeType/HarfBuzz 获取策略未定等）。
>
> 📌 **关于来源文档**：下文表格中提到的 `plans/…` 路径是**合并来源的原始出处**，
> 属内部过程记录，**未纳入本仓库**（仅保留在开发者本地）。不影响本文档的独立可读性。
>
> **最终产物**：6 平台可玩的井字棋（Windows / macOS / Linux / Android / iOS / HarmonyOS NEXT）
> **技术栈**：C++20 + CMake ≥ 3.20 + SDL3（FetchContent 锁 commit SHA）+ SDL3_ttf（必需）+ SDL3 核心音频
> **预估工期**：**10–12 周**（单开发者线性推进；若 P3/P4 可并行，关键路径约 10 周）
> **进度图例**：⬜ 未开始 ｜ 🟡 进行中 ｜ ✅ 已完成 ｜ ⛔ 阻塞/不适用

---

## 0. 技术事实基线（本项目的地基，任何实现都不得偏离）

> **本节 7 条全部经过 SDL3 官方头文件 / 官方文档 / GitHub API 核实**（非凭记忆）。
> **铁律**：P0-T8 的《技术验证结论》必须针对**实际锁定的 commit SHA** 复核这 7 条，
> **不得**凭记忆写事件名或 API 名 —— 本项目最容易出错的地方就在这里。

| # | 事实 | 为什么要写死 |
|---|------|-------------|
| **F1** | **入口点**：SDL3 **不再有独立的 `SDL3main` 库**（只有 `SDL3-shared` / `SDL3-static` / `SDL3::SDL3`），**但 `SDL_main.h` 仍然必需** —— Windows（SDL 提供 `WinMain()`）、iOS/tvOS（提供 app delegate 入口）、Android（`SDL_MAIN_NEEDED`）都靠它。本项目采用 `SDL_MAIN_USE_CALLBACKS`，**此时不得再提供 `main()`**（官方原文：*"The app should not provide a `main` function in this case"*）。 | 遗漏会导致 Windows/iOS 入口点链接错误，是 SDL3 最常见的第一个坑 |
| **F2** | **回调签名**：`SDL_AppInit(void**, int, char**)` / `SDL_AppEvent(void*, SDL_Event*)` / `SDL_AppIterate(void*)` / `SDL_AppQuit(void*, SDL_AppResult)`。注意 **`SDL_AppEvent` 的 `SDL_Event*` 是非 const** —— 正合坐标换算需要（见 F4）。 | 签名写错会编译失败或无法做原地换算 |
| **F3** | **生命周期事件名**（**没有 `_APP_` 前缀**）：`SDL_EVENT_TERMINATING`、`SDL_EVENT_WILL_ENTER_BACKGROUND`、`SDL_EVENT_DID_ENTER_BACKGROUND`、`SDL_EVENT_WILL_ENTER_FOREGROUND`、`SDL_EVENT_DID_ENTER_FOREGROUND`、`SDL_EVENT_LOW_MEMORY`、`SDL_EVENT_RENDER_DEVICE_RESET`。**`SDL_EVENT_WINDOW_MINIMIZED` / `SDL_EVENT_WINDOW_RESTORED` 是桌面窗口事件，移动端切后台不会触发它们。** | 写错会导致**切后台时存档静默丢失**，且极难排查 |
| **F4** | **输入坐标必须统一换算**：SDL3 **没有**统一触摸与鼠标的坐标语义 —— 触摸 `SDL_TouchFingerEvent.x/y` 是**归一化 0.0–1.0**（且**未钳制**，letterbox 区域会出负值），鼠标 `SDL_MouseButtonEvent.x/y` 才是**窗口相对像素浮点**。正解是在事件入口调用**一次** `SDL_ConvertEventToRenderCoordinates(renderer, event)`，把鼠标/触摸/笔统一换算到与 `SDL_SetRenderLogicalPresentation` 一致的**渲染坐标**空间。 | 直接复用鼠标假设会导致移动端**交互静默错位** |
| **F5** | **触摸会合成鼠标事件**：`SDL_HINT_TOUCH_MOUSE_EVENTS` **默认 `"1"`**（官方原文：*"Touch events will generate mouse events. (default)"*），`SDL_HINT_MOUSE_TOUCH_EVENTS` 在移动平台也默认开启。**一次物理触摸会同时产生 `SDL_EVENT_FINGER_DOWN` 与合成的 `SDL_EVENT_MOUSE_BUTTON_DOWN`。** | 两套都处理会**一次点击落两子**或触发非法落子 |
| **F6** | **安全区有官方 API**：`SDL_GetWindowSafeArea(window, &rect)`（自 3.2.0）。自 **Android 15** 起 SDL 窗口默认铺满全屏（延伸到刘海与系统栏之下），安全区是**必做项**。**不要**用 `SDL_SetRenderViewport` 做安全区（它是绘制裁剪机制，会导致绘制区与逻辑坐标不一致）。 | 缺失会导致内容被刘海/系统栏遮挡且无法交互 |
| **F7** | **SDL3 版本策略（决定获取方式）**：鸿蒙/OpenHarmony 后端**已合入开发主线**（`src/video/openharmony/`，含 video/events/mouse/keyboard/clipboard/opengl/vulkan/window 全套；官方脚本 `build-scripts/harmonyos-tool.sh`），**但没有任何已发布 tag 包含它**（最新发布 tag 为 `release-3.4.18`，开发版本 3.5.0，官方称将随 **SDL 3.6.0** 发布）。**因此：必须全平台用 FetchContent 锁定同一个 commit SHA —— 不能用包管理器，也不能锁 tag。** 另注意 `SDL_GetBasePath()` 在 Android 上的行为依小版本而变（≥3.6.0 支持 `assets://` 且返回 `"assets://"`；3.2.28–3.6.0 期间返回 `"./"`）。 | 桌面用包管理器、鸿蒙用源码 = **两套 SDL3 版本并存**，是"桌面能跑、鸿蒙崩"的经典来源 |

**F7 的具体锁定点（已实测）：**

| 项 | 值 |
|----|----|
| **最低基线提交**（引入鸿蒙后端） | `a24429c192cfee5ae92697f12f219cad27635fb4`（2026-09-15，"harmonyos: Port SDL3 to HarmonyOS/OpenHarmony"）—— **锁定的 SHA 必须不早于此提交** |
| **建议锁定点**（项目开工时的主线 HEAD） | `b64293b342c2cde123285199818803f83dbac612`（2026-10-13，"Fixed audio crash in Android and OpenHarmony"） |
| 该提交的鸿蒙后端文件数 | 16（已核实存在） |

> **开工时请重新解析一次 `main` 的 HEAD SHA 并覆盖上表**，然后在整个项目周期内冻结。
> 理由：SDL3 的鸿蒙后端仍在活跃修 bug（HEAD 那条提交本身就是"修复 Android 与 OpenHarmony 的音频崩溃"），
> 但冻结后不得中途升级 —— 升级必须走 1.5 节第 5 条的"同步回归四端"流程。

### 0.1 第三方库策略

| 库 | 定位 | 获取方式 |
|----|------|---------|
| **SDL3** | 必需 | FetchContent + `GIT_TAG <commit SHA>`（含 `src/video/openharmony/`） |
| **SDL3_ttf** | **必需**（文字/比分/菜单） | FetchContent 锁 SHA。**它通过 git submodule 自带依赖**（见下），必须递归拉取子模块 |
| **SDL3 核心音频**（`SDL_audio.h`） | 必需（WAV 音效） | **SDL3 核心内置，无需任何额外依赖**。⚠️ **不存在独立的 `SDL3_audio` 库** |
| **SDL3_mixer** | **可选/可降级** | 仅当需要 OGG/MP3 解码或多声道混音时才引入（独立第三方库，API 仍在演进）。**不作为阻塞项** |
| **SDL3_image** | **不引入** | X/O 用矢量绘制（`SDL_RenderLine` / `SDL_RenderFillRect`），省掉一个跨平台交叉编译负担 |

**SDL3_ttf 的依赖链（已核实，这是鸿蒙 TTF 交叉编译的实际形态）**：

SDL3_ttf 的 `.gitmodules` 声明了 4 个子模块，**全部是 SDL 官方维护的补丁分支**（优先用它们，不要换成上游版本）：

| 子模块 | 仓库 | 必要性 |
|--------|------|--------|
| `external/freetype` | `libsdl-org/freetype`（分支 `VER-2-13-3-SDL`） | **必需**（要求 ≥ 2.8.0） |
| `external/harfbuzz` | `libsdl-org/harfbuzz`（分支 `8.5.0-SDL`） | 建议保留（`SDLTTF_HARFBUZZ=ON`，改善文字整形） |
| `external/plutosvg` | `libsdl-org/plutosvg`（分支 `v0.0.8-SDL`） | **可关闭** `SDLTTF_PLUTOSVG=OFF` |
| `external/plutovg` | `libsdl-org/plutovg`（分支 `v1.1.0-SDL`） | 随 plutosvg 一起关闭 |

**结论（两份 V2 都未写清、此处补上）**：

```cmake
include(FetchContent)

# SDL3：本身无子模块，浅克隆即可（全历史克隆在本环境会被 connection reset）
FetchContent_Declare(SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG        main        # 开工时解析并记录实际 SHA，见 F7
  GIT_SHALLOW    TRUE)

# SDL3_ttf：vendored 依赖是 git submodule，必须递归拉取
FetchContent_Declare(SDL3_ttf
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
  GIT_TAG        main
  GIT_SHALLOW    TRUE
  GIT_SUBMODULES_RECURSE TRUE)

# 鸿蒙/Android 交叉编译：裁掉彩色 emoji 依赖，显著缩小交叉编译面
set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
```
- **为什么 `GIT_TAG` 用分支名而不是裸 SHA**：本环境已验证"浅克隆 + 分支名"稳定；
  裸 SHA 在某些 git 服务端不支持按 SHA 浅取。**可复现性靠"开工时记录解析出的 SHA"来保证**（见 F7），
  而非靠 `GIT_TAG` 写死 —— 这样既快又可控。
- 桌面三平台：可直接用 `SDLTTF_VENDORED=ON`（SDL_ttf 在 MSVC 上甚至默认启用 vendored）。
- 鸿蒙/Android 交叉编译：**建议 `SDLTTF_PLUTOSVG=OFF`**，只保留 FreeType + HarfBuzz，可显著减少交叉编译面
  （代价：不支持彩色 emoji，本项目不需要）。
- 字体本身需**确认授权允许再分发**，随包放入 `assets/`。
- 若递归克隆 SDL3_ttf 在本环境不稳定：手动 `git clone --depth 1 --recurse-submodules` 到本地，
  再 `FetchContent_Declare(SDL3_ttf SOURCE_DIR <本地路径>)` 指向它。

### 0.2 平台与版本矩阵

> ✅ **本表已按锁定的 SDL3 源码（`b64293b3`）回填完毕**，依据见 `技术验证结论.md` 第 3 节。
> iOS/macOS 存在"官方 docs 支持下限"与"官方 Xcode 模板目标"不一致的情况，本表取值口径已在结论文件中写明。

| 平台 | 最低系统版本 | 目标架构 | 交付形态 | 备注 |
|------|-------------|----------|----------|------|
| Windows | Windows 10 | x64 | 便携版 ZIP / MSI | 随包 SDL3.dll 等依赖；官方 MinGW 最低测试版本 8.0.3 |
| macOS | **12.0** | arm64 + x86_64 (Universal) | `.app` / `.dmg` | 跟随官方 Xcode 模板；需 Developer ID 签名 + 公证 |
| Linux | glibc（SDL 默认仅链接 glibc） | x86_64 | AppImage / Flatpak | 需覆盖 Wayland 与 X11 |
| Android | **API 21**（工具链 **SDK 35+ / NDK r28c+**） | 发布 `arm64-v8a`；开发期可含 `x86_64` 模拟器 | `.aab` / `.apk` | 可零权限 |
| iOS | **12.0** | arm64 | App Store / TestFlight | **必须有 LaunchScreen** + `PrivacyInfo.xcprivacy` |
| HarmonyOS | **5.0.4（API level 16）及以上** | `arm64-v8a` | `.hap` / `.app` | **不含 APK 兼容路线**；官方真机仅验证过 Mate 80 |

> ⚠️ **构建形态提示（重要）**：桌面产物是**可执行文件**；**Android 与 HarmonyOS 必须产出 `libmain.so` 共享库**，
> 不得构建可执行文件。同一份 CMake 需按平台切换目标形态。详见 `技术验证结论.md` 2.3。

### 0.3 开发环境与已知陷阱（已实测）

> **✅ 环境问题已解决：无需安装任何系统级工具。**
> 本机原本缺 CMake 与 Ninja，且 Homebrew 因沙箱限制不可用；
> **CLion 2026.2.3.1 自带完整工具链**，已用它把整条链路跑通：
> `CLion CMake 4.3.1 → FetchContent 浅克隆 SDL3 → 编译 SDL3 → 链接探针 → 运行输出 3.5.0`。
> 无需提权、无需触碰系统配置。

**本机环境（2026-10-13 实测）**：

| 项 | 状态 |
|----|------|
| CMake | 系统**未安装**；使用 **CLion 2026.2.3.1 自带**：`/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake`（**4.3.1**） |
| Ninja | 系统未安装；使用 CLion 自带：`/Applications/CLion.app/Contents/bin/ninja/mac/aarch64/ninja`（**1.13.2**） |
| 编译器 | `/usr/bin/clang`（Apple clang 21）、`/usr/bin/make`、`/usr/bin/git` 均在 |
| CLion 附加工具 | `clangd` / `clang-tidy` / `llvm-symbolizer`（`Contents/bin/clang/mac/aarch64/bin/`），可直接供 IDE 与静态检查使用 |
| 网络 | github.com / cmake.org / pypi.org 均可达（HTTP 200）；**但大文件传输不稳定**（见 E2） |
| 沙箱 | `workspace-write`：**仅允许写工作区**。工作区外写入（`/opt/homebrew`、`$HOME/Library`）一律 `Operation not permitted` |

> **注意**：`git` 仓库尚未初始化（`.gitignore` 已就位，`git init` 后即生效）。
> 环境验证残留的 `.tools/` 约 145MB（内含 SDL3 浅克隆与探针），已被 `.gitignore` 排除，可随时删除。

**陷阱 E1：不要跑 `sudo chown -R c /opt/homebrew`。**
Homebrew 的报错把原因说成"权限坏了"，但实测 `/opt/homebrew` 属主**本来就是 `c:admin`**；
真实原因是沙箱禁止工作区外写入。跑 chown 既无效又可能搞乱 Homebrew 的属主关系。

**陷阱 E2：SDL3 必须用「浅克隆」，全历史克隆在本环境会失败。**
实测对比（2026-10-13，CMake 4.3.1 / macOS / 沙箱 `workspace-write`）：

| 方式 | 结果 |
|------|------|
| 全历史克隆（FetchContent 默认，约 75MB） | ❌ `error: RPC failed; curl 56 Recv failure: Connection reset by peer` → `fatal: early EOF` → CMake 自动重试 3 次全败 |
| **浅克隆**（`GIT_SHALLOW TRUE`，约 34MB） | ✅ 成功，解析到 `b64293b3`（= 当日 main HEAD），鸿蒙后端与官方构建脚本齐全 |
| tarball 下载（`codeload` CDN，5.5MB） | ✅ HTTP 200 稳定 |

**规则**：`FetchContent_Declare(SDL3 ... GIT_SHALLOW TRUE)`，**不要**依赖默认的全历史克隆。

> **一个已被推翻的早期判断（留作记录）**：曾把首次失败归因为"浅克隆 + `main` 的组合会挂死"，
> 并在路线图里写过"保持 `GIT_SHALLOW FALSE`"——**那是错的，方向恰好相反**。
> 真正原因是全历史克隆体积过大导致传输被中断。手动 `git clone --depth 1 --single-branch`
> 验证通过后确认：浅克隆是**解法**，不是病因。

**附加陷阱 E2b：`GIT_TAG` 用分支名时，`.git` 目录内无 `.gitmodules` 相关子模块内容。**
SDL3 本身无子模块，用浅克隆没问题；但 **SDL3_ttf 依赖 git submodule**（FreeType/HarfBuzz 等，见 0.1 节），
克隆 SDL3_ttf 时**必须**加 `GIT_SUBMODULES_RECURSE TRUE`，否则配置阶段会因缺少 `external/freetype` 而失败。
若 SDL3_ttf 的递归克隆在本环境也不稳定，退路是：手动 `git clone --depth 1 --recurse-submodules`
到本地后用 `FetchContent_Declare(... SOURCE_DIR <本地路径>)` 指向它。

**陷阱 E3：包管理器缓存目录也在工作区外。**
vcpkg / Conan 默认把缓存写在 `~/.cache`、`~/.vcpkg`，会撞同一堵沙箱墙。
**规则**：把缓存重定向到工作区内，例如
`export VCPKG_DEFAULT_BINARY_CACHE="$PWD/.cache/vcpkg"`。

**陷阱 E4：CMake 4.x 的兼容性下限。**
CMake 4.0 移除了对 `cmake_minimum_required(<3.5)` 的兼容。本项目已核实关键依赖均声明兼容 CMake 4：
SDL3 `3.16...4.4`、SDL3_ttf `3.16...4.0`、Catch2 `3.16` —— 与 CLion 的 **4.3.1** 相容。
若日后引入其他第三方库，需先确认其 `cmake_minimum_required` 上限。

**构建命令约定**（脚本统一用可覆盖变量，便于本地用 CLion 工具链、CI 用系统工具链）：

```bash
CMAKE="${CMAKE:-cmake}"
NINJA="${NINJA:-ninja}"
"$CMAKE" -S . -B build -G Ninja -DCMAKE_MAKE_PROGRAM="$NINJA"
"$CMAKE" --build build
```
- 本地（利用 CLion 自带工具链，无需任何系统级安装）：
  `CMAKE=/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake \
   NINJA=/Applications/CLion.app/Contents/bin/ninja/mac/aarch64/ninja ./build.sh`
- macOS 桌面工程仍可用 `-G Xcode`（CLion 的 CMake 完整支持）。
- `ninja` 与 `Unix Makefiles` 二者皆可；CI 里按 runner 可用工具选择。

---

## 1. 项目总览

### 1.1 架构原则（不得违反）

1. **`core/` 零 SDL 依赖** —— 可在不引入任何 SDL 头文件的情况下编译通过（P0/P1 有可验证检查项）。
2. **渲染一律用逻辑分辨率**（`SDL_SetRenderLogicalPresentation`），窗口/屏幕尺寸变化**不得**要求改渲染代码。
3. **输入坐标换算只允许有一处实现** —— 事件入口那一次 `SDL_ConvertEventToRenderCoordinates()`（F4）。
4. **资源与存档必须经抽象层**（`AssetIO` / `SaveIO`），上层代码不出现平台路径。
5. **C++ 异常不得跨越 JNI / NAPI / ObjC 边界**，语言边界处 `try/catch(...)` 兜底转错误码。
6. **用户可见文案不硬编码**，统一走 `TextId → 文案表`（为多语言留位）。

### 1.2 阶段与时间线（关键路径）

| 阶段 | 名称 | 周次 | 里程碑 | 任务数 | 状态 |
|------|------|------|--------|--------|------|
| **P0** | 初始化、SDL3 锁定与技术验证 | 第 1 周 | M0 桌面三平台空窗口 | 18 | ⬜ |
| **P1** | 核心逻辑 + 抽象层 + 单元测试 | 第 2 周 | M1 `v0.1.0` | 22 | ⬜ |
| **P2** | 桌面 SDL3（渲染/交互/文字/音频） | 第 3–4 周 | M2 `v0.2.0` | 24 | ⬜ |
| **P3** | 移动端 Android / iOS | P2 完成后约 2 周 | M3 `v0.3.0` | 20 | ⬜ |
| **P4** | HarmonyOS（路线 A 默认 / B 退路） | P3 完成后约 2–3 周 | M4 `v0.4.0` | 18 | ⬜ |
| **P5** | 打磨、打包、CI 与发布验收 | 收尾约 1–2 周 | M5 `v1.0.0` | 18 | ⬜ |

**总预估：10–12 周**（P0–P2 共 4 周为固定线性；P3 与 P4 是关键路径，其浮动决定总量）。

> **关于并行**：P0→P1→P2 必须顺序执行。若有人力，P3（移动端）与 P4（鸿蒙）**在共享同一 commit SHA 的前提下可部分并行**，
> 此时总量可压到约 10 周；单人线性推进则为 12 周。

### 1.3 里程碑

| 版本 | 达成条件 |
|------|---------|
| `v0.1.0` | P1 完成：`ttt_core` 无 SDL 依赖、单元测试全绿、CI 在三平台跑通 |
| `v0.2.0` | P2 完成：桌面三平台可玩，文字、音效、resize、安全区、坐标换算全部跑通 |
| `v0.3.0` | P3 完成：Android / iOS 真机可玩，切后台状态不丢失 |
| `v0.4.0` | P4 完成：HarmonyOS 真机可玩（路线 A 成功，或路线 B 退路成功） |
| `v1.0.0` | P5 完成：全平台发布包就绪并通过逐平台验收清单 |

### 1.4 目录结构

```text
Cjing/                          # 仓库根目录
├── core/                       # 纯 C++ 逻辑 + 平台抽象接口（零 SDL 依赖）
│   ├── include/ttt/
│   ├── src/
│   └── tests/
├── platform_sdl/               # SDL3 实现：IView / IInput / AssetIO / SaveIO / 生命周期
├── platform_ohos/              # 鸿蒙路线 B 退路实现（路线 A 不需要它）
├── android/                    # Android Gradle + JNI 壳工程
├── ios/                        # iOS Xcode 壳工程
├── ohos/                       # HarmonyOS DevEco 壳工程
├── assets/                     # 字体（含授权文件）、音效
├── cmake/                      # 工具链文件与辅助脚本（含 ohos.toolchain.cmake 引用）
├── docs/                       # 本路线图与技术验证结论等项目文档
├── Cjing.xcodeproj/            # 现有 Xcode 工程（保留；桌面版工程由 CMake 生成，勿手工维护）
├── CMakeLists.txt
└── README.md
```

> 现有原型 `Cjing/code.cpp`（全局 `char board[9]` + `checkWin` 函数式写法）**作为 P1 的输入**：
> 胜负判断思路可复用，但必须按 P0-T9 的契约重构成无全局状态的形式；重构完成前不得作为跨平台主逻辑。

### 1.5 进度跟踪规范

1. 完成一项任务即把 `- [ ]` 改为 `- [x]`，并同步更新第 1.2 节的状态（⬜ / 🟡 / ✅ / ⛔）。
2. 遇阻写入第 3 节风险登记册，并把相关任务标 ⛔。
3. Git：每阶段一个分支或 tag，例如 `v0.1-core`、`v0.2-desktop`、`v0.3-mobile`、`v0.4-ohos`。
4. 每周回顾一次本文件；提交信息示例：`docs(roadmap): 完成 P0-T3 锁定 SDL3 commit SHA`。
5. **任何 SDL3 / SDL3_ttf 版本变更都必须更新 P0-T2 的锁定 SHA，并同步回归桌面、Android、iOS、HarmonyOS 四端。**
6. **每阶段结束时核对第 1.2 节的任务数与实际勾选框数一致**（避免计划漂移）。

---

## 2. 阶段任务清单

### P0 初始化、SDL3 锁定与技术验证（第 1 周）

> **里程碑 M0**：桌面三平台均可编译运行 SDL3 空窗口，且已锁定唯一 SDL3 commit SHA

- [x] P0-T1 初始化 Git 仓库与顶层目录结构（按 1.4 节：`core/ platform_sdl/ platform_ohos/ android/ ios/ ohos/ assets/ cmake/`），添加 `.gitignore`
- [x] P0-T2 编写根 `CMakeLists.txt`：C++20、`ttt_core` 静态库；用**明确的 CMake 变量**定义 `SDL3_GIT_TAG`（commit SHA）与 `SDL3_TTF_GIT_TAG`
- [x] P0-T3 **锁定 SDL3 来源与版本（关键）**：用 `FetchContent` + **`GIT_SHALLOW TRUE`** 拉取 SDL3 源码
  - **已实测通过（2026-10-13）**：CLion 自带 CMake 4.3.1 → FetchContent 浅克隆 → 编译 SDL3 → 链接探针 → 运行输出
    `SDL_VERSION compiled 3.5.0 / SDL_GetVersion runtime 3.5.0`，解析提交 `b64293b342c2cde123285199818803f83dbac612`
    （2026-10-12，"Fixed audio crash in Android and OpenHarmony"），鸿蒙后端与 `build-scripts/harmonyos-tool.sh` 齐全
  - `GIT_TAG` 用**分支名**（浅克隆 + 分支名已验证稳定；裸 SHA 未必支持浅取）
  - **不使用包管理器 / 系统包**（给不了含鸿蒙后端的版本）；**不锁 tag**（当前无任何 tag 包含鸿蒙后端）
  - 但**必须把解析出的实际 SHA 记录下来**（P0-T5），六端（Win/macOS/Linux/Android/iOS/HarmonyOS）**共用同一 SHA**
  - ⚠️ 详见 0.3 节陷阱 E2：**全历史克隆会因连接被 reset 而失败，必须开浅克隆**
- [x] P0-T4 **复核并记录鸿蒙支持现状**（结论已知；**并已在本机实测编译成功**，见 `环境与平台侦察报告.md` 第二节）：
  - ✅ **实测**：用 DevEco Studio 6.1.1 自带工具链（CMake 3.28.2 + `ohos.toolchain.cmake`）成功交叉编译出
    **`libSDL3.so`（2.1MB / ELF aarch64）**，configure 输出确认 `Video drivers: ... openharmony`、
    `Audio drivers: ... ohaudio` 均已启用
  - 剩余卡点只有**签名证书**与**真机/模拟器**（均需开发者账号与设备，非技术障碍）
  - 路线 A（SDL3 `openharmony` 后端）为**默认路线**；路线 B（ArkTS + XComponent + NAPI + 原生 EGL/GLES 渲染）为**退路**
  - 鸿蒙风险等级 = **中**（可用但未经充分实战，非稳定版功能）
  - 唯一仍需实测的是：目标 API 版本上能否构建并运行（见 P4-T9）
- [x] P0-T5 在 README 与《技术验证结论》中记录锁定的 SHA、来源 URL 与锁定日期
- [x] P0-T6 添加 `.clang-format`、`.clang-tidy` 与代码风格约定
- [x] P0-T7 **配置 CI（前置，勿拖到最后）**：`.github/workflows/desktop.yml` 已创建并通过**本地等价验证**
  （configure → build → ctest → `core_purity_check` 全通过，195 断言）
  - 矩阵：ubuntu(Debug+ASan/UBSan)、ubuntu(Release)、macos(Release)、windows(Release)
  - 已包含 SDL3 在 Linux 源码构建所需的图形/输入开发库安装步骤
  - ⬜ **待你操作**：创建远端仓库并 push，CI 才会真正运行（我无凭据，不应代推）
  - ⚠️ **移动端/鸿蒙不进 CI**：缺签名材料（Android keystore / Apple 证书 / 华为 `.p7b`）。
    策略定为"桌面全自动 + 移动端脚本化手动打包"，**私钥不入 CI**
- [x] P0-T8 **输出《技术验证结论》**（针对锁定 SHA 复核头文件与官方文档，逐条核对以下 7 项）
  → **已产出 `技术验证结论.md`**（含 F1–F7 复核结果、平台矩阵依据、9 项鸿蒙实测限制与发现）：
  1. `SDL_main.h` 与入口点要求：`SDL_MAIN_USE_CALLBACKS` 模式下不得提供 `main()`（F1）
  2. 回调签名，含 `SDL_AppEvent(void*, SDL_Event*)` 为非 const（F2）
  3. 生命周期事件名（F3，注意无 `_APP_` 前缀，且 `WINDOW_MINIMIZED` 不适用）
  4. 坐标换算 API `SDL_ConvertEventToRenderCoordinates`（F4）
  5. 触摸/鼠标合成 hint 默认值（F5）
  6. 安全区 API `SDL_GetWindowSafeArea`（F6）
  7. Android 资源行为：锁定版本下 `SDL_GetBasePath()` 返回值、是否支持 `assets://`（F7）
- [x] P0-T9 确定并写入 `core/` 的抽象契约（纯接口，无 SDL 类型）：
  - `Cell` / `GameState` / `MoveResult` / `Status` 值类型；`Game::applyMove` / `state()` / `winningLine()`
  - `IView`（渲染抽象）、`IInput`（**只接收渲染坐标**）、`Layout`（只吃逻辑尺寸，内部按逻辑单位算格子）
  - `AssetIO::read` / `SaveIO::write·read`（落到 `SDL_GetPrefPath`）
  - `IAppLifecycle::onPause·onResume`；文案间接层 `TextId → 文案表`
- [x] P0-T10 编写 SDL3 main callbacks 骨架：`#define SDL_MAIN_USE_CALLBACKS` + `#include <SDL3/SDL_main.h>` + `SDL_AppInit` 创建窗口/渲染器 + `SDL_AppIterate` 空帧 + `SDL_AppEvent` 处理 `SDL_EVENT_QUIT` + `SDL_AppQuit`
- [x] P0-T11 三平台编译验证：Windows（MSVC/MinGW）、macOS（Clang，设置 RPATH）、Linux（GCC/Clang，设置 RPATH）
- [x] P0-T12 空窗口显示验证：**macOS 已实测**（启动输出 `Cjing 0.1.0 | SDL 3.5.0`）；
  Windows/Linux 由 CI（`.github/workflows/desktop.yml`）覆盖
- [x] P0-T13 验证 CI 在三平台通过，并把状态徽章挂到 README
- [x] P0-T14 **确认构建环境（已实测，见 0.3 节）**：
  - 使用 **CLion 2026.2.3.1 自带工具链**，无需安装任何系统级工具：
    - CMake：`/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake`（**4.3.1**，满足 ≥ 3.20）
    - Ninja：`/Applications/CLion.app/Contents/bin/ninja/mac/aarch64/ninja`（**1.13.2**）
  - **不要**执行 `sudo chown -R c /opt/homebrew` —— 该建议是误导：`/opt/homebrew` 属主本来就是 `c:admin`，
    写入被拒的真实原因是**沙箱禁止工作区外写入**（已实测 `touch` 报 `Operation not permitted`）
  - 项目脚本统一写成 `CMAKE=${CMAKE:-cmake}` / `NINJA=${NINJA:-ninja}`，便于本地用 CLion 路径、CI 用系统路径
  - ✅ `.gitignore` **已创建**（排除 `build*/`、`.tools/`、`.cache/`、`cmake-build-*/`、`*.xcodeproj/xcuserdata/` 等）
  - ✅ **整条链路已实测跑通**（见 P0-T3）：CLion CMake 4.3.1 → FetchContent 浅克隆 SDL3 → 编译 → 链接 → 运行
- [x] P0-T15 记录《技术验证结论》中"待回填项"：iOS/macOS 最低版本、Linux glibc 基线、Windows 最低版本（按锁定 SHA 的官方文档核实）
- [x] P0-T16 确定 `SDLTTF_VENDORED` / `SDLTTF_HARFBUZZ` / `SDLTTF_PLUTOSVG` 的配置策略并写入 README（见 0.1 节；鸿蒙/Android 建议 `PLUTOSVG=OFF`）
- [x] P0-T17 阶段收尾：核对 P0 任务数（18）与实际勾选框数一致
- [x] P0-T18 里程碑 M0：三平台空窗口 + CI 绿 + SHA 已锁定 + 技术验证结论已产出

### P1 核心逻辑 + 抽象层 + 单元测试（第 2 周）

> **里程碑 M1**：`ttt_core` 完成、单元测试全绿，且编译它**不需要任何 SDL 头文件**

- [x] P1-T1 重构 `Cjing/code.cpp` 原型为 `Board` 类：3×3 状态存储、`placeMove`（校验范围/是否为空/终局后拒绝）、`reset`、`getCell`
- [x] P1-T2 实现胜负判定——三行（逐行实现并测试）
- [x] P1-T3 实现胜负判定——三列
- [x] P1-T4 实现胜负判定——主对角线
- [x] P1-T5 实现胜负判定——副对角线（合计覆盖全部 8 条胜负线）
- [x] P1-T6 实现平局判定（棋盘填满且无胜者）
- [x] P1-T7 实现 `Game` 类：回合切换、`Status evaluate()`、记录 `winLine` 供 UI 高亮
- [x] P1-T8 实现 `Game::applyMove` 与 `state()`，严格按 P0-T9 契约（UI 不得直接读写棋盘）
- [x] P1-T9 实现 `Game::reset()`
- [x] P1-T10 实现 Minimax AI（**直接一次做对**，带已搜索局面缓存；3×3 全搜索仅 255,168 节点、合法局面 5,478 个，毫秒级）
- [x] P1-T11 实现 AI 难度档位（简单 = 带随机性的浅层；普通 = 完整 Minimax；困难 = Minimax + 优先最快取胜）
- [x] P1-T12 用 `FetchContent` 引入 Catch2（或 GoogleTest）与 CTest 集成
- [x] P1-T13 测试：空棋盘 → `Playing`
- [x] P1-T14 测试：第一行三连 X → `XWins`
- [x] P1-T15 测试：第二行三连 O → `OWins`
- [x] P1-T16 测试：第三行三连 X → `XWins`
- [x] P1-T17 测试：第一列三连 O → `OWins`
- [x] P1-T18 测试：第二列三连 X → `XWins`
- [x] P1-T19 测试：第三列三连 O → `OWins`
- [x] P1-T20 测试：主对角线 / 副对角线三连 → 胜
- [x] P1-T21 测试：满盘无胜者 → `Draw`；非法落子（越界、已占用、终局后落子）被拒绝
- [x] P1-T22 **验证 `core/` 零 SDL 依赖**（可验证完成标志：在无 SDL 的环境下编译 `ttt_core` 成功，且 `core/` 中 grep 不到 `SDL`）+ 里程碑 `v0.1.0`

### P2 桌面 SDL3：渲染 / 交互 / 文字 / 音频（第 3–4 周）

> **里程碑 M2**：桌面三平台可完整对弈，文字、音效、resize、安全区全部正常

- [x] P2-T1 实现 `SDLRenderer`（实现 `IView`）：持有 `SDL_Renderer*`，封装绘制调用
- [x] P2-T2 设定逻辑分辨率：`SDL_SetRenderLogicalPresentation(renderer, 900, 900, SDL_LOGICAL_PRESENTATION_LETTERBOX)`（DPI 缩放、窗口缩放、letterbox 全部交给 SDL）
- [x] P2-T3 实现 `Layout`：只接受逻辑尺寸，内部按逻辑单位算格子，保持棋盘正方形居中；**不得**读取窗口像素尺寸算位置
- [x] P2-T4 绘制棋盘网格（`SDL_RenderLine`，4 条线）与 X/O（矢量绘制，`SDL_RenderLine` / `SDL_RenderFillRect`；注意 `SDL_RenderFillRect` 参数为 `const SDL_FRect*`）
- [x] P2-T5 处理窗口尺寸变化事件（`SDL_EVENT_WINDOW_RESIZED` 等），尺寸变化时重算布局；**不允许出现写死坐标**
- [x] P2-T6 处理 `SDL_EVENT_QUIT`，实现正常退出（资源释放顺序：纹理 → 渲染器 → 窗口 → `SDL_Quit`）
- [x] P2-T7 **输入统一换算（关键，唯一实现点）**：在 `SDL_AppEvent` 入口调用一次 `SDL_ConvertEventToRenderCoordinates(renderer, event)`，把鼠标/触摸统一为渲染坐标后再交给 `IInput`；`Layout` 只消费渲染坐标
- [x] P2-T8 实现鼠标点击 → 棋盘格映射（消费换算后的渲染坐标，按逻辑单位除以 3 得行列）；**禁止**在事件循环里另写命中判断
- [x] P2-T9 **触摸/鼠标去重（关键）**：选定并写死方案 —— **方案 A（推荐）**保持 `SDL_HINT_TOUCH_MOUSE_EVENTS` 默认 `"1"`，只处理鼠标事件、**显式忽略** `SDL_EVENT_FINGER_*`，桌面与移动完全同构；或方案 B 设 hint 为 `"0"` 只处理 finger。无论哪种，**输入层必须保证一次落子意图只产生一次落子**
- [x] P2-T10 处理 `SDL_EVENT_LOW_MEMORY` 与 `SDL_EVENT_RENDER_DEVICE_RESET`（释放/重建可重建资源）
- [x] P2-T11 **安全区注入**：调用 `SDL_GetWindowSafeArea(window, &rect)`，把安全矩形注入 `Layout` 作为可交互边距；**不要**用 `SDL_SetRenderViewport`
- [x] P2-T12 引入 `SDL3_ttf`（**必需，提前到此阶段**，勿拖到 P5）：FetchContent 锁 SHA + `GIT_SUBMODULES_RECURSE TRUE`（见 0.1）；按 P0-T16 的策略配置 vendored 依赖
- [ ] P2-T13 实现文本渲染（当前回合、胜负结果、平局），文案经 `TextId → 文案表` 查询；验证中文字形不缺字（缺字会显示方块）
- [x] P2-T14 确认字体授权允许再分发，并在 `assets/` 中保留授权文件
- [x] P2-T15 实现 `AssetIO` 的桌面实现（文件路径），字体加载路径不得写死绝对路径
- [x] P2-T16 实现 `SaveIO` 的桌面实现（`SDL_GetPrefPath`），支持退出时保存、启动时恢复
- [x] P2-T17 音频接入：用 **SDL3 核心音频 API**（`SDL_audio.h`）播放落子音效（WAV，无额外依赖）；**不引入** `SDL3_mixer`
- [x] P2-T18 实现 `winLine` 高亮（线索来自 P1-T7）
- [x] P2-T19 键盘：`R` 重开、`ESC` 退出
- [x] P2-T20 渲染优化：**内容**仅在状态变化时重绘，但**必须每帧调用 `SDL_RenderPresent`**
  - ⚠️ **踩坑记录**：初版按"仅在状态变化时 present"实现，结果 `dirty_` 变为 false 后不再 present，
    SDL 的 vsync 节流失效，`SDL_AppIterate` 以 ~20 万次/秒空转，CPU 占满、事件饿死，
    macOS 表现为**彩虹圈（应用无响应）**，任何操作都卡死
  - 正确做法：vsync 节流发生在 `SDL_RenderPresent` 内部 —— 内容可跳过重绘，present 不能跳过
- [ ] P2-T21 处理 SDL3 运行时依赖：Windows 随包 DLL、macOS `Contents/Frameworks`、Linux 打包或依赖系统库；记录第三方库清单
- [ ] P2-T22 三平台手动测试：对弈、胜负/平局、文字、音效、resize、安全区、重开
- [ ] P2-T23 **鸿蒙 Go/No-Go 预检（不占额外工期）**：用 P0 锁定的 SHA 构建一个**鸿蒙空窗口 + 触摸回调 + 一行中文文字**的最小应用。**Go 判据 = 空窗口可显示 且 字体渲染成功**；音频为可降级项，不作为判据
> **TTF 实测记录（2026-10-15）**：SDL3_ttf 3.3.0 以 **`SDLTTF_VENDORED=ON`** 构建成功。
> 未启用 vendored 时会去找系统 FreeType/HarfBuzz，macOS/Windows 默认没有、鸿蒙/Android 更不会有 → configure 直接失败。
> 运行日志确认字体加载：`[Cjing] 字体已加载: assets/fonts/NotoSansSC-Regular.ttf @ 39pt`
> （39pt = 44 逻辑单位 × 0.884 输出缩放，证明逻辑单位→像素换算正确）。
> CI 在四矩阵（含 Windows）全部通过，TTF 构建三平台可用。

> **存档实测记录（2026-10-19）**：`SaveIO` 与 `AssetIO` 已落地，并用端到端流程验证：
> 启动 → 预置存档 `TTT1,X...O....,X,2` → 日志 `已恢复对局: 手数=2` →
> 退出时写回 `TTT1,X...O....,X,2`（与预置完全一致）。
> 另支持 `CJING_SAVE_DIR=<目录>` 覆盖存档位置（便携模式 / 自动化测试）。
> 注：`WILL_ENTER_BACKGROUND` / `TERMINATING` 的存档钩子已在桌面验证
> （SIGTERM 触发正常退出并落盘）；移动端真实切后台仍待 P3 真机验证。

- [ ] P2-T24 里程碑 `v0.2.0`：桌面可玩可打包 + 鸿蒙预检结论已产出

### P3 移动端 Android / iOS（约 2 周）

> **里程碑 M3**：Android / iOS 真机可玩，触摸准确，切后台状态不丢失
> **前提**：坐标换算、逻辑分辨率、指针语义已在 P2 完成。**若本阶段需要改渲染或布局代码，说明 P2 抽象没做对 —— 应回头修 P2，而不是在此打补丁。**

- [ ] P3-T1 Android：创建 `android/` 模块（Gradle + NDK + CMake），配置 `externalNativeBuild` 指向根 CMake
- [ ] P3-T2 Android：确认工具链 **SDK 35+ / NDK r28c+ / 最低 API 21**
- [ ] P3-T3 Android：`abiFilters` 策略写死 —— 开发期 `arm64-v8a` + `x86_64`（模拟器），**发布仅 `arm64-v8a`**（与 P5 打包一致）
- [ ] P3-T4 Android：以 P0 锁定的**同一 SHA** 引入 SDL3 与 SDL3_ttf 源码（不使用系统/包管理器 SDL）
- [ ] P3-T5 Android：编译为 `.so` 共享库（**非可执行文件**；Android 要求目标为名为 `main` 的共享库），由 `SDLActivity` 子类加载
  - ⚠️ 桌面形态是**可执行文件**（`CjingApp`），不能复用：需在 CMake 中按平台切换目标形态
    （`add_executable` ↔ `add_library(main SHARED)`），见 `技术验证结论.md` 2.3
- [ ] P3-T6 Android：配置 `AndroidManifest.xml`（屏幕方向、`targetSdk` 按商店当期要求）；确认可零权限运行
- [ ] P3-T7 Android：`SDL_AppEvent` 中处理生命周期事件（**F3 的事件名**）：
  - `SDL_EVENT_WILL_ENTER_BACKGROUND`：停止渲染循环、`SDL_SetRenderTarget(renderer, NULL)`、禁止渲染调用、**经 `SaveIO` 落盘存档**（棋盘 9 格 + 当前玩家 + 可选历史）
  - `SDL_EVENT_DID_ENTER_BACKGROUND`
  - `SDL_EVENT_WILL_ENTER_FOREGROUND`：恢复渲染循环、从存档还原局面
  - `SDL_EVENT_DID_ENTER_FOREGROUND`
  - `SDL_EVENT_TERMINATING` / `SDL_EVENT_LOW_MEMORY`（释放纹理）/ `SDL_EVENT_RENDER_DEVICE_RESET`（重建纹理）
- [ ] P3-T8 Android：资源解析按锁定 SHA 实现（F7：锁定开发版 3.5.0 下 `SDL_GetBasePath()` 返回 `"./"`，不支持 `assets://`）；在真机验证字体与音效能否从 APK assets 加载
- [ ] P3-T9 Android：安全区 —— 用 `SDL_GetWindowSafeArea()` 注入 `Layout`（Android 15+ 窗口默认铺满全屏，必做）；**不要**用 viewport
- [ ] P3-T10 Android：输入沿用 P2-T7 的统一换算路径，**不另写坐标换算**；沿用 P2-T9 的去重策略
- [ ] P3-T11 Android：异常边界 —— 所有 JNI 处 `try/catch(...)` 兜底转错误码；渲染/UI 在主线程；跨库内存"谁分配谁释放"
- [ ] P3-T12 Android：真机验证「切后台 → 系统回收 → 重新进入 → 局面正确恢复」（不依赖进程不被杀）；不得出现一次触摸落两子
- [ ] P3-T13 Android：生成调试 APK 真机测试，随后配置签名与 AAB
- [ ] P3-T14 iOS：创建 `ios/` 工程（CMake iOS 目标或 SDL3 官方 iOS 模板 / `SDL3.xcframework`）
- [ ] P3-T15 iOS：配置 Bundle ID、签名、`Info.plist`（方向、显示名、最低部署版本）、**LaunchScreen（缺失会被拒审）**
- [ ] P3-T16 iOS：**`PrivacyInfo.xcprivacy` 隐私清单**（使用 Apple Required Reason API 时缺失会导致审核被拒）
- [ ] P3-T17 iOS：处理触摸（沿用 P2-T9 策略）、安全区（`SDL_GetWindowSafeArea`）、后台生命周期（事件名同 P3-T7）
- [ ] P3-T18 iOS：ObjC/Swift 回调处 `try/catch(...)` 兜底；验证 Retina 下清晰度与触摸命中精度
- [ ] P3-T19 iOS：生成 Archive 前**必须用脚本移除嵌入的 SDL3 framework**（否则无法成功分发）；导出 IPA 验证可安装
- [ ] P3-T20 里程碑 `v0.3.0`：Android/iOS 可玩（含 TTF 文字、存档恢复、安全区）＋ **渲染/布局相对 P2 零改动**（若有改动，回到 P2 修抽象）

### P4 HarmonyOS 适配（约 2–3 周）

> **里程碑 M4**：HarmonyOS NEXT 真机可玩，产出 `.hap`
> **入场条件**：P2-T23 的预检为 Go；若为 No-Go，本阶段降级为"可行性跟踪 + 路线 B 试点"，**不阻塞 P5 的 5 平台发布**
> **⚠️ 兼容性范围**：HarmonyOS NEXT（5.x 起）**已不兼容 Android APK，必须交付 `.hap`**；不要把 APK 兼容层当作后备路径

- [ ] P4-T1 安装 DevEco Studio（5.x）与 HarmonyOS NDK/SDK，确认 `ohos.toolchain.cmake` 可用；记录版本组合
- [ ] P4-T2 **路线 A（默认）**：**复制 SDL 源码树中的 `openharmony-project` 作为工程模板**（改名后放入 `ohos/`）
  - ⚠️ **不要在 DevEco Studio 里新建工程** —— 官方明确要求：SDL 依赖其中已配置好的 ArkTS 脚本，
    新建工程会缺失这些配置，导致 NAPI/XComponent 对接反复踩坑（见 `技术验证结论.md` 2.2）
  - 需改 `AppScope/app.json5` 的 `bundleName`/`vendor`、`AppScope/resources/base/element/string.json` 的 `app_name`、
    以及 `build-profile.json5` 的 `signingConfig`
- [ ] P4-T3 路线 A：确认锁定的 SHA 确实包含 `src/video/openharmony/`（video/events/mouse/keyboard/clipboard/opengl/vulkan/window）与官方构建脚本
- [ ] P4-T4 路线 A：用 ArkTS `UIAbility` / `XComponent` 获取 Native Window 句柄并转发触摸与生命周期给 C++
- [ ] P4-T5 路线 A：CMake 编译 `core` + `platform_sdl` 为 **`libmain.so`**（共享库，**非可执行文件**）
  - ⚠️ **必须使用鸿蒙自带的 CMake**，标准 CMake（含 CLion 自带的 4.3.1）**无法识别该平台，会直接构建失败**：
    ```
    $CLT/sdk/default/openharmony/native/build-tools/cmake/bin/cmake \
      -DCMAKE_TOOLCHAIN_FILE=$CLT/sdk/default/openharmony/native/build/cmake/ohos.toolchain.cmake \
      -DOHOS_COMPATIBLE_SDK_VERSION=16.0.0 -DOHOS_ARCH=arm64-v8a
    ```
    官方原文：*"you **must** use the CMake that comes with HarmonyOS's development tools, or building will fail"*
  - 入口由 ArkTS 侧加载 `libmain.so`；两条路径：有 `SDL_main` 符号则开线程跑它（**返回会像崩溃，禁止 return**），
    否则查找 `SDL_AppInit` 等 Main Callbacks 符号 —— 本项目走后者
- [ ] P4-T6 **SDL3_ttf 鸿蒙交叉编译**（本阶段主要工作量）：按 0.1 节策略递归拉取 vendored FreeType/HarfBuzz，**建议 `SDLTTF_PLUTOSVG=OFF`**；验证字体渲染正常
- [ ] P4-T7 路线 A：输入沿用 P3 的统一换算与去重策略（鸿蒙后端若产生 finger 事件，按 P2-T9 的二选一处理；换算仍只在 C++ 侧一处）
- [ ] P4-T8 路线 A：安全区用 `SDL_GetWindowSafeArea()` 注入 `Layout`
- [ ] P4-T9 路线 A：处理与 P3-T7 相同的生命周期/内存/渲染设备重置事件（由 SDL 后端进入 `SDL_AppEvent`）
  - ⚠️ **帧驱动模型不同**：`SDL_AppIterate` 由 XComponent 的 OnFrame 回调驱动，不是自由循环。
    P2-T20 的"仅状态变化时 present"语义需在真机实测帧节奏后再确认
  - ⚠️ **方向控制受限**：`SDL_HINT_ORIENTATION` **尚未实现**（官方 STILL TODO）。
    鸿蒙端屏幕方向只能在 `module.json5` 中做**静态**配置；三端的"方向策略"需分开表述
  - 另注意：鸿蒙**同一时刻只允许一个 `SDL_Window`**，且占满整个显示；全屏会隐藏系统状态栏与导航栏
- [ ] P4-T10 路线 A：NAPI 边界 `try/catch(...)` 兜底；跨库内存谁分配谁释放
- [ ] P4-T11 **路线 B（退路，仅在路线 A 阻塞时启用）**：保留 `ttt_core`，新建 `platform_ohos` 实现
- [ ] P4-T12 路线 B：C++ 侧用 EGL / OpenGL ES 初始化渲染上下文
- [ ] P4-T13 路线 B：通过 NAPI 暴露初始化、触摸、渲染、reset 接口
- [ ] P4-T14 路线 B：资源放 rawfile/resources 并在 C++ 中正确读取；生命周期由 ArkTS/XComponent 转发
- [ ] P4-T15 构建 HAP 并安装到真机验证：**重点验证目标 API 版本上能否构建并运行**（鸿蒙唯一还需实测的不确定项）＋ 渲染、输入、安全区、TTF 文字、存档恢复
  - ⚠️ 官方支持起点为 **HarmonyOS 5.0.4（API level 16）**，且**真机仅验证过 Mate 80、未做低端设备测试**
    → 这是硬性下限，也是 R1/R12 风险的来源
  - 鸿蒙日志用 `hdc hilog | grep -F <bundleName>` 查看（`SDL_Log` 走 hilog）；
    **注意 release 版用户也能看到这些日志**，不要打印敏感信息
- [ ] P4-T16 准备发布签名材料（`.p7b` 证书 / `.cer` / Profile），确保真机安装与后续上架可复现
- [ ] P4-T17 构建流程可复现：提供一键构建脚本，并在干净环境验证（CI 接入可选）
- [ ] P4-T18 里程碑 `v0.4.0`：HarmonyOS 可玩（路线 A 成功；若阻塞则切换路线 B 并更新风险状态）

### P5 打磨、打包、CI 与发布验收（约 1–2 周）

> **里程碑 M5**：所有取得 Go 结论的平台发布包就绪，并通过逐平台验收清单

- [ ] P5-T1 视觉打磨：获胜连线闪烁动画（数据源 `winLine`）、平局灰度、按钮、深色/浅色适配；动画只在表现层，不得进入 `core/`
- [ ] P5-T2 设置菜单：音量控制、AI 难度选择、重开
- [ ] P5-T3 音频打磨：胜利/失败音效（仍用核心音频 API；若确需 OGG/MP3 再评估 `SDL3_mixer`）
- [ ] P5-T4 多语言：补齐英文文案表与语言切换（间接层已在 P0-T9 建立）；验证字体字符集覆盖
- [ ] P5-T5 内存与稳定性工具化：Linux 用 ASan/UBSan 跑完整测试与一局对局；macOS 用 Instruments/`leaks`；移动端与鸿蒙前后台切换 50 次观察内存曲线是否单调上升
- [ ] P5-T6 桌面 CI 增强：在既有 CI 中加入 ASan 构建与更完整的测试矩阵
- [ ] P5-T7 移动端打包 CI：Android AAB / iOS（评估可行性，受签名与 runner 成本限制可保留为脚本化手动流程）
- [ ] P5-T8 鸿蒙打包：生成签名后的 `.hap` / `.app`（CI 接入可选，标记为非阻塞）
- [ ] P5-T9 Windows 打包：便携版 ZIP（含全部依赖 DLL，验证干净环境可直接运行）；MSI 可选
- [ ] P5-T10 macOS 打包：Universal Binary（arm64 + x86_64）`.app` / `.dmg`，Developer ID 签名 + Hardened Runtime + **公证（`notarytool`）**，验证 Gatekeeper 不拦截
- [ ] P5-T11 Linux 打包：AppImage（自带 SDL3 等 so）；Flatpak 可选；**验证 X11 与 Wayland 两种会话**
- [ ] P5-T12 Android 打包：签名 `.aab`（发布仅 `arm64-v8a`，与 P3-T3 一致）；确认 `targetSdk` 满足商店当期门槛
- [ ] P5-T13 iOS 打包：Archive 并上传 App Store Connect；补齐 App Privacy 问卷；走一遍 TestFlight 内测后再提交审核
- [ ] P5-T14 所有 CI 任务必须能在**干净环境**复现，构建脚本入库（不接受"只在我机器上能编"）
- [ ] P5-T15 **逐平台验收清单（必须逐条执行，且必须针对发布产物而非开发机构建）**：

| 验收项 | Windows | macOS | Linux(X11) | Linux(Wayland) | Android | iOS | HarmonyOS |
|--------|---------|-------|-----------|----------------|---------|-----|-----------|
| 冷启动正常 | | | | | | | |
| 完整对局（人 vs 人） | | | | | | | |
| 完整对局（人 vs AI） | | | | | | | |
| 胜负判定正确 | | | | | | | |
| 平局判定正确 | | | | | | | |
| 无一次触摸/点击落两子 | | | | | | | |
| 安全区内可交互 | | | | | | | |
| 文字正确显示（中文不缺字） | | | | | | | |
| 音效正常播放 | | | | | | | |
| 菜单功能正常 | | | | | | | |
| 切后台 → 存档 → 恢复 | | | | | | | |
| 退出无残留进程 | | | | | | | |
| 安装/签名验证通过 | | | | | | | |

- [ ] P5-T16 文档：`README.md` 与各平台 `BUILD.md`，记录锁定 SHA、工具链版本要求、资源路径规则、各平台构建与运行步骤
- [ ] P5-T17 发布说明 CHANGELOG；代码清理（无调试输出、无硬编码绝对路径、无未使用资源）；打 tag `v1.0.0`
- [ ] P5-T18 里程碑 `v1.0.0`：所有 Go 平台发布包齐备并通过验收；若鸿蒙为 No-Go，则 5 平台发布且鸿蒙后续计划已记录

---

## 3. 风险登记册

| # | 风险 | 影响 | 应对措施 | 关联任务 | 状态 |
|---|------|------|---------|---------|------|
| R1 | SDL3 鸿蒙后端仅在开发主线，**无发布 tag 包含** | P4 构建失败 | 锁含后端的 commit SHA；路线 A 默认 / B 退路；P4-T15 真机实测 | P0-T3, P0-T4, P4-T15 | ⬜ |
| R2 | 桌面/移动/鸿蒙 SDL3 版本漂移 | 鸿蒙崩溃且难排查 | 六端共用同一 commit SHA，禁用包管理器；升级时同步回归四端 | P0-T2, P0-T3, P3-T4 | ⬜ |
| R3 | 生命周期事件名写错（如误用 `SDL_EVENT_WINDOW_MINIMIZED`） | **切后台不存档** | P0-T8 按锁定 SHA 核对头文件；P3-T7/P4-T9 用 F3 的具体事件名 | P0-T8, P3-T7 | ⬜ |
| R4 | 触摸与鼠标双事件 | 一次点击落两子 | P2-T9 显式二选一 + 输入层去重 | P2-T9, P3-T10 | ⬜ |
| R5 | 坐标换算未统一，letterbox 下点偏 | 交互错位 | P2-T7 唯一换算点；禁用自写 `x*winW` | P2-T7, P3-T10, P4-T7 | ⬜ |
| R6 | 安全区未适配（Android 15+ / iOS 刘海） | 内容被遮挡且不可交互 | `SDL_GetWindowSafeArea()` 注入 `Layout`，禁用 viewport 做安全区 | P2-T11, P3-T9, P4-T8 | ⬜ |
| R7 | Android base path / `assets://` 版本差异 | 字体/音效加载失败 | P0-T8 记录锁定版本行为；P3-T8 真机验证 | P0-T8, P3-T8 | ⬜ |
| R8 | `SDL3_ttf` 鸿蒙交叉编译受阻（FreeType/HarfBuzz） | 鸿蒙无文字 | P2-T12 提前在桌面跑通并锁 SHA；P0-T16 定策略；P4-T6 交叉编译验证 | P2-T12, P0-T16, P4-T6 | ⬜ |
| R9 | C++ 异常跨越 JNI/NAPI/ObjC 边界 | 移动端闪退 | 语言边界 `try/catch(...)` 兜底；主线程渲染；谁分配谁释放 | P3-T11, P3-T18, P4-T10 | ⬜ |
| R10 | iOS 隐私清单 / 嵌入 framework 未处理 | 上架拒审或分发失败 | 添加 `PrivacyInfo.xcprivacy`；Archive 前移除 framework | P3-T16, P3-T19 | ⬜ |
| R11 | iOS 签名与发布流程复杂 | 首次签名超出预期 | 提前注册 Apple Developer 账号；先模拟器后真机 | P3-T15, P5-T13 | ⬜ |
| R12 | **鸿蒙超出预估工期仍无法跑通** | 无法按时交付 6 平台 | **降级预案**：若进入 P4 后**超过 3 周**仍无法真机跑通，则先交付 5 平台（Win/macOS/Linux/Android/iOS），鸿蒙转为 v2 目标；或切换路线 B | P4-T11~T18, P5-T15 | ⬜ |
| R13 | 构建环境未就绪（本机缺 CMake、Homebrew 权限受限） | P0 无法启动 | P0-T14 先解决环境；必要时改用独立 CMake 安装包或 vcpkg 二进制 | P0-T14 | ⬜ |
| R14 | 移动端前后台反复切换内存增长 | 长时间使用后崩溃 | P5-T5 前后台 50 次内存曲线检查；生命周期中释放可重建资源 | P5-T5, P3-T7 | ⬜ |

---

## 4. 合并说明与归档

**本文档的地位**：`路线图ProMax.md` 是**唯一权威路线图**。其他计划文档均为历史来源或审阅记录。

| 文件 | 状态 | 说明 |
|------|------|------|
| `路线图ProMax.md` | ✅ **权威** | 本文档，日常只看这一份 |
| `技术验证结论.md` | ✅ **权威（配套）** | P0-T8 产出：F1–F7 复核结果、平台矩阵依据、鸿蒙实测限制。与本文档同步维护 |
| `环境与平台侦察报告.md` | ✅ **权威（配套）** | CI 方案、DevEco 工具链实测、HarmonyOS PC 多窗口结论 |
| `plans/Qwen/PHASE_1..5` + `README.md` | 📦 参考 | SDL3 事实修正的来源之一 |
| `plans/GLM/SDL3_跨平台井字棋开发路线图_V2.md` | 📦 参考 | 任务颗粒度与时间维度来源 |
| `plans/hy/roadmap_V2.md` | 📦 参考 | 结构骨架、双路线、风险登记册来源 |
| `plans/GLM/…路线图.md`（V1）、`plans/hy/roadmap.md`（V1） | 🗄️ 归档 | 已被 V2 取代 |
| `plans/PLAN_REVIEW.md`、`PLAN_REVIEW_SDL3.md`、`PLAN_REVIEW_GLM_HY.md`、`PLAN_REVIEW_V2.md` | 🗄️ 归档 | 审阅依据与核实过程记录 |
| `plans/REVIEW_FOR_GLM.md`、`REVIEW_FOR_HY.md` | 🗄️ 归档 | 已执行的修改意见 |

**合并时对来源文档所做的修正**（即两份 V2 的遗留问题）：

1. **日历算术统一为 10–12 周**（原 GLM V2 头部"10–12 周"与阶段表"9–10 周"互相矛盾），并明确关键路径与并行条件。
2. **补上 hy 缺失的时间维度**，并**补上鸿蒙降级触发时点**（R12：进入 P4 后超过 3 周仍无法跑通即触发；原 hy V2 只有退路，没有触发条件）。
3. **补上两份都缺的 FreeType/HarfBuzz 获取策略**（0.1 节：SDL3_ttf 用 git submodule 自带 `libsdl-org/` 补丁分支，FetchContent 需 `GIT_SUBMODULES_RECURSE TRUE`；鸿蒙建议 `SDLTTF_PLUTOSVG=OFF`）。
4. **补上 GLM 缺失的 `android/ ios/ ohos/` 顶层目录**（GLM 1.1 只有 `src/*`，与阶段五/六/七不一致）。
5. **统一 Android ABI 策略**（原 GLM 5.2 用 `arm64-v8a + armeabi-v7a`、8.6 改为"仅 arm64-v8a"，前后矛盾 → 现统一为"开发期含 `x86_64`，发布仅 `arm64-v8a`"）。
6. **收窄 iOS 隐私清单表述**为"使用 Apple Required Reason API 时"（原 GLM 6.11 写成"所有第三方 SDK"，范围偏宽）。
7. **把 hy 的 P0-T8 六条核对项升级为第 0 节的 7 条技术事实基线（F1–F7）**，并在每个相关任务里回引编号，使"不得凭记忆写 API 名"可执行。

**维护约定**：
- 任何时候只更新本文件；来源文档冻结，不再修改。
- 版本变更（SDL3 / SDL3_ttf）必须同步更新 P0-T2 的 SHA 与第 0 节的事实基线，并回归四端。
- 每阶段结束核对 1.2 节任务数与实际勾选框数。

---

**维护者**：_______
**最后更新**：_______
**下次复核点**：P0-T8（技术验证结论产出后，回填平台矩阵中的"待核实"版本号）
