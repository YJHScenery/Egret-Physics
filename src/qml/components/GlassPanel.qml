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