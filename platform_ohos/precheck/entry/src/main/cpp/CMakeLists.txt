# 鸿蒙预检工程的原生构建（路线图 P2-T23）
#
# 三个要点（来自 SDL3 官方 README-harmonyos.md）：
#   1. 必须产出【共享库】libmain.so，不能是可执行文件 —— 鸿蒙由 ArkTS 侧加载它，
#      再从中找 SDL_AppInit / SDL_AppEvent 等回调符号。
#   2. SDL3 与 SDL3_ttf 通过 add_subdirectory 一起编译，
#      产物会被 hvigor 自动打进 hap（无需手工拷贝 .so）。
#   3. 源码通过 setup.sh 建立的符号链接引入，避免把几百 MB 源码复制进仓库。
cmake_minimum_required(VERSION 3.5.0)
project(cjing_precheck C)

set(NATIVEAPP_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

# 字体渲染需要内置 FreeType/HarfBuzz：复用源码里的 external 子模块
set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDLTTF_TESTS    OFF CACHE BOOL "" FORCE)
set(SDLTTF_SAMPLES  OFF CACHE BOOL "" FORCE)
set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)

add_subdirectory("SDL")
add_subdirectory("SDL_ttf")

include_directories(${NATIVEAPP_ROOT_PATH}
                    ${NATIVEAPP_ROOT_PATH}/include)

add_library(main SHARED main.c)
target_link_libraries(main PUBLIC SDL3::SDL3 SDL3_ttf::SDL3_ttf)
