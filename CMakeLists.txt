# =============================================================================
#  Cjing —— 跨平台井字棋（Windows / macOS / Linux / Android / iOS / HarmonyOS）
#
#  这份 CMakeLists 的设计原则（阅读顺序即执行顺序）：
#
#    1) 全局设置：标准、输出目录、默认构建类型
#    2) 选项声明：所有可选开关集中在此，不在子目录里零散声明
#    3) 依赖解析：先找本地缓存，找不到才联网（详见 cmake/CjingDeps.cmake）
#    4) 依赖获取：FetchContent 拉取并编译第三方库
#    5) 子目录：真实代码的构建规则交给 core/ 与 platform_sdl/
#    6) 摘要输出：configure 结束时打印"每个依赖从哪来"，便于排错
#
#  ⚠️ 本文件刻意不写 set(CMAKE_CXX_FLAGS ...)：
#     全局编译选项会污染第三方依赖（SDL3/HarfBuzz 自带告警），
#     正确做法是用 target_compile_options 只作用于我们自己的目标。
# =============================================================================

cmake_minimum_required(VERSION 3.20)
# 为什么是 3.20：FetchContent 的 FETCHCONTENT_SOURCE_DIR_<NAME> 跳过下载机制、
# 以及 target_link_options 等能力在此版本已稳定。CI 用的 CMake 均满足。
# 注意不要抬到 3.24+，Ubuntu 20.04 自带的 CMake 是 3.16，会挡住一部分用户。

project(Cjing
    VERSION 0.2.0
    DESCRIPTION "Cross-platform Tic-Tac-Toe (SDL3)"
    LANGUAGES C CXX)
# LANGUAGES 必须同时包含 C 和 CXX：
#   SDL3 与 FreeType 是 C 项目，只声明 CXX 会导致它们的编译失败。

# =============================================================================
#  一、全局设置
# =============================================================================

# 用 target_compile_features 传播标准比用 CMAKE_CXX_STANDARD 全局变量更精确，
# 但这里仍设一次全局值，作为"没有显式声明 feature 的目标"的兜底。
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)   # 编译器不支持 C++20 就直接报错，而不是悄悄降级
set(CMAKE_CXX_EXTENSIONS OFF)         # 禁用 gnu++20 扩展，保证跨编译器一致

# 生成 compile_commands.json：clangd / clang-tidy / CLion 的代码分析依赖它
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 单配置生成器（Ninja/Make）不会自动选构建类型，默认给 Debug 更方便调试；
# 多配置生成器（Xcode/VS）由 IDE 决定，故只在无配置时设置。
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "构建类型" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)   # 让 CMake GUI/CLion 显示为下拉框
endif()

# 把可执行文件、动态库、静态库分别收到 bin/ 与 lib/ 下。
# 好处：命令行运行时路径稳定（build/bin/CjingApp），打包脚本也只需盯这两个目录。
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# =============================================================================
#  二、构建选项（全部集中在此，便于 CMake GUI / CLion 统一查看与切换）
#
#  option(<名称> "<说明>" <默认值>)
#  这些都会成为 CACHE 变量，可在命令行用 -D<名称>=ON/OFF 覆盖。
# =============================================================================
option(CJING_FETCH_SDL3        "通过 FetchContent 获取 SDL3（关闭则用 find_package）" ON)
option(CJING_WITH_TTF          "启用 SDL3_ttf 文字渲染"                            ON)
option(CJING_BUILD_TESTS       "构建单元测试（Catch2）"                            ON)
option(CJING_BUILD_DESKTOP_APP "构建桌面可执行文件 CjingApp"                       ON)
option(CJING_BUILD_SELFTEST    "构建无界面自测 CjingSelfTest"                      ON)
option(CJING_BUILD_EXAMPLES    "构建学习用示例（examples/）"                      ON)
option(CJING_SANITIZE          "为本项目目标启用 ASan/UBSan（第三方依赖不受影响）"  OFF)
option(CJING_WERROR            "把告警当作错误（仅本项目代码）"                     OFF)
option(CJING_OFFLINE           "禁止联网：依赖必须来自本地缓存，否则直接报错"        OFF)

# SDL3 的版本选择：
#   用分支名 main 而不是 tag —— 因为鸿蒙后端（src/video/openharmony/）已合入主线，
#   但尚未进入任何发布 tag（最新 release-3.4.18 不含它）。若改成 tag，鸿蒙端会构建失败。
#   代价：分支会漂移，故必须把实际解析到的 SHA 记录到 docs/技术验证结论.md。
#   需要完全可复现时，可改成具体 commit SHA（但浅克隆可能不支持按 SHA 取）。
set(CJING_SDL3_GIT_TAG "main" CACHE STRING "SDL3 的 git ref（分支名或 commit SHA）")

# =============================================================================
#  三、依赖缓存目录 + 加载辅助模块
#
#  为什么不把依赖路径写死：项目可能在多台机器/多个目录间移动，
#  硬编码绝对路径会让 configure 直接失败。这里用一个"缓存根目录"统一管理，
#  需要时用 -DCJING_CACHE_DIR=<path> 指向别处即可。
# =============================================================================
set(CJING_CACHE_DIR "${CMAKE_SOURCE_DIR}/.tools" CACHE PATH "第三方依赖本地缓存根目录")

include("${CMAKE_SOURCE_DIR}/cmake/CjingDeps.cmake")      # 依赖解析（缓存优先）
include("${CMAKE_SOURCE_DIR}/cmake/CjingCompiler.cmake")  # 告警与 Sanitizer 封装

# 依赖探测顺序：用户显式指定 → 约定目录 → 历史目录（向后兼容）→ 交给网络。
# 候选路径可以写多个，命中第一个含 CMakeLists.txt 的目录即采用。
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

# CJING_OFFLINE=ON 时，任何依赖缺失都立刻中止并给出补救办法，
# 而不是让 FetchContent 去慢慢超时（在网络受限环境下可能卡几分钟）。
if(CJING_OFFLINE)
    cjing_require_offline(
        CJING_SDL3_SOURCE_DIR
        CJING_SDL3_TTF_SOURCE_DIR
        CJING_CATCH2_SOURCE_DIR)
endif()

# =============================================================================
#  四、依赖获取
#
#  统一模式（三个依赖都一样）：
#      cjing_prepare_dep(...)        <- 有本地源码就切过去并关闭联网更新
#      FetchContent_Declare(...)     <- 声明"如果需要，从哪下载什么版本"
#      FetchContent_MakeAvailable(...) <- 真正拉取/编译，并导出 target
#
#  注意顺序：SDL3_ttf 链接 SDL3，Catch2 被 core/tests 使用，
#  所以必须先准备好 SDL3，再处理 SDL3_ttf。
# =============================================================================
include(FetchContent)

# ------------------------------------------------------------------- SDL3
if(CJING_FETCH_SDL3)
    if(CJING_SDL3_SOURCE_DIR)
        # 显式告诉 FetchContent 使用本地源码：这样下载与 update 步骤都会被跳过。
        # 关键点：GIT_TAG 是分支名时，即使源码已在本地，CMake 默认每次 configure
        # 都会 git fetch 检查更新；网络不通就会报 sdl3-populate-gitupdate 失败。
        set(FETCHCONTENT_SOURCE_DIR_SDL3 "${CJING_SDL3_SOURCE_DIR}" CACHE PATH "" FORCE)
        # 双保险：全局关闭联网更新
        set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)
        message(STATUS "SDL3: 使用本地源码 ${CJING_SDL3_SOURCE_DIR}")
    endif()

    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        ${CJING_SDL3_GIT_TAG}
        GIT_SHALLOW    TRUE)
    # GIT_SHALLOW TRUE：只取最新一个提交（约 34MB，而非全历史 75MB）。
    # 全历史克隆在受限网络下经常被 reset；浅克隆稳定得多。

    FetchContent_MakeAvailable(SDL3)
    # 成功后会得到 SDL3::SDL3 目标（SDL3 自身提供，静态/动态库二选一）
else()
    # 使用系统/包管理器提供的 SDL3。注意：这种方式拿不到鸿蒙后端。
    find_package(SDL3 CONFIG REQUIRED)
endif()

# --------------------------------------------------------------- SDL3_ttf
if(CJING_WITH_TTF)
    # 必须启用 vendored：使用 SDL_ttf 自带的 freetype/harfbuzz 子模块。
    # 不开的话它会去找系统库 —— macOS/Windows 默认没有，鸿蒙/Android 更不会有，
    # 报错形如：Could NOT find Freetype (missing: FREETYPE_LIBRARY FREETYPE_INCLUDE_DIRS)
    set(SDLTTF_VENDORED  ON  CACHE BOOL "" FORCE)
    set(SDLTTF_PLUTOSVG  OFF CACHE BOOL "" FORCE)   # 彩色 emoji 支持，本项目不需要
    set(SDLTTF_TESTS     OFF CACHE BOOL "" FORCE)   # 不构建它自带的测试
    set(SDLTTF_SAMPLES   OFF CACHE BOOL "" FORCE)   # 不构建它自带的示例

    if(CJING_SDL3_TTF_SOURCE_DIR)
        set(FETCHCONTENT_SOURCE_DIR_SDL3_TTF "${CJING_SDL3_TTF_SOURCE_DIR}" CACHE PATH "" FORCE)
        message(STATUS "SDL3_ttf: 使用本地源码 ${CJING_SDL3_TTF_SOURCE_DIR}")
    endif()

    FetchContent_Declare(SDL3_ttf
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
        GIT_TAG        main
        GIT_SHALLOW    TRUE
        GIT_SUBMODULES_RECURSE TRUE)
    # GIT_SUBMODULES_RECURSE TRUE 不可省略：
    #   SDL_ttf 的 FreeType/HarfBuzz 是通过 git submodule 引入的，
    #   不递归拉取子模块，配置阶段会因为缺少 external/freetype 而失败。

    FetchContent_MakeAvailable(SDL3_ttf)
    # 得到 SDL3_ttf::SDL3_ttf 目标
endif()

# ------------------------------------------------------------------ Catch2
if(CJING_BUILD_TESTS)
    if(CJING_CATCH2_SOURCE_DIR)
        set(FETCHCONTENT_SOURCE_DIR_CATCH2 "${CJING_CATCH2_SOURCE_DIR}" CACHE PATH "" FORCE)
        set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)
        message(STATUS "Catch2: 使用本地源码 ${CJING_CATCH2_SOURCE_DIR}")
    endif()

    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1
        GIT_SHALLOW    TRUE)
    # 这里可以用 tag（v3.7.1）而不是分支：Catch2 的发布 tag 是稳定的，
    # 而且它没有"必须跟随最新版"的需求（与 SDL3 不同）。

    FetchContent_MakeAvailable(Catch2)
endif()

# =============================================================================
#  五、子目录
#
#  分层原则：
#    根 CMakeLists 只负责"有哪些依赖、有哪些可选开关"；
#    子目录只负责"我这个目标由哪些源文件组成、要开什么告警"。
#  这样依赖关系集中可见，不会散落在各处。
# =============================================================================

# enable_testing() 必须出现在【顶层】，不能只在 core/tests 里调用。
# 否则 ctest 在构建根目录看不到任何测试（报 "No tests were found!!!"）。
enable_testing()

add_subdirectory(core)          # 纯逻辑：ttt_core + 单元测试 + 纯净性检查
add_subdirectory(platform_sdl)  # SDL3 实现：渲染、应用外壳、自测

# 学习用示例：纯 C、直接调 SDL3，刻意不依赖本项目的抽象层
if(CJING_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# =============================================================================
#  六、配置摘要
#
#  为什么值得花几十行打印摘要：
#  排查"为什么它去联网了 / 为什么用的是旧版本"时，这几十行能省掉十分钟以上的猜测。
# =============================================================================
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
message(STATUS "  可选开关      : SANITIZE=${CJING_SANITIZE}  WERROR=${CJING_WERROR}  OFFLINE=${CJING_OFFLINE}  EXAMPLES=${CJING_BUILD_EXAMPLES}")
message(STATUS "============================================")
message(STATUS "")

# 走网络时给出提示，避免用户在受限网络下反复重试却不知道原因
if(NOT CJING_OFFLINE AND CJING_FETCH_SDL3 AND "${CJING_SDL3_SOURCE_DIR}" STREQUAL "")
    message(STATUS "提示：未找到 SDL3 本地缓存，本次将联网下载。")
    message(STATUS "      若网络受限，可把源码放到 ${CJING_CACHE_DIR}/sdl3-src，")
    message(STATUS "      或配置 -DCJING_SDL3_SOURCE_DIR=<目录>。")
endif()
