# 更新日志

## v0.2.0（2026-11-02）—— 桌面可玩可分发 + 鸿蒙预检 Go

这是第一个"可分发、有移动端可行性结论"的版本。

### 桌面端（Windows / macOS / Linux）

**可玩**
- 完整对局规则：8 条获胜线、平局、非法落子拒绝、终局锁定
- 鼠标与触摸统一处理（触摸合成鼠标事件 + 显式去重）
- 中文状态栏与提示（文案经 `TextId → 文案表` 查询，无硬编码）
- 获胜连线高亮、落子/胜利/平局音效（SDL 核心音频，WAV 随包）
- 切后台与退出时自动存档，启动时恢复（`SDL_GetPrefPath` 平台目录）

**可分发**
- `cmake --build build --target dist` 产出「可执行文件 + 动态库 + assets」目录，
  库搜索路径改为相对可执行文件（`@executable_path` / `$ORIGIN`），整目录可搬移
- 第三方依赖与许可证清单：`docs/THIRD_PARTY.md`

**质量**
- 单元测试 236 断言 / 34 用例（规则、布局、序列化、文案表完整性）
- 无界面自测：离屏渲染像素校验、存档往返、音频加载、字形覆盖（无缺字）、坐标换算
- CI：Ubuntu / macOS / Windows 三平台构建 + 测试 + 离屏截图 artifact，
  并有一道"搬移后运行"验证运行时依赖自包含

### 平台适配与已修复的关键问题

- **中文 Windows 编译失败**：源码为 UTF-8，MSVC 按系统代码页（936）解读导致
  `C2001`；统一加 `/utf-8` 与 `/EHsc`
- **文字双重缩放**：字体按输出像素栅格化后又被逻辑呈现放大一次，
  导致窗口放大时文字溢出、缩小时过小；引入 `textLogicalExtent()` 折回逻辑单位
- **缩放时重复读字体**：拖动窗口每次都重读 10.5MB 字体并重新解析；
  改为 `TTF_SetFontSize` 增量更新
- **HUD 定位两套实现**：正式程序与自测截图间距不一致；统一到 `Layout::hudLines()`

### HarmonyOS（预检结论：**Go**）

- 基于 SDL 官方 `openharmony-project` 模板搭建预检工程 `platform_ohos/precheck`
- 交叉编译 SDL3 / SDL3_ttf 通过；hap 打包成功（双架构 .so + 中文字体）
- **真机 API 26：窗口显示与中文渲染均正常**
- 模拟器不可用，根因在模拟器侧：`libdgles`（GLES→Vulkan 翻译层）在
  `replace_external_shader_source` 中崩溃（Vulkan 驱动不兼容）
- 结论：鸿蒙移植路线可行，后续以真机为验收环境

### 已知限制

- SDL3 / SDL3_ttf 仍固定在 `main` 分支（3.5.0-dev / 3.3.0-dev），构建不完全可复现；
  留待 P5 发布阶段决策
- 移动端（Android / iOS）尚未开始
