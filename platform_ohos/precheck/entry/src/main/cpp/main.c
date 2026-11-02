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

    g_renderer = SDL_CreateRenderer(g_window, NULL);
    if (g_renderer == NULL) {
        SDL_Log("[precheck] 创建渲染器失败: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* 与桌面版保持同样的逻辑分辨率与 letterbox 策略 */
    SDL_SetRenderLogicalPresentation(g_renderer, 900, 900,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

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

    /* 背景随触摸次数变化：即使没有文字，也能看出渲染与输入都在工作 */
    const Uint8 r = (Uint8)(20 + (g_touchCount * 40) % 120);
    const Uint8 g = (Uint8)(24 + (g_touchCount * 20) % 60);
    SDL_SetRenderDrawColor(g_renderer, r, g, 40, 255);
    SDL_RenderClear(g_renderer);

    /* 左上角画一个方块，颜色固定为暖黄，便于确认绘制坐标 */
    SDL_FRect marker = {60.0f, 60.0f, 120.0f, 120.0f};
    SDL_SetRenderDrawColor(g_renderer, 240, 200, 80, 255);
    SDL_RenderFillRect(g_renderer, &marker);

    /* 中文文字居中偏上 */
    if (g_textTex != NULL) {
        SDL_FRect dst = {(900.0f - g_textW) * 0.5f, 240.0f, g_textW, g_textH};
        SDL_RenderTexture(g_renderer, g_textTex, NULL, &dst);
    }

    SDL_RenderPresent(g_renderer);

    if (g_frames == 0) SDL_Log("[precheck] 第一帧已呈现");
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
