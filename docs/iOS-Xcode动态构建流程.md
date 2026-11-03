# 用 Xcode 从源码做 iOS 动态构建（官方风格流程）

> 本文描述的是一条**可选**路线：用 Xcode 工程 + 动态 framework。
> 本项目默认走的是「CMake + Ninja + 静态链接」（见 `ios/README.md`），
> 两条路并存：静态用于 CI 与日常发布，动态用于对齐官方、上架与 Xcode 调试。
>
> ⚠️ 说明：本文步骤依据 SDL 官方 `docs/README-ios.md` 与 Xcode 常规用法整理。
> 在编写时的环境里，Xcode 的构建产物目录受沙箱限制无法写入，
> 因此**动态这条路线尚未在本仓库端到端验证**；静态路线已实测通过。

---

## 0. 与仓库现有方案的区别

| | 静态（本项目默认） | 动态（本文） |
|---|---|---|
| 谁编译 SDL | CMake 随项目一起编译 | 先用 Xcode 编出 `SDL3.xcframework` |
| 产物形态 | `.a` 静态库合并进可执行文件 | `.framework` 动态库，运行时由 dyld 加载 |
| 是否嵌入 | 否 | **是**（Embed & Sign） |
| 分发前处理 | 无 | **必须移除嵌入的 framework** |
| 适合 | CI、日常迭代、单文件分发 | 对齐官方、上架、Xcode 内调试 |

---

## 1. 构建 SDL3.xcframework

```bash
cd /Users/c/Desktop/MacMiniTest/Cjing

xcodebuild -project .tools/sdl3-src/Xcode/SDL/SDL.xcodeproj \
  -scheme SDL3.xcframework \
  -configuration Release \
  -derivedDataPath build-xcframework \
  build
```

要点：

- 这个 scheme 会**分别**为真机（ios-arm64）与模拟器（ios-arm64_x86_64-simulator）
  各 archive 一次，再合并成 xcframework；首次约 5–15 分钟。
- 产物位置：`build-xcframework/Build/Products/Release/SDL3.xcframework`
  （不加 `-derivedDataPath` 时，则出现在工程旁边的 `Products/` 目录）。
- 不要用 `CODE_SIGNING_ALLOWED=NO` 之外的花样：framework 本身不需要签名，
  签名发生在"嵌入到你的 app"时。
- 也可以用 SDL 官方发布的 dmg 里的现成 xcframework，
  但那样版本不受本仓库锁定的 SHA 控制（P0 的可复现原则会失效）。

## 2. 构建 SDL3_ttf.xcframework

```bash
xcodebuild -project .tools/sdltff-src/Xcode/SDL_ttf.xcodeproj \
  -scheme SDL3_ttf.xcframework \
  -configuration Release \
  -derivedDataPath build-xcframework \
  build
```

SDL_ttf 依赖 SDL3 的头文件。若报找不到 `SDL3/SDL.h`，
在工程的 *Build Settings → Framework Search Paths* 里加上 SDL3.xcframework 的路径。

## 3. 新建 Xcode 应用工程

1. Xcode → *File → New → Project* → **iOS App**（或官方示例用的 Game 模板）；
2. Product Name：`Cjing`；Bundle Identifier：`com.timelightspeed.cjing`；
3. Deployment Target：**16.0**；只勾 **Portrait**；
4. 不需要 Core Data / Tests。

## 4. 加入 framework 并设置 Embed & Sign

1. 把 `SDL3.xcframework` 与 `SDL3_ttf.xcframework` 拖进工程；
2. 在 target 的 *General → Frameworks, Libraries, and Embedded Content* 中，
   两者都设为 **Embed & Sign**；
3. *Build Settings → Framework Search Paths* 加上 xcframework 所在目录，并设为 **recursive**；
4. 官方 README 提醒：把 `*.framework` 从 *Sub-Directories to Exclude in Recursive Searches* 里移除，
   否则头文件可能搜不到。

## 5. 加入源码

把本仓库的源码以 group / folder reference 加入 target：

```
core/src/*.cpp          game.cpp layout.cpp io.cpp text.cpp tap.cpp
platform_sdl/*.cpp      app.cpp main_callbacks.cpp sdl_view.cpp sdl_io.cpp sdl_audio.cpp
```

头文件搜索路径：

```
core/include
platform_sdl
```

**入口注意**：本项目用 `SDL_MAIN_USE_CALLBACKS`，
`main_callbacks.cpp` 里已经 `#define SDL_MAIN_USE_CALLBACKS` 并 `#include <SDL3/SDL_main.h>`，
**不要再自己写 `main()`**（写了会重复定义 `SDL_main`）。

## 6. 加入资源

把 `assets/` 作为 **folder reference**（蓝色文件夹）加入 target，
这样它会原样出现在 `.app/assets/` 下，与 `SDL_GetBasePath()` 的约定一致：

```
Cjng.app/assets/fonts/NotoSansSC-Regular.ttf
Cjng.app/assets/sounds/*.wav
```

## 7. Info.plist

必须包含：

```xml
<key>UILaunchScreen</key><dict/>
```

否则系统会按旧兼容模式运行、分辨率受限（官方 README 明确提到）。

其余：`CFBundleDisplayName`、`UISupportedInterfaceOrientations`（仅 Portrait）、
`MinimumOSVersion` 与 Deployment Target 一致。

## 8. 运行与手动测试

1. 选模拟器（如 iPhone 17 Pro）→ `Cmd + R`；
2. 手动测试点：
   - 点击格子落子
   - **双击提示行**重开（触屏文案会自动出现，因为视频驱动是 `uikit`）
   - `Cmd + Shift + H` 回主屏 → 再进来，验证存档恢复
   - 旋转设备，确认仍是竖屏
3. 看日志：`SDL_Log` 输出到 Xcode 的 Debug Console。

## 9. 真机

- target → *Signing & Capabilities* → 选 Team（自动管理证书）；
- 设备开启开发者模式并信任证书；
- 首次运行可能需要 `sudo xcodebuild -license accept`。

## 10. 分发（Archive 前的必要步骤）

官方 README（line 65）明确说明：嵌入的 framework 会被带进 Archive，
**导致分发失败**，因此在 *Embed & Sign* 之后需要一个 **Run Script 阶段**移除它：

```sh
# 示例：从打包内容中移除嵌入的 SDL framework
rm -rf "${TARGET_BUILD_DIR}/${FRAMEWORKS_FOLDER_PATH}/SDL3.framework"
rm -rf "${TARGET_BUILD_DIR}/${FRAMEWORKS_FOLDER_PATH}/SDL3_ttf.framework"
```

> 移除后应用仍然能跑，是因为 Archive 阶段代码已经链接完成；
> 这一步只为避免 framework 被当作"需签名分发的嵌入库"。

之后：*Product → Archive* → *Distribute App* → 导出 IPA。

---

## 常见错误对照

| 现象 | 原因 | 处理 |
|---|---|---|
| `GLuint` 未定义 | `SDL_opengl.h` 在 iOS 为空，SDL_ttf 仍编译 GL 文本引擎 | 不要用源码整体编译 SDL_ttf（本文用 xcframework 规避）；静态路线见 `ios/CMakeLists.txt` |
| `SDL_main` 重复定义 | 自己写了 `main()` 又包含 `SDL_main.h` | 删掉自定义 `main` |
| `SDL3/SDL.h` 找不到 | Framework Search Paths 没设或没递归 | 加路径 + recursive |
| Archive 分发失败 | 嵌入的 framework 未移除 | 按第 10 步加脚本 |
| 启动黑屏 / 分辨率异常 | 缺 `UILaunchScreen` | 补 Info.plist |

---

## 两条路的取舍建议

- **日常开发与 CI**：用静态（`./scripts/ios-xcode.sh` 生成工程，或 Ninja 构建）；
- **需要对齐官方/上架**：按本文走动态；
- 两者的业务代码完全相同（`core/` + `platform_sdl/`），只是"怎么编进 app"不同。
