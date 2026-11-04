#include "ohos_log.hpp"

#ifdef __OHOS__

#include <SDL3/SDL.h>

#include <hilog/log.h>

namespace ttt::sdl {
namespace {

constexpr unsigned int kDomain = 0x0000;      // 鸿蒙 domain 限 0x0000–0xFFFF，越界会被丢弃
constexpr const char*  kTag    = "Cjing";

// SDL 日志优先级 -> hilog 级别
LogLevel toOhosLevel(SDL_LogPriority p) {
    switch (p) {
        case SDL_LOG_PRIORITY_VERBOSE: return LOG_DEBUG;
        case SDL_LOG_PRIORITY_DEBUG:   return LOG_DEBUG;
        case SDL_LOG_PRIORITY_INFO:    return LOG_INFO;
        case SDL_LOG_PRIORITY_WARN:    return LOG_WARN;
        case SDL_LOG_PRIORITY_ERROR:   return LOG_ERROR;
        case SDL_LOG_PRIORITY_CRITICAL:return LOG_FATAL;
        default:                       return LOG_INFO;
    }
}

void SDLCALL ohosLogOutput(void* /*userdata*/, int /*category*/,
                           SDL_LogPriority priority, const char* message) {
    // %{public}s：让 hilog 原样输出，不做隐私脱敏（否则中文与参数会被打码）
    OH_LOG_Print(LOG_APP, toOhosLevel(priority), kDomain, kTag, "%{public}s", message);
}

}  // namespace

void installOhosLog() {
    SDL_SetLogOutputFunction(ohosLogOutput, nullptr);
}

}  // namespace ttt::sdl

#else

namespace ttt::sdl {
void installOhosLog() {}
}  // namespace ttt::sdl

#endif
