# core/ —— 纯 C++ 逻辑与平台抽象接口
# 铁律：本目标不得链接 SDL，也不得包含任何 SDL 头文件（路线图 1.1 架构原则 1）
add_library(ttt_core STATIC
    src/game.cpp
    src/layout.cpp
    src/io.cpp)

target_include_directories(ttt_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_compile_features(ttt_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(ttt_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(ttt_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
endif()

# 可验证完成标志（路线图 P1-T22）：core 目标不允许出现 SDL 依赖
add_custom_target(core_purity_check
    COMMAND ${CMAKE_COMMAND}
            -DCORE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P "${CMAKE_SOURCE_DIR}/cmake/CheckCorePurity.cmake"
    COMMENT "检查 core/ 中是否混入 SDL 依赖")

# 单元测试（可用 -DCJING_BUILD_TESTS=OFF 关闭）
option(CJING_BUILD_TESTS "Build unit tests" ON)
if(CJING_BUILD_TESTS)
    add_subdirectory(tests)
endif()
