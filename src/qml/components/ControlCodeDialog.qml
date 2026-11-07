/** @file ControlCodeDialog.qml @brief 多段 C 源码、对象别名、调度与后台诊断编辑器。 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Basic as Basic
import EditorManager 1.0
import "qrc:/components/components/theme"
import "qrc:/components/components/basic"
import "qrc:/components/components/editor"

Basic.Dialog {
    id: root
    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    title: "场景 C 控制程序"
    width: Math.min(1180, parent ? parent.width - 40 : 1180)
    height: Math.min(850, parent ? parent.height - 40 : 850)
    closePolicy: Popup.NoAutoClose
    padding: 18
    palette.window: theme.bg1
    palette.windowText: theme.textPrimary
    palette.base: theme.bg0
    palette.text: theme.textPrimary
    palette.placeholderText: theme.textSecondary
    palette.button: theme.bg2
    palette.buttonText: theme.textPrimary
    palette.highlight: theme.accent
    palette.highlightedText: "#ffffff"
    DeepBlueTheme {id: theme}
    /** @brief 与应用主题一致且支持键盘操作的按钮。 */
    component ActionButton: Basic.Button {
        id: control
        implicitHeight: 32
        implicitWidth: Math.max(60, implicitContentWidth + 24)
        padding: 8
        opacity: enabled ? 1 : 0.45
        contentItem: Text {text: control.text;color: theme.textPrimary;font.pixelSize: 12;horizontalAlignment: Text.AlignHCenter;verticalAlignment: Text.AlignVCenter}
        background: Rectangle {radius: 6;color: control.down ? theme.accentSoft : (control.hovered ? theme.bg2 : theme.bg1);border.color: control.visualFocus || control.hovered ? theme.accent : theme.border}
    }
    property var programs: []
    property int selected: -1
    property var objects: []
    property int revision: 0
    property string errorText: ""
    readonly property var current: selected >= 0 && selected < programs.length ? programs[selected] : null
    readonly property var aliases: {var r = revision; return current ? Object.keys(current.bindings || {}) : []}
    readonly property var parameterNames: {var r = revision; return current ? Object.keys(current.parameters || {}) : []}
    /** @brief 深拷贝场景配置，取消时不影响已保存文档。 */
    function clone(value) { return JSON.parse(JSON.stringify(value)) }
    /** @brief 更新一个程序属性并刷新绑定。 */
    function change(key, value) { if (current) {current[key] = value; revision++} }
    /** @brief 在打开时捕获场景对象及程序草稿。 */
    function begin() {
        programs = clone(EditorManager.sceneSettings.programs || [])
        var list = [], models = EditorManager.getAllModels()
        for (var i = 0; i < models.length; i++) if (models[i].category === 0)
            list.push({label: models[i].name + " [" + models[i].id + "]", value: String(models[i].id)})
        objects = list; selected = programs.length ? 0 : -1; errorText = ""; revision++; open()
    }
    /** @brief 插入独立状态的新程序。 */
    function add() {
        var list = clone(programs)
        list.push({version: 1, name: "控制程序 " + (list.length + 1), enabled: true, period: 0, start: 0, fuel: 1000000, bindings: {}, source: "/** @brief 场景控制入口。 */\nvoid on_step(void)\n{\n}\n"})
        programs = list; selected = list.length - 1; revision++
    }
    /** @brief 删除当前程序，至少保留一段源码。 */
    function remove() {if (programs.length < 2) return; var list = clone(programs); list.splice(selected, 1); programs = list; selected = Math.min(selected, list.length - 1); revision++}
    /** @brief 调整组合程序的稳定执行顺序。 */
    function move(offset) {var next = selected + offset;if (next < 0 || next >= programs.length) return;var list = clone(programs), value = list[selected];list[selected] = list[next];list[next] = value;programs = list;selected = next;revision++}
    /** @brief 绑定一个英文别名到稳定场景身份。 */
    function bind() {
        var key = aliasInput.text.trim()
        if (!/^[A-Za-z][A-Za-z0-9_]{0,62}$/.test(key) || objectInput.currentIndex < 0) {errorText = "请选择物体，并输入以英文字母开头的别名。"; return}
        var bindings = clone(current.bindings || {}); bindings[key] = objects[objectInput.currentIndex].value;change("bindings", bindings);aliasInput.clear();errorText = ""
    }
    /** @brief 删除绑定并保留源码，编译诊断将提示缺失别名。 */
    function unbind(key) {var bindings = clone(current.bindings);delete bindings[key];change("bindings", bindings)}
    /** @brief 保存具名数值参数，代码通过 parameter_number 读取。 */
    function parameter() {var key = parameterInput.text.trim(), value = Number(parameterValue.text);if (!/^[A-Za-z][A-Za-z0-9_]{0,62}$/.test(key) || !isFinite(value) || parameterValue.text.trim() === "") {errorText = "请输入英文参数名和有限数值。";return}var parameters = clone(current.parameters || {});parameters[key] = value;change("parameters", parameters);parameterInput.clear();parameterValue.clear();errorText = ""}
    /** @brief 删除一个参数，不改写源码。 */
    function removeParameter(key) {var parameters = clone(current.parameters || {});delete parameters[key];change("parameters", parameters)}
    /** @brief 将全部草稿作为一次可撤销操作保存。 */
    function save() {var settings = clone(EditorManager.sceneSettings);settings.programs = clone(programs);if (EditorManager.setSceneSettings(settings)) close();else errorText = "保存失败，请检查调度、预算和对象绑定。"}
    background: Rectangle {color: theme.bg1;radius: 12;border.color: theme.border}
    header: Label {text: root.title;color: theme.textPrimary;font.pixelSize: 18;padding: 18}
    contentItem: ColumnLayout {
        spacing: 10
        Label {Layout.fillWidth: true;wrapMode: Text.Wrap;color: theme.textSecondary;text: "源码随 epscene 保存；求解自动编译。各程序读取同一轮快照，按左侧顺序提交动作；后设置的力覆盖前设置。编辑只影响下一次求解。"}
        RowLayout {
            Layout.fillWidth: true;Layout.fillHeight: true;spacing: 14
            ColumnLayout {
                Layout.preferredWidth: 180;Layout.fillHeight: true
                ListView {
                    Layout.fillWidth: true;Layout.fillHeight: true;clip: true;model: root.programs;spacing: 5
                    delegate: Basic.ItemDelegate {
                        required property int index
                        required property var modelData
                        width: ListView.view.width
                        text: (modelData.enabled === false ? "○ " : "● ") + modelData.name
                        highlighted: root.selected === index
                        contentItem: Text {text: parent.text;color: theme.textPrimary;font.pixelSize: 13;elide: Text.ElideRight;verticalAlignment: Text.AlignVCenter}
                        background: Rectangle {radius: 6;color: parent.highlighted ? theme.accentSoft : (parent.hovered ? theme.bg2 : theme.bg0);border.color: parent.highlighted ? theme.accent : theme.border}
                        onClicked: root.selected = index
                    }
                }
                RowLayout {ActionButton {text: "添加";enabled: root.programs.length < 32;onClicked: root.add()} ActionButton {text: "删除";enabled: root.programs.length > 1;onClicked: root.remove()}}
                RowLayout {ActionButton {text: "上移";onClicked: root.move(-1)} ActionButton {text: "下移";onClicked: root.move(1)}}
            }
            ColumnLayout {
                Layout.fillWidth: true;Layout.fillHeight: true;enabled: root.current !== null
                RowLayout {
                    EgretTextField {Layout.fillWidth: true;text: root.current ? root.current.name : "";placeholderText: "程序名称";onTextEdited: root.change("name", text)}
                    EgretCheckBox {text: "启用";checked: root.current ? root.current.enabled !== false : false;onClicked: root.change("enabled", checked)}
                }
                RowLayout {
                    Label {text: "周期（秒，0 为每步）";color: theme.textSecondary}
                    EgretTextField {Layout.preferredWidth: 90;text: root.current ? String(root.current.period || 0) : "0";validator: DoubleValidator {bottom: 0}onTextEdited: root.change("period", Number(text))}
                    Label {text: "开始时间";color: theme.textSecondary}
                    EgretTextField {Layout.preferredWidth: 90;text: root.current ? String(root.current.start || 0) : "0";validator: DoubleValidator {bottom: 0}onTextEdited: root.change("start", Number(text))}
                    Label {text: "结束时间（空为持续）";color: theme.textSecondary}
                    EgretTextField {Layout.preferredWidth: 90;text: root.current && root.current.end !== undefined ? String(root.current.end) : "";validator: DoubleValidator {bottom: 0}onTextEdited: {if (text.trim() === "" && root.current) {delete root.current.end;root.revision++}else root.change("end", Number(text))}}
                }
                Label {Layout.fillWidth: true;wrapMode: Text.Wrap;color: theme.textSecondary;text: "到达结束时间只停止回调，已设置的持久力仍保持；请在代码中显式归零。对象同时可用作克隆模板。"}
                RowLayout {
                    EgretTextField {id: aliasInput;Layout.preferredWidth: 150;placeholderText: "英文别名，如 board"}
                    EgretComboBox {id: objectInput;Layout.fillWidth: true;model: root.objects;textRole: "label";currentIndex: root.objects.length ? 0 : -1}
                    ActionButton {text: "绑定对象";onClicked: root.bind()}
                    ActionButton {text: "插入示例";enabled: root.aliases.length > 0;onClicked: {
                        var alias = root.aliases[0]
                        root.change("source", "/** @brief 1 秒开始施力，3 秒结束。 */\nvoid on_step(void)\n{\n    Entity body = scene_entity(\"" + alias + "\");\n    if (simulation_time() >= 1.0 && !state_bool(\"started\")) {\n        entity_set_force(body, vector3(26.0, 0.0, 0.0));\n        state_set_bool(\"started\", true);\n    }\n    if (simulation_time() >= 3.0 && !state_bool(\"ended\")) {\n        entity_set_force(body, vector3(0.0, 0.0, 0.0));\n        state_set_bool(\"ended\", true);\n    }\n}\n")
                    }}
                }
                Flow {
                    Layout.fillWidth: true;Layout.preferredHeight: Math.min(90, implicitHeight);spacing: 5
                    Repeater {model: root.aliases;delegate: ActionButton {required property string modelData;text: modelData + " → " + root.current.bindings[modelData] + " ×";onClicked: root.unbind(modelData)}}
                }
                RowLayout {
                    EgretTextField {id: parameterInput;Layout.preferredWidth: 150;placeholderText: "参数名，如 gain"}
                    EgretTextField {id: parameterValue;Layout.preferredWidth: 120;placeholderText: "数值";validator: DoubleValidator {}}
                    ActionButton {text: "设置参数";onClicked: root.parameter()}
                    Label {text: "parameter_number(\"gain\")";color: theme.textSecondary}
                }
                Flow {Layout.fillWidth: true;Layout.preferredHeight: Math.min(70, implicitHeight);spacing: 5;Repeater {model: root.parameterNames;delegate: ActionButton {required property string modelData;text: modelData + " = " + root.current.parameters[modelData] + " ×";onClicked: root.removeParameter(modelData)}}}
                RowLayout {
                    Label {text: "C 源码";color: theme.textPrimary}
                    Item {Layout.fillWidth: true}
                    Label {text: "字体";color: theme.textSecondary}
                    EgretComboBox {
                        Layout.preferredWidth: 200
                        model: CodeEditorSettings.fontFamilies
                        currentIndex: Math.max(0, model.indexOf(CodeEditorSettings.fontFamily))
                        onActivated: CodeEditorSettings.fontFamily = currentText
                    }
                    Label {text: "字号";color: theme.textSecondary}
                    ActionButton {text: "−";implicitWidth: 32;enabled: CodeEditorSettings.fontSize > CodeEditorSettings.minimumFontSize;onClicked: CodeEditorSettings.fontSize--}
                    Label {text: CodeEditorSettings.fontSize + " px";color: theme.textPrimary}
                    ActionButton {text: "+";implicitWidth: 32;enabled: CodeEditorSettings.fontSize < CodeEditorSettings.maximumFontSize;onClicked: CodeEditorSettings.fontSize++}
                    ActionButton {text: "默认";onClicked: CodeEditorSettings.resetToDefaults()}
                }
                CodeEditor {
                    id: codeEditor
                    objectName: "codeEditor"
                    Layout.fillWidth: true;Layout.fillHeight: true;Layout.minimumHeight: 130
                    source: {var revision = root.revision;return root.current ? root.current.source : ""}
                    onEdited: function(value) {if (root.current) root.current.source = value}
                }
                Label {Layout.fillWidth: true;wrapMode: Text.Wrap;color: theme.textSecondary;text: "常用接口：scene_entity / scene_template / scene_spawn / entity_destroy / entity_position / entity_set_velocity / entity_set_force / state_number / state_entity / control_delta。仅定义 void on_step(void)，可使用变量、函数、循环和具名状态。"}
            }
        }
        Label {Layout.fillWidth: true;color: theme.danger;text: root.errorText;visible: text.length > 0;wrapMode: Text.Wrap}
        ScrollView {Layout.fillWidth: true;Layout.preferredHeight: 86;clip: true;Basic.TextArea {text: EditorManager.controlDiagnostics;readOnly: true;selectByMouse: true;wrapMode: TextEdit.Wrap;color: theme.textSecondary;selectionColor: theme.accentSoft;font.family: CodeEditorSettings.fontFamily;font.pixelSize: 12;background: Rectangle {color: theme.bg0;radius: 6;border.color: theme.border}}}
        RowLayout {
            ActionButton {text: EditorManager.controlValidationRunning ? "正在检查…" : "检查当前草稿";enabled: !EditorManager.controlValidationRunning;onClicked: EditorManager.validateControlPrograms(root.clone(root.programs))}
            BusyIndicator {running: EditorManager.controlValidationRunning;visible: running;Layout.preferredWidth: 28;Layout.preferredHeight: 28}
            Item {Layout.fillWidth: true}
            ActionButton {text: "取消";onClicked: root.close()}
            ActionButton {text: "保存全部程序";onClicked: root.save()}
        }
    }
}
