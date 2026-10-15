# platform_sdl/ —— SDL3 实现（渲染 / 输入 / 资源 / 存档 / 生命周期）

# ---------------------------------------------- SDL3_ttf（P2-T12，必需）
# 依赖通过 git submodule 自带（FreeType / HarfBuzz，均为 libsdl-org 维护的补丁分支），
# 因此必须 GIT_SUBMODULES_RECURSE TRUE，否则配置阶段会因缺少 external/freetype 失败。
# 鸿蒙/Android 交叉编译建议 SDLTTF_PLUTOSVG=OFF（省掉彩色 emoji 依赖，本项目不需要）。
option(CJING_WITH_TTF "Build text rendering (SDL3_ttf)" ON)

if(CJING_WITH_TTF)
    include(FetchContent)
    set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
    set(SDLTTF_TESTS OFF CACHE BOOL "" FORCE)
    set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(SDL3_ttf
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
        GIT_TAG        main
        GIT_SHALLOW    TRUE
        GIT_SUBMODULES_RECURSE TRUE)

    set(CJING_SDL3_TTF_SOURCE_DIR "" CACHE PATH "已有 SDL3_ttf 源码目录（跳过下载）")
    if(CJING_SDL3_TTF_SOURCE_DIR)
        set(FETCHCONTENT_SOURCE_DIR_SDL3_TTF "${CJING_SDL3_TTF_SOURCE_DIR}" CACHE PATH "" FORCE)
        message(STATUS "SDL3_ttf: 复用本地源码目录 ${CJING_SDL3_TTF_SOURCE_DIR}")
    endif()

    FetchContent_MakeAvailable(SDL3_ttf)
endif()
add_library(ttt_platform_sdl STATIC
    app.cpp
    sdl_view.cpp)

target_include_directories(ttt_platform_sdl PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}")

# SDL3 必须 PUBLIC：app.hpp 公开 include <SDL3/SDL.h>，消费者（可执行文件）需要其头文件目录
target_link_libraries(ttt_platform_sdl
    PUBLIC  ttt_core
            SDL3::SDL3)

if(CJING_WITH_TTF AND TARGET SDL3_ttf::SDL3_ttf)
    target_link_libraries(ttt_platform_sdl PUBLIC SDL3_ttf::SDL3_ttf)
    target_compile_definitions(ttt_platform_sdl PUBLIC CJING_HAVE_TTF=1)
    message(STATUS "SDL3_ttf: 已启用文字渲染")
endif()

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
