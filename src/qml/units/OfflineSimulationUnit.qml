/**
 * @file        OfflineSimulationUnit.qml
 * @brief       离线仿真工作区视图文件
 * @details     把「离线求解（写 EPSX 文件）」从实时仿真器中独立出来，成为单独的工作区：
 *              左侧复用共享 3D 视口显示待求解的场景，右侧提供场景选择、求解参数、
 *              开始 / 取消、进度、结果状态与文件保存 / 删除。
 *
 *              求解在后台线程执行，界面不阻塞；取消后已写入的帧仍会正常收尾，可直接播放。
 *
 * @author      jehor
 * @date        2026-10-03
 * @version     1.0.0
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 * @ingroup     View
 */

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs

import "qrc:/components/components"
import "qrc:/components/components/basic"
import "qrc:/components/components/viewport"

ColumnLayout {
    id: offlineRoot

    /** 主题对象（由 MainWindow 注入）。 */
    property var theme

    /** 离线求解：求解推进步长（毫秒）。 */
    property real solverStepMs: 0.5

    /** 离线求解：采样间隔（毫秒），只能取 1 / 2 / 5 / 10 / 20。 */
    property int sampleMs: 10

    /** 离线求解：仿真总时长（秒）。 */
    property real durationSeconds: 60

    spacing: 4

    /** 把 file:/// URL 转换为本地路径。 */
    function localPathFromUrl(fileUrl) {
        return fileUrl.toString().replace(/^file:\/\/\//, "");
    }

    FileDialog {
        id: openSceneFileDialog
        title: "打开场景文件作为求解初始状态"
        fileMode: FileDialog.OpenFile
        currentFolder: documentsFolder
        nameFilters: [
            "序列化场景 (*.json)",
            "所有文件 (*)"
        ]

        onAccepted: {
            sceneController.loadSceneFronJsonFile(offlineRoot.localPathFromUrl(selectedFile));
        }
    }

    FileDialog {
        id: saveSimulationFileDialog
        title: "保存离线仿真文件"
        fileMode: FileDialog.SaveFile
        currentFolder: documentsFolder
        defaultSuffix: "epsx"
        nameFilters: [
            "Egret 仿真文件 (*.epsx)",
            "所有文件 (*)"
        ]

        onAccepted: {
            simulationExporter.saveAs(offlineRoot.localPathFromUrl(selectedFile));
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 6

        GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: "待求解场景 / Offline Simulation Canvas"

            SimulationViewport {
                id: offlineViewport
                anchors.fill: parent

                // ---- 操作提示 ----
                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.leftMargin: 10
                    anchors.topMargin: 8
                    radius: theme.radiusS
                    color: "#0D274899"
                    border.width: 1
                    border.color: "#2B5E89"
                    width: 320
                    height: 66

                    Text {
                        anchors.fill: parent
                        anchors.margins: 10
                        color: "#D4EAFB"
                        font.pixelSize: 12
                        text: "坐标系: World z-up, 右手系\n右键拖拽旋转视角, 滚轮缩放\n求解在后台线程执行，界面不会卡住"
                    }
                }

                // ---- 只读浏览交互：右键轨道旋转 + 滚轮缩放 ----
                MouseArea {
                    id: offlineMouseLayer
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.RightButton

                    property real lastX: 0
                    property real lastY: 0

                    onPressed: function (mouse) {
                        offlineMouseLayer.lastX = mouse.x;
                        offlineMouseLayer.lastY = mouse.y;
                    }

                    onPositionChanged: function (mouse) {
                        if ((mouse.buttons & Qt.RightButton) === 0) {
                            return;
                        }
                        const dx = mouse.x - offlineMouseLayer.lastX;
                        const dy = mouse.y - offlineMouseLayer.lastY;
                        offlineMouseLayer.lastX = mouse.x;
                        offlineMouseLayer.lastY = mouse.y;

                        offlineViewport.orbitYaw -= dx * 0.22;
                        offlineViewport.orbitPitch =
                            offlineViewport.clamp(offlineViewport.orbitPitch - dy * 0.22, -90, 90);
                        offlineViewport.updateCameraPose();
                    }

                    onWheel: function (wheel) {
                        const factor = wheel.angleDelta.y > 0 ? 0.88 : 1.12;
                        offlineViewport.orbitDistance =
                            offlineViewport.clamp(offlineViewport.orbitDistance * factor, 120, 8000);
                        offlineViewport.updateCameraPose();
                    }
                }
            }
        }

        ColumnLayout {
            id: offlineControlColumn
            Layout.preferredWidth: 280
            Layout.minimumWidth: 280
            Layout.maximumWidth: 280
            Layout.fillHeight: true
            spacing: 4

            GridLayout {
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
                    metricName: "实体"
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

                MetricCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 32
                    Layout.maximumHeight: 32
                    metricName: "帧数"
                    metricValue: simulationExporter.frameCount
                    metricUnit: "f"
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
                    metricName: "文件"
                    metricValue: Number(simulationExporter.fileSizeMb).toFixed(2)
                    metricUnit: "MB"
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
                    metricName: "耗时"
                    metricValue: Number(simulationExporter.wallClockSeconds).toFixed(2)
                    metricUnit: "s"
                    metricColor: theme.accentSoft
                    nameFontSize: 12
                    valueFontSize: 12
                    unitFontSize: 10
                    dotSize: 8
                    contentPadding: 10
                    contentSpacing: 6
                }
            }

            // ============ 初始状态 ============
            GlassPanel {
                Layout.fillWidth: true
                Layout.preferredHeight: 150
                title: "求解初始状态"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    EgretPushButton {
                        text: "打开场景文件（.json）…"
                        onClicked: {
                            openSceneFileDialog.open();
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "初始状态取自当前加载的场景；未加载时无法开始求解。\n"
                            + "也可用菜单「文件 → 打开」加载场景。"
                        color: "#9EC4EA"
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "当前场景实体数：" + sceneController.entityCount
                        color: "#BDD8F3"
                        font.pixelSize: 11
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }

            // ============ 离线求解 ============
            GlassPanel {
                Layout.fillWidth: true
                Layout.preferredHeight: 300
                title: "离线求解"

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
                            value: offlineRoot.solverStepMs

                            onValueChanged: {
                                offlineRoot.solverStepMs = value;
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

                                readonly property bool active: offlineRoot.sampleMs === modelData

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
                                        offlineRoot.sampleMs = modelData;
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
                            value: offlineRoot.durationSeconds

                            onValueChanged: {
                                offlineRoot.durationSeconds = value;
                            }
                        }
                    }

                    // ---- 开始 / 取消 ----
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        EgretPushButton {
                            text: simulationExporter.running ? "求解中…" : "开始离线求解"
                            enabled: !simulationExporter.running
                            onClicked: {
                                simulationExporter.start(
                                    sceneController.currentSceneJson(),
                                    offlineRoot.durationSeconds,
                                    offlineRoot.solverStepMs,
                                    offlineRoot.sampleMs);
                            }
                        }

                        EgretPushButton {
                            text: simulationExporter.cancelRequested ? "正在取消…" : "取消"
                            enabled: simulationExporter.running && !simulationExporter.cancelRequested
                            onClicked: {
                                simulationExporter.cancel();
                            }
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
                                color: simulationExporter.cancelRequested ? theme.warning : theme.accent
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
