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
    // 缩放变化需重建视图：字体按新缩放重新栅格化，保证高分屏清晰
    const bool hadFont = (view_ != nullptr && view_->hasFont());
    view_.reset();
    view_ = std::make_unique<SDLView>(renderer_, scale_, *assetIO_);
    if (assetIO_) {
        (void)view_->loadFont(kFontAsset, 44);
        if (hadFont) SDL_Log("[Cjing] 缩放变化，字体已按新比例重新栅格化");
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

    // 显式启用 vsync（每帧等待显示器刷新）。
    // 实测（60Hz 显示器）：不显式设置时循环会跑到约 119fps，等于多画一倍；
    // 设成 1 后稳定在 63fps 左右，CPU 占用也随之下降。
    // 注意：某些后端/无显示器环境可能设置失败，此时仅记录日志，不影响运行。
    if (!SDL_SetRenderVSync(renderer_, 1)) {
        SDL_Log("[Cjing] 无法启用 vsync（%s），将依赖 present 自身的节流", SDL_GetError());
    }

    // P2-T9 触摸/鼠标去重：选定【方案 A】—— 只处理鼠标事件。
    // SDL_HINT_TOUCH_MOUSE_EVENTS 默认为 "1"（触摸会合成鼠标事件），
    // 因此移动端触摸与桌面鼠标走完全相同的代码路径；
    // 同时【显式忽略】 SDL_EVENT_FINGER_*，避免一次触摸落两子。
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");

    // ---- 资源访问：根目录由 SDL 给出（桌面是可执行文件目录，移动端为 assets://）----
    const char* base = SDL_GetBasePath();
    assetIO_ = std::make_unique<SDLAssetIO>((base != nullptr) ? base : "");

    // ---- 存档访问：默认用平台推荐的"用户偏好目录"（无需额外权限）----
    //
    // 若设置了环境变量 CJING_SAVE_DIR，则优先使用它。用途：
    //   · 便携模式（存档放在程序旁边）
    //   · 自动化测试（避免污染用户目录、也便于在受限环境验证）
    if (const char* overrideDir = SDL_getenv("CJING_SAVE_DIR")) {
        SDL_Log("[Cjing] 使用 CJING_SAVE_DIR 覆盖存档目录: %s", overrideDir);
        saveIO_ = std::make_unique<SDLSaveIO>(overrideDir);
    } else {
        saveIO_ = SDLSaveIO::create("TimeLightSpeedC", "Cjing");
    }

    refreshScale();
    rebuildLayout();

    // ---- 音频（可选能力）----
    // create() 失败返回 nullptr（无设备/子系统初始化失败），此时 play() 不会被调用，
    // 游戏照常运行 —— 音频不该成为启动的硬依赖。
    audio_ = SDLAudio::create(*assetIO_);
    if (audio_) {
        audio_->load(SoundId::Place, "sounds/place.wav");
        audio_->load(SoundId::Win,   "sounds/win.wav");
        audio_->load(SoundId::Draw,  "sounds/draw.wav");
    } else {
        SDL_Log("[Cjing] 音频不可用，将安静运行");
    }

    // 启动时尝试恢复上一局。没有存档 / 存档损坏都只是"从新局开始"，不是错误。
    loadSaved();

    // P2-T12/T13：字体已由 refreshScale() 创建视图时加载。
    // 失败不致命——胜负仍由连线高亮表达。
    if (view_ && !view_->hasFont()) {
        SDL_Log("[Cjing] 字体不可用，将以几何方式表达状态");
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
    if (r == MoveResult::Rejected) return;      // 非法落子：无声、无状态变化

    // 音效映射集中在这里（应用层知道"什么时候该响什么"）。
    // core 不参与：它只返回 MoveResult，不关心声音。
    if (audio_) {
        audio_->play(SoundId::Place);
        if (r == MoveResult::XWon || r == MoveResult::OWon) {
            audio_->play(SoundId::Win);
        } else if (r == MoveResult::Drawn) {
            audio_->play(SoundId::Draw);
        }
    }
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
        // 进入后台：必须在这里落盘 —— Android 之后可能不再给应用运行时间，
        // 甚至直接回收进程。这是移动端"局面丢失"的根因所在。
        case SDL_EVENT_WILL_ENTER_BACKGROUND:
            SDL_Log("[Cjing] WILL_ENTER_BACKGROUND → 保存对局");
            saveNow();
            return SDL_APP_CONTINUE;

        // 即将被系统终止：最后一次保存机会
        case SDL_EVENT_TERMINATING:
            SDL_Log("[Cjing] TERMINATING → 保存对局");
            saveNow();
            return SDL_APP_CONTINUE;

        case SDL_EVENT_DID_ENTER_BACKGROUND:
        case SDL_EVENT_WILL_ENTER_FOREGROUND:
        case SDL_EVENT_DID_ENTER_FOREGROUND:
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
    // 状态区位于棋盘上方预留空间内：状态行靠上，提示行其下，与棋盘留出间距
    const int statusY = (b.y * 12) / 100;
    const int lineH   = (b.y * 38) / 100;
    view_->drawTextAt(statusLine(), b.x, statusY, kHudR, kHudG, kHudB);
    view_->drawTextAt(hintLine(), b.x, statusY + lineH, kHintR, kHintG, kHintB);
}

SDL_AppResult App::iterate() {
    if (!view_) return SDL_APP_CONTINUE;

    // 【每帧全量重绘 + 每帧 present】
    //
    // 为什么不能"只在状态变化时重绘"（两次踩坑，均已实测）：
    //  1. 跳过 present → SDL 的 vsync 节流失效，主循环满速空转（实测 20 万 fps），
    //     CPU 占满、事件饿死，macOS 表现为彩虹圈（应用无响应）。
    //  2. 跳过重绘但仍 present → 渲染器是双缓冲的，每帧交换到另一块后备缓冲；
    //     跳过清屏与绘制就会 present 一块空白/陈旧缓冲，窗口在"有内容/黑屏"之间交替，
    //     表现为黑屏、偶发闪烁残影。
    //
    // 对井字棋这种绘制量（几十条线 + 少量文字），全量重绘的代价可以忽略。
    // 若将来需要"按需重绘"，必须改为渲染到持久化的离屏纹理，再整帧贴出；
    // 直接跳过绘制是不可行的。
    view_->beginFrame();
    view_->drawBoard(layout_);
    view_->drawPieces(game_.state(), layout_);
    view_->drawWinningLine(game_.state(), layout_);
    drawHud();
    view_->present();
    return SDL_APP_CONTINUE;
}

// -----------------------------------------------------------------------------
//  saveNow —— 把当前对局写入存档
//
//  存档内容只有棋盘 + 当前玩家 + 手数（见 core/src/io.cpp）：
//  胜负状态不存，恢复时由棋盘推导，避免"两份真相"不一致。
// -----------------------------------------------------------------------------
void App::saveNow() const {
    if (!saveIO_) return;

    const std::string data = serialize(game_.state());
    if (!saveIO_->write(kSaveFile, data)) {
        SDL_Log("[Cjing] 存档失败（不影响继续游戏）");
        return;
    }
    SDL_Log("[Cjing] 已保存: %s（%s）",
            std::string(kSaveFile).c_str(), data.c_str());
}

// -----------------------------------------------------------------------------
//  loadSaved —— 启动时恢复对局
//
//  三种情况都视为正常：
//    ① 没有存档文件（首次运行）
//    ② 存档损坏（被截断/被改过）
//    ③ 恢复成功
//  只有 ③ 才改变局面；其余静默从新局开始 —— 不该因为一个坏文件让游戏起不来。
// -----------------------------------------------------------------------------
void App::loadSaved() {
    if (!saveIO_) return;

    const auto raw = saveIO_->read(kSaveFile);
    if (!raw.has_value()) return;              // ① 无存档

    const auto restored = deserialize(*raw);
    if (!restored.has_value()) {               // ② 存档损坏
        SDL_Log("[Cjing] 存档无法解析，忽略并开始新局: %s", raw->c_str());
        return;
    }

    game_.restore(*restored);                  // ③ 恢复
    SDL_Log("[Cjing] 已恢复对局: 手数=%d，状态=%d", game_.moveCount(),
            static_cast<int>(game_.status()));
}

void App::quit() {
    // 正常退出也保存一次（ESC / 关窗口走这里）
    saveNow();

    // 音频要先于 SDL_Quit 释放：析构里会关闭设备并退出音频子系统
    audio_.reset();

    view_.reset();
    if (renderer_ != nullptr) SDL_DestroyRenderer(renderer_);
    if (window_ != nullptr)   SDL_DestroyWindow(window_);
    SDL_Quit();
    renderer_ = nullptr;
    window_   = nullptr;
}

}  // namespace ttt::sdl
