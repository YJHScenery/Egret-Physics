/**
 * @file        PlayerUnit.qml
 * @brief       离线仿真播放器单元视图文件
 * @details     定义离线仿真（.epsx）播放工作区：左侧复用共享 3D 视口显示播放画面，
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

    /** 请求主窗口打开 epsx（与数据分析工作区共享同一份文件）。 */
    signal epsxOpenRequested(string filePath)

    spacing: 4

    /** 将 file:/// URL 转换为本地路径。 */
    function localPathFromUrl(fileUrl) {
        return fileUrl.toString().replace(/^file:\/\/\//, "");
    }

    /** 当前播放进度比例（0 ~ 1）。 */
    readonly property real playbackRatio: playerSceneController.simulationDurationSeconds > 0
        ? Math.max(0, Math.min(playerSceneController.playbackTimeSeconds / playerSceneController.simulationDurationSeconds, 1))
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
            "Egret 仿真文件 (*.epsx)",
            "所有文件 (*)"
        ]

        onAccepted: {
            // 交给主窗口统一打开：同一份 epsx 会同时同步给数据分析工作区
            playerRoot.epsxOpenRequested(playerRoot.localPathFromUrl(selectedFile));
        }
    }

    /** 打开视频导出参数窗口，供菜单与播放器按钮共用。 */
    function showVideoExport() {
        if (playerSceneController.simulationLoaded && !videoExporter.running)
            videoExportDialog.open();
    }

    FileDialog {
        id: videoFileDialog
        title: "保存导出的视频"
        fileMode: FileDialog.SaveFile
        currentFolder: documentsFolder
        defaultSuffix: "mp4"
        nameFilters: ["MP4 视频 (*.mp4)"]
        onAccepted: {
            if (!playerSceneController.simulationLoaded) {
                videoResult.text = "播放器已关闭仿真文件，请重新打开后导出。";
                videoResult.open();
                return;
            }
            if (!videoExporter.start(playerSceneController.simulationFile, selectedFile,
                                     videoWidth.value, videoHeight.value, videoFps.value,
                                     playbackViewport.cameraState(), playbackViewport.coordinateSystem.visible)) {
                videoResult.text = videoExporter.message;
                videoResult.open();
            }
        }
    }

    /** 视频导出参数使用的主题数字输入框。 */
    component ExportSpinBox: SpinBox {
        id: control
        editable: true
        hoverEnabled: true
        implicitWidth: 148
        implicitHeight: 38
        leftPadding: 36
        rightPadding: 36
        font.pixelSize: 13
        contentItem: TextInput {
            text: control.textFromValue(control.value, control.locale)
            font: control.font
            color: theme.textPrimary
            selectionColor: theme.accentSoft
            selectedTextColor: "white"
            horizontalAlignment: Qt.AlignHCenter
            verticalAlignment: Qt.AlignVCenter
            readOnly: !control.editable
            validator: control.validator
            inputMethodHints: Qt.ImhDigitsOnly
        }
        background: Rectangle {
            radius: 7
            color: theme.bg1
            border.width: 1
            border.color: control.activeFocus ? theme.accent : theme.border
        }
        up.indicator: Rectangle {
            x: control.width - width - 1; y: 1
            width: 34; height: control.height - 2
            radius: 6
            color: control.value >= control.to ? "#102543" : control.up.pressed ? "#168CC8" : control.up.hovered ? "#286BA3" : "#16365F"
            Behavior on color { ColorAnimation { duration: 100 } }
            Text {
                anchors.centerIn: parent
                text: "+"; color: control.value < control.to ? theme.textPrimary : theme.textDim
                font.pixelSize: 18
            }
        }
        down.indicator: Rectangle {
            x: 1; y: 1
            width: 34; height: control.height - 2
            radius: 6
            color: control.value <= control.from ? "#102543" : control.down.pressed ? "#168CC8" : control.down.hovered ? "#286BA3" : "#16365F"
            Behavior on color { ColorAnimation { duration: 100 } }
            Text {
                anchors.centerIn: parent
                text: "−"; color: control.value > control.from ? theme.textPrimary : theme.textDim
                font.pixelSize: 18
            }
        }
    }

    Dialog {
        id: videoExportDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: Math.min(540, parent.width - 32)
        padding: 24
        closePolicy: Popup.CloseOnEscape
        background: Rectangle { color: "#0C2041"; radius: 12; border.color: theme.border }
        contentItem: ColumnLayout {
            spacing: 16
            RowLayout {
                Layout.fillWidth: true
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 3
                    Text { text: "导出视频"; color: theme.textPrimary; font.pixelSize: 20; font.bold: true }
                    Text { text: "MP4 · H.264"; color: theme.accent; font.pixelSize: 12 }
                }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#284D75" }
            Text {
                Layout.fillWidth: true
                text: "将完整仿真导出为视频，并沿用播放器当前的相机和网格设置。"
                color: theme.textSecondary; font.pixelSize: 13; wrapMode: Text.Wrap
            }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 7
                Text { text: "输出帧率"; color: theme.textPrimary; font.pixelSize: 13; font.bold: true }
                RowLayout {
                    ExportSpinBox { id: videoFps; from: 0; to: 240; value: 30 }
                    Text { text: "帧 / 秒"; color: theme.textSecondary; font.pixelSize: 12 }
                    Item { Layout.fillWidth: true }
                }
                Text {
                    Layout.fillWidth: true
                    text: "设为 0 则跟随仿真采样率；低于采样率时将跳过部分源帧。"
                    color: theme.textSecondary; font.pixelSize: 11; wrapMode: Text.Wrap
                }
            }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 7
                Text { text: "视频分辨率"; color: theme.textPrimary; font.pixelSize: 13; font.bold: true }
                RowLayout {
                    ExportSpinBox { id: videoWidth; from: 2; to: 7680; stepSize: 2; value: 1920 }
                    Text { text: "×"; color: theme.textSecondary; font.pixelSize: 16 }
                    ExportSpinBox { id: videoHeight; from: 2; to: 4320; stepSize: 2; value: 1080 }
                    Text { text: "像素"; color: theme.textSecondary; font.pixelSize: 12 }
                    Item { Layout.fillWidth: true }
                }
                Text { text: "宽度和高度必须为偶数。"; color: theme.textSecondary; font.pixelSize: 11 }
            }
            Rectangle {
                Layout.fillWidth: true; implicitHeight: encodingText.implicitHeight + 20
                radius: 7; color: "#102F52"; border.color: "#28547B"
                Text { id: encodingText; anchors.fill: parent; anchors.margins: 10
                    text: "编码器将自动选择；导出时可在播放器中查看进度并取消。"
                    color: theme.textSecondary; font.pixelSize: 11; wrapMode: Text.Wrap }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 10
                Item { Layout.fillWidth: true }
                EgretPushButton { text: "取消"; Layout.fillWidth: false; Layout.preferredWidth: 100; Layout.preferredHeight: 34; onClicked: videoExportDialog.close() }
                EgretPushButton { text: "选择保存位置…"; Layout.fillWidth: false; Layout.preferredWidth: 150; Layout.preferredHeight: 34
                    onClicked: { videoExportDialog.close(); videoFileDialog.open(); }
                }
            }
        }
    }

    Dialog {
        id: videoResult
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: Math.min(480, parent.width - 32)
        padding: 24
        property string text: ""
        background: Rectangle { color: "#0C2041"; radius: 12; border.color: theme.border }
        contentItem: ColumnLayout {
            spacing: 16
            Text { text: "视频导出"; color: theme.textPrimary; font.pixelSize: 18; font.bold: true }
            Text { Layout.fillWidth: true; text: videoResult.text; color: theme.textSecondary; font.pixelSize: 13; wrapMode: Text.Wrap }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                EgretPushButton { text: "确定"; Layout.fillWidth: false; Layout.preferredWidth: 100; Layout.preferredHeight: 34; onClicked: videoResult.close() }
            }
        }
    }

    Connections {
        target: videoExporter
        function onCompleted(success, message) {
            videoResult.text = message;
            videoResult.open();
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

                // 使用播放器自己的场景控制器：未打开仿真文件时是空世界，只显示坐标系与网格
                bodyModelSource: playerSceneController.bodyModel
                constraintModelSource: playerSceneController.constraintModel
                curveModelSource: playerSceneController.curveModel
                convexMeshModelSource: playerSceneController.convexMeshModel

                // ---- 空状态提示：未加载仿真文件时显示 ----
                Rectangle {
                    anchors.centerIn: parent
                    width: 320
                    height: 96
                    radius: theme.radiusS
                    color: "#0D2748"
                    border.width: 1
                    border.color: "#2C5D88"
                    visible: !playerSceneController.simulationLoaded

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
                    visible: playerSceneController.simulationLoaded

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
                    metricValue: playerSceneController.simulationFrameCount
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
                    metricValue: Number(playerSceneController.simulationDurationSeconds).toFixed(2)
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
                    metricValue: Number(playerSceneController.simulationSampleIntervalMs).toFixed(0)
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
                    metricValue: playerSceneController.playbackFrame
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
                Layout.preferredHeight: 280
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
                            playerSceneController.openDefaultSimulationFile();
                        }
                    }

                    EgretPushButton {
                        text: "导出视频（MP4）…"
                        enabled: playerSceneController.simulationLoaded && !videoExporter.running
                        onClicked: playerRoot.showVideoExport()
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: videoExporter.running
                        spacing: 5
                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "正在导出视频"; color: theme.textSecondary; font.pixelSize: 11 }
                            Item { Layout.fillWidth: true }
                            Text { text: Math.round(videoExporter.progress * 100) + "%"; color: theme.accent; font.pixelSize: 11 }
                        }
                        ProgressBar {
                            Layout.fillWidth: true
                            value: videoExporter.progress
                            background: Rectangle { implicitHeight: 6; radius: 3; color: "#183857" }
                            contentItem: Item {
                                Rectangle { width: parent.width * videoExporter.progress; height: parent.height; radius: 3; color: theme.accent }
                            }
                        }
                        EgretPushButton {
                            text: "取消导出"
                            onClicked: videoExporter.cancel()
                        }
                    }

                    EgretPushButton {
                        text: "关闭当前文件"
                        enabled: playerSceneController.simulationLoaded
                        onClicked: {
                            playerSceneController.closeSimulationFile();
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: playerSceneController.simulationLoaded
                            ? playerSceneController.simulationFile
                            : "未加载文件"
                        color: "#9EC4EA"
                        font.pixelSize: 10
                        elide: Text.ElideMiddle
                    }

                    Text {
                        Layout.fillWidth: true
                        text: playerSceneController.simulationError
                        color: theme.danger
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                        visible: playerSceneController.simulationError.length > 0
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
                            enabled: playerSceneController.simulationLoaded
                            onClicked: {
                                playerSceneController.stepPlaybackFrame(-1);
                            }
                        }

                        EgretPushButton {
                            text: playerSceneController.playbackPlaying ? "⏸ 暂停" : "▶ 播放"
                            enabled: playerSceneController.simulationLoaded
                            onClicked: {
                                playerSceneController.toggleSimulationPlayback();
                            }
                        }

                        EgretPushButton {
                            text: "下一帧 ⏭"
                            enabled: playerSceneController.simulationLoaded
                            onClicked: {
                                playerSceneController.stepPlaybackFrame(1);
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
                            enabled: playerSceneController.simulationLoaded
                            cursorShape: Qt.PointingHandCursor

                            function seekTo(x) {
                                const ratio = Math.max(0, Math.min(x / Math.max(1, progressTrack.width), 1));
                                playerSceneController.setPlaybackTime(ratio * playerSceneController.simulationDurationSeconds);
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
                            to: Math.max(0, playerSceneController.simulationDurationSeconds)
                            decimals: 2
                            suffix: "s"
                            readOnly: true
                            value: playerSceneController.playbackTimeSeconds
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
                            to: Math.max(0, playerSceneController.simulationDurationSeconds)
                            decimals: 2
                            suffix: "s"
                            enabled: playerSceneController.simulationLoaded
                            onValueChanged: {
                                if (playerSceneController.simulationLoaded) {
                                    playerSceneController.setPlaybackTime(value);
                                }
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: playerRoot.formatSeconds(playerSceneController.playbackTimeSeconds)
                            + " / " + playerRoot.formatSeconds(playerSceneController.simulationDurationSeconds)
                            + "   ·   帧 " + playerSceneController.playbackFrame
                            + " / " + Math.max(0, playerSceneController.simulationFrameCount - 1)
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
                                    Math.abs(playerSceneController.playbackRate - modelData.rate) < 0.01

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
                                        playerSceneController.playbackRate = modelData.rate;
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
