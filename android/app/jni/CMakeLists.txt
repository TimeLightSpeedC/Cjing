# =============================================================================
#  Android 原生构建（路线图 P3-T1/T5）
#
#  要点（与鸿蒙高度相似，经验直接复用）：
#   1. 必须产出【共享库】libmain.so（名为 main），由 SDLActivity 加载；
#      不能是可执行文件。
#   2. SDL3 与 SDL3_ttf 通过 add_subdirectory 一起编译，Gradle 会自动打包 .so。
#   3. 源码经 setup.sh 建立的符号链接引入，避免把几百 MB 源码复制进仓库。
# =============================================================================
cmake_minimum_required(VERSION 3.6)

project(cjing C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 字体渲染需要内置 FreeType/HarfBuzz
set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDLTTF_TESTS    OFF CACHE BOOL "" FORCE)
set(SDLTTF_SAMPLES  OFF CACHE BOOL "" FORCE)
set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)

add_subdirectory(SDL)
add_subdirectory(SDL_ttf)

# 仓库根目录（jni 在 android/app/jni，向上四级）
# jni 位于 <repo>/android/app/jni，向上三级即仓库根
get_filename_component(CJING_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)

add_library(main SHARED
    ${CJING_ROOT}/core/src/game.cpp
    ${CJING_ROOT}/core/src/layout.cpp
    ${CJING_ROOT}/core/src/io.cpp
    ${CJING_ROOT}/core/src/text.cpp
    ${CJING_ROOT}/platform_sdl/app.cpp
    ${CJING_ROOT}/platform_sdl/main_callbacks.cpp
    ${CJING_ROOT}/platform_sdl/sdl_view.cpp
    ${CJING_ROOT}/platform_sdl/sdl_io.cpp
    ${CJING_ROOT}/platform_sdl/sdl_audio.cpp)

target_include_directories(main PRIVATE
    ${CJING_ROOT}/core/include
    ${CJING_ROOT}/platform_sdl)

target_link_libraries(main PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf)

# TTF 可用开关（与桌面 CMake 保持一致，渲染层据此决定是否编文字路径）
target_compile_definitions(main PRIVATE CJING_HAVE_TTF=1)
