import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick3D 6.9
import "qrc:/components/components"

ColumnLayout {
    id: editorRoot

    spacing: 14

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 12


        GlassPanel {
            id: editorViewportPanel
            Layout.fillWidth: true
            Layout.minimumWidth: 520
            Layout.fillHeight: true
            title: "场景编辑器 / Infinite Stage"

            Item {
                id: editorCanvas
                anchors.fill: parent
                property real orbitYaw: 35
                property real orbitPitch: 45
                property real orbitDistance: 1200
                property real orbitTargetX: 0
                property real orbitTargetY: 0
                property real orbitTargetZ: 0
                property real gridSpacing: 100
                property int gridCount: 401
                property point lastMousePoint: Qt.point(0, 0)

                function clamp(value, minValue, maxValue) {
                    return Math.max(minValue, Math.min(maxValue, value));
                }

                function updateGridParams() {
                    const distance = orbitDistance;
                    const spacing = distance < 800 ? 50 : (distance < 2500 ? 100 : 200);
                    if (Math.abs(spacing - gridSpacing) > 0.1) {
                        gridSpacing = spacing;
                    }
                    const desired = Math.floor((distance * 2.2) / Math.max(gridSpacing, 0.1)) * 2 + 1;
                    gridCount = Math.max(201, Math.min(desired, 1201));
                }

                function cameraPhysicsPosition() {
                    const yawRad = orbitYaw * Math.PI / 180.0;
                    const pitchRad = orbitPitch * Math.PI / 180.0;
                    const cp = Math.cos(pitchRad);
                    const sp = Math.sin(pitchRad);
                    const cy = Math.cos(yawRad);
                    const sy = Math.sin(yawRad);

                    const px = orbitTargetX + orbitDistance * cp * cy;
                    const py = orbitTargetY + orbitDistance * cp * sy;
                    const pz = orbitTargetZ + orbitDistance * sp;
                    return Qt.vector3d(px, py, pz);
                }

                function toRenderVector(x, y, z) {
                    return Qt.vector3d(x, z, y);
                }

                function updateCameraPose() {
                    updateGridParams();
                    const cam = cameraPhysicsPosition();
                    const target = toRenderVector(orbitTargetX, orbitTargetY, orbitTargetZ);
                    editorCamera.position = toRenderVector(cam.x, cam.y, cam.z);
                    editorCamera.lookAt(target);
                }

                Component.onCompleted: {
                    updateCameraPose();
                }

                View3D {
                    id: editorView
                    anchors.fill: parent
                    camera: editorCamera

                    environment: SceneEnvironment {
                        backgroundMode: SceneEnvironment.Color
                        clearColor: "#061425"
                        antialiasingMode: SceneEnvironment.MSAA
                        antialiasingQuality: SceneEnvironment.VeryHigh
                    }

                    PerspectiveCamera {
                        id: editorCamera
                        fieldOfView: 40
                        clipNear: 1
                        clipFar: 80000
                    }

                    DirectionalLight {
                        eulerRotation: Qt.vector3d(-35, 25, 15)
                        brightness: 1.6
                    }

                    DirectionalLight {
                        eulerRotation: Qt.vector3d(-120, -25, -80)
                        brightness: 0.8
                    }

                    Node {
                        Coordinate {
                            id: editorCoordinate
                            orbitDistance: editorCanvas.orbitDistance
                            gridStep: editorCanvas.gridSpacing
                            maxGridLines: editorCanvas.gridCount
                            gridExtent: (editorCanvas.gridCount - 1) * editorCanvas.gridSpacing * 0.5
                            camera: editorCamera
                            viewportWidth: editorView.width
                            viewportHeight: editorView.height
                        }

                        Model {
                            source: "#Cube"
                            position: Qt.vector3d(0, 80, 0)
                            scale: Qt.vector3d(1.2, 0.12, 1.2)
                            materials: PrincipledMaterial {
                                baseColor: "#4CA3FF"
                                roughness: 0.3
                                metalness: 0.1
                            }
                        }
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.leftMargin: 12
                    anchors.topMargin: 10
                    radius: 8
                    color: "#0D274899"
                    border.width: 1
                    border.color: "#2B5E89"
                    width: 320
                    height: 72

                    Text {
                        anchors.fill: parent
                        anchors.margins: 10
                        color: "#D4EAFB"
                        font.pixelSize: 12
                        text: "无限网格 / World z-up\n右键旋转, 中键平移, 滚轮缩放"
                    }
                }

                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.rightMargin: 12
                    anchors.topMargin: 10
                    radius: 8
                    color: "#0D274899"
                    border.width: 1
                    border.color: "#2B5E89"
                    width: 210
                    height: 72

                    Column {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 4

                        Text {
                            text: "工具: 选择"
                            color: "#CFE6FF"
                            font.pixelSize: 12
                        }
                        Text {
                            text: "单位: 米 | 捕捉: 10cm"
                            color: "#9EC4EA"
                            font.pixelSize: 11
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton

                    onPressed: function (mouse) {
                        editorCanvas.lastMousePoint = Qt.point(mouse.x, mouse.y);
                    }

                    onPositionChanged: function (mouse) {
                        const dx = mouse.x - editorCanvas.lastMousePoint.x;
                        const dy = mouse.y - editorCanvas.lastMousePoint.y;
                        editorCanvas.lastMousePoint = Qt.point(mouse.x, mouse.y);

                        if ((mouse.buttons & Qt.RightButton) !== 0) {
                            editorCanvas.orbitYaw -= dx * 0.22;
                            editorCanvas.orbitPitch = editorCanvas.clamp(editorCanvas.orbitPitch - dy * 0.2, 10, 85);
                            editorCanvas.updateCameraPose();
                        }

                        if ((mouse.buttons & Qt.MiddleButton) !== 0) {
                            editorCanvas.orbitTargetX -= dx * 1.6;
                            editorCanvas.orbitTargetY += dy * 1.6;
                            editorCanvas.updateCameraPose();
                        }
                    }

                    onWheel: function (wheel) {
                        const factor = wheel.angleDelta.y > 0 ? 0.86 : 1.16;
                        editorCanvas.orbitDistance = editorCanvas.clamp(editorCanvas.orbitDistance * factor, 120, 12000);
                        editorCanvas.updateCameraPose();
                    }
                }
            }
        }
        GlassPanel {
            id: editorToolRail
            Layout.preferredWidth: 86
            Layout.minimumWidth: 86
            Layout.maximumWidth: 86
            Layout.fillHeight: true
            title: "工具"

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                Repeater {
                    model: ["选择", "移动", "旋转", "缩放", "测量", "标注"]

                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        radius: 10
                        property bool hovered: false
                        color: hovered ? "#1A3A72" : "#102B4B"
                        border.width: 1
                        border.color: hovered ? "#4CA3FF" : "#2C5D88"

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: {
                                parent.hovered = true;
                            }
                            onExited: {
                                parent.hovered = false;
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: modelData
                            color: "#B8D6F5"
                            font.pixelSize: 12
                        }
                    }
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }
        ColumnLayout {
            Layout.preferredWidth: 330
            Layout.minimumWidth: 330
            Layout.maximumWidth: 330
            Layout.fillHeight: true
            spacing: 12

            GlassPanel {
                Layout.fillWidth: true
                Layout.preferredHeight: 290
                title: "层级 / Outliner"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    Repeater {
                        model: ["World", "Terrain", "LightRig", "Camera", "EditorPreview"]

                        delegate: Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 38
                            radius: 8
                            property bool hovered: false
                            color: hovered ? "#12365D" : "#112F52"
                            border.width: 1
                            border.color: "#2C5D88"

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                onEntered: {
                                    parent.hovered = true;
                                }
                                onExited: {
                                    parent.hovered = false;
                                }
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                text: modelData
                                color: "#BFD8F4"
                                font.pixelSize: 12
                            }
                        }
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }

            GlassPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "属性 / Inspector"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    Repeater {
                        model: [
                            "位置: (0.0, 0.0, 0.0)",
                            "旋转: (0°, 0°, 0°)",
                            "缩放: (1.0, 1.0, 1.0)",
                            "材质: EgretBase",
                            "渲染层: Default"
                        ]

                        delegate: Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 40
                            radius: 8
                            property bool hovered: false
                            color: hovered ? "#12365D" : "#112F52"
                            border.width: 1
                            border.color: "#2C5D88"

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                onEntered: {
                                    parent.hovered = true;
                                }
                                onExited: {
                                    parent.hovered = false;
                                }
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                text: modelData
                                color: "#BFD8F4"
                                font.pixelSize: 12
                            }
                        }
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }
        }
    }

    GlassPanel {
        Layout.fillWidth: true
        Layout.preferredHeight: 170
        title: "时间轴 / Timeline"

        RowLayout {
            anchors.fill: parent
            spacing: 10

            Repeater {
                model: 6
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 10
                    property bool hovered: false
                    color: hovered ? "#1A3B83" : "#112F52"
                    border.width: 1
                    border.color: "#2C5D88"

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            parent.hovered = true;
                        }
                        onExited: {
                            parent.hovered = false;
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "片段 " + (index + 1)
                        color: "#9EC4EA"
                        font.pixelSize: 12
                    }
                }
            }
        }
    }
}
