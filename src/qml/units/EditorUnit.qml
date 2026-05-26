import QtQuick 2.15
import QtQuick.Controls 6.9
import QtQuick.Controls.Basic 6.9 as Basic
import QtQuick.Layouts 1.15
import QtQuick3D 6.9
import QtQuick.Dialogs 6.9
import ModelManager 1.0

import "qrc:/components/components"
import "qrc:/components/components/theme"

ColumnLayout {
    id: editorRoot

    spacing: 14

    DeepBlueTheme {
        id: theme
    }

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
                property string selectedObjectName: ""
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


                    // 基础配置属性
                    ListModel {
                        id: basicConfigListModel
                        Component.onCompleted: {
                            append([
                            ])
                        }
                    }


                    Node {
                        id: basicNode

                        property var alphaModeMap: ({
                            "Default": PrincipledMaterial.Default,
                            "Blend": PrincipledMaterial.Blend,
                            "Opaque": PrincipledMaterial.Opaque,
                            "Mask": PrincipledMaterial.Mask
                        })

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


                        Repeater3D {
                            id: repeater
                            model: ModelManager.getAllModels()



                            delegate: Model {
                                id: delegateModel

                                property var materialAlphaMode: basicNode.alphaModeMap[modelData.materials.alphaMode]
                                property bool isSelected: editorCanvas.selectedObjectName === objectName

                                objectName: modelData.name ? modelData.name : ("model-" + index)
                                source: modelData.source
                                position: modelData.pos
                                scale: modelData.scale
                                rotation: modelData.rotation
                                pickable: true

                                materials: PrincipledMaterial {
                                    baseColor: delegateModel.isSelected ? "#5CC8FF" : modelData.materials.baseColor
                                    roughness: modelData.materials.roughness
                                    metalness: modelData.materials.metalness
                                    emissiveFactor: delegateModel.isSelected ? "#1A6AA6" : "#000000"
                                    alphaMode: delegateModel.materialAlphaMode ? delegateModel.materialAlphaMode : PrincipledMaterial.Default
                                    cullMode: Material.NoCulling
                                }
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
                    preventStealing: true

                    onPressed: function (mouse) {
                        editorCanvas.lastMousePoint = Qt.point(mouse.x, mouse.y);
                        if (mouse.button === Qt.LeftButton) {
                            const pickResult = editorView.pick(mouse.x, mouse.y);
                            if (pickResult && pickResult.objectHit) {
                                editorCanvas.selectedObjectName = pickResult.objectHit.objectName;
                                // inspectorPanel.m_pos.x =
                                var models = ModelManager.findModelsByName(editorCanvas.selectedObjectName);
                                if (models.length > 0) {
                                    var model = models[0];
                                    inspectorPanel.m_pos = Qt.vector3d(model.pos.x, model.pos.y, model.pos.z);
                                    inspectorPanel.m_scale = Qt.vector3d(model.scale.x, model.scale.y, model.scale.z);
                                    inspectorPanel.m_rotation = model.rotation;
                                    inspectorPanel.m_mass = model.mass;
                                    inspectorPanel.m_name = model.name;
                                    inspectorPanel.m_source = model.source;
                                    // 从 materials 获取材质属性
                                    inspectorPanel.m_baseColor = model.materials.baseColor;
                                    inspectorPanel.m_metalness = model.materials.metalness;
                                    inspectorPanel.m_roughness = model.materials.roughness;
                                }

                            } else {
                                editorCanvas.selectedObjectName = "";
                            }
                        }
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

        ColumnLayout {
            Layout.preferredWidth: 370
            Layout.minimumWidth: 370
            Layout.maximumWidth: 370
            Layout.fillHeight: true
            spacing: 12


            GlassPanel {
                id: inspectorPanel
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "属性 / Inspector"

                property real m_mass: 0.0
                property real m_loadTime: 0.0
                property real m_restitution: 1.0
                property string m_name: ""
                property string m_source: ""
                property string m_type: ""
                // property color m_color: "#5CC8FF"
                property vector3d m_pos: Qt.vector3d(0, 0, 0)
                property vector3d m_scale: Qt.vector3d(1, 1, 1)
                property quaternion m_rotation: Qt.quaternion(1, 0, 0, 0)
                property vector3d m_initialVelo: Qt.vector3d(0, 0, 0)
                property vector3d m_initialAnguVelo: Qt.vector3d(0, 0, 0)

                property color m_baseColor: "#8FBCE8"
                property real m_metalness: 0.2
                property real m_roughness: 0.6

                ColorDialog {
                    id: baseColorDialog
                    title: "选择基础颜色"
                    selectedColor: inspectorPanel.m_baseColor
                    onAccepted: inspectorPanel.m_baseColor = baseColorDialog.currentColor
                }

                ScrollView {
                    id: inspectorScrollView
                    anchors.fill: parent
                    contentWidth: inspectorScrollView.availableWidth
                    clip: true

                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    rightPadding: inspectorScrollBar.visible ? inspectorScrollBar.width : 0

                    ScrollBar.vertical: Basic.ScrollBar {
                        id: inspectorScrollBar
                        width: 8
                        policy: ScrollBar.AsNeeded
                        visible: size < 1.0
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        minimumSize: 0.2
                        background: Rectangle {
                            radius: 4
                            color: "#0F2A4C"
                            border.width: 1
                            border.color: theme.border
                            opacity: 0.6
                        }
                        contentItem: Rectangle {
                            radius: 4
                            implicitWidth: 8
                            color: theme.accent
                            opacity: inspectorScrollBar.pressed ? 0.9 : 0.7
                        }
                    }

                    ColumnLayout {
                        width: Math.max(0, inspectorScrollView.availableWidth - inspectorScrollView.rightPadding)
                        spacing: 10

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            radius: 6
                            color: "#0F2B4B"
                            border.width: 1
                            border.color: "#2C5D88"

                            Text {
                                anchors.centerIn: parent
                                text: "基础 / Base"
                                color: "#CFE6FF"
                                font.pixelSize: 12
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 8

                            Label { text: "名称"; color: "#BFD8F4" }
                            Rectangle {
                                id: nameFieldShell
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                radius: theme.radiusS
                                color: theme.bg1
                                border.width: 1
                                border.color: nameTextField.activeFocus ? theme.accent : theme.border

                                TextField {
                                    id: nameTextField
                                    anchors.fill: parent
                                    text: inspectorPanel.m_name
                                    onTextChanged: inspectorPanel.m_name = text
                                    color: "#FFFFFF"
                                    horizontalAlignment: Text.AlignLeft
                                    verticalAlignment: Text.AlignVCenter
                                    leftPadding: 10
                                    rightPadding: 10
                                    selectByMouse: true
                                    background: null
                                }
                            }

                            Label { text: "资源"; color: "#BFD8F4" }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                radius: theme.radiusS
                                color: theme.bg1
                                border.width: 1
                                border.color: sourceTextField.activeFocus ? theme.accent : theme.border

                                TextField {
                                    id: sourceTextField
                                    anchors.fill: parent
                                    text: inspectorPanel.m_source
                                    onTextChanged: inspectorPanel.m_source = text
                                    color: "#FFFFFF"
                                    horizontalAlignment: Text.AlignLeft
                                    verticalAlignment: Text.AlignVCenter
                                    leftPadding: 10
                                    rightPadding: 10
                                    selectByMouse: true
                                    background: null
                                }
                            }

                            Label { text: "类型"; color: "#BFD8F4" }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                radius: theme.radiusS
                                color: theme.bg1
                                border.width: 1
                                border.color: typeTextField.activeFocus ? theme.accent : theme.border

                                TextField {
                                    id: typeTextField
                                    anchors.fill: parent
                                    text: inspectorPanel.m_type
                                    onTextChanged: inspectorPanel.m_type = text
                                    color: "#FFFFFF"
                                    horizontalAlignment: Text.AlignLeft
                                    verticalAlignment: Text.AlignVCenter
                                    leftPadding: 10
                                    rightPadding: 10
                                    selectByMouse: true
                                    background: null
                                }
                            }

                            Label { text: "质量"; color: "#BFD8F4" }
                            FloatField {
                                Layout.fillWidth: true
                                from: 0
                                to: 100000
                                stepSize: 0.1
                                value: inspectorPanel.m_mass
                                onValueChanged: inspectorPanel.m_mass = value
                            }

                            Label { text: "加载时间"; color: "#BFD8F4" }
                            FloatField {
                                Layout.fillWidth: true
                                from: 0
                                to: 120
                                stepSize: 0.1
                                value: inspectorPanel.m_loadTime
                                onValueChanged: inspectorPanel.m_loadTime = value
                            }

                            Label { text: "弹性"; color: "#BFD8F4" }
                            FloatField {
                                Layout.fillWidth: true
                                from: 0
                                to: 2
                                stepSize: 0.05
                                value: inspectorPanel.m_restitution
                                onValueChanged: inspectorPanel.m_restitution = value
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            radius: 6
                            color: "#0F2B4B"
                            border.width: 1
                            border.color: "#2C5D88"

                            Text {
                                anchors.centerIn: parent
                                text: "变换 / Transform"
                                color: "#CFE6FF"
                                font.pixelSize: 12
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 8

                            Label {
                                text: "位置"
                                color: "#BFD8F4"
                                Layout.columnSpan: 2
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.columnSpan: 2
                                spacing: 6
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_pos.x
                                    stepSize: 0.1
                                    onValueChanged: inspectorPanel.m_pos = Qt.vector3d(value, inspectorPanel.m_pos.y, inspectorPanel.m_pos.z)
                                }
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_pos.y
                                    stepSize: 0.1
                                    onValueChanged: inspectorPanel.m_pos = Qt.vector3d(inspectorPanel.m_pos.x, value, inspectorPanel.m_pos.z)
                                }
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_pos.z
                                    stepSize: 0.1
                                    onValueChanged: inspectorPanel.m_pos = Qt.vector3d(inspectorPanel.m_pos.x, inspectorPanel.m_pos.y, value)
                                }
                            }

                            Label {
                                text: "缩放"
                                color: "#BFD8F4"
                                Layout.columnSpan: 2
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.columnSpan: 2
                                spacing: 6
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_scale.x
                                    stepSize: 0.05
                                    onValueChanged: inspectorPanel.m_scale = Qt.vector3d(value, inspectorPanel.m_scale.y, inspectorPanel.m_scale.z)
                                }
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_scale.y
                                    stepSize: 0.05
                                    onValueChanged: inspectorPanel.m_scale = Qt.vector3d(inspectorPanel.m_scale.x, value, inspectorPanel.m_scale.z)
                                }
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_scale.z
                                    stepSize: 0.05
                                    onValueChanged: inspectorPanel.m_scale = Qt.vector3d(inspectorPanel.m_scale.x, inspectorPanel.m_scale.y, value)
                                }
                            }

                            Label {
                                text: "旋转 (x,y,z,w)"
                                color: "#BFD8F4"
                                Layout.columnSpan: 2
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.columnSpan: 2
                                spacing: 6
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_rotation.x
                                    stepSize: 0.05
                                    onValueChanged: inspectorPanel.m_rotation = Qt.quaternion(inspectorPanel.m_rotation.scalar, value, inspectorPanel.m_rotation.y, inspectorPanel.m_rotation.z)
                                }
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_rotation.y
                                    stepSize: 0.05
                                    onValueChanged: inspectorPanel.m_rotation = Qt.quaternion(inspectorPanel.m_rotation.scalar, inspectorPanel.m_rotation.x, value, inspectorPanel.m_rotation.z)
                                }
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_rotation.z
                                    stepSize: 0.05
                                    onValueChanged: inspectorPanel.m_rotation = Qt.quaternion(inspectorPanel.m_rotation.scalar, inspectorPanel.m_rotation.x, inspectorPanel.m_rotation.y, value)
                                }
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_rotation.scalar
                                    stepSize: 0.05
                                    onValueChanged: inspectorPanel.m_rotation = Qt.quaternion(value, inspectorPanel.m_rotation.x, inspectorPanel.m_rotation.y, inspectorPanel.m_rotation.z)
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            radius: 6
                            color: "#0F2B4B"
                            border.width: 1
                            border.color: "#2C5D88"

                            Text {
                                anchors.centerIn: parent
                                text: "初始速度 / Initial"
                                color: "#CFE6FF"
                                font.pixelSize: 12
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 8

                            Label {
                                text: "速度"
                                color: "#BFD8F4"
                                Layout.columnSpan: 2
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.columnSpan: 2
                                spacing: 6
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_initialVelo.x
                                    stepSize: 0.1
                                    onValueChanged: inspectorPanel.m_initialVelo = Qt.vector3d(value, inspectorPanel.m_initialVelo.y, inspectorPanel.m_initialVelo.z)
                                }
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_initialVelo.y
                                    stepSize: 0.1
                                    onValueChanged: inspectorPanel.m_initialVelo = Qt.vector3d(inspectorPanel.m_initialVelo.x, value, inspectorPanel.m_initialVelo.z)
                                }
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_initialVelo.z
                                    stepSize: 0.1
                                    onValueChanged: inspectorPanel.m_initialVelo = Qt.vector3d(inspectorPanel.m_initialVelo.x, inspectorPanel.m_initialVelo.y, value)
                                }
                            }

                            Label {
                                text: "角速度"
                                color: "#BFD8F4"
                                Layout.columnSpan: 2
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.columnSpan: 2
                                spacing: 6
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_initialAnguVelo.x
                                    stepSize: 0.1
                                    onValueChanged: inspectorPanel.m_initialAnguVelo = Qt.vector3d(value, inspectorPanel.m_initialAnguVelo.y, inspectorPanel.m_initialAnguVelo.z)
                                }
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_initialAnguVelo.y
                                    stepSize: 0.1
                                    onValueChanged: inspectorPanel.m_initialAnguVelo = Qt.vector3d(inspectorPanel.m_initialAnguVelo.x, value, inspectorPanel.m_initialAnguVelo.z)
                                }
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_initialAnguVelo.z
                                    stepSize: 0.1
                                    onValueChanged: inspectorPanel.m_initialAnguVelo = Qt.vector3d(inspectorPanel.m_initialAnguVelo.x, inspectorPanel.m_initialAnguVelo.y, value)
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            radius: 6
                            color: "#0F2B4B"
                            border.width: 1
                            border.color: "#2C5D88"

                            Text {
                                anchors.centerIn: parent
                                text: "材质 / Material"
                                color: "#CFE6FF"
                                font.pixelSize: 12
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 8

                            Label { text: "基础色"; color: "#BFD8F4" }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Rectangle {
                                    width: 28
                                    height: 18
                                    radius: 4
                                    color: inspectorPanel.m_baseColor
                                    border.width: 1
                                    border.color: "#2C5D88"
                                }
                                Rectangle {
                                    width: 48
                                    height: 22
                                    radius: 6
                                    color: pickerMouse.pressed ? "#214785" : (pickerMouse.containsMouse ? "#1A3A72" : "#102B4B")
                                    border.width: 1
                                    border.color: pickerMouse.containsMouse ? theme.accent : "#2C5D88"

                                    Text {
                                        anchors.centerIn: parent
                                        text: "选择"
                                        color: "#FFFFFF"
                                        font.pixelSize: 12
                                    }

                                    MouseArea {
                                        id: pickerMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: baseColorDialog.open()
                                    }
                                }
                            }

                            Label { text: "金属度"; color: "#BFD8F4" }
                            FloatField {
                                Layout.fillWidth: true
                                from: 0
                                to: 1
                                stepSize: 0.05
                                value: inspectorPanel.m_metalness
                                onValueChanged: inspectorPanel.m_metalness = value
                            }

                            Label { text: "粗糙度"; color: "#BFD8F4" }
                            FloatField {
                                Layout.fillWidth: true
                                from: 0
                                to: 1
                                stepSize: 0.05
                                value: inspectorPanel.m_roughness
                                onValueChanged: inspectorPanel.m_roughness = value
                            }
                        }

                        Item {
                            Layout.fillHeight: true
                        }
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
