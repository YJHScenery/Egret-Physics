/** @file ShortcutHelpDialog.qml @brief 说明菜单快捷键及其工作区和文本编辑范围。 */
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
    title: "快捷键与菜单说明"
    width: Math.min(560, parent ? parent.width - 32 : 560)
    padding: 20
    DeepBlueTheme {id: theme}
    background: Rectangle {color: theme.bg1;radius: theme.radiusM;border.color: theme.border}
    header: Label {text: root.title;color: theme.textPrimary;font.pixelSize: 18;padding: 20}
    contentItem: ColumnLayout {
        spacing: 16
        Label {
            Layout.fillWidth: true
            color: theme.textSecondary
            font.pixelSize: 13
            wrapMode: Text.Wrap
            text: "文件：Ctrl+N 新建，Ctrl+O 打开，Ctrl+S 保存，Ctrl+Shift+S 另存为，Ctrl+W 关闭场景\n\n编辑：Ctrl+Z 撤销，Ctrl+Y 重做，Ctrl+X/C/V 剪切／复制／粘贴，Ctrl+A 全选，Delete 删除\n\n场景：Ctrl+Alt+C 编辑 控制程序\n运行：F5 开始求解或播放／暂停，Shift+F5 取消求解或停止回放，Alt+←/→ 逐帧回放\n\n视图：Ctrl+1/2/3/4 切换工作区，Home 适配场景，Ctrl+G/H 显示网格／坐标轴\n设置：Ctrl+, 编辑器设置\n\n菜单会根据工作区、选择和任务状态自动启用。输入文字或编辑代码时，编辑快捷键交给文本框；模态窗口打开时，主窗口快捷键暂停。"
        }
        MenuActionButton {Layout.alignment: Qt.AlignRight;text: "关闭";onClicked: root.accept()}
    }
}
