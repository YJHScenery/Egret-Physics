/**
 * @file        filename.h
 * @brief       一句话概述文件的主要功能
 * @details     详细描述文件的用途、设计思路、使用注意事项等。
 *              可以写多行，说明模块在整个系统中的位置。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        创建日期 (如 2025-03-15)
 * @version     版本号 (如 1.0.0)
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     许可证 (如 GPL, MIT, BSD 等)
 *
 * @ingroup     所属模块组 (可选，用于分组)
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