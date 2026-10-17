# =============================================================================
#  cmake/CjingDeps.cmake —— 第三方依赖解析
#
#  ▍要解决的问题
#    本项目依赖 SDL3 / SDL3_ttf / Catch2，合计 200MB+。开发者的网络对
#    github.com:443 时通时断，因此"每次配置都联网"是不可接受的。
#
#  ▍CMake FetchContent 的默认行为（踩过的坑）
#    1. 若判定"没有源码"，它会 git clone —— 网络不通就直接失败。
#    2. 若 <build>/_deps/<name>-src 已存在，且 GIT_TAG 是【分支名】，
#       它每次 configure 都会 git fetch 检查更新 —— 即使源码就在本地。
#       网络受限时报错形如：
#         sdl3-populate-gitupdate.cmake ... Failed to connect to github.com port 443
#
#  ▍解法
#    显式把本地源码目录交给 FetchContent：设置 FETCHCONTENT_SOURCE_DIR_<NAME>。
#    一旦该变量指向有效目录，FetchContent 会【整体跳过下载与更新步骤】，
#    于是离线也能配置成功。这也是 CMake 官方文档推荐的做法。
#
#  ▍约定
#    <name> 必须与 FetchContent_Declare 的名字大小写一致；变量名要转大写。
#    例如声明名 SDL3_ttf  ->  FETCHCONTENT_SOURCE_DIR_SDL3_TTF
# =============================================================================

#-----------------------------------------------------------------------------
# cjing_detect_dep(<显示名> <变量名> [候选目录...])
#
# 功能：决定某个依赖"从哪来"，并把结果写入同名 CACHE 变量。
#
# 解析优先级（这个顺序很重要）：
#   1. 用户在命令行 -D 显式指定 —— 用户意图最高，允许覆盖任何自动探测
#   2. 候选目录中第一个含 CMakeLists.txt 的
#   3. 都没有 -> 置空，由调用方决定联网或报错
#
# 为什么用 CACHE 变量存结果：
#   ① 配置完成后可留在 CMakeCache.txt 里，便于事后确认"到底用了哪个目录"
#   ② 在 CLion / cmake-gui 中能看到并直接修改，不需要重新记命令行参数
#-----------------------------------------------------------------------------
function(cjing_detect_dep dep_name var_name)
    set(_candidates ${ARGN})

    # ---- 1) 用户显式指定 ----
    if(DEFINED ${var_name} AND NOT "${${var_name}}" STREQUAL "")
        if(EXISTS "${${var_name}}/CMakeLists.txt")
            message(STATUS "依赖 ${dep_name}: 使用指定源码 -> ${${var_name}}")
            set(${var_name} "${${var_name}}" PARENT_SCOPE)
            return()
        endif()
        # 指定了但无效：不直接失败，而是警告后继续探测。
        # 理由：路径可能因为换机器而失效，回退到缓存往往仍能工作。
        message(WARNING
            "依赖 ${dep_name}: 指定的源码目录无效（缺少 CMakeLists.txt）：${${var_name}}\n"
            "将回退到缓存探测。")
    endif()

    # ---- 2) 缓存探测 ----
    # 判定标准是"存在 CMakeLists.txt"而不是"目录存在"：
    # 半途中断的克隆会留下空目录，只看目录存在会把坏路径当成好路径。
    foreach(_cand IN LISTS _candidates)
        if(EXISTS "${_cand}/CMakeLists.txt")
            set(${var_name} "${_cand}" CACHE PATH "依赖 ${dep_name} 的本地源码目录" FORCE)
            message(STATUS "依赖 ${dep_name}: 命中本地缓存 -> ${_cand}")
            return()
        endif()
    endforeach()

    # ---- 3) 无缓存 ----
    set(${var_name} "" PARENT_SCOPE)
    message(STATUS "依赖 ${dep_name}: 本地无缓存，将尝试从网络获取")
endfunction()

#-----------------------------------------------------------------------------
# cjing_prepare_dep(<FetchContent 声明名> <变量名>)
#
# 功能：把"已解析到的本地源码"接入 FetchContent，并关闭联网更新。
# 变量为空时什么都不做（该依赖会走正常的联网流程）。
#-----------------------------------------------------------------------------
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

    # 核心：告诉 FetchContent 源码已就位，跳过下载
    set(FETCHCONTENT_SOURCE_DIR_${_upper} "${_dir}" CACHE PATH "" FORCE)

    # 双保险：即便某些路径仍触发 update 流程，也禁止其联网。
    # 注意 _<NAME> 后缀是"仅对该依赖生效"，不带后缀是全局开关。
    set(FETCHCONTENT_UPDATES_DISCONNECTED_${_upper} ON CACHE BOOL "" FORCE)
    set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "" FORCE)

    message(STATUS "依赖 ${fc_name}: 已切换到本地源码（跳过下载与更新）")
endfunction()

#-----------------------------------------------------------------------------
# cjing_require_offline([变量名...])
#
# 仅在 CJING_OFFLINE=ON 时生效：若任一依赖未解析到本地源码，就立刻中止。
#
# 为什么要主动失败：
#   否则 CMake 会尝试联网，在受限网络下要等几十秒才超时，
#   而且报错信息埋在 FetchContent 的调用栈里，难以判断该补什么。
#   这里直接给出三条可执行的补救路径。
#-----------------------------------------------------------------------------
function(cjing_require_offline)
    if(NOT CJING_OFFLINE)
        return()
    endif()
    foreach(_var IN LISTS ARGN)
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
