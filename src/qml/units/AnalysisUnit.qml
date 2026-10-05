/**
 * @file        AnalysisUnit.qml
 * @brief       数据分析工作区视图。
 * @details     左侧选择 EPSX 文件与要分析的实体 / 通道，中间用 Qt Graphs 画散点（按视口做
 *              min/max 抽稀），右侧列出已抽取序列并支持导出 xlsx / CSV。
 *
 *              数据全部来自上下文属性 analysisSession（egret::analysis::AnalysisSession）：
 *              界面只负责收集选择、发起抽取与绘制结果，不做任何数值计算。
 *
 * @author      jehor
 * @date        2026-10-05
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     View
 * @defgroup    Analysis (数据分析)
 */

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Basic 2.15 as Basic
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import QtGraphs

import "qrc:/components/components"
import "qrc:/components/components/basic"

ColumnLayout {
    id: analysisRoot

    /** 主题对象（由 MainWindow 注入）。 */
    property var theme

    /** 已勾选的实体键。 */
    property var selectedEntities: []

    /** 已勾选的通道键。 */
    property var selectedChannels: []

    /** 当前用于绘图的序列键。 */
    property string chartSeriesKey: ""

    /** 单条曲线最大绘图点数。 */
    property int maxPlotPoints: 4000

    spacing: 4

    /** 把 file:/// URL 转换为本地路径。 */
    function localPathFromUrl(fileUrl) {
        return fileUrl.toString().replace(/^file:\/\/\//, "");
    }

    /** 在数组中切换一个字符串项。 */
    function toggle(list, value, checked) {
        const next = [];
        for (let i = 0; i < list.length; ++i) {
            if (list[i] !== value) {
                next.push(list[i]);
            }
        }
        if (checked) {
            next.push(value);
        }
        return next;
    }

    /** 已勾选实体涉及的实体类别集合。 */
    function selectedKinds() {
        const kinds = {};
        const items = analysisSession.entities;
        for (let i = 0; i < items.length; ++i) {
            if (selectedEntities.indexOf(items[i].key) >= 0) {
                kinds[items[i].kind] = true;
            }
        }
        return kinds;
    }

    /** 判断某个通道是否适用于当前选择。 */
    function channelEnabled(channel) {
        if (channel.populated === false) {
            return false;
        }
        return selectedKinds()[channel.owner] === true;
    }

    /** 取某个序列的摘要。 */
    function summaryOf(key) {
        const items = analysisSession.seriesSummary;
        for (let i = 0; i < items.length; ++i) {
            if (items[i].key === key) {
                return items[i];
            }
        }
        return null;
    }

    /** 重画曲线：从 C++ 取抽稀后的点集，写入 Qt Graphs 序列。 */
    function refreshChart() {
        primarySeries.clear();
        const summary = summaryOf(chartSeriesKey);
        if (summary === null) {
            return;
        }

        const span = Math.max(1.0e-9, summary.xMax - summary.xMin);
        const paddingX = span * 0.02;
        const spanY = Math.max(1.0e-9, summary.yMax - summary.yMin);
        const paddingY = spanY * 0.05;

        axisX.min = summary.xMin - paddingX;
        axisX.max = summary.xMax + paddingX;
        axisX.tickInterval = (axisX.max - axisX.min) / 5.0;
        axisX.labelDecimals = 3;

        axisY.min = summary.yMin - paddingY;
        axisY.max = summary.yMax + paddingY;
        axisY.tickInterval = (axisY.max - axisY.min) / 5.0;
        axisY.labelDecimals = 4;

        const points = analysisSession.seriesPoints(chartSeriesKey,
                                                   axisX.min,
                                                   axisX.max,
                                                   maxPlotPoints,
                                                   false);
        for (let i = 0; i < points.length; ++i) {
            primarySeries.append(points[i].x, points[i].y);
        }
        chartTitle.text = summary.label + "（" + points.length + " / " + summary.sampleCount + " 点）";
    }

    FileDialog {
        id: openEpsxDialog
        title: "打开 EPSX 仿真数据文件"
        fileMode: FileDialog.OpenFile
        currentFolder: documentsFolder
        nameFilters: [
            "Egret 仿真文件 (*.epsx)",
            "所有文件 (*)"
        ]
        onAccepted: {
            analysisRoot.selectedEntities = [];
            analysisRoot.selectedChannels = [];
            analysisRoot.chartSeriesKey = "";
            analysisSession.openFile(analysisRoot.localPathFromUrl(selectedFile));
        }
    }

    FileDialog {
        id: exportXlsxDialog
        title: "导出为 Excel 表格"
        fileMode: FileDialog.SaveFile
        currentFolder: documentsFolder
        defaultSuffix: "xlsx"
        nameFilters: [ "Excel 工作簿 (*.xlsx)" ]
        onAccepted: analysisSession.exportTable(analysisRoot.localPathFromUrl(selectedFile), "xlsx")
    }

    FileDialog {
        id: exportCsvDialog
        title: "导出为 CSV"
        fileMode: FileDialog.SaveFile
        currentFolder: documentsFolder
        defaultSuffix: "csv"
        nameFilters: [ "逗号分隔文本 (*.csv)" ]
        onAccepted: analysisSession.exportTable(analysisRoot.localPathFromUrl(selectedFile), "csv")
    }

    Connections {
        target: analysisSession

        function onCatalogChanged() {
            analysisRoot.refreshChart();
        }

        function onResultChanged() {
            const items = analysisSession.seriesSummary;
            if (items.length > 0 && analysisRoot.summaryOf(analysisRoot.chartSeriesKey) === null) {
                analysisRoot.chartSeriesKey = items[0].key;
            }
            analysisRoot.refreshChart();
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 6

        // ================= 左：数据源与选择 =================
        GlassPanel {
            Layout.preferredWidth: 330
            Layout.fillHeight: true
            title: "数据源与选择 / Source"

            ScrollView {
                anchors.fill: parent
                anchors.margins: 8
                clip: true

                ColumnLayout {
                    width: parent.width
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        EgretPushButton {
                            text: "打开 EPSX"
                            Layout.fillWidth: true
                            onClicked: openEpsxDialog.open()
                        }

                        EgretPushButton {
                            text: "关闭"
                            onClicked: {
                                analysisSession.closeFile();
                                analysisRoot.selectedEntities = [];
                                analysisRoot.selectedChannels = [];
                                analysisRoot.chartSeriesKey = "";
                            }
                        }
                    }

                    Repeater {
                        model: analysisSession.fileInfo

                        delegate: Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WrapAnywhere
                            color: theme.textSecondary
                            font.pixelSize: 11
                            text: modelData.key + "：" + modelData.value
                        }
                    }

                    EgretSeparator {
                        Layout.fillWidth: true
                    }

                    Label {
                        text: "实体"
                        color: theme.textSecondary
                        font.pixelSize: 12
                    }

                    Repeater {
                        model: analysisSession.entities

                        delegate: CheckBox {
                            Layout.fillWidth: true
                            text: (modelData.name === undefined
                                   ? modelData.key
                                   : modelData.name)
                                  + "  [" + modelData.kindName + "]"
                            font.pixelSize: 12
                            checked: analysisRoot.selectedEntities.indexOf(modelData.key) >= 0
                            onToggled: {
                                analysisRoot.selectedEntities =
                                    analysisRoot.toggle(analysisRoot.selectedEntities,
                                                        modelData.key,
                                                        checked);
                            }
                        }
                    }

                    EgretSeparator {
                        Layout.fillWidth: true
                    }

                    Label {
                        text: "数据通道（灰色表示该文件未采集）"
                        color: theme.textSecondary
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    Repeater {
                        model: analysisSession.channels

                        delegate: CheckBox {
                            Layout.fillWidth: true
                            enabled: analysisRoot.channelEnabled(modelData)
                            opacity: enabled ? 1.0 : 0.45
                            font.pixelSize: 12
                            text: modelData.name
                                  + (modelData.unit.length > 0 ? " (" + modelData.unit + ")" : "")
                            checked: analysisRoot.selectedChannels.indexOf(modelData.key) >= 0
                            onToggled: {
                                analysisRoot.selectedChannels =
                                    analysisRoot.toggle(analysisRoot.selectedChannels,
                                                        modelData.key,
                                                        checked);
                            }
                        }
                    }

                    EgretSeparator {
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Label {
                            text: "抽帧比（每 N 帧取 1 个样本）"
                            color: theme.textSecondary
                            font.pixelSize: 12
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }

                        SpinBox {
                            id: strideBox
                            from: 1
                            to: 100000
                            value: 1
                            editable: true
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        EgretPushButton {
                            Layout.fillWidth: true
                            text: analysisSession.busy ? "抽取中…" : "开始抽取"
                            enabled: !analysisSession.busy
                                     && analysisRoot.selectedEntities.length > 0
                                     && analysisRoot.selectedChannels.length > 0
                            onClicked: analysisSession.extract(analysisRoot.selectedEntities,
                                                               analysisRoot.selectedChannels,
                                                               strideBox.value)
                        }

                        EgretPushButton {
                            text: "取消"
                            enabled: analysisSession.busy
                            onClicked: analysisSession.cancel()
                        }
                    }

                    ProgressBar {
                        Layout.fillWidth: true
                        from: 0.0
                        to: 1.0
                        value: analysisSession.progress
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: analysisSession.errorMessage.length > 0 ? "#FF9B9B" : theme.textDim
                        font.pixelSize: 11
                        text: analysisSession.errorMessage.length > 0
                              ? analysisSession.errorMessage
                              : analysisSession.statusMessage
                    }
                }
            }
        }

        // ================= 中：散点图 =================
        GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: "散点图 / Scatter"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Label {
                        text: "曲线"
                        color: theme.textSecondary
                        font.pixelSize: 12
                    }

                    ComboBox {
                        id: seriesBox
                        Layout.fillWidth: true
                        textRole: "label"
                        valueRole: "key"
                        model: analysisSession.seriesSummary
                        onActivated: {
                            analysisRoot.chartSeriesKey = currentValue;
                            analysisRoot.refreshChart();
                        }
                    }

                    CheckBox {
                        text: "均匀抽样"
                        font.pixelSize: 12
                        onToggled: analysisRoot.refreshChart()
                    }
                }

                Label {
                    id: chartTitle
                    Layout.fillWidth: true
                    color: theme.textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    text: "尚未抽取数据"
                }

                GraphsView {
                    id: chart
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    theme: GraphsTheme {
                        theme: GraphsTheme.Theme.UserDefined
                        colorScheme: GraphsTheme.ColorScheme.Dark
                    }

                    axisX: ValueAxis {
                        id: axisX
                        min: 0
                        max: 1
                        tickInterval: 0.2
                        labelDecimals: 3
                    }

                    axisY: ValueAxis {
                        id: axisY
                        min: 0
                        max: 1
                        tickInterval: 0.2
                        labelDecimals: 4
                    }

                    LineSeries {
                        id: primarySeries
                        width: 2.0
                        color: "#56B6FF"
                    }
                }
            }
        }

        // ================= 右：序列与导出 =================
        GlassPanel {
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            title: "序列与导出 / Export"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: theme.textSecondary
                    font.pixelSize: 12
                    text: "样本数：" + analysisSession.sampleCount
                          + "，采样间隔：" + analysisSession.sampleIntervalMs.toFixed(3) + " ms"
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: analysisSession.seriesSummary

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 42
                        radius: theme.radiusS
                        color: analysisRoot.chartSeriesKey === modelData.key ? "#1D4E87" : "transparent"

                        Label {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            color: theme.textPrimary
                            font.pixelSize: 12
                            text: modelData.label + "\n"
                                  + modelData.sampleCount + " 点，"
                                  + "y ∈ [" + modelData.yMin.toPrecision(4) + ", "
                                  + modelData.yMax.toPrecision(4) + "]"
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                analysisRoot.chartSeriesKey = modelData.key;
                                analysisRoot.refreshChart();
                            }
                        }
                    }
                }

                EgretSeparator {
                    Layout.fillWidth: true
                }

                EgretPushButton {
                    Layout.fillWidth: true
                    text: "导出 Excel (.xlsx)"
                    enabled: analysisSession.sampleCount > 0 && !analysisSession.busy
                    onClicked: exportXlsxDialog.open()
                }

                EgretPushButton {
                    Layout.fillWidth: true
                    text: "导出 CSV（大数据量推荐）"
                    enabled: analysisSession.sampleCount > 0 && !analysisSession.busy
                    onClicked: exportCsvDialog.open()
                }
            }
        }
    }
}
