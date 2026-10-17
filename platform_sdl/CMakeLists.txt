# =============================================================================
#  platform_sdl/ —— SDL3 实现层（渲染 / 输入 / 资源 / 存档 / 生命周期）
#
#  ▍本层的职责
#    实现 core/include/ttt/ 里定义的那些接口（IView / IInput / AssetIO / SaveIO），
#    并把 SDL 的类型、事件、坐标系全部挡在这一层里，不让它们泄漏到 core。
#
#  ▍为什么依赖在根目录声明
#    SDL3 与 SDL3_ttf 的获取、本地缓存解析都在根 CMakeLists 完成。
#    本文件只负责"用这些 target 构建我们自己的库与可执行文件"。
#    好处：依赖来源集中可见，构建规则保持简单。
# =============================================================================

add_library(ttt_platform_sdl STATIC
    app.cpp        # 应用外壳：窗口/渲染器/事件分发/生命周期
    sdl_view.cpp)  # IView 实现：棋盘、X/O、获胜连线、文字

target_include_directories(ttt_platform_sdl PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}")

# -----------------------------------------------------------------------------
#  链接方式：PUBLIC vs PRIVATE（这里踩过一次坑）
#
#  SDL3 必须用 PUBLIC，理由：本目录的公开头文件 app.hpp 里有
#      #include <SDL3/SDL.h>
#  也就是说 SDL 的头文件目录是"公开接口的一部分"。
#  若写成 PRIVATE，链接本库的可执行文件拿不到 SDL 头文件路径，编译失败：
#      fatal error: 'SDL3/SDL_main.h' file not found
#
#  PUBLIC 还会把 "包含目录 + 编译定义 + 链接依赖" 一起传递给上层目标。
# -----------------------------------------------------------------------------
target_link_libraries(ttt_platform_sdl
    PUBLIC  ttt_core
            SDL3::SDL3)

# -----------------------------------------------------------------------------
#  文字渲染（可选）
#
#  用一个编译定义 CJING_HAVE_TTF 把"有没有 TTF"传给源码，
#  而不是在 .cpp 里到处判断宏是否存在。这样：
#    · 关掉 TTF 时，代码走降级分支（只画几何图形，不显示中文）
#    · 打开时，界面显示"轮到 X / X 获胜！"等文字
#
#  用 PUBLIC 是因为这个宏会影响 app.cpp 与 selftest.cpp 两处编译。
# -----------------------------------------------------------------------------
if(CJING_WITH_TTF AND TARGET SDL3_ttf::SDL3_ttf)
    target_link_libraries(ttt_platform_sdl PUBLIC SDL3_ttf::SDL3_ttf)
    target_compile_definitions(ttt_platform_sdl PUBLIC CJING_HAVE_TTF=1)
    message(STATUS "SDL3_ttf: 已启用文字渲染")
elseif(CJING_WITH_TTF)
    # TTF 想开但目标不存在：警告而不是报错，让项目仍能构建（只是没有文字）
    message(WARNING "SDL3_ttf: 目标不可用，文字渲染将被禁用（界面将只显示几何图形）")
endif()

cjing_set_warnings(ttt_platform_sdl)
cjing_enable_sanitizers(ttt_platform_sdl)

# -----------------------------------------------------------------------------
#  桌面可执行文件
#
#  注意：这里是 add_executable，而 Android/HarmonyOS 必须改成
#  add_library(main SHARED)（产出 libmain.so）。原因见 docs/技术验证结论.md 2.3：
#    · Android/HarmonyOS 的进程入口是虚拟机（Java/ArkTS），原生代码只能是被加载的 .so
#    · iOS 则相反：main() 必须位于 .app 的可执行文件中，不能放在动态库里
#  目前只做桌面，跨平台分支留待 P3/P4 实现。
# -----------------------------------------------------------------------------
if(CJING_BUILD_DESKTOP_APP)
    # main_callbacks.cpp 里用 SDL_MAIN_USE_CALLBACKS，
    # 提供 SDL_AppInit/Event/Iterate/Quit 而不是 main()。
    add_executable(CjingApp main_callbacks.cpp)
    target_link_libraries(CjingApp PRIVATE ttt_platform_sdl)
    # 可执行文件也要开 sanitizer：静态库没有链接阶段，
    # 只给库加 -fsanitize 会导致运行库没有被链接进来（未定义符号 __asan_*）。
    cjing_enable_sanitizers(CjingApp)
endif()

# -----------------------------------------------------------------------------
#  无界面自测（可纳入 CI）
#
#  它解决的是"单元测试覆盖不到渲染链路"的问题：
#    · 用隐藏窗口 + 离屏纹理渲染，读回像素确认"画面真的有内容"（防黑屏）
#    · 合成鼠标事件走一遍坐标换算，验证 9 个格子命中正确
#    · 检查事件队列能被排空（防无限事件）
#  在无显示器的 Linux runner 上会自动回退到 dummy 视频驱动。
# -----------------------------------------------------------------------------
if(CJING_BUILD_SELFTEST)
    add_executable(CjingSelfTest selftest.cpp)
    target_link_libraries(CjingSelfTest PRIVATE ttt_platform_sdl)
    cjing_set_warnings(CjingSelfTest)
    cjing_enable_sanitizers(CjingSelfTest)
    add_test(NAME cjing_selftest COMMAND CjingSelfTest)
endif()
