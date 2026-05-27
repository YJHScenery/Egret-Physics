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