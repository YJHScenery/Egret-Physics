/** @file SceneCloseDialog.qml @brief 与应用深蓝主题一致的未保存场景确认窗口。 */
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import "theme"

Basic.Dialog {
    id: root
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    title: "未保存的场景"
    property int documentIndex: -1
    property string documentName: ""
    width: Math.min(460, parent ? parent.width - 32 : 460)
    padding: 20
    spacing: 12
    closePolicy: Popup.CloseOnEscape
    standardButtons: Dialog.NoButton
    DeepBlueTheme {id: theme}

    /** @brief 提供主题配色、键盘焦点和悬停反馈的操作按钮。 */
    component PromptButton: Basic.Button {
        id: control
        property bool primary: false
        implicitWidth: 100
        implicitHeight: 36
        hoverEnabled: true
        contentItem: Text {
            text: control.text
            font.pixelSize: 13
            color: theme.textPrimary
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: theme.radiusS
            color: control.down ? theme.accentSoft
                : (control.primary ? theme.accentSoft : (control.hovered ? theme.bg2 : theme.bg1))
            border.color: control.visualFocus || control.hovered || control.primary ? theme.accent : theme.border
            border.width: 1
        }
    }

    background: Rectangle {color: theme.bg1;radius: theme.radiusM;border.color: theme.border}
    header: Text {
        text: root.title
        color: theme.textPrimary
        font.pixelSize: 18
        font.bold: true
        leftPadding: 20;rightPadding: 20;topPadding: 20;bottomPadding: 4
    }
    contentItem: Text {
        text: "是否保存对“" + root.documentName + "”的修改？"
        textFormat: Text.PlainText
        color: theme.textSecondary
        font.pixelSize: 14
        wrapMode: Text.WrapAnywhere
    }
    footer: Item {
        implicitHeight: 60
        RowLayout {
            anchors {right: parent.right;bottom: parent.bottom;rightMargin: 20;bottomMargin: 20}
            spacing: 10
            PromptButton {text: "保存";primary: true;onClicked: root.accept()}
            PromptButton {text: "不保存";onClicked: {root.discarded();root.close()}}
            PromptButton {text: "取消";onClicked: root.reject()}
        }
    }
}
