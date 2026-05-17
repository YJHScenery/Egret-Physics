import QtQuick 2.15
import QtQuick3D 6.5
import CustomGeometry 1.0

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

    onAxisOn: function(on) { axisVisible = on; }
    onGridOn: function(on) { gridVisible = on; }

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
                property real offset: (index - Math.floor(lineCount / 2)) * step
                property bool major: planeRoot.isMajor(offset)
                geometry: SimpleGeometry {
                    shape: SimpleGeometry.Cube
                    size: 1.0
                }
                scale: Qt.vector3d(extent * 2, major ? majorThickness : minorThickness, major ? majorThickness : minorThickness)
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
                property real offset: (index - Math.floor(lineCount / 2)) * step
                property bool major: planeRoot.isMajor(offset)
                geometry: SimpleGeometry {
                    shape: SimpleGeometry.Cube
                    size: 1.0
                }
                scale: Qt.vector3d(major ? majorThickness : minorThickness, major ? majorThickness : minorThickness, extent * 2)
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
            geometry: SimpleGeometry {
                shape: SimpleGeometry.Cube
                size: 1.0
            }
            scale: Qt.vector3d(extent * 2, extent * 2, 0.02)
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
            color: "#E0E7FF"
            axis: Qt.vector3d(1, 0, 0)
        }
        AxisLine {
            length: axisLength
            thickness: axisThickness
            color: "#4B5563"
            axis: Qt.vector3d(0, 0, 1)
        }
        AxisLine {
            length: axisLength
            thickness: axisThickness
            color: "#9CA3AF"
            axis: Qt.vector3d(0, 1, 0)
        }

        AxisLine {
            length: axisLength
            thickness: axisThickness
            color: "#E0E7FF"
            axis: Qt.vector3d(-1, 0, 0)
        }
        AxisLine {
            length: axisLength
            thickness: axisThickness
            color: "#4B5563"
            axis: Qt.vector3d(0, 0, -1)
        }
        AxisLine {
            length: axisLength
            thickness: axisThickness
            color: "#9CA3AF"
            axis: Qt.vector3d(0, -1, 0)
        }

        AxisArrow {
            color: "#E0E7FF"
            direction: Qt.vector3d(1, 0, 0)
            offset: Qt.vector3d(axisLength, 0, 0)
            length: arrowLength
            radius: arrowRadius
        }
        AxisArrow {
            color: "#4B5563"
            direction: Qt.vector3d(0, 0, 1)
            offset: Qt.vector3d(0, 0, axisLength)
            length: arrowLength
            radius: arrowRadius
        }
        AxisArrow {
            color: "#9CA3AF"
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
            geometry: SimpleGeometry {
                shape: SimpleGeometry.Cube
                size: 1.0
            }
            scale: Qt.vector3d(Math.abs(axis.x) > 0 ? length : thickness, Math.abs(axis.y) > 0 ? length : thickness, Math.abs(axis.z) > 0 ? length : thickness)
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

        function rotationForDirection() {
            if (direction.x === 1) {
                return Qt.vector3d(0, 0, -90);
            }
            if (direction.x === -1) {
                return Qt.vector3d(0, 0, 90);
            }
            if (direction.z === 1) {
                return Qt.vector3d(-90, 0, 0);
            }
            if (direction.z === -1) {
                return Qt.vector3d(90, 0, 0);
            }
            if (direction.y === -1) {
                return Qt.vector3d(180, 0, 0);
            }
            return Qt.vector3d(0, 0, 0);
        }

        Model {
            geometry: SimpleGeometry {
                shape: SimpleGeometry.Cube
                size: 1.0
            }
            position: offset
            scale: Qt.vector3d(radius * 1.8, length, radius * 1.8)
            eulerRotation: rotationForDirection()
            materials: DefaultMaterial {
                diffuseColor: axisArrowRoot.color
                lighting: DefaultMaterial.NoLighting
            }
            castsShadows: false
            pickable: false
        }
    }
}
