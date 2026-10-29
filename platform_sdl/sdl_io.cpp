#include "sdl_io.hpp"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstring>

namespace ttt::sdl {
namespace {

// 把 name 拼到 dir 后面。dir 约定以 '/' 结尾（SDL_GetBasePath/GetPrefPath 都是），
// 但仍做一次判断，避免调用方传入不带斜杠的路径时拼出 "dirassets/..."。
std::string joinPath(const std::string& dir, std::string_view name) {
    std::string out = dir;
    // Windows 的 SDL_GetPrefPath / SDL_GetBasePath 返回的是带反斜杠的路径
    // （如 C:\Users\x\AppData\Roaming\Org\App\）。
    // 这里两种分隔符都认，避免拼出 "C:\...\App\/save.txt" 这种混合路径
    // —— 虽然 Win32 能接受，但日志里看着别扭，也容易误导排查。
    constexpr char kNativeSep = '\\';   // 在 C++ 源码里是单个反斜杠
    if (out.empty() || (out.back() != '/' && out.back() != kNativeSep)) {
        out.push_back('/');
    }
    out.append(name);
    return out;
}

}  // namespace

// =============================================================================
//  SDLAssetIO
// =============================================================================

SDLAssetIO::SDLAssetIO(std::string baseDir) : base_(std::move(baseDir)) {
    // 规范化：确保以 '/' 结尾，内部拼接逻辑就不用反复判断
    while (base_.size() > 1 && base_.back() == '/') base_.pop_back();
    if (!base_.empty()) base_.push_back('/');
}

std::optional<std::vector<std::byte>>
SDLAssetIO::read(std::string_view name) const {
    // 资源定位：先试 assets/<name>，找不到再向父目录回退几级。
    //
    // 为什么需要回退：开发时资源在项目根的 assets/，
    // 而可执行文件在 build/bin/ —— 两者相差两级目录。
    // 打包后资源通常就在可执行文件旁边，第一级就能命中。
    // 这段逻辑集中在这里，渲染层/逻辑层都不必关心目录结构。
    static const char* kPrefixes[] = {"", "../..", "../../..", "../../../.."};

    for (const char* prefix : kPrefixes) {
        std::string candidate = base_;
        candidate.append(prefix);
        candidate.append("/assets/");
        candidate.append(name);
        lastTried_ = candidate;

        SDL_IOStream* io = SDL_IOFromFile(candidate.c_str(), "rb");
        if (io == nullptr) continue;          // 该候选不存在，试下一个

        // 先问大小，一次性读入内存。
        // GetIOSize 失败时返回 -1：转成 size_t 前必须检查，否则会变成极大值。
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

    SDL_Log("[Cjing] 找不到资源: assets/%s（最后一次尝试 %s）",
            std::string(name).c_str(), lastTried_.c_str());
    return std::nullopt;
}

// =============================================================================
//  SDLSaveIO
// =============================================================================

SDLSaveIO::SDLSaveIO(std::string dir) : dir_(std::move(dir)) {
    while (dir_.size() > 1 && dir_.back() == '/') dir_.pop_back();
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
