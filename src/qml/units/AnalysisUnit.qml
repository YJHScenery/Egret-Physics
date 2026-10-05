/**
 * @file        AnalysisUnit.qml
 * @brief       数据分析工作区视图。
 * @details     左列自下而上分两块：上方「数据源与选择」负责打开 EPSX 与勾选实体 / 通道，
 *              下方「抽取控制」负责抽帧比、开始 / 取消与进度；中间用 Qt Graphs 画曲线
 *              （按视口做 min/max 抽稀），右侧列出已抽取序列并导出 xlsx / CSV。
 *
 *              样式统一约定：控件一律复用 EgretComboBox / EgretCheckBox / FloatField /
 *              EgretPushButton / GlassPanel，文字使用主题色（正文偏白 theme.textPrimary，
 *              辅助信息用 theme.textSecondary），使界面与其它工作区保持一致。
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
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import QtGraphs
import EgretCharts 1.0

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

    /** 可选的拟合模型（供下拉框使用）。 */
    readonly property var fitModels: [
        { key: "linear", label: "线性 y = a·x + b" },
        { key: "polynomial", label: "多项式 y = Σ a_k·u^k" },
        { key: "trigonometric", label: "三角函数 y = a0 + Σ(a·sin + b·cos)" },
        { key: "exponential", label: "指数 y = a·e^(bx)（可含偏移）" },
        { key: "logarithmic", label: "对数 y = a·ln(x) + b" }
    ]

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

    /** 序列键对应的下拉索引。 */
    function seriesIndexFor(key) {
        const items = analysisSession.seriesSummary;
        for (let i = 0; i < items.length; ++i) {
            if (items[i].key === key) {
                return i;
            }
        }
        return -1;
    }

    /** 解析「名称=数值」形式的常量表，例如 "x0=0, v0=1"。 */
    function parseVariables(text) {
        const result = {};
        const parts = text.split(/[,;，；]/);
        for (let i = 0; i < parts.length; ++i) {
            const piece = parts[i].trim();
            if (piece.length === 0) {
                continue;
            }
            const separator = piece.indexOf("=");
            if (separator <= 0) {
                continue;
            }
            const name = piece.substring(0, separator).trim();
            const value = Number(piece.substring(separator + 1));
            if (name.length > 0 && !isNaN(value)) {
                result[name] = value;
            }
        }
        return result;
    }

    /** 重画叠加曲线（拟合曲线或理论曲线）。 */
    function refreshOverlay() {
        chart.setOverlaySeries(analysisSession.overlayPoints, "拟合 / 理论曲线");
    }

    /** 依据序列摘要设置默认坐标轴范围（完整数据范围 + 5% 余量）。 */
    function applyDefaultAxisRange(summary) {
        const spanX = Math.max(1.0e-9, summary.xMax - summary.xMin);
        const paddingX = spanX * 0.02;
        const spanY = Math.max(1.0e-9, summary.yMax - summary.yMin);
        const paddingY = spanY * 0.05;

        axisX.min = summary.xMin - paddingX;
        axisX.max = summary.xMax + paddingX;

        axisY.min = summary.yMin - paddingY;
        axisY.max = summary.yMax + paddingY;
    }

    /**
     * @brief 取「好看」的刻度间隔：把粗略步长归整到 1 / 2 / 5 × 10^k。
     * @param span 坐标轴跨度。
     * @param targetTicks 期望的刻度段数。
     * @return 刻度间隔；跨度非法时返回一个安全值。
     */
    function niceTickInterval(span, targetTicks) {
        if (!(span > 0) || !(targetTicks > 0) || !isFinite(span)) {
            return 1;
        }
        const rough = span / targetTicks;
        const magnitude = Math.pow(10, Math.floor(Math.log(rough) / Math.LN10));
        const normalized = rough / magnitude;
        let step = 10;
        if (normalized <= 1) {
            step = 1;
        } else if (normalized <= 2) {
            step = 2;
        } else if (normalized <= 5) {
            step = 5;
        }
        const interval = step * magnitude;
        return interval > 0 && isFinite(interval) ? interval : span / targetTicks;
    }

    /**
     * @brief 由刻度间隔推出需要显示的小数位数。
     * @param step 刻度间隔。
     * @return 小数位数，范围 0 ~ 8。
     */
    function decimalsForStep(step) {
        if (!(step > 0) || !isFinite(step)) {
            return 2;
        }
        const decimals = Math.ceil(-Math.log(step) / Math.LN10);
        return Math.max(0, Math.min(8, decimals));
    }

    /** 以视图中心为基准缩放坐标轴：factor < 1 放大，> 1 缩小。 */
    function zoomChart(factor) {
        chart.zoomBy(factor);
    }

    /**
     * @brief 取绘图区矩形。
     * @details 优先用 GraphsView.plotArea；布局尚未完成或它给出的矩形异常时，
     *          退化为整块图表区域，保证交互在任何时候都还能用。
     * @return 绘图区矩形。
     */
    function plotRect() {
        const area = chart.plotArea;
        if (area.width > 1 && area.height > 1
                && area.x > -1 && area.y > -1
                && area.x + area.width <= chart.width + 1
                && area.y + area.height <= chart.height + 1) {
            return area;
        }
        return Qt.rect(0, 0, Math.max(1, chart.width), Math.max(1, chart.height));
    }

    /** 交互期间延迟重采样曲线：拖动中每帧重建会让 GraphsView 丢掉鼠标抓取。 */
    function scheduleChartRefresh() {
        refreshTimer.restart();
    }

    /**
     * @brief 把绘图区内的像素横坐标换算成数据横坐标。
     * @param px 相对 GraphsView 的像素 x。
     * @return 对应的横轴数值。
     */
    function dataXAt(px) {
        const area = plotRect();
        if (!(area.width > 0)) {
            return axisX.min;
        }
        return axisX.min + (px - area.x) / area.width * (axisX.max - axisX.min);
    }

    /**
     * @brief 把绘图区内的像素纵坐标换算成数据纵坐标（屏幕 y 向下，数据 y 向上）。
     * @param py 相对 GraphsView 的像素 y。
     * @return 对应的纵轴数值。
     */
    function dataYAt(py) {
        const area = plotRect();
        if (!(area.height > 0)) {
            return axisY.min;
        }
        return axisY.max - (py - area.y) / area.height * (axisY.max - axisY.min);
    }

    /**
     * @brief 按像素位移平移视图（内容跟随鼠标移动）。
     * @param dx 鼠标横向位移（像素）。
     * @param dy 鼠标纵向位移（像素）。
     */
    function panByPixels(dx, dy) {
        const area = plotRect();
        if (!(area.width > 0) || !(area.height > 0)) {
            return;
        }
        const shiftX = dx / area.width * (axisX.max - axisX.min);
        const shiftY = dy / area.height * (axisY.max - axisY.min);
        axisX.min -= shiftX;
        axisX.max -= shiftX;
        axisY.min += shiftY;
        axisY.max += shiftY;
        // 注意：拖动过程中绝不能重建序列。GraphsView 每次重建内部几何都会丢鼠标抓取，
        // 表现为「只能拖很短一段就被强制结束」。刷新统一放在松手时做。
    }

    /**
     * @brief 以鼠标位置为锚点缩放：光标下的数据点保持不动。
     * @param px 锚点像素 x。
     * @param py 锚点像素 y。
     * @param factor 缩放系数，< 1 放大，> 1 缩小。
     */
    function zoomAt(px, py, factor) {
        const anchorX = dataXAt(px);
        const anchorY = dataYAt(py);

        axisX.min = anchorX + (axisX.min - anchorX) * factor;
        axisX.max = anchorX + (axisX.max - anchorX) * factor;
        axisY.min = anchorY + (axisY.min - anchorY) * factor;
        axisY.max = anchorY + (axisY.max - anchorY) * factor;
        analysisRoot.scheduleChartRefresh();
    }

    /**
     * @brief 把框选出来的像素矩形放大成新的视图范围。
     * @param px 矩形左上角像素 x。
     * @param py 矩形左上角像素 y。
     * @param w 矩形宽（像素）。
     * @param h 矩形高（像素）。
     */
    function zoomToRect(px, py, w, h) {
        const left = dataXAt(px);
        const right = dataXAt(px + w);
        const bottom = dataYAt(py + h);
        const top = dataYAt(py);

        axisX.min = Math.min(left, right);
        axisX.max = Math.max(left, right);
        axisY.min = Math.min(bottom, top);
        axisY.max = Math.max(bottom, top);
        analysisRoot.scheduleChartRefresh();
    }

    /** 把视图恢复到整条序列的完整范围。 */
    function resetChart() {
        chart.resetView();
    }

    /** 把分析区间同步为某条序列的完整范围。 */
    function syncRangeToSeries(key) {
        const summary = summaryOf(key);
        if (summary === null) {
            return;
        }
        rangeMinField.setValue(summary.xMin);
        rangeMaxField.setValue(summary.xMax);
    }

    /** 重画曲线：从 C++ 取抽稀后的点集，交给 QCustomPlot。 */
    function refreshChart(resetView) {
        const summary = summaryOf(chartSeriesKey);
        if (summary === null) {
            chartTitle.text = "尚未抽取数据";
            chart.clearSeries();
            return;
        }

        const points = analysisSession.seriesPoints(chartSeriesKey,
                                                   summary.xMin,
                                                   summary.xMax,
                                                   maxPlotPoints,
                                                   uniformCheck.checked);
        chart.setPrimarySeries(points, summary.label);
        if (resetView !== false) {
            chart.resetView();
        }
        chartTitle.text = summary.label
                          + "　显示 " + points.length + " / " + summary.sampleCount + " 点";
        analysisRoot.refreshOverlay();
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
            analysisRoot.syncRangeToSeries(analysisRoot.chartSeriesKey);
        }

        function onOverlayChanged() {
            analysisRoot.refreshOverlay();
        }
    }

    /**
     * @brief 交互（拖动 / 滚轮）停止后延迟重采样曲线。
     * @details 拖动过程中若每帧都重建序列，GraphsView 会重建内部场景并丢掉鼠标抓取，
     *          表现为「拖一下就断」。因此拖动期间只改坐标轴，停手后再统一重采样。
     */
    Timer {
        id: refreshTimer
        interval: 90
        repeat: false
        onTriggered: {
            // 仍在拖动中就不要重建，交给松手后的那次刷新
            if (!plotMouse.pressed) {
                analysisRoot.refreshChart(false);
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 6

        // ================= 左列：数据源 + 抽取控制（两块等宽） =================
        ColumnLayout {
            Layout.preferredWidth: 264
            Layout.minimumWidth: 264
            Layout.maximumWidth: 264
            Layout.fillHeight: true
            spacing: 6

            GlassPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "数据源与选择 / Source"

                ScrollView {
                    anchors.fill: parent
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

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
                                color: theme.textPrimary
                                font.pixelSize: 11
                                text: modelData.key + "：" + modelData.value
                            }
                        }

                        EgretSeparator {
                            Layout.fillWidth: true
                        }

                        Label {
                            text: "实体"
                            color: theme.textPrimary
                            font.pixelSize: 12
                            font.bold: true
                        }

                        Repeater {
                            model: analysisSession.entities

                            delegate: EgretCheckBox {
                                Layout.fillWidth: true
                                text: (modelData.name === undefined
                                       ? modelData.key
                                       : modelData.name)
                                      + "  [" + modelData.kindName + "]"
                                textColor: theme.textPrimary
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
                            text: "数据通道"
                            color: theme.textPrimary
                            font.pixelSize: 12
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: theme.textSecondary
                            font.pixelSize: 11
                            text: "灰色项表示该 EPSX 文件未采集，或与当前选中的实体类别不匹配。"
                        }

                        Repeater {
                            model: analysisSession.channels

                            delegate: EgretCheckBox {
                                Layout.fillWidth: true
                                enabled: analysisRoot.channelEnabled(modelData)
                                text: modelData.name
                                      + (modelData.unit.length > 0
                                         ? " (" + modelData.unit + ")"
                                         : "")
                                textColor: theme.textPrimary
                                checked: analysisRoot.selectedChannels.indexOf(modelData.key) >= 0
                                onToggled: {
                                    analysisRoot.selectedChannels =
                                        analysisRoot.toggle(analysisRoot.selectedChannels,
                                                            modelData.key,
                                                            checked);
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
                Layout.fillWidth: true
                Layout.preferredHeight: 214
                title: "抽取控制 / Extraction"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: theme.textPrimary
                            font.pixelSize: 12
                            text: "抽帧比（每 N 帧取 1 个样本）"
                        }

                        FloatField {
                            id: strideField
                            Layout.preferredWidth: 92
                            from: 1
                            to: 1000000
                            decimals: 0
                            value: 1
                            suffix: "帧"
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        EgretPushButton {
                            Layout.fillWidth: true
                            text: analysisSession.busy ? "抽取中…" : "开始抽取"
                            enabled: !analysisSession.busy
                                     && analysisRoot.selectedEntities.length > 0
                                     && analysisRoot.selectedChannels.length > 0
                            onClicked: analysisSession.extract(analysisRoot.selectedEntities,
                                                               analysisRoot.selectedChannels,
                                                               Math.round(strideField.value))
                        }

                        EgretPushButton {
                            text: "取消"
                            enabled: analysisSession.busy
                            onClicked: analysisSession.cancel()
                        }
                    }

                    ProgressBar {
                        id: progressBar
                        Layout.fillWidth: true
                        from: 0.0
                        to: 1.0
                        value: analysisSession.progress

                        background: Rectangle {
                            implicitHeight: 8
                            radius: 4
                            color: "#0A1A3A"
                            border.width: 1
                            border.color: "#2F5F8D"
                        }

                        contentItem: Item {
                            implicitHeight: 8

                            Rectangle {
                                width: parent.width * progressBar.visualPosition
                                height: parent.height
                                radius: 4
                                color: theme.accent
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: analysisSession.errorMessage.length > 0
                               ? theme.danger
                               : theme.textSecondary
                        font.pixelSize: 11
                        text: analysisSession.errorMessage.length > 0
                              ? analysisSession.errorMessage
                              : analysisSession.statusMessage
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }
        }

        // ================= 中：散点图 =================
        GlassPanel {
            Layout.fillWidth: true
            Layout.minimumWidth: 320
            Layout.fillHeight: true
            title: "散点图 / Scatter"

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: "曲线"
                        color: theme.textPrimary
                        font.pixelSize: 12
                    }

                    EgretComboBox {
                        id: seriesBox
                        Layout.fillWidth: true
                        model: analysisSession.seriesSummary
                        textRole: "label"
                        entryValueRole: "key"
                        currentIndex: analysisRoot.seriesIndexFor(analysisRoot.chartSeriesKey)
                        onActivated: function (index) {
                            analysisRoot.chartSeriesKey = seriesBox.entryValue;
                            analysisRoot.refreshChart();
                            analysisRoot.syncRangeToSeries(analysisRoot.chartSeriesKey);
                        }
                    }

                    EgretCheckBox {
                        id: uniformCheck
                        text: "均匀抽样"
                        textColor: theme.textPrimary
                        onToggled: analysisRoot.refreshChart(false)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Label {
                        text: "视图"
                        color: theme.textPrimary
                        font.pixelSize: 12
                    }

                    EgretPushButton {
                        Layout.preferredWidth: 56
                        text: "放大"
                        onClicked: analysisRoot.zoomChart(0.7)
                    }

                    EgretPushButton {
                        Layout.preferredWidth: 56
                        text: "缩小"
                        onClicked: analysisRoot.zoomChart(1.4)
                    }

                    EgretPushButton {
                        Layout.preferredWidth: 80
                        text: "重置视图"
                        onClicked: analysisRoot.resetChart()
                    }

                    EgretCheckBox {
                        id: areaZoomCheck
                        text: "框选放大"
                        textColor: theme.textPrimary
                        onToggled: chart.setAreaZoomEnabled(checked)
                    }

                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                        color: theme.textSecondary
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        text: areaZoomCheck.checked
                              ? "滚轮缩放 · 左键拖拽框选放大"
                              : "滚轮缩放 · 左键拖拽平移"
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

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                // 图表改用 QCustomPlot：平移 / 滚轮缩放 / 框选放大 / 自适应刻度
                // 都是它内建且成熟的功能，不再自行实现交互。
                QCustomPlotItem {
                    id: chart
                    anchors.fill: parent
                }

                // 覆盖在 GraphsView 之上的交互层。
                // 用指针处理器而不是 MouseArea：DragHandler 在按下后会一直持有独占抓取，
                // 即便 GraphsView 因坐标轴变化重建了内部几何也不会把手势抢走。
                Item {
                    id: plotSurface
                    anchors.fill: parent
                    // 交互已交给 QCustomPlot，本层停用，避免拦截鼠标事件
                    enabled: false

                    /** 框选起点（plotSurface 坐标）。 */
                    property real pressX: 0

                    /** 框选起点（plotSurface 坐标）。 */
                    property real pressY: 0

                    DragHandler {
                        id: panHandler
                        target: null
                        acceptedButtons: Qt.LeftButton
                        cursorShape: Qt.CrossCursor

                        property real lastX: 0
                        property real lastY: 0

                        onActiveChanged: {
                            if (active) {
                                lastX = translation.x;
                                lastY = translation.y;
                                plotSurface.pressX = point.position.x;
                                plotSurface.pressY = point.position.y;
                                rubber.visible = false;
                                return;
                            }

                            if (areaZoomCheck.checked && rubber.visible
                                    && rubber.width > 8 && rubber.height > 8) {
                                analysisRoot.zoomToRect(rubber.x, rubber.y,
                                                        rubber.width, rubber.height);
                            }
                            rubber.visible = false;
                            analysisRoot.refreshChart(false);
                        }

                        onTranslationChanged: {
                            if (!active) {
                                return;
                            }
                            const dx = translation.x - lastX;
                            const dy = translation.y - lastY;
                            lastX = translation.x;
                            lastY = translation.y;

                            if (areaZoomCheck.checked) {
                                const cx = point.position.x;
                                const cy = point.position.y;
                                rubber.visible = true;
                                rubber.x = Math.min(plotSurface.pressX, cx);
                                rubber.y = Math.min(plotSurface.pressY, cy);
                                rubber.width = Math.abs(cx - plotSurface.pressX);
                                rubber.height = Math.abs(cy - plotSurface.pressY);
                            } else if (dx !== 0 || dy !== 0) {
                                analysisRoot.panByPixels(dx, dy);
                            }
                        }
                    }

                    WheelHandler {
                        onWheel: function (event) {
                            // 滚轮向上 = 放大（与常见绘图工具一致）
                            const factor = event.angleDelta.y > 0 ? 1.0 / 1.15 : 1.15;
                            analysisRoot.zoomAt(event.position.x, event.position.y, factor);
                        }
                    }

                    Rectangle {
                        id: rubber
                        visible: false
                        color: "#3356B6FF"
                        border.width: 1
                        border.color: theme.accent
                    }
                }
                }
            }
        }

        // ================= 右：序列与导出 =================
        ColumnLayout {
            Layout.preferredWidth: 288
            Layout.minimumWidth: 288
            Layout.maximumWidth: 288
            Layout.fillHeight: true
            spacing: 6

            // ---- 上：序列列表（与下方面板等分右栏高度）----
            GlassPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 1
                title: "序列 / Series"

                ScrollView {
                    anchors.fill: parent
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width: parent.width
                        spacing: 8

                        Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: theme.textPrimary
                    font.pixelSize: 12
                    text: "样本数：" + analysisSession.sampleCount
                          + "　采样间隔：" + analysisSession.sampleIntervalMs.toFixed(3) + " ms"
                        }

                Repeater {
                    model: analysisSession.seriesSummary

                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        radius: theme.radiusS
                        color: analysisRoot.chartSeriesKey === modelData.key
                               ? "#1D4E87"
                               : "transparent"

                        Label {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            color: theme.textPrimary
                            font.pixelSize: 12
                            text: modelData.label + "\n"
                                  + modelData.sampleCount + " 点，y ∈ ["
                                  + Number(modelData.yMin).toPrecision(4) + ", "
                                  + Number(modelData.yMax).toPrecision(4) + "]"
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                analysisRoot.chartSeriesKey = modelData.key;
                                analysisRoot.refreshChart();
                                analysisRoot.syncRangeToSeries(analysisRoot.chartSeriesKey);
                            }
                        }
                    }
                }
                }
            }
            }

            // ---- 下：导出与分析（与上方面板等分右栏高度）----
            GlassPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 1
                title: "导出与分析 / Export & Analysis"

                ScrollView {
                    anchors.fill: parent
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width: parent.width
                        spacing: 8

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

                EgretSeparator {
                    Layout.fillWidth: true
                }

                Label {
                    text: "曲线拟合"
                    color: theme.textPrimary
                    font.pixelSize: 12
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: "分析区间（拟合与误差对比只统计区间内的样本）"
                    color: theme.textSecondary
                    font.pixelSize: 11
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    FloatField {
                        id: rangeMinField
                        Layout.fillWidth: true
                        from: -1.0e12
                        to: 1.0e12
                        decimals: 4
                        value: 0
                    }

                    Label {
                        text: "→"
                        color: theme.textSecondary
                        font.pixelSize: 12
                    }

                    FloatField {
                        id: rangeMaxField
                        Layout.fillWidth: true
                        from: -1.0e12
                        to: 1.0e12
                        decimals: 4
                        value: 0
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    EgretPushButton {
                        Layout.fillWidth: true
                        text: "取当前视图"
                        onClicked: {
                            rangeMinField.setValue(chart.visibleXMin());
                            rangeMaxField.setValue(chart.visibleXMax());
                        }
                    }

                    EgretPushButton {
                        Layout.fillWidth: true
                        text: "恢复全范围"
                        onClicked: analysisRoot.syncRangeToSeries(analysisRoot.chartSeriesKey)
                    }
                }

                EgretComboBox {
                    id: fitModelBox
                    Layout.fillWidth: true
                    model: analysisRoot.fitModels
                    textRole: "label"
                    entryValueRole: "key"
                    currentIndex: 1
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Label {
                        text: "阶数"
                        color: theme.textSecondary
                        font.pixelSize: 11
                    }

                    FloatField {
                        id: degreeField
                        Layout.preferredWidth: 66
                        from: 1
                        to: 12
                        decimals: 0
                        value: 2
                    }

                    Label {
                        text: "谐波"
                        color: theme.textSecondary
                        font.pixelSize: 11
                    }

                    FloatField {
                        id: harmonicField
                        Layout.preferredWidth: 66
                        from: 1
                        to: 8
                        decimals: 0
                        value: 1
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: "角频率 ω（0 = 自动估计）"
                        color: theme.textSecondary
                        font.pixelSize: 11
                    }

                    FloatField {
                        id: omegaField
                        Layout.preferredWidth: 92
                        from: 0
                        to: 1000000
                        decimals: 4
                        value: 0
                    }
                }

                EgretCheckBox {
                    id: offsetCheck
                    text: "指数模型含偏移项 c"
                    textColor: theme.textPrimary
                }

                EgretPushButton {
                    Layout.fillWidth: true
                    text: "执行拟合并叠加"
                    enabled: analysisSession.sampleCount > 0
                             && analysisRoot.chartSeriesKey.length > 0
                             && !analysisSession.busy
                    onClicked: analysisSession.fitSeries(analysisRoot.chartSeriesKey,
                                                         fitModelBox.entryValue,
                                                         Math.round(degreeField.value),
                                                         Math.round(harmonicField.value),
                                                         omegaField.value,
                                                         offsetCheck.checked,
                                                         rangeMinField.value,
                                                         rangeMaxField.value)
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 11
                    color: analysisSession.fitInfo.ok === true
                           ? theme.textPrimary
                           : theme.textSecondary
                    text: analysisSession.fitInfo.ok === true
                          ? analysisSession.fitInfo.equation + "\n"
                            + "R² = " + Number(analysisSession.fitInfo.rSquared).toFixed(6)
                            + "　RMSE = "
                            + Number(analysisSession.fitInfo.rmse).toPrecision(4)
                            + "\n拟合区间 [" + Number(analysisSession.fitInfo.rangeMin).toPrecision(4)
                            + ", " + Number(analysisSession.fitInfo.rangeMax).toPrecision(4) + "]"
                          : (analysisSession.fitInfo.message !== undefined
                             ? analysisSession.fitInfo.message
                             : "")
                }

                EgretSeparator {
                    Layout.fillWidth: true
                }

                Label {
                    text: "误差对比（理论方程）"
                    color: theme.textPrimary
                    font.pixelSize: 12
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Label {
                        text: "自变量"
                        color: theme.textSecondary
                        font.pixelSize: 11
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 26
                        radius: theme.radiusS
                        color: theme.bg1
                        border.width: 1
                        border.color: varField.activeFocus ? theme.accent : theme.border

                        TextField {
                            id: varField
                            anchors.fill: parent
                            text: "t"
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            leftPadding: 8
                            rightPadding: 8
                            verticalAlignment: Text.AlignVCenter
                            background: null
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52
                    radius: theme.radiusS
                    color: theme.bg1
                    border.width: 1
                    border.color: exprField.activeFocus ? theme.accent : theme.border

                    TextField {
                        id: exprField
                        anchors.fill: parent
                        color: "#FFFFFF"
                        font.pixelSize: 12
                        leftPadding: 8
                        rightPadding: 8
                        topPadding: 6
                        verticalAlignment: Text.AlignTop
                        wrapMode: Text.WrapAnywhere
                        background: null
                        placeholderText: "x0 + v0*t + 0.5*a*t*t"
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 26
                    radius: theme.radiusS
                    color: theme.bg1
                    border.width: 1
                    border.color: constField.activeFocus ? theme.accent : theme.border

                    TextField {
                        id: constField
                        anchors.fill: parent
                        color: "#FFFFFF"
                        font.pixelSize: 12
                        leftPadding: 8
                        rightPadding: 8
                        verticalAlignment: Text.AlignVCenter
                        background: null
                        placeholderText: "常量：x0=0, v0=1, a=-9.8"
                    }
                }

                EgretPushButton {
                    Layout.fillWidth: true
                    text: "对比并叠加理论曲线"
                    enabled: analysisSession.sampleCount > 0
                             && analysisRoot.chartSeriesKey.length > 0
                             && exprField.text.length > 0
                             && !analysisSession.busy
                    onClicked: analysisSession.compareSeries(analysisRoot.chartSeriesKey,
                                                             exprField.text,
                                                             varField.text,
                                                             analysisRoot.parseVariables(constField.text),
                                                             rangeMinField.value,
                                                             rangeMaxField.value)
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 11
                    color: analysisSession.comparisonInfo.ok === true
                           ? theme.textPrimary
                           : theme.textSecondary
                    text: analysisSession.comparisonInfo.ok === true
                          ? "R² = "
                            + Number(analysisSession.comparisonInfo.rSquared).toFixed(6)
                            + "　RMSE = "
                            + Number(analysisSession.comparisonInfo.rmse).toPrecision(4)
                            + "\n最大绝对误差 = "
                            + Number(analysisSession.comparisonInfo.maxAbsError).toPrecision(4)
                            + "　平均相对误差 = "
                            + (Number(analysisSession.comparisonInfo.meanRelativeError) * 100.0).toFixed(3)
                            + "%"
                          : (analysisSession.comparisonInfo.message !== undefined
                             ? analysisSession.comparisonInfo.message
                             : "")
                }
                }
            }
            }
        }
    }
}
