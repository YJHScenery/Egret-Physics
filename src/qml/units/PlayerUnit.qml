/**
 * @file        PlayerUnit.qml
 * @brief       离线仿真播放器单元视图文件
 * @details     定义离线仿真（.egps）播放工作区：左侧复用共享 3D 视口显示播放画面，
 *              右侧提供文件选择、播放 / 暂停、进度拖动、时间输入与倍速切换。
 *
 *              播放数据全部由 SceneManagerViewModel 的播放后端提供：
 *              打开文件即进入播放模式（实时求解器停止），逐帧套用记录状态并刷新模型。
 *
 * @author      jehor
 * @date        2026-10-02
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
    id: playerRoot

    /** 主题对象（由 MainWindow 注入）。 */
    property var theme

    spacing: 4

    /** 将 file:/// URL 转换为本地路径。 */
    function localPathFromUrl(fileUrl) {
        return fileUrl.toString().replace(/^file:\/\/\//, "");
    }

    /** 当前播放进度比例（0 ~ 1）。 */
    readonly property real playbackRatio: sceneController.simulationDurationSeconds > 0
        ? Math.max(0, Math.min(sceneController.playbackTimeSeconds / sceneController.simulationDurationSeconds, 1))
        : 0

    /** 把播放进度格式化显示。 */
    function formatSeconds(seconds) {
        const value = Math.max(0, Number(seconds));
        const minutes = Math.floor(value / 60);
        const rest = value - minutes * 60;
        return (minutes > 0 ? minutes.toFixed(0) + ":" : "") + (rest < 10 && minutes > 0 ? "0" : "") + rest.toFixed(2);
    }

    FileDialog {
        id: openSimulationFileDialog
        title: "打开离线仿真文件"
        fileMode: FileDialog.OpenFile
        currentFolder: documentsFolder
        nameFilters: [
            "Egret 仿真文件 (*.egps)",
            "所有文件 (*)"
        ]

        onAccepted: {
            sceneController.openSimulationFile(playerRoot.localPathFromUrl(selectedFile));
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 6

        GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: "回放视图 / Playback Canvas"

            // 3D 视口（共享组件）：渲染与相机计算在组件内部，
            // 播放模式下只做只读浏览（轨道旋转 + 缩放）。
            SimulationViewport {
                id: playbackViewport
                anchors.fill: parent

                // ---- 空状态提示：未加载仿真文件时显示 ----
                Rectangle {
                    anchors.centerIn: parent
                    width: 320
                    height: 96
                    radius: theme.radiusS
                    color: "#0D2748"
                    border.width: 1
                    border.color: "#2C5D88"
                    visible: !sceneController.simulationLoaded

                    Text {
                        anchors.centerIn: parent
                        text: "尚未加载离线仿真文件\n点击右侧「打开仿真文件」开始回放"
                        color: "#5A7D9E"
                        font.pixelSize: 13
                        font.italic: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

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
                    width: 300
                    height: 48
                    visible: sceneController.simulationLoaded

                    Text {
                        anchors.fill: parent
                        anchors.margins: 10
                        color: "#D4EAFB"
                        font.pixelSize: 12
                        text: "坐标系: World z-up, 右手系\n右键拖拽旋转视角, 滚轮缩放"
                    }
                }

                // ---- 只读浏览交互：右键轨道旋转 + 滚轮缩放 ----
                MouseArea {
                    id: playbackMouseLayer
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.RightButton

                    property real lastX: 0
                    property real lastY: 0

                    onPressed: function (mouse) {
                        playbackMouseLayer.lastX = mouse.x;
                        playbackMouseLayer.lastY = mouse.y;
                    }

                    onPositionChanged: function (mouse) {
                        if ((mouse.buttons & Qt.RightButton) === 0) {
                            return;
                        }
                        const dx = mouse.x - playbackMouseLayer.lastX;
                        const dy = mouse.y - playbackMouseLayer.lastY;
                        playbackMouseLayer.lastX = mouse.x;
                        playbackMouseLayer.lastY = mouse.y;

                        playbackViewport.orbitYaw -= dx * 0.22;
                        playbackViewport.orbitPitch =
                            playbackViewport.clamp(playbackViewport.orbitPitch - dy * 0.22, -90, 90);
                        playbackViewport.updateCameraPose();
                    }

                    onWheel: function (wheel) {
                        const factor = wheel.angleDelta.y > 0 ? 0.88 : 1.12;
                        playbackViewport.orbitDistance =
                            playbackViewport.clamp(playbackViewport.orbitDistance * factor, 120, 8000);
                        playbackViewport.updateCameraPose();
                    }
                }
            }
        }

        ColumnLayout {
            id: playerControlColumn
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
                    metricName: "帧数"
                    metricValue: sceneController.simulationFrameCount
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
                    metricName: "时长"
                    metricValue: Number(sceneController.simulationDurationSeconds).toFixed(2)
                    metricUnit: "s"
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
                    metricName: "采样"
                    metricValue: Number(sceneController.simulationSampleIntervalMs).toFixed(0)
                    metricUnit: "ms"
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
                    metricName: "当前帧"
                    metricValue: sceneController.playbackFrame
                    metricUnit: "f"
                    metricColor: theme.accentSoft
                    nameFontSize: 12
                    valueFontSize: 12
                    unitFontSize: 10
                    dotSize: 8
                    contentPadding: 10
                    contentSpacing: 6
                }
            }

            // ============ 仿真文件 ============
            GlassPanel {
                Layout.fillWidth: true
                Layout.preferredHeight: 210
                title: "仿真文件"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    EgretPushButton {
                        text: "打开仿真文件…"
                        onClicked: {
                            openSimulationFileDialog.open();
                        }
                    }

                    EgretPushButton {
                        text: "打开默认缓存文件"
                        onClicked: {
                            sceneController.openDefaultSimulationFile();
                        }
                    }

                    EgretPushButton {
                        text: "关闭当前文件"
                        enabled: sceneController.simulationLoaded
                        onClicked: {
                            sceneController.closeSimulationFile();
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: sceneController.simulationLoaded
                            ? sceneController.simulationFile
                            : "未加载文件"
                        color: "#9EC4EA"
                        font.pixelSize: 10
                        elide: Text.ElideMiddle
                    }

                    Text {
                        Layout.fillWidth: true
                        text: sceneController.simulationError
                        color: theme.danger
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                        visible: sceneController.simulationError.length > 0
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }

            // ============ 播放控制 ============
            GlassPanel {
                Layout.fillWidth: true
                Layout.preferredHeight: 300
                title: "播放控制"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        EgretPushButton {
                            text: "⏮ 上一帧"
                            enabled: sceneController.simulationLoaded
                            onClicked: {
                                sceneController.stepPlaybackFrame(-1);
                            }
                        }

                        EgretPushButton {
                            text: sceneController.playbackPlaying ? "⏸ 暂停" : "▶ 播放"
                            enabled: sceneController.simulationLoaded
                            onClicked: {
                                sceneController.toggleSimulationPlayback();
                            }
                        }

                        EgretPushButton {
                            text: "下一帧 ⏭"
                            enabled: sceneController.simulationLoaded
                            onClicked: {
                                sceneController.stepPlaybackFrame(1);
                            }
                        }
                    }

                    // ---- 进度条（可拖动跳转） ----
                    Item {
                        id: progressBarItem
                        Layout.fillWidth: true
                        Layout.preferredHeight: 22

                        Rectangle {
                            id: progressTrack
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            height: 6
                            radius: 3
                            color: "#12365D"
                            border.width: 1
                            border.color: "#2E5D89"

                            Rectangle {
                                width: Math.max(0, progressTrack.width * playerRoot.playbackRatio)
                                height: parent.height
                                radius: parent.radius
                                color: theme.accent
                            }
                        }

                        Rectangle {
                            id: progressHandle
                            x: Math.max(0, Math.min(progressTrack.width, progressTrack.width * playerRoot.playbackRatio))
                                - width * 0.5
                            anchors.verticalCenter: parent.verticalCenter
                            width: 12
                            height: 12
                            radius: 6
                            color: "#EAF5FF"
                            border.width: 1
                            border.color: theme.accent
                        }

                        MouseArea {
                            anchors.fill: parent
                            enabled: sceneController.simulationLoaded
                            cursorShape: Qt.PointingHandCursor

                            function seekTo(x) {
                                const ratio = Math.max(0, Math.min(x / Math.max(1, progressTrack.width), 1));
                                sceneController.setPlaybackTime(ratio * sceneController.simulationDurationSeconds);
                            }

                            onPressed: function (mouse) {
                                seekTo(mouse.x);
                            }

                            onPositionChanged: function (mouse) {
                                if (pressed) {
                                    seekTo(mouse.x);
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: "时间"
                            color: "#AAC4D8"
                            font.pixelSize: 11
                            font.bold: true
                        }

                        // 只读显示：绑定的属性只被外部写入，用户无法编辑，
                        // 因此不会触发 FloatField 内部的 setValue 而破坏绑定。
                        FloatField {
                            id: playbackTimeDisplay
                            Layout.fillWidth: true
                            from: 0
                            to: Math.max(0, sceneController.simulationDurationSeconds)
                            decimals: 2
                            suffix: "s"
                            readOnly: true
                            value: sceneController.playbackTimeSeconds
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: "跳转"
                            color: "#AAC4D8"
                            font.pixelSize: 11
                            font.bold: true
                        }

                        // 可编辑输入：自身持有数值，用户编辑（回车确认）后跳转播放位置。
                        FloatField {
                            id: playbackJumpField
                            Layout.fillWidth: true
                            from: 0
                            to: Math.max(0, sceneController.simulationDurationSeconds)
                            decimals: 2
                            suffix: "s"
                            enabled: sceneController.simulationLoaded
                            onValueChanged: {
                                if (sceneController.simulationLoaded) {
                                    sceneController.setPlaybackTime(value);
                                }
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: playerRoot.formatSeconds(sceneController.playbackTimeSeconds)
                            + " / " + playerRoot.formatSeconds(sceneController.simulationDurationSeconds)
                            + "   ·   帧 " + sceneController.playbackFrame
                            + " / " + Math.max(0, sceneController.simulationFrameCount - 1)
                        color: "#BDD8F3"
                        font.pixelSize: 11
                    }

                    EgretSeparator {
                        Layout.fillWidth: true
                    }

                    Text {
                        text: "播放倍速"
                        color: "#AAC4D8"
                        font.pixelSize: 11
                        font.bold: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Repeater {
                            model: [
                                { label: "0.5×", rate: 0.5 },
                                { label: "1×",   rate: 1.0 },
                                { label: "2×",   rate: 2.0 },
                                { label: "4×",   rate: 4.0 }
                            ]

                            Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.preferredHeight: 24
                                radius: theme.radiusS

                                readonly property bool active:
                                    Math.abs(sceneController.playbackRate - modelData.rate) < 0.01

                                color: active
                                    ? "#1A5E9E"
                                    : (rateBtnMouse.containsMouse ? "#1E3D5C" : "#1A2D44")
                                border.width: 1
                                border.color: active ? "#58B8FF" : "#3A5A7A"

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: "#EAF5FF"
                                    font.pixelSize: 12
                                    font.bold: parent.active
                                }

                                MouseArea {
                                    id: rateBtnMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        sceneController.playbackRate = modelData.rate;
                                    }
                                }
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
}
