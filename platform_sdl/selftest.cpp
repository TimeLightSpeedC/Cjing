// platform_sdl/selftest.cpp —— 无界面自测：验证事件→输入→落子→渲染全链路
// 目的：在无 GUI 环境下重现"操作后卡死"类问题，并可纳入 CI。
//
// 用法: CjingSelfTest
// 退出码 0 = 全部通过；非 0 = 失败
#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>

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

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::printf("  ✗ SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    // 隐藏窗口 + 软件渲染，保证 CI 无显示器也能跑
    SDL_Window* win = SDL_CreateWindow("selftest", 800, 900,
                                       SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    check(win != nullptr, "创建隐藏窗口");
    if (win == nullptr) { SDL_Quit(); return 1; }

    SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
    check(ren != nullptr, "创建渲染器");
    if (ren == nullptr) { SDL_DestroyWindow(win); SDL_Quit(); return 1; }

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
