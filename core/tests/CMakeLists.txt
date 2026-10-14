# core/tests —— 单元测试（必做，路线图 P1-T12）
include(FetchContent)

# Catch2 v3：与 CMake 4.x 相容（cmake_minimum_required 3.16）
# 浅克隆：本环境大文件传输不稳定（路线图陷阱 E2）
set(CATCH_CONFIG_NO_POSIX_SIGNALS OFF)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE)

# 与 SDL3 同样支持复用本地克隆（本环境大文件传输不稳定，见路线图陷阱 E2）
set(CJING_CATCH2_SOURCE_DIR "" CACHE PATH "已有 Catch2 源码目录（跳过下载）")
if(CJING_CATCH2_SOURCE_DIR)
    set(FETCHCONTENT_SOURCE_DIR_CATCH2 "${CJING_CATCH2_SOURCE_DIR}" CACHE PATH "" FORCE)
    message(STATUS "Catch2: 复用本地源码目录 ${CJING_CATCH2_SOURCE_DIR}")
endif()

FetchContent_MakeAvailable(Catch2)

add_executable(ttt_tests
    test_game.cpp
    test_layout.cpp
    test_io.cpp)

target_link_libraries(ttt_tests
    PRIVATE ttt_core
            Catch2::Catch2WithMain)

target_compile_features(ttt_tests PRIVATE cxx_std_20)

if(NOT MSVC)
    target_compile_options(ttt_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME ttt_core_tests COMMAND ttt_tests)
