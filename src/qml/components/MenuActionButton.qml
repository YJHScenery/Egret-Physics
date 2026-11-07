/** @file MenuActionButton.qml @brief 菜单辅助窗口共用的深蓝主题操作按钮。 */
import QtQuick
import QtQuick.Controls.Basic as Basic
import "theme"

Basic.Button {
    id: control
    implicitHeight: 34
    implicitWidth: Math.max(80, implicitContentWidth + 24)
    padding: 8
    DeepBlueTheme {id: theme}
    contentItem: Text {text: control.text;font.pixelSize: 13;color: theme.textPrimary;horizontalAlignment: Text.AlignHCenter;verticalAlignment: Text.AlignVCenter}
    background: Rectangle {radius: theme.radiusS;color: control.down ? theme.accentSoft : (control.hovered ? theme.bg2 : theme.bg1);border.color: control.hovered || control.visualFocus ? theme.accent : theme.border}
}
