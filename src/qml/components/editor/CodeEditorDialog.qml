/** @file CodeEditorDialog.qml @brief 复用控制器编辑区的独立源码草稿弹窗。 */
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import "qrc:/components/components"
import "qrc:/components/components/theme"
import "qrc:/components/components/editor"

Basic.Dialog {
    id: root
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    closePolicy: Popup.NoAutoClose
    title: "编辑矢量力场源码"
    width: Math.min(1100, parent ? parent.width - 40 : 1100)
    height: Math.min(760, parent ? parent.height - 40 : 760)
    padding: 18
    /** @brief 当前编辑的参数标识。 */
    property string parameterKey: ""
    /** @brief 返回弹窗中的草稿文本。 */
    property alias source: panel.source
    /** @brief 确认保存时提交参数标识与草稿。 */
    signal sourceSaved(string key, string value)
    DeepBlueTheme { id: theme }
    palette.window: theme.bg1
    palette.windowText: theme.textPrimary
    palette.base: theme.bg0
    palette.text: theme.textPrimary
    palette.button: theme.bg2
    palette.buttonText: theme.textPrimary
    palette.highlight: theme.accent
    background: Rectangle { color: theme.bg1; radius: 12; border.color: theme.border }
    header: Label { text: root.title; color: theme.textPrimary; font.pixelSize: 18; padding: 18 }
    /** @brief 载入参数草稿并打开弹窗。 */
    function begin(key, value) { parameterKey = key; panel.source = value || ""; open() }
    /** @brief 保存草稿并关闭，取消时不提交。 */
    function save() { sourceSaved(parameterKey, panel.source); close() }
    contentItem: ColumnLayout {
        spacing: 10
        Label { Layout.fillWidth: true; wrapMode: Text.Wrap; color: theme.textSecondary; text: "定义作用域函数 field_contains 和力函数 field_force。修改在点击保存后写入当前场参数。" }
        CodeEditorPanel { id: panel; objectName: "fieldCodePanel"; Layout.fillWidth: true; Layout.fillHeight: true; language: "c" }
        RowLayout {
            Item { Layout.fillWidth: true }
            MenuActionButton { text: "取消"; onClicked: root.close() }
            MenuActionButton { text: "保存"; onClicked: root.save() }
        }
    }
}

