/**
 * @file        SceneEntityListPanel.qml
 * @brief       场景实体列表面板组件，分三栏显示实体、力场、约束器列表。
 * @details     在编辑器左侧显示当前场景中所有实例的分类列表，
 *              每项显示名称、类型和可见性状态。动态响应 ModelManager 变化。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-29
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
import QtQuick.Controls.Basic 6.9 as Basic
import QtQuick.Layouts 1.15
import ModelManager 1.0
import EnumHandler 1.0

import "qrc:/components/components"
import "qrc:/components/components/theme"

// ── 场景实体列表面板 (左侧三栏) ──

ColumnLayout {
    id: root

    spacing: 12

    // ── 分类常量 ──
    readonly property int categoryShape: 0
    readonly property int categoryField: 1
    readonly property int categoryConstraint: 2

    // ── 各分类的模型列表（C++ 侧过滤，性能优于 QML JS 循环） ──
    property var shapeModels: []
    property var fieldModels: []
    property var constraintModels: []

    // ── 刷新所有分类列表 ──
    function refreshAll() {
        shapeModels = ModelManager.getModelsByCategory(categoryShape);
        fieldModels = ModelManager.getModelsByCategory(categoryField);
        constraintModels = ModelManager.getModelsByCategory(categoryConstraint);
    }

    // ── 根据类别解析类型名称 ──
    function resolveTypeName(modelData) {
        switch (modelData.category) {
            case 1: return EnumHandler.fieldTypeToString(modelData.type);
            case 2: return EnumHandler.constraintTypeToString(modelData.type);
            default: return EnumHandler.shapeToString(modelData.type);
        }
    }

    Component.onCompleted: {
        refreshAll();
    }

    Connections {
        target: ModelManager

        function onModelListChanged() {
            root.refreshAll();
        }
    }

    // ═══════════════════════════════════════════
    // 实体列表 (Shape)
    // ═══════════════════════════════════════════
    GlassPanel {
        id: shapePanel
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 80
        Layout.preferredHeight: Math.min(80 + shapeModels.length * 26, 400)
        title: "实体列表"

        ScrollView {
            id: shapeScrollView
            anchors.fill: parent
            anchors.margins: 6
            contentWidth: shapeScrollView.availableWidth
            clip: true

            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            rightPadding: shapeScrollBar.visible ? shapeScrollBar.width : 0

            ScrollBar.vertical: Basic.ScrollBar {
                id: shapeScrollBar
                width: 8
                policy: ScrollBar.AsNeeded
                visible: size < 1.0
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                minimumSize: 0.2
                background: Rectangle {
                    color: "#0D2748"
                }
                contentItem: Rectangle {
                    color: "#3F79B0"
                    radius: 4
                }
            }

            Column {
                id: shapeColumn
                width: parent.width
                
                spacing: 4

                Repeater {
                    model: root.shapeModels

                    delegate: listItemDelegate
                }

                // 空状态提示
                Label {
                    visible: root.shapeModels.length === 0
                    text: "（空）"
                    color: "#5A7FA0"
                    font.pixelSize: 12
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }
    }

    // ═══════════════════════════════════════════
    // 力场列表 (Field)
    // ═══════════════════════════════════════════
    GlassPanel {
        id: fieldPanel
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 80
        Layout.preferredHeight: Math.min(80 + fieldModels.length * 26, 400)
        title: "场列表"

        ScrollView {
            id: fieldScrollView
            anchors.fill: parent
            anchors.margins: 6
            contentWidth: fieldScrollView.availableWidth
            clip: true

            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            rightPadding: fieldScrollBar.visible ? fieldScrollBar.width : 0

            ScrollBar.vertical: Basic.ScrollBar {
                id: fieldScrollBar
                width: 8
                policy: ScrollBar.AsNeeded
                visible: size < 1.0
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                minimumSize: 0.2
                background: Rectangle {
                    color: "#0D2748"
                }
                contentItem: Rectangle {
                    color: "#3F79B0"
                    radius: 4
                }
            }

            Column {
                id: fieldColumn
                width: parent.width
                spacing: 4

                Repeater {
                    model: root.fieldModels

                    delegate: listItemDelegate
                }

                Label {
                    visible: root.fieldModels.length === 0
                    text: "（空）"
                    color: "#5A7FA0"
                    font.pixelSize: 12
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }
    }

    // ═══════════════════════════════════════════
    // 约束器列表 (Constraint)
    // ═══════════════════════════════════════════
    GlassPanel {
        id: constraintPanel
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 80
        Layout.preferredHeight: Math.min(80 + constraintModels.length * 26, 400)
        title: "约束器列表"

        ScrollView {
            id: constraintScrollView
            anchors.fill: parent
            anchors.margins: 6
            contentWidth: constraintScrollView.availableWidth
            clip: true

            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            rightPadding: constraintScrollBar.visible ? constraintScrollBar.width : 0

            ScrollBar.vertical: Basic.ScrollBar {
                id: constraintScrollBar
                width: 8
                policy: ScrollBar.AsNeeded
                visible: size < 1.0
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                minimumSize: 0.2
                background: Rectangle {
                    color: "#0D2748"
                }
                contentItem: Rectangle {
                    color: "#3F79B0"
                    radius: 4
                }
            }

            Column {
                id: constraintColumn
                width: parent.width
                spacing: 4

                Repeater {
                    model: root.constraintModels

                    delegate: listItemDelegate
                }

                Label {
                    visible: root.constraintModels.length === 0
                    text: "（空）"
                    color: "#5A7FA0"
                    font.pixelSize: 12
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }
    }

    // ═══════════════════════════════════════════
    // 列表项代理组件
    // ═══════════════════════════════════════════
    Component {
        id: listItemDelegate

        Rectangle {
            id: itemRect
            width: parent ? parent.width : 200
            height: 42
            radius: 6
            color: "#0D2748"
            border.width: 1
            border.color: "#1E4A7A"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 8
                spacing: 8

                // ── 可见性指示器 ──
                Rectangle {
                    width: 10
                    height: 10
                    radius: 5
                    color: modelData.visible ? "#27B5FF" : "#3A5068"
                }

                // ── 名称 ──
                Label {
                    text: modelData.name ? modelData.name : ("未命名-" + index)
                    color: "#D4EAFB"
                    font.pixelSize: 13
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                // ── 类型标签 ──
                Rectangle {
                    radius: 4
                    color: "#102A4A"
                    border.width: 1
                    border.color: "#2A5A8A"
                    implicitWidth: typeLabel.implicitWidth + 12
                    implicitHeight: typeLabel.implicitHeight + 4

                    Label {
                        id: typeLabel
                        anchors.centerIn: parent
                        text: root.resolveTypeName(modelData)
                        color: "#9ABFE3"
                        font.pixelSize: 11
                    }
                }
            }
        }
    }
}
