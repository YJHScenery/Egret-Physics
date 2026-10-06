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

    ListModel {
        id: workspaceRouteModel
        ListElement {
            label: "实时仿真"; route: "scene"; badge: "不再维护"
        }
        ListElement {
            label: "离线仿真"; route: "offline"; badge: ""
        }
        ListElement {
            label: "场景编辑器"; route: "solver"; badge: ""
        }
        ListElement {
            label: "播放器"; route: "player"; badge: ""
        }
        ListElement {
            label: "数据分析"; route: "analysis"; badge: ""
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
                "序列化场景 (*.epscene)",
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
                        EditorManager.loadFromJson(localPath);
                        break;
                    }case 2: {
                        // 离线仿真工作区：场景加载到它自己的世界里（即求解初始状态）
                        offlineSceneController.loadSceneFronJsonFile(localPath);
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
            defaultSuffix: "epscene"
            nameFilters: [
                "序列化场景 (*.epscene)",
                "所有文件 (*)"
            ]

            // currentFile: "Untitled.epscene"

            onAccepted: {
                // 注意：selectedFile 返回的是 file:// 开头的 URL，需转换为本地路径
                var fileUrl = selectedFile
                var localPath = fileUrl.toString().replace(/^file:\/\/\//, "")  // 简单转换
                EditorManager.saveToJson(localPath)
            }

            onRejected: {
                console.log("用户取消保存")
            }
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
                {
                    title: "文件(&F)",
                    items: [
                        {
                            text: "新建(&N)",
                            shortcut: "Ctrl+N",
                            onTriggered: function () {
                                mainRowLayout.currentRoute = "solver"
                                sceneController.reset();
                            },
                            icon: "qrc:/main_icons/assets/icons/new_scene.svg"
                        },
                        {
                            text: "打开...(&O)",
                            shortcut: "Ctrl+O",
                            onTriggered: function () {
                                console.log("Open scene");

                                // 按「路由」而不是 Combo 索引分派：Combo 项一旦增减，
                                // 索引就会错位（离线仿真曾被误派给编辑器）。
                                switch (mainRowLayout.currentRoute){
                                    case "player":
                                    case "analysis":{
                                        // 播放器与数据分析用 epsx，而不是场景 JSON
                                        openEpsxFileDialog.open();
                                        break;
                                    }
                                    case "solver":{
                                        openJsonFileDialog.mode = 1;   // 场景编辑器
                                        openJsonFileDialog.open();
                                        break;
                                    }case "offline":{
                                        openJsonFileDialog.mode = 2;   // 离线仿真
                                        openJsonFileDialog.open();
                                        break;
                                    }default:{
                                        openJsonFileDialog.mode = 0;   // 实时仿真（默认）
                                        openJsonFileDialog.open();
                                        break;
                                    }
                                }
                            },
                            icon: "qrc:/main_icons/assets/icons/open.svg"
                        },
                        {
                            text: "保存(&S)",
                            shortcut: "Ctrl+S",
                            onTriggered: function () {
                                if (mainRowLayout.currentRoute !== "solver"){
                                    mainMsgDialog.title = "警告"
                                    mainMsgDialog.text = "仿真器不支持保存文件！"
                                    mainMsgDialog.open();
                                    return;
                                }

                                saveJsonFileDialog.open()
                                console.log("Save");
                            },
                            icon: "qrc:/main_icons/assets/icons/save.svg"
                        },
                        {
                            text: "另存为...(&A)",
                            shortcut: "Ctrl+Shift+S",
                            onTriggered: function () {
                                if (mainRowLayout.currentRoute !== "solver"){
                                    mainMsgDialog.title = "警告"
                                    mainMsgDialog.text = "仿真器不支持保存文件！"
                                    mainMsgDialog.open();
                                    return;
                                }

                                saveJsonFileDialog.open()
                                console.log("Save");
                            },
                            icon: "qrc:/main_icons/assets/icons/save_as.svg"
                        },
                        {
                            separator: true
                        },
                        {
                            text: "偏好设置(&P)",
                            shortcut: "Ctrl+P",
                            onTriggered: function () {
                                console.log("Preference");
                            },
                            // icon: "qrc:/main_icons/assets/icons/settings.svg"
                            icon: "qrc:/main_icons/assets/icons/settings.svg",
                        },
                        {
                            separator: true
                        },
                        {
                            text: "退出(&Q)",
                            shortcut: "Alt+F4",
                            onTriggered: function () {
                                Qt.quit();
                            },
                            icon: "qrc:/main_icons/assets/icons/exit.svg"
                        }

                    ]
                },
                {
                    title: "编辑(&E)",
                    items: [
                        {
                            text: "撤销(&U)",
                            shortcut: "Ctrl+Z",
                            enabled: editorUnit.visible,
                            icon: "qrc:/main_icons/assets/icons/undo.svg",
                            onTriggered: function () {
                                EditorManager.undo();
                            }
                        },
                        {
                            text: "恢复(&R)",
                            shortcut: "Ctrl+Y",
                            enabled: editorUnit.visible,
                            icon: "qrc:/main_icons/assets/icons/recover.svg",
                            onTriggered: function () {
                                EditorManager.redo();
                            }
                        },
                        {
                            separator: true
                        },
                        {
                            text: "剪切(&T)",
                            shortcut: "Ctrl+X",
                            enabled: editorUnit.visible,
                            icon: "qrc:/main_icons/assets/icons/cut.svg",
                            onTriggered: function () {
                                editorUnit.cutSelectedEntities();
                            }
                        },
                        {
                            text: "复制(C)",
                            shortcut: "Ctrl+C",
                            enabled: editorUnit.visible,
                            icon: "qrc:/main_icons/assets/icons/copy.svg",
                            onTriggered: function () {
                                editorUnit.copySelectedEntities();
                            }
                        },
                        {
                            text: "粘贴(P)",
                            shortcut: "Ctrl+V",
                            enabled: editorUnit.visible,
                            icon: "qrc:/main_icons/assets/icons/paste.svg",
                            onTriggered: function () {
                                editorUnit.pasteEntitiesFromClipboard();
                            }
                        },
                        {
                            separator: true
                        },
                        {
                            text: "删除(&D)",
                            shortcut: "Delete",
                            enabled: editorUnit.visible,
                            icon: "qrc:/main_icons/assets/icons/delete.svg",
                            onTriggered: function () {
                                editorUnit.deleteSelectedEntities();
                            }
                        }
                    ]
                },
                {
                    title: "视图(&V)",
                    items: [
                        {
                            separator: true
                        },
                        {
                            text: "仿真器",
                            icon: "qrc:/main_icons/assets/icons/simulator.svg",
                            submenu: [
                                {
                                    text: "场景",
                                    icon: "qrc:/main_icons/assets/icons/view.svg",
                                    submenu: [
                                        {
                                            text: "网格(&G)",
                                            shortcut: "Ctrl+G",
                                            checkable: true,
                                            checked: true,
                                            onTriggered: function () {
                                                console.log("Toggle grid");
                                                simulatorUnit.coordinateSystem.gridOn(this.checked)
                                            }
                                        },
                                        {
                                            text: "坐标轴(&A)",
                                            shortcut: "Ctrl+H",
                                            checkable: true,
                                            checked: true,
                                            onTriggered: function () {
                                                console.log("Toggle axes");
                                                simulatorUnit.coordinateSystem.axisOn(this.checked)
                                            }
                                        }
                                    ]
                                },
                                {
                                    separator: true
                                },
                                {
                                    text: "数据流(&D)",
                                    shortcut: "Ctrl+Alt+D",
                                    checkable: true,
                                    checked: true,
                                    onTriggered: function () {
                                        console.log("数据流");
                                        simulatorUnit.dataStreamVisible = this.checked === true
                                    }
                                },
                                {
                                    text: "控制台(&I)",
                                    shortcut: "Ctrl+Alt+I",
                                    checkable: true,
                                    checked: true,
                                    onTriggered: function () {
                                        simulatorUnit.infoPanelVisible = this.checked === true
                                    }
                                }
                            ]
                        },

                    ]
                },
                {
                    title: "帮助(&H)",
                    items: [
                        {
                            text: "关于(&A)",
                            shortcut: "F1",
                            onTriggered: function () {
                                aboutDialog.open();
                            },
                            icon: "qrc:/main_icons/assets/icons/about.svg"
                        },
                        {
                            text: "关于Qt(&Q)",
                            shortcut: "Shift+Alt+Q",
                            onTriggered: function () {
                                qtHelper.showAboutQt();
                            },
                            icon: "qrc:/main_icons/assets/icons/qt_logo.png"
                        }
                    ]
                }
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

            // 默认进入实时仿真工作区；由命令行 --play / --edit 启动时直接落在
            // 播放器 / 场景编辑器工作区。
            property string currentRoute: (typeof startupWorkspaceRoute !== "undefined"
                                           && startupWorkspaceRoute.length > 0)
                ? startupWorkspaceRoute
                : "scene"

            // 双击文件启动时按类型分派：
            //   .epsx  → --play <文件>：切到播放器工作区，打开并立即播放；
            //   .epscene → --edit <文件>：切到场景编辑器并载入场景。
            Component.onCompleted: {
                if (typeof startupSceneFile !== "undefined"
                        && startupSceneFile.length > 0) {
                    currentRoute = "solver";
                    EditorManager.loadFromJson(startupSceneFile);
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

            OfflineSimulationUnit {
                id: offlineSimulationUnit
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: mainRowLayout.currentRoute === "offline"
                theme: theme
            }

            EditorUnit {
                id: editorUnit
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: mainRowLayout.currentRoute === "solver"
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
