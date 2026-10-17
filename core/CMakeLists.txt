# core/ —— 纯 C++ 逻辑与平台抽象接口
#
# 铁律：本目标不得链接 SDL，也不得包含任何 SDL 头文件（架构原则 1）。
# 这条规则由下面的 core_purity_check 目标自动校验，不要靠人工审查。

add_library(ttt_core STATIC
    src/game.cpp
    src/layout.cpp
    src/io.cpp)

target_include_directories(ttt_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_compile_features(ttt_core PUBLIC cxx_std_20)

cjing_set_warnings(ttt_core)
cjing_enable_sanitizers(ttt_core)

# ---------------------------------------------------------------------------
# 可验证完成标志（P1-T22）：core/ 中不允许出现 SDL 依赖
#
# 用法：cmake --build <build> --target core_purity_check
# 检查逻辑见 cmake/CheckCorePurity.cmake（扫描 #include <SDL...>
# 与 #include "SDL..."，命中即失败并列出违规文件）。
# ---------------------------------------------------------------------------
add_custom_target(core_purity_check
    COMMAND ${CMAKE_COMMAND}
            -DCORE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P "${CMAKE_SOURCE_DIR}/cmake/CheckCorePurity.cmake"
    COMMENT "检查 core/ 中是否混入 SDL 依赖"
    VERBATIM)

# 单元测试（开关在根 CMakeLists：CJING_BUILD_TESTS）
if(CJING_BUILD_TESTS)
    add_subdirectory(tests)
endif()
