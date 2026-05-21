cmake_minimum_required(VERSION 3.30.1)
project(Egret_Physics)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "D:/Qt/6.9.3/msvc2022_64")

option(DEBUG_SIMULATE_RELEASE "Simulate Release behavior in Debug build" OFF)

include_directories(
        dependency/include
        dependency/include/boost_1_90_0
        dependency/include/eigen_5_0_0
        src
        src/model
        src/model/physics/constraints
        src/model/solver
        src/model/scene
        src/model/physics
        src/model/physics/rigid
        src/model/physics/field
        src/model/physics/rigid/shape
        src/model/physics/rigid/shape/register
        src/model/physics/rigid/shape/standard/collide_judge
        src/model/physics/rigid/shape/standard
        src/view_model/qt
        src/view_model/scene
        src/model/strategy
        src/view_model
        src/view
        src/utils
        src/utils/io
)

set(QT_MODULES_LIST
        Core Gui Widgets Quick Qml Quick3D Sql
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
        src/utils/io/json_controller.cpp
        src/utils/io/json_controller.h
        src/utils/io/logger.cpp
        src/utils/io/logger.h
        src/utils/language/cpp_property.cpp

        src/model/physics/physics_utils.h
        src/model/physics/rigid/particle.cpp
        src/model/physics/rigid/particle.h
        src/model/physics/rigid/physical_entity.cpp
        src/model/physics/rigid/physical_entity.h
        src/model/physics/rigid/rigid_body.cpp
        src/model/physics/rigid/rigid_body.h

        src/model/physics/rigid/shape/standard/collide_judge/standard_collide_judge_group.h
        src/model/physics/rigid/shape/standard/collide_judge/standard_collide_judge_group.cpp
        src/model/physics/rigid/shape/standard/collide_judge/ccd_collide_judge_group.h
        src/model/physics/rigid/shape/standard/collide_judge/ccd_collide_judge_group.cpp
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
        src/model/physics/rigid/shape/register/shape_register.cpp
        src/model/physics/rigid/shape/register/shape_register.h
        src/model/physics/rigid/shape/register/shape_factory_registry.cpp
        src/model/physics/rigid/shape/register/shape_factory_registry.h
        src/model/physics/rigid/shape/register/standard_shape_factory_registration.cpp
        src/model/physics/rigid/shape/register/standard_register_dummy.cpp
        src/model/physics/rigid/shape/register/ccd_register_dummy.cpp


        src/model/physics/field/field_base.cpp
        src/model/physics/field/field_base.h
        src/model/physics/field/gravity_field.cpp
        src/model/physics/field/gravity_field.h
        src/model/physics/field/gravitational_field.cpp
        src/model/physics/field/gravitational_field.h

        src/model/physics/constraints/constraints_base.cpp
        src/model/physics/constraints/constraints_base.h
        src/model/physics/constraints/support_surface.cpp
        src/model/physics/constraints/support_surface.h
        src/model/physics/constraints/sliding_rail.cpp
        src/model/physics/constraints/sliding_rail.h
        src/model/physics/constraints/connecting_line.cpp
        src/model/physics/constraints/connecting_line.h
        src/model/physics/constraints/connecting_rod.cpp
        src/model/physics/constraints/connecting_rod.h

        src/model/solver/solver.h
        src/model/solver/solver.cpp
        src/model/solver/solver_scene_snapshot.h
        src/model/solver/solver_types.h
        src/model/scene/world_scene_manager.h
        src/model/scene/world_scene_manager.cpp

        src/model/strategy/broad_phase_strategy/broad_phase_strategy.h
        src/model/strategy/broad_phase_strategy/brute_force_broad_phase.h
        src/model/strategy/broad_phase_strategy/implementation.cpp

        src/model/strategy/contact_strategy/contact_resolver_strategy.h
        src/model/strategy/contact_strategy/frictionless_contact_resolver.h
        src/model/strategy/contact_strategy/implementation.cpp

        src/model/strategy/integrator_strategy/integrator_strategy.h
        src/model/strategy/integrator_strategy/semi_implicit_euler_integrator.h
        src/model/strategy/integrator_strategy/implementation.cpp

        src/view_model/qt/scene_manager.cpp
        src/view_model/qt/scene_manager.h
        src/view_model/qt/scene_body_model.h
        src/view_model/qt/scene_body_model.cpp

        src/view/coordinate.cpp
        src/view/coordinate.h
        src/view/qt_component_helper.cpp
        src/view/qt_component_helper.h

        src/view/resource_helper.cpp
        src/view/resource_helper.h
)


add_executable(${PROJECT_NAME}
        ${CPP_FILES}
        ${QRC_FILES}
        resources/app.rc
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        ${QT_MODULES_LIST_WITH_PREFIX}
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



option(BUILD_TESTS "Build unit tests" ON)

if (BUILD_TESTS)
    set(TEST_SOURCES
            # tests/test_collision.cpp
            tests/test_ccd.cpp
            tests/boost_test_dependency.h
    )

    add_executable(Egret_Physics_Tests
            ${TEST_SOURCES}
            src/model/physics/rigid/physical_entity.cpp
            src/model/physics/rigid/rigid_body.cpp
            src/model/physics/rigid/particle.cpp
            src/model/physics/rigid/shape/standard/shape_base.cpp
            src/model/physics/rigid/shape/register/shape_register.cpp
            src/model/physics/rigid/shape/register/shape_factory_registry.cpp
            src/model/physics/rigid/shape/register/standard_shape_factory_registration.cpp
            src/model/physics/rigid/shape/standard/shape_sphere.cpp
            src/model/physics/rigid/shape/standard/shape_box.cpp
            src/model/physics/rigid/shape/standard/shape_cylinder.cpp
            src/model/physics/rigid/shape/standard/shape_cylindrical_shell.cpp
            src/model/physics/rigid/shape/standard/shape_disk.cpp
            src/model/physics/rigid/shape/standard/shape_ring.cpp
            src/model/physics/rigid/shape/standard/shape_rod.cpp
            src/model/physics/rigid/shape/standard/shape_spherical_shell.cpp
            src/model/physics/rigid/shape/standard/collide_judge/standard_collide_judge_group.cpp
            src/model/physics/rigid/shape/standard/collide_judge/ccd_collide_judge_group.cpp
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


