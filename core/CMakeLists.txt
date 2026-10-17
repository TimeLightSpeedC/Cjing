# =============================================================================
#  core/ —— 纯 C++ 逻辑与平台抽象接口
#
#  ▍本目录的铁律
#    不链接 SDL，不含任何 SDL 头文件。因为：
#      · 它要在鸿蒙/Android/iOS 上原样复用
#      · 它要能脱离窗口被单元测试覆盖
#    这条规则由 core_purity_check 目标自动校验，不要靠人工审查。
#
#  ▍为什么这里只有三行源文件
#    逻辑（game）、布局（layout）、存档序列化（io）被刻意拆成三个编译单元，
#    彼此不依赖。好处是改一个不会牵连其它，编译也更快。
# =============================================================================

add_library(ttt_core STATIC
    src/game.cpp
    src/layout.cpp
    src/io.cpp)

# PUBLIC 的含义：所有链接 ttt_core 的目标都自动获得这个头文件搜索路径，
# 不需要各自再写一遍 include_directories。
target_include_directories(ttt_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

# 用 PUBLIC 传播 C++20 要求：谁链接 ttt_core，谁就必须用 C++20 编译。
# 这比全局设置 CMAKE_CXX_STANDARD 更精确（只约束真正使用它的目标）。
target_compile_features(ttt_core PUBLIC cxx_std_20)

# 告警与 Sanitizer 只作用于本项目代码，不影响第三方依赖。
# 具体开关与理由见 cmake/CjingCompiler.cmake。
cjing_set_warnings(ttt_core)
cjing_enable_sanitizers(ttt_core)

# -----------------------------------------------------------------------------
#  可验证完成标志：core/ 中不允许出现 SDL 依赖
#
#  用法：cmake --build <build> --target core_purity_check
#
#  这是一个 add_custom_target：不打进构建图，只能被显式调用
#  （CI 里单独一步执行）。检查逻辑见 cmake/CheckCorePurity.cmake。
#
#  VERBATIM 的作用：原样传递参数，避免路径中的空格/分号被 CMake 二次解释。
# -----------------------------------------------------------------------------
add_custom_target(core_purity_check
    COMMAND ${CMAKE_COMMAND}
            -DCORE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P "${CMAKE_SOURCE_DIR}/cmake/CheckCorePurity.cmake"
    COMMENT "检查 core/ 中是否混入 SDL 依赖"
    VERBATIM)

# 单元测试作为子目录加入（开关在根 CMakeLists：CJING_BUILD_TESTS）。
# 用子目录而不是把测试写在这里，是为了让"产品代码"与"测试代码"物理分开。
if(CJING_BUILD_TESTS)
    add_subdirectory(tests)
endif()
