/** @file AgentSidebar.qml @brief 编辑器和控制器共用的 Agent 侧栏。 */
import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import "qrc:/components/components/theme"

Rectangle {
    id: root
    required property string ownerId
    property bool codeMode: false
    property string codeLanguage: ""
    property string context: ""
    property string submittedContext: ""
    property bool codeTargetAvailable: true
    signal applySource(string source)
    readonly property bool ownsResult: sceneAgent.owner === ownerId
    readonly property int task: codeMode ? 1 : operation.currentIndex
    color: theme.bg1
    border.color: theme.border
    radius: 6
    DeepBlueTheme { id: theme }
    AgentSettingsDialog { id: settings }
    FileDialog {
        id: output
        title: "保存 Agent 草稿"
        fileMode: FileDialog.SaveFile
        nameFilters: ["草稿 (*." + sceneAgent.suffix + ")"]
        onAccepted: if (root.ownsResult) sceneAgent.saveDraft(selectedFile)
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6
        RowLayout {
            Basic.Label { text: "Agent"; color: theme.textPrimary; font.bold: true; Layout.fillWidth: true }
            MenuActionButton { text: "设置"; onClicked: settings.open() }
        }
        EgretComboBox {
            id: operation
            visible: !root.codeMode
            Layout.fillWidth: true
            enabled: !sceneAgent.busy
            model: ["生成场景", "生成控制器", "编辑当前场景"]
        }
        EgretComboBox {
            id: language
            visible: !root.codeMode && root.task === 1
            Layout.fillWidth: true
            enabled: !sceneAgent.busy
            model: ["请选择语言", "C", "Lua"]
        }
        Basic.Label { visible: root.codeMode; color: theme.textSecondary; text: "当前控制器 · " + root.codeLanguage }
        Basic.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: theme.textSecondary
            text: root.codeMode ? "需求、当前控制器配置及 Skill 将发送给 DeepSeek。"
                                : (root.task === 2 ? "需求、当前场景及 Skill 将发送给 DeepSeek。" : "需求及 Skill 将发送给 DeepSeek。")
        }
        Basic.ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 104
            clip: true
            Basic.TextArea {
                id: request
                placeholderText: "描述你要生成或修改的内容"
                color: theme.textPrimary
                placeholderTextColor: theme.textSecondary
                wrapMode: TextEdit.Wrap
                background: Rectangle { color: theme.bg0; border.color: theme.border; radius: 4 }
            }
        }
        RowLayout {
            MenuActionButton {
                text: root.task === 2 ? "编辑当前场景" : (root.task === 0 ? "生成新场景" : "生成草稿")
                opacity: enabled ? 1 : 0.45
                enabled: !sceneAgent.busy && request.text.trim().length > 0 &&
                         (root.codeMode ? root.codeTargetAvailable : root.task !== 1 || language.currentIndex > 0)
                onClicked: {
                    root.submittedContext = root.context
                    sceneAgent.start(root.task, request.text, root.codeMode ? root.codeLanguage : language.currentText,
                                     root.context, root.ownerId)
                }
            }
            MenuActionButton { text: "取消请求"; visible: sceneAgent.busy; onClicked: sceneAgent.cancel() }
        }
        Basic.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: theme.textSecondary
            text: root.ownsResult ? sceneAgent.status : (sceneAgent.busy ? "Agent 正在处理另一处的请求…" : "")
        }
        Basic.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            Basic.TextArea {
                text: root.ownsResult ? sceneAgent.preview : ""
                readOnly: true
                selectByMouse: true
                color: theme.textPrimary
                wrapMode: TextEdit.Wrap
                background: Rectangle { color: theme.bg0; border.color: theme.border; radius: 4 }
            }
        }
        RowLayout {
            MenuActionButton {
                visible: root.task === 1
                text: "另存草稿…"
                opacity: enabled ? 1 : 0.45
                enabled: root.ownsResult && !sceneAgent.busy && sceneAgent.suffix.length > 0
                onClicked: output.open()
            }
            MenuActionButton {
                text: "写入代码草稿"
                opacity: enabled ? 1 : 0.45
                visible: root.codeMode
                enabled: root.ownsResult && !sceneAgent.busy && sceneAgent.source.length > 0 &&
                         root.codeTargetAvailable && root.context === root.submittedContext
                onClicked: root.applySource(sceneAgent.source)
            }
        }
    }
}
