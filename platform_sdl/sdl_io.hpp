// =============================================================================
//  platform_sdl/sdl_io.hpp —— AssetIO / SaveIO 的 SDL3 实现
//
//  ▍为什么需要这两层抽象
//    core/ 里定义了 io.hpp 的两个接口，但"资源在哪、存档写哪"是平台问题：
//      · 桌面：资源在可执行文件旁的 assets/，存档在 ~/Library/...
//      · Android：资源在 APK 的 assets 里（根本没有"路径"这个说法）
//      · 鸿蒙：资源在 HAP 的 rawfile 里，需要 SDL_IOFromFile 的 assets:// 语义
//    把差异关在这一层，core/ 与渲染层就永远不用关心平台。
//
//  ▍为什么用 SDL_IOStream 而不是 fopen
//    SDL 的 IOStream 在 Android/HarmonyOS 上会自动走 RawFile/AssetManager，
//    同一行代码在六端都能读打包进应用的资源；
//    fopen 在移动端读不到打包资源，只能读真实文件系统路径。
// =============================================================================
#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <ttt/io.hpp>

namespace ttt::sdl {

// -----------------------------------------------------------------------------
// SDLAssetIO —— 只读资源（字体、音效）
//
// 构造参数是"资源根目录"，即以 '/' 结尾的基础路径（桌面为 SDL_GetBasePath()）。
// 内部会自动拼出 assets/<name>，并在路径不匹配时尝试向父目录回退几级 ——
// 这样无论是 build/bin/ 下的开发构建，还是打包后的发布目录都能找到资源。
// -----------------------------------------------------------------------------
class SDLAssetIO final : public AssetIO {
public:
    explicit SDLAssetIO(std::string baseDir);

    // 读整个文件到内存。失败返回 nullopt（调用方决定用默认资源还是报错）。
    [[nodiscard]] std::optional<std::vector<std::byte>>
    read(std::string_view name) const override;

    // 便于日志/调试：返回实际解析到的候选路径（即使文件不存在）
    [[nodiscard]] std::string lastTriedPath() const { return lastTried_; }

private:
    std::string base_;        // 以 '/' 结尾
    mutable std::string lastTried_;
};

// -----------------------------------------------------------------------------
// SDLSaveIO —— 可写目录（存档）
//
// 构造函数接收一个已存在的目录（以 '/' 结尾）。
// 通常用 create() 拿到平台推荐的"用户偏好目录"（SDL_GetPrefPath）：
//   macOS   ~/Library/Application Support/<org>/<app>/
//   Windows %APPDATA%/<org>/<app>/
//   Linux   ~/.local/share/<org>/<app>/
//   Android /data/data/<package>/files/
//   iOS     .../Documents/
// -----------------------------------------------------------------------------
class SDLSaveIO final : public SaveIO {
public:
    explicit SDLSaveIO(std::string dir);

    // 用 SDL_GetPrefPath 创建（目录不存在时 SDL 会创建）。
    // 失败返回 nullptr（例如应用无写权限）。
    static std::unique_ptr<SDLSaveIO> create(const char* org, const char* app);

    [[nodiscard]] std::string writableDir() const override { return dir_; }

    bool write(std::string_view name, std::string_view data) const override;
    [[nodiscard]] std::optional<std::string> read(std::string_view name) const override;

private:
    std::string dir_;         // 以 '/' 结尾
};

}  // namespace ttt::sdl
