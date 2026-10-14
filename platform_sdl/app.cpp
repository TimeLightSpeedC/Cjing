#include "app.hpp"

#include <cstdio>

namespace ttt::sdl {

SDL_AppResult App::init() {
    // SDL3: SDL_Init 返回 bool（true = 成功），不再是 0（路线图已验证 API）
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "[Cjing] SDL_Init 失败: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    window_ = SDL_CreateWindow("Tic-Tac-Toe", 800, 800, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window_ == nullptr) {
        std::fprintf(stderr, "[Cjing] 创建窗口失败: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (renderer_ == nullptr) {
        std::fprintf(stderr, "[Cjing] 创建渲染器失败: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // F4/P2-T2：逻辑分辨率 + letterbox —— DPI 与窗口缩放全交给 SDL
    SDL_SetRenderLogicalPresentation(renderer_, kLogicalSize, kLogicalSize,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    rebuildLayout();

    SDL_Log("Cjing %s | SDL %d.%d.%d", "0.1.0",
            SDL_VERSIONNUM_MAJOR(SDL_GetVersion()),
            SDL_VERSIONNUM_MINOR(SDL_GetVersion()),
            SDL_VERSIONNUM_MICRO(SDL_GetVersion()));
    return SDL_APP_CONTINUE;
}

void App::rebuildLayout() {
    // F6/P2-T11：安全区注入 Layout（Android 15+ / 刘海屏必做）
    SDL_Rect safe{0, 0, 0, 0};
    SDL_GetWindowSafeArea(window_, &safe);

    // 安全区以窗口像素给出；换算为逻辑单位（按渲染输出宽度等比）
    int outW = 0, outH = 0;
    SDL_GetRenderOutputSize(renderer_, &outW, &outH);
    const float scale = (outW > 0) ? static_cast<float>(kLogicalSize) / static_cast<float>(outW) : 1.0f;

    const int insetTop    = static_cast<int>(static_cast<float>(safe.y) * scale);
    const int insetLeft   = static_cast<int>(static_cast<float>(safe.x) * scale);
    const int insetBottom = static_cast<int>(static_cast<float>(outH - safe.y - safe.h) * scale);
    const int insetRight  = static_cast<int>(static_cast<float>(outW - safe.x - safe.w) * scale);

    layout_ = Layout::compute(kLogicalSize, insetTop, insetRight, insetBottom, insetLeft);
    dirty_  = true;
}

SDL_AppResult App::event(SDL_Event* e) {
    switch (e->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;

        // P2-T5：窗口尺寸/DPI 变化时重算布局
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
            rebuildLayout();
            return SDL_APP_CONTINUE;

        // F3：这些是应用生命周期事件（切后台/恢复），移动端必需；
        //     SDL_EVENT_WINDOW_MINIMIZED 是桌面窗口事件，不能用于此目的
        case SDL_EVENT_WILL_ENTER_BACKGROUND:
            SDL_Log("[Cjing] WILL_ENTER_BACKGROUND → 此处落盘存档并停止渲染（P3-T7）");
            return SDL_APP_CONTINUE;
        case SDL_EVENT_DID_ENTER_BACKGROUND:
        case SDL_EVENT_WILL_ENTER_FOREGROUND:
        case SDL_EVENT_DID_ENTER_FOREGROUND:
        case SDL_EVENT_TERMINATING:
        case SDL_EVENT_LOW_MEMORY:
            return SDL_APP_CONTINUE;
        case SDL_EVENT_RENDER_DEVICE_RESET:
            return SDL_APP_CONTINUE;

        // P2-T7/P2-T8：指针输入 —— 先换算到渲染坐标，再交给 Layout
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (e->button.button != SDL_BUTTON_LEFT) return SDL_APP_CONTINUE;
            // F4：唯一换算点。触摸事件同样经此路径（F5：触摸会合成鼠标事件）
            SDL_ConvertEventToRenderCoordinates(renderer_, e);
            const int cell = layout_.cellAt(static_cast<int>(e->button.x),
                                           static_cast<int>(e->button.y));
            if (cell >= 0) {
                const MoveResult r = game_.applyMove(cell);
                if (r != MoveResult::Rejected) dirty_ = true;
            }
            return SDL_APP_CONTINUE;
        }

        case SDL_EVENT_KEY_DOWN:
            if (e->key.key == SDLK_R) { game_.reset(); dirty_ = true; }
            return SDL_APP_CONTINUE;

        default:
            return SDL_APP_CONTINUE;
    }
}

SDL_AppResult App::iterate() {
    // 空帧循环：P0 只验证渲染器可用（后续 P2 绘制棋盘）
    SDL_SetRenderDrawColor(renderer_, 24, 24, 28, 255);
    SDL_RenderClear(renderer_);

    if (dirty_) {
        SDL_RenderPresent(renderer_);   // P2-T20：仅状态变化时呈现
        dirty_ = false;
    }
    return SDL_APP_CONTINUE;
}

void App::quit() {
    if (renderer_ != nullptr) SDL_DestroyRenderer(renderer_);
    if (window_ != nullptr)   SDL_DestroyWindow(window_);
    SDL_Quit();
    renderer_ = nullptr;
    window_   = nullptr;
}

}  // namespace ttt::sdl
