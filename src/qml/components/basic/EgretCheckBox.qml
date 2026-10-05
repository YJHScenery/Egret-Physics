/**
 * @file        EgretCheckBox.qml
 * @brief       深蓝主题的复选框组件。
 * @details     系统默认样式的复选框文字取调色板颜色，在深蓝背景上接近黑色、几乎不可读；
 *              本组件用主题色重绘指示器与文字，并统一禁用态的半透明表现，供各工作区复用。
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
import QtQuick.Controls.Basic 2.15 as Basic

Basic.CheckBox {
    id: control

    /** 文字颜色。 */
    property color textColor: "#DCEAFF"

    /** 未选中时指示器底色。 */
    property color boxColor: "#0A1A3A"

    spacing: 8
    leftPadding: 0
    font.pixelSize: 12

    indicator: Rectangle {
        implicitWidth: 15
        implicitHeight: 15
        x: 0
        y: control.height / 2 - height / 2
        radius: 3
        color: control.checked ? theme.accent : control.boxColor
        border.width: 1
        border.color: control.checked
            ? "#8FD8FF"
            : (control.hovered ? "#3B82F6" : "#2F5F8D")
        opacity: control.enabled ? 1.0 : 0.45

        Text {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -1
            text: "\u2713"
            color: "#06203A"
            font.pixelSize: 11
            font.bold: true
            visible: control.checked
        }
    }

    contentItem: Text {
        text: control.text
        color: control.enabled ? control.textColor : "#5F84AD"
        font: control.font
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
        elide: Text.ElideRight
    }
}
