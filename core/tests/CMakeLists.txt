# =============================================================================
#  core/tests —— 单元测试（Catch2）
#
#  ▍分工说明
#    Catch2 的获取、本地缓存解析都在【根 CMakeLists】完成，
#    本文件只声明测试目标。这样做的原因：
#      · 依赖来源只在一处管理，避免同一依赖被声明多次（CMake 会警告/行为异常）
#      · 关闭测试时（CJING_BUILD_TESTS=OFF）根目录根本不会去拉 Catch2，省一次下载
#
#  ▍测试的三个层次（本项目目前覆盖前两层）
#    1. 纯逻辑：胜负/平局/非法落子/存档往返   -> 可在任何平台跑
#    2. 布局几何：格子命中、安全区、往返映射   -> 不需要窗口
#    3. 渲染与事件：见 platform_sdl/selftest.cpp（需要 SDL，但可离屏）
# =============================================================================

# Catch2 的部分行为受宏控制。这里显式关闭 POSIX 信号处理：
# 调试器接管信号时，某些 CI 环境会因此卡住或误报。
set(CATCH_CONFIG_NO_POSIX_SIGNALS OFF)

# 前置断言：如果根目录没能提供 Catch2 目标，直接报错并指出原因。
# 比让用户对着一堆"找不到 Catch2::Catch2WithMain"的链接错误猜要友好得多。
if(NOT TARGET Catch2::Catch2WithMain)
    message(FATAL_ERROR
        "未找到 Catch2 目标。请确认根 CMakeLists 中 CJING_BUILD_TESTS=ON，"
        "且 Catch2 已成功获取（或在 ${CJING_CACHE_DIR}/catch2-src 提供本地副本）。")
endif()

add_executable(ttt_tests
    test_game.cpp     # 8 条获胜线、平局、非法落子、回合切换
    test_layout.cpp   # 正方形/居中/安全区/格子往返映射/退化输入
    test_io.cpp
    test_text.cpp)    # 文案表完整性：无空条目、无重复、越界安全      # 存档序列化往返、损坏存档拒绝、状态由棋盘推导
# 注意：上面三行的闭合括号必须在注释之外，否则会被当成注释内容，报
#       "Parse error. Function missing ending ')'"。这是 CMake 注释的常见陷阱：
#       行内 # 之后直到行尾，全部被视为注释。

# Catch2WithMain 自带 main()，所以测试文件里不需要写 main。
target_link_libraries(ttt_tests
    PRIVATE ttt_core
            Catch2::Catch2WithMain)

target_compile_features(ttt_tests PRIVATE cxx_std_20)

cjing_set_warnings(ttt_tests)
cjing_enable_sanitizers(ttt_tests)

# 注册到 CTest：之后可用 ctest 统一运行，也可被 CI 直接调用。
add_test(NAME ttt_core_tests COMMAND ttt_tests)
