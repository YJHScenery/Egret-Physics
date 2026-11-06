/** @file EgretTextField.qml @brief 统一深蓝主题的文本与科学计数法输入外观。 */
import QtQuick
import QtQuick.Controls.Basic as Basic
import "qrc:/components/components/theme"

Basic.TextField {
    id: control
    DeepBlueTheme { id: inputTheme }

    implicitWidth: 140
    implicitHeight: 32
    leftPadding: 10
    rightPadding: 10
    font.pixelSize: 13
    color: enabled ? inputTheme.textPrimary : inputTheme.textDim
    placeholderTextColor: inputTheme.textDim
    selectionColor: inputTheme.accentSoft
    selectedTextColor: inputTheme.textPrimary
    background: Rectangle {
        radius: 6
        color: inputTheme.bg0
        border.width: 1
        border.color: !control.acceptableInput ? inputTheme.danger
            : (control.activeFocus ? inputTheme.accent : inputTheme.border)
        opacity: control.enabled ? 1 : 0.5
    }
}
