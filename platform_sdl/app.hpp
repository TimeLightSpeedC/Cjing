// platform_sdl/app.hpp —— SDL3 应用外壳（main callbacks 范式）
// 路线图 F1/F2：SDL_MAIN_USE_CALLBACKS 模式下不得提供 main()；
// SDL_AppEvent 的参数是非 const SDL_Event*，便于原地坐标换算（F4）。
#pragma once

#include <SDL3/SDL.h>

#include <memory>

#include <ttt/game.hpp>
#include <ttt/layout.hpp>

#include "sdl_view.hpp"

namespace ttt::sdl {

// 逻辑分辨率（F4：与 SDL_SetRenderLogicalPresentation 一致）
inline constexpr int kLogicalSize = 900;
// 顶部为状态文字预留的逻辑单位（P2-T13 接入文字后使用）
inline constexpr int kReserveTop  = 120;

class App {
public:
    App() = default;

    SDL_AppResult init();
    SDL_AppResult event(SDL_Event* e);
    SDL_AppResult iterate();
    void quit();

private:
    void rebuildLayout();
    void refreshScale();
    void handlePointer(int renderX, int renderY);

    SDL_Window*   window_   = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    std::unique_ptr<SDLView> view_{};
    Layout        layout_{};
    Game          game_{};
    float         scale_    = 1.0F;   // 逻辑单位 -> 输出像素
    int           lastCell_ = -1;     // 触摸/鼠标去重（P2-T9）
    bool          dirty_    = true;   // 仅在状态变化时 present（P2-T20）
};

}  // namespace ttt::sdl
