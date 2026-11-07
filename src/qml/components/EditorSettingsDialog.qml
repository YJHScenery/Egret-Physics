/** @file EditorSettingsDialog.qml @brief 主菜单可访问的代码编辑器字体设置。 */
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import EditorManager 1.0
import "theme"

Basic.Dialog {
    id: root
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    title: "编辑器设置"
    width: Math.min(480, parent ? parent.width - 32 : 480)
    padding: 20
    DeepBlueTheme {id: theme}
    background: Rectangle {color: theme.bg1;radius: theme.radiusM;border.color: theme.border}
    header: Label {text: root.title;color: theme.textPrimary;font.pixelSize: 18;padding: 20}
    contentItem: ColumnLayout {
        spacing: 14
        Label {text: "C 代码字体";color: theme.textSecondary}
        EgretComboBox {
            Layout.fillWidth: true
            model: CodeEditorSettings.fontFamilies
            currentIndex: Math.max(0, model.indexOf(CodeEditorSettings.fontFamily))
            onActivated: CodeEditorSettings.fontFamily = currentText
        }
        RowLayout {
            Label {text: "字号（像素）";color: theme.textSecondary}
            EgretTextField {
                Layout.preferredWidth: 100
                text: String(CodeEditorSettings.fontSize)
                validator: IntValidator {bottom: CodeEditorSettings.minimumFontSize;top: CodeEditorSettings.maximumFontSize}
                onEditingFinished: if (acceptableInput) CodeEditorSettings.fontSize = Number(text)
            }
        }
        Label {Layout.fillWidth: true;text: "修改立即生效并自动保存。默认使用 JetBrains Mono，缩进为 4 个空格。";wrapMode: Text.Wrap;color: theme.textSecondary}
        RowLayout {
            MenuActionButton {text: "恢复默认";onClicked: CodeEditorSettings.resetToDefaults()}
            Item {Layout.fillWidth: true}
            MenuActionButton {text: "完成";onClicked: root.accept()}
        }
    }
}
