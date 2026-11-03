#include "sdl_io.hpp"

#include <SDL3/SDL.h>

#include <cstdio>

namespace ttt::sdl {
namespace {

// 把 name 拼到 dir 后面。dir 约定以 '/' 或 '\' 结尾
// （SDL_GetBasePath / SDL_GetPrefPath 在 Windows 上返回反斜杠结尾）。
// 两种分隔符都认，避免拼出 "C:\...\App\/save.txt" 这类混合路径。
std::string joinPath(const std::string& dir, std::string_view name) {
    std::string out = dir;
    constexpr char kNativeSep = '\\';
    if (out.empty() || (out.back() != '/' && out.back() != kNativeSep)) {
        out.push_back('/');
    }
    out.append(name);
    return out;
}

}  // namespace

// -----------------------------------------------------------------------------
//  assetCandidates —— 资源候选路径（详见头文件说明）
//
//  两种 base 形态：
//    · "assets://"           包内资源 URI（Android / HarmonyOS），直接拼名字
//    · 普通目录               <base>/assets/<name>，并尝试若干父目录回退
// -----------------------------------------------------------------------------
std::vector<std::string> assetCandidates(std::string_view base, std::string_view name) {
    std::vector<std::string> out;

    // ---- 形态 1：包内资源 URI ----
    constexpr std::string_view kAssetsUri = "assets://";
    if (base.substr(0, kAssetsUri.size()) == kAssetsUri) {
        std::string candidate(base);
        if (candidate.back() != '/') candidate.push_back('/');

        // 去掉前导斜杠，避免出现 assets:///fonts/...
        std::string_view clean = name;
        while (!clean.empty() && clean.front() == '/') clean.remove_prefix(1);

        candidate.append(clean);
        out.push_back(std::move(candidate));
        return out;
    }

    // ---- 形态 2：普通文件系统目录 ----
    // 开发构建中可执行文件位于 build/bin，与源码树相差数级，故需父目录回退。
    std::string root(base);
    if (!root.empty() && root.back() != '/' && root.back() != '\\') {
        root.push_back('/');
    }

    static const char* kPrefixes[] = {"", "../..", "../../..", "../../../.."};
    for (const char* prefix : kPrefixes) {
        std::string candidate = root;
        // prefix 非空时补一个分隔符，避免拼出 "/tmp/x//assets/" 这种双斜杠
        if (prefix[0] != '\0') {
            candidate.append(prefix);
            candidate.push_back('/');
        }
        candidate.append("assets/");
        candidate.append(name);
        out.push_back(std::move(candidate));
    }
    return out;
}

// =============================================================================
//  SDLAssetIO
// =============================================================================

SDLAssetIO::SDLAssetIO(std::string baseDir) : base_(std::move(baseDir)) {
    // 包内资源 URI 必须【原样保留】：若按目录习惯裁掉尾斜杠，
    // "assets://" 会变成 "assets:/"，SDL 就不认这个 scheme 了。
    constexpr std::string_view kAssetsUri = "assets://";
    if (base_.substr(0, kAssetsUri.size()) == kAssetsUri) return;

    // 普通目录：统一成以 '/' 结尾
    while (base_.size() > 1 && (base_.back() == '/' || base_.back() == '\\')) base_.pop_back();
    if (!base_.empty()) base_.push_back('/');
}

std::optional<std::vector<std::byte>>
SDLAssetIO::read(std::string_view name) const {
    for (const std::string& candidate : assetCandidates(base_, name)) {
        lastTried_ = candidate;

        SDL_IOStream* io = SDL_IOFromFile(candidate.c_str(), "rb");
        if (io == nullptr) continue;          // 该候选不存在，试下一个

        // 先问大小，一次性读入内存。
        // GetIOSize 失败返回 -1：转成 size_t 前必须检查，否则会变成极大值。
        const Sint64 size = SDL_GetIOSize(io);
        if (size <= 0) {
            SDL_CloseIO(io);
            continue;
        }

        std::vector<std::byte> data(static_cast<std::size_t>(size));
        const size_t got = SDL_ReadIO(io, data.data(), data.size());
        SDL_CloseIO(io);

        if (got != data.size()) {
            SDL_Log("[Cjing] 资源读取不完整: %s（期望 %zu，实际 %zu）",
                    candidate.c_str(), data.size(), got);
            continue;
        }
        SDL_Log("[Cjing] 资源已加载: %s（%zu 字节）", candidate.c_str(), got);
        return data;
    }

    SDL_Log("[Cjing] 找不到资源: %s（最后一次尝试 %s）",
            std::string(name).c_str(), lastTried_.c_str());
    return std::nullopt;
}

// =============================================================================
//  SDLSaveIO
// =============================================================================

SDLSaveIO::SDLSaveIO(std::string dir) : dir_(std::move(dir)) {
    while (dir_.size() > 1 && (dir_.back() == '/' || dir_.back() == '\\')) dir_.pop_back();
    if (!dir_.empty()) dir_.push_back('/');
}

std::unique_ptr<SDLSaveIO> SDLSaveIO::create(const char* org, const char* app) {
    // SDL_GetPrefPath 返回【堆上分配的字符串】，必须用 SDL_free 释放，
    // 不能 free()（分配器可能不同）。这是 SDL 常见的所有权约定。
    char* path = SDL_GetPrefPath(org, app);
    if (path == nullptr) {
        SDL_Log("[Cjing] 无法获取可写目录: %s", SDL_GetError());
        return nullptr;
    }
    std::string dir(path);
    SDL_free(path);
    SDL_Log("[Cjing] 存档目录: %s", dir.c_str());
    return std::make_unique<SDLSaveIO>(std::move(dir));
}

bool SDLSaveIO::write(std::string_view name, std::string_view data) const {
    if (dir_.empty()) return false;

    const std::string path = joinPath(dir_, name);

    // "wb"：二进制写，会截断已有文件。
    // 存档是全量覆盖语义（每次写入完整状态），所以截断是期望行为。
    SDL_IOStream* io = SDL_IOFromFile(path.c_str(), "wb");
    if (io == nullptr) {
        SDL_Log("[Cjing] 打开存档失败（写）%s: %s", path.c_str(), SDL_GetError());
        return false;
    }

    const size_t written = SDL_WriteIO(io, data.data(), data.size());
    // 先 Close 再判断结果：CloseIO 内部会 flush，
    // 只检查 WriteIO 的返回值可能漏掉"写入缓冲区但落盘失败"的情况。
    const bool closed = SDL_CloseIO(io);

    if (written != data.size() || !closed) {
        SDL_Log("[Cjing] 存档写入不完整: %s（%zu/%zu）", path.c_str(), written, data.size());
        return false;
    }
    SDL_Log("[Cjing] 存档已写入: %s（%zu 字节）", path.c_str(), written);
    return true;
}

std::optional<std::string> SDLSaveIO::read(std::string_view name) const {
    if (dir_.empty()) return std::nullopt;

    const std::string path = joinPath(dir_, name);
    SDL_IOStream* io = SDL_IOFromFile(path.c_str(), "rb");
    if (io == nullptr) {
        // 首次运行没有存档是正常情况，不是错误 —— 用 Log 而非 Error
        SDL_Log("[Cjing] 无存档文件: %s（%s）", path.c_str(), SDL_GetError());
        return std::nullopt;
    }

    const Sint64 size = SDL_GetIOSize(io);
    if (size <= 0) {
        SDL_CloseIO(io);
        return std::nullopt;
    }

    std::string data(static_cast<std::size_t>(size), '\0');
    const size_t got = SDL_ReadIO(io, data.data(), data.size());
    SDL_CloseIO(io);

    if (got != data.size()) {
        SDL_Log("[Cjing] 存档读取不完整: %s", path.c_str());
        return std::nullopt;
    }
    return data;
}

}  // namespace ttt::sdl
