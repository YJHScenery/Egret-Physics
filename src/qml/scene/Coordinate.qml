import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick3D 6.5
import CustomGeometry 1.0

Window {
    id: root
    width: 1280
    height: 720
    visible: true
    color: "#0A0F18"
    title: "Smart 3D Coordinate System"

    property vector3d initialCameraPosition: Qt.vector3d(10, 10, 10)
    property vector3d initialCameraTarget: Qt.vector3d(0, 0, 0)
    property real gridStep: 1.0

    property real coordX: 0
    property real coordY: 0
    property real coordZ: 0

    property real orbitYaw: 45
    property real orbitPitch: 35
    property real orbitDistance: 17
    property real minorLinePixels: 0.35
    property real majorLinePixels: 0.8
    property real axisLinePixels: 1.8
    property real gridExtent: 60
    property int maxGridLines: 241

    function updateGridStep() {
        var dx = camera.position.x
        var dy = camera.position.y
        var dz = camera.position.z
        var distance = Math.sqrt(dx * dx + dy * dy + dz * dz)
        var newStep = distance < 15 ? 0.1 : (distance < 30 ? 0.5 : 1.0)
        if (Math.abs(newStep - gridStep) > 0.0001) {
            gridStep = newStep
        }
    }

    function resetView() {
        orbitYaw = 45
        orbitPitch = 35
        orbitDistance = 17
        updateCameraFromOrbit()
    }

    function updateCameraFromOrbit() {
        var yawRad = orbitYaw * Math.PI / 180.0
        var pitchRad = orbitPitch * Math.PI / 180.0
        var cp = Math.cos(pitchRad)
        var sp = Math.sin(pitchRad)
        var cy = Math.cos(yawRad)
        var sy = Math.sin(yawRad)

        var x = orbitDistance * cp * cy + initialCameraTarget.x
        var y = orbitDistance * sp + initialCameraTarget.y
        var z = orbitDistance * cp * sy + initialCameraTarget.z

        camera.position = Qt.vector3d(x, y, z)
        camera.lookAt(initialCameraTarget)
        updateGridStep()
    }

    function unitsPerPixel(distance) {
        var h = view.height
        if (h <= 0) {
            return 0.001
        }
        var fovRad = camera.fieldOfView * Math.PI / 180.0
        var frustumHeight = 2.0 * distance * Math.tan(fovRad / 2.0)
        return frustumHeight / h
    }

    function addCube() {
        var pos = zUpToScene(Qt.vector3d(2.5, 1.5, 2.5))
        objectsModel.append({
            kind: "cube",
            x: pos.x,
            y: pos.y,
            z: pos.z,
            size: 0.5
        })
    }

    function addSphere() {
        var pos = zUpToScene(Qt.vector3d(-2.5, 2.0, -2.5))
        objectsModel.append({
            kind: "sphere",
            x: pos.x,
            y: pos.y,
            z: pos.z,
            radius: 0.3
        })
    }

    function sceneToZUp(pos) {
        return Qt.vector3d(pos.x, pos.z, pos.y)
    }

    function zUpToScene(pos) {
        return Qt.vector3d(pos.x, pos.z, pos.y)
    }

    Component.onCompleted: resetView()


    View3D {
        id: view
        anchors.fill: parent
        environment: SceneEnvironment {
            clearColor: "#0A0F18"
            backgroundMode: SceneEnvironment.Color
        }
        camera: camera

        Node {
            id: sceneRoot

            Node {
                id: pivot
                position: root.initialCameraTarget
            }

            PerspectiveCamera {
                id: camera
                position: root.initialCameraPosition
                clipNear: 0.1
                clipFar: 1000
                fieldOfView: 60
                onPositionChanged: root.updateGridStep()
            }


            AxisHelper {
                axisLength: 50
                axisThickness: root.unitsPerPixel(root.orbitDistance) * root.axisLinePixels
            }

            GridPlane {
                id: gridXZ
                extent: root.gridExtent
                step: root.gridStep
                maxLines: root.maxGridLines
            }

            GridPlane {
                extent: root.gridExtent
                step: root.gridStep
                maxLines: root.maxGridLines
                eulerRotation.x: 90
            }

            GridPlane {
                extent: root.gridExtent
                step: root.gridStep
                maxLines: root.maxGridLines
                eulerRotation.z: 90
            }

            Repeater3D {
                id: objectsRepeater
                model: objectsModel
                delegate: Node {
                    property bool isCube: kind === "cube"
                    property real cubeSize: (typeof size === "number") ? size : 0.5
                    property real sphereRadius: (typeof radius === "number") ? radius : 0.3
                    position: Qt.vector3d(x, y, z)

                    Model {
                        visible: isCube
                        geometry: SimpleGeometry {
                            shape: SimpleGeometry.Cube
                            size: 1.0
                        }
                        scale: Qt.vector3d(cubeSize, cubeSize, cubeSize)
                        materials: DefaultMaterial {
                            diffuseColor: "#5b6f85"
                            lighting: DefaultMaterial.NoLighting
                            opacity: 0.9
                        }
                        pickable: true
                    }

                    Model {
                        visible: !isCube
                        geometry: SimpleGeometry {
                            shape: SimpleGeometry.Sphere
                            radius: 0.5
                            rings: 16
                            slices: 16
                        }
                        scale: Qt.vector3d(sphereRadius * 2, sphereRadius * 2, sphereRadius * 2)
                        materials: DefaultMaterial {
                            diffuseColor: "#5b6f85"
                            lighting: DefaultMaterial.NoLighting
                            opacity: 0.9
                        }
                        pickable: true
                    }
                }
            }
        }
    }

    ListModel {
        id: objectsModel
    }

    Item {
        id: inputLayer
        anchors.fill: parent
        focus: true

        property bool dragging: false
        property real lastX: 0
        property real lastY: 0

        Keys.onPressed: {
            if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_R) {
                resetView()
                event.accepted = true
            }
        }

        MouseArea {
            anchors.fill: inputLayer
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
            onPressed: function(mouse) {
                inputLayer.dragging = true
                inputLayer.lastX = mouse.x
                inputLayer.lastY = mouse.y
                mouse.accepted = true
            }
            onReleased: function(mouse) {
                inputLayer.dragging = false
                mouse.accepted = true
            }
            onWheel: function(wheel) {
                var delta = wheel.angleDelta.y / 120.0
                orbitDistance = Math.max(2.0, Math.min(100.0, orbitDistance - delta))
                updateCameraFromOrbit()
                wheel.accepted = true
            }
            onPositionChanged: function(mouse) {
                if (inputLayer.dragging) {
                    var dx = mouse.x - inputLayer.lastX
                    var dy = mouse.y - inputLayer.lastY
                    inputLayer.lastX = mouse.x
                    inputLayer.lastY = mouse.y

                    orbitYaw -= dx * 0.3
                    orbitPitch = Math.max(-80, Math.min(80, orbitPitch - dy * 0.3))
                    updateCameraFromOrbit()
                }
                var hit = view.pick(mouse.x, mouse.y)
                if (hit && hit.objectHit) {
                    var zUp = root.sceneToZUp(hit.scenePosition)
                    root.coordX = zUp.x
                    root.coordY = zUp.y
                    root.coordZ = zUp.z
                }
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 12
        radius: 6
        color: "#12121b"
        border.color: "#2b2b3b"
        border.width: 1
        opacity: 0.9

        Column {
            padding: 10
            spacing: 4
            Text {
                text: "X: " + root.coordX.toFixed(2)
                color: "#c9c9d9"
                font.pixelSize: 14
            }
            Text {
                text: "Y: " + root.coordY.toFixed(2)
                color: "#c9c9d9"
                font.pixelSize: 14
            }
            Text {
                text: "Z: " + root.coordZ.toFixed(2)
                color: "#c9c9d9"
                font.pixelSize: 14
            }
        }
    }

    Row {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        spacing: 8

        Button {
            text: "Add Cube"
            onClicked: root.addCube()
        }
        Button {
            text: "Add Sphere"
            onClicked: root.addSphere()
        }
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
            return Math.abs(offset) % majorEvery < 0.001
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
            pickable: true
            castsShadows: false
        }
    }

    component AxisHelper: Node {
        property real axisLength: 50
        property real axisThickness: 0.06
        property real arrowLength: 1.2
        property real arrowRadius: 0.22

        AxisLine { length: axisLength; thickness: axisThickness; color: "#E0E7FF"; axis: Qt.vector3d(1, 0, 0) }
        AxisLine { length: axisLength; thickness: axisThickness; color: "#9CA3AF"; axis: Qt.vector3d(0, 0, 1) }
        AxisLine { length: axisLength; thickness: axisThickness; color: "#4B5563"; axis: Qt.vector3d(0, 1, 0) }

        AxisLine { length: axisLength; thickness: axisThickness; color: "#E0E7FF"; axis: Qt.vector3d(-1, 0, 0) }
        AxisLine { length: axisLength; thickness: axisThickness; color: "#9CA3AF"; axis: Qt.vector3d(0, 0, -1) }
        AxisLine { length: axisLength; thickness: axisThickness; color: "#4B5563"; axis: Qt.vector3d(0, -1, 0) }

        AxisArrow {
            color: "#E0E7FF"
            direction: Qt.vector3d(1, 0, 0)
            offset: Qt.vector3d(axisLength, 0, 0)
            length: arrowLength
            radius: arrowRadius
        }
        AxisArrow {
            color: "#9CA3AF"
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
            geometry: SimpleGeometry {
                shape: SimpleGeometry.Cube
                size: 1.0
            }
            scale: Qt.vector3d(
                Math.abs(axis.x) > 0 ? length : thickness,
                Math.abs(axis.y) > 0 ? length : thickness,
                Math.abs(axis.z) > 0 ? length : thickness
            )
            position: Qt.vector3d(
                axis.x * length / 2,
                axis.y * length / 2,
                axis.z * length / 2
            )
            materials: DefaultMaterial {
                diffuseColor: axisLineRoot.color
                lighting: DefaultMaterial.NoLighting
            }
            castsShadows: false
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
                return Qt.vector3d(0, 0, -90)
            }
            if (direction.x === -1) {
                return Qt.vector3d(0, 0, 90)
            }
            if (direction.z === 1) {
                return Qt.vector3d(-90, 0, 0)
            }
            if (direction.z === -1) {
                return Qt.vector3d(90, 0, 0)
            }
            if (direction.y === -1) {
                return Qt.vector3d(180, 0, 0)
            }
            return Qt.vector3d(0, 0, 0)
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
        }
    }
}
