/* =============================================================================
 *  examples/minimal/main.c —— SDL3 最小可运行示例
 *
 *  这个文件的目的不是"实现功能"，而是把 SDL3 程序的骨架压缩到最短，
 *  方便你改着玩：
 *    - 把这一行换成别的颜色 / 坐标 / 图形
 *    - 加一个画圆的函数
 *    - 试着删掉 SDL_RenderPresent，观察会发生什么（CPU 占满、界面无响应）
 *
 *  它刻意用【纯 C】写，因为 SDL3 本身就是 C 库：
 *    C++ 里的引用、类、异常在这里都不存在，你看到的指针就是 SDL 的真实接口形态。
 * ========================================================================== */

/* 这两行必须在最前：它们把程序入口交给 SDL。
 * 定义 SDL_MAIN_USE_CALLBACKS 之后，本文件【不能】再写 main() 函数，
 * 否则会因为入口冲突而编译失败。 */
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>

#include <SDL3/SDL.h>
#include <stdio.h>

/* -----------------------------------------------------------------------------
 * 全局变量：两个不透明指针（opaque handle）
 *
 *   SDL_Window   *w   星号表示 w 是一个"指针"，它保存的是某个 SDL_Window 的地址
 *                     （不是窗口本身）。我们永远不知道 SDL_Window 内部长什么样，
 *                     也不应该知道 —— 这就是"不透明类型"。
 *
 *   为什么能放全局：SDL 保证窗口/渲染器的创建与销毁都在主线程；
 *   本例只有一个线程，用全局变量最简单。
 *   若多线程访问，就必须加锁 —— 这也是 SDL 文档反复强调线程安全的原因。
 * -------------------------------------------------------------------------- */
static SDL_Window   *g_window   = NULL;   /* NULL：空指针，表示"还没创建" */
static SDL_Renderer *g_renderer = NULL;

/* -----------------------------------------------------------------------------
 * SDL_AppInit —— 初始化，只调用一次
 *
 * 参数里的星号比较多，逐个看：
 *
 *   void **appstate
 *       第一个星号：appstate 是一个指针
 *       第二个星号：它指向的那个东西，本身也是指针（void *）
 *       读法：appstate 是"一个可以存放 void* 的地址"
 *       为什么这么设计：SDL 想让你把"自己的应用状态"存进去，之后每次回调
 *       再把它还给你。所以它给你一个【可以写入的变量地址】，
 *       你写 *appstate = my_state; 就存进去了。
 *       本例用不到状态，所以写 (void)s 表示"我知道这个参数存在，但不用它"。
 *
 *   int argc, char **argv
 *       没有星号的 argc：命令行参数个数
 *       两个星号：argv 是"指向字符指针的指针"
 *       它其实是一个字符串数组的起点：
 *           argv[0] -> "CjingExample"
 *           argv[1] -> 第一个参数...
 *       为什么是 char** 而不是 char*[]：C 里数组作参数会退化为指针，
 *       char*[] 和 char** 在这里是同一个东西。
 * -------------------------------------------------------------------------- */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    (void)appstate;   /* 未使用的参数用 (void) 抑制编译器告警 */
    (void)argc;
    (void)argv;

    /* SDL3 的初始化返回 bool：true 表示成功（SDL2 时代是返回 0） */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init 失败: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* 创建窗口。注意返回值是【指针】，需要判空 */
    g_window = SDL_CreateWindow("SDL3 最小示例", 640, 480, SDL_WINDOW_RESIZABLE);
    if (g_window == NULL) {
        fprintf(stderr, "创建窗口失败: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* 创建渲染器。第二个参数传 NULL 表示"让 SDL 自动选后端" */
    g_renderer = SDL_CreateRenderer(g_window, NULL);
    if (g_renderer == NULL) {
        fprintf(stderr, "创建渲染器失败: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* 打印一下版本，方便确认运行的是哪套 SDL */
    printf("SDL3 示例已启动，运行时版本 %d.%d.%d\n",
           SDL_VERSIONNUM_MAJOR(SDL_GetVersion()),
           SDL_VERSIONNUM_MINOR(SDL_GetVersion()),
           SDL_VERSIONNUM_MICRO(SDL_GetVersion()));
    fflush(stdout);

    return SDL_APP_CONTINUE;   /* 返回这个值，SDL 才会开始调用 Iterate/Event */
}

/* -----------------------------------------------------------------------------
 * SDL_AppEvent —— 事件回调，可能被调用很多次
 *
 *   SDL_Event *event
 *       星号：event 是一个指针，指向 SDL 内部构造好的事件对象。
 *       为什么要传指针而不是值：SDL_Event 是一个 union，体积较大（几十字节），
 *       每次都拷贝不划算；而且 SDL3 允许你原地修改它
 *       （例如 SDL_ConvertEventToRenderCoordinates 就是把坐标直接改掉）。
 *
 *   event->type
 *       -> 是 (*event).type 的简写，意思是"先解引用拿到事件对象，再取它的 type 字段"。
 *       C++ 里这两种写法等价，-> 只是为了可读性。
 * -------------------------------------------------------------------------- */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    (void)appstate;

    switch (event->type) {
        case SDL_EVENT_QUIT:          /* 点窗口关闭按钮 */
            return SDL_APP_SUCCESS;   /* 返回"成功退出"，SDL 会去调 AppQuit */

        case SDL_EVENT_KEY_DOWN:
            if (event->key.key == SDLK_ESCAPE) {
                return SDL_APP_SUCCESS;
            }
            break;

        default:
            break;
    }
    return SDL_APP_CONTINUE;
}

/* -----------------------------------------------------------------------------
 * SDL_AppIterate —— 每帧调用一次（帧率由 vsync 决定，通常 60Hz）
 * -------------------------------------------------------------------------- */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    (void)appstate;

    /* ① 清屏：用深色背景 */
    SDL_SetRenderDrawColor(g_renderer, 24, 26, 32, 255);
    SDL_RenderClear(g_renderer);

    /* ② 画一条线（注意 SDL3 叫 SDL_RenderLine，SDL2 叫 SDL_RenderDrawLine） */
    SDL_SetRenderDrawColor(g_renderer, 240, 200, 80, 255);
    SDL_RenderLine(g_renderer, 80.f, 80.f, 560.f, 400.f);

    /* ③ 画一个矩形。
     *    SDL3 的矩形用 SDL_FRect（浮点），指针传递：&rect 表示"rect 的地址"。
     *    这里出现两种星号语义：
     *      SDL_FRect rect;      无星号：栈上的一个对象
     *      &rect                取地址：得到一个指向它的指针
     *      const SDL_FRect *    形参类型：只读的矩形指针
     */
    SDL_FRect rect;
    rect.x = 220.f;
    rect.y = 160.f;
    rect.w = 200.f;
    rect.h = 120.f;
    SDL_SetRenderDrawColor(g_renderer, 110, 190, 250, 255);
    SDL_RenderRect(g_renderer, &rect);

    /* ④ 呈现。
     *    这一行是【必须】的：SDL 的 vsync 等待就发生在 SDL_RenderPresent 内部。
     *    删掉它，主循环会以极高帧率空转（实测可达 20 万 fps），CPU 占满、
     *    事件处理被饿死，macOS 直接判定应用无响应。
     */
    SDL_RenderPresent(g_renderer);

    return SDL_APP_CONTINUE;
}

/* -----------------------------------------------------------------------------
 * SDL_AppQuit —— 清理，只调用一次
 *
 * 清理顺序必须是【创建顺序的逆序】：
 *     创建：Window -> Renderer
 *     销毁：Renderer -> Window -> SDL_Quit
 * 反过来会在某些平台崩溃（渲染器还引用着窗口）。
 * -------------------------------------------------------------------------- */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)appstate;
    (void)result;

    if (g_renderer != NULL) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = NULL;      /* 置空，避免"悬空指针"（dangling pointer） */
    }
    if (g_window != NULL) {
        SDL_DestroyWindow(g_window);
        g_window = NULL;
    }
    SDL_Quit();
    printf("示例已退出\n");
}
