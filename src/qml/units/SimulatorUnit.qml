import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Basic 2.15 as Basic
import QtQuick.Layouts 1.15
import QtQuick3D 6.9
import EnumHandler 1.0
import ModelManager 1.0

import "qrc:/components/components"

ColumnLayout {
    id: simulatorRoot

    property bool dataStreamVisible: true
    property bool infoPanelVisible: true
    property var theme
    property alias coordinateSystem: coordinateSystem


    spacing: 14

    function formatVec(x, y, z) {
        return "(" + x.toFixed(2) + ", " + y.toFixed(2) + ", " + z.toFixed(2) + ")";
    }

    property int extraDataBlocks: 0
    property int dataBlockWidth: 230

    function addDataBlock() {
        extraDataBlocks += 1;
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 12

        GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: "场景视图 / Dynamics Canvas"

            Item {
                id: canvas3d
                anchors.fill: parent
                property real orbitYaw: 45
                property real orbitPitch: 55
                property real orbitDistance: 900
                property real orbitTargetX: 0
                property real orbitTargetY: 0
                property real orbitTargetZ: 0
                property real lineThickness: 0.45
                property int gridCount: 41
                property real gridSpacing: 50
                property int activeDragBodyId: -1
                property point lastMousePoint: Qt.point(0, 0)

                function clamp(value, minValue, maxValue) {
                    return Math.max(minValue, Math.min(maxValue, value));
                }

                function cameraState() {
                    const cam = cameraPhysicsPosition();
                    return {
                        positionX: cam.x,
                        positionY: cam.y,
                        positionZ: cam.z,
                        targetX: orbitTargetX,
                        targetY: orbitTargetY,
                        targetZ: orbitTargetZ,
                        upX: 0,
                        upY: 0,
                        upZ: 1,
                        fovY: sceneCamera.fieldOfView,
                        nearClip: sceneCamera.clipNear,
                        farClip: sceneCamera.clipFar
                    };
                }

                function updateCameraPose() {
                    updateGridSpacing();
                    updateGridLineCount();
                    const cam = cameraPhysicsPosition();
                    const target = toRenderVector(orbitTargetX, orbitTargetY, orbitTargetZ);
                    sceneCamera.position = toRenderVector(cam.x, cam.y, cam.z);
                    sceneCamera.lookAt(target);
                }

                function updateGridSpacing() {
                    const distance = orbitDistance;
                    const newSpacing = distance < 400 ? 20 : (distance < 1200 ? 50 : 100);
                    if (Math.abs(newSpacing - gridSpacing) > 0.1) {
                        gridSpacing = newSpacing;
                    }
                }

                function updateGridLineCount() {
                    const h = sceneView.height;
                    const w = sceneView.width;
                    if (h <= 0 || w <= 0) {
                        return;
                    }
                    const fovRad = sceneCamera.fieldOfView * Math.PI / 180.0;
                    const halfHeight = orbitDistance * Math.tan(fovRad / 2.0);
                    const aspect = w / h;
                    const halfWidth = halfHeight * aspect;
                    const extent = Math.max(halfWidth, halfHeight) * 1.2;
                    const desired = Math.floor(extent / Math.max(gridSpacing, 0.05)) * 2 + 1;
                    const clamped = Math.max(41, Math.min(desired, 1201));
                    if (gridCount !== clamped) {
                        gridCount = clamped;
                    }
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

                function shapeSource(kind) {
                    return resourceHelper.getSourceByShape(kind);
                }

                function scaleForShape(kind, sx, sy, sz) {
                    const unit = 100.0;
                    const safeX = Math.max(1.0, sx);
                    const safeY = Math.max(1.0, sy);
                    const safeZ = Math.max(1.0, sz);
                    if (kind === "standard_sphere" || kind === "standard_spherical_shell" || kind === "standard_disk" || kind === "standard_ring") {
                        const uniform = Math.max(safeX, safeY, safeZ) / unit;
                        return Qt.vector3d(uniform, uniform, uniform);
                    }
                    if (kind === "standard_cylinder" || kind === "standard_cylinder_shell") {
                        return Qt.vector3d(safeX / unit, safeY / unit, safeZ / unit);
                    }
                    if (kind === "standard_rod") {
                        return Qt.vector3d(6 / unit, 6 / unit, safeZ / unit);
                    }
                    return Qt.vector3d(safeX / unit, safeY / unit, safeZ / unit);
                }

                Component.onCompleted: {
                    updateCameraPose();
                }

                View3D {
                    id: sceneView
                    anchors.fill: parent
                    camera: sceneCamera

                    environment: SceneEnvironment {
                        backgroundMode: SceneEnvironment.Color
                        clearColor: "#061425"
                        antialiasingMode: SceneEnvironment.MSAA
                        antialiasingQuality: SceneEnvironment.VeryHigh
                    }

                    PerspectiveCamera {
                        id: sceneCamera
                        fieldOfView: 45
                        clipNear: 1
                        clipFar: 50000
                    }

                    DirectionalLight {
                        eulerRotation: Qt.vector3d(-35, 22, 20)
                        brightness: 1.8
                    }

                    DirectionalLight {
                        eulerRotation: Qt.vector3d(-120, -30, -70)
                        brightness: 0.7
                    }

                    Node {
                        id: worldRoot
                        Coordinate {
                            id: coordinateSystem
                            orbitDistance: canvas3d.orbitDistance
                            gridStep: canvas3d.gridSpacing
                            maxGridLines: canvas3d.gridCount
                            gridExtent: (canvas3d.gridCount - 1) * canvas3d.gridSpacing * 0.5
                            camera: sceneCamera
                            viewportWidth: sceneView.width
                            viewportHeight: sceneView.height
                        }

                        Repeater3D {
                            model: sceneController.bodyModel

                            delegate: Model {
                                id: modelXX
                                objectName: "body-" + bodyId
                                source: canvas3d.shapeSource(bodyKind)
                                property var renderCenter: bodyPosition
                                property var renderScale: bodyScale
                                property var renderRotation: bodyRotation

                                position: Qt.vector3d(renderCenter[0], renderCenter[1], renderCenter[2])
                                scale: Qt.vector3d(renderScale[0], renderScale[1], renderScale[2])
                                // scale: Qt.vector3d(renderScale[0], renderScale[1], renderScale[2])
                                rotation: Qt.quaternion(renderRotation[0], renderRotation[1], renderRotation[2], renderRotation[3])

                                materials: PrincipledMaterial {
                                    baseColor: bodyColor
                                    roughness: 0.18
                                    specularAmount: 0.7
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.leftMargin: 10
                    anchors.topMargin: 8
                    radius: 8
                    color: "#0D274899"
                    border.width: 1
                    border.color: "#2B5E89"
                    width: 300
                    height: 66

                    Text {
                        anchors.fill: parent
                        anchors.margins: 10
                        color: "#D4EAFB"
                        font.pixelSize: 12
                        text: "坐标系: World z-up, 右手系\n左键拖拽实体(投影到 XY 平面), 右键旋转, 滚轮缩放"
                    }
                }

                MouseArea {
                    id: mouseLayer
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                    preventStealing: true

                    onPressed: function (mouse) {
                        canvas3d.lastMousePoint = Qt.point(mouse.x, mouse.y);
                        if (mouse.button === Qt.LeftButton) {
                            const pickResult = sceneView.pick(mouse.x, mouse.y);
                            if (pickResult && pickResult.objectHit && pickResult.objectHit.objectName.indexOf("body-") === 0) {
                                const idText = pickResult.objectHit.objectName.substring(5);
                                canvas3d.activeDragBodyId = Number(idText);
                                sceneController.beginBodyDrag(canvas3d.activeDragBodyId, mouse.x, mouse.y, width, height, canvas3d.cameraState(), "xy_plane");
                            }
                        }
                    }

                    onPositionChanged: function (mouse) {
                        const dx = mouse.x - canvas3d.lastMousePoint.x;
                        const dy = mouse.y - canvas3d.lastMousePoint.y;
                        canvas3d.lastMousePoint = Qt.point(mouse.x, mouse.y);

                        if ((mouse.buttons & Qt.RightButton) !== 0) {
                            canvas3d.orbitYaw -= dx * 0.22;
                            canvas3d.orbitPitch = canvas3d.clamp(canvas3d.orbitPitch - dy * 0.22, -90, 90);
                            canvas3d.updateCameraPose();
                        }

                        if ((mouse.buttons & Qt.LeftButton) !== 0 && canvas3d.activeDragBodyId >= 0) {
                            sceneController.updateBodyDrag(mouse.x, mouse.y, width, height, canvas3d.cameraState());
                        }
                    }

                    onReleased: function (mouse) {
                        if (mouse.button === Qt.LeftButton) {
                            canvas3d.activeDragBodyId = -1;
                            sceneController.endBodyDrag();
                        }
                    }

                    onWheel: function (wheel) {
                        const factor = wheel.angleDelta.y > 0 ? 0.88 : 1.12;
                        canvas3d.orbitDistance = canvas3d.clamp(canvas3d.orbitDistance * factor, 120, 8000);
                        canvas3d.updateCameraPose();
                    }
                }
            }
        }

        ColumnLayout {
            id: rightInfoColumn
            Layout.preferredWidth: simulatorRoot.infoPanelVisible ? 320 : 0
            Layout.minimumWidth: simulatorRoot.infoPanelVisible ? 320 : 0
            Layout.maximumWidth: simulatorRoot.infoPanelVisible ? 320 : 0
            Layout.fillHeight: true
            visible: simulatorRoot.infoPanelVisible
            opacity: simulatorRoot.infoPanelVisible ? 1 : 0
            enabled: simulatorRoot.infoPanelVisible

            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8

                MetricCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                    metricName: "积分步长"
                    metricValue: Number(sceneController.fixedStepMs).toFixed(2)
                    metricUnit: "ms"
                    metricColor: theme.accent
                    nameFontSize: 10
                    valueFontSize: 18
                    unitFontSize: 10
                    dotSize: 8
                    contentPadding: 10
                    contentSpacing: 6
                }

                MetricCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                    metricName: "实时帧率"
                    metricValue: Number(sceneController.fps).toFixed(1)
                    metricUnit: "FPS"
                    metricColor: theme.success
                    nameFontSize: 10
                    valueFontSize: 18
                    unitFontSize: 10
                    dotSize: 8
                    contentPadding: 10
                    contentSpacing: 6
                }

                MetricCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                    metricName: "活动刚体"
                    metricValue: sceneController.entityCount
                    metricUnit: "obj"
                    metricColor: theme.warning
                    nameFontSize: 10
                    valueFontSize: 18
                    unitFontSize: 10
                    dotSize: 8
                    contentPadding: 10
                    contentSpacing: 6
                }
            }

            GlassPanel {
                id: infoPanel
                Layout.preferredWidth: simulatorRoot.infoPanelVisible ? 320 : 0
                Layout.minimumWidth: simulatorRoot.infoPanelVisible ? 320 : 0
                Layout.maximumWidth: simulatorRoot.infoPanelVisible ? 320 : 0
                Layout.fillHeight: true
                visible: simulatorRoot.infoPanelVisible
                opacity: simulatorRoot.infoPanelVisible ? 1 : 0
                enabled: simulatorRoot.infoPanelVisible
                title: "参数信息"

                ScrollView {
                    id: infoScrollView
                    anchors.fill: parent
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    rightPadding: infoScrollBar.visible ? (infoScrollBar.width) : 0
                    ScrollBar.vertical: Basic.ScrollBar {
                        id: infoScrollBar
                        width: 8
                        policy: ScrollBar.AsNeeded
                        visible: size < 1.0
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom

                        contentItem: Rectangle {
                            radius: 4
                            implicitWidth: 8
                            color: theme.accent
                            opacity: infoScrollBar.pressed ? 0.9 : 0.7
                        }

                        background: Rectangle {
                            radius: 4
                            implicitWidth: 8
                            color: "#0F2A4C"
                            border.width: 1
                            border.color: theme.border
                            opacity: 0.6
                        }
                    }

                    ColumnLayout {
                        width: Math.max(0, infoScrollView.availableWidth - infoScrollView.rightPadding)
                        spacing: 10

                        Repeater {
                            model: ["重力场 g = " + resourceHelper.getStandardGravity() + "m/s^2", "积分器: RK4 (Adaptive)", "碰撞模型: Impulse + Friction", "约束求解: Sequential Impulse", "误差阈值: 1e-6"]

                            delegate: Rectangle {
                                id: repeatItem
                                Layout.fillWidth: true
                                Layout.preferredHeight: 42
                                property bool hovered: false
                                radius: 9
                                color: hovered ? "#122c5d" : "#12365D"
                                border.width: 1
                                border.color: "#2E5D89"

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onEntered: {
                                        repeatItem.hovered = true;
                                    }
                                    onExited: {
                                        repeatItem.hovered = false;
                                    }
                                }

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left
                                    anchors.leftMargin: 10
                                    text: modelData
                                    color: "#BDD8F3"
                                    font.pixelSize: 13
                                }
                            }
                        }

                        Item {
                            Layout.fillHeight: true
                        }
                    }
                }
            }

            GlassPanel {
                id: controlPanel
                Layout.preferredWidth: simulatorRoot.infoPanelVisible ? 320 : 0
                Layout.minimumWidth: simulatorRoot.infoPanelVisible ? 320 : 0
                Layout.maximumWidth: simulatorRoot.infoPanelVisible ? 320 : 0
                visible: simulatorRoot.infoPanelVisible
                opacity: simulatorRoot.infoPanelVisible ? 1 : 0
                enabled: simulatorRoot.infoPanelVisible
                Layout.preferredHeight: 220
                title: "仿真控制"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    Rectangle {
                        id: computeBtnRect
                        Layout.fillWidth: true
                        Layout.preferredHeight: 46
                        property bool hovered: false
                        radius: 10
                        color: hovered ? "#1a3b83" : "#1A4E83"
                        border.width: 1
                        border.color: "#58B8FF"

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: {
                                computeBtnRect.hovered = true;
                            }
                            onExited: {
                                computeBtnRect.hovered = false;
                            }
                            onClicked: {
                                sceneController.toggleRunning();
                            }
                        }
                        Text {
                            anchors.centerIn: parent
                            text: sceneController.running ? "暂停仿真" : "开始仿真"
                            color: "#EAF5FF"
                            font.pixelSize: 14
                            font.bold: true
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
        id: dataStreamPanel
        Layout.fillWidth: true
        Layout.preferredHeight: simulatorRoot.dataStreamVisible ? 170 : 0
        Layout.minimumHeight: simulatorRoot.dataStreamVisible ? 170 : 0
        Layout.maximumHeight: simulatorRoot.dataStreamVisible ? 170 : 0
        visible: simulatorRoot.dataStreamVisible
        opacity: simulatorRoot.dataStreamVisible ? 1 : 0
        enabled: simulatorRoot.dataStreamVisible
        title: "数据流"

        readonly property int scrollBarThickness: 8
        readonly property int scrollBarGap: 6

        Flickable {
            id: dataStreamFlick
            anchors.fill: parent
            anchors.margins: 6
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            contentWidth: dataStreamRow.implicitWidth
            contentHeight: dataStreamRow.implicitHeight

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                hoverEnabled: true

                onWheel: function (wheel) {
                    const step = Math.max(40, Math.abs(wheel.angleDelta.y));
                    const delta = wheel.angleDelta.y > 0 ? -step : step;
                    const maxX = Math.max(0, dataStreamFlick.contentWidth - dataStreamFlick.width);
                    dataStreamFlick.contentX = Math.max(0, Math.min(dataStreamFlick.contentX + delta, maxX));
                    wheel.accepted = true;
                }
            }

            Row {
                id: dataStreamRow
                spacing: 10
                height: dataStreamFlick.height

                Repeater {
                    model: sceneController.bodyModel

                    delegate: Rectangle {
                        width: simulatorRoot.dataBlockWidth
                        height: dataStreamFlick.height
                        radius: 10
                        color: "#112F52"
                        border.width: 1
                        border.color: "#2C5D88"

                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6

                            Text {
                                text: "实体 " + bodyId
                                color: "#EAF5FF"
                                font.pixelSize: 12
                                font.bold: true
                            }
                            Text {
                                text: "速度: " + simulatorRoot.formatVec(bodyVelocity[0], bodyVelocity[1], bodyVelocity[2])
                                color: "#9EC4EA"
                                font.pixelSize: 11
                            }
                            Text {
                                text: "位置: " + simulatorRoot.formatVec(bodyPosition[0], bodyPosition[1], bodyPosition[2])
                                color: "#9EC4EA"
                                font.pixelSize: 11
                            }
                        }
                    }
                }

                Repeater {
                    model: simulatorRoot.extraDataBlocks

                    delegate: Rectangle {
                        width: simulatorRoot.dataBlockWidth
                        height: dataStreamFlick.height
                        radius: 10
                        color: "#0F2B4A"
                        border.width: 1
                        border.color: "#244A70"

                        Text {
                            anchors.centerIn: parent
                            text: "空数据块"
                            color: "#7FA6C8"
                            font.pixelSize: 12
                        }
                    }
                }

                Rectangle {
                    width: 90
                    height: dataStreamFlick.height
                    radius: 10
                    color: "#0D2748"
                    border.width: 1
                    border.color: "#2C5D88"

                    Text {
                        anchors.centerIn: parent
                        text: "+"
                        color: "#8DB3D9"
                        font.pixelSize: 28
                        font.bold: true
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: simulatorRoot.addDataBlock()
                    }
                }
            }

            ScrollBar.horizontal: dataStreamHScrollBar
        }

        Basic.ScrollBar {
            id: dataStreamHScrollBar
            height: dataStreamPanel.scrollBarThickness
            policy: ScrollBar.AsNeeded
            visible: size < 1.0
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 6
            anchors.rightMargin: 6
            anchors.bottomMargin: -(dataStreamPanel.scrollBarThickness + dataStreamPanel.scrollBarGap - 5)

            contentItem: Rectangle {
                radius: 4
                implicitHeight: dataStreamPanel.scrollBarThickness
                color: theme.accent
                opacity: dataStreamHScrollBar.pressed ? 0.9 : 0.7
            }

            background: Rectangle {
                radius: 4
                implicitHeight: dataStreamPanel.scrollBarThickness
                color: "#0F2A4C"
                border.width: 1
                border.color: theme.border
                opacity: 0.6
            }
        }
    }
}
