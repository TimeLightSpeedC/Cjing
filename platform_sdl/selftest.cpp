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

#include "sdl_audio.hpp"
#include "sdl_io.hpp"
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

    // 资源访问抽象。基目录给当前工作目录即可 —— SDLAssetIO 内部会向上回退几级，
    // 因此无论从项目根还是 build/bin 运行都能找到 assets/。
    const ttt::sdl::SDLAssetIO assets(".");

    // ---- 布局 ----
    const ttt::Layout layout = ttt::Layout::compute(900, 0, 0, 0, 0, 120);
    check(layout.cellSize() > 0, "布局计算：格子尺寸 > 0");

    // ---- 渲染一帧（含全部绘制路径）----
    {
        ttt::sdl::SDLView view(ren, 1.0F, assets);
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

    // ---- 像素级验证：离屏渲染并读回，确认画面真的画出了内容 ----
    // 这是"黑屏/只闪一下"类问题的回归测试：仅验证"没崩溃"是不够的。
    {
        SDL_Texture* target = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888,
                                                SDL_TEXTUREACCESS_TARGET, 900, 900);
        if (target == nullptr) {
            std::printf("  · 渲染目标不可用（%s），跳过像素验证\n", SDL_GetError());
        } else if (!SDL_SetRenderTarget(ren, target)) {
            std::printf("  · 无法切换到离屏渲染目标（%s），跳过像素验证\n", SDL_GetError());
            SDL_DestroyTexture(target);
        } else {
            ttt::sdl::SDLView view(ren, 1.0F, assets);
            ttt::Game game;
            game.applyMove(0);          // 放一个 X，确保有前景内容
            view.beginFrame();
            view.drawBoard(layout);
            view.drawPieces(game.state(), layout);
            view.drawWinningLine(game.state(), layout);

            SDL_Surface* shot = SDL_RenderReadPixels(ren, nullptr);
            if (shot == nullptr) {
                std::printf("  · 读回像素失败（%s），跳过像素验证\n", SDL_GetError());
            } else {
                // 统计与背景色 (24,24,28) 不同的像素数量
                int nonBg = 0;
                SDL_Surface* conv = SDL_ConvertSurface(shot, SDL_PIXELFORMAT_RGBA8888);
                SDL_Surface* use  = (conv != nullptr) ? conv : shot;
                for (int y = 0; y < use->h; y += 3) {
                    for (int x = 0; x < use->w; x += 3) {
                        Uint8 r = 0, g = 0, b = 0, a = 0;
                        if (!SDL_ReadSurfacePixel(use, x, y, &r, &g, &b, &a)) continue;
                        if (r != 24 || g != 24 || b != 28) ++nonBg;
                    }
                }
                std::printf("  · 前景像素采样数: %d\n", nonBg);
                // 可选：把离屏渲染结果存盘，便于人工/工具核对画面
                // 额外导出一张"获胜局面"（含 O 与获胜连线），便于核对 O 圆与高亮
                if (const char* shotPath0 = SDL_getenv("CJING_SHOT")) {
                    ttt::Game winGame;   // 注意：不要命名为 win，避免遮蔽外层 SDL_Window*
                    for (int c : {0, 3, 1, 4, 2}) winGame.applyMove(c);   // X 第一行获胜，含 2 个 O
                    view.beginFrame();
                    view.drawBoard(layout);
                    view.drawPieces(winGame.state(), layout);
                    view.drawWinningLine(winGame.state(), layout);
                    if (SDL_Surface* s2 = SDL_RenderReadPixels(ren, nullptr)) {
                        SDL_Surface* c2 = SDL_ConvertSurface(s2, SDL_PIXELFORMAT_XRGB8888);
                        std::string winPath = std::string(shotPath0);
                        auto dot = winPath.find_last_of('.');
                        if (dot != std::string::npos) winPath.insert(dot, "-win");
                        else winPath += "-win";
                        if (SDL_SaveBMP(c2 != nullptr ? c2 : s2, winPath.c_str())) {
                            std::printf("  · 获胜局面已保存: %s\n", winPath.c_str());
                        }
                        if (c2 != nullptr) SDL_DestroySurface(c2);
                        SDL_DestroySurface(s2);
                    }
                }
                // 截图模式下额外验证：加载字体并绘制中文 HUD
                ttt::sdl::SDLView hud(ren, 1.0F, assets);
                if (const char* shotPath = SDL_getenv("CJING_SHOT")) {
                    if (hud.loadFont("fonts/NotoSansSC-Regular.ttf", 44)) {
                        ttt::Game g2;
                        view.beginFrame();
                        view.drawBoard(layout);
                        view.drawPieces(g2.state(), layout);
                        hud.drawTextAt("轮到 X", layout.boardRect().x, layout.boardRect().y / 3,
                                       236, 238, 244);
                        hud.drawTextAt("点击棋盘落子 · R 重开 · ESC 退出",
                                       layout.boardRect().x,
                                       layout.boardRect().y / 3 + layout.boardRect().y / 3,
                                       140, 144, 156);
                        if (SDL_Surface* s3 = SDL_RenderReadPixels(ren, nullptr)) {
                            SDL_Surface* c3 = SDL_ConvertSurface(s3, SDL_PIXELFORMAT_XRGB8888);
                            std::string hp = std::string(shotPath);
                            auto d2 = hp.find_last_of('.');
                            if (d2 != std::string::npos) hp.insert(d2, "-hud");
                            else hp += "-hud";
                            if (SDL_SaveBMP(c3 != nullptr ? c3 : s3, hp.c_str())) {
                                std::printf("  · HUD 画面已保存: %s\n", hp.c_str());
                            }
                            if (c3 != nullptr) SDL_DestroySurface(c3);
                            SDL_DestroySurface(s3);
                        }
                    } else {
                        std::printf("  · 字体加载失败，跳过 HUD 截图\n");
                    }
                }
                {
                    (void)hud;
                }
                if (const char* shotPath = SDL_getenv("CJING_SHOT")) {
                    SDL_Surface* conv2 = SDL_ConvertSurface(use, SDL_PIXELFORMAT_XRGB8888);
                    SDL_Surface* out = (conv2 != nullptr) ? conv2 : use;
                    if (SDL_SaveBMP(out, shotPath)) {
                        std::printf("  · 画面已保存: %s\n", shotPath);
                    } else {
                        std::printf("  · 保存画面失败: %s\n", SDL_GetError());
                    }
                    if (conv2 != nullptr) SDL_DestroySurface(conv2);
                }
                check(nonBg > 100, "离屏渲染确有前景内容（非黑屏）");
                if (conv != nullptr) SDL_DestroySurface(conv);
                SDL_DestroySurface(shot);
            }
            SDL_SetRenderTarget(ren, nullptr);
            SDL_DestroyTexture(target);
        }
    }

    // ---- 存档往返：SaveIO 写入 → 读回 → 反序列化 → 棋盘一致 ----
    // 用【项目内临时目录】而不是用户目录：
    //   · CI 与沙箱环境可能不允许写 ~/Library
    //   · 测试结束后可以干净删除，不在用户机器上留垃圾
    {
        const std::string tmpDir = "selftest-tmp";
        SDL_CreateDirectory(tmpDir.c_str());     // 已存在时返回 false，无妨

        ttt::sdl::SDLSaveIO save(tmpDir);
        ttt::Game g;
        g.applyMove(0);      // X 落左上
        g.applyMove(4);      // O 落中心
        const std::string data = ttt::serialize(g.state());
        const std::string name = "save.txt";

        const bool wrote = save.write(name, data);
        const auto back = save.read(name);
        check(wrote && back.has_value() && *back == data,
              "SaveIO 写入/读回内容完全一致");

        if (back.has_value()) {
            const auto parsed = ttt::deserialize(*back);
            check(parsed.has_value() && parsed->board == g.state().board &&
                      parsed->current == g.state().current,
                  "读回内容可解析且棋盘/当前玩家一致");
        }

        // 清理：先删文件再删目录
        std::remove((save.writableDir() + name).c_str());
        SDL_RemovePath(tmpDir.c_str());
    }

    // ---- 音频（可选能力）----
    // 自测只验证"能初始化、能加载、能调用播放而不崩溃"，
    // 无法验证实际听感 —— 那必须在真机上由人耳确认。
    {
        // 自测统一使用 dummy 音频驱动。
        //
        // 理由与视频驱动相同：CI runner 没有音频后端，
        // 若走真实后端（pulse/alsa），不同环境行为不一，甚至会被
        // LeakSanitizer 报出 SDL 内部的泄漏，导致测试结果不稳定。
        // dummy 驱动不需要任何硬件，能稳定地覆盖"打开设备 / 加载 WAV /
        // 绑定流 / 播放"这条我们自己的代码路径。
#if !defined(_WIN32)
        setenv("SDL_AUDIODRIVER", "dummy", 1);
#else
        _putenv_s("SDL_AUDIODRIVER", "dummy");
#endif
        SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, "dummy", SDL_HINT_OVERRIDE);

        const auto audio = ttt::sdl::SDLAudio::create(assets);
        if (!audio) {
            std::printf("  · 无可用音频设备，跳过音效测试（CI/静音环境属正常）\n");
        } else {
            check(audio->available(), "音频设备可用");
            const bool l1 = audio->load(ttt::SoundId::Place, "sounds/place.wav");
            const bool l2 = audio->load(ttt::SoundId::Win,   "sounds/win.wav");
            const bool l3 = audio->load(ttt::SoundId::Draw,  "sounds/draw.wav");
            check(l1 && l2 && l3, "三个音效资源均加载成功");
            check(!audio->load(ttt::SoundId::Place, "sounds/does-not-exist.wav"),
                  "缺失的音效资源被正确拒绝");

            // 连播三次，确认重复触发不会崩溃
            for (int i = 0; i < 3; ++i) {
                audio->play(ttt::SoundId::Place);
                audio->play(ttt::SoundId::Win);
                audio->play(ttt::SoundId::Draw);
            }
            check(true, "连续播放未崩溃");
        }
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
