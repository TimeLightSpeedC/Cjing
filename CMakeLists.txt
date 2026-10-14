cmake_minimum_required(VERSION 3.20)

project(Cjing
    VERSION 0.1.0
    DESCRIPTION "Cross-platform Tic-Tac-Toe (SDL3) — Windows/macOS/Linux/Android/iOS/HarmonyOS"
    LANGUAGES C CXX)

# ---------------------------------------------------------------- 全局设置
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # 供 clangd / clang-tidy 使用

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# 统一输出目录，避免各子目录散落产物
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# ------------------------------------------------- SDL3 依赖（路线图 F7 / P0-T3）
# 版本策略：全平台共用同一份 SDL3 源码。
#   - 必须用 FetchContent + 浅克隆：全历史克隆在本环境会因连接被 reset 而失败（路线图陷阱 E2）
#   - 不使用包管理器 / 系统包：它们提供的版本不含 HarmonyOS 后端
#   - 不锁 tag：当前没有任何已发布 tag 包含 src/video/openharmony/
#   - GIT_TAG 用分支名（浅克隆 + 分支名已验证稳定）；实际解析出的 SHA 记录在 README 与
#     《技术验证结论》中，六端共用
option(CJING_FETCH_SDL3 "Fetch SDL3 via FetchContent (else find_package)" ON)

set(CJING_SDL3_GIT_TAG "main" CACHE STRING "SDL3 git ref (branch name; record resolved SHA)")

set(CJING_SDL3_SOURCE_DIR "" CACHE PATH
    "已有 SDL3 源码目录（跳过下载；用于离线开发与复用克隆）")

if(CJING_FETCH_SDL3)
    include(FetchContent)
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        ${CJING_SDL3_GIT_TAG}
        GIT_SHALLOW    TRUE)
    if(CJING_SDL3_SOURCE_DIR)
        # 复用本地克隆：避免重复下载（本环境大文件传输不稳定，见路线图陷阱 E2）
        # 必须用 FETCHCONTENT_SOURCE_DIR_<name>（大写），它是 FetchContent 官方的跳过下载机制
        set(FETCHCONTENT_SOURCE_DIR_SDL3 "${CJING_SDL3_SOURCE_DIR}" CACHE PATH "" FORCE)
        message(STATUS "SDL3: 复用本地源码目录 ${CJING_SDL3_SOURCE_DIR}")
    endif()
    FetchContent_MakeAvailable(SDL3)
else()
    find_package(SDL3 CONFIG REQUIRED)
endif()

# --------------------------------------------------------------- 测试入口
# 注意：enable_testing() 必须出现在顶层，否则 ctest 在构建根目录看不到测试
enable_testing()

# ------------------------------------------------------------------- 子目录
add_subdirectory(core)
add_subdirectory(platform_sdl)

# ------------------------------------------------------------------- 摘要
if(CJING_FETCH_SDL3)
    if(CJING_SDL3_SOURCE_DIR)
        set(_sdl3_origin "FetchContent(${CJING_SDL3_GIT_TAG}) [复用本地: ${CJING_SDL3_SOURCE_DIR}]")
    else()
        set(_sdl3_origin "FetchContent(${CJING_SDL3_GIT_TAG})")
    endif()
else()
    set(_sdl3_origin "find_package(SDL3)")
endif()

message(STATUS "")
message(STATUS "Cjing ${PROJECT_VERSION}  (${CMAKE_BUILD_TYPE})")
message(STATUS "  C++ 标准      : ${CMAKE_CXX_STANDARD}")
message(STATUS "  SDL3 来源     : ${_sdl3_origin}")
message(STATUS "  构建目录      : ${CMAKE_BINARY_DIR}")
message(STATUS "")
