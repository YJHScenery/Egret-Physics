/** @file Main.qml @brief 三维矢量场交互验证界面。 */
import QtQuick
import QtQuick.Controls
import QtQuick3D
import FieldDemo 1.0
ApplicationWindow {
    id: root
    visible: true; width: 1200; height: 800; title: "三维矢量场实例化验证"
    property real extent: 8
    property real phase: 0
    Timer { interval: 50; running: true; repeat: true; onTriggered: root.phase += 0.05 }
    View3D {
        anchors.fill: parent
        environment: SceneEnvironment { clearColor: "#111b2d"; backgroundMode: SceneEnvironment.Color; antialiasingMode: SceneEnvironment.MSAA }
        PerspectiveCamera { position: Qt.vector3d(0, root.extent*1.4, root.extent*3.2); eulerRotation.x: -20; clipFar: 300 }
        DirectionalLight { eulerRotation.x: -30; eulerRotation.y: -30; brightness: 1 }
        Model {
            geometry: ArrowGeometry {}
            instancing: FieldInstances { id: instances; time: root.phase; extent: root.extent }
            materials: PrincipledMaterial { baseColor: "#50caff"; lighting: PrincipledMaterial.NoLighting; cullMode: Material.NoCulling }
        }
    }
    Rectangle {
        anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 16; width: 380; height: 110; radius: 8; color: "#cc15243a"
        Column { anchors.fill: parent; anchors.margins: 12; spacing: 8
            Label { color: "white"; text: "滚轮缩放采样范围：" + root.extent.toFixed(1) + "　箭头：" + instances.count }
            Label { color: "#a9bdd4"; text: "旋涡场 + 随时间变化的垂直分量" }
            Slider { width: 350; from: 2; to: 30; value: root.extent; onMoved: root.extent = value }
        }
    }
    WheelHandler { onWheel: function(event) { root.extent = Math.max(2, Math.min(30, root.extent * (event.angleDelta.y > 0 ? 0.9 : 1.1))) } }
}
