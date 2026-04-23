import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "qrc:/components/components"


Window {
    id: window

    width: 1600
    height: 920
    visible: true
    title: "Egret Physics - Classical Mechanics Studio"

    DeepBlueTheme {
        id: theme
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: theme.bg0 }
            GradientStop { position: 0.5; color: theme.bg1 }
            GradientStop { position: 1.0; color: theme.bg2 }
        }
    }

    Rectangle {
        id: haloA
        width: 460
        height: 460
        radius: 230
        x: window.width * 0.63
        y: -190
        color: "#2D8BFF"
        opacity: 0.12
    }

    SequentialAnimation {
        running: true
        loops: Animation.Infinite
        NumberAnimation { target: haloA; property: "opacity"; to: 0.2; duration: 2200; easing.type: Easing.InOutSine }
        NumberAnimation { target: haloA; property: "opacity"; to: 0.08; duration: 2200; easing.type: Easing.InOutSine }
    }

    Rectangle {
        id: haloB
        width: 380
        height: 380
        radius: 190
        x: -120
        y: window.height - 240
        color: "#26C8FF"
        opacity: 0.10
    }

    SequentialAnimation {
        running: true
        loops: Animation.Infinite
        NumberAnimation { target: haloB; property: "opacity"; to: 0.18; duration: 2600; easing.type: Easing.InOutSine }
        NumberAnimation { target: haloB; property: "opacity"; to: 0.06; duration: 2600; easing.type: Easing.InOutSine }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 68
            radius: theme.radiusL
            color: "#11325ECC"
            border.width: 1
            border.color: theme.border

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 16

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
                    id: newSceneButton
                    implicitWidth: 190
                    implicitHeight: 36
                    radius: 10

                    property bool hovered: false
                    property bool isPressed: false  // 新增：标记是否处于点击高亮状态
                    property int highlightDuration: 100

                    Timer {
                        id: colorTimer
                        interval: parent.highlightDuration
                        onTriggered: {
                            parent.isPressed = false  // 清除点击高亮标记
                            // 不需要手动设置颜色，让颜色绑定属性自动处理
                        }
                    }

                    // 根据状态动态计算颜色
                    color: {
                        if (isPressed) return "#2986ef"      // 点击高亮颜色
                        if (hovered) return "#1E3A8A"        // 悬停颜色
                        return "#0A1A3A"                      // 默认颜色
                    }

                    border.width: 1
                    border.color: {
                        if (isPressed) return "#2563EB"
                        if (hovered) return "#3B82F6"
                        return "#1E3A8A"
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "+ 新建仿真场景"
                        color: "#8bb5ea"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    MouseArea {
                        id: newSceneMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            newSceneButton.hovered = true
                        }
                        onExited: {
                            newSceneButton.hovered = false
                        }
                        onClicked: {
                            console.log("New Scene Button Clicked")
                            newSceneButton.isPressed = true
                            colorTimer.restart()  // 使用 restart 确保每次点击都会重新计时
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14

            LeftNavPanel {
                Layout.preferredWidth: 250
                Layout.fillHeight: true
                onNavSelected: {
                    console.log("Switch route:", route);
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 112
                    spacing: 12

                    MetricCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 80
                        metricName: "积分步长"
                        metricValue: "0.5"
                        metricUnit: "ms"
                        metricColor: theme.accent
                    }

                    MetricCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 80
                        metricName: "实时帧率"
                        metricValue: "120"
                        metricUnit: "FPS"
                        metricColor: theme.success
                    }

                    MetricCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 80
                        metricName: "活动刚体"
                        metricValue: "84"
                        metricUnit: "obj"
                        metricColor: theme.warning
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12

                    GlassPanel {

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        title: "场景视图 / Dynamics Canvas"

                        Item {
                            anchors.fill: parent

                            clip: true

                            Repeater {
                                model: 18
                                Rectangle {
                                    width: parent.width
                                    height: 1
                                    y: index * (parent.height / 18)
                                    color: index % 3 === 0 ? "#234E78" : "#173756"
                                    opacity: 0.5
                                }
                            }

                            Repeater {
                                model: 24
                                Rectangle {
                                    width: 1
                                    height: parent.height
                                    x: index * (parent.width / 24)
                                    color: index % 4 === 0 ? "#285683" : "#193A5D"
                                    opacity: 0.45
                                }
                            }

                            Rectangle {
                                width: 180
                                height: 180
                                radius: 90
                                anchors.centerIn: parent
                                color: "#1D6EB80F"
                                border.width: 2
                                border.color: "#3EC5FF"
                            }

                            Text {
                                anchors.centerIn: parent
                                text: "3D/2D 仿真视图占位\n可接入自研渲染器或 Qt Quick 3D"
                                horizontalAlignment: Text.AlignHCenter
                                color: theme.textSecondary
                                font.pixelSize: 14
                            }
                        }
                    }

                    GlassPanel {
                        Layout.preferredWidth: 320
                        Layout.fillHeight: true
                        title: "参数与检查器"

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 10

                            Repeater {
                                model: [
                                    "重力场 g = 9.80665 m/s^2",
                                    "积分器: RK4 (Adaptive)",
                                    "碰撞模型: Impulse + Friction",
                                    "约束求解: Sequential Impulse",
                                    "误差阈值: 1e-6"
                                ]

                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 42
                                    radius: 9
                                    color: "#12365D"
                                    border.width: 1
                                    border.color: "#2E5D89"

                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.left: parent.left
                                        anchors.leftMargin: 10
                                        text: modelData
                                        color: "#BDD8F3"
                                        font.pixelSize: 13
                                    }
                                }
                            }

                            Item {
                                Layout.fillHeight: true
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 46
                                radius: 10
                                color: "#1A4E83"
                                border.width: 1
                                border.color: "#58B8FF"

                                Text {
                                    anchors.centerIn: parent
                                    text: "运行数值求解"
                                    color: "#EAF5FF"
                                    font.pixelSize: 14
                                    font.bold: true
                                }
                            }
                        }
                    }
                }

                GlassPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 170
                    title: "时间轴 / 数据流"

                    RowLayout {
                        anchors.fill: parent
                        spacing: 10

                        Repeater {
                            model: 5
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: 10
                                color: "#112F52"
                                border.width: 1
                                border.color: "#2C5D88"

                                Text {
                                    anchors.centerIn: parent
                                    text: "数据通道 " + (index + 1)
                                    color: "#8DB3D9"
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            radius: theme.radiusS
            color: "#102B4B"
            border.width: 1
            border.color: "#325E89"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12

                Label {
                    text: "Ready | Precision Mode"
                    color: theme.textSecondary
                    font.pixelSize: 12
                }

                Item {
                    Layout.fillWidth: true
                }

                Label {
                    text: "Solver Queue: 3 tasks"
                    color: theme.textDim
                    font.pixelSize: 12
                }
            }
        }
    }
}
