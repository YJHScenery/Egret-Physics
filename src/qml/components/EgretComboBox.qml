/**
 * @file        EgretComboBox.qml
 * @brief       统一样式的下拉选择框组件。
 * @details     从 MainWindow 的工作区选择框抽取而来，供整个应用复用：
 *              深蓝底 + 蓝色描边、白色正文、可选的行尾彩色徽标（例如「不再维护」），
 *              弹出列表与选中项使用同一套对齐与配色。
 *
 *              用法：
 *              - `model` 支持 ListModel 与 JS 数组（QVariantList）；
 *              - `textRole` 指定显示字段，`entryValueRole` 指定 currentValue 取值字段；
 *              - `badgeRole` 非空时，在该字段有值时以 theme.warning 颜色显示在文字之后。
 *
 * @author      jehor
 * @date        2026-10-05
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     View
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Basic 2.15 as Basic

Basic.ComboBox {
    id: control

    /** 行尾徽标字段名；为空表示不显示徽标。 */
    property string badgeRole: ""

    /**
     * @brief currentValue 取值字段名；为空时 currentValue 返回整条记录。
     * @note  不能命名为 valueRole：ComboBox 自身已有 FINAL 的 valueRole 属性，
     *        同名重声明会在运行时直接报「Cannot override FINAL property」。
     */
    property string entryValueRole: ""

    /** 正文颜色。 */
    property color bodyColor: "#DCEAFF"

    /** 下拉项普通颜色。 */
    property color itemColor: "#9ABFE3"

    /** 下拉项高亮颜色。 */
    property color itemHighlightColor: "#E7F2FF"

    implicitHeight: 36
    implicitWidth: 210
    leftPadding: 12
    rightPadding: 28
    font.pixelSize: 13

    /**
     * @brief  取模型中第 index 条记录。
     * @param  index 下标。
     * @return 记录对象；越界返回 null。
     */
    function entryAt(index) {
        if (index === undefined || index === null || index < 0) {
            return null;
        }
        if (model === undefined || model === null) {
            return null;
        }
        if (typeof model.get === "function") {
            return model.get(index);
        }
        return model[index] !== undefined ? model[index] : null;
    }

    /**
     * @brief  取第 index 条记录的显示文本。
     * @param  index 下标。
     * @return 文本；取不到返回空串。
     */
    function textAt(index) {
        const entry = entryAt(index);
        if (entry === null) {
            return "";
        }
        const role = textRole.length > 0 ? textRole : "text";
        return entry[role] !== undefined ? String(entry[role]) : "";
    }

    /**
     * @brief  取第 index 条记录的徽标文本。
     * @param  index 下标。
     * @return 徽标文本；无徽标返回空串。
     */
    function badgeAt(index) {
        if (badgeRole.length === 0) {
            return "";
        }
        const entry = entryAt(index);
        if (entry === null) {
            return "";
        }
        return entry[badgeRole] !== undefined ? String(entry[badgeRole]) : "";
    }

    /** 当前条目的徽标文本。 */
    readonly property string badgeText: badgeAt(currentIndex)

    /**
     * @brief 当前条目的显示文本。
     * @details 不直接依赖 displayText：JS 数组模型下 displayText 可能为空，
     *          这里统一走 textAt()，对 ListModel 与 QVariantList 都成立。
     */
    readonly property string bodyText: {
        const text = textAt(currentIndex);
        return text.length > 0 ? text : displayText;
    }

    /**
     * @brief 当前条目的取值（entryValueRole 为空时为整条记录）。
     * @note  不能命名为 currentValue：ComboBox 自 Qt 6.7 起已有同名只读属性。
     */
    readonly property var entryValue: {
        const entry = entryAt(currentIndex);
        if (entry === null || entryValueRole.length === 0) {
            return entry;
        }
        return entry[entryValueRole] !== undefined ? entry[entryValueRole] : null;
    }

    // contentItem 只用单个 Text：控制项会负责它的尺寸，Text 自己按
    // horizontalAlignment / verticalAlignment 对齐，这样「左对齐 + 上下居中」最稳；
    // 放入位置器（Row / RowLayout）反而会破坏这套对齐，出现文字贴顶、上下边距不等。
    contentItem: Text {
        textFormat: Text.RichText
        text: control.bodyText
            + (control.badgeText.length > 0
                ? "    <font color=\"" + theme.warning.toString() + "\">"
                    + control.badgeText + "</font>"
                : "")
        color: control.enabled ? control.bodyColor : "#5F84AD"
        font.pixelSize: control.font.pixelSize
        horizontalAlignment: Text.AlignLeft
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
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

    background: Rectangle {
        radius: theme.radiusS
        color: !control.enabled
            ? "#08152C"
            : (control.pressed ? "#1E3A8A" : (control.hovered ? "#153564" : "#0A1A3A"))
        border.width: 1
        border.color: !control.enabled
            ? "#1E3A5C"
            : (control.hovered ? "#3B82F6" : "#1E3A8A")
    }

    popup: Basic.Popup {
        y: control.height + 6
        width: control.width
        padding: 6
        clip: false

        background: Rectangle {
            radius: theme.radiusS
            color: "#10294C"
            border.width: 1
            border.color: theme.border
        }

        contentItem: Rectangle {
            radius: theme.radiusS
            color: "transparent"
            clip: true
            implicitHeight: Math.min(listView.contentHeight + 4, 240)

            ListView {
                id: listView
                anchors.fill: parent
                anchors.margins: 2
                model: control.popup.visible ? control.delegateModel : null
                clip: true

                ScrollIndicator.vertical: ScrollIndicator {
                }
            }
        }
    }

    delegate: Basic.ItemDelegate {
        width: listView ? listView.width : control.width
        height: 34
        // 基础样式默认 padding: 12，会把 34 px 的行压到只剩 10 px 内容高度，
        // 文字只能溢出显示、上下留白不可控；这里改为上下 0，让内容区占满行高。
        topPadding: 0
        bottomPadding: 0
        highlighted: control.highlightedIndex === index

        readonly property string entryBadge: control.badgeAt(index)

        contentItem: Text {
            textFormat: Text.RichText
            text: control.textAt(index)
                + (entryBadge.length > 0
                    ? "    <font color=\"" + theme.warning.toString() + "\">"
                        + entryBadge + "</font>"
                    : "")
            color: highlighted ? control.itemHighlightColor : control.itemColor
            font.pixelSize: 12
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10
            rightPadding: 10
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: theme.radiusS
            color: highlighted ? "#1D4E87" : "#102A4A"
            border.width: 1
            border.color: highlighted ? "#56B6FF" : "#2F5F8D"
        }
    }
}
