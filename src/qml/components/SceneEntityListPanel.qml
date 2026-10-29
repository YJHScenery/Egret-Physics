/**
 * @file        SceneEntityListPanel.qml
 * @brief       场景实体列表面板组件，分三栏显示实体、力场、约束器列表。
 * @details     在编辑器左侧显示当前场景中所有实例的分类列表，
 *              每项显示名称、类型和可见性状态。动态响应 EditorManager 变化。
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
import EditorManager 1.0
import EnumHandler 1.0

import "qrc:/components/components"
import "qrc:/components/components/theme"

// ── 场景实体列表面板 (左侧三栏) ──
ColumnLayout {
    id: root

    spacing: 6

    // ── 分类常量 ──
    readonly property int categoryShape: 0
    readonly property int categoryField: 1
    readonly property int categoryConstraint: 2

    // ── 各分类的模型列表（C++ 侧过滤，性能优于 QML JS 循环） ──
    property var shapeModels: []
    property var fieldModels: []
    property var constraintModels: []

    // ── 实体选择模式（约束器创建时使用） ──
    property bool selectionMode: false
    property var selectedEntityIds: []    // 选中的实体 ID 列表
    property int minSelection: 1
    property int maxSelection: 1

    DeepBlueTheme {
        id: theme
    }

    signal selectionChanged(var entityIds)

    /**
     * @brief 单击实体时发出（非约束选择模式），供 EditorUnit 触发 3D 视口高亮拾取。
     * @param entityId 实体 ID 字符串。
     */
    signal entitySingleClicked(string entityId)

    /**
     * @brief 双击实体时发出，供 EditorUnit 将实体加载到属性面板进入修改模式。
     * @param entityId 实体 ID 字符串。
     */
    signal entityDoubleClicked(string entityId)

    /**
     * @brief 切换实体选择状态。
     * @param entityId 实体 ID 字符串。
     * @param ctrlHeld 是否按住 Ctrl（多选模式）。
     */
    function toggleEntitySelection(entityId, ctrlHeld) {
        var ids = selectedEntityIds.slice();

        if (selectionMode) {
            // 约束创建模式：保留原有 min/maxSelection 逻辑
            if (!ctrlHeld) {
                ids = [entityId];
            } else {
                var idx = ids.indexOf(entityId);
                if (idx >= 0) {
                    ids.splice(idx, 1);
                } else {
                    if (ids.length >= maxSelection) return;
                    ids.push(entityId);
                }
            }
        } else {
            // 通用选择模式：单击单选，Ctrl+单击多选
            var idx = ids.indexOf(entityId);
            if (ctrlHeld) {
                if (idx >= 0) {
                    ids.splice(idx, 1);
                } else {
                    ids.push(entityId);
                }
            } else {
                // 无 Ctrl：如果已选中该项则清除，否则单选该项
                if (ids.length === 1 && ids[0] === entityId) {
                    ids = [];
                } else {
                    ids = [entityId];
                }
            }
        }

        selectedEntityIds = ids;
        selectionChanged(ids);
    }

    /**
     * @brief 设置仅选中指定 ID（用于同步 3D 视口 pick）。
     * @param entityId 实体 ID。
     */
    function setSingleSelection(entityId) {
        if (selectionMode) return;
        selectedEntityIds = [entityId];
        selectionChanged([entityId]);
    }

    /**
     * @brief 检查实体是否被选中。
     * @param entityId 实体 ID 字符串。
     */
    function isEntitySelected(entityId) {
        return selectedEntityIds.indexOf(entityId) >= 0;
    }

    /**
     * @brief 清空所有选中。
     */
    function clearSelection() {
        if (selectedEntityIds.length > 0) {
            selectedEntityIds = [];
            selectionChanged([]);
        }
    }

    // ── 刷新所有分类列表 ──
    function refreshAll() {
        shapeModels = EditorManager.getModelsByCategory(categoryShape);
        fieldModels = EditorManager.getModelsByCategory(categoryField);
        constraintModels = EditorManager.getModelsByCategory(categoryConstraint);
    }

    // ── 根据类别解析类型名称 ──
    function resolveTypeName(modelData) {
        // 可直接引用枚举值进行比较（无需函数调用）
        const type = modelData.type;
        switch (modelData.category){
            case 0:{
                // if (type === ShapeType.Box)             return qsTr("盒体");
                // if (type === ShapeType.Sphere)          return qsTr("球体");
                // if (type === ShapeType.Cylinder)        return qsTr("圆柱");
                // if (type === ShapeType.Rod)             return qsTr("杆");
                return EnumHandler.shapeTypeToString(type); // fallback
            }
            case 1: {
                // if (type === FieldType.Gravity)    return qsTr("重力场");
                // if (type === FieldType.Gravitation) return qsTr("引力场");
                return EnumHandler.fieldTypeToString(type);   // fallback
            }
            case 2: {
                // if (type === ConstraintType.Spring) return qsTr("弹簧");
                return EnumHandler.constraintTypeToString(type);
            }
            default: {
                return qsTr("Unknown");
            }
        }

    }

    Component.onCompleted: {
        refreshAll();
    }

    Connections {
        target: EditorManager

        function onModelListChanged() {
            root.refreshAll();
        }
    }


    // 实体列表 (Shape)

    GlassPanel {
        id: shapePanel
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 80
        Layout.preferredHeight: Math.min(80 + shapeModels.length * 18, 400)
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
                    radius: theme.radiusS
                }
            }

            Column {
                id: shapeColumn
                width: parent.width
                
                spacing: 2

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

    // 力场列表 (Field)
    GlassPanel {
        id: fieldPanel
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 80
        Layout.preferredHeight: Math.min(80 + fieldModels.length * 18, 400)
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
                spacing: 2

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

    // 约束器列表 (Constraint)
    GlassPanel {
        id: constraintPanel
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 80
        Layout.preferredHeight: Math.min(80 + constraintModels.length * 18, 400)
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
                spacing: 2

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

    // 列表项代理组件
    Component {
        id: listItemDelegate

        Rectangle {
            id: itemRect
            width: parent ? parent.width : 200
            height: 28
            radius: 6

            // ── 高亮（选择模式或通用选择） ──
            readonly property bool isSelected: root.isEntitySelected(modelData.id)

            color: isSelected ? "#1A4D8C" : "#0D2748"
            border.width: isSelected ? 2 : 1
            border.color: isSelected ? "#3F9FFF" : "#1E4A7A"

            // ── 单击 / 双击 选择区域 ──
            MouseArea {
                id: itemMouse
                anchors.fill: parent
                enabled: true
                cursorShape: Qt.PointingHandCursor

                // 双击检测状态 — 防止双击时重复触发单击逻辑
                property bool doubleClickFired: false

                Timer {
                    id: singleClickTimer
                    interval: 300
                    repeat: false
                    onTriggered: {
                        if (!itemMouse.doubleClickFired) {
                            // 非约束模式时通知 EditorUnit 进行 3D 视口高亮拾取
                            root.entitySingleClicked(modelData.id);
                        }
                        itemMouse.doubleClickFired = false;
                    }
                }

                onClicked: function(mouse) {
                    var ctrlHeld = mouse.modifiers & Qt.ControlModifier;
                    root.toggleEntitySelection(modelData.id, ctrlHeld);

                    // 启动单击计时器：若 300ms 内无双击则视为单击
                    if (!doubleClickFired) {
                        singleClickTimer.start();
                    }
                }

                onDoubleClicked: function(mouse) {
                    singleClickTimer.stop();
                    doubleClickFired = true;
                    root.entityDoubleClicked(modelData.id);
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 7
                anchors.rightMargin: 5
                spacing: 6

                // ── 选择勾选标记（选中时显示勾号，通用选择时显示可点选框） ──
                Rectangle {
                    width: 10
                    height: 10
                    radius: 2
                    color: itemRect.isSelected ? "#3F9FFF" : "#1A3050"
                    border.width: !itemRect.isSelected ? 1 : 0
                    border.color: "#3F9FFF"

                    // 选中勾号
                    Text {
                        anchors.centerIn: parent
                        text: itemRect.isSelected ? "✓" : ""
                        color: "#FFFFFF"
                        font.pixelSize: 8
                        font.bold: true
                    }
                }

                // ── 名称 ──
                Label {
                    text: modelData.name ? modelData.name : ("未命名-" + index)
                    color: itemRect.isSelected ? "#FFFFFF" : "#D4EAFB"
                    font.pixelSize: 11
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
                        font.pixelSize: 10
                    }
                }
            }
        }
    }
}
