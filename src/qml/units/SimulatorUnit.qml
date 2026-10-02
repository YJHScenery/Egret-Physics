/**
 * @file        SimulatorUnit.qml
 * @brief       仿真器单元视图文件
 * @details     定义仿真器界面，包含物理场景的3D视图和实体列表展示。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-29
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     View
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Basic 2.15 as Basic
import QtQuick.Layouts 1.15
import QtQuick.Effects
import QtQuick3D 6.9
import EnumHandler 1.0
import EditorManager 1.0
import QtQuick.Dialogs

import EgretPhysics 1.0

import "qrc:/components/components"
import "qrc:/materials/materials"
import "qrc:/components/components/basic"
import "qrc:/components/components/viewport"

ColumnLayout {
    id: simulatorRoot

    property bool dataStreamVisible: true
    property bool infoPanelVisible: true
    property var theme
    property alias coordinateSystem: canvas3d.coordinateSystem

    /** 离线仿真：求解推进步长（毫秒）。 */
    property real simulationSolverStepMs: 0.5

    /** 离线仿真：采样间隔（毫秒），只能取 1 / 2 / 5 / 10 / 20。 */
    property int simulationSampleMs: 10

    /** 离线仿真：仿真总时长（秒）。 */
    property real simulationDurationSeconds: 60

    spacing: 4

    FileDialog {
        id: saveTrackDataFileDialog
        title: "保存追踪数据"
        fileMode: FileDialog.SaveFile
        currentFolder: documentsFolder
        // currentFile: ""
        defaultSuffix: "xlsx"
        nameFilters: [
            "Microsoft Excel Shell (*.xlsx)"
        ]

        property string bodyID: ""
        property string bodyName: ""

        selectedFile: "数据追踪-" + bodyName + "(" + bodyID + ").xlsx"

        onAccepted: {
            var fileUrl = selectedFile;
            var localPath = fileUrl.toString().replace(/^file:\/\/\//, "");

            // 1. 开始录制：激活追踪 + 设置路径 + 启动定时刷新
            sceneController.startRecording(bodyID, localPath);

            // 2. 同步 QML 追踪状态 — 继承用户已选中的项，未选任何项时默认全选
            simulatorRoot.ensureTrackingEntry(bodyID);
            var entry = simulatorRoot.trackingMap[bodyID];
            if (!entry.position && !entry.velocity && !entry.angularVelocity && !entry.kineticEnergy) {
                entry.position = true;
                entry.velocity = true;
                entry.angularVelocity = true;
                entry.kineticEnergy = true;
            }
            sceneController.setBodyTracking(bodyID,
                entry.position, entry.velocity,
                entry.angularVelocity, entry.kineticEnergy);
            simulatorRoot.trackingVersion++;

            // 3. 设置录制状态（控制按钮可见性）
            simulatorRoot.recordingState[bodyID] = "recording";

            console.log("开始录制: " + localPath);
        }

        onRejected: {
            console.log("用户取消保存")
        }
    }

    FileDialog {
        id: saveSimulationFileDialog
        title: "保存离线仿真文件"
        fileMode: FileDialog.SaveFile
        currentFolder: documentsFolder
        defaultSuffix: "egps"
        nameFilters: [
            "Egret 仿真文件 (*.egps)",
            "所有文件 (*)"
        ]

        onAccepted: {
            simulationExporter.saveAs(simulatorRoot.localPathFromUrl(selectedFile));
        }
    }

    /** 把 file:/// URL 转换为本地路径。 */
    function localPathFromUrl(fileUrl) {
        return fileUrl.toString().replace(/^file:\/\/\//, "");
    }

    function formatVec(x, y, z) {
        return "(" + x.toFixed(2) + ", " + y.toFixed(2) + ", " + z.toFixed(2) + ")";
    }

    function formatVecWithMagnitude(x, y, z) {
        return formatVec(x, y, z) + "[" + (Math.sqrt(x ** 2 + y ** 2 + z ** 2)).toFixed(2) + "]"
    }

    // property int extraDataBlocks: 0
    property int dataBlockWidth: 230

    // ============ 数据追踪状态 ============

    /**
     * 追踪状态映射。
     * 结构: { bodyId: { position: bool, velocity: bool, angularVelocity: bool, kineticEnergy: bool } }
     */
    property var trackingMap: ({})

    /** 当前被 pick 的物体 ID（用于构建弹出菜单） */
    property string pickedBodyId: ""

    /** 上下文菜单弹出位置 */
    property point contextMenuPos: Qt.point(0, 0)

    /** 上下文菜单可见性 */
    property bool contextMenuVisible: false

    /** 确保 bodyId 在 trackingMap 中有条目 */
    function ensureTrackingEntry(bodyId) {
        if (!trackingMap[bodyId]) {
            trackingMap[bodyId] = {
                position: false,
                velocity: false,
                angularVelocity: false,
                kineticEnergy: false
            };
        }
    }

    /** 检查 bodyId 是否有任何追踪项被勾选 */
    function hasTrackingData(bodyId) {
        var entry = trackingMap[bodyId];
        if (!entry) return false;
        return entry.position || entry.velocity || entry.angularVelocity || entry.kineticEnergy;
    }

    /** 检查 bodyId 的指定属性是否被追踪 */
    function isTracking(bodyId, propertyName) {
        var entry = trackingMap[bodyId];
        if (!entry) return false;
        return entry[propertyName] === true;
    }

    /** 检查是否有任何物体被追踪（用于控制空状态提示的可见性） */
    function hasAnyTrackingData() {
        for (var key in trackingMap) {
            if (hasTrackingData(key)) return true;
        }
        return false;
    }

    /** 构建指定物体的上下文菜单项数组 */
    function buildContextMenu(bodyId) {
        ensureTrackingEntry(bodyId);

        return [
            {
                text: "查看参数",
                onTriggered: function () {
                    console.log("查看参数: body-" + bodyId);
                    // TODO: 打开参数面板
                }
            },
            {
                text: "跟踪数据",
                submenu: [
                    {
                        text: "位置",
                        checkable: true,
                        checked: trackingMap[bodyId].position,
                        onTriggered: function () {
                            trackingMap[bodyId].position = !trackingMap[bodyId].position;
                            trackingVersion++;
                            sceneController.setBodyTracking(bodyId,
                                trackingMap[bodyId].position,
                                trackingMap[bodyId].velocity,
                                trackingMap[bodyId].angularVelocity,
                                trackingMap[bodyId].kineticEnergy);
                        }
                    },
                    {
                        text: "速度",
                        checkable: true,
                        checked: trackingMap[bodyId].velocity,
                        onTriggered: function () {
                            trackingMap[bodyId].velocity = !trackingMap[bodyId].velocity;
                            trackingVersion++;
                            sceneController.setBodyTracking(bodyId,
                                trackingMap[bodyId].position,
                                trackingMap[bodyId].velocity,
                                trackingMap[bodyId].angularVelocity,
                                trackingMap[bodyId].kineticEnergy);
                        }
                    },
                    {
                        text: "角速度",
                        checkable: true,
                        checked: trackingMap[bodyId].angularVelocity,
                        onTriggered: function () {
                            trackingMap[bodyId].angularVelocity = !trackingMap[bodyId].angularVelocity;
                            trackingVersion++;
                            sceneController.setBodyTracking(bodyId,
                                trackingMap[bodyId].position,
                                trackingMap[bodyId].velocity,
                                trackingMap[bodyId].angularVelocity,
                                trackingMap[bodyId].kineticEnergy);
                        }
                    },
                    {
                        text: "动能",
                        checkable: true,
                        checked: trackingMap[bodyId].kineticEnergy,
                        onTriggered: function () {
                            trackingMap[bodyId].kineticEnergy = !trackingMap[bodyId].kineticEnergy;
                            trackingVersion++;
                            sceneController.setBodyTracking(bodyId,
                                trackingMap[bodyId].position,
                                trackingMap[bodyId].velocity,
                                trackingMap[bodyId].angularVelocity,
                                trackingMap[bodyId].kineticEnergy);
                        }
                    }
                ]
            },
            {
                separator: true
            },
            {
                text: "附加 Gizmo",
                onTriggered: function () {
                    console.log("附加 Gizmo: body-" + bodyId);
                    // TODO: 附加 Gizmo
                }
            }
        ];
    }

    /** 追踪版本计数器 — 用于触发 QML 绑定重新求值（深层 var 对象变更检测） */
    property int trackingVersion: 0

    /**
     * 录制状态映射：{ bodyId: "none" | "recording" | "paused" }
     */
    property var recordingState: ({})

    // function addDataBlock() {
    //     extraDataBlocks += 1;
    // }

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 6


        GlassPanel {
            id: dataStreamPanel
            // Layout.fillWidth: true
            // Layout.preferredHeight: simulatorRoot.dataStreamVisible ? 180 : 0
            // Layout.minimumHeight: simulatorRoot.dataStreamVisible ? 180 : 0
            // Layout.maximumHeight: simulatorRoot.dataStreamVisible ? 180 : 0


            Layout.preferredWidth: 200
            Layout.fillHeight: true

            visible: simulatorRoot.dataStreamVisible
            opacity: simulatorRoot.dataStreamVisible ? 1 : 0
            enabled: simulatorRoot.dataStreamVisible
            title: "数据流"

            readonly property int scrollBarThickness: 8
            readonly property int scrollBarGap: 6

            Flickable {
                id: dataStreamFlick
                anchors.fill: parent
                anchors.margins: 3
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: dataStreamFlick.width
                contentHeight: dataStreamRow.implicitHeight

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    hoverEnabled: true

                    onWheel: function (wheel) {
                        const step = Math.max(40, Math.abs(wheel.angleDelta.y));
                        const delta = wheel.angleDelta.y > 0 ? -step : step;
                        const maxY = Math.max(0, dataStreamFlick.contentHeight - dataStreamFlick.height);
                        dataStreamFlick.contentY = Math.max(0, Math.min(dataStreamFlick.contentY + delta, maxY));
                        wheel.accepted = true;
                    }
                }

                Column {
                    id: dataStreamRow
                    spacing: 6
                    width: dataStreamFlick.width

                    Repeater {
                        model: sceneController.bodyModel

                        delegate: Rectangle {
                            // 通过 trackingVersion 触发重新求值，实现深层 var 变更检测
                            property bool isTracked: {
                                simulatorRoot.trackingVersion;
                                return simulatorRoot.hasTrackingData(bodyId);
                            }
                            visible: isTracked

                            width: parent.width
                            height: dataBlockContent.implicitHeight + 20
                            radius: theme.radiusS
                            color: "#112F52"
                            border.width: 1
                            border.color: "#2C5D88"

                            Column {
                                id: dataBlockContent
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 4

                                // 标题行
                                RowLayout {
                                    width: parent.width
                                    spacing: 8

                                    ColumnLayout{
                                        spacing: 3
                                        Text {
                                            text: "实体: " + bodyName
                                            color: "#EAF5FF"
                                            font.pixelSize: 12
                                            font.bold: true
                                        }

                                        Text {
                                            text: "ID: " + bodyId
                                            color: "#c8c8c8"
                                            font.pixelSize: 8
                                            font.bold: false
                                        }
                                    }


                                    // ---- 录制控制按钮（录制中/暂停时显示） ----
                                    Item {
                                        id: recCtrls
                                        property string state: {
                                            simulatorRoot.trackingVersion;
                                            return simulatorRoot.recordingState[bodyId] || "none";
                                        }
                                        visible: state === "recording" || state === "paused"
                                        Layout.preferredWidth: state === "none" ? 0 : 44
                                        Layout.preferredHeight: 18

                                        RowLayout {
                                            anchors.fill: parent
                                            spacing: 3

                                            // 暂停 / 恢复
                                            Item {
                                                Layout.preferredWidth: 18
                                                Layout.preferredHeight: 18

                                                Image {
                                                    id: imgPauseResume
                                                    anchors.fill: parent
                                                    source: recCtrls.state === "paused"
                                                        ? "qrc:/main_icons/assets/icons/record_resume.svg"
                                                        : "qrc:/main_icons/assets/icons/record_pause.svg"
                                                    fillMode: Image.PreserveAspectFit
                                                }
                                                MultiEffect {
                                                    anchors.fill: imgPauseResume
                                                    source: imgPauseResume
                                                    colorization: 1.0
                                                    colorizationColor: mousePauseResume.containsMouse ? "#FFFFFF" : "#8DB3D9"
                                                    brightness: mousePauseResume.containsMouse ? 0.9 : 0.7
                                                    enabled: true
                                                }
                                                MouseArea {
                                                    id: mousePauseResume
                                                    anchors.fill: parent
                                                    cursorShape: Qt.PointingHandCursor
                                                    hoverEnabled: true
                                                    onClicked: {
                                                        if (simulatorRoot.recordingState[bodyId] === "paused") {
                                                            sceneController.resumeRecording(bodyId);
                                                            simulatorRoot.recordingState[bodyId] = "recording";
                                                        } else {
                                                            sceneController.pauseRecording(bodyId);
                                                            simulatorRoot.recordingState[bodyId] = "paused";
                                                        }
                                                    }
                                                }
                                            }

                                            // 停止
                                            Item {
                                                Layout.preferredWidth: 18
                                                Layout.preferredHeight: 18

                                                Image {
                                                    id: imgStop
                                                    anchors.fill: parent
                                                    source: "qrc:/main_icons/assets/icons/record_stop.svg"
                                                    fillMode: Image.PreserveAspectFit
                                                }
                                                MultiEffect {
                                                    anchors.fill: imgStop
                                                    source: imgStop
                                                    colorization: 1.0
                                                    colorizationColor: mouseStop.containsMouse ? "#FF6B6B" : "#8DB3D9"
                                                    brightness: mouseStop.containsMouse ? 0.9 : 0.7
                                                    enabled: true
                                                }
                                                MouseArea {
                                                    id: mouseStop
                                                    anchors.fill: parent
                                                    cursorShape: Qt.PointingHandCursor
                                                    hoverEnabled: true
                                                    onClicked: {
                                                        sceneController.stopRecording(bodyId);
                                                        simulatorRoot.recordingState[bodyId] = "none";
                                                        simulatorRoot.trackingVersion++;
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    // ---- 保存按钮（非录制时显示） ----
                                    Item {
                                        id: recSave
                                        property string state: {
                                            simulatorRoot.trackingVersion;
                                            return simulatorRoot.recordingState[bodyId] || "none";
                                        }
                                        visible: state === "none"
                                        Layout.preferredWidth: 18
                                        Layout.preferredHeight: 18


                                        // Image {
                                        //     id: imgSave
                                        //     anchors.fill: parent
                                        //     source: "qrc:/main_icons/assets/icons/record_save.svg"
                                        //     fillMode: Image.PreserveAspectFit
                                        // }
                                        // MultiEffect {
                                        //     anchors.fill: imgSave
                                        //     source: imgSave
                                        //     colorization: 1.0
                                        //     colorizationColor: mouseSave.containsMouse ? "#FFFFFF" : "#8DB3D9"
                                        //     brightness: mouseSave.containsMouse ? 0.9 : 0.7
                                        //     enabled: true
                                        // }

                                        Text{
                                            id: imgSave
                                            anchors.fill: parent
                                            text: "💾"

                                        }
                                        MouseArea {
                                            id: mouseSave
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            hoverEnabled: true
                                            onClicked: {
                                                saveTrackDataFileDialog.bodyID = bodyId;
                                                saveTrackDataFileDialog.bodyName = bodyName;
                                                // const now = new Date();
                                                saveTrackDataFileDialog.open();
                                            }
                                        }
                                    }
                                }

                                // 位置（追踪时显示）
                                Text {
                                    property bool show: {
                                        simulatorRoot.trackingVersion;
                                        return simulatorRoot.isTracking(bodyId, "position");
                                    }
                                    visible: show
                                    text: "位置: " + simulatorRoot.formatVec(bodyPosition[2], bodyPosition[0], bodyPosition[1])
                                    color: "#9EC4EA"
                                    font.pixelSize: 10
                                }

                                // 速度（追踪时显示）
                                Text {
                                    property bool show: {
                                        simulatorRoot.trackingVersion;
                                        return simulatorRoot.isTracking(bodyId, "velocity");
                                    }
                                    visible: show
                                    text: "速度: " + simulatorRoot.formatVecWithMagnitude(bodyVelocity[2], bodyVelocity[0], bodyVelocity[1])
                                    color: "#9EC4EA"
                                    font.pixelSize: 10
                                }

                                // 角速度（追踪时显示）
                                Text {
                                    property bool show: {
                                        simulatorRoot.trackingVersion;
                                        return simulatorRoot.isTracking(bodyId, "angularVelocity");
                                    }
                                    visible: show
                                    text: "角速度: " + (typeof bodyAngularVelocity !== 'undefined'
                                        ? simulatorRoot.formatVecWithMagnitude(bodyAngularVelocity[2], bodyAngularVelocity[0], bodyAngularVelocity[1])
                                        : "N/A")
                                    color: "#9EC4EA"
                                    font.pixelSize: 10
                                }

                                // 动能（追踪时显示）
                                Text {
                                    property bool show: {
                                        simulatorRoot.trackingVersion;
                                        return simulatorRoot.isTracking(bodyId, "kineticEnergy");
                                    }
                                    visible: show
                                    text: "动能: " + Number(bodyKineticEnergy).toFixed(2) + " J"
                                    color: "#9EC4EA"
                                    font.pixelSize: 10
                                }
                            }
                        }
                    }

                    // ---- 空状态提示：无追踪数据时显示 ----
                    Rectangle {
                        width: parent.width
                        height: 100
                        radius: theme.radiusS
                        color: "#0D2748"
                        border.width: 1
                        border.color: "#2C5D88"

                        property bool show: {
                            simulatorRoot.trackingVersion;
                            return !simulatorRoot.hasAnyTrackingData();
                        }
                        visible: show

                        Text {
                            anchors.centerIn: parent
                            text: "点击 3D 物体 → \n 跟踪数据 → \n 勾选属性"
                            color: "#5A7D9E"
                            font.pixelSize: 13
                            font.italic: true
                        }
                    }
                }

                ScrollBar.vertical: dataStreamVScrollBar
            }

            Basic.ScrollBar {
                id: dataStreamVScrollBar
                width: dataStreamPanel.scrollBarThickness
                policy: ScrollBar.AsNeeded
                visible: size < 1.0
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.topMargin: 6
                anchors.bottomMargin: 6
                anchors.rightMargin: -(dataStreamPanel.scrollBarThickness + dataStreamPanel.scrollBarGap - 5)

                contentItem: Rectangle {
                    radius: theme.radiusS
                    implicitWidth: dataStreamPanel.scrollBarThickness
                    color: theme.accent
                    opacity: dataStreamVScrollBar.pressed ? 0.9 : 0.7
                }

                background: Rectangle {
                    radius: theme.radiusS
                    implicitWidth: dataStreamPanel.scrollBarThickness
                    color: "#0F2A4C"
                    border.width: 1
                    border.color: theme.border
                    opacity: 0.6
                }
            }
        }

        GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: "场景视图 / Dynamics Canvas"

            // 3D 视口（共享组件）：渲染与相机计算在组件内部，
            // 鼠标交互 / 拾取 / 右键菜单由本单元叠加在组件之上。
            SimulationViewport {
                id: canvas3d
                anchors.fill: parent

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.leftMargin: 10
                    anchors.topMargin: 8
                    radius: theme.radiusS
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

                    property var contextMenuPopup: null

                    onPressed: function (mouse) {
                        canvas3d.lastMousePoint = Qt.point(mouse.x, mouse.y);
                        canvas3d.pressPoint = Qt.point(mouse.x, mouse.y);
                        if (mouse.button === Qt.LeftButton) {
                            // 左键按下：留待 onReleased 判断是否为点击 pick
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
                    }

                    onReleased: function (mouse) {
                        if (mouse.button === Qt.LeftButton) {
                            // 判断是否为点击（位移 < 5px 视为点击，否则为拖拽）
                            const dx = mouse.x - canvas3d.pressPoint.x;
                            const dy = mouse.y - canvas3d.pressPoint.y;
                            const dist = Math.sqrt(dx * dx + dy * dy);

                            if (dist < 5) {
                                // 关闭已存在的菜单
                                if (mouseLayer.contextMenuPopup) {
                                    mouseLayer.contextMenuPopup.close();
                                    mouseLayer.contextMenuPopup.destroy();
                                    mouseLayer.contextMenuPopup = null;
                                }

                                // 尝试 pick 3D 物体
                                const pickResult = canvas3d.pickAt(mouse.x, mouse.y);
                                if (pickResult && pickResult.objectHit) {
                                    const objName = pickResult.objectHit.objectName;
                                    if (objName && objName.startsWith("body-")) {
                                        const bodyId = objName.substring(5);
                                        simulatorRoot.pickedBodyId = bodyId;
                                        canvas3d.selectedBodyId = bodyId;

                                        // 构建菜单项
                                        const items = simulatorRoot.buildContextMenu(bodyId);

                                        // 创建 EgretPopupMenu
                                        const popup = contextMenuFactory.createObject(canvas3d, {
                                            "menuItems": items,
                                            "x": mouse.x,
                                            "y": mouse.y
                                        });

                                        popup.menuClosed.connect(function () {
                                            if (mouseLayer.contextMenuPopup) {
                                                mouseLayer.contextMenuPopup.destroy();
                                                mouseLayer.contextMenuPopup = null;
                                            }
                                            simulatorRoot.contextMenuVisible = false;
                                        });

                                        mouseLayer.contextMenuPopup = popup;
                                        popup.open();
                                        simulatorRoot.contextMenuVisible = true;
                                    }
                                } else {
                                    // 点击空白：取消选中
                                    canvas3d.selectedBodyId = "";
                                    simulatorRoot.contextMenuVisible = false;
                                }
                            }
                        }
                    }

                    onWheel: function (wheel) {
                        const factor = wheel.angleDelta.y > 0 ? 0.88 : 1.12;
                        canvas3d.orbitDistance = canvas3d.clamp(canvas3d.orbitDistance * factor, 120, 8000);
                        canvas3d.updateCameraPose();
                    }
                }

                // ---- 上下文菜单工厂 ----
                Component {
                    id: contextMenuFactory
                    EgretPopupMenu {}
                }
            }
        }

        ColumnLayout {
            id: rightInfoColumn
            Layout.preferredWidth: simulatorRoot.infoPanelVisible ? 260 : 0
            Layout.minimumWidth: simulatorRoot.infoPanelVisible ? 260 : 0
            Layout.maximumWidth: simulatorRoot.infoPanelVisible ? 260 : 0
            Layout.fillHeight: true
            visible: simulatorRoot.infoPanelVisible
            opacity: simulatorRoot.infoPanelVisible ? 1 : 0
            enabled: simulatorRoot.infoPanelVisible

            spacing: 4


            GridLayout{
                Layout.fillWidth: true
                rows: 2
                columns: 2
                rowSpacing: 4
                columnSpacing: 4


                MetricCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 32
                    Layout.maximumHeight: 32
                    metricName: "步长"
                    metricValue: Number(sceneController.fixedStepMs).toFixed(2)
                    metricUnit: "ms"
                    metricColor: theme.accent
                    nameFontSize: 12
                    valueFontSize: 12
                    unitFontSize: 10
                    dotSize: 8
                    contentPadding: 10
                    contentSpacing: 6
                }

                MetricCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 32
                    Layout.maximumHeight: 32
                    metricName: "时间"
                    metricValue: Number(sceneController.simTime).toFixed(2)
                    metricUnit: "s"
                    metricColor: theme.accent
                    nameFontSize: 12
                    valueFontSize: 12
                    unitFontSize: 10
                    dotSize: 8
                    contentPadding: 10
                    contentSpacing: 6
                }

                MetricCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 32
                    Layout.maximumHeight: 32
                    metricName: "帧率"
                    metricValue: Number(sceneController.fps).toFixed(1)
                    metricUnit: "FPS"
                    metricColor: theme.success
                    nameFontSize: 12
                    valueFontSize: 12
                    unitFontSize: 10
                    dotSize: 8
                    contentPadding: 10
                    contentSpacing: 6
                }

                MetricCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 32
                    Layout.maximumHeight: 32
                    metricName: "刚体"
                    metricValue: sceneController.entityCount
                    metricUnit: "obj"
                    metricColor: theme.warning
                    nameFontSize: 12
                    valueFontSize: 12
                    unitFontSize: 10
                    dotSize: 8
                    contentPadding: 10
                    contentSpacing: 6
                }

            }

            GlassPanel {
                id: infoPanel
                Layout.preferredWidth: simulatorRoot.infoPanelVisible ? 260 : 0
                Layout.minimumWidth: simulatorRoot.infoPanelVisible ? 260 : 0
                Layout.maximumWidth: simulatorRoot.infoPanelVisible ? 260 : 0
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
                            radius: theme.radiusS
                            implicitWidth: 8
                            color: theme.accent
                            opacity: infoScrollBar.pressed ? 0.9 : 0.7
                        }

                        background: Rectangle {
                            radius: theme.radiusS
                            implicitWidth: 8
                            color: "#0F2A4C"
                            border.width: 1
                            border.color: theme.border
                            opacity: 0.6
                        }
                    }

                    ColumnLayout {
                        width: Math.max(0, infoScrollView.availableWidth - infoScrollView.rightPadding)
                        spacing: 6

                        Repeater {
                            model: [
                                "广相位碰撞模型：" + sceneController.getBroadStrategyName(),
                                "积分器: " + sceneController.getIntegratorName(),
                                "碰撞模型: CCD",
                                "约束求解: Sequential Impulse",
                                "误差阈值: 1e-6"]

                            delegate: Rectangle {
                                id: repeatItem
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                property bool hovered: false
                                radius: theme.radiusS
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
                Layout.preferredWidth: simulatorRoot.infoPanelVisible ? 260 : 0
                Layout.minimumWidth: simulatorRoot.infoPanelVisible ? 260 : 0
                Layout.maximumWidth: simulatorRoot.infoPanelVisible ? 260 : 0
                visible: simulatorRoot.infoPanelVisible
                opacity: simulatorRoot.infoPanelVisible ? 1 : 0
                enabled: simulatorRoot.infoPanelVisible
                Layout.preferredHeight: 160
                title: "仿真控制"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    // ---- 时间缩放选择器 ----
                    Text {
                        text: "模拟速度"
                        color: "#AAC4D8"
                        font.pixelSize: 11
                        font.bold: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Repeater {
                            model: [
                                { label: "0.1×", scale: 0.1 },
                                { label: "1×",   scale: 1.0 },
                                { label: "3×",   scale: 3.0 },
                                { label: "10×",  scale: 10.0 }
                            ]

                            Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.preferredHeight: 24
                                radius: theme.radiusS
                                color: {
                                    const active = Math.abs(sceneController.timeScale - modelData.scale) < 0.01;
                                    if (active)
                                        return "#1A5E9E";
                                    return speedBtnMouse.containsMouse ? "#1E3D5C" : "#1A2D44";
                                }
                                border.width: 1
                                border.color: {
                                    const active = Math.abs(sceneController.timeScale - modelData.scale) < 0.01;
                                    return active ? "#58B8FF" : "#3A5A7A";
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: "#EAF5FF"
                                    font.pixelSize: 12
                                    font.bold: Math.abs(sceneController.timeScale - modelData.scale) < 0.01
                                }

                                MouseArea {
                                    id: speedBtnMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        sceneController.setTimeScale(modelData.scale);
                                    }
                                }
                            }
                        }
                    }

                    EgretSeparator {
                        Layout.fillWidth: true
                    }


                    Rectangle {
                        id: computeBtnRect
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        property bool hovered: false
                        radius: theme.radiusS
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

            // ============ 离线仿真（EPS 导出） ============
            GlassPanel {
                id: offlineSimulationPanel
                Layout.preferredWidth: simulatorRoot.infoPanelVisible ? 260 : 0
                Layout.minimumWidth: simulatorRoot.infoPanelVisible ? 260 : 0
                Layout.maximumWidth: simulatorRoot.infoPanelVisible ? 260 : 0
                visible: simulatorRoot.infoPanelVisible
                opacity: simulatorRoot.infoPanelVisible ? 1 : 0
                enabled: simulatorRoot.infoPanelVisible
                Layout.preferredHeight: 340
                title: "离线仿真"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6

                    // ---- 求解步长 ----
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: "求解步长"
                            color: "#AAC4D8"
                            font.pixelSize: 11
                            font.bold: true
                        }

                        FloatField {
                            Layout.fillWidth: true
                            from: 0.01
                            to: 10
                            decimals: 2
                            suffix: "ms"
                            enabled: !simulationExporter.running
                            value: simulatorRoot.simulationSolverStepMs

                            onValueChanged: {
                                simulatorRoot.simulationSolverStepMs = value;
                            }
                        }
                    }

                    // ---- 采样间隔（五档） ----
                    Text {
                        text: "采样间隔 (ms)"
                        color: "#AAC4D8"
                        font.pixelSize: 11
                        font.bold: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Repeater {
                            model: [1, 2, 5, 10, 20]

                            Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.preferredHeight: 22
                                radius: theme.radiusS

                                readonly property bool active:
                                    simulatorRoot.simulationSampleMs === modelData

                                color: active
                                    ? "#1A5E9E"
                                    : (sampleBtnMouse.containsMouse ? "#1E3D5C" : "#1A2D44")
                                border.width: 1
                                border.color: active ? "#58B8FF" : "#3A5A7A"

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: "#EAF5FF"
                                    font.pixelSize: 11
                                    font.bold: parent.active
                                }

                                MouseArea {
                                    id: sampleBtnMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    enabled: !simulationExporter.running
                                    onClicked: {
                                        simulatorRoot.simulationSampleMs = modelData;
                                    }
                                }
                            }
                        }
                    }

                    // ---- 仿真时长 ----
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: "仿真时长"
                            color: "#AAC4D8"
                            font.pixelSize: 11
                            font.bold: true
                        }

                        FloatField {
                            Layout.fillWidth: true
                            from: 0.1
                            to: 3600
                            decimals: 1
                            suffix: "s"
                            enabled: !simulationExporter.running
                            value: simulatorRoot.simulationDurationSeconds

                            onValueChanged: {
                                simulatorRoot.simulationDurationSeconds = value;
                            }
                        }
                    }

                    EgretPushButton {
                        text: simulationExporter.running ? "离线求解中…" : "开始离线仿真"
                        enabled: !simulationExporter.running
                        onClicked: {
                            simulationExporter.start(
                                sceneController.currentSceneJson(),
                                simulatorRoot.simulationDurationSeconds,
                                simulatorRoot.simulationSolverStepMs,
                                simulatorRoot.simulationSampleMs);
                        }
                    }

                    // ---- 求解进度 ----
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 16

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            height: 6
                            radius: 3
                            color: "#12365D"
                            border.width: 1
                            border.color: "#2E5D89"

                            Rectangle {
                                width: Math.max(0, Math.min(parent.width * simulationExporter.progress, parent.width))
                                height: parent.height
                                radius: parent.radius
                                color: theme.accent
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: (100 * Math.max(0, Math.min(simulationExporter.progress, 1))).toFixed(0)
                            + "%   ·   "
                            + (simulationExporter.statusMessage.length > 0
                                ? simulationExporter.statusMessage
                                : "尚未执行")
                        color: "#9EC4EA"
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: simulationExporter.errorMessage
                        color: theme.danger
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                        visible: simulationExporter.errorMessage.length > 0
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        EgretPushButton {
                            text: "保存…"
                            enabled: simulationExporter.frameCount > 0 && !simulationExporter.running
                            onClicked: {
                                saveSimulationFileDialog.open();
                            }
                        }

                        EgretPushButton {
                            text: "删除缓存"
                            enabled: !simulationExporter.running
                            onClicked: {
                                simulationExporter.removeCacheFile();
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "缓存：" + simulationExporter.defaultFilePath()
                        color: "#5F84AD"
                        font.pixelSize: 9
                        elide: Text.ElideMiddle
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }
        }
    }

}
