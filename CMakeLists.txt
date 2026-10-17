cmake_minimum_required(VERSION 3.20)

project(Cjing
    VERSION 0.2.0
    DESCRIPTION "Cross-platform Tic-Tac-Toe (SDL3) — Windows/macOS/Linux/Android/iOS/HarmonyOS"
    LANGUAGES C CXX)

# ===========================================================================
#  全局设置
# ===========================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # 供 clangd / clang-tidy 使用

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# 统一输出目录，避免产物散落在各子目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# ===========================================================================
#  构建选项（集中在此，便于 CMake GUI / CLion 中统一查看）
# ===========================================================================
option(CJING_FETCH_SDL3       "通过 FetchContent 获取 SDL3（关闭则用 find_package）" ON)
option(CJING_WITH_TTF         "启用 SDL3_ttf 文字渲染"                            ON)
option(CJING_BUILD_TESTS      "构建单元测试（Catch2）"                            ON)
option(CJING_BUILD_DESKTOP_APP "构建桌面可执行文件 CjingApp"                      ON)
option(CJING_BUILD_SELFTEST   "构建无界面自测 CjingSelfTest"                      ON)
option(CJING_SANITIZE         "为本项目目标启用 ASan/UBSan（第三方依赖不受影响）"  OFF)
option(CJING_WERROR           "把告警当作错误（仅本项目代码）"                    OFF)
option(CJING_OFFLINE          "禁止联网：依赖必须来自本地缓存，否则直接报错"      OFF)

set(CJING_SDL3_GIT_TAG "main" CACHE STRING
    "SDL3 的 git ref。用分支名（如 main）便于跟随开发主线（鸿蒙后端尚未进入任何发布版本）；
     如需完全可复现，可改为具体的 commit SHA，并记录到 docs/技术验证结论.md")

# ===========================================================================
#  依赖解析：本地缓存优先，网络兜底
# ===========================================================================
# 默认缓存位置为项目内 .tools/（不纳入版本控制）。
# 若依赖放在别处，用 -DCJING_CACHE_DIR=<path> 指定。
set(CJING_CACHE_DIR "${CMAKE_SOURCE_DIR}/.tools" CACHE PATH "第三方依赖本地缓存根目录")

include("${CMAKE_SOURCE_DIR}/cmake/CjingDeps.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/CjingCompiler.cmake")

# 约定布局（优先）与历史布局（兼容）
cjing_detect_dep(SDL3 CJING_SDL3_SOURCE_DIR
    "${CJING_CACHE_DIR}/sdl3-src"
    "${CJING_CACHE_DIR}/sdl3probe/build/_deps/sdl3-src")

if(CJING_WITH_TTF)
    cjing_detect_dep(SDL3_ttf CJING_SDL3_TTF_SOURCE_DIR
        "${CJING_CACHE_DIR}/sdltff-src"
        "${CJING_CACHE_DIR}/ttf-fetch/build/_deps/sdl3_ttf-src")
endif()

if(CJING_BUILD_TESTS)
    cjing_detect_dep(Catch2 CJING_CATCH2_SOURCE_DIR
        "${CJING_CACHE_DIR}/catch2-src"
        "${CMAKE_SOURCE_DIR}/build/_deps/catch2-src")
endif()

if(CJING_OFFLINE)
    cjing_require_offline(
        CJING_SDL3_SOURCE_DIR
        CJING_SDL3_TTF_SOURCE_DIR
        CJING_CATCH2_SOURCE_DIR)
endif()

# ===========================================================================
#  依赖获取
# ===========================================================================
include(FetchContent)

# ---------------------------------------------------------------- SDL3
if(CJING_FETCH_SDL3)
    cjing_prepare_dep(SDL3 CJING_SDL3_SOURCE_DIR)
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        ${CJING_SDL3_GIT_TAG}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(SDL3)
else()
    find_package(SDL3 CONFIG REQUIRED)
endif()

# ------------------------------------------------------------ SDL3_ttf
if(CJING_WITH_TTF)
    # 必须启用 vendored：使用 SDL_ttf 自带的 freetype/harfbuzz 子模块。
    # 否则它会去找系统库，而 macOS/Windows 默认没有，鸿蒙/Android 更不会有。
    set(SDLTTF_VENDORED  ON  CACHE BOOL "" FORCE)
    set(SDLTTF_PLUTOSVG  OFF CACHE BOOL "" FORCE)   # 彩色 emoji 依赖，本项目不需要
    set(SDLTTF_TESTS     OFF CACHE BOOL "" FORCE)
    set(SDLTTF_SAMPLES   OFF CACHE BOOL "" FORCE)

    cjing_prepare_dep(SDL3_ttf CJING_SDL3_TTF_SOURCE_DIR)
    FetchContent_Declare(SDL3_ttf
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
        GIT_TAG        main
        GIT_SHALLOW    TRUE
        GIT_SUBMODULES_RECURSE TRUE)   # 依赖是 git submodule，必须递归
    FetchContent_MakeAvailable(SDL3_ttf)
endif()

# --------------------------------------------------------------- Catch2
if(CJING_BUILD_TESTS)
    cjing_prepare_dep(Catch2 CJING_CATCH2_SOURCE_DIR)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(Catch2)
endif()

# ===========================================================================
#  子目录
# ===========================================================================
# 注意：enable_testing() 必须在顶层，否则 ctest 在构建根目录看不到测试
enable_testing()

add_subdirectory(core)
add_subdirectory(platform_sdl)

# ===========================================================================
#  配置摘要
# ===========================================================================
function(cjing_dep_origin dep_name dir_var)
    if("${${dir_var}}" STREQUAL "")
        set(_where "网络获取")
    else()
        set(_where "本地缓存")
    endif()
    set(${dep_name}_ORIGIN "${_where}: ${${dir_var}}" PARENT_SCOPE)
endfunction()

if(CJING_FETCH_SDL3)
    cjing_dep_origin(_sdl3 CJING_SDL3_SOURCE_DIR)
    set(_sdl3_line "${_sdl3_ORIGIN}  (tag: ${CJING_SDL3_GIT_TAG})")
else()
    set(_sdl3_line "find_package(SDL3)")
endif()

if(CJING_WITH_TTF)
    cjing_dep_origin(_ttf CJING_SDL3_TTF_SOURCE_DIR)
    set(_ttf_line "${_ttf_ORIGIN}")
else()
    set(_ttf_line "已禁用（CJING_WITH_TTF=OFF）")
endif()

if(CJING_BUILD_TESTS)
    cjing_dep_origin(_catch CJING_CATCH2_SOURCE_DIR)
    set(_catch_line "${_catch_ORIGIN}")
else()
    set(_catch_line "已禁用（CJING_BUILD_TESTS=OFF）")
endif()

message(STATUS "")
message(STATUS "========== Cjing ${PROJECT_VERSION} ==========")
message(STATUS "  构建类型      : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ 标准      : ${CMAKE_CXX_STANDARD}")
message(STATUS "  编译器        : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  SDL3          : ${_sdl3_line}")
message(STATUS "  SDL3_ttf      : ${_ttf_line}")
message(STATUS "  Catch2        : ${_catch_line}")
message(STATUS "  依赖缓存目录  : ${CJING_CACHE_DIR}")
message(STATUS "  构建目录      : ${CMAKE_BINARY_DIR}")
message(STATUS "  可选开关      : SANITIZE=${CJING_SANITIZE}  WERROR=${CJING_WERROR}  OFFLINE=${CJING_OFFLINE}")
message(STATUS "============================================")
message(STATUS "")

# 提示：若依赖走网络且本机网络受限，给出可执行建议
if(NOT CJING_OFFLINE)
    if(CJING_FETCH_SDL3 AND "${CJING_SDL3_SOURCE_DIR}" STREQUAL "")
        message(STATUS "提示：未找到 SDL3 本地缓存，本次将联网下载。")
        message(STATUS "      若网络受限，可把源码放到 ${CJING_CACHE_DIR}/sdl3-src，")
        message(STATUS "      或配置 -DCJING_SDL3_SOURCE_DIR=<目录>。")
    endif()
endif()
