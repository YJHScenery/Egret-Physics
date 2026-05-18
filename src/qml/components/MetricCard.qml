import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    property string metricName: "Metric"
    property string metricValue: "--"
    property string metricUnit: ""
    property color metricColor: "#27B5FF"

    property int nameFontSize: 12
    property int valueFontSize: 24
    property int unitFontSize: 12
    property int dotSize: 10
    property int contentPadding: 12
    property int contentSpacing: 8

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
        anchors.margins: root.contentPadding
        spacing: root.contentSpacing

        Label {
            text: root.metricName
            color: "#8DB3D9"
            font.pixelSize: root.nameFontSize
        }

        RowLayout {
            spacing: 8

            Label {
                text: root.metricValue
                color: "#DCEAFF"
                font.pixelSize: root.valueFontSize
                font.bold: true
            }

            Label {
                text: root.metricUnit
                color: "#8DB3D9"
                font.pixelSize: root.unitFontSize
                Layout.alignment: Qt.AlignBottom
            }

            Item {
                Layout.fillWidth: true
            }

            Rectangle {
                width: root.dotSize
                height: root.dotSize
                radius: root.dotSize / 2
                color: root.metricColor
                opacity: 0.95
            }
        }
    }
}