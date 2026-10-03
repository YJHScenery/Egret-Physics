cmake_minimum_required(VERSION 3.30.1)
project(Egret_Physics)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "D:/Qt/6.9.3/msvc2022_64")

option(BUILD_DOCS "Build documentation with Doxygen" ON)


# ========== 获取Git信息 =========
# 获取完整提交哈希（40位）
execute_process(
        COMMAND git rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)

# 获取短哈希（8位）
execute_process(
        COMMAND git rev-parse --short=8 HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_SHORT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)

# 获取分支名
execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)

# 获取最近的标签
execute_process(
        COMMAND git describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)

# 检查是否有未提交的修改
execute_process(
        COMMAND git status --porcelain
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DIRTY
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)

# 如果存在未提交修改，添加-dirty标记
if(GIT_DIRTY)
    set(GIT_COMMIT_SHORT "${GIT_COMMIT_SHORT}-dirty")
    set(GIT_COMMIT_HASH "${GIT_COMMIT_HASH}-dirty")
endif()

# 获取构建时间
string(TIMESTAMP BUILD_TIME "%Y-%m-%d %H:%M:%S")

# 如果没有获取到Git信息，设置默认值
if(NOT GIT_COMMIT_SHORT)
    set(GIT_COMMIT_SHORT "unknown")
    set(GIT_COMMIT_HASH "unknown")
    set(GIT_BRANCH "unknown")
    set(GIT_TAG "unknown")
endif()

message(STATUS "Git Commit: ${GIT_COMMIT_SHORT}")
message(STATUS "Git Branch: ${GIT_BRANCH}")
message(STATUS "Git Tag: ${GIT_TAG}")

# ========== 生成版本头文件 ==========
configure_file(
        ${CMAKE_SOURCE_DIR}/src/utils/git_version.h.in
        ${CMAKE_BINARY_DIR}/generated/git_version.h
        @ONLY
)


if (BUILD_DOCS)
    find_package(Doxygen)
    if (DOXYGEN_FOUND)
        set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/Doxyfile)
        set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/docs)

        add_custom_target(doc_doxygen ALL
                COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_IN}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/doxygen
                COMMENT "Generating API documentation with Doxygen"
                VERBATIM)

        if(EXISTS "${DOXYGEN_OUT}/html")
            install(DIRECTORY ${DOXYGEN_OUT}/html
                    DESTINATION share/doc/${PROJECT_NAME})
        endif()
    else ()
        message(WARNING "Doxygen not found, documentation will not be built")
    endif ()
endif ()

include_directories(
        dependency/include
        dependency/include/assimp_6_0_2
        dependency/include/boost_1_90_0
        dependency/include/eigen_5_0_0
        dependency/include/magic_enum_0.9.8/magic_enum
        src
        src/model
        src/model/physics/constraints
        src/model/solver
        src/model/scene
        src/model/physics
        src/model/physics/collision
        src/model/geometry
        src/model/geometry/surface
        src/model/geometry/curve
        src/model/physics/rigid
        src/model/physics/field
        src/model/physics/rigid/shape
        src/model/physics/rigid/shape/standard
        src/view_model/
        src/model/strategy
        src/model/strategy/broad_phase_strategy
        src/model/strategy/contact_strategy
        src/model/strategy/integrator_strategy
        src/model/strategy/sleep_strategy
        src/view_model/simulator
        src/view_model/simulator/visual_item
        src/view_model/serialize
        src/view_model/simulation_serializer
        src/view_model/simulation_player
        src/view_model/editor
        src/view
        src/view/geometry/
        src/utils
        src/utils/io
        src/utils/command
)

set(QT_MODULES_LIST
        Core Gui Widgets Quick Qml Quick3D Sql Quick3DAssetImport QuickControls2 Multimedia
)

find_package(Qt6 COMPONENTS
        ${QT_MODULES_LIST}
        REQUIRED)

list(TRANSFORM QT_MODULES_LIST PREPEND "Qt::" OUTPUT_VARIABLE QT_MODULES_LIST_WITH_PREFIX)

set(QRC_FILES
        resources/global.qrc
        src/qml/qml.qrc
)

set(CPP_FILES
        src/main.cpp

        src/utils/constants.h
        src/utils/basic_utils.h
        src/utils/transform.h
        src/utils/transform.cpp
        src/utils/os/windows/windows_config.h
        src/utils/os/windows/windows_config.cpp
        src/utils/io/database_controller.cpp
        src/utils/io/database_controller.h
        src/utils/io/async_logger.cpp
        src/utils/io/async_logger.h
        src/utils/language/cpp_property.cpp

        src/utils/command/command_line.cpp
        src/utils/command/command_line.h
        src/utils/command/solve_command.cpp
        src/utils/command/solve_command.h
        src/utils/command/parse_command.cpp
        src/utils/command/parse_command.h

        src/model/geometry/curve/curve_base.h
        src/model/geometry/curve/curve_base.cpp
        src/model/geometry/curve/curve_bezier.h
        src/model/geometry/curve/curve_bezier.cpp
        src/model/geometry/curve/curve_conic_section.h
        src/model/geometry/curve/curve_conic_section.cpp
        src/model/geometry/curve/curve_helix.h
        src/model/geometry/curve/curve_helix.cpp

        src/model/physics/physics_utils.h
        src/model/physics/rigid/particle.cpp
        src/model/physics/rigid/particle.h
        src/model/physics/rigid/physical_entity.cpp
        src/model/physics/rigid/physical_entity.h
        src/model/physics/rigid/rigid_body.cpp
        src/model/physics/rigid/rigid_body.h

        src/model/physics/rigid/shape/standard/shape_base.cpp
        src/model/physics/rigid/shape/standard/shape_base.h
        src/model/physics/rigid/shape/standard/shape_sphere.cpp
        src/model/physics/rigid/shape/standard/shape_sphere.h
        src/model/physics/rigid/shape/standard/shape_spherical_shell.cpp
        src/model/physics/rigid/shape/standard/shape_spherical_shell.h
        src/model/physics/rigid/shape/standard/shape_box.cpp
        src/model/physics/rigid/shape/standard/shape_box.h
        src/model/physics/rigid/shape/standard/shape_cylinder.cpp
        src/model/physics/rigid/shape/standard/shape_cylinder.h
        src/model/physics/rigid/shape/standard/shape_ring.cpp
        src/model/physics/rigid/shape/standard/shape_ring.h
        src/model/physics/rigid/shape/standard/shape_disk.h
        src/model/physics/rigid/shape/standard/shape_disk.cpp
        src/model/physics/rigid/shape/standard/shape_cylindrical_shell.cpp
        src/model/physics/rigid/shape/standard/shape_cylindrical_shell.h
        src/model/physics/rigid/shape/standard/shape_rod.cpp
        src/model/physics/rigid/shape/standard/shape_rod.h

        src/model/physics/field/field_base.cpp
        src/model/physics/field/field_base.h
        src/model/physics/field/gravity_field.cpp
        src/model/physics/field/gravity_field.h
        src/model/physics/field/gravitational_field.cpp
        src/model/physics/field/gravitational_field.h

        src/model/physics/constraints/constraints_base.cpp
        src/model/physics/constraints/constraints_base.h
        src/model/physics/constraints/connecting_line.cpp
        src/model/physics/constraints/connecting_line.h
        src/model/physics/constraints/connecting_rod.cpp
        src/model/physics/constraints/connecting_rod.h
        src/model/physics/constraints/spring.cpp
        src/model/physics/constraints/spring.h

        src/model/solver/solver.h
        src/model/solver/solver.cpp
        src/model/solver/solver_scene_snapshot.h
        src/model/solver/solver_types.h
        src/model/scene/world_scene_manager.h
        src/model/scene/world_scene_manager.cpp
        src/model/scene/scene_convex_mesh_render_item.h
        src/model/scene/scene_curve_render_item.h

        src/model/strategy/broad_phase_strategy/broad_phase_strategy.h
        src/model/strategy/broad_phase_strategy/brute_force_broad_strategy.h
        src/model/strategy/broad_phase_strategy/brute_force_broad_strategy.cpp

        src/model/strategy/contact_strategy/contact_resolver_strategy.h
        src/model/strategy/contact_strategy/frictionless_contact_resolver.h
        src/model/strategy/contact_strategy/frictionless_contact_resolver.cpp
        src/model/strategy/contact_strategy/frictional_contact_resolver.h
        src/model/strategy/contact_strategy/frictional_contact_resolver.cpp

        src/model/strategy/integrator_strategy/integrator_strategy.h
        src/model/strategy/integrator_strategy/semi_implicit_euler_integrator.h
        src/model/strategy/integrator_strategy/semi_implicit_euler_integrator.cpp
        src/model/strategy/integrator_strategy/velocity_verlet_integrator.h
        src/model/strategy/integrator_strategy/velocity_verlet_integrator.cpp

        src/model/strategy/sleep_strategy/sleep_config.h
        src/model/strategy/sleep_strategy/sleep_metric.h
        src/model/strategy/sleep_strategy/sleep_strategy.h
        src/model/strategy/sleep_strategy/kinetic_sleep_strategy.h
        src/model/strategy/sleep_strategy/kinetic_sleep_strategy.cpp

        src/view_model/simulator/scene_manager.cpp
        src/view_model/simulator/scene_manager.h
        src/view_model/simulation_player/offline_simulation_runner.cpp
        src/view_model/simulation_player/offline_simulation_runner.h
        src/view_model/simulation_player/simulation_export_controller.cpp
        src/view_model/simulation_player/simulation_export_controller.h
        src/view_model/simulator/visual_item/scene_body_model.h
        src/view_model/simulator/visual_item/scene_body_model.cpp
        src/view_model/simulator/visual_item/scene_constraint_model.h
        src/view_model/simulator/visual_item/scene_constraint_model.cpp
        src/view_model/simulator/visual_item/scene_curve_model.h
        src/view_model/simulator/visual_item/scene_curve_model.cpp
        src/view_model/editor/editor_manager.cpp
        src/view_model/editor/editor_manager.h
        src/view_model/editor/parameter_manager.cpp
        src/view_model/editor/parameter_manager.h
        src/view_model/serialize/model_item_data.cpp
        src/view_model/serialize/model_item_data.h
        src/view_model/simulation_serializer/epsx_crc32c.h
        src/view_model/simulation_serializer/epsx_common.h
        src/view_model/simulation_serializer/epsx_format.h
        src/view_model/simulation_serializer/epsx_writer.cpp
        src/view_model/simulation_serializer/epsx_writer.h
        src/view_model/simulation_serializer/epsx_reader.cpp
        src/view_model/simulation_serializer/epsx_reader.h

        src/view/qt_component_helper.cpp
        src/view/qt_component_helper.h

        src/view/resource_helper.cpp
        src/view/resource_helper.h
        src/model/physics/constraints/simple_pendulum.cpp
        src/model/physics/constraints/simple_pendulum.h
        src/view/file_handler.cpp
        src/view/file_handler.h
        src/view/enum_handler.cpp
        src/view/enum_handler.h
        src/view/geometry/spring_geometry.cpp
        src/view/geometry/spring_geometry.h
        src/view/geometry/bezier_curve_geometry.cpp
        src/view/geometry/bezier_curve_geometry.h
        src/view/geometry/curve_geometry.cpp
        src/view/geometry/curve_geometry.h
        src/view/geometry/convex_mesh_geometry.cpp
        src/view/geometry/convex_mesh_geometry.h
        src/view_model/serialize/transform_transformer.cpp
        src/view_model/serialize/transform_transformer.h
        src/utils/egret_version.h
        src/utils/unique_id_generator.cpp
        src/utils/unique_id_generator.h
        src/model/physics/physics_abstract.cpp
        src/model/physics/physics_abstract.h
        src/utils/io/qdebug_handler.h
        src/utils/io/qdebug_handler.cpp
        src/model/physics/constraints/sliding_rail.cpp
        src/model/physics/constraints/sliding_rail.h
        src/model/physics/constraints/air_resistance.cpp
        src/model/physics/constraints/air_resistance.h
        src/view_model/simulator/visual_item/basic_render_item.h
        src/model/physics/rigid/shape/standard/convex_mesh.cpp
        src/model/physics/rigid/shape/standard/convex_mesh.h
        src/model/geometry/tessellated_mesh.h
        src/model/geometry/tessellated_mesh.cpp
        src/view/qt_media_helper.cpp
        src/view/qt_media_helper.h
        src/utils/os/windows/windows_sound.h
        src/utils/os/windows/windows_sound.cpp
        src/view_model/editor/clipboard_helper.cpp
        src/view_model/editor/clipboard_helper.h
        src/model/strategy/broad_phase_strategy/bvh_broad_strategy.cpp
        src/model/strategy/broad_phase_strategy/bvh_broad_strategy.h
        src/model/strategy/broad_phase_strategy/kernel/bvh_kernel.h
        src/utils/aabb.cpp
        src/utils/aabb.h
        src/model/strategy/broad_phase_strategy/octree_broad_strategy.cpp
        src/model/strategy/broad_phase_strategy/octree_broad_strategy.h
        src/model/strategy/broad_phase_strategy/kernel/octree_kernel.h
        src/view_model/simulator/visual_item/scene_convex_mesh_model.cpp
        src/view_model/simulator/visual_item/scene_convex_mesh_model.h
        src/model/physics/rigid/shape/standard/shape_capsule.cpp
        src/model/physics/rigid/shape/standard/shape_capsule.h

        src/model/solver/ccd_solver_adapter.h
        src/model/solver/ccd_solver_adapter.cpp

        src/model/physics/motion/motion_utils.h
        src/model/physics/motion/generalization_rigid_motion.h

        # 碰撞检测模板头文件（原 collision 模块已改为经典头文件）
        src/model/physics/collision/collision.h
        src/model/physics/collision/gilbert_johnson_keerthi.h
        src/model/physics/collision/expanding_polytope_algorithm.h
        src/model/physics/collision/continuous_collision_detector.h


)


add_executable(${PROJECT_NAME}
        ${CPP_FILES}
        ${QRC_FILES}
        resources/app.rc
)

# CCD 适配器翻译单元包含全部 GJK/EPA/CCD 模板实例化，需 /bigobj
set_source_files_properties(
        src/model/solver/ccd_solver_adapter.cpp
        PROPERTIES COMPILE_FLAGS "/bigobj"
)

# QXlsx 静态库
add_library(QXlsx::QXlsx STATIC IMPORTED)
target_include_directories(QXlsx::QXlsx INTERFACE "${CMAKE_SOURCE_DIR}/dependency/include/qxlsx_qt6_1.5.0/QXlsxQt6")
set_target_properties(QXlsx::QXlsx PROPERTIES
        IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/dependency/lib/QXlsxQt6/QXlsxQt6.lib"
        IMPORTED_LOCATION_DEBUG "${CMAKE_SOURCE_DIR}/dependency/lib/QXlsxQt6/QXlsxQt6d.lib"
        INTERFACE_LINK_LIBRARIES "Qt6::Core;Qt6::GuiPrivate"
)

# Assimp 导入库
add_library(Assimp::Assimp SHARED IMPORTED)
set_target_properties(Assimp::Assimp PROPERTIES
        IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/dependency/bin/Assimp/assimp-vc143-mt.dll"
        IMPORTED_IMPLIB "${CMAKE_SOURCE_DIR}/dependency/lib/Assimp/assimp-vc143-mt.lib"
        IMPORTED_LOCATION_DEBUG "${CMAKE_SOURCE_DIR}/dependency/bin/Assimp/assimp-vc143-mtd.dll"
        IMPORTED_IMPLIB_DEBUG "${CMAKE_SOURCE_DIR}/dependency/lib/Assimp/assimp-vc143-mtd.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/dependency/include/assimp_6_0_2"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        ${QT_MODULES_LIST_WITH_PREFIX}
        QXlsx::QXlsx
        Assimp::Assimp
)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_BINARY_DIR}/generated
)

if (WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(DEBUG_SUFFIX)
    if (MSVC AND CMAKE_BUILD_TYPE MATCHES "Debug")
        set(DEBUG_SUFFIX "d")
    endif ()
    set(QT_INSTALL_PATH "${CMAKE_PREFIX_PATH}")
    if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
        set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
            set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        endif ()
    endif ()
    if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
    endif ()
    foreach (QT_LIB ${QT_MODULES_LIST})
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    endforeach (QT_LIB)

    # 复制 Assimp DLL 到输出目录
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/dependency/bin/Assimp/assimp-vc143-mt${DEBUG_SUFFIX}.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>")

    find_program(WINDEPLOYQT_EXECUTABLE
            NAMES windeployqt
            HINTS "${QT_INSTALL_PATH}/bin")
    if (WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/src/qml"
                "$<TARGET_FILE:${PROJECT_NAME}>"
                COMMENT "Deploying Qt runtime + QML modules via windeployqt")
    else ()
        message(WARNING "windeployqt not found under '${QT_INSTALL_PATH}/bin'. QML modules (QtQuick/Controls/Layouts) may fail to load at runtime.")
    endif ()


    # 设置调试工作目录
    set_target_properties(${PROJECT_NAME} PROPERTIES
            VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    
endif ()


# ========== 子项目 ==========
# .epsx 文件关联工具：独立的控制台程序，负责写入 HKCU\Software\Classes，
# 与主程序解耦（安装包或手动部署时单独调用它即可）。
add_subdirectory(subprojects/epsx_register)

# 构建主程序时一并构建关联工具。
# 项目约定的构建命令是 `cmake --build <dir> --config Release --target Egret_Physics`，
# 带 --target 时只会构建该目标及其依赖（默认的 ALL 目标不参与），
# 因此在主程序上显式声明依赖，避免子项目被跳过；
# 若不带 --target 构建整个解决方案，两者本来就会一起构建。
add_dependencies(${PROJECT_NAME} epsx_register)

# 视频导出工具：独立的命令行程序（.epsx → MP4），复用主工程源码但**不**加入主程序依赖链，
# 避免拖慢日常构建；需要时显式构建：
#     cmake --build <build-dir> --config Release --target video_encoder
add_subdirectory(subprojects/video_encoder)



option(BUILD_TESTS "Build unit tests" ON)

if (BUILD_TESTS)
    set(TEST_SOURCES
            # tests/test_collision.cpp
            tests/test_ccd.cpp
            tests/test_sleep.cpp
            # tests/test_connecting_line_scene.cpp
            # tests/boost_test_dependency.h
    )

    add_executable(Egret_Physics_Tests
            ${TEST_SOURCES}
            src/model/physics/rigid/physical_entity.cpp
            src/model/physics/rigid/rigid_body.cpp
            src/model/physics/rigid/particle.cpp
            src/model/strategy/sleep_strategy/kinetic_sleep_strategy.cpp
            src/utils/io/async_logger.cpp
            src/model/physics/rigid/shape/standard/shape_base.cpp
            # .deprecated/register/shape_register.cpp
            # .deprecated/register/shape_factory_registry.cpp
            # .deprecated/register/standard_shape_factory_registration.cpp
            src/model/physics/rigid/shape/standard/shape_sphere.cpp
            src/model/physics/rigid/shape/standard/shape_box.cpp
            src/model/physics/rigid/shape/standard/shape_cylinder.cpp
            src/model/physics/rigid/shape/standard/shape_cylindrical_shell.cpp
            src/model/physics/rigid/shape/standard/shape_disk.cpp
            src/model/physics/rigid/shape/standard/shape_ring.cpp
            src/model/physics/rigid/shape/standard/shape_rod.cpp
            src/model/physics/rigid/shape/standard/shape_spherical_shell.cpp
            # .deprecated/collide_judge/standard_collide_judge_group.cpp
            # .deprecated/collide_judge/ccd_collide_judge_group.cpp
            src/model/physics/field/field_base.cpp
            src/model/physics/field/gravity_field.cpp
            src/model/scene/world_scene_manager.cpp
            src/utils/transform.cpp
    )

    target_link_libraries(Egret_Physics_Tests PRIVATE
            ${QT_MODULES_LIST_WITH_PREFIX}
    )

    enable_testing()
    add_test(NAME EgretPhysicsTests COMMAND Egret_Physics_Tests)
endif ()


# CPack 打包配置 (NSIS)

# 基本信息
set(CPACK_PACKAGE_NAME              "Egret_Physics_Studio")
set(CPACK_PACKAGE_VERSION           "1.0.0")
set(CPACK_PACKAGE_VENDOR            "Ye Jinghao")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Egret Physics Studio — 交互式物理模拟引擎")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Egret Physics Studio")

# NSIS（Windows 安装包向导）
set(CPACK_GENERATOR                 "NSIS")
set(CPACK_NSIS_DISPLAY_NAME        "Egret Physics Studio")
set(CPACK_NSIS_PACKAGE_NAME        "Egret Physics Studio")

# 安装包图标
set(CPACK_NSIS_MUI_ICON            "${CMAKE_SOURCE_DIR}/resources/assets/favicon/favicon_install.ico")

# 卸载程序图标
set(CPACK_NSIS_MUI_UNIICON         "${CMAKE_SOURCE_DIR}/resources/assets/favicon/favicon_uninstall.ico")

# 快捷方式可执行文件所在目录：相对安装根目录，"." 即根目录
set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")

# 注册表
set(CPACK_NSIS_INSTALLED_ICON_NAME  "Egret_Physics.exe")

set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)

# 可执行文件注册
# CPack 据此自动创建开始菜单快捷方式
set(CPACK_PACKAGE_EXECUTABLES       "Egret_Physics" "Egret Physics")

# 桌面快捷方式（值必须匹配 CPACK_PACKAGE_EXECUTABLES 中的可执行文件名）
set(CPACK_CREATE_DESKTOP_LINKS      "Egret_Physics")

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

# 主可执行文件 + .epsx 关联工具
install(TARGETS ${PROJECT_NAME} epsx_register
    RUNTIME DESTINATION .
    )

# 构建输出目录中 windeployqt 部署的所有运行时文件
# 排除调试符号、链接产物、以及 CMake / Ninja 构建系统自身生成的文件
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/release/"
    DESTINATION .
    USE_SOURCE_PERMISSIONS
    # --- 构建产物（通配） ---
    PATTERN "*.pdb" EXCLUDE
    PATTERN "*.ilk" EXCLUDE
    PATTERN "*.exp" EXCLUDE
    PATTERN "*.lib" EXCLUDE
    PATTERN "*.obj" EXCLUDE 
    # --- CMake 生成文件 ---
    PATTERN "cmake_install.cmake" EXCLUDE
    PATTERN "CMakeCache.txt" EXCLUDE
    PATTERN "CMakeDoxyfile.in" EXCLUDE
    PATTERN "CMakeDoxygenDefaults.cmake" EXCLUDE
    PATTERN "CPackConfig.cmake" EXCLUDE
    PATTERN "CPackSourceConfig.cmake" EXCLUDE
    PATTERN "CTestTestfile.cmake" EXCLUDE
    PATTERN "compile_commands.json" EXCLUDE
    # --- Ninja 生成文件 ---
    PATTERN ".ninja_deps" EXCLUDE
    PATTERN ".ninja_log" EXCLUDE
    PATTERN "build.ninja" EXCLUDE
    # --- 构建系统目录 ---
    PATTERN "CMakeFiles" EXCLUDE
    PATTERN "Egret_Physics_autogen" EXCLUDE
    PATTERN "Egret_Physics_Tests_autogen" EXCLUDE
    PATTERN ".cmake" EXCLUDE
    PATTERN ".qt" EXCLUDE
    # --- 项目自身非运行时目录 ---
    PATTERN "generated" EXCLUDE
    PATTERN ".gitignore" EXCLUDE
    )

include(CPack)

# 绕过 CPack 4.2 转义 bug：CPack 写入 CPackConfig.cmake 时不转义引号/反斜杠
# 用 file(APPEND) 直接追加已正确转义的 set 命令
file(APPEND "${CMAKE_BINARY_DIR}/CPackConfig.cmake"
[=[
# --- 桌面快捷方式默认创建，绕过复选框默认不勾选的问题 ---
set(CPACK_NSIS_CREATE_ICONS_EXTRA "CreateShortCut \"$DESKTOP\\Egret Physics.lnk\" \"$INSTDIR\\Egret_Physics.exe\"")
]=])


