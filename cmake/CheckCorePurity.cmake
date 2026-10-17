# =============================================================================
#  cmake/CheckCorePurity.cmake —— 架构守卫：core/ 不得依赖 SDL
#
#  ▍为什么要有这个脚本
#    本项目的核心设计是"逻辑与平台分离"：core/ 是可测试、可移植的纯 C++，
#    不依赖 SDL。这个约束靠人自觉是守不住的 —— 某天为了图快在 core 里
#    include 一个 SDL 头文件，整个抽象层就废了（鸿蒙/Android 端将无法复用）。
#    -> 把规则变成可执行检查，纳入 CI，违反即构建失败。
#
#  ▍用法（由 core_purity_check 目标调用）
#    cmake --build <build> --target core_purity_check
#    等价于：cmake -DCORE_DIR=<core 目录> -P CheckCorePurity.cmake
#
#  ▍为什么用 -P 脚本模式
#    它不参与项目构建图，只做纯文本扫描，速度快且不需要在配置阶段介入。
# =============================================================================

# 参数校验：缺少 CORE_DIR 说明调用方写错了，直接失败而不是静默"通过"。
if(NOT DEFINED CORE_DIR)
    message(FATAL_ERROR "必须传入 -DCORE_DIR=<core 目录>")
endif()

# 递归收集所有源码与头文件。
# GLOB_RECURSE 的已知缺陷是"新增文件不会自动触发重新配置"，
# 但这里是每次显式 --target 时重新执行脚本，所以不受影响。
file(GLOB_RECURSE _sources
    "${CORE_DIR}/*.hpp" "${CORE_DIR}/*.h"
    "${CORE_DIR}/*.cpp" "${CORE_DIR}/*.cc")

set(_violations "")

foreach(_f IN LISTS _sources)
    file(READ "${_f}" _content)
    # 匹配 #include <SDL...> 或 #include "SDL..."
    # 用 [ \t]* 容忍 # 与 include 之间的空白：
    #   #include <SDL3/SDL.h>   /   #  include "SDL3/SDL.h"
    if(_content MATCHES "#[ \t]*include[ \t]*[<\"]SDL")
        list(APPEND _violations "${_f}")
    endif()
endforeach()

if(_violations)
    # 列出全部违规文件，而不是只报第一个 —— 一次改完比反复试错省时间。
    string(REPLACE ";" "\n  " _pretty "${_violations}")
    message(FATAL_ERROR
        "core/ 中检测到 SDL 依赖（架构原则 1 被违反）:\n  ${_pretty}\n"
        "正确做法：平台相关代码放到 platform_sdl/，core/ 只保留接口与纯逻辑。")
endif()

list(LENGTH _sources _n)
message(STATUS "core/ 纯净性检查通过：${_n} 个文件，无 SDL 依赖")
