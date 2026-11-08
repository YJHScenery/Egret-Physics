# /** @file ControlDependencies.cmake @brief 为共享控制契约补充显式对象依赖，防止增量构建混用结构布局。 */
include_guard(GLOBAL)

# /** @brief 为指定目标的项目 C++ 源码登记控制契约头文件依赖。 @param target 需要同步重编译的构建目标。 */
function(egret_apply_control_dependencies target)
    get_filename_component(control_project_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    set(control_contract_headers
        "${control_project_root}/src/control_core/control_program.h"
        "${control_project_root}/src/control_core/control_abi.h"
        "${control_project_root}/src/model/control/scene_control.h"
        "${control_project_root}/src/model/control/predicate_program.h"
        "${control_project_root}/src/view_model/serialize/scene_settings.h")
    get_target_property(control_target_directory "${target}" SOURCE_DIR)
    get_target_property(control_target_sources "${target}" SOURCES)
    foreach(control_source IN LISTS control_target_sources)
        if(control_source MATCHES "\\.(cpp|cxx|cc)$" AND NOT control_source MATCHES "^\\$<")
            get_filename_component(control_source_path "${control_source}" ABSOLUTE BASE_DIR "${control_target_directory}")
            # 不改变第三方源码的构建依赖；项目源文件与测试保守同步，避免间接包含漏报。
            if(control_source_path MATCHES "^${control_project_root}/(src|tests)/")
                set_property(SOURCE "${control_source_path}" DIRECTORY "${control_target_directory}"
                    APPEND PROPERTY OBJECT_DEPENDS "${control_contract_headers}")
            endif()
        endif()
    endforeach()
endfunction()
