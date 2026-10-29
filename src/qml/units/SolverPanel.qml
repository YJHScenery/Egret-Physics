/**
 * @file SolverPanel.qml
 * @brief 编辑器中的离线仿真求解面板。
 * @details 使用当前编辑场景作为求解初始状态，提供参数设置、进度和结果操作。
 */
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import EditorManager 1.0
import "qrc:/components/components"
import "qrc:/components/components/basic"

GlassPanel {
    id: root
    title: "仿真求解 / Simulation"
    property var theme
    property real solverStepMs: 0.5
    property int sampleMs: 10
    property real durationSeconds: 60

    FileDialog {
        id: saveSimulationFileDialog
        title: "保存离线仿真文件"
        fileMode: FileDialog.SaveFile
        currentFolder: documentsFolder
        defaultSuffix: "epsx"
        nameFilters: ["Egret 仿真文件 (*.epsx)"]
        onAccepted: simulationExporter.saveAs(decodeURIComponent(selectedFile.toString().replace(/^file:\/\/\//, "")))
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ColumnLayout {
            width: scroll.availableWidth
            spacing: 10
            Text {
                Layout.fillWidth: true
                text: "求解当前编辑场景，包括尚未保存的修改。"
                color: theme.textSecondary; font.pixelSize: 12; wrapMode: Text.Wrap
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#2C4E74" }
            Text { text: "求解设置"; color: theme.textPrimary; font.bold: true; font.pixelSize: 13 }
            RowLayout {
                Layout.fillWidth: true
                Text { text: "求解步长"; color: theme.textSecondary; font.pixelSize: 12 }
                FloatField {
                    Layout.fillWidth: true; from: 0.01; to: 10; decimals: 2; suffix: "ms"
                    readOnly: simulationExporter.running; value: root.solverStepMs
                    onValueChanged: root.solverStepMs = value
                }
            }
            Text { text: "采样间隔（毫秒）"; color: theme.textSecondary; font.pixelSize: 12 }
            RowLayout {
                Layout.fillWidth: true; spacing: 4
                Repeater {
                    model: [1, 2, 5, 10, 20]
                    delegate: Rectangle {
                        required property int modelData
                        Layout.fillWidth: true; Layout.preferredHeight: 28; radius: 5
                        color: root.sampleMs === modelData ? "#1A5E9E" : sampleMouse.containsMouse ? "#1E3D5C" : "#1A2D44"
                        border.color: root.sampleMs === modelData ? "#58B8FF" : "#3A5A7A"
                        Text { anchors.centerIn: parent; text: parent.modelData; color: theme.textPrimary; font.pixelSize: 11 }
                        MouseArea { id: sampleMouse; anchors.fill: parent; hoverEnabled: true; enabled: !simulationExporter.running; onClicked: root.sampleMs = modelData }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Text { text: "仿真时长"; color: theme.textSecondary; font.pixelSize: 12 }
                FloatField {
                    Layout.fillWidth: true; from: 0.1; to: 3600; decimals: 1; suffix: "s"
                    readOnly: simulationExporter.running; value: root.durationSeconds
                    onValueChanged: root.durationSeconds = value
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 6
                EgretPushButton {
                    text: simulationExporter.running ? "求解中…" : "开始求解"
                    enabled: !simulationExporter.running && EditorManager.count > 0
                    onClicked: {
                        forceActiveFocus();
                        simulationExporter.start(EditorManager.toJsonString(false), root.durationSeconds, root.solverStepMs, root.sampleMs);
                    }
                }
                EgretPushButton {
                    text: simulationExporter.cancelRequested ? "正在取消…" : "取消"
                    enabled: simulationExporter.running && !simulationExporter.cancelRequested
                    onClicked: simulationExporter.cancel()
                }
            }
            ProgressBar {
                id: solveProgress
                Layout.fillWidth: true; Layout.preferredHeight: 8; value: simulationExporter.progress
                background: Rectangle { radius: 4; color: "#12365D"; border.color: "#2E5D89" }
                contentItem: Item {
                    Rectangle { width: parent.width * solveProgress.visualPosition; height: parent.height; radius: 4
                        color: simulationExporter.cancelRequested ? theme.warning : theme.accent }
                }
            }
            Text {
                Layout.fillWidth: true
                text: Math.round(simulationExporter.progress * 100) + "% · " + (simulationExporter.statusMessage || "尚未执行")
                color: theme.textSecondary; font.pixelSize: 11; wrapMode: Text.Wrap
            }
            Text { Layout.fillWidth: true; text: simulationExporter.errorMessage; color: theme.danger; font.pixelSize: 11; wrapMode: Text.Wrap; visible: text.length > 0 }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#2C4E74" }
            Text { text: "求解结果"; color: theme.textPrimary; font.bold: true; font.pixelSize: 13 }
            Text {
                Layout.fillWidth: true
                text: "帧数：" + simulationExporter.frameCount + "    文件：" + Number(simulationExporter.fileSizeMb).toFixed(2) + " MB\n耗时：" + Number(simulationExporter.wallClockSeconds).toFixed(2) + " 秒"
                color: theme.textSecondary; font.pixelSize: 11; lineHeight: 1.5
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 6
                EgretPushButton { text: "保存结果…"; enabled: simulationExporter.frameCount > 0 && !simulationExporter.running; onClicked: saveSimulationFileDialog.open() }
                EgretPushButton { text: "删除缓存"; enabled: !simulationExporter.running; onClicked: simulationExporter.removeCacheFile() }
            }
            Text {
                Layout.fillWidth: true; text: "缓存：" + simulationExporter.defaultFilePath()
                color: theme.textDim; font.pixelSize: 10; elide: Text.ElideMiddle
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#2C4E74" }
            Text { text: "求解概览"; color: theme.textPrimary; font.bold: true; font.pixelSize: 13 }
            Text {
                Layout.fillWidth: true
                text: "积分器：" + sceneController.getIntegratorName() + "\n广相位：" + sceneController.getBroadStrategyName() + "\n碰撞：CCD · 约束：Sequential Impulse\n采样：抽帧 · 精度：float64"
                color: theme.textSecondary; font.pixelSize: 11; lineHeight: 1.6; wrapMode: Text.Wrap
            }
            Item { Layout.preferredHeight: 8 }
        }
    }
}
