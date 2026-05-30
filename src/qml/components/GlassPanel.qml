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
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    property string title: ""
    property bool showHeader: title.length > 0
    default property alias panelContent: bodyItem.data

    radius: 14
    color: "#122F56B3"
    border.width: 1
    border.color: "#3F79B0"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 1

        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: root.showHeader ? 44 : 0
            visible: root.showHeader
            radius: root.radius
            color: "#133760"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 10

                Label {
                    text: root.title
                    color: "#DCEAFF"
                    font.pixelSize: 14
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: "#27B5FF"
                    opacity: 0.9
                }
            }
        }

        Item {
            id: bodyItem
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 12
        }
    }
}