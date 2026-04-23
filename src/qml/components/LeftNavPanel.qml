import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    signal navSelected(string route)

    ListModel {
        id: navModel
        ListElement { label: "场景构建"; route: "scene" }
        ListElement { label: "动力学求解"; route: "solver" }
        ListElement { label: "参数扫描"; route: "sweep" }
        ListElement { label: "实验教学"; route: "teaching" }
        ListElement { label: "数据分析"; route: "analysis" }
    }

    radius: 14
    color: "#0F2A4CB3"
    border.width: 1
    border.color: "#3F79B0"

    ColumnLayout {
        id: columnItem
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        Label {
            text: "WORKBENCH"
            color: "#8DB3D9"
            font.pixelSize: 11
            font.letterSpacing: 2
        }

        property int currentIndex: 0
        property int hoveredIndex: -1

        Repeater {
            model: navModel

            delegate: Rectangle {
                // width: parent.width
                // height: 42
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 10
                color: index === columnItem.currentIndex ? "#1D4E87" : (index === columnItem.hoveredIndex ? "#163C68" : "#102A4A")
                border.width: 3
                border.color: index === columnItem.currentIndex ? "#56B6FF" : (index === columnItem.hoveredIndex ? "#428AC6" : "#2F5F8D")
                // border.color: "#"

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    text: label
                    color: index === columnItem.currentIndex ? "#E7F2FF" : "#9ABFE3"
                    font.pixelSize: 14
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: {
                        columnItem.hoveredIndex = index;
                    }
                    onExited: {
                        columnItem.hoveredIndex = -1
                    }
                    onClicked: {
                        columnItem.currentIndex = index;
                        root.navSelected(route);
                    }

                }
            }
        }
    }
}