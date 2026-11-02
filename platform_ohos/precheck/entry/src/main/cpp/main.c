/* =============================================================================
 *  鸿蒙 Go/No-Go 预检（路线图 P2-T23）
 *
 *  判据（见路线图）：
 *    Go     = 空窗口能显示  &&  中文文字能渲染
 *    音频为可降级项，不作为判据
 *
 *  这个程序刻意做得极简：
 *    · 只有一个回调式 SDL 应用（SDL_MAIN_USE_CALLBACKS）
 *    · 画一个背景 + 一行中文 + 一个随触摸变色的方块（证明触摸事件到达）
 *    · 屏幕上没有交互也能看出状态：颜色块 = 累计触摸次数
 *
 *  入口说明（官方文档 README-harmonyos.md）：
 *    鸿蒙侧加载 libmain.so 后，先找 SDL_main 符号；没有则找
 *    SDL_AppInit/SDL_AppIterate 等回调 —— 我们走后者。
 * ========================================================================== */
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <stdio.h>

/* 中文文案直接用 UTF-8 字面量。注意本文件必须保存为 UTF-8 —— 与 Windows 的
 * /utf-8 问题同源；鸿蒙工具链默认按 UTF-8 处理，无此困扰。 */
static const char *kProbeText = "鸿蒙预检 · 中文渲染正常";

static SDL_Window   *g_window   = NULL;
static SDL_Renderer *g_renderer = NULL;
static TTF_Font     *g_font     = NULL;
static SDL_Texture  *g_textTex  = NULL;
static float         g_textW    = 0.0f;
static float         g_textH    = 0.0f;
static int           g_touchCount = 0;
static int           g_frames     = 0;
static int           g_lastOutW   = 0;
static int           g_lastOutH   = 0;

/* 用 TTF 渲染一行中文到纹理。失败不致命：窗口仍会显示，便于区分
 * "连窗口都起不来" 与 "字体有问题" 两种失败。 */
static bool buildTextTexture(void)
{
    if (g_font == NULL || g_renderer == NULL) return false;

    SDL_Color fg = {240, 240, 245, 255};
    SDL_Surface *surf = TTF_RenderText_Blended(g_font, kProbeText, 0, fg);
    if (surf == NULL) {
        SDL_Log("[precheck] TTF_RenderText_Blended 失败: %s", SDL_GetError());
        return false;
    }
    /* 先在 CPU 侧确认字体真的画出了内容：抽样统计非透明像素。
     * 这样即使显示链路有问题，也能单独证明"中文渲染成功"。 */
    {
        int nonEmpty = 0, samples = 0;
        for (int y = 0; y < surf->h; y += 4) {
            for (int x = 0; x < surf->w; x += 4) {
                Uint8 r = 0, g = 0, b = 0, a = 0;
                if (SDL_ReadSurfacePixel(surf, x, y, &r, &g, &b, &a) && a > 16) {
                    ++nonEmpty;
                }
                ++samples;
            }
        }
        SDL_Log("[precheck] 文字表面 %dx%d，抽样 %d 点，非透明 %d 点",
                surf->w, surf->h, samples, nonEmpty);
    }

    g_textTex = SDL_CreateTextureFromSurface(g_renderer, surf);
    g_textW = (float)surf->w;
    g_textH = (float)surf->h;
    SDL_DestroySurface(surf);
    if (g_textTex == NULL) {
        SDL_Log("[precheck] 创建文字纹理失败: %s", SDL_GetError());
        return false;
    }
    return true;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    (void)appstate; (void)argc; (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("[precheck] SDL_Init 失败: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* 窗口尺寸在鸿蒙上由系统决定，这里给一个初始值即可 */
    g_window = SDL_CreateWindow("Cjing 预检", 720, 1280, SDL_WINDOW_RESIZABLE);
    if (g_window == NULL) {
        SDL_Log("[precheck] 创建窗口失败: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* 渲染器必须在 AppInit 创建：实测鸿蒙后端的帧循环由"渲染器 + present"驱动，
     * 没有渲染器时 SDL_AppIterate 根本不会被调用（试过延迟创建，结果一帧都不跑）。
     * 代价是创建时窗口尺寸可能还是 0，viewport 需要稍后强制刷新（见 iterate）。 */
    /* 依次尝试渲染后端：实测鸿蒙模拟器上 ogl_es2 后端"清屏生效、几何全丢"，
     * 因此优先试 gpu / vulkan，最后才回退默认。 */
    static const char *kDrivers[] = {"gpu", "vulkan", "opengles2", NULL};
    for (int i = 0; kDrivers[i] != NULL && g_renderer == NULL; ++i) {
        g_renderer = SDL_CreateRenderer(g_window, kDrivers[i]);
        if (g_renderer != NULL) {
            SDL_Log("[precheck] 渲染器后端 = %s（请求 %s）",
                    SDL_GetRendererName(g_renderer), kDrivers[i]);
        } else {
            SDL_Log("[precheck] 后端 %s 不可用: %s", kDrivers[i], SDL_GetError());
        }
    }
    if (g_renderer == NULL) {
        SDL_Log("[precheck] 创建渲染器失败: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    {
        int lw = 0, lh = 0, pw = 0, ph = 0, ow2 = 0, oh2 = 0;
        SDL_GetWindowSize(g_window, &lw, &lh);
        SDL_GetWindowSizeInPixels(g_window, &pw, &ph);
        SDL_GetCurrentRenderOutputSize(g_renderer, &ow2, &oh2);
        SDL_Log("[precheck] 创建渲染器时：window=%dx%d pixels=%dx%d output=%dx%d",
                lw, lh, pw, ph, ow2, oh2);
    }

    /* 诊断阶段：先不用逻辑呈现，直接按输出像素绘制。
     * 目的：区分"绘制完全不可见"与"逻辑呈现映射错误"。 */

    /* 字体：鸿蒙上 SDL_GetBasePath() 返回 "assets://"，
     * 对应 hap 里 entry/src/main/resources/rawfile/ 的内容。
     * 所以字体放在 rawfile/fonts/ 下，路径就是 assets://fonts/... */
    if (!TTF_Init()) {
        SDL_Log("[precheck] TTF_Init 失败: %s", SDL_GetError());
    } else {
        g_font = TTF_OpenFont("assets://fonts/NotoSansSC-Regular.ttf", 44.0f);
        if (g_font == NULL) {
            SDL_Log("[precheck] 字体打开失败（将只显示窗口）: %s", SDL_GetError());
        } else {
            SDL_Log("[precheck] 字体已加载");
            (void)buildTextTexture();
        }
    }

    SDL_Log("[precheck] 初始化完成：窗口=%p 渲染器=%p 字体=%p 文字=%p",
            (void *)g_window, (void *)g_renderer, (void *)g_font, (void *)g_textTex);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    (void)appstate;

    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;

        /* 触摸回调：预检要求触摸事件能到达原生层。
         * 这里只计数并打印，屏幕上用颜色块体现。 */
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            ++g_touchCount;
            SDL_Log("[precheck] 触摸/点击 #%d", g_touchCount);
            break;

        default:
            break;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    (void)appstate;
    if (g_renderer == NULL) return SDL_APP_CONTINUE;

    int ow = 0, oh = 0;
    SDL_GetCurrentRenderOutputSize(g_renderer, &ow, &oh);
    if (ow <= 0 || oh <= 0) return SDL_APP_CONTINUE;

    if (ow != g_lastOutW || oh != g_lastOutH) {
        /* 用【显式矩形】而不是 NULL：NULL 表示"自动铺满"，
         * 依赖渲染器内部记录的 drawable 尺寸；当它在创建时是 0，
         * 自动值会算成 0x0，于是所有几何被裁掉。显式矩形能强制刷新 glViewport。 */
        SDL_Rect full = {0, 0, ow, oh};
        SDL_SetRenderViewport(g_renderer, &full);
        SDL_SetRenderClipRect(g_renderer, NULL);
        g_lastOutW = ow;
        g_lastOutH = oh;
        SDL_Log("[precheck] 输出尺寸 %dx%d", ow, oh);
    }

    /* 背景随触摸次数变化 —— 触摸是否到达原生层，肉眼可见 */
    const Uint8 br = (Uint8)(20 + (g_touchCount * 40) % 120);
    const Uint8 bg = (Uint8)(24 + (g_touchCount * 20) % 60);
    SDL_SetRenderDrawColor(g_renderer, br, bg, 40, 255);
    SDL_RenderClear(g_renderer);

    /* 左上角暖黄方块：证明几何绘制可用 */
    SDL_FRect marker = {60.0f, 60.0f, 140.0f, 140.0f};
    SDL_SetRenderDrawColor(g_renderer, 240, 200, 80, 255);
    SDL_RenderFillRect(g_renderer, &marker);

    /* 中文文字：Go 判据的核心 */
    if (g_textTex != NULL) {
        SDL_FRect dst = {60.0f, 260.0f, g_textW, g_textH};
        SDL_RenderTexture(g_renderer, g_textTex, NULL, &dst);
    }

    const bool presented = SDL_RenderPresent(g_renderer);
    if (g_frames == 0) {
        SDL_Log("[precheck] present 返回 %d，err=%s", (int)presented, SDL_GetError());
    }

    if (g_frames == 0) {
        SDL_Surface *rb = SDL_RenderReadPixels(g_renderer, NULL);
        if (rb != NULL) {
            Uint8 cr = 0, cg = 0, cb = 0, ca = 0;
            if (SDL_ReadSurfacePixel(rb, 100, 100, &cr, &cg, &cb, &ca)) {
                SDL_Log("[precheck] 方块中心读回 = (%u,%u,%u)（期望 240,200,80）", cr, cg, cb);
            }
            SDL_DestroySurface(rb);
        }
        SDL_Log("[precheck] 第一帧已呈现：window=%dx%d output=%dx%d 文字=%.0fx%.0f",
                ow, oh, ow, oh, g_textW, g_textH);
    }
    ++g_frames;
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)appstate; (void)result;
    SDL_Log("[precheck] 退出：共渲染 %d 帧，触摸 %d 次", g_frames, g_touchCount);

    if (g_textTex != NULL) SDL_DestroyTexture(g_textTex);
    if (g_font != NULL) TTF_CloseFont(g_font);
    TTF_Quit();
    if (g_renderer != NULL) SDL_DestroyRenderer(g_renderer);
    if (g_window != NULL) SDL_DestroyWindow(g_window);
    SDL_Quit();
}
