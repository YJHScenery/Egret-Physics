/**
 * @file        EditorUnit.qml
 * @brief       编辑器单元视图文件
 * @details     定义物理引擎编辑器界面，提供物理实体的编辑功能。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-27
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     View
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

// noinspection UnnecessaryLabelJS

import QtQuick 2.15
import QtQuick.Controls 6.9
import QtQuick.Controls.Basic 6.9 as Basic
import QtQuick.Layouts 1.15
import QtQuick3D 6.9
import QtQuick.Dialogs 6.9
import EditorManager 1.0
import EnumHandler 1.0
import EditorManager 1.0
import ClipboardManager 1.0

import "qrc:/components/components"
import "qrc:/components/components/message_box"
import "qrc:/components/components/theme"
import "qrc:/components/components/basic"

ColumnLayout {
    id: editorRoot

    spacing: 10

    DeepBlueTheme {
        id: theme
    }

    EgretMessageDialog {
        id: msgDialog
        level: EgretMessageDialog.Level.Warning
        title: "Default Title"
        text: "Default Text"
        buttons: EgretMessageDialog.StandardButton.Ok
        soundType: "Warning"
    }

    Connections {
        target: EditorManager

        function onWarning(title, message) {
            msgDialog.title = title;
            msgDialog.text = message;
            msgDialog.soundType = "Warning";
            msgDialog.open();
        }

        function onInformation(title, message) {
            msgDialog.title = title;
            msgDialog.text = message;
            msgDialog.soundType = "Information";
            msgDialog.open();
        }

        function onError(title, message) {
            msgDialog.title = title;
            msgDialog.text = message;
            msgDialog.soundType = "Error";
            msgDialog.open();
        }
    }

    // ── 剪贴板操作辅助函数 ──
    /**
     * @brief 获取当前选中的 Shape 实体 ID 列表。
     * @details 优先使用左侧栏多选（sceneEntityListPanel.selectedEntityIds），
     *          否则使用 3D 视口单选（editorCanvas.selectedObjectName）。
     *          仅返回 category == 0 的 Shape 实体。
     */
    function getSelectedShapeIds() {
        var ids = [];

        // 优先左侧栏多选
        var sidebarIds = sceneEntityListPanel.selectedEntityIds;
        if (sidebarIds && sidebarIds.length > 0) {
            for (var i = 0; i < sidebarIds.length; ++i) {
                var m = EditorManager.getModel(sidebarIds[i]);
                if (m && m.category === 0) {
                    ids.push(sidebarIds[i]);
                }
            }
            return ids;
        }

        // 其次 3D 视口单选
        var pickedName = editorCanvas.selectedObjectName;
        if (pickedName && pickedName !== "") {
            var models = EditorManager.findModelsByName(pickedName);
            if (models.length > 0 && models[0].category === 0) {
                ids.push(models[0].id);
            }
        }

        return ids;
    }

    // ── 锚点位置（单摆约束使用） ──
    property real anchorPosX: 0.0
    property real anchorPosY: 0.0
    property real anchorPosZ: 0.0

    // ── 约束对象选择是否有效 ──
    readonly property bool constraintSelectionValid: {
        if (!inspectorColumn.isConstraintCategory || !operatingButton.isCreateMode) return true;
        var tid = EditorParameterManager.currentTypeId;
        var count = sceneEntityListPanel.selectedEntityIds.length;
        if (tid === 1 || tid === 3 || tid === 5) return count === 2;  // 连接线/连接杆/弹簧
        if (tid === 2) return count === 1;                              // 单摆
        if (tid === 7) return count >= 1;                              // 空气阻力
        return true;
    }

    // ── 剪贴板公开函数（供 MainWindow MenuBar 快捷键调用） ──
    /** 复制当前选中的 Shape 实体到剪贴板 */
    function copySelectedEntities() {
        var ids = getSelectedShapeIds();
        if (ids.length > 0) {
            ClipboardManager.copyEntities(ids);
            console.log("Copy:", ids.length, "entities");
        }
    }

    /** 从剪贴板粘贴实体 */
    function pasteEntitiesFromClipboard() {
        if (ClipboardManager.hasContent) {
            var newIds = ClipboardManager.pasteEntities();
            console.log("Paste:", newIds.length, "entities, IDs:", newIds);
            return newIds;
        }
        return [];
    }

    /** 剪切当前选中的 Shape 实体 */
    function cutSelectedEntities() {
        var ids = getSelectedShapeIds();
        if (ids.length > 0) {
            editorGizmo.detach();
            editorCanvas.gizmoActive = false;
            editorCanvas.selectedObjectName = "";
            ClipboardManager.cutEntities(ids);
            console.log("Cut:", ids.length, "entities");
        }
    }

    // ── 右键上下文菜单 ──
    EgretPopupMenu {
        id: clipboardContextMenu
        // 每次打开前由 buildContextMenuItems() 动态填充 menuItems
    }

    /** 构建右键菜单项数组（每次打开时重新求值，确保 enabled 状态是最新的） */
    function buildContextMenuItems() {
        var hasSelection = getSelectedShapeIds().length > 0;
        return [
            {
                text: "复制",
                shortcut: "Ctrl+C",
                enabled: hasSelection,
                icon: "qrc:/main_icons/assets/icons/copy.svg",
                onTriggered: function () {
                    var ids = getSelectedShapeIds();
                    if (ids.length > 0) {
                        ClipboardManager.copyEntities(ids);
                        console.log("ContextMenu Copy:", ids.length, "entities");
                    }
                }
            },
            {
                text: "粘贴",
                shortcut: "Ctrl+V",
                enabled: ClipboardManager.hasContent,
                icon: "qrc:/main_icons/assets/icons/paste.svg",
                onTriggered: function () {
                    var newIds = ClipboardManager.pasteEntities();
                    console.log("ContextMenu Paste:", newIds.length, "entities");
                }
            },
            {
                text: "剪切",
                shortcut: "Ctrl+X",
                enabled: hasSelection,
                icon: "qrc:/main_icons/assets/icons/cut.svg",
                onTriggered: function () {
                    var ids = getSelectedShapeIds();
                    if (ids.length > 0) {
                        editorGizmo.detach();
                        editorCanvas.gizmoActive = false;
                        editorCanvas.selectedObjectName = "";
                        ClipboardManager.cutEntities(ids);
                        console.log("ContextMenu Cut:", ids.length, "entities");
                    }
                }
            },
            {separator: true},
            {
                text: "撤销",
                shortcut: "Ctrl+Z",
                icon: "qrc:/main_icons/assets/icons/undo.svg",
                onTriggered: function () {
                    EditorManager.undo();
                }
            },
            {
                text: "重做",
                shortcut: "Ctrl+Y",
                icon: "qrc:/main_icons/assets/icons/recover.svg",
                onTriggered: function () {
                    EditorManager.redo();
                }
            }
        ];
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 10

        SceneEntityListPanel {
            id: sceneEntityListPanel
            Layout.preferredWidth: 200
            Layout.minimumWidth: 200
            Layout.maximumWidth: 240
            Layout.fillHeight: true
        }

        GlassPanel {
            id: editorViewportPanel
            Layout.fillWidth: true
            Layout.minimumWidth: 520
            Layout.fillHeight: true
            title: "场景编辑器 / Infinite Stage"

            Item {
                id: editorCanvas
                anchors.fill: parent
                property string selectedObjectName: ""
                property real orbitYaw: 35
                property real orbitPitch: 45
                property real orbitDistance: 1200
                property real orbitTargetX: 0
                property real orbitTargetY: 0
                property real orbitTargetZ: 0
                property real gridSpacing: 100
                property int gridCount: 401
                property point lastMousePoint: Qt.point(0, 0)
                property bool gizmoActive: false

                function clamp(value, minValue, maxValue) {
                    return Math.max(minValue, Math.min(maxValue, value));
                }

                function updateGridParams() {
                    const distance = orbitDistance;
                    const spacing = distance < 800 ? 50 : (distance < 2500 ? 100 : 200);
                    if (Math.abs(spacing - gridSpacing) > 0.1) {
                        gridSpacing = spacing;
                    }
                    const desired = Math.floor((distance * 2.2) / Math.max(gridSpacing, 0.1)) * 2 + 1;
                    gridCount = Math.max(201, Math.min(desired, 1201));
                }

                function cameraPhysicsPosition() {
                    const yawRad = orbitYaw * Math.PI / 180.0;
                    const pitchRad = orbitPitch * Math.PI / 180.0;
                    const cp = Math.cos(pitchRad);
                    const sp = Math.sin(pitchRad);
                    const cy = Math.cos(yawRad);
                    const sy = Math.sin(yawRad);

                    const px = orbitTargetX + orbitDistance * cp * cy;
                    const py = orbitTargetY + orbitDistance * cp * sy;
                    const pz = orbitTargetZ + orbitDistance * sp;
                    return Qt.vector3d(px, py, pz);
                }

                function toRenderVector(x, y, z) {
                    return Qt.vector3d(x, z, y);
                }

                // ── Quick3D (y-up) → 数学坐标系 (z-up) 逆向转换 ──
                // 正向: buildQuick3DRenderPosition  (math.y, math.z, math.x)
                // 逆向: (q3d.z, q3d.x, q3d.y)

                function fromRenderPosition(pos) {
                    return Qt.vector3d(pos.z, pos.x, pos.y);
                }

                // 正向: buildQuick3DRenderRotation  (math.scalar, math.y, math.z, math.x)
                // 逆向: (q3d.scalar, q3d.z, q3d.x, q3d.y)

                function fromRenderRotation(quat) {
                    return Qt.quaternion(quat.scalar, quat.z, quat.x, quat.y);
                }

                // Gizmo 拖拽松手后，将 Quick3D 坐标同步到 inspectorPanel（数学 z-up）
                function syncGizmoToInspector() {
                    if (!editorGizmo.targetNode) return;
                    var q3dPos = editorGizmo.targetNode.position;
                    var q3dRot = editorGizmo.targetNode.rotation;
                    inspectorPanel.m_pos = fromRenderPosition(q3dPos);
                    inspectorPanel.m_rotation = fromRenderRotation(q3dRot);
                    console.log("Gizmo released — math pos:", inspectorPanel.m_pos,
                        "math rot:", inspectorPanel.m_rotation);
                }

                function updateCameraPose() {
                    updateGridParams();
                    const cam = cameraPhysicsPosition();
                    const target = toRenderVector(orbitTargetX, orbitTargetY, orbitTargetZ);
                    editorCamera.position = toRenderVector(cam.x, cam.y, cam.z);
                    editorCamera.lookAt(target);
                }

                Component.onCompleted: {
                    updateCameraPose();
                }

                View3D {
                    id: editorView
                    anchors.fill: parent
                    camera: editorCamera
                    // refreshMode: View3D.OnDemand

                    environment: SceneEnvironment {
                        backgroundMode: SceneEnvironment.Color
                        clearColor: "#061425"
                        antialiasingMode: SceneEnvironment.MSAA
                        antialiasingQuality: SceneEnvironment.VeryHigh
                    }

                    PerspectiveCamera {
                        id: editorCamera
                        fieldOfView: 40
                        clipNear: 1
                        clipFar: 80000
                    }

                    DirectionalLight {
                        eulerRotation: Qt.vector3d(-35, 25, 15)
                        brightness: 1.6
                    }

                    DirectionalLight {
                        eulerRotation: Qt.vector3d(-120, -25, -80)
                        brightness: 0.8
                    }

                    // 基础配置属性
                    ListModel {
                        id: basicConfigListModel
                        Component.onCompleted: {
                            append([]);
                        }
                    }

                    Node {
                        id: basicNode

                        property var alphaModeMap: ({
                            "Default": PrincipledMaterial.Default,
                            "Blend": PrincipledMaterial.Blend,
                            "Opaque": PrincipledMaterial.Opaque,
                            "Mask": PrincipledMaterial.Mask
                        })

                        property var modelList: EditorManager.getAllModels()

                        Connections {
                            target: EditorManager

                            function onModelListChanged() {
                                console.log("Model list changed, refreshing Repeater3D");
                                basicNode.modelList = [] // 不要删！！！这是强制让这个勾八QML引擎刷新的
                                basicNode.modelList = EditorManager.getAllModels();
                            }
                        }

                        Coordinate {
                            id: editorCoordinate
                            orbitDistance: editorCanvas.orbitDistance
                            gridStep: editorCanvas.gridSpacing
                            maxGridLines: editorCanvas.gridCount
                            gridExtent: (editorCanvas.gridCount - 1) * editorCanvas.gridSpacing * 0.5
                            camera: editorCamera
                            viewportWidth: editorView.width
                            viewportHeight: editorView.height
                        }

                        Repeater3D {
                            id: repeater
                            model: basicNode.modelList

                            delegate: Model {
                                id: delegateModel

                                property var materialAlphaMode: basicNode.alphaModeMap[modelData.materials.alphaMode]
                                property bool isSelected: editorCanvas.selectedObjectName === objectName
                                property var transform: EditorManager.setQuick3DRenderTransform(index)


                                objectName: modelData.name ? modelData.name : ("model-" + index)

                                source: modelData.source

                                // z-up 到 y-up 的交换
                                // position: Qt.vector3d(modelData.pos.x, modelData.pos.z, -modelData.pos.y)

                                position: transform["position"]
                                scale: transform["scale"]
                                rotation: transform["rotation"] // modelData.rotation

                                pickable: true

                                materials: PrincipledMaterial {
                                    baseColor: delegateModel.isSelected ? "#5CC8FF" : modelData.materials.baseColor
                                    roughness: modelData.materials.roughness
                                    metalness: modelData.materials.metalness
                                    emissiveFactor: delegateModel.isSelected ? "#1A6AA6" : "#000000"
                                    alphaMode: delegateModel.materialAlphaMode ? delegateModel.materialAlphaMode : PrincipledMaterial.Default
                                    cullMode: Material.NoCulling
                                }

                            }
                        }

                    }
                }

                // ── Gizmo 叠加层 (独立 View3D, Offscreen, 透明背景) ──

                View3D {
                    id: gizmoView
                    anchors.fill: parent
                    renderMode: View3D.Offscreen
                    camera: gizmoCamera
                    z: 1

                    environment: SceneEnvironment {
                        clearColor: "#00000000"
                        backgroundMode: SceneEnvironment.Transparent
                    }

                    PerspectiveCamera {
                        id: gizmoCamera
                        position: editorCamera.position
                        rotation: editorCamera.rotation
                        clipNear: editorCamera.clipNear
                        clipFar: editorCamera.clipFar
                        fieldOfView: editorCamera.fieldOfView
                    }

                    DirectionalLight {
                        eulerRotation: Qt.vector3d(-350, 250, 150)
                        brightness: 1.6
                    }

                    DirectionalLight {
                        eulerRotation: Qt.vector3d(-120, -120, 80)
                        brightness: 1.6
                    }

                    TransformGizmo {
                        id: editorGizmo
                        view3d: gizmoView
                        camera: gizmoCamera
                        visible: targetNode !== null
                        canScale: false

                        // 动态缩放：gizmo 大小随相机距离变化
                        size: {
                            var dist = absolutePosition.minus(gizmoCamera.position).length();
                            return Math.max(0.1, 0.125 * dist);
                        }

                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.leftMargin: 12
                    anchors.topMargin: 10
                    radius: theme.radiusS
                    color: "#0D274899"
                    border.width: 1
                    border.color: "#2B5E89"
                    width: 320
                    height: 72

                    Text {
                        anchors.fill: parent
                        anchors.margins: 10
                        color: "#D4EAFB"
                        font.pixelSize: 12
                        text: "左键选中, 右键旋转, 中键平移, 滚轮缩放"
                    }
                }

                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.rightMargin: 12
                    anchors.topMargin: 10
                    radius: theme.radiusS
                    color: "#0D274899"
                    border.width: 1
                    border.color: "#2B5E89"
                    width: 210
                    height: 72

                    Column {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 4

                        Text {
                            text: "使用 Gizmo 或调整参数"
                            color: "#CFE6FF"
                            font.pixelSize: 12
                        }
                        Text {
                            text: "单位: 米"
                            color: "#9EC4EA"
                            font.pixelSize: 11
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                    preventStealing: true

                    // ── 辅助：将选中实体的数据加载到 inspectorPanel ──
                    function loadModelToInspector(modelName) {
                        var models = EditorManager.findModelsByName(modelName);
                        if (models.length === 0) return false;

                        var model = models[0];
                        inspectorPanel.m_name = model.name;
                        inspectorPanel.m_mass = model.mass;
                        inspectorPanel.m_loadTime = model.loadTime;
                        inspectorPanel.m_restitution = model.restitution;
                        inspectorPanel.m_pos = Qt.vector3d(model.pos.x, model.pos.y, model.pos.z);
                        inspectorPanel.m_scale = Qt.vector3d(model.scale.x, model.scale.y, model.scale.z);
                        inspectorPanel.m_rotation = model.rotation;
                        inspectorPanel.m_initialVelo = model.initialVelo;
                        inspectorPanel.m_initialAnguVelo = model.initialAnguVelo;

                        inspectorPanel.m_baseColor = model.materials.baseColor;
                        inspectorPanel.m_metalness = model.materials.metalness;
                        inspectorPanel.m_roughness = model.materials.roughness;

                        inspectorColumn.syncTypeAndGeometryToUi(model);
                        operatingButton.isCreateMode = false;
                        return true;
                    }

                    onClicked: function (mouse) {
                        if (mouse.button === Qt.RightButton) {
                            // 动态填充菜单项（确保 enabled 状态实时计算）
                            clipboardContextMenu.menuItems = buildContextMenuItems();
                            // 将鼠标坐标转换到 editorRoot 坐标系（Popup 父级）
                            var pos = mapToItem(editorRoot, mouse.x, mouse.y);
                            clipboardContextMenu.x = pos.x;
                            clipboardContextMenu.y = pos.y;
                            clipboardContextMenu.open();
                        }
                    }

                    onPressed: function (mouse) {
                        editorCanvas.lastMousePoint = Qt.point(mouse.x, mouse.y);
                        // 右键交给 onClicked 处理
                        if (mouse.button === Qt.RightButton) return;
                        if (mouse.button !== Qt.LeftButton) return;

                        // 优先检测 Gizmo 交互（在独立的 gizmoView 中拾取）
                        if (editorGizmo.visible) {
                            const gizmoPick = gizmoView.pick(mouse.x, mouse.y);
                            if (gizmoPick && gizmoPick.objectHit) {
                                editorGizmo.handlePress(gizmoPick);
                                if (editorGizmo.activeElement !== TransformGizmo.UIElement.None) {
                                    editorCanvas.gizmoActive = true;
                                    return;
                                }
                            }
                        }

                        // 场景物体拾取（在 editorView 中）
                        const pickResult = editorView.pick(mouse.x, mouse.y);
                        if (!pickResult || !pickResult.objectHit) {
                            // 点击空白：取消选中 + detach Gizmo
                            editorCanvas.selectedObjectName = "";
                            editorGizmo.detach();
                            editorCanvas.gizmoActive = false;
                            return;
                        }

                        const objName = pickResult.objectHit.objectName;

                        // 点击实体模型 → 选中 + attach Gizmo
                        editorCanvas.selectedObjectName = objName;
                        editorCanvas.gizmoActive = false;
                        if (loadModelToInspector(objName)) {
                            console.log("Attaching gizmo to:", objName,
                                "pos:", pickResult.objectHit.position);
                            editorGizmo.attachTo(pickResult.objectHit);
                            console.log("Gizmo visible:", editorGizmo.visible,
                                "targetNode:", editorGizmo.targetNode ? "set" : "null");
                        }
                    }

                    onPositionChanged: function (mouse) {
                        const dx = mouse.x - editorCanvas.lastMousePoint.x;
                        const dy = mouse.y - editorCanvas.lastMousePoint.y;
                        editorCanvas.lastMousePoint = Qt.point(mouse.x, mouse.y);

                        // ── Gizmo 拖拽中 ──
                        if (editorGizmo.activeElement !== TransformGizmo.UIElement.None) {
                            editorGizmo.handleDrag(dx, dy);
                            return;
                        }

                        // ── Hover 追踪（Gizmo 可见时，在 gizmoView 中拾取） ──
                        if (editorGizmo.visible && mouse.buttons === Qt.NoButton) {
                            const gizmoPick = gizmoView.pick(mouse.x, mouse.y);
                            editorGizmo.handleHover(gizmoPick);
                        }

                        // ── 相机轨道（右键 / 中键） ──
                        if ((mouse.buttons & Qt.RightButton) !== 0) {
                            editorCanvas.orbitYaw -= dx * 0.22;
                            editorCanvas.orbitPitch = editorCanvas.clamp(editorCanvas.orbitPitch - dy * 0.2, -85, 85);
                            editorCanvas.updateCameraPose();
                        }

                        if ((mouse.buttons & Qt.MiddleButton) !== 0) {
                            editorCanvas.orbitTargetX -= dx * 1.6;
                            editorCanvas.orbitTargetY += dy * 1.6;
                            editorCanvas.updateCameraPose();
                        }
                    }

                    onReleased: function (mouse) {
                        if (mouse.button === Qt.LeftButton
                            && editorCanvas.gizmoActive
                            && editorGizmo.activeElement !== TransformGizmo.UIElement.None) {
                            // Gizmo 拖拽松手 → handleRelease → 同步坐标 → 直接写 C++ 数据模型
                            editorGizmo.handleRelease();

                            // 读取 Quick3D (y-up) 坐标，转换为数学 (z-up)
                            var q3dPos = editorGizmo.targetNode.position;
                            var q3dRot = editorGizmo.targetNode.rotation;
                            var mathPos = editorCanvas.fromRenderPosition(q3dPos);
                            var mathRot = editorCanvas.fromRenderRotation(q3dRot);

                            // 同步到 inspectorPanel（UI 显示）
                            inspectorPanel.m_pos = mathPos;
                            inspectorPanel.m_rotation = mathRot;

                            // 直接写入 C++ 数据模型 — 不触发 modelListChanged，Repeater3D 不重建
                            var models = EditorManager.findModelsByName(editorCanvas.selectedObjectName);
                            if (models.length > 0) {
                                models[0].pos = mathPos;
                                models[0].rotation = mathRot;
                                console.log("Gizmo auto-saved — direct model update,"
                                    + " delegate & gizmo preserved");
                            }

                            editorCanvas.gizmoActive = false;
                        }
                    }

                    onWheel: function (wheel) {
                        const factor = wheel.angleDelta.y > 0 ? 0.86 : 1.16;
                        editorCanvas.orbitDistance = editorCanvas.clamp(editorCanvas.orbitDistance * factor, 120, 12000);
                        editorCanvas.updateCameraPose();
                    }
                }
            }
        }

        ColumnLayout {
            Layout.preferredWidth: 280
            Layout.minimumWidth: 280
            Layout.maximumWidth: 280
            Layout.fillHeight: true
            spacing: 8

            GlassPanel {
                id: gizmoSettingsPanel
                Layout.fillWidth: true
                Layout.preferredHeight: 130
                title: "Gizmo 设置"

                GridLayout {
                    anchors.fill: parent
                    anchors.margins: 4
                    columns: 2
                    columnSpacing: 4
                    rowSpacing: 4

                    // ── 平移步长 ──
                    Label {
                        text: "平移步长"
                        color: "#BFD8F4"
                        font.pixelSize: 12
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        FloatField {
                            id: linearSpeedField
                            Layout.preferredWidth: 65
                            suffix: "m"
                            value: editorGizmo.linearSpeed
                            from: 0.001
                            to: 100.0
                            stepSize: 0.01
                            decimals: 3
                            onValueChanged: editorGizmo.linearSpeed = value
                        }
                        Repeater {
                            model: [0.1, 1.0, 5.0, 10.0]
                            delegate: EgretPushButton {
                                text: modelData
                                pixelSize: 10
                                implicitHeight: 22
                                implicitWidth: 38
                                onClicked: linearSpeedField.setValue(modelData)
                            }
                        }
                    }

                    // ── 旋转步长 ──
                    Label {
                        text: "旋转步长"
                        color: "#BFD8F4"
                        font.pixelSize: 12
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        FloatField {
                            id: angularSpeedField
                            Layout.preferredWidth: 65
                            suffix: "°"
                            value: editorGizmo.angularSpeed
                            from: 0.1
                            to: 180.0
                            stepSize: 0.5
                            decimals: 1
                            onValueChanged: editorGizmo.angularSpeed = value
                        }
                        Repeater {
                            model: [2.0, 5.0, 10.0, 20.0]
                            delegate: EgretPushButton {
                                text: modelData
                                pixelSize: 10
                                implicitHeight: 22
                                implicitWidth: 32
                                onClicked: angularSpeedField.setValue(modelData)
                            }
                        }
                    }
                }
            }

            GlassPanel {
                id: inspectorPanel
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "属性 / Inspector"

                // 几何参数由 EditorParameterManager 统一管理，不再平铺在此处

                property real m_mass: 0.0
                property real m_loadTime: 0.0
                property real m_restitution: 1.0
                property string m_name: ""

                property int m_type: 0

                property vector3d m_pos: Qt.vector3d(0, 0, 0)
                property vector3d m_scale: Qt.vector3d(1, 1, 1)
                property quaternion m_rotation: Qt.quaternion(1, 0, 0, 0)
                property vector3d m_initialVelo: Qt.vector3d(0, 0, 0)
                property vector3d m_initialAnguVelo: Qt.vector3d(0, 0, 0)

                property int m_fieldType: 0
                property vector3d m_gravityAcceleration: Qt.vector3d(0, 0, 0)
                property vector3d m_gravitationalFieldSource: Qt.vector3d(0, 0, 0)
                property bool m_gravitationalFieldFixed: true
                property vector3d m_gravitationalFieldVelo: Qt.vector3d(0, 0, 0)


                property color m_baseColor: "#8FBCE8"
                property real m_metalness: 0.2
                property real m_roughness: 0.6

                function serializeToJson() {
                    var geoParams = EditorParameterManager.collectAllValues();
                    var category = EditorParameterManager.currentCategory;

                    // 辅助：将 QVariantMap 中的 QVector3D 展平为数组
                    function flattenParams(params) {
                        var result = {};
                        for (var key in params) {
                            var val = params[key];
                            if (val && typeof val.x !== "undefined") {
                                result[key] = [val.x, val.y, val.z];
                            } else {
                                result[key] = val;
                            }
                        }
                        return result;
                    }

                    var data = {
                        entity_name: inspectorPanel.m_name,
                        entity_type: inspectorPanel.m_type,
                        category: category,
                        load_time: inspectorPanel.m_loadTime,
                        position: [inspectorPanel.m_pos.x, inspectorPanel.m_pos.y, inspectorPanel.m_pos.z]
                    };

                    if (category === 0) {
                        // ============ Shape ============
                        data.entity_mass = inspectorPanel.m_mass;
                        data.restitution = inspectorPanel.m_restitution;
                        data.scale = [inspectorPanel.m_scale.x, inspectorPanel.m_scale.y, inspectorPanel.m_scale.z];
                        data.rotation = [inspectorPanel.m_rotation.scalar, inspectorPanel.m_rotation.x, inspectorPanel.m_rotation.y, inspectorPanel.m_rotation.z];
                        data.initial_velocity = [inspectorPanel.m_initialVelo.x, inspectorPanel.m_initialVelo.y, inspectorPanel.m_initialVelo.z];
                        data.initial_angular_velocity = [inspectorPanel.m_initialAnguVelo.x, inspectorPanel.m_initialAnguVelo.y, inspectorPanel.m_initialAnguVelo.z];
                        data.material_base_color = inspectorPanel.m_baseColor.toString();
                        data.material_metalness = inspectorPanel.m_metalness;
                        data.material_roughness = inspectorPanel.m_roughness;

                        // 几何参数
                        var geoFlat = flattenParams(geoParams);
                        for (var key in geoFlat) {
                            if (key.indexOf("sub_") === 0 || key.indexOf("subsub_") === 0) continue;
                            data[key] = geoFlat[key];
                        }
                    } else if (category === 1) {
                        // ============ Field ============
                        data.field_type = inspectorPanel.m_type;

                        // 直接从 EditorParameterManager 读取所有参数（唯一真实数据源）
                        var allValues = EditorParameterManager.parameterValues;
                        var fieldParams = flattenParams(allValues);

                        // 同步到 inspectorPanel 的场属性（双向保持一致）
                        inspectorPanel.m_gravityAcceleration = allValues["gravity"] || Qt.vector3d(0, 0, 0);
                        inspectorPanel.m_gravitationalFieldSource = allValues["sourcePos"] || Qt.vector3d(0, 0, 0);
                        inspectorPanel.m_gravitationalFieldFixed = allValues["fixed"] !== 0;
                        var velo = allValues["sourceVelo"] || Qt.vector3d(0, 0, 0);
                        inspectorPanel.m_gravitationalFieldVelo = velo;

                        data.field_data = fieldParams;
                    } else if (category === 2) {
                        // ============ Constraint ============
                        data.constraint_type = inspectorPanel.m_type;

                        // 约束对象实体 ID
                        data.entity_ids = sceneEntityListPanel.selectedEntityIds;

                        // 锚点位置（单摆使用）
                        if (inspectorPanel.m_type === 2) {
                            data.anchor_pos = {
                                x: editorRoot.anchorPosX,
                                y: editorRoot.anchorPosY,
                                z: editorRoot.anchorPosZ
                            };
                        }

                        var constraintParams = flattenParams(geoParams);
                        data.constraint_data = constraintParams;
                    }

                    var jsonData = JSON.stringify(data);
                    console.log(jsonData);
                    return jsonData;
                }

                ColorDialog {
                    id: baseColorDialog
                    title: "选择基础颜色"
                    selectedColor: inspectorPanel.m_baseColor
                    onAccepted: {
                        inspectorPanel.m_baseColor = baseColorDialog.selectedColor;
                    }
                }

                ScrollView {
                    id: inspectorScrollView
                    anchors.fill: parent
                    contentWidth: inspectorScrollView.availableWidth
                    clip: true

                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    rightPadding: inspectorScrollBar.visible ? inspectorScrollBar.width : 0

                    ScrollBar.vertical: Basic.ScrollBar
                    {
                        id: inspectorScrollBar
                        width: 8
                        policy: ScrollBar.AsNeeded
                        visible: size < 1.0
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        minimumSize: 0.2
                        background: Rectangle {
                            radius: theme.radiusS
                            color: "#0F2A4C"
                            border.width: 1
                            border.color: theme.border
                            opacity: 0.6
                        }
                        contentItem: Rectangle {
                            radius: theme.radiusS
                            implicitWidth: 8
                            color: theme.accent
                            opacity: inspectorScrollBar.pressed ? 0.9 : 0.7
                        }
                    }

                    ColumnLayout {
                        id: inspectorColumn
                        width: Math.max(0, inspectorScrollView.availableWidth - inspectorScrollView.rightPadding)
                        spacing: 10

                        // 几何参数的本地快照 — 绕过 QML C++ 绑定延迟问题
                        property var geometryParams: []
                        // 从 pick 加载数据时置 true，阻止 onCurrentIndexChanged 重复触发类型切换
                        property bool isLoadingFromPick: false

                        // ============ Section 可见性逻辑 ============
                        readonly property bool isShapeCategory: EditorParameterManager.currentCategory === 0
                        readonly property bool isFieldCategory: EditorParameterManager.currentCategory === 1
                        readonly property bool isConstraintCategory: EditorParameterManager.currentCategory === 2

                        // 变换 (Transform)：仅实体
                        readonly property bool isTransformVisible: isShapeCategory

                        // 初始速度：实体 或 非固定的引力场
                        // 绑定到 parameterValues（具有 NOTIFY）以在参数值变化时重新求值
                        readonly property bool isVelocitySectionVisible: isShapeCategory
                            || (isFieldCategory && EditorParameterManager.currentTypeId === 2
                                && EditorParameterManager.parameterValues["fixed"] === 0)

                        // 角速度：仅实体
                        readonly property bool isAngularVelocityVisible: isShapeCategory

                        // 材质：仅实体
                        readonly property bool isMaterialVisible: isShapeCategory

                        Component.onCompleted: {
                            geometryParams = EditorParameterManager.currentParameters;
                        }

                        // 参数变化时自动同步 geometryParams（切换类别/类型时触发）
                        Connections {
                            target: EditorParameterManager

                            function onCurrentParametersChanged() {
                                inspectorColumn.geometryParams = EditorParameterManager.currentParameters;
                            }
                        }

                        // ── 约束长度是否锁定（连接线/杆选中2个实体时自动计算） ──
                        readonly property bool constraintLengthReadOnly: {
                            if (!isConstraintCategory) return false;
                            var tid = EditorParameterManager.currentTypeId;
                            if (tid !== 1 && tid !== 3) return false;  // 仅连接线和连接杆
                            return sceneEntityListPanel.selectedEntityIds.length === 2;
                        }

                        // 根据 model 的 typeId 更新所有 UI 状态（由 pick 调用）
                        function syncTypeAndGeometryToUi(model) {
                            isLoadingFromPick = true;

                            // 1. 更新 inspector 通用属性
                            inspectorPanel.m_type = model.type;

                            // 2. 切到正确的 ComboBox index
                            var types = EditorParameterManager.availableTypes;
                            for (var i = 0; i < types.length; i++) {
                                if (types[i].typeId === model.type) {
                                    sourceComboBox.currentIndex = i;
                                    break;
                                }
                            }

                            // 3. 设置几何参数并同步到 EditorParameterManager
                            EditorParameterManager.currentTypeId = model.type;
                            EditorParameterManager.loadFromValues({
                                "box_size": model.boxSize,
                                "circle_radius": model.radius,
                                "height": model.height,
                                "length": model.length
                            });

                            // 4. 刷新几何参数 UI
                            geometryParams = EditorParameterManager.currentParameters;

                            isLoadingFromPick = false;
                        }

                        RowLayout {
                            width: parent.width

                            EgretPushButton {
                                id: createNewEntityButton
                                text: "添加实体"
                                pixelSize: 12
                                onClicked: {
                                    operatingButton.isCreateMode = true;
                                    EditorParameterManager.currentCategory = 0;
                                }
                            }

                            EgretPushButton {
                                id: createNewFieldButton
                                text: "添加场"
                                pixelSize: 12
                                onClicked: {
                                    operatingButton.isCreateMode = true;
                                    EditorParameterManager.currentCategory = 1;
                                }
                            }

                            EgretPushButton {
                                id: createNewConstraintButton
                                text: "添加约束器"
                                pixelSize: 12
                                onClicked: {
                                    operatingButton.isCreateMode = true;
                                    EditorParameterManager.currentCategory = 2;
                                }
                            }
                        }

                        EgretSeparator {
                            Layout.fillWidth: true
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            radius: theme.radiusS
                            color: "#0F2B4B"
                            border.width: 1
                            border.color: "#2C5D88"

                            Text {
                                anchors.centerIn: parent
                                text: "基础 / Base"
                                color: "#CFE6FF"
                                font.pixelSize: 12
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 8

                            Label {
                                text: "名称"
                                color: "#BFD8F4"
                            }

                            Rectangle {
                                id: nameFieldShell
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28
                                radius: theme.radiusS
                                color: theme.bg1
                                border.width: 1
                                border.color: nameTextField.activeFocus ? theme.accent : theme.border

                                TextField {
                                    id: nameTextField
                                    // property bool isReadOnly: false
                                    readOnly: !(operatingButton.isCreateMode)
                                    anchors.fill: parent
                                    text: inspectorPanel.m_name
                                    onTextChanged: inspectorPanel.m_name = text
                                    color: "#FFFFFF"
                                    horizontalAlignment: Text.AlignLeft
                                    verticalAlignment: Text.AlignVCenter
                                    leftPadding: 10
                                    rightPadding: 10
                                    selectByMouse: true
                                    background: null
                                }
                            }

                            Label {
                                text: "类型"
                                color: "#BFD8F4"
                            }

                            ComboBox {
                                id: sourceComboBox
                                Layout.fillWidth: true
                                Layout.preferredHeight: 28

                                // 数据驱动：model 来自 EditorParameterManager
                                textRole: "displayName"
                                valueRole: "typeId"
                                model: EditorParameterManager.availableTypes

                                onCurrentIndexChanged: {
                                    if (currentIndex < 0) return;
                                    // pick 加载时跳过，避免重复触发类型切换
                                    if (inspectorColumn.isLoadingFromPick) return;

                                    // 直接从 availableTypes 列表按索引取值 — 最可靠的方式
                                    var types = EditorParameterManager.availableTypes;
                                    if (currentIndex >= types.length) return;
                                    var typeId = types[currentIndex].typeId;

                                    // console.log("EditorUnit: currentIndex =", currentIndex,
                                    //     "typeId =", typeId,
                                    //     "displayName =", types[currentIndex].displayName);

                                    EditorParameterManager.currentTypeId = typeId;
                                    inspectorPanel.m_type = typeId;
                                    // 强制同步刷新几何参数
                                    inspectorColumn.geometryParams = EditorParameterManager.currentParameters;
                                    // console.log("EditorUnit: geometryParams.length =", inspectorColumn.geometryParams.length);
                                }

                                // 下拉菜单样式
                                popup.width: parent.width * 0.82
                                popup.background: Rectangle {
                                    // y: parent.height
                                    color: theme.bg1
                                    border.width: 1
                                    border.color: theme.border
                                    radius: theme.radiusS
                                }

                                // 当前选中项的显示
                                contentItem: Text {
                                    text: sourceComboBox.displayText
                                    color: "#FFFFFF"
                                    horizontalAlignment: Text.AlignLeft
                                    verticalAlignment: Text.AlignVCenter
                                    leftPadding: 10
                                    rightPadding: 10
                                    font: sourceComboBox.font
                                    elide: Text.ElideRight
                                }

                                // 背景（与原 Rectangle 样式一致）
                                background: Rectangle {
                                    color: theme.bg1
                                    radius: theme.radiusS
                                    border.width: 1
                                    border.color: sourceComboBox.activeFocus ? theme.accent : theme.border
                                }

                                // 下拉箭头
                                indicator: Image {
                                    source: "qrc:/units/assets/units/triangle_arrow.svg"
                                    anchors.right: parent.right
                                    anchors.rightMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 16
                                    height: 16
                                    opacity: sourceComboBox.enabled ? 1 : 0.3
                                }

                                // 下拉列表项样式
                                delegate: ItemDelegate {
                                    width: sourceComboBox.width
                                    height: 28
                                    highlighted: sourceComboBox.highlightedIndex === index
                                    hoverEnabled: true

                                    contentItem: Text {
                                        text: modelData.displayName
                                        color: highlighted ? "#FFFFFF" : (hovered ? "#FFFFFF" : "#CCCCCC")
                                        horizontalAlignment: Text.AlignLeft
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: 10
                                        font.pixelSize: 12
                                    }

                                    background: Rectangle {
                                        color: highlighted ? theme.accent : (hovered ? "#1D4E87" : "transparent")
                                        radius: theme.radiusS
                                    }
                                }
                            }

                            Label {
                                text: "质量"
                                color: "#BFD8F4"
                                visible: EditorParameterManager.currentCategory === 0
                            }
                            FloatField {
                                Layout.fillWidth: true
                                from: 0
                                to: 100000
                                stepSize: 0.1
                                value: inspectorPanel.m_mass
                                onValueChanged: inspectorPanel.m_mass = value
                                visible: EditorParameterManager.currentCategory === 0
                            }

                            Label {
                                text: "加载时间"
                                color: "#BFD8F4"
                            }
                            FloatField {
                                Layout.fillWidth: true
                                from: 0
                                to: 120
                                stepSize: 0.1
                                value: inspectorPanel.m_loadTime
                                onValueChanged: inspectorPanel.m_loadTime = value
                            }

                            Label {
                                text: "弹性"
                                color: "#BFD8F4"
                                visible: EditorParameterManager.currentCategory === 0
                            }
                            FloatField {
                                Layout.fillWidth: true
                                from: 0
                                to: 2
                                stepSize: 0.05
                                value: inspectorPanel.m_restitution
                                onValueChanged: inspectorPanel.m_restitution = value
                                visible: EditorParameterManager.currentCategory === 0
                            }
                        }

                        EgretSeparator {
                            Layout.fillWidth: true
                        }

                        // ============================================================
                        //  约束对象 / Constraint Targets — 仅约束创建时可见
                        // ============================================================

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            radius: theme.radiusS
                            color: "#0F2B4B"
                            border.width: 1
                            border.color: "#2C5D88"

                            visible: inspectorColumn.isConstraintCategory && operatingButton.isCreateMode

                            Text {
                                anchors.centerIn: parent
                                text: "约束对象 / Targets"
                                color: "#CFE6FF"
                                font.pixelSize: 12
                            }
                        }

                        // ── 约束对象选择面板 ──
                        Column {
                            visible: inspectorColumn.isConstraintCategory && operatingButton.isCreateMode
                            width: parent.width
                            Layout.fillWidth: true
                            spacing: 6

                            // ── 根据约束类型自动配置选择参数 ──
                            Component.onCompleted: {
                                sceneEntityListPanel.selectionMode = true;
                            }
                            Component.onDestruction: {
                                sceneEntityListPanel.selectionMode = false;
                                sceneEntityListPanel.clearSelection();
                            }

                            // ── 监听类别/类型切换，管理选择模式 ──
                            Connections {
                                target: EditorParameterManager

                                // 类别变化：非约束时关闭选择模式
                                function onCurrentCategoryChanged() {
                                    if (EditorParameterManager.currentCategory !== 2) {
                                        sceneEntityListPanel.selectionMode = false;
                                        sceneEntityListPanel.clearSelection();
                                    } else {
                                        sceneEntityListPanel.selectionMode = true;
                                    }
                                }

                                // 类型切换：清空已选并更新选择参数
                                function onCurrentTypeIdChanged() {
                                    sceneEntityListPanel.clearSelection();
                                    var tid = EditorParameterManager.currentTypeId;
                                    // ConstraintType: ConnectingLine=1, SimplePendulum=2, ConnectingRod=3, Spring=5, SupportSurface=6
                                    if (tid === 1 || tid === 3 || tid === 5) {
                                        // 双实体约束：ConnectingLine, ConnectingRod, Spring
                                        sceneEntityListPanel.minSelection = 2;
                                        sceneEntityListPanel.maxSelection = 2;
                                    } else if (tid === 2) {
                                        // 单摆：1 实体 + 锚点
                                        sceneEntityListPanel.minSelection = 1;
                                        sceneEntityListPanel.maxSelection = 1;
                                    } else if (tid === 7) {
                                        // 空气阻力：≥1
                                        sceneEntityListPanel.minSelection = 1;
                                        sceneEntityListPanel.maxSelection = 999;
                                    } else {
                                        sceneEntityListPanel.minSelection = 0;
                                        sceneEntityListPanel.maxSelection = 0;
                                    }
                                }
                            }

                            // ── 实体选择变化：连接线/杆自动计算长度 ──
                            Connections {
                                target: sceneEntityListPanel

                                function onSelectionChanged(entityIds) {
                                    var tid = EditorParameterManager.currentTypeId;
                                    if ((tid !== 1 && tid !== 3) || entityIds.length !== 2) return;

                                    var m1 = EditorManager.getModel(entityIds[0]);
                                    var m2 = EditorManager.getModel(entityIds[1]);
                                    if (!m1 || !m2) return;

                                    var p1 = m1.pos;
                                    var p2 = m2.pos;
                                    var dx = p2.x - p1.x;
                                    var dy = p2.y - p1.y;
                                    var dz = p2.z - p1.z;
                                    var dist = Math.sqrt(dx * dx + dy * dy + dz * dz);

                                    EditorParameterManager.setParameterValue("length", dist);
                                }
                            }

                            // ── 类型提示 ──
                            Label {
                                id: selectionHint
                                text: {
                                    var tid = EditorParameterManager.currentTypeId;
                                    if (tid === 1) return "连接线需选择 2 个实体 — 长度自动计算";
                                    if (tid === 2) return "单摆需选择 1 个实体 + 设置锚点位置";
                                    if (tid === 3) return "连接杆需选择 2 个实体 — 长度自动计算";
                                    if (tid === 5) return "弹簧需选择 2 个实体（在左侧实体列表中点击选择）";
                                    if (tid === 7) return "空气阻力需选择至少 1 个实体";
                                    return "";
                                }
                                color: "#8FB8E0"
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            // ── 已选实体名称列表 ──
                            Repeater {
                                model: sceneEntityListPanel.selectedEntityIds
                                delegate: Label {
                                    text: {
                                        var m = EditorManager.getModel(modelData);
                                        return m ? ("  ✓ " + m.name) : ("  ✓ " + modelData);
                                    }
                                    color: "#4FC3F7"
                                    font.pixelSize: 11
                                }
                            }

                            // ── 锚点位置编辑器（仅单摆显示） ──
                            Column {
                                visible: EditorParameterManager.currentTypeId === 2
                                width: parent.width
                                Layout.fillWidth: true
                                spacing: 4

                                Label {
                                    text: "锚点位置"
                                    color: "#BFD8F4"
                                    font.pixelSize: 11
                                }
                                RowLayout {
                                    spacing: 6
                                    FloatField {
                                        Layout.fillWidth: true
                                        from: -100000;
                                        to: 100000; stepSize: 0.1; decimals: 3
                                        value: editorRoot.anchorPosX
                                        onValueChanged: editorRoot.anchorPosX = value
                                    }
                                    FloatField {
                                        Layout.fillWidth: true
                                        from: -100000;
                                        to: 100000; stepSize: 0.1; decimals: 3
                                        value: editorRoot.anchorPosY
                                        onValueChanged: editorRoot.anchorPosY = value
                                    }
                                    FloatField {
                                        Layout.fillWidth: true
                                        from: -100000;
                                        to: 100000; stepSize: 0.1; decimals: 3
                                        value: editorRoot.anchorPosZ
                                        onValueChanged: editorRoot.anchorPosZ = value
                                    }
                                }
                            }
                        }

                        EgretSeparator {
                            Layout.fillWidth: true
                            visible: inspectorColumn.isConstraintCategory && operatingButton.isCreateMode
                        }

                        // ============================================================
                        //  几何参数 / Geometry — 数据驱动，来自 EditorParameterManager
                        // ============================================================

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            radius: theme.radiusS
                            color: "#0F2B4B"
                            border.width: 1
                            border.color: "#2C5D88"

                            visible: EditorParameterManager.currentCategory === 0 || inspectorColumn.isConstraintCategory

                            Text {
                                anchors.centerIn: parent
                                text: "几何参数 / Geometry"
                                color: "#CFE6FF"
                                font.pixelSize: 12
                            }
                        }

                        // 主参数列表 — 只包含控件类型分发，不含业务类型 switch
                        Repeater {
                            id: geometryRepeater
                            model: inspectorColumn.geometryParams

                            delegate: Column {
                                id: paramColumn
                                property var paramData: modelData
                                width: parent.width
                                Layout.fillWidth: true

                                Label {
                                    text: modelData.displayName
                                    color: "#BFD8F4"
                                }

                                // float 类型参数编辑器
                                Loader {
                                    width: parent.width
                                    active: modelData.type === "float"
                                    sourceComponent: Component {
                                        FloatField {
                                            Layout.fillWidth: true
                                            from: modelData.min !== undefined ? modelData.min : -1000000
                                            to: modelData.max !== undefined ? modelData.max : 1000000
                                            stepSize: modelData.step !== undefined ? modelData.step : 0.1
                                            decimals: modelData.decimals !== undefined ? modelData.decimals : 3
                                            value: modelData.value
                                            readOnly: modelData.key === "length" && inspectorColumn.constraintLengthReadOnly
                                            suffix: modelData.key === "length" && inspectorColumn.constraintLengthReadOnly ? "(auto)" : ""
                                            onValueChanged: EditorParameterManager.setParameterValue(modelData.key, value)
                                        }
                                    }
                                }

                                // vector3d 类型参数编辑器
                                Loader {
                                    width: parent.width
                                    active: modelData.type === "vector3d"
                                    sourceComponent: Component {
                                        RowLayout {
                                            spacing: 6
                                            FloatField {
                                                Layout.fillWidth: true
                                                value: modelData.value ? modelData.value.x : 0
                                                stepSize: 0.1
                                                onValueChanged: {
                                                    var v = EditorParameterManager.parameterValue(modelData.key);
                                                    if (!v) v = Qt.vector3d(0, 0, 0);
                                                    EditorParameterManager.setParameterValue(modelData.key, Qt.vector3d(value, v.y, v.z));
                                                }
                                            }
                                            FloatField {
                                                Layout.fillWidth: true
                                                value: modelData.value ? modelData.value.y : 0
                                                stepSize: 0.1
                                                onValueChanged: {
                                                    var v = EditorParameterManager.parameterValue(modelData.key);
                                                    if (!v) v = Qt.vector3d(0, 0, 0);
                                                    EditorParameterManager.setParameterValue(modelData.key, Qt.vector3d(v.x, value, v.z));
                                                }
                                            }
                                            FloatField {
                                                Layout.fillWidth: true
                                                value: modelData.value ? modelData.value.z : 0
                                                stepSize: 0.1
                                                onValueChanged: {
                                                    var v = EditorParameterManager.parameterValue(modelData.key);
                                                    if (!v) v = Qt.vector3d(0, 0, 0);
                                                    EditorParameterManager.setParameterValue(modelData.key, Qt.vector3d(v.x, v.y, value));
                                                }
                                            }
                                        }
                                    }
                                }

                                // enum 类型参数编辑器
                                Loader {
                                    width: parent.width
                                    active: modelData.type === "enum"
                                    sourceComponent: Component {
                                        ComboBox {
                                            width: parent.width
                                            implicitHeight: 28
                                            model: modelData.enumOptions ? modelData.enumOptions : []
                                            currentIndex: modelData.value !== undefined ? modelData.value : 0

                                            popup.width: parent.width * 0.82
                                            popup.background: Rectangle {
                                                color: theme.bg1; border.width: 1; border.color: theme.border; radius: theme.radiusS
                                            }
                                            contentItem: Text {
                                                text: parent.displayText; color: "#FFFFFF"; horizontalAlignment: Text.AlignLeft; verticalAlignment: Text.AlignVCenter; leftPadding: 10; font.pixelSize: 12; elide: Text.ElideRight
                                            }
                                            background: Rectangle {
                                                color: theme.bg1; radius: theme.radiusS; border.width: 1; border.color: parent.activeFocus ? theme.accent : (comboHover.hovered ? "#4A7FBF" : theme.border)
                                                HoverHandler {
                                                    id: comboHover
                                                }
                                            }
                                            indicator: Image {
                                                source: "qrc:/units/assets/units/triangle_arrow.svg"
                                                anchors.right: parent.right
                                                anchors.rightMargin: 8
                                                anchors.verticalCenter: parent.verticalCenter
                                                width: 16
                                                height: 16
                                            }
                                            delegate: ItemDelegate {
                                                width: parent.width; height: 28; hoverEnabled: true
                                                contentItem: Text {
                                                    text: modelData; color: highlighted ? "#FFFFFF" : (hovered ? "#FFFFFF" : "#CCCCCC"); horizontalAlignment: Text.AlignLeft; verticalAlignment: Text.AlignVCenter; leftPadding: 10; font.pixelSize: 12
                                                }
                                                background: Rectangle {
                                                    color: highlighted ? theme.accent : (hovered ? "#1D4E87" : "transparent"); radius: theme.radiusS
                                                }
                                            }
                                            onCurrentIndexChanged: {
                                                if (currentIndex >= 0)
                                                    EditorParameterManager.setParameterValue(modelData.key, currentIndex);
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // 子类型选择 — 第 1 层（如 Constraint=AirResistance → DragModel 类型）
                        Column {
                            visible: EditorParameterManager.hasSubTypes
                            width: parent.width
                            Layout.fillWidth: true

                            Label {
                                text: "阻力模型"
                                color: "#BFD8F4"
                            }
                            ComboBox {
                                width: parent.width
                                implicitHeight: 28
                                textRole: "displayName"
                                valueRole: "typeId"
                                model: EditorParameterManager.subTypes

                                popup.width: parent.width * 0.82
                                popup.background: Rectangle {
                                    color: theme.bg1; border.width: 1; border.color: theme.border; radius: theme.radiusS
                                }
                                contentItem: Text {
                                    text: parent.displayText; color: "#FFFFFF"; horizontalAlignment: Text.AlignLeft; verticalAlignment: Text.AlignVCenter; leftPadding: 10; font.pixelSize: 12; elide: Text.ElideRight
                                }
                                background: Rectangle {
                                    color: theme.bg1; radius: theme.radiusS; border.width: 1; border.color: parent.activeFocus ? theme.accent : theme.border
                                }
                                indicator: Image {
                                    source: "qrc:/units/assets/units/triangle_arrow.svg"
                                    anchors.right: parent.right
                                    anchors.rightMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 16
                                    height: 16
                                }
                                delegate: ItemDelegate {
                                    width: parent.width; height: 28; hoverEnabled: true
                                    contentItem: Text {
                                        text: modelData.displayName; color: highlighted ? "#FFFFFF" : (hovered ? "#FFFFFF" : "#CCCCCC"); horizontalAlignment: Text.AlignLeft; verticalAlignment: Text.AlignVCenter; leftPadding: 10; font.pixelSize: 12
                                    }
                                    background: Rectangle {
                                        color: highlighted ? theme.accent : (hovered ? "#1D4E87" : "transparent"); radius: theme.radiusS
                                    }
                                }

                                onCurrentIndexChanged: {
                                    if (currentIndex >= 0 && model[currentIndex])
                                        EditorParameterManager.subTypeId = model[currentIndex].typeId;
                                }
                            }

                            // 子类型参数列表
                            Repeater {
                                model: EditorParameterManager.subParameters
                                delegate: Column {
                                    width: parent.width
                                    Layout.fillWidth: true
                                    Label {
                                        text: modelData.displayName; color: "#BFD8F4"
                                    }
                                    Loader {
                                        width: parent.width
                                        active: modelData.type === "float"
                                        sourceComponent: Component {
                                            FloatField {
                                                Layout.fillWidth: true
                                                from: modelData.min !== undefined ? modelData.min : -1000000
                                                to: modelData.max !== undefined ? modelData.max : 1000000
                                                stepSize: modelData.step !== undefined ? modelData.step : 0.1
                                                decimals: modelData.decimals !== undefined ? modelData.decimals : 3
                                                value: modelData.value
                                                onValueChanged: EditorParameterManager.setParameterValue(modelData.key, value)
                                            }
                                        }
                                    }
                                    Loader {
                                        width: parent.width
                                        active: modelData.type === "vector3d"
                                        sourceComponent: Component {
                                            RowLayout {
                                                spacing: 6
                                                FloatField {
                                                    Layout.fillWidth: true
                                                    value: modelData.value ? modelData.value.x : 0
                                                    stepSize: 0.1
                                                    onValueChanged: {
                                                        var v = EditorParameterManager.parameterValue(modelData.key);
                                                        if (!v) v = Qt.vector3d(0, 0, 0);
                                                        EditorParameterManager.setParameterValue(modelData.key, Qt.vector3d(value, v.y, v.z));
                                                    }
                                                }
                                                FloatField {
                                                    Layout.fillWidth: true
                                                    value: modelData.value ? modelData.value.y : 0
                                                    stepSize: 0.1
                                                    onValueChanged: {
                                                        var v = EditorParameterManager.parameterValue(modelData.key);
                                                        if (!v) v = Qt.vector3d(0, 0, 0);
                                                        EditorParameterManager.setParameterValue(modelData.key, Qt.vector3d(v.x, value, v.z));
                                                    }
                                                }
                                                FloatField {
                                                    Layout.fillWidth: true
                                                    value: modelData.value ? modelData.value.z : 0
                                                    stepSize: 0.1
                                                    onValueChanged: {
                                                        var v = EditorParameterManager.parameterValue(modelData.key);
                                                        if (!v) v = Qt.vector3d(0, 0, 0);
                                                        EditorParameterManager.setParameterValue(modelData.key, Qt.vector3d(v.x, v.y, value));
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    Loader {
                                        width: parent.width
                                        active: modelData.type === "enum"
                                        sourceComponent: Component {
                                            ComboBox {
                                                width: parent.width
                                                implicitHeight: 28
                                                model: modelData.enumOptions ? modelData.enumOptions : []
                                                currentIndex: modelData.value !== undefined ? modelData.value : 0
                                                popup.width: parent.width * 0.82
                                                popup.background: Rectangle {
                                                    color: theme.bg1; border.width: 1; border.color: theme.border; radius: theme.radiusS
                                                }
                                                contentItem: Text {
                                                    text: parent.displayText; color: "#FFFFFF"; horizontalAlignment: Text.AlignLeft; verticalAlignment: Text.AlignVCenter; leftPadding: 10; font.pixelSize: 12; elide: Text.ElideRight
                                                }
                                                background: Rectangle {
                                                    color: theme.bg1; radius: theme.radiusS; border.width: 1; border.color: parent.activeFocus ? theme.accent : theme.border
                                                }
                                                indicator: Image {
                                                    source: "qrc:/units/assets/units/triangle_arrow.svg"
                                                    anchors.right: parent.right
                                                    anchors.rightMargin: 8
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    width: 16
                                                    height: 16
                                                }
                                                delegate: ItemDelegate {
                                                    width: parent.width; height: 28
                                                    contentItem: Text {
                                                        text: modelData; color: highlighted ? "#FFFFFF" : "#CCCCCC"; horizontalAlignment: Text.AlignLeft; verticalAlignment: Text.AlignVCenter; leftPadding: 10; font.pixelSize: 12
                                                    }
                                                    background: Rectangle {
                                                        color: highlighted ? theme.accent : "transparent"; radius: theme.radiusS
                                                    }
                                                }
                                                onCurrentIndexChanged: {
                                                    if (currentIndex >= 0)
                                                        EditorParameterManager.setParameterValue(modelData.key, currentIndex);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // 子类型选择 — 第 2 层（如 Surface=Cylindrical → Curve 类型）
                        Column {
                            visible: EditorParameterManager.hasSubSubTypes
                            width: parent.width
                            Layout.fillWidth: true

                            Label {
                                text: "曲线类型"
                                color: "#BFD8F4"
                            }

                            ComboBox {
                                width: parent.width
                                implicitHeight: 28
                                textRole: "displayName"
                                valueRole: "typeId"
                                model: EditorParameterManager.subSubTypes

                                popup.width: parent.width * 0.82
                                popup.background: Rectangle {
                                    color: theme.bg1; border.width: 1; border.color: theme.border; radius: theme.radiusS
                                }
                                contentItem: Text {
                                    text: parent.displayText; color: "#FFFFFF"; horizontalAlignment: Text.AlignLeft; verticalAlignment: Text.AlignVCenter; leftPadding: 10; font.pixelSize: 12; elide: Text.ElideRight
                                }
                                background: Rectangle {
                                    color: theme.bg1; radius: theme.radiusS; border.width: 1; border.color: parent.activeFocus ? theme.accent : theme.border
                                }
                                indicator: Image {
                                    source: "qrc:/units/assets/units/triangle_arrow.svg"
                                    anchors.right: parent.right
                                    anchors.rightMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 16
                                    height: 16
                                }
                                delegate: ItemDelegate {
                                    width: parent.width; height: 28; hoverEnabled: true
                                    contentItem: Text {
                                        text: modelData.displayName; color: highlighted ? "#FFFFFF" : (hovered ? "#FFFFFF" : "#CCCCCC"); horizontalAlignment: Text.AlignLeft; verticalAlignment: Text.AlignVCenter; leftPadding: 10; font.pixelSize: 12
                                    }
                                    background: Rectangle {
                                        color: highlighted ? theme.accent : (hovered ? "#1D4E87" : "transparent"); radius: theme.radiusS
                                    }
                                }

                                onCurrentIndexChanged: {
                                    if (currentIndex >= 0 && model[currentIndex])
                                        EditorParameterManager.subSubTypeId = model[currentIndex].typeId;
                                }
                            }

                            // 第 2 层子类型参数列表
                            Repeater {
                                model: EditorParameterManager.subSubParameters
                                delegate: Column {
                                    width: parent.width
                                    Layout.fillWidth: true
                                    Label {
                                        text: modelData.displayName; color: "#BFD8F4"
                                    }
                                    Loader {
                                        width: parent.width
                                        active: modelData.type === "float"
                                        sourceComponent: Component {
                                            FloatField {
                                                Layout.fillWidth: true
                                                from: modelData.min !== undefined ? modelData.min : -1000000
                                                to: modelData.max !== undefined ? modelData.max : 1000000
                                                stepSize: modelData.step !== undefined ? modelData.step : 0.1
                                                decimals: modelData.decimals !== undefined ? modelData.decimals : 3
                                                value: modelData.value
                                                onValueChanged: EditorParameterManager.setParameterValue(modelData.key, value)
                                            }
                                        }
                                    }
                                    Loader {
                                        width: parent.width
                                        active: modelData.type === "vector3d"
                                        sourceComponent: Component {
                                            RowLayout {
                                                spacing: 6
                                                FloatField {
                                                    Layout.fillWidth: true
                                                    value: modelData.value ? modelData.value.x : 0
                                                    stepSize: 0.1
                                                    onValueChanged: {
                                                        var v = EditorParameterManager.parameterValue(modelData.key);
                                                        if (!v) v = Qt.vector3d(0, 0, 0);
                                                        EditorParameterManager.setParameterValue(modelData.key, Qt.vector3d(value, v.y, v.z));
                                                    }
                                                }
                                                FloatField {
                                                    Layout.fillWidth: true
                                                    value: modelData.value ? modelData.value.y : 0
                                                    stepSize: 0.1
                                                    onValueChanged: {
                                                        var v = EditorParameterManager.parameterValue(modelData.key);
                                                        if (!v) v = Qt.vector3d(0, 0, 0);
                                                        EditorParameterManager.setParameterValue(modelData.key, Qt.vector3d(v.x, value, v.z));
                                                    }
                                                }
                                                FloatField {
                                                    Layout.fillWidth: true
                                                    value: modelData.value ? modelData.value.z : 0
                                                    stepSize: 0.1
                                                    onValueChanged: {
                                                        var v = EditorParameterManager.parameterValue(modelData.key);
                                                        if (!v) v = Qt.vector3d(0, 0, 0);
                                                        EditorParameterManager.setParameterValue(modelData.key, Qt.vector3d(v.x, v.y, value));
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    Loader {
                                        width: parent.width
                                        active: modelData.type === "enum"
                                        sourceComponent: Component {
                                            ComboBox {
                                                width: parent.width
                                                implicitHeight: 28
                                                model: modelData.enumOptions ? modelData.enumOptions : []
                                                currentIndex: modelData.value !== undefined ? modelData.value : 0
                                                popup.width: parent.width * 0.82
                                                popup.background: Rectangle {
                                                    color: theme.bg1; border.width: 1; border.color: theme.border; radius: theme.radiusS
                                                }
                                                contentItem: Text {
                                                    text: parent.displayText; color: "#FFFFFF"; horizontalAlignment: Text.AlignLeft; verticalAlignment: Text.AlignVCenter; leftPadding: 10; font.pixelSize: 12; elide: Text.ElideRight
                                                }
                                                background: Rectangle {
                                                    color: theme.bg1; radius: theme.radiusS; border.width: 1; border.color: parent.activeFocus ? theme.accent : theme.border
                                                }
                                                indicator: Image {
                                                    source: "qrc:/units/assets/units/triangle_arrow.svg"
                                                    anchors.right: parent.right
                                                    anchors.rightMargin: 8
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    width: 16
                                                    height: 16
                                                }
                                                delegate: ItemDelegate {
                                                    width: parent.width; height: 28; hoverEnabled: true
                                                    contentItem: Text {
                                                        text: modelData; color: highlighted ? "#FFFFFF" : (hovered ? "#FFFFFF" : "#CCCCCC"); horizontalAlignment: Text.AlignLeft; verticalAlignment: Text.AlignVCenter; leftPadding: 10; font.pixelSize: 12
                                                    }
                                                    background: Rectangle {
                                                        color: highlighted ? theme.accent : (hovered ? "#1D4E87" : "transparent"); radius: theme.radiusS
                                                    }
                                                }
                                                onCurrentIndexChanged: {
                                                    if (currentIndex >= 0)
                                                        EditorParameterManager.setParameterValue(modelData.key, currentIndex);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        EgretSeparator {
                            Layout.fillWidth: true
                            visible: inspectorColumn.isTransformVisible
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            radius: theme.radiusS
                            color: "#0F2B4B"
                            border.width: 1
                            border.color: "#2C5D88"
                            visible: inspectorColumn.isTransformVisible

                            Text {
                                anchors.centerIn: parent
                                text: "变换 / Transform"
                                color: "#CFE6FF"
                                font.pixelSize: 12
                            }
                        }

                        GridLayout {
                            id: basicParamsGrid
                            visible: inspectorColumn.isTransformVisible
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 8

                            // 变换参数由 EditorParameterManager 的几何参数部分处理；
                            // 此 GridLayout 仅包含位置、缩放、旋转

                            Label {
                                text: "位置 (x, y, z)"
                                color: "#BFD8F4"
                                Layout.columnSpan: 2
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.columnSpan: 2
                                spacing: 6
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_pos.x
                                    stepSize: 0.1
                                    onValueChanged: inspectorPanel.m_pos = Qt.vector3d(value, inspectorPanel.m_pos.y, inspectorPanel.m_pos.z)
                                }

                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_pos.y
                                    stepSize: 0.1
                                    onValueChanged: inspectorPanel.m_pos = Qt.vector3d(inspectorPanel.m_pos.x, value, inspectorPanel.m_pos.z)
                                }

                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_pos.z
                                    stepSize: 0.1
                                    onValueChanged: inspectorPanel.m_pos = Qt.vector3d(inspectorPanel.m_pos.x, inspectorPanel.m_pos.y, value)
                                }
                            }

                            Label {
                                text: "缩放 (x, y, z)"
                                color: "#BFD8F4"
                                Layout.columnSpan: 2
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.columnSpan: 2
                                spacing: 6
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_scale.x
                                    stepSize: 0.05
                                    onValueChanged: inspectorPanel.m_scale = Qt.vector3d(value, inspectorPanel.m_scale.y, inspectorPanel.m_scale.z)
                                }
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_scale.y
                                    stepSize: 0.05
                                    onValueChanged: inspectorPanel.m_scale = Qt.vector3d(inspectorPanel.m_scale.x, value, inspectorPanel.m_scale.z)
                                }
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_scale.z
                                    stepSize: 0.05
                                    onValueChanged: inspectorPanel.m_scale = Qt.vector3d(inspectorPanel.m_scale.x, inspectorPanel.m_scale.y, value)
                                }
                            }

                            Label {
                                text: "旋转 (scalar, x, y, z)"
                                color: "#BFD8F4"
                                Layout.columnSpan: 2
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.columnSpan: 2
                                spacing: 6
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_rotation.scalar
                                    stepSize: 0.05
                                    onValueChanged: inspectorPanel.m_rotation = Qt.quaternion(value, inspectorPanel.m_rotation.x, inspectorPanel.m_rotation.y, inspectorPanel.m_rotation.z)
                                }

                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_rotation.x
                                    stepSize: 0.05
                                    onValueChanged: inspectorPanel.m_rotation = Qt.quaternion(inspectorPanel.m_rotation.scalar, value, inspectorPanel.m_rotation.y, inspectorPanel.m_rotation.z)
                                }

                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_rotation.y
                                    stepSize: 0.05
                                    onValueChanged: inspectorPanel.m_rotation = Qt.quaternion(inspectorPanel.m_rotation.scalar, inspectorPanel.m_rotation.x, value, inspectorPanel.m_rotation.z)
                                }

                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_rotation.z
                                    stepSize: 0.05
                                    onValueChanged: inspectorPanel.m_rotation = Qt.quaternion(inspectorPanel.m_rotation.scalar, inspectorPanel.m_rotation.x, inspectorPanel.m_rotation.y, value)
                                }
                            }
                        }

                        EgretSeparator {
                            Layout.fillWidth: true
                            visible: inspectorColumn.isVelocitySectionVisible
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            radius: theme.radiusS
                            color: "#0F2B4B"
                            border.width: 1
                            border.color: "#2C5D88"
                            visible: inspectorColumn.isVelocitySectionVisible

                            Text {
                                anchors.centerIn: parent
                                text: "初始速度 / Initial"
                                color: "#CFE6FF"
                                font.pixelSize: 12
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 8
                            visible: inspectorColumn.isVelocitySectionVisible

                            Label {
                                text: "速度"
                                color: "#BFD8F4"
                                Layout.columnSpan: 2
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.columnSpan: 2
                                spacing: 6
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_initialVelo.x
                                    stepSize: 0.1
                                    onValueChanged: inspectorPanel.m_initialVelo = Qt.vector3d(value, inspectorPanel.m_initialVelo.y, inspectorPanel.m_initialVelo.z)
                                }
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_initialVelo.y
                                    stepSize: 0.1
                                    onValueChanged: inspectorPanel.m_initialVelo = Qt.vector3d(inspectorPanel.m_initialVelo.x, value, inspectorPanel.m_initialVelo.z)
                                }
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_initialVelo.z
                                    stepSize: 0.1
                                    onValueChanged: inspectorPanel.m_initialVelo = Qt.vector3d(inspectorPanel.m_initialVelo.x, inspectorPanel.m_initialVelo.y, value)
                                }
                            }

                            Label {
                                text: "角速度"
                                color: "#BFD8F4"
                                Layout.columnSpan: 2
                                visible: inspectorColumn.isAngularVelocityVisible
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.columnSpan: 2
                                spacing: 6
                                visible: inspectorColumn.isAngularVelocityVisible
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_initialAnguVelo.x
                                    stepSize: 0.1
                                    onValueChanged: inspectorPanel.m_initialAnguVelo = Qt.vector3d(value, inspectorPanel.m_initialAnguVelo.y, inspectorPanel.m_initialAnguVelo.z)
                                }
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_initialAnguVelo.y
                                    stepSize: 0.1
                                    onValueChanged: inspectorPanel.m_initialAnguVelo = Qt.vector3d(inspectorPanel.m_initialAnguVelo.x, value, inspectorPanel.m_initialAnguVelo.z)
                                }
                                FloatField {
                                    Layout.fillWidth: true
                                    value: inspectorPanel.m_initialAnguVelo.z
                                    stepSize: 0.1
                                    onValueChanged: inspectorPanel.m_initialAnguVelo = Qt.vector3d(inspectorPanel.m_initialAnguVelo.x, inspectorPanel.m_initialAnguVelo.y, value)
                                }
                            }
                        }

                        EgretSeparator {
                            Layout.fillWidth: true
                            visible: inspectorColumn.isMaterialVisible
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            radius: theme.radiusS
                            color: "#0F2B4B"
                            border.width: 1
                            border.color: "#2C5D88"
                            visible: inspectorColumn.isMaterialVisible

                            Text {
                                anchors.centerIn: parent
                                text: "材质 / Material"
                                color: "#CFE6FF"
                                font.pixelSize: 12
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 10
                            rowSpacing: 8
                            visible: inspectorColumn.isMaterialVisible

                            Label {
                                text: "基础色"
                                color: "#BFD8F4"
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Rectangle {
                                    width: 28
                                    height: 18
                                    radius: theme.radiusS
                                    color: inspectorPanel.m_baseColor
                                    border.width: 1
                                    border.color: "#2C5D88"
                                }
                                Rectangle {
                                    width: 48
                                    height: 22
                                    radius: theme.radiusS
                                    color: pickerMouse.pressed ? "#214785" : (pickerMouse.containsMouse ? "#1A3A72" : "#102B4B")
                                    border.width: 1
                                    border.color: pickerMouse.containsMouse ? theme.accent : "#2C5D88"

                                    Text {
                                        anchors.centerIn: parent
                                        text: "选择"
                                        color: "#FFFFFF"
                                        font.pixelSize: 12
                                    }

                                    MouseArea {
                                        id: pickerMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: baseColorDialog.open()
                                    }
                                }
                            }

                            Label {
                                text: "金属度"
                                color: "#BFD8F4"
                            }
                            FloatField {
                                Layout.fillWidth: true
                                from: 0
                                to: 1
                                stepSize: 0.05
                                value: inspectorPanel.m_metalness
                                onValueChanged: inspectorPanel.m_metalness = value
                            }

                            Label {
                                text: "粗糙度"
                                color: "#BFD8F4"
                            }
                            FloatField {
                                Layout.fillWidth: true
                                from: 0
                                to: 1
                                stepSize: 0.05
                                value: inspectorPanel.m_roughness
                                onValueChanged: inspectorPanel.m_roughness = value
                            }
                        }

                        EgretSeparator {
                            Layout.fillWidth: true
                        }

                        EgretPushButton {
                            id: operatingButton
                            property bool isCreateMode: true
                            text: isCreateMode ? "创建" : "确定修改"
                            enabled: !isCreateMode || editorRoot.constraintSelectionValid
                            onClicked: {
                                if (isCreateMode) {
                                    if (!editorRoot.constraintSelectionValid) {
                                        console.warn("Constraint creation blocked: entity selection invalid");
                                        return;
                                    }
                                    EditorManager.addModelByJsonString(inspectorPanel.serializeToJson());
                                    console.log("Create Successfully");
                                    // 创建后仅清除已选实体，保持选择模式以便继续创建
                                    sceneEntityListPanel.clearSelection();
                                } else {
                                    // 修改前先 detach Gizmo，避免 Repeater3D 重建后持有悬空 Node 引用
                                    editorGizmo.detach();
                                    editorCanvas.gizmoActive = false;
                                    EditorManager.modifyModelByJsonString(inspectorPanel.serializeToJson());
                                    console.log("Modify Successfully — gizmo detached before model refresh");
                                }
                            }
                        }

                        Item {
                            Layout.fillHeight: true
                        }
                    }
                }
            }
        }
    }

    GlassPanel {
        Layout.fillWidth: true
        Layout.preferredHeight: 140
        title: "时间轴 / Timeline"

        RowLayout {
            anchors.fill: parent
            spacing: 10

            Repeater {
                model: 6
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: theme.radiusS
                    property bool hovered: false
                    color: hovered ? "#1A3B83" : "#112F52"
                    border.width: 1
                    border.color: "#2C5D88"

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            parent.hovered = true;
                        }
                        onExited: {
                            parent.hovered = false;
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "片段 " + (index + 1)
                        color: "#9EC4EA"
                        font.pixelSize: 12
                    }
                }
            }
        }
    }
}
