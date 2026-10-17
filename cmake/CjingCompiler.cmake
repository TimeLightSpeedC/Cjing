# =============================================================================
#  cmake/CjingCompiler.cmake —— 统一的编译选项封装
#
#  ▍为什么要封装成函数
#    如果把 if(MSVC) ... else() ... 直接写进每个子目录，会出现三个问题：
#      1. 重复：五个目标就要抄五遍，改一次忘一处
#      2. 不一致：某天有人只给 A 目标加了 -Wshadow，告警就没意义了
#      3. 污染：若写成 set(CMAKE_CXX_FLAGS ...)，第三方依赖（SDL3/HarfBuzz）
#         会被一起开高告警，而它们的代码不归我们维护，报错也没法改
#    -> 用函数，且只对"我们自己的 target"调用。
#
#  ▍CMake 的目标属性传播（这是最容易搞错的地方）
#    PRIVATE  只影响自己
#    PUBLIC   影响自己 + 所有链接它的目标
#    INTERFACE 只影响链接它的目标（自己用不到）
#    本项目里踩过的真实例子：platform_sdl 曾用 PRIVATE 链接 SDL3，
#    而它的公开头文件 app.hpp 里有 #include <SDL3/SDL.h>，
#    导致上层可执行文件找不到 SDL 头文件。正确做法是 PUBLIC。
# =============================================================================

#-----------------------------------------------------------------------------
# cjing_set_warnings(<target>)
#
# 为本项目自己的代码开启常见告警。第三方依赖不要调用本函数。
#
# 选的这几个开关各有原因：
#   -Wall      基础告警集
#   -Wextra    额外告警（未使用参数等）
#   -Wpedantic 非标准扩展提示（保证 Windows/Linux/macOS 行为一致）
#   -Wshadow   局部变量遮蔽外层同名变量 —— 实测抓到过真实 bug：
#              自测里 ttt::Game win 遮蔽了外层的 SDL_Window* win
#-----------------------------------------------------------------------------
function(cjing_set_warnings target)
    if(MSVC)
        # /W4      警告等级 4（MSVC 的最高常规等级）
        # /permissive- 关闭宽松模式，向标准靠拢，避免"MSVC 能编、clang 不能编"
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(CJING_WERROR)
            target_compile_options(${target} PRIVATE /WX)   # 告警即错误
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow)
        if(CJING_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

#-----------------------------------------------------------------------------
# cjing_enable_sanitizers(<target> [SANITIZERS address;undefined])
#
# 启用 AddressSanitizer / UndefinedBehaviorSanitizer。
#
# 注意两个平台差异（都实测过）：
#  1) MSVC 不支持 GCC 风格的 -fsanitize。Windows 上需另开 /fsanitize=address
#     的专用配置，故这里直接跳过并给出提示，而不是写出编译不过的命令。
#  2) LeakSanitizer（detect_leaks）只有 Linux 支持。在 macOS 上设置
#     ASAN_OPTIONS=detect_leaks=1 会报 "detect_leaks is not supported"，
#     所以 CI 只在 Linux 的 sanitizer 作业里设这个环境变量。
#
# 为什么同时加 target_link_options：
#   -fsanitize 需要运行库（libasan/libubsan）。静态库没有链接阶段，
#   只写 compile_options 会得到"未定义符号 __asan_*"的链接错误。
#   因此除静态库外，可执行目标也要调用本函数。
#-----------------------------------------------------------------------------
function(cjing_enable_sanitizers target)
    if(NOT CJING_SANITIZE)
        return()
    endif()

    cmake_parse_arguments(ARG "" "" "SANITIZERS" ${ARGN})
    if(NOT ARG_SANITIZERS)
        set(ARG_SANITIZERS address undefined)   # 默认两款都开
    endif()

    if(MSVC)
        message(STATUS "CJING_SANITIZE 在 MSVC 下未启用（请使用 /fsanitize=address 专用配置）")
        return()
    endif()

    # CMake 列表用分号分隔，而编译器要逗号：address;undefined -> address,undefined
    string(REPLACE ";" "," _san "${ARG_SANITIZERS}")

    target_compile_options(${target} PRIVATE
        -fsanitize=${_san}
        -fno-omit-frame-pointer)   # 保留帧指针，堆栈回溯才准确
    target_link_options(${target} PRIVATE -fsanitize=${_san})
endfunction()
