/** @file SceneScaleControls.qml @brief 显示参考尺度、微小物体标记与自动取景工具栏。 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "qrc:/components/components/theme"
import "qrc:/components/components/basic"

Rectangle {
    id:root
    DeepBlueTheme { id: theme }
    property real referenceLength:1
    property bool markers:true
    property bool selectionAvailable:false
    signal referenceRequested(real value)
    signal markersRequested(bool value)
    signal fitRequested(bool selectedOnly)
    color:theme.bg1;radius:theme.radiusS;implicitHeight:48
    border.width:1;border.color:theme.border
    RowLayout {
        anchors.fill:parent;anchors.margins:6;spacing:6
        Label {text:"显示参考";color:theme.textSecondary;font.pixelSize:12}
        EgretComboBox {
            id:presets;Layout.preferredWidth:100;model:["厘米","米","千米","自定义"]
            currentIndex:root.referenceLength===0.01?0:(root.referenceLength===1?1:(root.referenceLength===1000?2:3))
            onActivated:if(currentIndex<3)root.referenceRequested([0.01,1,1000][currentIndex])
        }
        EgretTextField {
            id:custom;visible:presets.currentIndex===3;Layout.preferredWidth:100
            text:String(root.referenceLength);placeholderText:"参考长度/m"
            validator:DoubleValidator {bottom:1e-12;top:1e12;notation:DoubleValidator.ScientificNotation}
            onEditingFinished:if(acceptableInput)root.referenceRequested(Number(text))
        }
        Label {visible:presets.currentIndex===3;text:"m";color:theme.textSecondary;font.pixelSize:12}
        EgretCheckBox {text:"小物体标记";checked:root.markers;onClicked:root.markersRequested(checked)}
        Item {Layout.fillWidth:true}
        EgretPushButton {Layout.fillWidth:false;Layout.preferredWidth:76;Layout.preferredHeight:32;text:"适应场景";onClicked:root.fitRequested(false)}
        EgretPushButton {Layout.fillWidth:false;Layout.preferredWidth:76;Layout.preferredHeight:32;text:"适应所选";enabled:root.selectionAvailable;onClicked:root.fitRequested(true)}
    }
}
