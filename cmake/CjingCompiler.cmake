# cmake/CjingCompiler.cmake
#
# 统一的编译告警与可选静态分析配置。
# 目的：避免各子目录重复写 if(MSVC) ... else() ... ，并保证标准一致。

#---------------------------------------------------------------------------
# cjing_set_warnings(<target>)
# 为本项目自己的代码开启常见告警（不施加到第三方依赖）。
# 注意：第三方的 harfbuzz 等会自带告警，故不要全局 set(CMAKE_CXX_FLAGS)。
#---------------------------------------------------------------------------
function(cjing_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        # 可选：消除常见噪音
        if(CJING_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow)
        if(CJING_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

#---------------------------------------------------------------------------
# cjing_enable_sanitizers(<target>)
# 仅在 CJING_SANITIZE=ON 时生效；用于本地与 CI 的内存/未定义行为检查。
# 参数：
#   SANITIZERS  <list>  默认 "address;undefined"
#---------------------------------------------------------------------------
function(cjing_enable_sanitizers target)
    if(NOT CJING_SANITIZE)
        return()
    endif()
    cmake_parse_arguments(ARG "" "" "SANITIZERS" ${ARGN})
    if(NOT ARG_SANITIZERS)
        set(ARG_SANITIZERS address undefined)
    endif()

    if(MSVC)
        # MSVC 不支持 GCC 风格 -fsanitize，建议改用 ASan 专用构建配置
        message(STATUS "CJING_SANITIZE 在 MSVC 下未启用（请使用 /fsanitize=address 配置）")
        return()
    endif()

    string(REPLACE ";" "," _san "${ARG_SANITIZERS}")
    target_compile_options(${target} PRIVATE -fsanitize=${_san} -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=${_san})
endfunction()
