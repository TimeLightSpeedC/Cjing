# platform_sdl/ —— SDL3 实现（渲染 / 输入 / 资源 / 存档 / 生命周期）
#
# 依赖（SDL3、SDL3_ttf）的获取与本地缓存解析已在根 CMakeLists 完成，
# 这里只负责构建目标并链接既有 target。

add_library(ttt_platform_sdl STATIC
    app.cpp
    sdl_view.cpp)

target_include_directories(ttt_platform_sdl PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}")

# SDL3 必须 PUBLIC：app.hpp 公开 include <SDL3/SDL.h>，
# 消费者（可执行文件）需要继承其头文件搜索路径与链接依赖。
target_link_libraries(ttt_platform_sdl
    PUBLIC  ttt_core
            SDL3::SDL3)

# ---------------------------------------------------------------- 文字渲染
if(CJING_WITH_TTF AND TARGET SDL3_ttf::SDL3_ttf)
    target_link_libraries(ttt_platform_sdl PUBLIC SDL3_ttf::SDL3_ttf)
    target_compile_definitions(ttt_platform_sdl PUBLIC CJING_HAVE_TTF=1)
    message(STATUS "SDL3_ttf: 已启用文字渲染")
elseif(CJING_WITH_TTF)
    message(WARNING "SDL3_ttf: 目标不可用，文字渲染将被禁用（界面将只显示几图形状）")
endif()

cjing_set_warnings(ttt_platform_sdl)
cjing_enable_sanitizers(ttt_platform_sdl)

# ------------------------------------------------------------------ 可执行
if(CJING_BUILD_DESKTOP_APP)
    add_executable(CjingApp main_callbacks.cpp)
    target_link_libraries(CjingApp PRIVATE ttt_platform_sdl)
    cjing_enable_sanitizers(CjingApp)
endif()

# ---------------------------------------------------- 无界面自测（可入 CI）
if(CJING_BUILD_SELFTEST)
    add_executable(CjingSelfTest selftest.cpp)
    target_link_libraries(CjingSelfTest PRIVATE ttt_platform_sdl)
    cjing_set_warnings(CjingSelfTest)
    cjing_enable_sanitizers(CjingSelfTest)
    add_test(NAME cjing_selftest COMMAND CjingSelfTest)
endif()
