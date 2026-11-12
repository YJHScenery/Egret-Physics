/** @file AgentSettingsDialog.qml @brief 配置 Agent 凭据、请求预算与领域 Skill。 */
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import "qrc:/components/components/theme"
import "qrc:/components/components/basic"

Basic.Dialog {
    id: root
    title: "Agent 设置"
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(510, parent.width - 30)
    modal: true
    property url skill
    property bool hasKey
    DeepBlueTheme { id: theme }
    padding: 14
    background: Rectangle {
        color: theme.bg1
        border.color: theme.border
        radius: theme.radiusM
    }
    header: Basic.Label {
        text: root.title
        color: theme.textPrimary
        font.bold: true
        leftPadding: root.padding
        topPadding: root.padding
    }

    /** @brief 为数值输入提供圆角边框和带圆角的增减按钮。 */
    component RoundedSpinBox: Basic.SpinBox {
        id: spin
        editable: true
        font.pixelSize: 13
        background: Rectangle {
            implicitWidth: 132
            implicitHeight: 34
            radius: theme.radiusS
            color: theme.bg0
            border.color: spin.activeFocus ? theme.accent : theme.border
        }
        contentItem: TextInput {
            text: spin.displayText
            font: spin.font
            color: theme.textPrimary
            selectionColor: theme.accentSoft
            selectedTextColor: theme.textPrimary
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            validator: spin.validator
            inputMethodHints: spin.inputMethodHints
            clip: true
        }
        up.indicator: StepIndicator {
            x: spin.mirrored ? 3 : spin.width - width - 3
            y: 3
            height: spin.height - 6
            symbol: "+"
            pressed: spin.up.pressed
            hovered: spin.up.hovered
        }
        down.indicator: StepIndicator {
            x: spin.mirrored ? spin.width - width - 3 : 3
            y: 3
            height: spin.height - 6
            symbol: "−"
            pressed: spin.down.pressed
            hovered: spin.down.hovered
        }
    }

    /** @brief 绘制数值控件的增减指示器。 */
    component StepIndicator: Rectangle {
        id: step
        required property string symbol
        required property bool pressed
        required property bool hovered
        implicitWidth: 29
        radius: 5
        color: pressed ? theme.accentSoft : (hovered ? theme.bg2 : theme.bg1)
        Text {
            anchors.centerIn: parent
            text: step.symbol
            color: step.enabled ? theme.textPrimary : theme.textDim
            font.pixelSize: 15
        }
    }

    palette.window: theme.bg1
    palette.windowText: theme.textPrimary
    palette.base: theme.bg0
    palette.text: theme.textPrimary
    palette.button: theme.bg2
    palette.buttonText: theme.textPrimary
    onOpened: {
        const settings = sceneAgent.settings()
        hasKey = settings.hasKey
        timeout.value = settings.timeout
        tokens.value = settings.tokens
        skill = settings.skill
        key.clear()
        clearKey.checked = false
        error.text = ""
    }
    FileDialog {
        id: skillPicker
        title: "选择 Skill 说明"
        nameFilters: ["Markdown (*.md)"]
        onAccepted: root.skill = selectedFile
    }
    contentItem: ColumnLayout {
        Basic.Label { text: "DeepSeek V4.1 Flash（deepseek-flash）"; color: theme.textPrimary }
        EgretTextField {
            id: key
            Layout.fillWidth: true
            echoMode: TextInput.Password
            placeholderText: root.hasKey ? "已保存，留空保留" : "API Key，留空使用环境变量"
        }
        EgretCheckBox { id: clearKey; text: "清除已保存的 Key"; enabled: root.hasKey && key.text.length === 0 }
        RowLayout {
            Basic.Label { text: "超时（秒）"; color: theme.textPrimary }
            RoundedSpinBox { id: timeout; from: 10; to: 600 }
            Basic.Label { text: "输出 Token 上限"; color: theme.textPrimary }
            RoundedSpinBox { id: tokens; from: 1024; to: 65536; stepSize: 1024 }
        }
        Basic.Label {
            Layout.fillWidth: true
            wrapMode: Text.WrapAnywhere
            color: theme.textSecondary
            text: root.skill.toString() || "未配置 Skill"
        }
        RowLayout {
            MenuActionButton { text: "选择 SKILL.md…"; onClicked: skillPicker.open() }
            MenuActionButton { text: "清除 Skill"; onClicked: root.skill = "" }
        }
        Basic.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: "所选 Skill 的 Markdown 会随请求发送给 DeepSeek，作为领域说明。"
            color: theme.textSecondary
        }
        Basic.Label { id: error; Layout.fillWidth: true; wrapMode: Text.Wrap; color: theme.danger }
        RowLayout {
            Item { Layout.fillWidth: true }
            MenuActionButton { text: "取消"; onClicked: root.close() }
            MenuActionButton {
                text: "保存"
                onClicked: {
                    error.text = sceneAgent.saveSettings(key.text, clearKey.checked, timeout.value, tokens.value, root.skill)
                    if (!error.text.length) root.close()
                }
            }
        }
    }
    onClosed: key.clear()
}
