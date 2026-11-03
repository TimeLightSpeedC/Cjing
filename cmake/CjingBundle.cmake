# =============================================================================
#  cmake/CjingBundle.cmake —— 运行时依赖处理（路线图 P2-T21）
#
#  ▍要解决的问题
#    编译出来的可执行文件依赖 SDL3 / SDL3_ttf 的【动态库】。
#    默认情况下，这些库的查找路径写的是【构建目录的绝对路径】：
#
#        otool -l build/bin/CjingApp  →  LC_RPATH  /Users/你/.../build/lib
#
#    于是把 CjingApp 拷到别的机器（或别的目录）就会报"找不到库"。
#
#  ▍解决思路（两步）
#    ① 把运行时库复制到可执行文件【旁边】
#    ② 把查找路径改成"相对于可执行文件自身"：
#         macOS : @executable_path
#         Linux : $ORIGIN          （ELF 的"本文件所在目录"）
#         Windows: 不需要 —— 系统默认先搜索 exe 所在目录
#
#    做完之后，整个输出目录就是【可搬移】的：拷到哪都能跑。
# =============================================================================

# 收集本工程需要随包分发的运行时库文件
#
# 注意 macOS/Linux 用 TARGET_SONAME_FILE 而不是 TARGET_FILE：
#   动态库的真实文件名通常是 libSDL3.0.501.0.dylib，
#   而程序依赖记录的是 SONAME（libSDL3.0.dylib），
#   两者之间靠符号链接关联。复制真实文件会缺少符号链接，加载器就找不到。
#   TARGET_SONAME_FILE 给出的正是"加载器会去找的那个名字"。
function(cjing_runtime_dep_files out_var)
    set(_files)
        foreach(_dep IN ITEMS SDL3::SDL3 SDL3_ttf::SDL3_ttf)
            if(TARGET ${_dep})
                # 静态链接（CJING_STATIC=ON）时没有运行时库可复制：
                # 静态库没有 SONAME，$<TARGET_SONAME_FILE:...> 会直接报错，
                # 因此按目标类型过滤。
                get_target_property(_dep_type ${_dep} TYPE)
                if(NOT _dep_type STREQUAL "SHARED_LIBRARY")
                    continue()
                endif()

                if(WIN32)
                    list(APPEND _files $<TARGET_FILE:${_dep}>)
                else()
                    list(APPEND _files $<TARGET_SONAME_FILE:${_dep}>)
                endif()
            endif()
        endforeach()
    set(${out_var} "${_files}" PARENT_SCOPE)
endfunction()

# 让某个可执行目标"自带运行时依赖"
function(cjing_bundle_runtime target)
    # ① 把库搜索路径改成相对于可执行文件
    if(APPLE)
        set_target_properties(${target} PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "@executable_path")
    elseif(UNIX)
        set_target_properties(${target} PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "$ORIGIN")
    endif()
    # Windows 无需 rpath：DLL 搜索顺序里 exe 所在目录优先级很高

    # ② 构建完成后把依赖库复制到输出目录
    cjing_runtime_dep_files(_deps)
    if(_deps)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_deps}
                    "$<TARGET_FILE_DIR:${target}>"
            COMMAND_EXPAND_LISTS
            VERBATIM
            COMMENT "复制运行时依赖到 ${target} 的输出目录")
    endif()
endfunction()
