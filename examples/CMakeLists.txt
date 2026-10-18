# =============================================================================
#  examples/ —— 学习用示例（不参与产品构建，但会被 CI 编译以确认没腐烂）
#
#  和 core/、platform_sdl/ 的区别：
#    · 主程序是 C++20 + 自家抽象层
#    · 示例是【纯 C】+ 直接用 SDL3 裸接口 —— 目的就是让你看清 SDL 本身的样子
#  因此它不链接 ttt_core / ttt_platform_sdl，只链接 SDL3::SDL3。
# =============================================================================

add_executable(CjingMinimal
    minimal/main.c)

# PUBLIC 继承 SDL3 的头文件路径与链接依赖
target_link_libraries(CjingMinimal PRIVATE SDL3::SDL3)

# 示例是 C 代码，用 C 的告警集（不要套用 C++ 的 -Wshadow 等策略）
if(MSVC)
    target_compile_options(CjingMinimal PRIVATE /W4)
else()
    target_compile_options(CjingMinimal PRIVATE -Wall -Wextra -Wpedantic)
endif()

cjing_enable_sanitizers(CjingMinimal)

message(STATUS "示例: 已加入 CjingMinimal（纯 C / 直接使用 SDL3）")
