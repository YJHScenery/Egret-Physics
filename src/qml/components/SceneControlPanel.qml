/** @file SceneControlPanel.qml @brief 教师可用的时间轴条件卡片与多条件、多动作编辑表单。 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EditorManager 1.0
import "qrc:/components/components/theme"
import "qrc:/components/components/basic"

ColumnLayout {
    id: root
    DeepBlueTheme { id: theme }
    spacing: 8

    /** @brief 与属性面板统一的标签排版。 */
    component FormLabel: Label {
        color: theme.textSecondary
        font.pixelSize: 12
    }
    /** @brief 根据文字宽度布局的主题按钮。 */
    component PanelButton: EgretPushButton {
        Layout.fillWidth: false
        Layout.preferredHeight: 32
        implicitWidth: Math.max(64, text.length * pixelSize + 24)
        implicitHeight: 32
    }
    /** @brief 条件和动作使用同一套深蓝卡片与标题。 */
    component RuleGroup: GroupBox {
        id: group
        padding: 14
        topPadding: 34
        label: FormLabel {
            x: 14
            y: 10
            text: group.title
            color: theme.textPrimary
            font.bold: true
        }
        background: Rectangle {
            color: theme.bg0
            radius: theme.radiusS
            border.width: 1
            border.color: theme.bg2
        }
    }
    /** @brief 统一滚动条的深蓝底色与悬浮反馈。 */
    component PanelScrollBar: ScrollBar {
        id: bar
        orientation: Qt.Vertical
        policy: ScrollBar.AsNeeded
        visible: policy === ScrollBar.AlwaysOn || (policy !== ScrollBar.AlwaysOff && size < 1)
        contentItem: Rectangle {
            implicitWidth: 6
            implicitHeight: 6
            radius: 3
            color: bar.pressed ? theme.accent : (bar.hovered ? theme.border : theme.bg2)
        }
    }
    property var draft: null
    property int editIndex: -1
    property int revision: 0
    property string errorText: ""
    readonly property var rules: EditorManager.sceneSettings.rules || []
    property var objects: []
    readonly property var kinds: [
        {label:"仿真时间",value:"Time"},{label:"位置",value:"Position"},{label:"速度",value:"Velocity"},
        {label:"角速度",value:"AngularVelocity"},{label:"相对位置 A−B",value:"RelativePosition"},
        {label:"相对速度 A−B",value:"RelativeVelocity"},{label:"相对角速度 A−B",value:"RelativeAngularVelocity"},
        {label:"两物体距离",value:"Distance"},{label:"物体参与求解",value:"Active"},
        {label:"物体处于休眠",value:"Asleep"},{label:"两物体接触",value:"Contact"},{label:"始终成立",value:"Always"}]
    readonly property var comparisons: [ {label:"大于等于",value:"GreaterEqual"},{label:"大于",value:"Greater"},
        {label:"小于等于",value:"LessEqual"},{label:"小于",value:"Less"},{label:"近似等于",value:"Near"},
        {label:"区间内 [下限,上限)",value:"Inside"},{label:"区间外",value:"Outside"}]
    readonly property var actions: [{label:"设置控制力",value:"SetForce"},{label:"设置速度",value:"SetVelocity"},
        {label:"设置位置",value:"SetPosition"},{label:"物体启用／停用",value:"SetActive"}]
    /** @brief 深拷贝设置，避免未确认的表单修改影响场景。 */
    function clone(value) { return JSON.parse(JSON.stringify(value)); }
    /** @brief 刷新物体选择项，使用稳定 ID 避免同名对象混淆。 */
    function refreshObjects() {
        var items=[];var models=EditorManager.getAllModels();
        for(var i=0;i<models.length;++i) if(models[i].category===0)
            items.push({label:models[i].name+" ["+models[i].id+"]",value:models[i].id});
        objects=items;
    }
    /** @brief 根据持久标识定位下拉选项，失效引用保持未选中。 */
    function indexOf(items,value) { for(var i=0;i<items.length;++i) if(items[i].value===value)return i;return -1; }
    /** @brief 创建可直接编辑的时间条件。 */
    function condition() { return {kind:"Time",comparison:"GreaterEqual",component:"X",lower:1,upper:2,tolerance:0.000001,first:objects.length?objects[0].value:"",second:objects.length>1?objects[1].value:(objects.length?objects[0].value:""),expected:true,negate:false}; }
    /** @brief 创建默认恒力动作。 */
    function action() {return {kind:"SetForce",target:objects.length?objects[0].value:"",value:[0,0,0],enabled:true};}
    /** @brief 打开新增或修改规则表单。 */
    function edit(index) {refreshObjects();editIndex=index;draft=index<0?{name:"新规则",enabled:true,trigger:"Once",match:"All",conditions:[condition()],actions:[action()]}:clone(rules[index]);errorText="";revision++;ruleDialog.open();}
    /** @brief 修改条件字段并触发局部绑定刷新。 */
    function changeCondition(index,key,value) {draft.conditions[index][key]=value;revision++;}
    /** @brief 修改动作字段并触发局部绑定刷新。 */
    function changeAction(index,key,value) {draft.actions[index][key]=value;revision++;}
    /** @brief 保存整个规则批次，后端校验失败时保留表单。 */
    function save() {
        var settings=clone(EditorManager.sceneSettings);var list=clone(rules);
        if(editIndex<0)list.push(draft);else list[editIndex]=draft;
        settings.rules=list;
        if(EditorManager.setSceneSettings(settings)) ruleDialog.close();else errorText="保存失败，请检查数值、区间与物体选择。";
    }
    /** @brief 删除一条规则，交由场景管理器保存撤销历史。 */
    function remove(index) {var settings=clone(EditorManager.sceneSettings);var list=clone(rules);list.splice(index,1);settings.rules=list;EditorManager.setSceneSettings(settings);}
    /** @brief 判断条件是否需要第二个物体。 */
    function relative(kind) {return kind.indexOf("Relative")===0 || kind==="Distance" || kind==="Contact";}
    /** @brief 判断条件是否使用布尔状态。 */
    function isStateCondition(kind) {return kind==="Active" || kind==="Asleep" || kind==="Contact";}
    /** @brief 返回物理量的输入单位。 */
    function unit(kind) {if(kind==="Time")return "秒";if(kind.indexOf("Angular")>=0)return "rad/s";if(kind.indexOf("Velocity")>=0)return "m/s";return "米";}
    Connections {target:EditorManager;function onModelListChanged(){root.refreshObjects();}}
    Component.onCompleted: refreshObjects()
    RowLayout {
        Layout.fillWidth:true
        FormLabel {text:"时间轴与条件控制";color:theme.textPrimary;font.bold:true}
        FormLabel {text:root.rules.length+" 条规则";color:theme.textSecondary}
        Item {Layout.fillWidth:true}
        PanelButton {text:"＋ 添加规则";onClicked:root.edit(-1)}
        PanelButton {text:"C 代码控制";onClicked:codeDialog.begin()}
    }
    FormLabel {visible:root.rules.length===0;text:"例如：1 秒后施加拉力；接触后改变速度；越过边界后停用物体。";color:theme.textSecondary;Layout.fillWidth:true;wrapMode:Text.Wrap}
    ScrollView {
        id:ruleListScroll
        Layout.fillWidth:true;Layout.fillHeight:true;clip:true;contentWidth:availableWidth
        ScrollBar.vertical: PanelScrollBar {
            id: ruleListBar
            parent: ruleListScroll
            x: ruleListScroll.mirrored ? 0 : ruleListScroll.width - width
            y: ruleListScroll.topPadding
            height: ruleListScroll.availableHeight
        }
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ColumnLayout {
            width:ruleListScroll.availableWidth-(ruleListBar.visible?ruleListBar.width+6:0)
            Repeater {
                model:root.rules
                delegate:RowLayout {
                    required property int index
                    required property var modelData
                    Layout.fillWidth:true
                    EgretCheckBox {checked:modelData.enabled!==false;text:modelData.name || "规则 "+(index+1);onClicked:{var s=root.clone(EditorManager.sceneSettings);s.rules[index].enabled=checked;EditorManager.setSceneSettings(s);}}
                    FormLabel {Layout.fillWidth:true;color:theme.textSecondary;text:(modelData.match==="Any"?"任一":"全部")+" "+modelData.conditions.length+" 个条件 → "+modelData.actions.length+" 个动作"}
                    PanelButton {text:"编辑";onClicked:root.edit(index)}
                    PanelButton {text:"删除";onClicked:root.remove(index)}
                }
            }
        }
    }
    ControlCodeDialog {id: codeDialog}
    Dialog {
        id:ruleDialog;parent:Overlay.overlay;modal:true;title:root.editIndex<0?"添加控制规则":"编辑控制规则"
        width:Math.min(1100,parent?parent.width-40:1100);height:Math.min(740,parent?parent.height-40:740)
        anchors.centerIn:parent
        padding: 16
        spacing: 12
        background: Rectangle {
            color: theme.bg1
            radius: theme.radiusM
            border.width: 1
            border.color: theme.border
        }
        header: FormLabel {
            text: ruleDialog.title
            color: theme.textPrimary
            font.pixelSize: 17
            font.bold: true
            leftPadding: 18
            topPadding: 16
            bottomPadding: 8
        }
        Overlay.modal: Rectangle {
            color: Qt.rgba(theme.bg0.r, theme.bg0.g, theme.bg0.b, 0.75)
        }
        contentItem:ColumnLayout {
            spacing: 12
            RowLayout {
                FormLabel {text:"名称"}
                EgretTextField {Layout.fillWidth:true;text:root.draft?root.draft.name:"";onTextEdited:root.draft.name=text}
                EgretComboBox {model:["首次成立执行一次","每次由假变真","成立期间每步执行"];currentIndex:root.draft?["Once","Rising","WhileTrue"].indexOf(root.draft.trigger):0;onActivated:root.draft.trigger=["Once","Rising","WhileTrue"][currentIndex]}
            }
            RowLayout {
                FormLabel {text:"何时满足";font.bold:true}
                EgretComboBox {model:["以下全部条件","以下任一条件"];currentIndex:root.draft&&root.draft.match==="Any"?1:0;onActivated:root.draft.match=currentIndex===1?"Any":"All"}
                PanelButton {text:"＋ 条件";onClicked:{root.draft.conditions.push(root.condition());root.revision++;}}
            }
            ScrollView {
                id:formScroll
                Layout.fillWidth:true;Layout.fillHeight:true;clip:true;contentWidth:availableWidth
                ScrollBar.vertical: PanelScrollBar {
                    id: formBar
                    parent: formScroll
                    x: formScroll.mirrored ? 0 : formScroll.width - width
                    y: formScroll.topPadding
                    height: formScroll.availableHeight
                }
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ColumnLayout {
                    width:formScroll.availableWidth-(formBar.visible?formBar.width+6:0);spacing:10
                    Repeater {
                        model: {root.revision;return root.draft?root.draft.conditions.length:0;}
                        delegate:RuleGroup {
                            id:conditionRow;required property int index
                            property var item:{root.revision;return root.draft.conditions[index]||root.condition();}
                            Layout.fillWidth:true;title:"条件 "+(index+1)
                            ColumnLayout {
                                anchors.fill:parent
                                RowLayout {
                                    EgretComboBox {model:root.kinds;textRole:"label";currentIndex:root.indexOf(root.kinds,conditionRow.item.kind);onActivated:root.changeCondition(conditionRow.index,"kind",root.kinds[currentIndex].value);Layout.preferredWidth:170}
                                    EgretComboBox {visible:conditionRow.item.kind!=="Time"&&conditionRow.item.kind!=="Always";model:root.objects;textRole:"label";currentIndex:root.indexOf(root.objects,conditionRow.item.first);displayText:currentIndex<0?"请选择物体 A":currentText;onActivated:root.changeCondition(conditionRow.index,"first",root.objects[currentIndex].value);Layout.preferredWidth:180}
                                    EgretComboBox {visible:root.relative(conditionRow.item.kind);model:root.objects;textRole:"label";currentIndex:root.indexOf(root.objects,conditionRow.item.second);displayText:currentIndex<0?"请选择物体 B":currentText;onActivated:root.changeCondition(conditionRow.index,"second",root.objects[currentIndex].value);Layout.preferredWidth:180}
                                    EgretComboBox {visible:conditionRow.item.kind!=="Time"&&conditionRow.item.kind!=="Always"&&conditionRow.item.kind!=="Distance"&&!root.isStateCondition(conditionRow.item.kind);model:["X","Y","Z","模长"];currentIndex:["X","Y","Z","Magnitude"].indexOf(conditionRow.item.component);onActivated:root.changeCondition(conditionRow.index,"component",["X","Y","Z","Magnitude"][currentIndex])}
                                    EgretComboBox {visible:root.isStateCondition(conditionRow.item.kind);model:["是","否"];currentIndex:conditionRow.item.expected===false?1:0;onActivated:root.changeCondition(conditionRow.index,"expected",currentIndex===0)}
                                    EgretCheckBox {text:"取反";checked:conditionRow.item.negate===true;onClicked:root.changeCondition(conditionRow.index,"negate",checked)}
                                    Item {Layout.fillWidth:true}
                                    PanelButton {text:"删除";enabled:root.draft.conditions.length>1;onClicked:{root.draft.conditions.splice(conditionRow.index,1);root.revision++;}}
                                }
                                RowLayout {
                                    visible:conditionRow.item.kind!=="Always"&&!root.isStateCondition(conditionRow.item.kind)
                                    EgretComboBox {model:root.comparisons;textRole:"label";currentIndex:root.indexOf(root.comparisons,conditionRow.item.comparison);onActivated:root.changeCondition(conditionRow.index,"comparison",root.comparisons[currentIndex].value);Layout.preferredWidth:180}
                                    FormLabel {text:conditionRow.item.comparison==="Inside"||conditionRow.item.comparison==="Outside"?"下限":"阈值"}
                                    EgretTextField {Layout.preferredWidth:130;text:String(conditionRow.item.lower);onEditingFinished:root.changeCondition(conditionRow.index,"lower",Number(text))}
                                    FormLabel {visible:conditionRow.item.comparison==="Inside"||conditionRow.item.comparison==="Outside";text:"上限（不含）"}
                                    EgretTextField {visible:conditionRow.item.comparison==="Inside"||conditionRow.item.comparison==="Outside";Layout.preferredWidth:130;text:String(conditionRow.item.upper);onEditingFinished:root.changeCondition(conditionRow.index,"upper",Number(text))}
                                    FormLabel {visible:conditionRow.item.comparison==="Near";text:"允许误差"}
                                    EgretTextField {visible:conditionRow.item.comparison==="Near";Layout.preferredWidth:130;text:String(conditionRow.item.tolerance);onEditingFinished:root.changeCondition(conditionRow.index,"tolerance",Number(text))}
                                    FormLabel {text:root.unit(conditionRow.item.kind)}
                                }
                            }
                        }
                    }
                    RowLayout {FormLabel {text:"执行以下动作";font.bold:true} PanelButton {text:"＋ 动作";onClicked:{root.draft.actions.push(root.action());root.revision++;}}}
                    Repeater {
                        model:{root.revision;return root.draft?root.draft.actions.length:0;}
                        delegate:RuleGroup {
                            id:actionRow;required property int index
                            property var item:{root.revision;return root.draft.actions[index]||root.action();}
                            Layout.fillWidth:true;title:"动作 "+(index+1)
                            RowLayout {
                                anchors.fill:parent
                                EgretComboBox {model:root.actions;textRole:"label";currentIndex:root.indexOf(root.actions,actionRow.item.kind);onActivated:root.changeAction(actionRow.index,"kind",root.actions[currentIndex].value)}
                                EgretComboBox {model:root.objects;textRole:"label";currentIndex:root.indexOf(root.objects,actionRow.item.target);displayText:currentIndex<0?"请选择目标物体":currentText;onActivated:root.changeAction(actionRow.index,"target",root.objects[currentIndex].value);Layout.preferredWidth:180}
                                Repeater {
                                    model:actionRow.item.kind==="SetActive"?0:3
                                    delegate:EgretTextField {
                                        required property int index
                                        Layout.preferredWidth:110;placeholderText:["X","Y","Z"][index]
                                        text:String(actionRow.item.value[index])
                                        onEditingFinished:{var vector=root.clone(actionRow.item.value);vector[index]=Number(text);root.changeAction(actionRow.index,"value",vector);}
                                    }
                                }
                                EgretComboBox {visible:actionRow.item.kind==="SetActive";model:["启用","停用（保留历史）"];currentIndex:actionRow.item.enabled===false?1:0;onActivated:root.changeAction(actionRow.index,"enabled",currentIndex===0)}
                                FormLabel {text:actionRow.item.kind==="SetForce"?"N":(actionRow.item.kind==="SetVelocity"?"m/s":(actionRow.item.kind==="SetPosition"?"m":""))}
                                Item {Layout.fillWidth:true}
                                PanelButton {text:"删除";enabled:root.draft.actions.length>1;onClicked:{root.draft.actions.splice(actionRow.index,1);root.revision++;}}
                            }
                        }
                    }
                    FormLabel {Layout.fillWidth:true;wrapMode:Text.Wrap;text:"所有数值使用物理单位，支持 1e-8 等科学计数法。控制力会持续保持；结束时另设一条规则，将控制力设为 (0,0,0)。接触读取上一求解步；时间条件按步边界检测。";color:theme.textSecondary}
                    FormLabel {Layout.fillWidth:true;wrapMode:Text.Wrap;visible:root.errorText!=="";text:root.errorText;color:theme.danger}
                }
            }
        }
        footer: Rectangle {
            implicitHeight: 58
            color: theme.bg1
            RowLayout {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 16
                spacing: 8
                PanelButton {text:"取消";onClicked:ruleDialog.close()}
                PanelButton {text:"保存规则";border.color:theme.accent;onClicked:root.save()}
            }
        }
    }
}
