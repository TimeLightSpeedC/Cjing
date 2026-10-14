# 校验 core/ 中不出现 SDL 依赖（路线图 1.1 架构原则 1 / P1-T22）
# 用法: cmake -DCORE_DIR=<path> -P CheckCorePurity.cmake

if(NOT DEFINED CORE_DIR)
    message(FATAL_ERROR "必须传入 -DCORE_DIR=<core 目录>")
endif()

file(GLOB_RECURSE _sources
    "${CORE_DIR}/*.hpp" "${CORE_DIR}/*.h"
    "${CORE_DIR}/*.cpp" "${CORE_DIR}/*.cc")

set(_violations "")
foreach(_f IN LISTS _sources)
    file(READ "${_f}" _content)
    # 匹配 #include <SDL...> 或 #include "SDL...
    if(_content MATCHES "#[ \t]*include[ \t]*[<\"]SDL")
        list(APPEND _violations "${_f}")
    endif()
endforeach()

if(_violations)
    string(REPLACE ";" "\n  " _pretty "${_violations}")
    message(FATAL_ERROR "core/ 中检测到 SDL 依赖（架构原则 1 被违反）:\n  ${_pretty}")
endif()

list(LENGTH _sources _n)
message(STATUS "core/ 纯净性检查通过：${_n} 个文件，无 SDL 依赖")
