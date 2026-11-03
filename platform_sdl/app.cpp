#include "app.hpp"

#include <cstdio>

namespace ttt::sdl {
namespace {
// HUD 配色（与 sdl_view.cpp 的 kText* 保持一致的视觉基调）
constexpr Uint8 kHudR = 236, kHudG = 238, kHudB = 244;
constexpr Uint8 kHintR = 140, kHintG = 144, kHintB = 156;
}  // namespace

void App::refreshScale() {
    // 逻辑单位 -> 渲染输出像素 的换算（唯一读取像素尺寸的地方）
    // 以渲染器的【实际】缩放为准，而不是自己按宽度推算。
    // 原因：LETTERBOX 取两个方向里较小的缩放比；只按宽度算，
    // 窗口偏扁时（如 1600x900）会得到偏大的值，文字就会比棋盘更大。
    float sx = 1.0F, sy = 1.0F;
    float next = 1.0F;
    if (SDL_GetRenderScale(renderer_, &sx, &sy) && sx > 0.0F) {
        next = sx;
    } else {
        int outW = 0, outH = 0;
        SDL_GetRenderOutputSize(renderer_, &outW, &outH);
        const int shorter = (outW < outH) ? outW : outH;
        next = (shorter > 0) ? static_cast<float>(shorter) / static_cast<float>(kLogicalSize)
                             : 1.0F;
    }

    // 首次调用：创建视图并加载字体（只发生一次）
    if (view_ == nullptr) {
        scale_ = next;
        view_ = std::make_unique<SDLView>(renderer_, scale_, *assetIO_);
        if (assetIO_) {
            (void)view_->loadFont(kFontAsset, 44);
        }
        return;
    }

    // 后续 resize：只更新缩放，不重建视图、不重读字体。
    // setScale 内部会判等，所以密集的 resize 事件里绝大多次是空操作。
    const float old = scale_;
    if (view_->setScale(next)) {
        scale_ = next;
        SDL_Log("[Cjing] 缩放变化 %.3f -> %.3f，字号已重设（字体未重新读取）",
                static_cast<double>(old), static_cast<double>(next));
    }
}

SDL_AppResult App::init() {
    // ---------------------------------------------------------------------
    // 触摸/鼠标去重（P2-T9 / P3-T10）：必须在 SDL_Init 之前设置。
    //
    // SDL_HINT_TOUCH_MOUSE_EVENTS 默认 "1"：触摸会合成鼠标事件。
    // 我们采用【方案 A】——只处理鼠标事件，显式忽略 SDL_EVENT_FINGER_*，
    // 这样桌面鼠标与移动端触摸走完全相同的代码路径，不会一次触摸落两子。
    //
    // 为什么放在 Init 之前：
    //   SDL 在鼠标子系统初始化时通过 SDL_AddHintCallback 注册回调，
    //   之后再 SDL_SetHint 虽然也会触发回调（实测 SDL_SetHintWithPriority
    //   会同步派发），但那依赖"注册早于设置"的隐含时序，容易在版本变化时失效。
    //   在初始化前设置是唯一不依赖时序的写法。
    // ---------------------------------------------------------------------
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");

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

    // 诊断：打印一次布局与缩放，便于在真机/模拟器上核对"棋盘是否正方形"
    {
        int ww = 0, wh = 0, ow = 0, oh = 0;
        float rsx = 0.0f, rsy = 0.0f;
        SDL_GetWindowSize(window_, &ww, &wh);
        SDL_GetCurrentRenderOutputSize(renderer_, &ow, &oh);
        SDL_GetRenderScale(renderer_, &rsx, &rsy);
        const Recti& br = layout_.boardRect();
        SDL_Log("[Cjing] 布局: window=%dx%d output=%dx%d 渲染缩放=%.3fx%.3f 棋盘=%dx%d@(%d,%d) 格子=%d scale_=%.3f",
                ww, wh, ow, oh, rsx, rsy, br.w, br.h, br.x, br.y,
                layout_.cellSize(), static_cast<double>(scale_));
    }


    // 是否"没有实体键盘"的平台：决定提示文案（不能对手机说"按 R"）。
    //
    // 判据用**视频驱动**而不是"有没有触摸设备"：
    //   带触摸屏的 Windows 笔记本既有触摸也有键盘，R/ESC 依然可用，
    //   若按触摸设备判定会错误地隐藏键盘提示。
    //   而 android / uikit / openharmony 这三个驱动才是真正的移动平台。
    {
        const char* driver = SDL_GetCurrentVideoDriver();
        const bool mobile = (driver != nullptr) &&
                            (SDL_strcmp(driver, "android") == 0 ||
                             SDL_strcmp(driver, "uikit") == 0 ||
                             SDL_strcmp(driver, "openharmony") == 0);

        int touchCount = 0;
        SDL_TouchID* devs = SDL_GetTouchDevices(&touchCount);   // 堆分配，需 SDL_free
        if (devs != nullptr) SDL_free(devs);

        touchMode_ = mobile;
        SDL_Log("[Cjing] 输入模式: %s（视频驱动 %s，触摸设备 %d 个）",
                touchMode_ ? "触屏" : "键盘/鼠标",
                (driver != nullptr) ? driver : "?", touchCount);
    }

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
    // F6/P2-T11：安全区注入 Layout（Android 15+ / 刘海屏必做）
    //
    // ⚠️ 这里不能把"安全区像素"直接除以缩放就当作逻辑内缩。
    // 启用逻辑呈现（LETTERBOX）后，900x900 的逻辑方形被居中放进窗口，
    // 竖屏手机上状态栏/手势条都位于这个方形【之外】——此时内缩应为 0。
    // 早期实现忽略了 letterbox 偏移，导致棋盘被压小、文字起始点右移后溢出。
    //
    // 正确做法：取"安全区"与"逻辑呈现区域"的交集，
    // 只有真正遮挡到逻辑区域的那部分才换算成内缩。
    SDL_Rect safe{0, 0, 0, 0};
    SDL_GetWindowSafeArea(window_, &safe);

    SDL_FRect present{0, 0, 0, 0};   // 逻辑呈现区域（输出像素坐标）
    if (!SDL_GetRenderLogicalPresentationRect(renderer_, &present) ||
        present.w <= 0.0F || present.h <= 0.0F) {
        // 拿不到就退化为"整个窗口即逻辑区域"
        int ow = 0, oh = 0;
        SDL_GetRenderOutputSize(renderer_, &ow, &oh);
        present = SDL_FRect{0.0F, 0.0F, static_cast<float>(ow), static_cast<float>(oh)};
    }

    const float pxPerLogical = present.w / static_cast<float>(kLogicalSize);
    const float k = (pxPerLogical > 0.0F) ? (1.0F / pxPerLogical) : 1.0F;

    auto clamp0 = [](float v) -> int {
        return (v > 0.0F) ? static_cast<int>(v) : 0;
    };

    const int insetLeft   = clamp0((safe.x - present.x) * k);
    const int insetTop    = clamp0((safe.y - present.y) * k);
    const int insetRight  = clamp0(((present.x + present.w) - (safe.x + safe.w)) * k);
    const int insetBottom = clamp0(((present.y + present.h) - (safe.y + safe.h)) * k);

    layout_ = Layout::compute(kLogicalSize, insetTop, insetRight, insetBottom, insetLeft,
                              kReserveTop);
}

void App::handlePointer(int renderX, int renderY) {
    // ---- 触屏的重开入口：双击提示行 ----
    //
    // 为什么需要它：移动端没有实体键盘，R / ESC 这两条提示无法执行。
    // 桌面端保留键盘快捷键，同时这里也生效（双击无副作用）。
    //
    // 命中带取"提示行所在的水平条"，而不是精确到每个字：
    // 文字宽度随语言变化，按字做命中区既脆弱又没必要；
    // 这条带子位于棋盘上方，不会与落子区域重叠。
    {
        const Recti& b = layout_.boardRect();
        const Layout::HudLines hud = layout_.hudLines();
        if (b.w > 0) {
            const int top    = hud.hintY - 8;
            const int bottom = hud.hintY + 56;      // 44pt 行高 + 余量
            const int left   = b.x;
            const int right  = kLogicalSize;        // 文案可能比棋盘宽，右侧放开

            if (renderY >= top && renderY <= bottom && renderX >= left && renderX <= right) {
                const Tap now{SDL_GetTicks(), renderX, renderY, true};
                if (isDoubleTap(lastTap_, now)) {
                    game_.reset();
                    lastCell_ = -1;
                    lastTap_  = Tap{};
                    SDL_Log("[Cjing] 双击提示行 → 重开");
                } else {
                    lastTap_ = now;
                }
                return;   // 提示行区域不参与落子
            }
        }
    }

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
            return SDL_APP_CONTINUE;

        // 低内存：释放可重建资源，并把对局落盘。
        // 为什么顺手存档：系统发这个事件时，进程随时可能被回收，
        // 而我们无法保证之后还有机会收到 TERMINATING。
        case SDL_EVENT_LOW_MEMORY:
            SDL_Log("[Cjing] LOW_MEMORY → 释放缓存并保存对局");
            if (view_) view_->releaseCachedResources();
            saveNow();
            return SDL_APP_CONTINUE;

        // 渲染设备重置（GPU 驱动重置 / 显卡切换 / 移动端上下文丢失）：
        // 所有已创建的纹理都失效，必须丢弃并重建。
        // 渲染器对象本身仍有效，不需要重建。
        case SDL_EVENT_RENDER_DEVICE_RESET:
            SDL_Log("[Cjing] RENDER_DEVICE_RESET → 重建渲染资源");
            if (view_) view_->onRenderDeviceReset();
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
#ifndef NDEBUG
            } else if (e->key.key == SDLK_F9 || e->key.key == SDLK_F10) {
                // 【仅 Debug】合成事件，用于在桌面上验证移动端才有的处理逻辑。
                // 原因：macOS 后端不会自然产生 LOW_MEMORY / RENDER_DEVICE_RESET，
                // 没有触发器就无法验证 P2-T10 的实现。
                // 用 SDL_PushEvent 而不是直接调用处理函数：这样走的是真实事件循环，
                // 能顺带验证"事件能否被正确投递与分发"。
                SDL_Event synth{};
                synth.type = (e->key.key == SDLK_F9) ? SDL_EVENT_LOW_MEMORY
                                                     : SDL_EVENT_RENDER_DEVICE_RESET;
                if (SDL_PushEvent(&synth)) {
                    SDL_Log("[Cjing] 已合成事件 type=%u（方式：F9=低内存 / F10=设备重置）",
                            synth.type);
                }
#endif
            }
            return SDL_APP_CONTINUE;

        default:
            return SDL_APP_CONTINUE;
    }
}

TextId App::statusText() const {
    // 状态机 -> 文案 ID 的映射。视图负责把 ID 变成字符串，
    // 应用层不再出现任何用户可见的中文字面量。
    const GameState& st = game_.state();
    switch (st.status) {
        case Status::XWins: return TextId::XWins;
        case Status::OWins: return TextId::OWins;
        case Status::Draw:  return TextId::Draw;
        case Status::Playing:
        default:            return (st.current == Player::X) ? TextId::TurnX : TextId::TurnO;
    }
}

TextId App::hintText() const {
    // 触屏平台用"双击这行字"的文案：手机上没有 R 键可按。
    const bool over = game_.state().isOver();
    if (touchMode_) {
        return over ? TextId::HintOverTouch : TextId::HintPlayingTouch;
    }
    return over ? TextId::HintOver : TextId::HintPlaying;
}

void App::drawHud() {
    if (!view_) return;
    const Recti& b = layout_.boardRect();
    if (b.w <= 0) return;
    // 两行的位置由 Layout 统一给出：自测截图用的是同一份计算，
    // 避免出现"CI 截图与真机画面间距不一致"这类只有肉眼能发现的偏差。
    const Layout::HudLines hud = layout_.hudLines();
    view_->drawText(statusText(), b.x, hud.statusY, kHudR, kHudG, kHudB);
    view_->drawText(hintText(), b.x, hud.hintY, kHintR, kHintG, kHintB);
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
