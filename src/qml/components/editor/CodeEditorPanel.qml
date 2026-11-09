/** @file CodeEditorPanel.qml @brief 可复用的代码编辑区及统一字体工具栏。 */
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import EditorManager 1.0
import "qrc:/components/components"
import "qrc:/components/components/theme"
import "qrc:/components/components/editor"

ColumnLayout {
    id: root
    /** @brief 源码文本，程序设置不会触发用户编辑信号。 */
    property alias source: editor.source
    /** @brief 源码语言。 */
    property alias language: editor.language
    /** @brief 编辑区标题。 */
    property string editorTitle: language === "lua" ? "Lua 源码" : "C 源码"
    /** @brief 用户修改源码时通知调用方。 */
    signal edited(string value)
    spacing: 6
    DeepBlueTheme { id: theme }
    /** @brief 与控制器编辑窗口一致的工具栏按钮。 */
    component ActionButton: Basic.Button {
        id: control
        implicitHeight: 32
        implicitWidth: Math.max(60, implicitContentWidth + 24)
        padding: 8
        opacity: enabled ? 1 : 0.45
        contentItem: Text { text: control.text; color: theme.textPrimary; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 6; color: control.down ? theme.accentSoft : (control.hovered ? theme.bg2 : theme.bg1); border.color: control.visualFocus || control.hovered ? theme.accent : theme.border }
    }
    RowLayout {
        Layout.fillWidth: true
        Label { text: root.editorTitle; color: theme.textPrimary }
        Item { Layout.fillWidth: true }
        Label { text: "字体"; color: theme.textSecondary }
        EgretComboBox {
            Layout.preferredWidth: 200
            model: CodeEditorSettings.fontFamilies
            currentIndex: Math.max(0, model.indexOf(CodeEditorSettings.fontFamily))
            onActivated: CodeEditorSettings.fontFamily = currentText
        }
        Label { text: "字号"; color: theme.textSecondary }
        ActionButton { text: "−"; implicitWidth: 32; enabled: CodeEditorSettings.fontSize > CodeEditorSettings.minimumFontSize; onClicked: CodeEditorSettings.fontSize-- }
        Label { text: CodeEditorSettings.fontSize + " px"; color: theme.textPrimary }
        ActionButton { text: "+"; implicitWidth: 32; enabled: CodeEditorSettings.fontSize < CodeEditorSettings.maximumFontSize; onClicked: CodeEditorSettings.fontSize++ }
        ActionButton { text: "默认"; onClicked: CodeEditorSettings.resetToDefaults() }
    }
    CodeEditor {
        id: editor
        objectName: "codeEditor"
        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 130
        onEdited: function(value) { root.edited(value) }
    }
}
