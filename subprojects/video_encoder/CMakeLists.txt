# =============================================================================
#  video_encoder —— .epsx 仿真文件 → MP4 视频（独立子项目）
#
#  用途：把离线仿真的 .epsx 文件离屏渲染成逐帧画面，再编码为视频文件。
#  单独成 exe 的原因：
#    1. 导出属于批处理任务，不需要主界面，也不应占用主界面线程；
#    2. 主程序不必携带离屏渲染与视频编码代码；
#    3. 命令行形态便于脚本批量导出。
#
#  复用主工程源码（去掉主程序入口），以保证「场景重建 + QML 视口」与播放器完全一致，
#  代价是首次编译需要额外编译一份公共源码；主程序构建不会被拖慢，因为本目标没有
#  加入主程序的依赖链，需要显式指定目标：
#      cmake --build <build-dir> --config Release --target video_encoder
# =============================================================================

project(video_encoder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Quick3D Multimedia)

# ---- 复用主工程源码（仅去掉主程序入口）----
set(EGRET_SHARED_SOURCES "")
foreach (_source IN LISTS CPP_FILES)
    if (NOT _source STREQUAL "src/main.cpp")
        list(APPEND EGRET_SHARED_SOURCES "${CMAKE_SOURCE_DIR}/${_source}")
    endif ()
endforeach ()

set(EGRET_SHARED_RESOURCES "")
foreach (_resource IN LISTS QRC_FILES)
    list(APPEND EGRET_SHARED_RESOURCES "${CMAKE_SOURCE_DIR}/${_resource}")
endforeach ()

add_executable(video_encoder
        src/main.cpp
        src/epsx_video_encoder.h
        src/epsx_video_encoder.cpp
        src/offscreen_scene_renderer.h
        src/offscreen_scene_renderer.cpp
        src/video_frame_writer.h
        src/video_frame_writer.cpp
        src/video_encoder_options.h
        src/video_encoder_options.cpp
        resources/video_encoder.qrc
        ${EGRET_SHARED_SOURCES}
        ${EGRET_SHARED_RESOURCES}
)

# CCD 适配器翻译单元包含全部 GJK/EPA/CCD 模板实例化，需要 /bigobj（与主程序一致）。
set_source_files_properties(
        "${CMAKE_SOURCE_DIR}/src/model/solver/ccd_solver_adapter.cpp"
        PROPERTIES COMPILE_FLAGS "/bigobj"
)

target_link_libraries(video_encoder PRIVATE
        ${QT_MODULES_LIST_WITH_PREFIX}
        Qt6::GuiPrivate          # 离屏渲染回读需要 rhi/qrhi.h
        QXlsx::QXlsx
        Assimp::Assimp
)

target_include_directories(video_encoder PRIVATE
        ${CMAKE_BINARY_DIR}/generated
)

set_target_properties(video_encoder PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# ---- 部署：把 Qt 运行时与 QML 模块补齐到输出目录，保证工具可直接运行 ----
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

    add_custom_command(TARGET video_encoder POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:video_encoder>/plugins/platforms/")
    add_custom_command(TARGET video_encoder POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
            "$<TARGET_FILE_DIR:video_encoder>/plugins/platforms/")

    foreach (QT_LIB ${QT_MODULES_LIST})
        add_custom_command(TARGET video_encoder POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/bin/Qt6${QT_LIB}${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:video_encoder>")
    endforeach ()

    add_custom_command(TARGET video_encoder POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/dependency/bin/Assimp/assimp-vc143-mt${DEBUG_SUFFIX}.dll"
            "$<TARGET_FILE_DIR:video_encoder>")

    find_program(WINDEPLOYQT_EXECUTABLE
            NAMES windeployqt
            HINTS "${QT_INSTALL_PATH}/bin")
    if (WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET video_encoder POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --qmldir "${CMAKE_SOURCE_DIR}/src/qml"
                "$<TARGET_FILE:video_encoder>"
                COMMENT "Deploying Qt runtime + QML modules for video_encoder")
    else ()
        message(WARNING "未找到 windeployqt，QML 模块可能无法在输出目录加载")
    endif ()
endif ()
