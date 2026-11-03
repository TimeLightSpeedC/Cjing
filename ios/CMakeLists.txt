# =============================================================================
#  iOS 构建（路线图 P3-T14）
#
#  设计选择：**不用 SDL3.xcframework，而是从源码静态链接**。
#  理由：
#   1. 与桌面/Android 完全一致——同一份源码、同一套 CMake，只换工具链；
#   2. 避免"嵌入 framework → 打包前再移除"的额外步骤（T19 的负担来源）；
#   3. 可以直接用 Ninja 构建，不依赖 Xcode 工程（便于 CI）。
#
#  用法（在仓库根执行）：
#    cmake -S ios -B build-ios-sim -G Ninja \
#      -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator \
#      -DCMAKE_OSX_ARCHITECTURES=arm64
#    cmake --build build-ios-sim
# =============================================================================
cmake_minimum_required(VERSION 3.21)

project(cjing C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- 依赖源码位置（与其它平台共用 .tools 缓存）----
get_filename_component(CJING_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
if(NOT DEFINED CJING_SDL3_DIR)
    set(CJING_SDL3_DIR "${CJING_ROOT}/.tools/sdl3-src")
endif()
if(NOT DEFINED CJING_SDLTTF_DIR)
    set(CJING_SDLTTF_DIR "${CJING_ROOT}/.tools/sdltff-src")
endif()

set(SDL_TESTS    OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL  OFF CACHE BOOL "" FORCE)
add_subdirectory("${CJING_SDL3_DIR}" sdl3)

# 字体渲染需要内置 FreeType/HarfBuzz
set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDLTTF_TESTS    OFF CACHE BOOL "" FORCE)
set(SDLTTF_SAMPLES  OFF CACHE BOOL "" FORCE)
set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
add_subdirectory("${CJING_SDLTTF_DIR}" sdltff)

# iOS 上 SDL3 的 SDL_opengl.h 是**空文件**（源码里明确写着 "No OpenGL on iOS"），
# 而 SDL_ttf 会无条件编译 src/SDL_gl_textengine.c —— 该文件因此缺少 GLuint 等类型，
# 编译失败。
#
# 我们只用 surface 渲染路径（TTF_RenderText_Blended），不需要 OpenGL 文本引擎，
# 所以把它从目标源列表中移除。这样既不改动 vendored 源码，
# 也不会引入任何未被调用的符号。
foreach(_tgt SDL3_ttf-static SDL3_ttf-shared)
    if(TARGET ${_tgt})
        get_target_property(_srcs ${_tgt} SOURCES)
        if(_srcs)
            list(FILTER _srcs EXCLUDE REGEX "SDL_gl_textengine\\.c$")
            set_target_properties(${_tgt} PROPERTIES SOURCES "${_srcs}")
        endif()
    endif()
endforeach()

# ---- 应用本体（与桌面共用同一份源码）----
add_executable(cjing MACOSX_BUNDLE
    ${CJING_ROOT}/core/src/game.cpp
    ${CJING_ROOT}/core/src/layout.cpp
    ${CJING_ROOT}/core/src/io.cpp
    ${CJING_ROOT}/core/src/text.cpp
    ${CJING_ROOT}/core/src/tap.cpp
    ${CJING_ROOT}/platform_sdl/app.cpp
    ${CJING_ROOT}/platform_sdl/main_callbacks.cpp
    ${CJING_ROOT}/platform_sdl/sdl_view.cpp
    ${CJING_ROOT}/platform_sdl/sdl_io.cpp
    ${CJING_ROOT}/platform_sdl/sdl_audio.cpp)

target_include_directories(cjing PRIVATE
    ${CJING_ROOT}/core/include
    ${CJING_ROOT}/platform_sdl)

target_link_libraries(cjing PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf)
target_compile_definitions(cjing PRIVATE CJING_HAVE_TTF=1)

set_target_properties(cjing PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME            "井字棋"
    MACOSX_BUNDLE_GUI_IDENTIFIER         "com.timelightspeed.cjing"
    MACOSX_BUNDLE_BUNDLE_VERSION         "0.2.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING   "0.2.0"
    MACOSX_BUNDLE_INFO_PLIST             "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in")

# ---- 资源：复制到 .app/assets/ ----
# SDL_GetBasePath() 在 iOS 返回 .app 目录，AssetIO 期望 <base>/assets/<name>
add_custom_command(TARGET cjing POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CJING_ROOT}/assets"
            "$<TARGET_BUNDLE_DIR:cjing>/assets"
    COMMENT "复制资源到 app bundle")
