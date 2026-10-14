# platform_sdl/ —— SDL3 实现（渲染 / 输入 / 资源 / 存档 / 生命周期）
add_library(ttt_platform_sdl STATIC
    app.cpp)

target_include_directories(ttt_platform_sdl PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}")

# SDL3 必须 PUBLIC：app.hpp 公开 include <SDL3/SDL.h>，消费者（可执行文件）需要其头文件目录
target_link_libraries(ttt_platform_sdl
    PUBLIC  ttt_core
            SDL3::SDL3)

if(MSVC)
    target_compile_options(ttt_platform_sdl PRIVATE /W4 /permissive-)
else()
    target_compile_options(ttt_platform_sdl PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 可按平台开关：CJING_BUILD_APP 用于桌面可执行文件
option(CJING_BUILD_DESKTOP_APP "Build the desktop executable" ON)
if(CJING_BUILD_DESKTOP_APP)
    add_executable(CjingApp main_callbacks.cpp)
    target_link_libraries(CjingApp PRIVATE ttt_platform_sdl)
endif()
