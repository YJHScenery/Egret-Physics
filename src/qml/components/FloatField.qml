import QtQuick 2.15
import QtQuick.Controls 6.9
import "qrc:/components/components/theme"

Item {
    id: root

    DeepBlueTheme {
        id: theme
    }

    property real from: -1000000
    property real to: 1000000
    property real stepSize: 0.1
    property int decimals: 3
    property real value: 0

    implicitHeight: 28
    implicitWidth: 120

    function clampValue(v) {
        return Math.max(from, Math.min(to, v));
    }

    function formatValue(v) {
        return Number(v).toFixed(decimals);
    }

    function setValue(v) {
        var clamped = clampValue(v);
        if (Math.abs(clamped - value) > 0.0000001) {
            value = clamped;
        }
        if (!field.activeFocus) {
            field.text = formatValue(value);
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: theme.radiusS
        color: theme.bg1
        border.width: 1
        border.color: field.activeFocus ? theme.accent : theme.border

        TextField {
            id: field
            anchors.fill: parent
            text: formatValue(root.value)
            color: "#FFFFFF"
            font.pixelSize: 12
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10
            rightPadding: 10
            selectByMouse: true
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            background: null

            validator: DoubleValidator {
                bottom: root.from
                top: root.to
                decimals: root.decimals
            }

            onEditingFinished: {
                var parsed = Number(text);
                if (isNaN(parsed)) {
                    text = formatValue(root.value);
                    return;
                }
                root.setValue(parsed);
                text = formatValue(root.value);
            }
        }
    }

    onValueChanged: {
        if (!field.activeFocus) {
            field.text = formatValue(value);
        }
    }
}
