// platform_sdl/main_callbacks.cpp
// 路线图 F1/F2：SDL3 main callbacks 入口。
//   - 必须 #define SDL_MAIN_USE_CALLBACKS 后再 #include <SDL3/SDL_main.h>
//   - 此模式下【不得】提供 main()（官方头文件明确要求）
//   - SDL_AppEvent 收到的是非 const SDL_Event*，便于原地做坐标换算（F4）

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>

#include <new>

#include "app.hpp"

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    auto* app = new (std::nothrow) ttt::sdl::App();
    if (app == nullptr) return SDL_APP_FAILURE;
    *appstate = app;
    const SDL_AppResult r = app->init();
    if (r != SDL_APP_CONTINUE) {
        delete app;
        *appstate = nullptr;
    }
    return r;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* app = static_cast<ttt::sdl::App*>(appstate);
    if (app == nullptr || event == nullptr) return SDL_APP_FAILURE;
    return app->event(event);
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    auto* app = static_cast<ttt::sdl::App*>(appstate);
    if (app == nullptr) return SDL_APP_FAILURE;
    return app->iterate();
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    (void)result;
    auto* app = static_cast<ttt::sdl::App*>(appstate);
    if (app == nullptr) return;
    app->quit();
    delete app;
}
