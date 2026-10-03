/**
 * @file        VideoExportScene.qml
 * @brief       视频导出的离屏 3D 场景根对象。
 * @details     本文件只做三件事：
 *
 *              1. 用共享的 SimulationViewport 渲染与播放器完全一致的场景
 *                 （灯光、材质、网格以及刚体/约束/曲线/凸体网格委托全部复用）；
 *              2. 暴露相机位置、注视点、视场角与裁剪面，供 C++ 在渲染前写入；
 *              3. 提供 applyCamera()，把上述参数立即应用到视口。
 *
 *              该场景挂在不会显示的 QQuickWindow（QQuickRenderControl）上，由
 *              OffscreenSceneRenderer 驱动渲染，因此不包含任何鼠标交互逻辑。
 *
 * @author      jehor
 * @date        2026-10-03
 * @version     1.0.0
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 * @ingroup     VideoEncoder
 */

import QtQuick 2.15
import QtQuick3D 6.9

import "qrc:/components/components/viewport"

Item {
    id: sceneRoot

    /** 是否显示网格与坐标轴。 */
    property bool showGrid: true

    /** 相机位置（物理坐标系，米）。 */
    property vector3d cameraPosition: Qt.vector3d(0, 0, 0)

    /** 相机注视点（物理坐标系，米）。 */
    property vector3d cameraTarget: Qt.vector3d(0, 0, 0)

    /** 垂直视场角（度）。 */
    property real cameraFieldOfView: 45

    /** 近裁剪面距离。 */
    property real cameraClipNear: 1

    /** 远裁剪面距离。 */
    property real cameraClipFar: 50000

    /**
     * @brief 把当前相机参数应用到视口。
     * @remark 由 C++ 在修改相机参数后调用；重复调用无副作用。
     */
    function applyCamera() {
        exportViewport.useExplicitCamera = true
        exportViewport.explicitCameraPosition = cameraPosition
        exportViewport.explicitCameraTarget = cameraTarget
        exportViewport.explicitFieldOfView = cameraFieldOfView
        exportViewport.explicitClipNear = cameraClipNear
        exportViewport.explicitClipFar = cameraClipFar
        exportViewport.applyExplicitCamera()
    }

    SimulationViewport {
        id: exportViewport
        anchors.fill: parent

        // 数据来源：由 C++ 注入的导出专用场景控制器（与播放器同一条数据链路）。
        bodyModelSource: sceneController.bodyModel
        constraintModelSource: sceneController.constraintModel
        curveModelSource: sceneController.curveModel
        convexMeshModelSource: sceneController.convexMeshModel

        // 网格与坐标轴的显隐由导出参数控制。
        coordinateSystem.visible: sceneRoot.showGrid

        // 导出场景没有交互，相机完全由 C++ 指定。
        useExplicitCamera: true

        Component.onCompleted: sceneRoot.applyCamera()
    }
}
