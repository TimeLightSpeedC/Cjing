# Android 工程（P3）

基于 SDL 官方 `android-project` 模板改造。**原生库名为 `libmain.so`**，
由 `MainActivity`（继承 `org.libsdl.app.SDLActivity`）加载。

---

## 一次性准备

```bash
./android/setup.sh
```

它做两件事：

1. 把 `.tools/sdl3-src`、`.tools/sdltff-src` 软链接到 `app/jni/`
2. 把 `assets/` 复制到 `app/src/main/assets/`（字体与音效随 APK 打包）

> 软链接与 assets 副本都不入库（见 `.gitignore`）。

---

## 环境要求（P3-T2）

| 组件 | 版本 |
|---|---|
| JDK | 17 |
| Android SDK | 35 |
| NDK | 28.2.13676358 |
| ABI（开发期） | `arm64-v8a`（真机）+ `x86_64`（模拟器） |
| 最低 API | 21 |

发布版只保留 `arm64-v8a`（与 P5 打包一致）。

---

## 构建

```bash
cd android
./gradlew assembleDebug
```

产物：`android/app/build/outputs/apk/debug/app-debug.apk`

安装到设备：

```bash
adb install -r android/app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.timelightspeed.cjing/.MainActivity
```

---

## 资源路径（重要差异，已修正）

Android 上 `SDL_GetBasePath()` 返回 **`"assets://"`** —— 与 HarmonyOS 相同。

早期路线图记录为"返回 `./`、不支持 assets://"，**该结论已过时**：
SDL3 源码里 `assets://` 同时覆盖 `SDL_PLATFORM_ANDROID` 与 `SDL_PLATFORM_OPENHARMONY`。

因此资源路径规则是：

```
base = "assets://", name = "fonts/a.ttf"  ->  "assets://fonts/a.ttf"
```

不能再插一层 `assets/`（那会变成 `assets:///assets/...`）。
`SDLAssetIO` 已按此修正，并有单元测试锁定（见 `platform_sdl/selftest.cpp`）。

---

## 构建验证

本地无 Android SDK 时，可依赖 CI：
`.github/workflows/android.yml` 会在 Ubuntu 上安装 SDK 35 + NDK 28.2，
执行 `setup.sh` 与 `./gradlew assembleDebug`，并上传 APK 作为 artifact。
