# =============================================================================
#  鸿蒙正式工程的原生构建（路线图 P3 / 跨平台一致性）
#
#  与 Android 高度相似：
#    · 必须产出【共享库】libmain.so（SDL 的鸿蒙入口会 dlopen("libmain.so")）
#    · SDL3 / SDL3_ttf 通过 add_subdirectory 一起编译，hvigor 自动打进 hap
#    · 源码经 setup.sh 符号链接引入，不复制几百 MB
#
#  与桌面/Android 的唯一差别：这里的"应用代码"是 core/ + platform_sdl/，
#  而不是预检时的单个 main.c。
# =============================================================================
cmake_minimum_required(VERSION 3.5.0)

# 允许给其它目录创建的目标（SDL3-shared）追加链接库
if(POLICY CMP0079)
    cmake_policy(SET CMP0079 NEW)
endif()
project(cjing C CXX)

set(NATIVEAPP_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 字体渲染需要内置 FreeType/HarfBuzz
set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDLTTF_TESTS    OFF CACHE BOOL "" FORCE)
set(SDLTTF_SAMPLES  OFF CACHE BOOL "" FORCE)
set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)

add_subdirectory("SDL")

# SDL 的鸿蒙音频后端默认采纳系统的回调帧数（4096 帧 ≈ 93ms）。
# scripts/patch-sdl-ohos-audio.py 打了补丁，让它读取这个宏来设置更小的帧数。
# 该宏只在鸿蒙构建定义，其它平台不受影响。
set(CJING_OHOS_AUDIO_FRAMES 512 CACHE STRING "鸿蒙音频回调帧数（越小延迟越低）")
if(TARGET SDL3-shared)
    target_compile_definitions(SDL3-shared PRIVATE CJING_OHOS_AUDIO_FRAMES=${CJING_OHOS_AUDIO_FRAMES})
    target_link_libraries(SDL3-shared PRIVATE ohaudio)
endif()
add_subdirectory("SDL_ttf")

# 仓库根：cpp 位于 <repo>/platform_ohos/app/entry/src/main/cpp，向上五级
get_filename_component(CJING_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../.." ABSOLUTE)

add_library(main SHARED
    ${CJING_ROOT}/core/src/game.cpp
    ${CJING_ROOT}/core/src/layout.cpp
    ${CJING_ROOT}/core/src/io.cpp
    ${CJING_ROOT}/core/src/text.cpp
    ${CJING_ROOT}/core/src/tap.cpp
    ${CJING_ROOT}/platform_sdl/app.cpp
    ${CJING_ROOT}/platform_sdl/main_callbacks.cpp
    ${CJING_ROOT}/platform_sdl/sdl_view.cpp
    ${CJING_ROOT}/platform_sdl/sdl_io.cpp
    ${CJING_ROOT}/platform_sdl/sdl_audio.cpp
    ${CJING_ROOT}/platform_sdl/ohos_log.cpp)

target_include_directories(main PRIVATE
    ${CJING_ROOT}/core/include
    ${CJING_ROOT}/platform_sdl)

# hilog_ndk.z：OH_LOG_Print 所在库（鸿蒙 NDK 的日志接口）
target_link_libraries(main PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf hilog_ndk.z)
target_compile_definitions(main PRIVATE CJING_HAVE_TTF=1)
