/**
 * @file        AboutDialog.qml
 * @brief       关于对话框组件文件
 * @details     定义应用程序的关于对话框，显示版本信息和版权信息。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-16
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
import "qrc:/components/components/theme"

Dialog {
    id: aboutDialog

    title: "About Egret Physics"
    modal: true
    standardButtons: Dialog.Ok
    width: 520
    height: 520

    anchors.centerIn: parent

    opacity: 0
    scale: 0.8

    onOpened: {
        opacityAnimator.start();
        scaleAnimator.start();
    }

    NumberAnimation {
        id: opacityAnimator
        target: aboutDialog
        property: "opacity"
        from: 0
        to: 1
        duration: 300
        easing.type: Easing.OutCubic
    }

    NumberAnimation {
        id: scaleAnimator
        target: aboutDialog
        property: "scale"
        from: 0.8
        to: 1
        duration: 300
        easing.type: Easing.OutCubic
    }

    NumberAnimation {
        id: closeOpacityAnimator
        target: aboutDialog
        property: "opacity"
        from: 1
        to: 0
        duration: 200
        easing.type: Easing.InCubic
        onRunningChanged: {
            if (!running && closeAnimationActive) {
                closeAnimationActive = false;
                aboutDialog.close();
            }
        }
    }

    NumberAnimation {
        id: closeScaleAnimator
        target: aboutDialog
        property: "scale"
        from: 1
        to: 0.8
        duration: 200
        easing.type: Easing.InCubic
    }

    property bool closeAnimationActive: false

    function closeWithAnimation() {
        if (closeAnimationActive)
            return;
        closeAnimationActive = true;
        closeOpacityAnimator.start();
        closeScaleAnimator.start();
    }

    onAccepted: {
        if (closeAnimationActive)
            return;
        closeAnimationActive = true;
        closeOpacityAnimator.start();
        closeScaleAnimator.start();
    }

    onRejected: {
        if (closeAnimationActive)
            return;
        closeAnimationActive = true;
        closeOpacityAnimator.start();
        closeScaleAnimator.start();

    }

    background: Rectangle {
        color: "#0A1738"
        border.width: 1
        border.color: "#4F8AC0"
        radius: 12
    }

    header: Rectangle {
        color: "transparent"
        height: 50

        Text {
            anchors.centerIn: parent
            text: aboutDialog.title
            color: "#DCEAFF"
            font.pixelSize: 18
            font.bold: true
        }
    }

    contentItem: Rectangle {
        color: "transparent"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            Image {
                source: "qrc:/app_icon/assets/favicon/favicon_256.png"
                Layout.preferredWidth: 100
                Layout.preferredHeight: 100
                Layout.alignment: Qt.AlignHCenter
                fillMode: Image.PreserveAspectFit
            }

            Text {
                text: "Egret Physics"
                color: "#27B5FF"
                font.pixelSize: 24
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: "Classical Mechanics Studio"
                color: "#8DB3D9"
                font.pixelSize: 14
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: resourceHelper.getVersionString("Version", "")
                // text: "Version 0.0.1"
                color: "#8DB3D9"
                font.pixelSize: 12
                Layout.alignment: Qt.AlignHCenter
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#4F8AC0"
            }

            Text {
                text: "A professional physics simulation software for classical mechanics.\nDesigned for educational and research purposes."
                color: "#8DB3D9"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#4F8AC0"
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 8

                Text {
                    text: "License:"
                    color: "#8DB3D9"
                    font.pixelSize: 12
                }

                Text {
                    text: "GNU General Public License v3.0"
                    color: "#27B5FF"
                    font.pixelSize: 12
                    font.underline: true

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Qt.openUrlExternally("https://www.gnu.org/licenses/gpl-3.0.html")
                    }
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 8

                Text {
                    text: "Github:"
                    color: "#8DB3D9"
                    font.pixelSize: 12
                }

                Text {
                    text: "Egret Physics Github Repo"
                    color: "#27B5FF"
                    font.pixelSize: 12
                    font.underline: true

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Qt.openUrlExternally("https://github.com/YJHScenery/Egret-Physics")
                    }
                }
            }

            Text {
                text: "© 2026 Egret Physics Development Team"
                color: "#5F84AD"
                font.pixelSize: 11
                Layout.alignment: Qt.AlignHCenter
            }

            Item {
                Layout.fillHeight: true
            }
        }
    }

    footer: Rectangle {
        color: "transparent"
        height: 50

        Rectangle {
            anchors.centerIn: parent
            width: 100
            height: 36
            radius: 8
            border.width: 1
            border.color: "#4F8AC0"
            color: hovered ? "#2B6DE0" : "#143663"

            property bool hovered: false

            Text {
                anchors.centerIn: parent
                text: "OK"
                color: "#DCEAFF"
                font.pixelSize: 14
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onEntered: parent.hovered = true
                onExited: parent.hovered = false
                onClicked: aboutDialog.closeWithAnimation()
            }
        }
    }
}
