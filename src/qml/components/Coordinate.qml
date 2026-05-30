/**
 * @file        filename.h
 * @brief       一句话概述文件的主要功能
 * @details     详细描述文件的用途、设计思路、使用注意事项等。
 *              可以写多行，说明模块在整个系统中的位置。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        创建日期 (如 2025-03-15)
 * @version     版本号 (如 1.0.0)
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     许可证 (如 GPL, MIT, BSD 等)
 *
 * @ingroup     所属模块组 (可选，用于分组)
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

import QtQuick 2.15
import QtQuick3D 6.5

Node {
    id: root

    property real orbitDistance: 900
    property real gridStep: 50
    property real gridExtent: 1000
    property int maxGridLines: 241
    property real minorLinePixels: 0.35
    property real majorLinePixels: 0.8
    property real axisLinePixels: 1.8
    property real axisLength: gridExtent
    property real viewportHeight: 720
    property real viewportWidth: 1280
    property var camera: null
    property bool autoExtent: true
    readonly property real effectiveExtent: autoExtent ? gridExtentForView() : gridExtent

    property bool axisVisible: true
    property bool gridVisible: true

    signal axisOn(bool on)

    signal gridOn(bool on)

    onAxisOn: function (on) {
        axisVisible = on;
    }
    onGridOn: function (on) {
        gridVisible = on;
    }

    function unitsPerPixel(distance) {
        var h = viewportHeight;
        if (h <= 0 || !camera) {
            return 0.001;
        }
        var fovRad = camera.fieldOfView * Math.PI / 180.0;
        var frustumHeight = 2.0 * distance * Math.tan(fovRad / 2.0);
        return frustumHeight / h;
    }

    function gridExtentForView() {
        if (!camera || viewportHeight <= 0 || viewportWidth <= 0) {
            return gridExtent;
        }
        var fovRad = camera.fieldOfView * Math.PI / 180.0;
        var halfHeight = orbitDistance * Math.tan(fovRad / 2.0);
        var aspect = viewportWidth / viewportHeight;
        var halfWidth = halfHeight * aspect;
        return Math.max(halfWidth, halfHeight) * 1.1;
    }

    AxisHelper {
        id: axisHelper
        axisLength: root.effectiveExtent
        axisThickness: root.unitsPerPixel(root.orbitDistance) * root.axisLinePixels
        visible: root.axisVisible
    }

    GridPlane {
        id: gridPlaneX
        extent: root.effectiveExtent
        step: root.gridStep
        maxLines: root.maxGridLines
        visible: root.gridVisible
    }

    GridPlane {
        id: gridPlaneZ
        extent: root.effectiveExtent
        step: root.gridStep
        maxLines: root.maxGridLines
        eulerRotation.x: 90
        visible: root.gridVisible
    }

    GridPlane {
        id: gridPlaneY
        extent: root.effectiveExtent
        step: root.gridStep
        maxLines: root.maxGridLines
        eulerRotation.z: 90
        visible: root.gridVisible
    }

    component GridPlane: Node {
        id: planeRoot
        property real extent: 20
        property real step: 1.0
        property int maxLines: 241
        property color majorColor: "#2f3b4b"
        property color minorColor: "#1c2634"
        property real minorThickness: root.unitsPerPixel(root.orbitDistance) * root.minorLinePixels
        property real majorThickness: root.unitsPerPixel(root.orbitDistance) * root.majorLinePixels
        readonly property int desiredLines: Math.floor(extent / Math.max(step, 0.05)) * 2 + 1
        readonly property int lineCount: Math.min(desiredLines, maxLines)
        readonly property real majorEvery: Math.max(step * 5, 0.5)

        function isMajor(offset) {
            return Math.abs(offset) % majorEvery < 0.001;
        }

        Repeater3D {
            model: lineCount
            delegate: Model {
                source: "#Cube"

                property real offset: (index - Math.floor(lineCount / 2)) * step
                property bool major: planeRoot.isMajor(offset)
                scale: Qt.vector3d(extent * 0.02, 0.01 * (major ? majorThickness : minorThickness), 0.01 * (major ? majorThickness : minorThickness))
                position: Qt.vector3d(0, 0, offset)
                materials: DefaultMaterial {
                    diffuseColor: major ? planeRoot.majorColor : planeRoot.minorColor
                    lighting: DefaultMaterial.NoLighting
                    opacity: 0.8
                }
                castsShadows: false
                pickable: false
            }
        }

        Repeater3D {
            model: lineCount
            delegate: Model {
                source: "#Cube"

                property real offset: (index - Math.floor(lineCount / 2)) * step
                property bool major: planeRoot.isMajor(offset)
                scale: Qt.vector3d(0.01 * (major ? majorThickness : minorThickness), 0.01 * (major ? majorThickness : minorThickness), extent * 0.02)
                position: Qt.vector3d(offset, 0, 0)
                materials: DefaultMaterial {
                    diffuseColor: major ? planeRoot.majorColor : planeRoot.minorColor
                    lighting: DefaultMaterial.NoLighting
                    opacity: 0.8
                }
                castsShadows: false
                pickable: false
            }
        }

        Model {
            source: "#Cube"

            scale: Qt.vector3d(extent * 0.02, extent * 0.02, 0.0002)
            materials: DefaultMaterial {
                diffuseColor: "#0c1220"
                lighting: DefaultMaterial.NoLighting
                opacity: 0.06
            }
            pickable: false
            castsShadows: false
        }
    }

    component AxisHelper: Node {
        property real axisLength: 50
        property real axisThickness: 0.06
        property real arrowLength: 1.2
        property real arrowRadius: 0.22

        AxisLine {
            length: axisLength
            thickness: axisThickness
            color: "#9CA3AF"
            axis: Qt.vector3d(1, 0, 0)
        }
        AxisLine {
            length: axisLength
            thickness: axisThickness
            color: "#E0E7FF"
            axis: Qt.vector3d(0, 0, 1)
        }
        AxisLine {
            length: axisLength
            thickness: axisThickness
            color: "#4B5563"
            axis: Qt.vector3d(0, 1, 0)
        }

        AxisLine {
            length: axisLength
            thickness: axisThickness
            color: "#9CA3AF"
            axis: Qt.vector3d(-1, 0, 0)
        }
        AxisLine {
            length: axisLength
            thickness: axisThickness
            color: "#E0E7FF"
            axis: Qt.vector3d(0, 0, -1)
        }

        AxisLine {
            length: axisLength
            thickness: axisThickness
            color: "#4B5563"
            axis: Qt.vector3d(0, -1, 0)
        }

        AxisArrow {
            color: "#9CA3AF"
            direction: Qt.vector3d(1, 0, 0)
            offset: Qt.vector3d(axisLength, 0, 0)
            length: arrowLength
            radius: arrowRadius
        }

        AxisArrow {
            color: "#E0E7FF"
            direction: Qt.vector3d(0, 0, 1)
            offset: Qt.vector3d(0, 0, axisLength)
            length: arrowLength
            radius: arrowRadius
        }

        AxisArrow {
            color: "#4B5563"
            direction: Qt.vector3d(0, 1, 0)
            offset: Qt.vector3d(0, axisLength, 0)
            length: arrowLength
            radius: arrowRadius
        }
    }

    component AxisLine: Node {
        id: axisLineRoot
        property real length: 50
        property color color: "#ffffff"
        property vector3d axis: Qt.vector3d(1, 0, 0)
        property real thickness: 0.06

        Model {
            source: "#Cube"
            scale: Qt.vector3d(0.01 * (Math.abs(axis.x) > 0 ? length : thickness), 0.01 * (Math.abs(axis.y) > 0 ? length : thickness), 0.01 * (Math.abs(axis.z) > 0 ? length : thickness))
            position: Qt.vector3d(axis.x * length / 2, axis.y * length / 2, axis.z * length / 2)
            materials: DefaultMaterial {
                diffuseColor: axisLineRoot.color
                lighting: DefaultMaterial.NoLighting
            }
            castsShadows: false
            pickable: false
        }
    }

    component AxisArrow: Node {
        id: axisArrowRoot
        property color color: "#ffffff"
        property vector3d direction: Qt.vector3d(1, 0, 0)
        property vector3d offset: Qt.vector3d(0, 0, 0)
        property real length: 1.2
        property real radius: 0.22

        property quaternion rotation: {
            if (direction.y === 1) return Qt.quaternion(1, 0, 0, 0)  // 单位旋转
            if (direction.y === -1) return Qt.quaternion(0, 1, 0, 0) // 绕 X 轴 180°
            if (direction.x === 1) return Qt.quaternion(0.7071068, 0, 0, -0.7071068)  // 绕 Z 轴 -90°
            if (direction.x === -1) return Qt.quaternion(0.7071068, 0, 0, 0.7071068)   // 绕 Z 轴 +90°
            if (direction.z === 1) return Qt.quaternion(0.7071068, 0.7071068, 0, 0)    // 绕 X 轴 +90°
            if (direction.z === -1) return Qt.quaternion(0.7071068, -0.7071068, 0, 0)   // 绕 X 轴 -90°
            return Qt.quaternion(1, 0, 0, 0)
        }

        Model {
            source: "#Cone"
            position: offset
            scale: Qt.vector3d(radius * 0.8, length, radius * 0.8)
            rotation: axisArrowRoot.rotation
            materials: DefaultMaterial {
                diffuseColor: axisArrowRoot.color
                lighting: DefaultMaterial.NoLighting
            }
            castsShadows: false
            pickable: false
        }
    }
}
