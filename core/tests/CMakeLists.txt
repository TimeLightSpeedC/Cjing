# core/tests —— 单元测试（Catch2）
#
# Catch2 的获取与本地缓存解析已上移到根 CMakeLists，
# 这里只负责声明测试目标，避免同一依赖在多处重复声明。

set(CATCH_CONFIG_NO_POSIX_SIGNALS OFF)

if(NOT TARGET Catch2::Catch2WithMain)
    message(FATAL_ERROR
        "未找到 Catch2 目标。请确认根 CMakeLists 中 CJING_BUILD_TESTS=ON，"
        "且 Catch2 已成功获取（或在 ${CJING_CACHE_DIR}/catch2-src 提供本地副本）。")
endif()

add_executable(ttt_tests
    test_game.cpp
    test_layout.cpp
    test_io.cpp)

target_link_libraries(ttt_tests
    PRIVATE ttt_core
            Catch2::Catch2WithMain)

target_compile_features(ttt_tests PRIVATE cxx_std_20)

cjing_set_warnings(ttt_tests)
cjing_enable_sanitizers(ttt_tests)

add_test(NAME ttt_core_tests COMMAND ttt_tests)
