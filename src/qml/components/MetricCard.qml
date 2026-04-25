import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    property string metricName: "Metric"
    property string metricValue: "--"
    property string metricUnit: ""
    property color metricColor: "#27B5FF"

    radius: 12
    color: hovered ? "#013088CC" : "#123660CC"
    border.width: 1
    property bool hovered: false
    border.color: "#356FA4"

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onEntered: {
            root.hovered = true
        }
        onExited: {
            root.hovered = false
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Label {
            text: root.metricName
            color: "#8DB3D9"
            font.pixelSize: 12
        }

        RowLayout {
            spacing: 8

            Label {
                text: root.metricValue
                color: "#DCEAFF"
                font.pixelSize: 24
                font.bold: true
            }

            Label {
                text: root.metricUnit
                color: "#8DB3D9"
                font.pixelSize: 12
                Layout.alignment: Qt.AlignBottom
            }

            Item {
                Layout.fillWidth: true
            }

            Rectangle {
                width: 10
                height: 10
                radius: 5
                color: root.metricColor
                opacity: 0.95
            }
        }
    }
}