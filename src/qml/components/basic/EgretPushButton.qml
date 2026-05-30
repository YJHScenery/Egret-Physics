/**
 * @file        EgretPushButton.qml
 * @brief       按钮组件文件
 * @details     定义应用程序的按钮组件，支持文本显示和点击事件。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-27
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     View
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

import QtQuick 2.15
import QtQuick.Layouts 1.15
import "qrc:/components/components/theme"

Rectangle {
    id: root

    property string text: ""
    property int pixelSize: 12
    property bool enabled: true

    signal clicked()

    Layout.fillWidth: true
    Layout.preferredHeight: 28
    radius: 6
    color: buttonMouse.pressed ? theme.accentSoft : (buttonMouse.containsMouse && root.enabled ? theme.bg2 : theme.bg1)
    border.width: 1
    border.color: buttonMouse.containsMouse && root.enabled ? theme.accent : theme.border
    opacity: root.enabled ? 1 : 0.5

    Text {
        anchors.centerIn: parent
        text: root.text
        color: theme.textPrimary
        font.pixelSize: root.pixelSize
    }

    MouseArea {
        id: buttonMouse
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.enabled
        onClicked: {
            root.clicked()
        }
    }
}