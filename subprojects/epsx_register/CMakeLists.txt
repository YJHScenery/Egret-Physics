# =============================================================================
#  epsx_register —— .epsx 文件关联工具（独立子项目）
#
#  用途：把 .epsx 扩展名注册 / 注销到当前用户（HKCU\Software\Classes）。
#  单独成 exe 的原因：注册是安装期的一次性动作，与播放器主程序无关；
#  拆开后主程序不必携带注册表代码，安装包也可以直接调用本工具。
#
#  产物：epsx_register.exe（控制台程序），默认与主程序输出到同一目录。
# =============================================================================

project(epsx_register LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core)

add_executable(epsx_register
        src/main.cpp
        src/file_association.h
        src/file_association.cpp
        resources/epsx_register_icon.qrc
)

target_link_libraries(epsx_register PRIVATE Qt6::Core)

# 与主程序同目录输出（CMake 默认行为）：windeployqt 已把 Qt6Core.dll 部署在那里，
# 工具无需额外部署即可运行。
set_target_properties(epsx_register PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
