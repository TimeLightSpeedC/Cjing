# 第三方依赖清单

本文件对应路线图 **P2-T21**（运行时依赖处理）。它回答两个问题：

1. 我们分发的东西里，哪些是别人的代码？
2. 它们的许可证允许我们这样做吗？

> 版本信息中的提交号来自本仓库 `.tools/` 下实际锁定的源码
> （`git -C .tools/sdl3-src rev-parse --short HEAD`）。

---

## 一、会被打包进发布物的依赖

| 组件 | 版本 | 许可证 | 用途 | 分发形式 |
|---|---|---|---|---|
| **SDL3** | 3.5.0-dev（commit `b64293b3`） | zlib | 窗口、渲染、输入、音频、文件 IO | 动态库（`SDL3.dll` / `libSDL3.0.dylib` / `libSDL3.so.0`） |
| **SDL3_ttf** | 3.3.0-dev（commit `e7cf7ce`） | zlib | TrueType 字体渲染 | 动态库（同上） |
| **FreeType** | 随 SDL3_ttf 内置 | FTL（FreeType License） | 字形光栅化 | **静态编入** SDL3_ttf |
| **HarfBuzz** | 随 SDL3_ttf 内置 | MIT | 复杂文本整形（中文排版必需） | **静态编入** SDL3_ttf |
| **plutosvg / plutovg** | 随 SDL3_ttf 内置 | MIT | 彩色 SVG 字体 | **未启用**（`SDLTTF_PLUTOSVG=OFF`） |
| **Noto Sans SC Regular** | 随仓库提供 | SIL Open Font License 1.1 | 中文显示 | 资源文件 `assets/fonts/` |

### 许可证合规要点

- **zlib / MIT / FTL** 都允许动态或静态链接、允许商业分发，
  要求是**保留版权声明与许可证文本**。发布时需随包附带本文件与各自的 LICENSE。
- **OFL 1.1** 允许随软件分发字体，但**不得单独售卖字体**，且修改后的字体
  若沿用 "Noto" 名称需遵循其保留字体名规则。我们原样分发，未做修改。
- 完整的字体许可证见 `assets/fonts/OFL.txt`。

---

## 二、只在开发/测试中使用（不进入发布物）

| 组件 | 版本 | 许可证 | 用途 |
|---|---|---|---|
| **Catch2** | v3.7.1 | BSL-1.0 | 单元测试框架 |
| **CMake / Ninja** | 构建工具 | BSD-3 / Apache-2.0 | 构建系统 |

---

## 三、我们自己生成的资源

| 文件 | 生成方式 | 版权 |
|---|---|---|
| `assets/sounds/place.wav` | Python 合成正弦波 + 指数衰减包络 | 本项目自有，随项目许可证发布 |
| `assets/sounds/win.wav` | 同上（C-E-G 三音） | 同上 |
| `assets/sounds/draw.wav` | 同上（低沉两音） | 同上 |

---

## 四、运行时依赖是怎么打包的

见 `cmake/CjingBundle.cmake`。两步：

1. **构建后复制动态库**到可执行文件旁边
   （`POST_BUILD` + `$<TARGET_SONAME_FILE:...>`；
   用 SONAME 而不是真实文件名，因为加载器找的是 SONAME）
2. **把库搜索路径改为相对可执行文件**
   - macOS：`@executable_path`
   - Linux：`$ORIGIN`
   - Windows：无需设置（系统默认先搜 exe 所在目录）

结果：`build/bin/` 整个目录可直接搬移；`cmake --build build --target dist`
会把「可执行文件 + 动态库 + assets」组装成 `build/dist/`。

**验收方式**：CI 每次构建后都会把可执行文件复制到构建目录之外再运行，
若依赖不完整会立即失败（见工作流中的「搬移后运行」步骤）。

---

## 五、已知限制

- **SDL3 与 SDL3_ttf 当前 pin 在 `main` 分支**，不是固定 tag。
  好处是能用到最新修复；代价是**构建不完全可复现**（上游一更新，行为可能变）。
  上游稳定 tag 目前是 `release-3.4.18` / `release-3.2.2`，
  而我们用的 `main` 是 3.5.0-dev —— 直接降级有 API 风险，
  因此**留到 P5 发布阶段再决定是否锁定**，届时需要完整跑一轮回归。
- 记录实际使用的提交号，是当前阶段保证可追溯性的手段（见本文开头的版本表）。
