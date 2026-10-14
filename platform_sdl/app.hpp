// platform_sdl/app.hpp —— SDL3 应用外壳（main callbacks 范式）
// 路线图 F1/F2：SDL_MAIN_USE_CALLBACKS 模式下不得提供 main()；
// SDL_AppEvent 的参数是非 const SDL_Event*，便于后续原地坐标换算（F4）。
#pragma once

#include <SDL3/SDL.h>

#include <ttt/game.hpp>
#include <ttt/layout.hpp>
#include <ttt/text_id.hpp>

namespace ttt::sdl {

// 逻辑分辨率（F4：与 SDL_SetRenderLogicalPresentation 一致）
inline constexpr int kLogicalSize = 900;

class App {
public:
    SDL_AppResult init();
    SDL_AppResult event(SDL_Event* e);
    SDL_AppResult iterate();
    void quit();

private:
    void rebuildLayout();

    SDL_Window*   window_   = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    Layout        layout_{};
    Game          game_{};
    bool          dirty_    = true;   // 仅在状态变化时 present（P2-T20）
};

}  // namespace ttt::sdl
