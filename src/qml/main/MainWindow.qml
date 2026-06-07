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
import QtQuick.Controls.Basic 2.15 as Basic
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Dialogs
import EditorManager 1.0
import QtQuick3D 6.9

import "qrc:/components/components"
import "qrc:/components/components/message_box"
import "qrc:/components/components/theme"
import "qrc:/units/units"

ApplicationWindow {
    id: window

    width: 1440
    height: 1080
    minimumWidth: 1024
    minimumHeight: 768

    visible: true
    title: "Egret Physics - Classical Mechanics Studio"

    DeepBlueTheme {
        id: theme
    }

    ListModel {
        id: workspaceRouteModel
        ListElement {
            label: "仿真器"; route: "scene"
        }
        ListElement {
            label: "场景编辑器"; route: "solver"
        }
        ListElement {
            label: "参数扫描"; route: "sweep"
        }
        ListElement {
            label: "实验教学"; route: "teaching"
        }
        ListElement {
            label: "数据分析"; route: "analysis"
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
        spacing: 8

        FileDialog {
            id: openJsonFileDialog
            title: "打开序列化场景文件"
            fileMode: FileDialog.OpenFile
            currentFolder: documentsFolder
            nameFilters: [
                "序列化场景 (*.json)",
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
                        ModelManager.loadFromJson(localPath);
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
            defaultSuffix: "json"
            nameFilters: [
                "序列化场景 (*.json)",
                "所有文件 (*)"
            ]

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
            id: msgDialog
            level: EgretMessageDialog.Level.Warning
            title: "警告"
            text: "仿真器不支持保存文件！"
            buttons: EgretMessageDialog.StandardButton.Ok
            soundType: "Warning"
        }

        EgretMenuBar {
            Layout.fillWidth: true
            barHeight: 36
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
                                sceneController.reset();
                            },
                            icon: "qrc:/main_icons/assets/icons/new_scene.svg"
                        },
                        {
                            text: "打开...(&O)",
                            shortcut: "Ctrl+O",
                            onTriggered: function () {
                                console.log("Open scene");

                                switch (workspaceComboBox.currentIndex){
                                    case 0:{
                                        openJsonFileDialog.mode = 0;
                                        openJsonFileDialog.open();
                                        break;
                                    }case 1:{
                                        openJsonFileDialog.mode = 1;
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
                                if (workspaceComboBox.currentIndex !== 1){
                                    msgDialog.open();
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
                                console.log("Save as");
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
                            enabled: false,
                            icon: "qrc:/main_icons/assets/icons/undo.svg"
                        },
                        {
                            text: "恢复(&R)",
                            shortcut: "Ctrl+Y",
                            enabled: false,
                            icon: "qrc:/main_icons/assets/icons/recover.svg"
                        },
                        {
                            separator: true
                        },
                        {
                            text: "剪切(&T)",
                            shortcut: "Ctrl+X",
                            icon: "qrc:/main_icons/assets/icons/cut.svg"
                        },
                        {
                            text: "复制(C)",
                            shortcut: "Ctrl+C",
                            icon: "qrc:/main_icons/assets/icons/copy.svg"
                        },
                        {
                            text: "粘贴(P)",
                            shortcut: "Ctrl+V",
                            icon: "qrc:/main_icons/assets/icons/paste.svg"
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
                spacing: 8

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

                Rectangle {
                    id: workspaceComboShell
                    Layout.preferredWidth: 210
                    Layout.preferredHeight: 36
                    radius: 10
                    color: workspaceComboBox.pressed ? "#1E3A8A" : (workspaceComboBox.hovered ? "#153564" : "#0A1A3A")
                    border.width: 1
                    border.color: workspaceComboBox.hovered ? "#3B82F6" : "#1E3A8A"

                    Basic.ComboBox {
                        id: workspaceComboBox
                        anchors.fill: parent
                        model: workspaceRouteModel
                        textRole: "label"
                        leftPadding: 12
                        rightPadding: 28
                        currentIndex: routeIndexFor(mainRowLayout.currentRoute)

                        onActivated: function (index) {
                            mainRowLayout.currentRoute = workspaceRouteModel.get(index).route;
                        }

                        contentItem: Text {
                            text: workspaceComboBox.displayText
                            color: theme.textPrimary
                            font.pixelSize: 13
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }

                        indicator: Canvas {
                            width: 10
                            height: 6
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            contextType: "2d"

                            onPaint: {
                                context.reset();
                                context.fillStyle = "#8BB5EA";
                                context.beginPath();
                                context.moveTo(0, 0);
                                context.lineTo(width, 0);
                                context.lineTo(width * 0.5, height);
                                context.closePath();
                                context.fill();
                            }
                        }

                        background: Item {
                        }

                        popup: Basic.Popup
                        {
                            y: workspaceComboBox.height + 6
                            width: workspaceComboBox.width
                            padding: 6
                            clip: false
                            background: Rectangle {
                                radius: 10
                                color: "#10294C"
                                border.width: 1
                                border.color: theme.border
                            }

                            contentItem: Rectangle {
                                radius: 8
                                color: "transparent"
                                clip: true
                                implicitHeight: Math.min(listView.contentHeight + 4, 240)

                                ListView {
                                    id: listView
                                    anchors.fill: parent
                                    anchors.margins: 2
                                    model: workspaceComboBox.popup.visible ? workspaceComboBox.delegateModel : null
                                    clip: true

                                    ScrollIndicator.vertical: ScrollIndicator {
                                    }
                                }
                            }
                        }

                        delegate: Basic.ItemDelegate
                        {
                            width: listView ? listView.width : workspaceComboBox.width
                            height: 34
                            text: model.label
                            highlighted: workspaceComboBox.highlightedIndex === index
                            contentItem: Text {
                                text: model.label
                                color: highlighted ? "#E7F2FF" : "#9ABFE3"
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                leftPadding: 10
                            }
                            background: Rectangle {
                                radius: 8
                                color: highlighted ? "#1D4E87" : "#102A4A"
                                border.width: 1
                                border.color: highlighted ? "#56B6FF" : "#2F5F8D"
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            id: mainRowLayout
            Layout.fillWidth: true
            Layout.fillHeight: true
            property string currentRoute: "scene";

            spacing: 12

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

                Item {
                    Layout.fillWidth: true
                }

                Label {
                    text: "Solver"
                    color: theme.textDim
                    font.pixelSize: 12
                }
            }
        }
    }
}
