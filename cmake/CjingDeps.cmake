# cmake/CjingDeps.cmake
#
# 第三方依赖解析：统一实现"本地缓存优先，网络兜底"。
#
# 为什么需要这一层（来自实际踩坑）：
#   1. SDL3 / SDL3_ttf / Catch2 体量合计 200MB+，而本项目的开发网络对 github.com:443
#      连通性不稳定（常被阻断或传输中断）。
#   2. 若 CMake 判定"没有源码"，FetchContent 会尝试克隆；而在已经存在
#      <build>/_deps/<name>-src 的情况下，它又会执行 git fetch 试图更新分支，
#      报错形如：sdl3-populate-gitupdate.cmake ... Failed to connect to github.com port 443。
#      —— 也就是说：只要 GIT_TAG 是分支名，即使源码已在本地，每次 configure 都要联网。
#   3. 解决办法：显式把本地源码目录交给 FetchContent（FETCHCONTENT_SOURCE_DIR_<NAME>），
#      这样下载/更新步骤会被整体跳过，离线也能打开工程。
#
# 使用方式（通常在根 CMakeLists 中）：
#   set(CJING_CACHE_DIR "${CMAKE_SOURCE_DIR}/.tools" CACHE PATH "...")
#   include(cmake/CjingDeps.cmake)
#   cjing_detect_dep(SDL3 CJING_SDL3_SOURCE_DIR
#       "${CJING_CACHE_DIR}/sdl3-src"
#       "${CJING_CACHE_DIR}/sdl3probe/build/_deps/sdl3-src")
#   cjing_prepare_dep(SDL3 CJING_SDL3_SOURCE_DIR)

#---------------------------------------------------------------------------
# cjing_detect_dep(<显示名> <变量名> [候选目录...])
#
# 解析顺序：
#   1. 若 <变量名> 已被用户显式指定，则校验后采用（-D 指定优先）
#   2. 依次检查候选目录，命中第一个含 CMakeLists.txt 的目录
#   3. 都不命中则置空，交由 FetchContent 联网获取
#
# 命中时会把结果写入同名 CACHE 变量，便于在 CMake GUI / CLion 中查看与覆盖。
#---------------------------------------------------------------------------
function(cjing_detect_dep dep_name var_name)
    set(_candidates ${ARGN})

    # --- 1) 用户显式指定 ---
    if(DEFINED ${var_name} AND NOT "${${var_name}}" STREQUAL "")
        if(EXISTS "${${var_name}}/CMakeLists.txt")
            message(STATUS "依赖 ${dep_name}: 使用指定源码 -> ${${var_name}}")
            set(${var_name} "${${var_name}}" PARENT_SCOPE)
            return()
        endif()
        message(WARNING
            "依赖 ${dep_name}: 指定的源码目录无效（缺少 CMakeLists.txt）：${${var_name}}\n"
            "将回退到缓存探测。")
    endif()

    # --- 2) 缓存探测 ---
    foreach(_cand IN LISTS _candidates)
        if(EXISTS "${_cand}/CMakeLists.txt")
            set(${var_name} "${_cand}" CACHE PATH "依赖 ${dep_name} 的本地源码目录" FORCE)
            message(STATUS "依赖 ${dep_name}: 命中本地缓存 -> ${_cand}")
            return()
        endif()
    endforeach()

    # --- 3) 无缓存 ---
    set(${var_name} "" PARENT_SCOPE)
    message(STATUS "依赖 ${dep_name}: 本地无缓存，将尝试从网络获取")
endfunction()

#---------------------------------------------------------------------------
# cjing_prepare_dep(<FetchContent 声明名> <变量名>)
#
# 若 <变量名> 指向有效源码目录，则：
#   - 设置 FETCHCONTENT_SOURCE_DIR_<NAME>，令 FetchContent 跳过下载
#   - 关闭该依赖与全局的联网更新（UPDATES_DISCONNECTED）
# 变量名为空时不做任何事，由 FetchContent 正常联网获取。
#
# 注意：<NAME> 必须与 FetchContent_Declare 使用的大小写一致，此处按大写处理。
#---------------------------------------------------------------------------
function(cjing_prepare_dep fc_name dir_var)
    set(_dir "${${dir_var}}")
    if("${_dir}" STREQUAL "")
        return()
    endif()

    if(NOT EXISTS "${_dir}/CMakeLists.txt")
        message(FATAL_ERROR
            "cjing_prepare_dep: ${dir_var} 指向的目录无效（缺少 CMakeLists.txt）\n"
            "  ${_dir}")
    endif()

    string(TOUPPER "${fc_name}" _upper)
    set(FETCHCONTENT_SOURCE_DIR_${_upper} "${_dir}" CACHE PATH "" FORCE)
    set(FETCHCONTENT_UPDATES_DISCONNECTED_${_upper} ON CACHE BOOL "" FORCE)
    # 全局兜底：避免其它未显式屏蔽的依赖也去联网更新
    set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)
    message(STATUS "依赖 ${fc_name}: 已切换到本地源码（跳过下载与更新）")
endfunction()

#---------------------------------------------------------------------------
# cjing_require_offline(<dep1> [dep2...])
#
# 在 CJING_OFFLINE=ON 时，检查各依赖是否都已解析到本地源码；
# 若有缺失则给出可执行的补救提示后中止，而不是让 FetchContent 去慢慢超时。
# 参数为变量名（如 CJING_SDL3_SOURCE_DIR）。
#---------------------------------------------------------------------------
function(cjing_require_offline)
    if(NOT CJING_OFFLINE)
        return()
    endif()
    foreach(_var IN LISTS ARGN)
        # 允许为空：调用方按需传入
        if("${${_var}}" STREQUAL "")
            message(FATAL_ERROR
                "CJING_OFFLINE=ON，但 ${_var} 未解析到本地源码。\n"
                "请任选其一：\n"
                "  1) 把依赖源码放入 ${CJING_CACHE_DIR}/ 下的约定目录；\n"
                "  2) 用 -D${_var}=<源码目录> 显式指定；\n"
                "  3) 去掉 CJING_OFFLINE 以允许联网下载。")
        endif()
    endforeach()
endfunction()
