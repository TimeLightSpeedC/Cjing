# HarmonyOS 正式工程

从 `precheck/` 进化而来：预检工程验证了「平台能跑 SDL + TTF + 中文」，
本工程把**产品源码**接进来。

## 与 Android 的相似性（经验直接复用）

| 方面 | 做法 |
|---|---|
| 产物 | **共享库 `libmain.so`**（不是可执行文件） |
| 加载方式 | SDL 的鸿蒙入口 `dlopen("libmain.so")` 后找 `SDL_main` / 回调 |
| SDL 依赖 | `add_subdirectory(SDL)` 源码编译，随包打入 |
| 资源 | `resources/rawfile/`，运行时 `SDL_GetBasePath()` 返回 `assets://` |
| 工具链 | CMake + 官方 SDK（DevEco） |

## 准备

```bash
./platform_ohos/app/setup.sh
```

做两件事：

1. 把 `.tools/sdl3-src`、`.tools/sdltff-src` 软链到 `entry/src/main/cpp/`
2. 把 `assets/` 复制到 `entry/src/main/resources/rawfile/`

> 软链与 rawfile 副本都不入库（见 `.gitignore`）。

## 构建

```bash
cd platform_ohos/app
export JAVA_HOME=/Applications/DevEco-Studio.app/Contents/jbr/Contents/Home
export DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk
/Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw assembleHap \
  --mode module -p product=default -p buildMode=debug --no-daemon
```

产物：`entry/build/default/outputs/default/entry-default-unsigned.hap`

> **签名暂未配置**：`build-profile.json5` 里已移除 `"signingConfig": "default"`，
> 调试阶段用未签名 HAP 直接 `hdc install` 即可（预检阶段已验证可行）。
> 正式签名在 DevEco Studio 的 *Project Structure → Signing Configs* 生成，
> 与 Android/iOS 统一在 P3 收尾时处理。

## 安装与运行

```bash
HDC=/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc
$HDC install entry/build/default/outputs/default/entry-default-unsigned.hap
$HDC shell aa start -a EntryAbility -b com.timelightspeed.cjing
```

真机需要：**USB 连接 + 解锁 + 允许调试**。

## 日志与截图

```bash
$HDC hilog | grep -E "\[Cjing\]|SDL"
$HDC shell snapshot_display -f /data/local/tmp/shot.jpeg
$HDC file recv /data/local/tmp/shot.jpeg ./shot.jpeg
```

## 验证结果（模拟器 API 26 + 真机，2026-11-04）

![鸿蒙运行画面](../../docs/images/ohos-product.png)

| 项 | 结果 | 证据 |
|---|---|---|
| 构建 | ✅ | `hvigorw assembleHap` → unsigned HAP 27MB |
| 安装 | ✅ | 未签名 HAP 可安装（真机则要求签名，见下） |
| 中文渲染 | ✅ | 画面「轮到 X / 点击棋盘落子 · 双击这行字重开」 |
| 棋盘布局 | ✅ | 正方形，居中，无裁切 |
| 触摸落子 | ✅ | `uitest uiInput click` 后 X 落在左上格，状态转「轮到 O」 |
| 双击重开 | ✅ | 两次点击提示行后棋盘清空（截图 `ohos-restart.png`） |
| 切后台存档 | ✅ | `files/Cjing/save.txt` 内容 `TTT1,X........,O,1` |
| 冷启动恢复 | ✅ | 重启后 X 与「轮到 O」正确恢复 |
| 音效 | ⚠️ 未确认 | 应用不依赖音频，缺失时静音运行 |
| 安全区 | ⚠️ 未确认 | 鸿蒙后端会调用 `SDL_SetWindowSafeAreaInsets`，当前画面未见遮挡 |

### 真机验证（Mate 系列，`6BEUT26106000176`）

![真机运行画面](../../docs/images/ohos-real-device.png)

| 项 | 结果 | 证据 |
|---|---|---|
| 签名安装 | ✅ | `entry-default-signed.hap` 安装成功 |
| 渲染 | ✅ | 中文、正方形棋盘、1320×2856 全屏 |
| 触摸落子 | ✅ | `uitest uiInput click` → X 落左上格 |
| 双击重开 | ✅ | 点两次提示行 → 棋盘清空 |
| 切后台存档 | ✅ | `files/Cjing/save.txt` = `TTT1,X........,O,1` |
| 冷启动恢复 | ✅ | 重启后 X 与「轮到 O」都在 |

## ⚠️ 签名配置的坑（出现过两次，务必注意）

**症状**：在 DevEco 里点了 *Automatically generate signature*，构建仍报
「未检测到签名配置 / The bundleName ... does not match」。

**根因**：`build-profile.json5` 里 **product 必须显式引用签名配置**：

```json5
"products": [
  {
    "name": "default",
    "signingConfig": "default",   // ← 少了这一行就报「未检测到签名配置」
    ...
  }
]
```

`signingConfigs` 里即使有完整材料（证书路径、密码），**没有这一行引用也不会生效**。

**排查步骤**：

1. 确认 product 里有 `"signingConfig": "default"`
2. 确认 `signingConfigs[0].material` 指向的 `.p12/.cer/.p7b` 存在（`~/.ohos/config/`）
3. 确认 profile 里的 `bundle-name` 与 `AppScope/app.json5` 的 `bundleName` 一致：
   ```bash
   strings ~/.ohos/config/default_app_*.p7b | grep bundle-name
   ```
4. 包名改过之后，旧签名材料会因 bundleName 不匹配而失败，需在 DevEco 里**重新生成**

> 签名材料（含加密后的密码）是**机器相关**的，提交在仓库里只为让本机可复现；
> 其他开发者应各自在 DevEco 生成，或把 `signingConfigs` 段替换为自己的材料。

## 调试提示

- 应用日志：`hdc shell hilog -x | grep -iE "cjing|SDL"`（SDL 的 `SDL_Log` 不一定进 hilog）
- 触摸注入：`hdc shell uitest uiInput click <x> <y>`
- 按键注入：`hdc shell uitest uiInput keyEvent Home`
- 截图：`hdc shell snapshot_display -f /data/local/tmp/s.jpeg` + `hdc file recv`
- 存档路径：`/data/app/el2/100/base/com.timelightspeed.cjing/files/Cjing/save.txt`
