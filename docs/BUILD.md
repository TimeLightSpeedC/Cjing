# 构建说明

本项目跨 6 平台，但**日常开发只需要关心桌面三平台**。本文件说明 CMake 选项、依赖缓存机制，以及常见的配置失败处理。

---

## 1. 依赖是怎么来的

三个第三方依赖都通过 CMake `FetchContent` 获取：

| 依赖 | 用途 | 体积 |
|------|------|------|
| **SDL3** | 窗口/渲染/输入/音频 | ~34 MB（浅克隆） |
| **SDL3_ttf** | 中文文字渲染（自带 FreeType + HarfBuzz 子模块） | ~169 MB |
| **Catch2** | 单元测试 | ~10 MB |

**关键设计：本地缓存优先。**

CMake 会先在 `${CJING_CACHE_DIR}`（默认 `<项目>/.tools`）下查找已存在的源码树：

```
.tools/
├── sdl3-src/        <- SDL3 源码（含 src/video/openharmony/）
├── sdltff-src/      <- SDL3_ttf 源码（含 external/{freetype,harfbuzz}）
└── catch2-src/      <- Catch2 源码
```

命中就直接使用并**完全跳过下载与 `git fetch`**；找不到才会联网克隆。

> **为什么要这么设计**：若 `GIT_TAG` 是分支名（本项目用 `main`，因为鸿蒙后端尚未进入任何发布版本），
> 而 CMake 判定源码"需要更新"，它会在**每次 configure** 执行 `git fetch`。
> 在网络受限的环境下，这会直接导致配置失败，典型报错：
>
> ```
> sdl3-populate-gitupdate.cmake ... 
> fatal: unable to access 'https://github.com/libsdl-org/SDL.git/':
>        Failed to connect to github.com port 443
> ```
>
> 把本地源码显式交给 `FETCHCONTENT_SOURCE_DIR_<NAME>` 就能让整个下载/更新步骤被跳过。

---

## 2. CMake 选项

| 选项 | 默认 | 说明 |
|------|------|------|
| `CJING_CACHE_DIR` | `<项目>/.tools` | 依赖缓存根目录 |
| `CJING_FETCH_SDL3` | `ON` | 关掉则改用 `find_package(SDL3 CONFIG)` |
| `CJING_WITH_TTF` | `ON` | 关掉则不构建文字渲染（界面无中文状态提示） |
| `CJING_BUILD_TESTS` | `ON` | 是否构建 Catch2 单元测试 |
| `CJING_BUILD_DESKTOP_APP` | `ON` | 是否构建 `CjingApp` |
| `CJING_BUILD_SELFTEST` | `ON` | 是否构建无界面自测 `CjingSelfTest` |
| `CJING_SANITIZE` | `OFF` | 为本项目目标启用 ASan + UBSan（不污染第三方） |
| `CJING_WERROR` | `OFF` | 告警即错误（仅本项目代码） |
| `CJING_OFFLINE` | `OFF` | 禁止联网：依赖缺失时直接报错并给出补救提示 |
| `CJING_SDL3_GIT_TAG` | `main` | SDL3 的 ref；需要完全可复现时改为具体 SHA |

---

## 3. 命令行构建

```bash
cd Cjing

# 首次配置（自动使用 .tools 中的缓存；没有则联网下载）
cmake -S . -B build

# 编译
cmake --build build --parallel

# 测试
ctest --test-dir build --output-on-failure

# 架构守卫：确保 core/ 没有混入 SDL 依赖
cmake --build build --target core_purity_check
```

**完全离线**（依赖必须已在 `.tools` 中）：

```bash
cmake -S . -B build -DCJING_OFFLINE=ON
```

**带内存/UB 检查**（推荐在提交前跑一次）：

```bash
cmake -S . -B build-asan -DCJING_SANITIZE=ON
cmake --build build-asan --parallel && ctest --test-dir build-asan
```

> macOS 上 ASan 可用，但 **LeakSanitizer 不受支持**（只有 Linux 有）。
> 因此不要设 `ASAN_OPTIONS=detect_leaks=1`，否则会提示 unsupported。

---

## 4. 在 CLion 中使用

CLion 需要 CMake 模型才能提供跳转与补全。

1. `File → Open` 选择 **`CMakeLists.txt`**（打开文件而不是目录），选 *Open as Project*
2. 打开 `Settings → Build, Execution, Deployment → CMake`
3. 确认 **Build directory**（例如 `cmake-build-debug`）与 **CMake options**

   **正常情况下不需要填任何 options** —— 依赖缓存的自动探测会处理。
   若你的依赖放在别处，再显式指定：

   ```
   -DCJING_CACHE_DIR=<你的缓存目录>
   # 或逐个指定
   -DCJING_SDL3_SOURCE_DIR=<SDL3 源码>
   -DCJING_SDL3_TTF_SOURCE_DIR=<SDL3_ttf 源码>
   -DCJING_CATCH2_SOURCE_DIR=<Catch2 源码>
   ```

4. 若之前配置失败过，先 `Tools → CMake → Reset Cache and Reload Project`，
   并删除残留的 `cmake-build-*`（里面可能有半成品克隆）。

**可用的目标（右上角配置下拉框）**：`CjingApp`、`ttt_tests`、`CjingSelfTest`、`core_purity_check`

---

## 5. 常见问题

**Q: configure 报 `Failed to connect to github.com port 443`**

说明走的是联网路径。三种处理：
1. 把源码放到 `.tools/{sdl3-src,sdltff-src,catch2-src}`
2. `-DCJING_SDL3_SOURCE_DIR=<目录>` 显式指定
3. 网络可用时重试（该连接问题是间歇性的）

**Q: 报 `Could NOT find Freetype`**

`SDLTTF_VENDORED` 没生效。本项目已在根 CMakeLists 强制设为 `ON`；
若你手工改过，请恢复 —— SDL3_ttf 必须使用自带的子模块版本，系统里通常没有 FreeType。

**Q: 改了 `.tools` 里的源码但不生效**

`FETCHCONTENT_SOURCE_DIR_*` 是缓存变量，可用 `Tools → CMake → Reset Cache` 清除后重配。

**Q: 只想跑逻辑测试，不想编译 SDL**

```bash
cmake -S . -B build-core -DCJING_FETCH_SDL3=OFF ...   # 仍需 SDL3 供 platform_sdl
```
目前 `platform_sdl` 总是被构建，若只想验证 `core/`，可直接编译 `ttt_core` 与 `ttt_tests` 两个目标。

---

## 6. 平台相关

| 平台 | 额外要求 |
|------|----------|
| Windows | MSVC 2019+ 或 MinGW-w64；`SDL_main.h` 必需（已处理） |
| macOS | Xcode Command Line Tools；部署目标 12.0+ |
| Linux | `libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev libxtst-dev libwayland-dev libdecor-0-dev libfribidi-dev libasound2-dev ...`（CI 工作流里有完整清单） |
| Android | **SDK 35+ / NDK r28c+**；产物必须是 `libmain.so` 共享库，不是可执行文件 |
| iOS | `main()` 不能位于动态加载库中（与 Android/HarmonyOS 相反） |
| HarmonyOS | **必须使用鸿蒙自带 CMake**（标准 CMake 无法识别该平台），且工程要以官方 `openharmony-project` 为模板 |

详见 [技术验证结论.md](技术验证结论.md) 与 [路线图ProMax.md](路线图ProMax.md)。

## 打包成可分发的目录（P2-T21）

```bash
cmake --build build --target dist
```

产物在 `build/dist/`，包含：

| 内容 | 说明 |
|---|---|
| `CjingApp`（或 `CjingApp.exe`） | 可执行文件 |
| `libSDL3*.dylib` / `SDL3*.dll` / `libSDL3*.so*` | 运行时动态库 |
| `assets/` | 字体与音效 |

因为库搜索路径已设为"相对于可执行文件"（macOS `@executable_path`、
Linux `$ORIGIN`、Windows 默认搜索 exe 目录），**整个目录可以直接拷到别的机器运行**，
不需要安装过程。

第三方组件与其许可证见 [THIRD_PARTY.md](THIRD_PARTY.md)。

---

## Windows 上手动构建（中文系统注意事项）

在 **"x64 Native Tools Command Prompt for VS 2022"** 中执行，不要用普通 PowerShell
（否则缺 `INCLUDE` / `LIB`，报 `stdarg.h` 或 `kernel32.lib` 找不到）。

```bat
cmake -S . -B build -G Ninja
cmake --build build --parallel
```

### 三个容易踩的坑

| 现象 | 原因 | 处理 |
|---|---|---|
| `C2001 常量中有换行符` / `C4819` | 源码是 UTF-8，MSVC 按系统代码页（中文为 936）解读 | **已在 CMake 中统一加 `/utf-8`**；若仍出现，确认已拉取最新代码 |
| 大量 `注意: 包含文件` 刷屏 | CMake 需要识别 `/showIncludes` 的本地化前缀 | 构建前 `set VSLANG=1033`（让 MSVC 输出英文） |
| `LNK1181 kernel32.lib 找不到` | 不在 VS 开发者环境 | 用 Native Tools 命令提示符，或先 `call vcvars64.bat` |

### 从 macOS 拷贝源码时

会带上 `._xxx`（AppleDouble 扩展属性文件）。SDL3 用通配符收集 `src/libm/*.c`，
会把这些二进制元数据当源码，导致 `C4821/C4828`。清理：

```powershell
Get-ChildItem -Recurse -Force -File -Filter "._*" | Remove-Item -Force
Remove-Item build -Recurse -Force   # 通配符结果在 configure 阶段生成，必须重新配置
```

### 存档位置

默认在 `%APPDATA%\TimeLightSpeedC\Cjing\save.txt`。
C 盘紧张时可以用环境变量改到别的盘：

```bat
setx CJING_SAVE_DIR "E:\Codements\save"
```

---

## 验证能力与盲区（重要）

CI 提供三类证据，但**不能替代真机**：

| 证据 | 能证明什么 | 不能证明什么 |
|---|---|---|
| 单元测试 | 逻辑正确性（规则、布局、序列化、文案表） | 任何渲染行为 |
| 离屏截图 artifact | 各平台"能画出正确画面" | **缩放相关缺陷**（见下） |
| 搬移后运行 | 运行时依赖自包含 | 交互、音频、DPI |

### 已知盲区：离屏截图固定在 scale = 1

自测把画面渲染到 **900×900 的离屏纹理**，而逻辑呈现（letterbox）只作用于窗口、
不作用于纹理目标。因此所有 CI 截图都在 **scale = 1** 下生成。

**后果**：文字双重缩放、窗口尺寸相关的比例错误，**在 CI 中不可见**，
必须通过真机改变窗口大小来验证。

> 这条盲区是靠真机发现的：Windows 上把窗口放大后，文字比棋盘长得更快并右侧溢出，
> 而 CI 截图当时完全正常。相关修复见 `textLogicalExtent()`。

---

## 静态链接（可选，`-DCJING_STATIC=ON`）

默认是**动态链接**：构建后会把 `libSDL3*.dylib` / `.so` / `.dll` 复制到可执行文件旁边。

如果希望**产物自包含**（单文件分发、不复制动态库）：

```bash
cmake -S . -B build-static -G Ninja -DCJING_STATIC=ON
cmake --build build-static
```

实测对比（macOS arm64，Release）：

| 方案 | 分发内容 | 体积 |
|---|---|---|
| 动态（默认） | `CjingApp` + `libSDL3.0.dylib` + `libSDL3_ttf.0.dylib` | 267 KB + 4.8 MB + 6.1 MB ≈ 11 MB |
| **静态** | 仅 `CjingApp` | **11 MB** |

- 体积基本持平（动态库原本也占这么多），收益是**分发简单**：一个文件即可运行；
- `otool -L` 确认不再依赖任何 SDL / FreeType 动态库；
- 静态库没有 SONAME，`cmake/CjingBundle.cmake` 会自动跳过复制步骤。

> iOS 固定为静态（`ios/CMakeLists.txt` 里的 `-DBUILD_SHARED_LIBS=OFF`），
> 原因见 `docs/构建流程对比.md`。
