cmake_minimum_required(VERSION 3.30.1)
project(Egret_Physics)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_PREFIX_PATH "D:/Qt/6.9.3/msvc2022_64")

include_directories(
        dependency/include
        dependency/include/boost_1_90_0
        dependency/include/eigen_5_0_0
        src
        src/model
        src/model/physics
        src/model/physics/rigid
        src/model/physics/field
        src/model/physics/rigid/shape
        src/view_model/qt
        src/view_model/scene
        src/model/strategy
        src/model/strategy/implementations
        src/model/strategy/interfaces
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


set(CPP_FILES
        src/main.cpp
        src/utils/constants.h
        src/utils/io/database_controller.cpp
        src/utils/io/database_controller.h
        src/view_model/qt/scene_manager.cpp
        src/view_model/qt/scene_manager.h
        src/model/physics/rigid/particle.cpp
        src/model/physics/rigid/particle.h
        src/utils/io/json_controller.cpp
        src/utils/io/json_controller.h
        src/utils/io/logger.cpp
        src/utils/io/logger.h
        src/model/physics/rigid/physical_entity.cpp
        src/model/physics/rigid/physical_entity.h
        src/model/physics/rigid/rigid_body.cpp
        src/model/physics/rigid/rigid_body.h
        src/model/physics/rigid/shape/shape_base.cpp
        src/model/physics/rigid/shape/shape_base.h
        src/utils/geometry.h
        src/model/physics/rigid/shape/shape_sphere.cpp
        src/model/physics/rigid/shape/shape_sphere.h
        src/model/physics/rigid/shape/shape_spherical_shell.cpp
        src/model/physics/rigid/shape/shape_spherical_shell.h
        src/model/physics/rigid/shape/shape_cube.cpp
        src/model/physics/rigid/shape/shape_cube.h
        src/model/physics/rigid/shape/shape_cylinder.cpp
        src/model/physics/rigid/shape/shape_cylinder.h
        src/model/physics/rigid/shape/shape_ring.cpp
        src/model/physics/rigid/shape/shape_ring.h
        src/model/physics/rigid/shape/shape_cylindrical_shell.cpp
        src/model/physics/rigid/shape/shape_cylindrical_shell.h
)

set(QRC_FILES
        resources/global.qrc
        src/qml/qml.qrc
        src/model/physics/rigid/shape/shape_rod.cpp
        src/model/physics/rigid/shape/shape_rod.h
)

add_executable(${PROJECT_NAME}
        ${CPP_FILES}
        ${QRC_FILES}
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
endif ()


