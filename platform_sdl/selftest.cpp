// platform_sdl/selftest.cpp —— 无界面自测：验证事件→输入→落子→渲染全链路
// 目的：在无 GUI 环境下重现"操作后卡死"类问题，并可纳入 CI。
//
// 用法: CjingSelfTest
// 退出码 0 = 全部通过；非 0 = 失败
#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>

#ifndef _WIN32
#include <cstdlib>   // setenv
#endif

#include <ttt/game.hpp>
#include <ttt/layout.hpp>

#include "sdl_view.hpp"

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    std::printf("  %s %s\n", ok ? "✓" : "✗", what);
    if (!ok) ++g_failures;
}

}  // namespace

int main() {
    std::printf("=== Cjing 自测（无界面）===\n");

    // SDL 会把视频驱动名缓存进 hint 系统，仅改环境变量不足以覆盖，
    // 必须用 SDL_HINT_OVERRIDE 强制，并在重试前 SDL_Quit。
    auto forceDummyDriver = [] {
#ifndef _WIN32
        setenv("SDL_VIDEODRIVER", "dummy", 1);
#else
        _putenv_s("SDL_VIDEODRIVER", "dummy");
#endif
        SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    };

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        // 指定的视频驱动不可用（如 CI 上强制 x11 但无 $DISPLAY）
        std::printf("  · SDL_Init 失败（%s），回退到 dummy 驱动\n", SDL_GetError());
        SDL_Quit();
        forceDummyDriver();
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            std::printf("  ✗ dummy 驱动初始化失败: %s\n", SDL_GetError());
            return 1;
        }
    }

    // 隐藏窗口（CI 的 Linux runner 无显示器）。
    // 若当前视频驱动不可用（无 DISPLAY / WAYLAND_DISPLAY），自动回退到 dummy 驱动，
    // 使本测试可在任何环境运行，无需 CI 特判。
    auto createHidden = [] {
        return SDL_CreateWindow("selftest", 800, 900,
                                SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    };
    SDL_Window* win = createHidden();
    if (win == nullptr) {
        std::printf("  · 当前视频驱动不可用（%s），回退到 dummy 驱动\n", SDL_GetError());
        // 环境变量 SDL_VIDEODRIVER 的优先级高于 hint，故直接改写环境变量
        forceDummyDriver();
        SDL_Quit();
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            std::printf("  ✗ dummy 驱动初始化失败: %s\n", SDL_GetError());
            return 1;
        }
        win = createHidden();
    }
    check(win != nullptr, "创建隐藏窗口");
    if (win == nullptr) {
        std::printf("  ✗ 无法创建窗口: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
    if (ren == nullptr) {
        // dummy 驱动可能不提供渲染器：跳过渲染相关检查，但仍验证布局与逻辑
        std::printf("  · 渲染器不可用（%s），跳过渲染检查\n", SDL_GetError());
        ttt::Layout l = ttt::Layout::compute(900, 0, 0, 0, 0, 120);
        check(l.cellSize() > 0, "布局计算（无渲染器环境）");
        bool allOk = true;
        for (int i = 0; i < ttt::kCellCount; ++i) { int cx=0, cy=0; if(!l.cellCenter(i,cx,cy) || l.cellAt(cx,cy)!=i) { allOk=false; break; } }
        check(allOk, "布局往返映射（无渲染器环境）");
        SDL_DestroyWindow(win);
        SDL_Quit();
        std::printf("=== %s（跳过渲染部分）===\n", g_failures == 0 ? "通过" : "存在失败");
        return g_failures == 0 ? 0 : 1;
    }
    check(ren != nullptr, "创建渲染器");

    SDL_SetRenderLogicalPresentation(ren, 900, 900, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    // ---- 布局 ----
    const ttt::Layout layout = ttt::Layout::compute(900, 0, 0, 0, 0, 120);
    check(layout.cellSize() > 0, "布局计算：格子尺寸 > 0");

    // ---- 渲染一帧（含全部绘制路径）----
    {
        ttt::sdl::SDLView view(ren, 1.0F, ".");
        ttt::Game game;
        view.beginFrame();
        view.drawBoard(layout);
        view.drawPieces(game.state(), layout);
        view.drawWinningLine(game.state(), layout);
        view.drawTextAt("轮到 X", 10, 10, 255, 255, 255);   // 无字体时应安全空转
        view.present();
        check(true, "空棋盘渲染一帧未卡死");

        // 落满一局（X 获胜）后再渲染，覆盖 X/O 与连线路径
        for (int c : {0, 3, 1, 4, 2}) game.applyMove(c);
        view.beginFrame();
        view.drawBoard(layout);
        view.drawPieces(game.state(), layout);
        view.drawWinningLine(game.state(), layout);
        view.present();
        check(game.status() == ttt::Status::XWins, "对局推进到 X 获胜");
        check(true, "获胜局面渲染一帧未卡死");
    }

    // ---- 坐标换算链路（复现输入路径）----
    {
        bool allOk = true;
        for (int i = 0; i < ttt::kCellCount; ++i) {
            int cx = 0, cy = 0;
            if (!layout.cellCenter(i, cx, cy)) { allOk = false; break; }
            // 构造合成鼠标事件并做坐标换算（与 App::event 相同的调用）
            SDL_Event ev{};
            ev.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            ev.button.button = SDL_BUTTON_LEFT;
            ev.button.x = static_cast<float>(cx);
            ev.button.y = static_cast<float>(cy);
            if (!SDL_ConvertEventToRenderCoordinates(ren, &ev)) { allOk = false; break; }
            if (layout.cellAt(static_cast<int>(ev.button.x),
                              static_cast<int>(ev.button.y)) != i) { allOk = false; break; }
        }
        check(allOk, "合成鼠标事件经坐标换算后命中正确格子（9 格全测）");
    }

    // ---- 事件循环不应挂起 ----
    {
        SDL_Event ev{};
        int polls = 0;
        while (SDL_PollEvent(&ev)) { ++polls; if (polls > 1000) break; }
        check(polls <= 1000, "事件队列可被排空（无无限事件）");
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    std::printf("=== %s（失败 %d 项）===\n", g_failures == 0 ? "全部通过" : "存在失败", g_failures);
    return g_failures == 0 ? 0 : 1;
}
