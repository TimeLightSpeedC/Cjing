# iOS 工程（P3-T14 ~ T19）

## 设计选择：源码静态链接，不用 xcframework

SDL 官方推荐 `SDL3.xcframework` + Xcode 工程。本项目选择**从源码构建**：

| 方案 | 本项目为何不选/选 |
|---|---|
| xcframework + Xcode 工程 | 需要 Xcode GUI/工程文件；分发前还要**移除嵌入的 framework**（T19 的负担） |
| **源码静态链接（本方案）** | 与桌面/Android 共用同一份源码与 CMake；Ninja 即可构建；无嵌入 framework 问题 |

## 构建

```bash
cmake -S ios -B build-ios-sim -G Ninja \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphonesimulator \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
  -DCMAKE_C_COMPILER="$(xcrun -find clang)" \
  -DCMAKE_CXX_COMPILER="$(xcrun -find clang++)" \
  -DBUILD_SHARED_LIBS=OFF

cmake --build build-ios-sim
```

产物：`build-ios-sim/cjing.app`

## 安装到模拟器

```bash
xcrun simctl boot "iPhone 17 Pro"
xcrun simctl install booted build-ios-sim/cjing.app
xcrun simctl launch booted com.timelightspeed.cjing
xcrun simctl io booted screenshot /tmp/ios.png
```

## 两个必须知道的坑

### 1. SDL_ttf 的 GL 文本引擎在 iOS 上编译不过

`SDL_opengl.h` 在 iOS 上整份是空的（源码注释：*No OpenGL on iOS*），
而 SDL_ttf 无条件编译 `src/SDL_gl_textengine.c` → 缺少 `GLuint` 等类型。

本项目只用 surface 渲染路径（`TTF_RenderText_Blended`），
因此在 `CMakeLists.txt` 里把该文件从目标源列表剔除（不改 vendored 源码）：

```cmake
list(FILTER _srcs EXCLUDE REGEX "SDL_gl_textengine\\.c$")
```

### 2. 安全区单位：点 vs 像素

- `SDL_GetWindowSafeArea()` 在 iOS 返回**点**（402×874）
- 逻辑呈现矩形是**像素**（1206×1206）

直接相减会把底部内缩算成 800+，可用高度变负 → 棋盘 0×0。
必须先按 `ppu = 呈现宽度 / 窗口宽度` 换算到像素空间（见 `rebuildLayout`）。

## 已验证（iPhone 17 Pro 模拟器 / iOS 26.5）

![iOS 模拟器运行画面](../docs/images/ios-simulator.png)

| 项 | 结果 |
|---|---|
| 构建 | ✅ CMake + Ninja，无 Xcode 工程 |
| 启动 | ✅ 进程存活，SDL 视频驱动 `uikit` |
| 中文字体 | ✅ 从 `.app/assets/fonts/` 加载 |
| 音效 | ✅ 三个 WAV 均加载 |
| 布局 | ✅ 棋盘 684×684（正方形），DPI 3x 下文字清晰 |
| 触屏文案 | ✅ 自动显示「双击这行字重开」 |

## 待办（T15~T19 剩余）

- 签名：需要你的 Apple 开发者账号（Team ID），模拟器不需要
- `PrivacyInfo.xcprivacy` 隐私清单（T16）
- Archive / IPA 导出验证（T19）
