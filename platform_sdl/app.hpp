// platform_sdl/app.hpp —— SDL3 应用外壳（main callbacks 范式）
// 路线图 F1/F2：SDL_MAIN_USE_CALLBACKS 模式下不得提供 main()；
// SDL_AppEvent 的参数是非 const SDL_Event*，便于原地坐标换算（F4）。
#pragma once

#include <SDL3/SDL.h>

#include <memory>
#include <string>
#include <string_view>

#include <ttt/game.hpp>
#include <ttt/layout.hpp>

#include "sdl_io.hpp"
#include "sdl_view.hpp"

namespace ttt::sdl {

// 逻辑分辨率（F4：与 SDL_SetRenderLogicalPresentation 一致）
inline constexpr int kLogicalSize = 900;
// 顶部为状态文字预留的逻辑单位（P2-T13 接入文字后使用）
inline constexpr int kReserveTop  = 120;

// 资源与存档的文件名（集中定义，避免各处硬编码字符串）
inline constexpr std::string_view kFontAsset = "fonts/NotoSansSC-Regular.ttf";
inline constexpr std::string_view kSaveFile  = "save.txt";

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

    // 存档：把当前对局写入 SaveIO；启动时读回并恢复
    void saveNow() const;
    void loadSaved();
    // 状态行文案（UTF-8）。文案属应用逻辑，不放在渲染层。
    [[nodiscard]] std::string statusLine() const;
    [[nodiscard]] std::string hintLine() const;
    void drawHud();

    SDL_Window*   window_   = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    std::unique_ptr<SDLView> view_{};
    Layout        layout_{};
    Game          game_{};
    float         scale_    = 1.0F;   // 逻辑单位 -> 输出像素

    // 平台资源与存档（core 只定义接口，实现留在这里）
    std::unique_ptr<SDLAssetIO> assetIO_;
    std::unique_ptr<SDLSaveIO>  saveIO_;
    int           lastCell_ = -1;     // 触摸/鼠标去重（P2-T9）
};

}  // namespace ttt::sdl
