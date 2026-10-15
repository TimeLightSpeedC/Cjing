#include "app.hpp"

#include <cstdio>

namespace ttt::sdl {
namespace {
// HUD 配色（与 sdl_view.cpp 的 kText* 保持一致的视觉基调）
constexpr Uint8 kHudR = 236, kHudG = 238, kHudB = 244;
constexpr Uint8 kHintR = 140, kHintG = 144, kHintB = 156;
}  // namespace

void App::refreshScale() {
    // 逻辑单位 -> 渲染输出像素 的换算（唯一读取像素尺寸的地方，供视图用）
    int outW = 0, outH = 0;
    SDL_GetRenderOutputSize(renderer_, &outW, &outH);
    scale_ = (outW > 0) ? static_cast<float>(outW) / static_cast<float>(kLogicalSize) : 1.0F;
    const bool hadFont = (view_ != nullptr && view_->hasFont());
    view_.reset();                     // 缩放变化后重建视图（字体随缩放重新栅格化）
    view_ = std::make_unique<SDLView>(renderer_, scale_, assetBase_);
    if (hadFont && !assetBase_.empty()) {
        view_->loadFont("fonts/NotoSansSC-Regular.ttf", 44);
    }
}

SDL_AppResult App::init() {
    // SDL3: SDL_Init 返回 bool（true = 成功），不再是 0
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "[Cjing] SDL_Init 失败: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    window_ = SDL_CreateWindow("Tic-Tac-Toe", 800, 900,
                               SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
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

    // P2-T9 触摸/鼠标去重：选定【方案 A】—— 只处理鼠标事件。
    // SDL_HINT_TOUCH_MOUSE_EVENTS 默认为 "1"（触摸会合成鼠标事件），
    // 因此移动端触摸与桌面鼠标走完全相同的代码路径；
    // 同时【显式忽略】 SDL_EVENT_FINGER_*，避免一次触摸落两子。
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");

    // 资源根目录：桌面为可执行文件所在目录；移动端由 SDL 给出 assets:// 语义
    const char* base = SDL_GetBasePath();
    assetBase_ = (base != nullptr) ? base : ".";

    refreshScale();
    rebuildLayout();

    // P2-T12/T13：加载字体（SDL3_ttf）。失败不致命——胜负仍由连线高亮表达。
    if (view_) {
        if (!view_->loadFont("fonts/NotoSansSC-Regular.ttf", 44)) {
            SDL_Log("[Cjing] 字体不可用，将以几何方式表达状态");
        }
    }

    SDL_Log("Cjing %s | SDL %d.%d.%d | 逻辑分辨率 %d",
            "0.2.0",
            SDL_VERSIONNUM_MAJOR(SDL_GetVersion()),
            SDL_VERSIONNUM_MINOR(SDL_GetVersion()),
            SDL_VERSIONNUM_MICRO(SDL_GetVersion()),
            kLogicalSize);
    return SDL_APP_CONTINUE;
}

void App::rebuildLayout() {
    // F6/P2-T11：安全区注入 Layout（Android 15+ / 刘海屏必做，PC 端为窗口装饰留白）
    SDL_Rect safe{0, 0, 0, 0};
    SDL_GetWindowSafeArea(window_, &safe);

    int outW = 0, outH = 0;
    SDL_GetRenderOutputSize(renderer_, &outW, &outH);
    const float s = (outW > 0) ? static_cast<float>(kLogicalSize) / static_cast<float>(outW) : 1.0F;

    const int insetTop    = static_cast<int>(static_cast<float>(safe.y) * s);
    const int insetLeft   = static_cast<int>(static_cast<float>(safe.x) * s);
    const int insetBottom = static_cast<int>(static_cast<float>(outH - safe.y - safe.h) * s);
    const int insetRight  = static_cast<int>(static_cast<float>(outW - safe.x - safe.w) * s);

    layout_ = Layout::compute(kLogicalSize, insetTop, insetRight, insetBottom, insetLeft,
                              kReserveTop);
    dirty_  = true;
}

void App::handlePointer(int renderX, int renderY) {
    const int cell = layout_.cellAt(renderX, renderY);
    if (cell < 0) return;

    // P2-T9 去重：同一次指针交互只允许产生一次落子意图。
    // 触摸会同时产生 finger 与合成鼠标事件（F5），此外还可能重复投递；
    // 这里以"同一格子未抬起前不重复落子"作为守卫。
    if (cell == lastCell_) return;
    lastCell_ = cell;

    const MoveResult r = game_.applyMove(cell);
    if (r != MoveResult::Rejected) dirty_ = true;
}

SDL_AppResult App::event(SDL_Event* e) {
    switch (e->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;

        // P2-T5：窗口尺寸/DPI 变化时重算布局与缩放
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
            refreshScale();
            rebuildLayout();
            return SDL_APP_CONTINUE;

        // F5/P2-T9：显式忽略触摸事件（方案 A：触摸已由 SDL 合成鼠标事件）
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_CANCELED:
            return SDL_APP_CONTINUE;

        // F3：应用生命周期事件（移动端切后台必需）。
        //     SDL_EVENT_WINDOW_MINIMIZED 是桌面窗口事件，不能用于此目的。
        case SDL_EVENT_WILL_ENTER_BACKGROUND:
            SDL_Log("[Cjing] WILL_ENTER_BACKGROUND → 此处落盘存档并停止渲染（P3-T7）");
            return SDL_APP_CONTINUE;
        case SDL_EVENT_DID_ENTER_BACKGROUND:
        case SDL_EVENT_WILL_ENTER_FOREGROUND:
        case SDL_EVENT_DID_ENTER_FOREGROUND:
        case SDL_EVENT_TERMINATING:
        case SDL_EVENT_LOW_MEMORY:
        case SDL_EVENT_RENDER_DEVICE_RESET:
            return SDL_APP_CONTINUE;

        // P2-T7/P2-T8：指针输入 —— 先换算到渲染坐标，再交给 Layout
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (e->button.button != SDL_BUTTON_LEFT) return SDL_APP_CONTINUE;
            // F4：唯一换算点（含 letterbox 换算）。触摸经此路径同样处理。
            SDL_ConvertEventToRenderCoordinates(renderer_, e);
            handlePointer(static_cast<int>(e->button.x), static_cast<int>(e->button.y));
            return SDL_APP_CONTINUE;
        }

        case SDL_EVENT_MOUSE_BUTTON_UP:
            lastCell_ = -1;   // 抬起后允许在同一格再次落子（新的一次交互）
            return SDL_APP_CONTINUE;

        // P2-T19：键盘
        case SDL_EVENT_KEY_DOWN:
            if (e->key.key == SDLK_R) {
                game_.reset();
                lastCell_ = -1;
                dirty_    = true;
            } else if (e->key.key == SDLK_ESCAPE) {
                return SDL_APP_SUCCESS;
            }
            return SDL_APP_CONTINUE;

        default:
            return SDL_APP_CONTINUE;
    }
}

std::string App::statusLine() const {
    const GameState& st = game_.state();
    switch (st.status) {
        case Status::XWins: return "X 获胜！";
        case Status::OWins: return "O 获胜！";
        case Status::Draw:  return "平局";
        case Status::Playing:
        default:
            return (st.current == Player::X) ? "轮到 X" : "轮到 O";
    }
}

std::string App::hintLine() const {
    return game_.state().isOver() ? "按 R 再来一局 · ESC 退出"
                                  : "点击棋盘落子 · R 重开 · ESC 退出";
}

void App::drawHud() {
    if (!view_) return;
    const Recti& b = layout_.boardRect();
    if (b.w <= 0) return;
    // 状态区位于棋盘上方预留空间内
    view_->drawTextAt(statusLine(), b.x, b.y / 3, kHudR, kHudG, kHudB);
    view_->drawTextAt(hintLine(), b.x, b.y / 3 + b.y / 3, kHintR, kHintG, kHintB);
}

SDL_AppResult App::iterate() {
    if (view_) {
        view_->beginFrame();
        view_->drawBoard(layout_);
        view_->drawPieces(game_.state(), layout_);
        view_->drawWinningLine(game_.state(), layout_);
        drawHud();
        if (dirty_) {
            view_->present();          // P2-T20：仅状态变化时呈现
            dirty_ = false;
        }
    }
    return SDL_APP_CONTINUE;
}

void App::quit() {
    view_.reset();
    if (renderer_ != nullptr) SDL_DestroyRenderer(renderer_);
    if (window_ != nullptr)   SDL_DestroyWindow(window_);
    SDL_Quit();
    renderer_ = nullptr;
    window_   = nullptr;
}

}  // namespace ttt::sdl
