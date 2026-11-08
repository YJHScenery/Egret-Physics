/**
 * @file        MainWindow.qml
 * @brief       物理引擎主窗口文件
 * @details     定义应用程序的主窗口，包含仿真器和编辑器单元的切换功能。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-24
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
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Dialogs
import EditorManager 1.0
import QtQuick3D 6.9
import ClipboardManager 1.0

import "qrc:/components/components"
import "qrc:/components/components/message_box"
import "qrc:/components/components/theme"
import "qrc:/units/units"

ApplicationWindow {
    id: window

    width: 1440
    height: 900
    minimumWidth: 1024
    minimumHeight: 600

    visible: true
    title: "Egret Physics - Classical Mechanics Studio"

    DeepBlueTheme {
        id: theme
    }
    readonly property bool editingText: isTextInput(mainMenu.openIndex >= 0 ? mainMenu.focusBeforeMenu : window.activeFocusItem)
    readonly property bool entityEditEnabled: mainRowLayout.currentRoute === "solver" && !editingText
    readonly property var activeCoordinates: mainRowLayout.currentRoute === "solver" ? editorUnit.coordinateSystem
        : mainRowLayout.currentRoute === "player" ? playerUnit.coordinateSystem
        : mainRowLayout.currentRoute === "scene" ? simulatorUnit.coordinateSystem : null
    readonly property bool modalDialogOpen: closeDocumentDialog.visible || aboutDialog.visible || mainMsgDialog.visible
        || editorSettingsDialog.visible || shortcutHelpDialog.visible || editorUnit.controlDialogVisible
        || playerUnit.exportDialogVisible || openJsonFileDialog.visible || saveJsonFileDialog.visible || openEpsxFileDialog.visible
        || (analysisUnitLoader.item !== null && analysisUnitLoader.item.dialogVisible)
    property bool exitPending: false
    property bool exitApproved: false

    /** @brief 判断焦点是否属于可编辑文本，避免场景快捷键抢占文本操作。 */
    function isTextInput(item) {
        return !!item && typeof item.selectAll === "function" && typeof item.insert === "function"
    }
    /** @brief 根据工作区打开场景或 EPSX 文件。 */
    function openWorkspaceFile() {
        if (mainRowLayout.currentRoute === "player" || mainRowLayout.currentRoute === "analysis") openEpsxFileDialog.open()
        else {openJsonFileDialog.mode = mainRowLayout.currentRoute === "solver" ? 1 : 0;openJsonFileDialog.open()}
    }
    /** @brief 当前工作区恢复完整取景或分析曲线范围。 */
    function fitWorkspace() {
        if (mainRowLayout.currentRoute === "solver") editorUnit.fitView(false)
        else if (mainRowLayout.currentRoute === "player") playerUnit.fitView()
        else if (mainRowLayout.currentRoute === "analysis" && analysisUnitLoader.item) analysisUnitLoader.item.resetChart()
        else if (mainRowLayout.currentRoute === "scene") simulatorUnit.fitView()
    }
    /** @brief 开始退出流程，对所有未保存标签逐个询问。 */
    function requestExit() {
        if (exitPending) return
        exitPending = true
        continueExit()
    }
    /** @brief 保存或放弃一个标签后继续退出，取消则终止整个退出流程。 */
    function continueExit() {
        if (!exitPending) return
        for (var i = 0; i < EditorManager.documents.length; ++i) {
            if (EditorManager.documents[i].dirty) {requestCloseDocument(i);return}
        }
        simulationExporter.cancel()
        videoExporter.cancel()
        analysisSession.cancel()
        sceneController.pause()
        playerSceneController.pauseSimulation()
        exitApproved = true
        Qt.callLater(window.close)
    }
    onClosing: function(event) {
        if (!exitApproved) {event.accepted = false;requestExit()}
    }
    EditorSettingsDialog {id: editorSettingsDialog}
    ShortcutHelpDialog {id: shortcutHelpDialog}

    ListModel {
        id: workspaceRouteModel
        ListElement {
            label: "编辑与求解"; route: "solver"; badge: ""
        }
        ListElement {
            label: "播放器"; route: "player"; badge: ""
        }
        ListElement {
            label: "数据分析"; route: "analysis"; badge: ""
        }
        ListElement {
            label: "实时仿真"; route: "scene"; badge: "不再维护"
        }
    }

    function routeIndexFor(route) {
        for (let i = 0; i < workspaceRouteModel.count; i += 1) {
            if (workspaceRouteModel.get(i).route === route) {
                return i;
            }
        }
        return 0;
    }

    /** 当前打开的 epsx 文件路径：播放器与数据分析工作区共享同一份文件。 */
    property string currentSimulationPath: ""

    /** 把 file:/// URL 转换为本地路径。 */
    function localPathFromUrl(fileUrl) {
        return fileUrl.toString().replace(/^file:\/\/\//, "");
    }

    /**
     * @brief 打开 epsx：数据分析与播放器共用同一份文件。
     * @details 两个工作区（以及菜单里的「打开…」）都只通过本函数打开文件，
     *          所以不会出现「一个工作区打开了、另一个没同步」的情况。
     * @param path epsx 文件路径。
     * @return 数据分析侧打开成功返回 true。
     */
    function openEpsx(path) {
        if (path === undefined || path === null || path.length === 0) {
            return false;
        }
        if (!analysisSession.openFile(path)) {
            return false;
        }
        window.currentSimulationPath = analysisSession.sourcePath;
        // 先关掉播放器里可能残留的上一份仿真，再加载新文件
        playerSceneController.closeSimulationFile();
        playerSceneController.openSimulationFile(window.currentSimulationPath);
        return true;
    }

    /** 关闭 epsx：数据分析与播放器一起清空，状态栏同步。 */
    function closeEpsx() {
        analysisSession.closeFile();
        playerSceneController.closeSimulationFile();
        window.currentSimulationPath = "";
    }

    /** 即将保存后关闭的标签索引，-1 表示普通保存。 */
    property int closeAfterSave: -1

    /** @brief 保存当前场景，必要时请求新的文件路径。 */
    function saveEditorDocument(forceDialog) {
        if (forceDialog || EditorManager.documentNeedsSaveAs()) {
            saveJsonFileDialog.open();
            return;
        }
        if (EditorManager.saveDocument()) {
            if (closeAfterSave >= 0) EditorManager.closeDocument(closeAfterSave);
            closeAfterSave = -1;
            Qt.callLater(window.continueExit);
        } else {
            closeAfterSave = -1;
            exitPending = false;
            mainMsgDialog.title = "保存失败";
            mainMsgDialog.text = "无法保存场景，请检查文件路径和权限。";
            mainMsgDialog.open();
        }
    }

    /** @brief 关闭标签前处理未保存的修改。 */
    function requestCloseDocument(index) {
        var doc = EditorManager.documents[index];
        if (!doc) return;
        if (!doc.dirty) {
            var wasCurrent = index === EditorManager.currentDocument;
            EditorManager.closeDocument(index);
            if (wasCurrent) editorUnit.resetInspector();
            return;
        }
        closeDocumentDialog.documentIndex = index;
        closeDocumentDialog.documentName = doc.name;
        closeDocumentDialog.open();
    }

    SceneCloseDialog {
        id: closeDocumentDialog
        onAccepted: {
            EditorManager.switchDocument(documentIndex);
            editorUnit.resetInspector();
            window.closeAfterSave = documentIndex;
            window.saveEditorDocument(false);
        }
        onDiscarded: {
            var wasCurrent = documentIndex === EditorManager.currentDocument;
            EditorManager.closeDocument(documentIndex);
            if (wasCurrent) editorUnit.resetInspector();
            Qt.callLater(window.continueExit);
        }
        onRejected: window.exitPending = false
    }

    FileDialog {
        id: openEpsxFileDialog
        title: "打开 EPSX 仿真数据文件"
        fileMode: FileDialog.OpenFile
        currentFolder: documentsFolder
        nameFilters: [
            "Egret 仿真文件 (*.epsx)",
            "所有文件 (*)"
        ]
        onAccepted: window.openEpsx(window.localPathFromUrl(selectedFile))
    }

    AboutDialog {
        id: aboutDialog
        parent: Overlay.overlay
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: theme.bg0
            }
            GradientStop {
                position: 0.5
                color: theme.bg1
            }
            GradientStop {
                position: 1.0
                color: theme.bg2
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 4

        FileDialog {
            id: openJsonFileDialog
            title: "打开序列化场景文件"
            fileMode: FileDialog.OpenFile
            currentFolder: documentsFolder
            nameFilters: [
                "场景文件 (*.epscenex *.epscene)",
                "所有文件 (*)"
            ]

            property int mode: 0

            onAccepted: {
                var fileUrl = selectedFile
                var localPath = fileUrl.toString().replace(/^file:\/\/\//, "")  // 简单转换

                switch (mode){
                    case 0:{
                        sceneController.loadSceneFronJsonFile(localPath);
                        break;
                    }case 1: {
                        if (EditorManager.openDocument(fileUrl.toString())) editorUnit.resetInspector();
                        else { mainMsgDialog.title = "打开失败"; mainMsgDialog.text = "无法读取场景文件。"; mainMsgDialog.open(); }
                        break;
                    }
                }
            }
            onRejected: {
                console.log("用户取消打开")
            }
        }

        FileDialog {
            id: saveJsonFileDialog
            title: "保存场景"
            fileMode: FileDialog.SaveFile
            currentFolder: documentsFolder
            defaultSuffix: selectedNameFilter.index === 1 ? "epscene" : "epscenex"
            nameFilters: [
                "场景 V2.0 (*.epscenex)",
                "兼容场景 V1.0 (*.epscene)",
                "所有文件 (*)"
            ]

            // currentFile: "Untitled.epscenex"

            onAccepted: {
                // 注意：selectedFile 返回的是 file:// 开头的 URL，需转换为本地路径
                var fileUrl = selectedFile
                var localPath = fileUrl.toString().replace(/^file:\/\/\//, "")  // 简单转换
                if (EditorManager.saveDocument(fileUrl.toString())) {
                    if (window.closeAfterSave >= 0) {
                        EditorManager.closeDocument(window.closeAfterSave);
                        editorUnit.resetInspector();
                    }
                    window.closeAfterSave = -1;
                    Qt.callLater(window.continueExit);
                } else {
                    window.closeAfterSave = -1;
                    window.exitPending = false;
                    mainMsgDialog.title = "保存失败";
                    mainMsgDialog.text = "无法保存场景，请检查文件路径和权限。";
                    mainMsgDialog.open();
                }
            }

            onRejected: { window.closeAfterSave = -1; window.exitPending = false; }
        }

        EgretMessageDialog {
            id: mainMsgDialog
            level: EgretMessageDialog.Level.Warning
            title: "Default Title"
            text: "Default Content"
            buttons: EgretMessageDialog.StandardButton.Ok
            soundType: "Warning"
        }

        EgretMenuBar {
            id: mainMenu
            objectName: "mainMenu"
            shortcutsEnabled: !window.modalDialogOpen
            textInputActive: window.editingText
            Layout.fillWidth: true
            barHeight: 24
            barBackground: theme.panelStrong
            barBorderColor: theme.border
            itemHoverBackground: theme.accentSoft
            itemActiveBackground: "#1C3F6C"
            textColor: theme.textSecondary
            textHoverColor: theme.textPrimary
            menuBackground: "#10294C"
            menuBorderColor: theme.border
            menuItemHoverBackground: "#1E4474"
            menuItemPressedBackground: "#25507E"
            menuItemTextColor: theme.textSecondary
            menuItemHoverTextColor: theme.textPrimary
            menuSeparatorColor: "#2C4E74"

            menus: [
                {title: "文件(&F)", items: [
                    {id: "newScene", icon: "qrc:/main_icons/assets/icons/new_scene.svg", text: "新建场景(&N)", shortcut: "Ctrl+N", onTriggered: function() {mainRowLayout.currentRoute = "solver";EditorManager.newDocument();editorUnit.resetInspector()}},
                    {id: "openFile", icon: "qrc:/main_icons/assets/icons/open.svg", text: "打开…(&O)", shortcut: "Ctrl+O", onTriggered: function() {window.openWorkspaceFile()}},
                    {separator: true},
                    {id: "saveScene", icon: "qrc:/main_icons/assets/icons/save.svg", text: "保存场景(&S)", shortcut: "Ctrl+S", enabled: editorUnit.visible, onTriggered: function() {window.saveEditorDocument(false)}},
                    {id: "saveSceneAs", icon: "qrc:/main_icons/assets/icons/save_as.svg", text: "场景另存为…(&A)", shortcut: "Ctrl+Shift+S", enabled: editorUnit.visible, onTriggered: function() {window.saveEditorDocument(true)}},
                    {id: "closeDocument", text: "关闭当前场景(&C)", shortcut: "Ctrl+W", enabled: editorUnit.visible, onTriggered: function() {window.requestCloseDocument(EditorManager.currentDocument)}},
                    {id: "closeResult", text: "关闭仿真文件", enabled: window.currentSimulationPath.length > 0 && (playerUnit.visible || analysisUnitLoader.visible) && !videoExporter.running && !analysisSession.busy, onTriggered: function() {window.closeEpsx()}},
                    {separator: true},
                    {text: "保存／导出结果", icon: "qrc:/main_icons/assets/icons/save_as.svg", enabled: (editorUnit.visible && simulationExporter.frameCount > 0 && !simulationExporter.running) || (playerUnit.visible && playerSceneController.simulationLoaded && !videoExporter.running) || (analysisUnitLoader.visible && analysisSession.sampleCount > 0 && !analysisSession.busy), submenu: [
                        {id: "saveSolveResult", icon: "qrc:/main_icons/assets/icons/record_save.svg", text: "仿真结果（.epsx）…", enabled: editorUnit.visible && simulationExporter.frameCount > 0 && !simulationExporter.running, onTriggered: function() {editorUnit.saveSolveResult()}},
                        {id: "exportVideo", icon: "qrc:/main_icons/assets/icons/record_save.svg", text: "视频（.mp4）…", enabled: playerUnit.visible && playerSceneController.simulationLoaded && !videoExporter.running, onTriggered: function() {playerUnit.showVideoExport()}},
                        {id: "exportXlsx", icon: "qrc:/main_icons/assets/icons/save_as.svg", text: "分析数据（.xlsx）…", enabled: analysisUnitLoader.visible && analysisUnitLoader.item !== null && analysisSession.sampleCount > 0 && !analysisSession.busy, onTriggered: function() {analysisUnitLoader.item.exportData("xlsx")}},
                        {id: "exportCsv", icon: "qrc:/main_icons/assets/icons/save_as.svg", text: "分析数据（.csv）…", enabled: analysisUnitLoader.visible && analysisUnitLoader.item !== null && analysisSession.sampleCount > 0 && !analysisSession.busy, onTriggered: function() {analysisUnitLoader.item.exportData("csv")}}
                    ]},
                    {separator: true},
                    {id: "editorSettings", icon: "qrc:/main_icons/assets/icons/settings.svg", text: "编辑器设置…(&P)", shortcut: "Ctrl+,", onTriggered: function() {editorSettingsDialog.open()}},
                    {id: "exit", icon: "qrc:/main_icons/assets/icons/exit.svg", text: "退出(&Q)", shortcut: "Alt+F4", onTriggered: function() {window.requestExit()}}
                ]},
                {title: "编辑(&E)", items: [
                    {id: "undo", icon: "qrc:/main_icons/assets/icons/undo.svg", text: "撤销(&U)", shortcut: "Ctrl+Z", editing: true, enabled: window.entityEditEnabled && EditorManager.canUndo, onTriggered: function() {EditorManager.undo()}},
                    {id: "redo", icon: "qrc:/main_icons/assets/icons/recover.svg", text: "重做(&R)", shortcut: "Ctrl+Y", editing: true, enabled: window.entityEditEnabled && EditorManager.canRedo, onTriggered: function() {EditorManager.redo()}},
                    {separator: true},
                    {id: "cut", icon: "qrc:/main_icons/assets/icons/cut.svg", text: "剪切(&T)", shortcut: "Ctrl+X", editing: true, enabled: window.entityEditEnabled && editorUnit.selectedCount > 0, onTriggered: function() {editorUnit.cutSelectedEntities()}},
                    {id: "copy", icon: "qrc:/main_icons/assets/icons/copy.svg", text: "复制(&C)", shortcut: "Ctrl+C", editing: true, enabled: window.entityEditEnabled && editorUnit.selectedCount > 0, onTriggered: function() {editorUnit.copySelectedEntities()}},
                    {id: "paste", icon: "qrc:/main_icons/assets/icons/paste.svg", text: "粘贴(&P)", shortcut: "Ctrl+V", editing: true, enabled: window.entityEditEnabled && ClipboardManager.hasContent, onTriggered: function() {editorUnit.pasteEntitiesFromClipboard()}},
                    {id: "delete", icon: "qrc:/main_icons/assets/icons/delete.svg", text: "删除(&D)", shortcut: "Delete", editing: true, enabled: window.entityEditEnabled && editorUnit.selectedCount > 0, onTriggered: function() {editorUnit.deleteSelectedEntities()}},
                    {separator: true},
                    {id: "selectAll", text: "全选实体(&A)", shortcut: "Ctrl+A", editing: true, enabled: window.entityEditEnabled && EditorManager.count > 0, onTriggered: function() {editorUnit.selectAllEntities()}},
                    {id: "clearSelection", text: "取消实体选择", enabled: window.entityEditEnabled && editorUnit.selectedCount > 0, onTriggered: function() {editorUnit.clearEntitySelection()}}
                ]},
                {title: "场景(&S)", items: [
                    {id: "presets", icon: "qrc:/main_icons/assets/icons/new_scene.svg", text: "添加预设", enabled: editorUnit.visible, submenu: EditorManager.availablePresets().map(function(entry) {
                        return {id: "preset_" + entry.id, text: entry.name, onTriggered: function() {EditorManager.addPreset(entry.id)}}
                    })},
                    {id: "controlCode", icon: "qrc:/main_icons/assets/icons/edit.svg", text: "控制程序…(&C)", shortcut: "Ctrl+Alt+C", enabled: editorUnit.visible, onTriggered: function() {editorUnit.editControlCode()}},
                    {separator: true},
                    {id: "propertiesPanel", icon: "qrc:/main_icons/assets/icons/edit.svg", text: "对象属性面板", enabled: editorUnit.visible, checkable: true, actionGroup: "editorTool", checked: editorUnit.rightTool === "properties", onTriggered: function() {editorUnit.rightTool = "properties"}},
                    {id: "solverPanel", icon: "qrc:/main_icons/assets/icons/settings.svg", text: "求解设置面板", enabled: editorUnit.visible, checkable: true, actionGroup: "editorTool", checked: editorUnit.rightTool === "solver", onTriggered: function() {editorUnit.rightTool = "solver"}}
                ]},
                {title: "运行(&R)", items: [
                    {id: "run", icon: "qrc:/main_icons/assets/icons/" + ((playerUnit.visible && playerSceneController.playbackPlaying) || (simulatorUnit.visible && sceneController.running) ? "pause.svg" : "play.svg"), text: editorUnit.visible ? "开始求解" : (playerUnit.visible ? (playerSceneController.playbackPlaying ? "暂停回放" : "播放回放") : "运行／暂停实时仿真"), shortcut: "F5", enabled: (editorUnit.visible && EditorManager.count > 0 && !simulationExporter.running) || (playerUnit.visible && playerSceneController.simulationLoaded) || simulatorUnit.visible, onTriggered: function() {
                        if (editorUnit.visible) editorUnit.startSolve()
                        else if (playerUnit.visible) playerSceneController.toggleSimulationPlayback()
                        else sceneController.toggleRunning()
                    }},
                    {id: "stop", icon: "qrc:/main_icons/assets/icons/stop.svg", text: editorUnit.visible ? "取消求解" : "停止并回到起点", shortcut: "Shift+F5", enabled: (editorUnit.visible && simulationExporter.running && !simulationExporter.cancelRequested) || (playerUnit.visible && playerSceneController.simulationLoaded), onTriggered: function() {
                        if (editorUnit.visible) simulationExporter.cancel()
                        else {playerSceneController.pauseSimulation();playerSceneController.setPlaybackFrame(0)}
                    }},
                    {separator: true},
                    {id: "previousFrame", text: "上一帧", shortcut: "Alt+Left", enabled: playerUnit.visible && playerSceneController.simulationLoaded && playerSceneController.playbackFrame > 0, onTriggered: function() {playerSceneController.pauseSimulation();playerSceneController.stepPlaybackFrame(-1)}},
                    {id: "nextFrame", text: "下一帧", shortcut: "Alt+Right", enabled: playerUnit.visible && playerSceneController.simulationLoaded && playerSceneController.playbackFrame + 1 < playerSceneController.simulationFrameCount, onTriggered: function() {playerSceneController.pauseSimulation();playerSceneController.stepPlaybackFrame(1)}}
                ]},
                {title: "视图(&V)", items: [
                    {text: "工作区", icon: "qrc:/main_icons/assets/icons/view.svg", submenu: [
                        {id: "workspaceSolver", icon: "qrc:/main_icons/assets/icons/edit.svg", text: "编辑与求解", shortcut: "Ctrl+1", checkable: true, actionGroup: "workspace", checked: editorUnit.visible, onTriggered: function() {mainRowLayout.currentRoute = "solver"}},
                        {id: "workspacePlayer", icon: "qrc:/main_icons/assets/icons/play.svg", text: "播放器", shortcut: "Ctrl+2", checkable: true, actionGroup: "workspace", checked: playerUnit.visible, onTriggered: function() {mainRowLayout.currentRoute = "player"}},
                        {id: "workspaceAnalysis", icon: "qrc:/main_icons/assets/icons/view.svg", text: "数据分析", shortcut: "Ctrl+3", checkable: true, actionGroup: "workspace", checked: analysisUnitLoader.visible, onTriggered: function() {mainRowLayout.currentRoute = "analysis"}},
                        {id: "workspaceRealtime", icon: "qrc:/main_icons/assets/icons/simulator.svg", text: "实时仿真（旧版）", shortcut: "Ctrl+4", checkable: true, actionGroup: "workspace", checked: simulatorUnit.visible, onTriggered: function() {mainRowLayout.currentRoute = "scene"}}
                    ]},
                    {separator: true},
                    {id: "fitView", icon: "qrc:/main_icons/assets/icons/view.svg", text: "适配当前视图", shortcut: "Home", editing: true, enabled: !window.editingText, onTriggered: function() {window.fitWorkspace()}},
                    {id: "grid", text: "显示网格(&G)", shortcut: "Ctrl+G", checkable: true, enabled: window.activeCoordinates !== null, checked: window.activeCoordinates ? window.activeCoordinates.gridVisible : false, onTriggered: function() {window.activeCoordinates.gridVisible = this.checked}},
                    {id: "axes", text: "显示坐标轴(&A)", shortcut: "Ctrl+H", checkable: true, enabled: window.activeCoordinates !== null, checked: window.activeCoordinates ? window.activeCoordinates.axisVisible : false, onTriggered: function() {window.activeCoordinates.axisVisible = this.checked}},
                    {separator: true},
                    {text: "实时仿真面板", icon: "qrc:/main_icons/assets/icons/simulator.svg", enabled: simulatorUnit.visible, submenu: [
                        {id: "dataStream", icon: "qrc:/main_icons/assets/icons/view.svg", text: "数据流", shortcut: "Ctrl+Alt+D", checkable: true, checked: simulatorUnit.dataStreamVisible, onTriggered: function() {simulatorUnit.dataStreamVisible = this.checked}},
                        {id: "console", text: "控制台", shortcut: "Ctrl+Alt+I", checkable: true, checked: simulatorUnit.infoPanelVisible, onTriggered: function() {simulatorUnit.infoPanelVisible = this.checked}}
                    ]}
                ]},
                {title: "帮助(&H)", items: [
                    {id: "shortcuts", icon: "qrc:/main_icons/assets/icons/help.svg", text: "快捷键与菜单说明", shortcut: "F1", onTriggered: function() {shortcutHelpDialog.open()}},
                    {id: "about", icon: "qrc:/main_icons/assets/icons/about.svg", text: "关于 Egret Physics", onTriggered: function() {aboutDialog.open()}},
                    {id: "aboutQt", icon: "qrc:/main_icons/assets/icons/qt_logo.png", text: "关于 Qt", onTriggered: function() {qtHelper.showAboutQt()}}
                ]}
            ]
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            radius: theme.radiusS
            color: "#11325ECC"
            border.width: 1
            border.color: theme.border

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 4

                Image {
                    id: image
                    source: "qrc:/app_icon/assets/favicon/favicon_64.png"
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    fillMode: Image.PreserveAspectFit  // 按比例缩放，完整显示
                }

                Label {
                    text: "EGRET PHYSICS"
                    color: theme.textPrimary
                    font.pixelSize: 24
                    font.bold: true
                }

                Label {
                    text: "CLASSICAL MECHANICS STUDIO"
                    color: theme.textSecondary
                    font.pixelSize: 12
                    font.letterSpacing: 2
                }

                Item {
                    Layout.fillWidth: true
                }

                // 工作区选择框：复用统一样式的 EgretComboBox（components/EgretComboBox.qml）
                EgretComboBox {
                    id: workspaceComboBox
                    Layout.preferredWidth: 210
                    Layout.preferredHeight: 36
                    model: workspaceRouteModel
                    textRole: "label"
                    badgeRole: "badge"
                    currentIndex: routeIndexFor(mainRowLayout.currentRoute)

                    onActivated: function (index) {
                        mainRowLayout.currentRoute = workspaceRouteModel.get(index).route;
                    }
                }
            }
        }

        RowLayout {
            id: mainRowLayout
            Layout.fillWidth: true
            Layout.fillHeight: true

            // 默认进入编辑与求解工作区；由命令行 --play / --edit 启动时直接落在
            // 播放器 / 场景编辑器工作区。
            property string currentRoute: (typeof startupWorkspaceRoute !== "undefined"
                                           && startupWorkspaceRoute.length > 0)
                ? startupWorkspaceRoute
                : "solver"

            /** 上一次工作区，用于在切换时停止其任务。 */
            property string previousRoute: currentRoute
            onCurrentRouteChanged: {
                if (previousRoute === "scene") { sceneController.pause(); sceneController.pauseSimulation(); }
                else if (previousRoute === "solver") simulationExporter.cancel();
                else if (previousRoute === "player") { playerSceneController.pauseSimulation(); videoExporter.cancel(); }
                else if (previousRoute === "analysis") analysisSession.cancel();
                previousRoute = currentRoute;
            }

            // 双击文件启动时按类型分派：
            //   .epsx  → --play <文件>：切到播放器工作区，打开并立即播放；
            //   .epscene / .epscenex → --edit <文件>：切到场景编辑器并载入场景。
            Component.onCompleted: {
                // 启动时也暂停未显示工作区的默认计算，避免后台持续运行。
                if (currentRoute !== "scene") sceneController.pause();
                if (currentRoute !== "player") playerSceneController.pauseSimulation();
                if (typeof startupSceneFile !== "undefined"
                        && startupSceneFile.length > 0) {
                    currentRoute = "solver";
                    EditorManager.openDocument(startupSceneFile);
                    editorUnit.resetInspector();
                    return;
                }
                if (typeof startupSimulationFile === "undefined"
                        || startupSimulationFile.length === 0) {
                    return;
                }
                currentRoute = "player";
                // 与数据分析工作区共享同一份文件：先写入共享状态，
                // 播放器由上面的 Connections 自动加载。
                if (window.openEpsx(startupSimulationFile)) {
                    playerSceneController.playSimulation();
                }
            }

            spacing: 6

            SimulatorUnit {
                id: simulatorUnit
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: mainRowLayout.currentRoute === "scene"
                theme: theme
            }

            EditorUnit {
                id: editorUnit
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: mainRowLayout.currentRoute === "solver"
                onCloseDocumentRequested: function(index) { window.requestCloseDocument(index); }
            }

            PlayerUnit {
                id: playerUnit
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: mainRowLayout.currentRoute === "player"
                theme: theme
                onEpsxOpenRequested: function (filePath) {
                    window.openEpsx(filePath);
                }
            }

            // 数据分析工作区按需加载：即使该工作区自身出错，也不会影响其它工作区。
            Loader {
                id: analysisUnitLoader
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: mainRowLayout.currentRoute === "analysis"
                active: mainRowLayout.currentRoute === "analysis"
                source: "qrc:/units/units/AnalysisUnit.qml"

                onLoaded: {
                    item.theme = theme;
                    item.epsxOpenRequested.connect(function (filePath) {
                        window.openEpsx(filePath);
                    });
                    item.epsxCloseRequested.connect(function () {
                        window.closeEpsx();
                    });
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 20
            radius: theme.radiusS
            color: "#102B4B"
            border.width: 1
            border.color: "#325E89"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12

                Label {
                    text: "就绪"
                    color: theme.textSecondary
                    font.pixelSize: 12
                }

                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                    color: theme.textDim
                    font.pixelSize: 12
                    text: window.currentSimulationPath.length > 0
                          ? "EPSX：" + window.currentSimulationPath
                          : ""
                }

                Label {
                    text: workspaceComboBox.currentText
                    color: theme.textDim
                    font.pixelSize: 12
                }
            }
        }
    }
}
