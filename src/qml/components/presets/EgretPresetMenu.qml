/**
 * @file        EgretPresetMenu.qml
 * @brief       预设场景弹出菜单组件
 * @details     以主题化弹出面板列出 EditorManager 提供的全部内置预设场景，
 *              每个条目显示名称、分类与说明文字，点击任意条目即把该预设叠加到
 *              当前文档。组件自身从 EditorManager 读取预设清单并调用
 *              addPreset()，因此调用方只需要 openAt() 即可，无需额外绑定逻辑。
 *
 *              使用方式：
 *              @code
 *              EgretPresetMenu { id: presetMenu }
 *              // 在某个按钮的点击回调中：
 *              var pos = button.mapToItem(parentItem, 0, button.height + 6);
 *              presetMenu.openAt(pos.x, pos.y);
 *              @endcode
 *
 * @date        2026-11-07
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     View
 * @defgroup    预设场景
 */

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Basic 2.15 as Basic
import QtQuick.Layouts 1.15
import EditorManager 1.0
import "qrc:/components/components/theme"

Basic.Popup {
    id: root

    /** 单个预设条目高度（像素）。 */
    readonly property int itemHeight: 64
    /** 清单最大可见高度（像素），超出后内部滚动。 */
    readonly property int maxListHeight: parent ? Math.max(itemHeight, Math.min(448, parent.height - 110)) : 448
    /** 预设清单，每项包含 id / name / category / description。打开时自动刷新。 */
    property var presets: []

    DeepBlueTheme {
        id: theme
    }

    modal: false
    focus: true
    padding: 0
    width: 380
    height: 46 + 1 + Math.min(presetList.contentHeight, maxListHeight) + 30
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: theme.bg1
        border.width: 1
        border.color: theme.border
        radius: theme.radiusM
    }

    /**
     * @brief       刷新预设清单。
     * @details     从 EditorManager 读取当前全部内置预设；预设清单由 C++ 侧维护，
     *              QML 不重复声明，新增预设只需在 resources/presets.qrc 与
     *              EditorManager::availablePresets() 中登记。
     */
    function reloadPresets() {
        presets = EditorManager.availablePresets();
    }

    /**
     * @brief       在指定坐标处打开预设菜单。
     * @param px 弹出位置 x（相对 Popup 父级坐标系）。
     * @param py 弹出位置 y（相对 Popup 父级坐标系）。
     */
    function openAt(px, py) {
        reloadPresets();
        // 用隐式尺寸把弹窗收拢在父级（窗口）可视区域内，避免超出右/下边界
        var menuWidth = width > 0 ? width : implicitWidth;
        var menuHeight = height > 0 ? height : implicitHeight;
        var boundsWidth = parent ? parent.width : 0;
        var boundsHeight = parent ? parent.height : 0;
        x = boundsWidth > 0 ? Math.max(8, Math.min(px, boundsWidth - menuWidth - 8)) : px;
        y = boundsHeight > 0 ? Math.max(8, Math.min(py, boundsHeight - menuHeight - 8)) : py;
        open();
    }

    /**
     * @brief       触发指定预设：叠加到当前场景并关闭菜单。
     * @param entry 预设描述对象（presets 中的一项）。
     */
    function triggerPreset(entry) {
        if (!entry || !entry.id)
            return;
        // 添加成功才关闭菜单；失败时保持打开以便用户另选
        if (EditorManager.addPreset(entry.id))
            close();
    }

    contentItem: ColumnLayout {
        spacing: 0
        implicitWidth: 380
        implicitHeight: 46 + 1 + Math.min(presetList.contentHeight, root.maxListHeight) + 30

        // ── 标题栏 ──
        Item {
            Layout.fillWidth: true
            implicitHeight: 46

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                text: "添加预设场景"
                color: theme.textPrimary
                font.pixelSize: 14
                font.bold: true
            }

            Text {
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                text: "点击即叠加"
                color: theme.textSecondary
                font.pixelSize: 11
            }
        }

        // ── 分隔线 ──
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: theme.border
            opacity: 0.45
        }

        // ── 预设列表 ──
        ListView {
            id: presetList
            Layout.fillWidth: true
            implicitHeight: Math.min(contentHeight, root.maxListHeight)
            model: root.presets
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > root.maxListHeight
            ScrollBar.vertical: Basic.ScrollBar {
                contentItem: Rectangle {implicitWidth: 5;implicitHeight: 20;radius: 3;color: theme.border}
            }

            delegate: Rectangle {
                id: presetItem

                /** 当前预设描述对象。 */
                property var entry: modelData

                width: presetList.width
                height: root.itemHeight
                radius: theme.radiusS
                color: presetMouse.containsMouse ? theme.bg2 : "transparent"

                Item {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    anchors.topMargin: 9
                    anchors.bottomMargin: 7

                    Text {
                        id: presetNameText
                        text: presetItem.entry ? presetItem.entry.name : ""
                        color: theme.textPrimary
                        font.pixelSize: 13
                        font.bold: true
                    }

                    // 分类小标签
                    Rectangle {
                        id: presetCategoryChip
                        anchors.left: presetNameText.right
                        anchors.leftMargin: 8
                        anchors.verticalCenter: presetNameText.verticalCenter
                        width: presetCategoryText.implicitWidth + 12
                        height: 16
                        radius: 8
                        color: "#1B3D63"
                        border.width: 1
                        border.color: theme.border
                        opacity: presetItem.entry && presetItem.entry.category ? 1 : 0

                        Text {
                            id: presetCategoryText
                            anchors.centerIn: parent
                            text: presetItem.entry ? (presetItem.entry.category || "") : ""
                            color: theme.textSecondary
                            font.pixelSize: 10
                        }
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: presetNameText.bottom
                        anchors.topMargin: 5
                        text: presetItem.entry ? (presetItem.entry.description || "") : ""
                        color: theme.textSecondary
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        wrapMode: Text.NoWrap
                    }
                }

                // 条目之间的细分隔线
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: theme.border
                    opacity: 0.15
                }

                MouseArea {
                    id: presetMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.triggerPreset(presetItem.entry)
                }
            }
        }

        // ── 底部提示 ──
        Item {
            Layout.fillWidth: true
            implicitHeight: 30

            Text {
                anchors.centerIn: parent
                text: "预设会叠加到当前场景，可重复添加，也可一次撤销"
                color: theme.textDim
                font.pixelSize: 10
            }
        }
    }
}
