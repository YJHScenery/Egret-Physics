import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick3D 6.9
import "qrc:/components/components"
import "qrc:/components/components/theme"

ApplicationWindow {
    id: window

    width: 1920
    height: 1080
    minimumWidth: 1920
    minimumHeight: 1080

    visible: true
    title: "Egret Physics - Classical Mechanics Studio"

    DeepBlueTheme {
        id: theme
    }

    AboutDialog {
        id: aboutDialog
        parent: Overlay.overlay
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: theme.bg0
            }
            GradientStop {
                position: 0.5
                color: theme.bg1
            }
            GradientStop {
                position: 1.0
                color: theme.bg2
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14

        EgretMenuBar {
            Layout.fillWidth: true
            barHeight: 36
            barBackground: theme.panelStrong
            barBorderColor: theme.border
            itemHoverBackground: theme.accentSoft
            itemActiveBackground: "#1C3F6C"
            textColor: theme.textSecondary
            textHoverColor: theme.textPrimary
            menuBackground: "#10294C"
            menuBorderColor: theme.border
            menuItemHoverBackground: "#1E4474"
            menuItemPressedBackground: "#25507E"
            menuItemTextColor: theme.textSecondary
            menuItemHoverTextColor: theme.textPrimary
            menuSeparatorColor: "#2C4E74"

            menus: [
                {
                    title: "文件(&F)",
                    items: [
                        {
                            text: "新建(&N)",
                            shortcut: "Ctrl+N",
                            onTriggered: function () {
                                sceneController.reset();
                            },
                            icon: "qrc:/main_icons/assets/icons/new_scene.svg"
                        },
                        {
                            text: "打开...(&O)",
                            shortcut: "Ctrl+O",
                            onTriggered: function () {
                                console.log("Open scene");
                            },
                            icon: "qrc:/main_icons/assets/icons/open.svg"
                        },
                        {
                            text: "保存(&S)",
                            shortcut: "Ctrl+S",
                            onTriggered: function () {
                                console.log("Save");
                            },
                            icon: "qrc:/main_icons/assets/icons/save.svg"
                        },
                        {
                            text: "另存为...(&A)",
                            shortcut: "Ctrl+Shift+S",
                            onTriggered: function () {
                                console.log("Save as");
                            },
                            icon: "qrc:/main_icons/assets/icons/save_as.svg"
                        },
                        {
                            separator: true
                        },
                        {
                            text: "设置(&S)",
                            shortcut: "Ctrl+Alt+S",
                            onTriggered: function() {
                                console.log("Settings");
                            },
                            // icon: "qrc:/main_icons/assets/icons/settings.svg"
                            icon: "qrc:/main_icons/assets/icons/settings.svg",
                        },
                        {
                            separator: true
                        },
                        {
                            text: "退出(&Q)",
                            shortcut: "Alt+F4",
                            onTriggered: function () {
                                Qt.quit();
                            },
                            icon: "qrc:/main_icons/assets/icons/exit.svg"
                        }

                    ]
                },
                {
                    title: "编辑(&E)",
                    items: [
                        {
                            text: "撤销(&U)",
                            shortcut: "Ctrl+Z",
                            enabled: false,
                            icon: "qrc:/main_icons/assets/icons/undo.svg"
                        },
                        {
                            text: "恢复(&R)",
                            shortcut: "Ctrl+Y",
                            enabled: false,
                            icon: "qrc:/main_icons/assets/icons/recover.svg"
                        },
                        {
                            separator: true
                        },
                        {
                            text: "剪切(&T)",
                            shortcut: "Ctrl+X",
                            icon: "qrc:/main_icons/assets/icons/cut.svg"
                        },
                        {
                            text: "复制(C)",
                            shortcut: "Ctrl+C",
                            icon: "qrc:/main_icons/assets/icons/copy.svg"
                        },
                        {
                            text: "粘贴(P)",
                            shortcut: "Ctrl+V",
                            icon: "qrc:/main_icons/assets/icons/paste.svg"
                        }
                    ]
                },
                {
                    title: "视图(&V)",
                    items: [
                        {
                            text: "工作台(&W)",
                            shortcut: "Ctrl+W",
                            checkable: true,
                            checked: true,
                            onTriggered: function () {
                                mainRowLayout.workbenchVisible = this.checked === true
                            },

                        },
                        {
                            separator: true
                        },
                        {
                            text: "仿真器",
                            icon: "qrc:/main_icons/assets/icons/simulator.svg",
                            submenu: [
                                {
                                    text: "场景",
                                    icon: "qrc:/main_icons/assets/icons/view.svg",
                                    submenu: [
                                        {
                                            text: "网格(&G)",
                                            shortcut: "Ctrl+G",
                                            checkable: true,
                                            checked: true,
                                            onTriggered: function () {
                                                console.log("Toggle grid");

                                                coordinateSystem.gridOn(this.checked)
                                            }
                                        },
                                        {
                                            text: "坐标轴(&A)",
                                            shortcut: "Ctrl+H",
                                            checkable: true,
                                            checked: true,
                                            onTriggered: function () {
                                                console.log("Toggle axes");
                                                coordinateSystem.axisOn(this.checked)
                                            }
                                        }
                                    ]
                                },
                                {
                                    separator: true
                                },
                                {
                                    text: "数据流(&D)",
                                    shortcut: "Ctrl+Alt+D",
                                    checkable: true,
                                    checked: true,
                                    onTriggered: function () {
                                        console.log("数据流");
                                        sceneColumnLayout.dataStreamVisible = this.checked === true
                                    }
                                },
                                {
                                    text: "参数信息(&I)",
                                    shortcut: "Ctrl+Alt+I",
                                    checkable: true,
                                    checked: true,
                                    onTriggered: function () {
                                        sceneColumnLayout.infoPanelVisible = this.checked === true
                                    }
                                }
                            ]
                        },

                    ]
                },
                {
                    title: "帮助(&H)",
                    items: [
                        {
                            text: "关于(&A)",
                            shortcut: "F1",
                            onTriggered: function () {
                                aboutDialog.open();
                            },
                            icon: "qrc:/main_icons/assets/icons/about.svg"
                        },
                        {
                            text: "关于Qt(&Q)",
                            shortcut: "Shift+Alt+Q",
                            onTriggered: function () {
                                qtHelper.showAboutQt();
                            },
                            icon: "qrc:/main_icons/assets/icons/qt_logo.png"
                        }
                    ]
                }
            ]
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 68
            radius: theme.radiusL
            color: "#11325ECC"
            border.width: 1
            border.color: theme.border

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 16

                Image {
                    id: image
                    source: "qrc:/app_icon/assets/favicon/favicon_64.png"
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    fillMode: Image.PreserveAspectFit  // 按比例缩放，完整显示
                }

                Label {
                    text: "EGRET PHYSICS"
                    color: theme.textPrimary
                    font.pixelSize: 24
                    font.bold: true
                }

                Label {
                    text: "CLASSICAL MECHANICS STUDIO"
                    color: theme.textSecondary
                    font.pixelSize: 12
                    font.letterSpacing: 2
                }

                Item {
                    Layout.fillWidth: true
                }

                Rectangle {
                    id: newSceneButton
                    Layout.preferredWidth: 190
                    Layout.preferredHeight: 36
                    radius: 10

                    property bool hovered: false
                    property bool isPressed: false  // 新增：标记是否处于点击高亮状态
                    property int highlightDuration: 100

                    Timer {
                        id: colorTimer
                        interval: parent.highlightDuration
                        repeat: false
                        running: false
                        onTriggered: {
                            parent.isPressed = false;  // 清除点击高亮标记
                            // 不需要手动设置颜色，让颜色绑定属性自动处理
                        }
                    }

                    // 根据状态动态计算颜色
                    color: {
                        if (isPressed)
                            return "#2986ef";      // 点击高亮颜色
                        if (hovered)
                            return "#1E3A8A";        // 悬停颜色
                        return "#0A1A3A";                      // 默认颜色
                    }

                    border.width: 1
                    border.color: {
                        if (isPressed)
                            return "#2563EB";
                        if (hovered)
                            return "#3B82F6";
                        return "#1E3A8A";
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "+ 新建仿真场景"
                        color: "#8bb5ea"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    MouseArea {
                        id: newSceneMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: {
                            newSceneButton.hovered = true;
                        }
                        onExited: {
                            newSceneButton.hovered = false;
                        }
                        onClicked: {
                            console.log("New Scene Button Clicked");
                            newSceneButton.isPressed = true;
                            // 新代码说明：按钮直接调用 ViewModel 的重置命令，驱动 Model 层重建世界。
                            sceneController.reset();
                            colorTimer.restart();  // 使用 restart 确保每次点击都会重新计时
                        }
                    }
                }
            }
        }

        RowLayout {
            id: mainRowLayout
            Layout.fillWidth: true
            Layout.fillHeight: true
            property string currentRoute: "scene";
            property bool workbenchVisible: true
            spacing: 14

            LeftNavPanel {
                id: leftNevPanel
                Layout.preferredWidth: mainRowLayout.workbenchVisible ? 250 : 0
                Layout.minimumWidth: mainRowLayout.workbenchVisible ? 250 : 0
                Layout.maximumWidth: mainRowLayout.workbenchVisible ? 250 : 0
                Layout.fillHeight: true
                opacity: mainRowLayout.workbenchVisible ? 1 : 0
                enabled: mainRowLayout.workbenchVisible
                onNavSelected: function(route) {
                    console.log("Switch route:", route);
                    mainRowLayout.currentRoute = route;
                }
            }

            ColumnLayout {
                id: sceneColumnLayout
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: mainRowLayout.currentRoute === "scene"
                property bool dataStreamVisible: true
                property bool infoPanelVisible: true
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12

                    GlassPanel {

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        title: "场景视图 / Dynamics Canvas"

                        Item {
                            id: canvas3d
                            anchors.fill: parent
                            property real orbitYaw: 45
                            property real orbitPitch: 55
                            property real orbitDistance: 900
                            property real orbitTargetX: 0
                            property real orbitTargetY: 0
                            property real orbitTargetZ: 0
                            property real lineThickness: 0.45
                            property int gridCount: 41
                            property real gridSpacing: 50
                            property int activeDragBodyId: -1
                            property point lastMousePoint: Qt.point(0, 0)

                            function clamp(value, minValue, maxValue) {
                                return Math.max(minValue, Math.min(maxValue, value));
                            }

                            function cameraState() {
                                const cam = cameraPhysicsPosition();
                                return {
                                    positionX: cam.x,
                                    positionY: cam.y,
                                    positionZ: cam.z,
                                    targetX: orbitTargetX,
                                    targetY: orbitTargetY,
                                    targetZ: orbitTargetZ,
                                    upX: 0,
                                    upY: 0,
                                    upZ: 1,
                                    fovY: sceneCamera.fieldOfView,
                                    nearClip: sceneCamera.clipNear,
                                    farClip: sceneCamera.clipFar
                                };
                            }

                            function updateCameraPose() {
                                updateGridSpacing();
                                updateGridLineCount();
                                const cam = cameraPhysicsPosition();
                                const target = toRenderVector(orbitTargetX, orbitTargetY, orbitTargetZ);
                                sceneCamera.position = toRenderVector(cam.x, cam.y, cam.z);
                                sceneCamera.lookAt(target);
                            }

                            function updateGridSpacing() {
                                const distance = orbitDistance;
                                const newSpacing = distance < 400 ? 20 : (distance < 1200 ? 50 : 100);
                                if (Math.abs(newSpacing - gridSpacing) > 0.1) {
                                    gridSpacing = newSpacing;
                                }
                            }

                            function updateGridLineCount() {
                                const h = sceneView.height;
                                const w = sceneView.width;
                                if (h <= 0 || w <= 0) {
                                    return;
                                }
                                const fovRad = sceneCamera.fieldOfView * Math.PI / 180.0;
                                const halfHeight = orbitDistance * Math.tan(fovRad / 2.0);
                                const aspect = w / h;
                                const halfWidth = halfHeight * aspect;
                                const extent = Math.max(halfWidth, halfHeight) * 1.2;
                                const desired = Math.floor(extent / Math.max(gridSpacing, 0.05)) * 2 + 1;
                                const clamped = Math.max(41, Math.min(desired, 1201));
                                if (gridCount !== clamped) {
                                    gridCount = clamped;
                                }
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

                            function shapeSource(kind) {
                                if (kind === "standard_sphere" || kind === "standard_spherical_shell" || kind === "standard_disk" || kind === "standard_ring") {
                                    return "#Sphere";
                                }
                                if (kind === "standard_cylinder" || kind === "standard_cylinder_shell" || kind === "standard_rod") {
                                    return "#Cylinder";
                                }
                                if (kind === "standard_box") {
                                    return "#Cube";
                                }
                                return "#Cube";
                            }

                            function scaleForShape(kind, sx, sy, sz) {
                                const unit = 100.0;
                                const safeX = Math.max(1.0, sx);
                                const safeY = Math.max(1.0, sy);
                                const safeZ = Math.max(1.0, sz);
                                if (kind === "standard_sphere" || kind === "standard_spherical_shell" || kind === "standard_disk" || kind === "standard_ring") {
                                    const uniform = Math.max(safeX, safeY, safeZ) / unit;
                                    return Qt.vector3d(uniform, uniform, uniform);
                                }
                                if (kind === "standard_cylinder" || kind === "standard_cylinder_shell") {
                                    return Qt.vector3d(safeX / unit, safeY / unit, safeZ / unit);
                                }
                                if (kind === "standard_rod") {
                                    return Qt.vector3d(6 / unit, 6 / unit, safeZ / unit);
                                }
                                return Qt.vector3d(safeX / unit, safeY / unit, safeZ / unit);
                            }

                            Component.onCompleted: {
                                updateCameraPose();
                            }

                            View3D {
                                id: sceneView
                                anchors.fill: parent
                                camera: sceneCamera

                                environment: SceneEnvironment {
                                    backgroundMode: SceneEnvironment.Color
                                    clearColor: "#061425"
                                    antialiasingMode: SceneEnvironment.MSAA
                                    antialiasingQuality: SceneEnvironment.VeryHigh
                                }

                                PerspectiveCamera {
                                    id: sceneCamera
                                    fieldOfView: 45
                                    clipNear: 1
                                    clipFar: 50000
                                }

                                DirectionalLight {
                                    eulerRotation: Qt.vector3d(-35, 22, 20)
                                    brightness: 1.8
                                }

                                DirectionalLight {
                                    eulerRotation: Qt.vector3d(-120, -30, -70)
                                    brightness: 0.7
                                }

                                Node {
                                    id: worldRoot
                                    Coordinate {
                                        id: coordinateSystem
                                        orbitDistance: canvas3d.orbitDistance
                                        gridStep: canvas3d.gridSpacing
                                        maxGridLines: canvas3d.gridCount
                                        gridExtent: (canvas3d.gridCount - 1) * canvas3d.gridSpacing * 0.5
                                        camera: sceneCamera
                                        viewportWidth: sceneView.width
                                        viewportHeight: sceneView.height
                                    }

                                    Repeater3D {
                                        model: sceneController.bodyModel

                                        delegate: Model {
                                            objectName: "body-" + bodyId
                                            source: canvas3d.shapeSource(shapeKind)
                                            position: canvas3d.toRenderVector(bodyCenterX, bodyCenterY, bodyCenterZ)
                                            scale: canvas3d.scaleForShape(shapeKind, bodySizeX, bodySizeZ, bodySizeY)

                                            materials: PrincipledMaterial {
                                                baseColor: bodyColor
                                                roughness: 0.18
                                                specularAmount: 0.7
                                            }
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.leftMargin: 10
                                anchors.topMargin: 8
                                radius: 8
                                color: "#0D274899"
                                border.width: 1
                                border.color: "#2B5E89"
                                width: 300
                                height: 66

                                Text {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    color: "#D4EAFB"
                                    font.pixelSize: 12
                                    text: "坐标系: World z-up, 右手系\n左键拖拽实体(投影到 XY 平面), 右键旋转, 滚轮缩放"
                                }
                            }

                            MouseArea {
                                id: mouseLayer
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
                                preventStealing: true

                                onPressed: function (mouse) {
                                    canvas3d.lastMousePoint = Qt.point(mouse.x, mouse.y);
                                    if (mouse.button === Qt.LeftButton) {
                                        const pickResult = sceneView.pick(mouse.x, mouse.y);
                                        if (pickResult && pickResult.objectHit && pickResult.objectHit.objectName.indexOf("body-") === 0) {
                                            const idText = pickResult.objectHit.objectName.substring(5);
                                            canvas3d.activeDragBodyId = Number(idText);
                                            sceneController.beginBodyDrag(canvas3d.activeDragBodyId, mouse.x, mouse.y, width, height, canvas3d.cameraState(), "xy_plane");
                                        }
                                    }
                                }

                                onPositionChanged: function (mouse) {
                                    const dx = mouse.x - canvas3d.lastMousePoint.x;
                                    const dy = mouse.y - canvas3d.lastMousePoint.y;
                                    canvas3d.lastMousePoint = Qt.point(mouse.x, mouse.y);

                                    if ((mouse.buttons & Qt.RightButton) !== 0) {
                                        canvas3d.orbitYaw -= dx * 0.22;
                                        canvas3d.updateCameraPose();
                                    }

                                    if ((mouse.buttons & Qt.LeftButton) !== 0 && canvas3d.activeDragBodyId >= 0) {
                                        sceneController.updateBodyDrag(mouse.x, mouse.y, width, height, canvas3d.cameraState());
                                    }
                                }

                                onReleased: function (mouse) {
                                    if (mouse.button === Qt.LeftButton) {
                                        canvas3d.activeDragBodyId = -1;
                                        sceneController.endBodyDrag();
                                    }
                                }

                                onWheel: function (wheel) {
                                    const factor = wheel.angleDelta.y > 0 ? 0.88 : 1.12;
                                    canvas3d.orbitDistance = canvas3d.clamp(canvas3d.orbitDistance * factor, 120, 8000);
                                    canvas3d.updateCameraPose();
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        id: rightInfoColumn
                        Layout.preferredWidth: 320
                        Layout.minimumWidth: 320
                        Layout.maximumWidth: 320
                        Layout.fillHeight: true
                        spacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            MetricCard {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 56
                                metricName: "积分步长"
                                metricValue: Number(sceneController.fixedStepMs).toFixed(2)
                                metricUnit: "ms"
                                metricColor: theme.accent
                                nameFontSize: 10
                                valueFontSize: 18
                                unitFontSize: 10
                                dotSize: 8
                                contentPadding: 10
                                contentSpacing: 6
                            }

                            MetricCard {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 56
                                metricName: "实时帧率"
                                metricValue: Number(sceneController.fps).toFixed(1)
                                metricUnit: "FPS"
                                metricColor: theme.success
                                nameFontSize: 10
                                valueFontSize: 18
                                unitFontSize: 10
                                dotSize: 8
                                contentPadding: 10
                                contentSpacing: 6
                            }

                            MetricCard {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 56
                                metricName: "活动刚体"
                                metricValue: sceneController.entityCount
                                metricUnit: "obj"
                                metricColor: theme.warning
                                nameFontSize: 10
                                valueFontSize: 18
                                unitFontSize: 10
                                dotSize: 8
                                contentPadding: 10
                                contentSpacing: 6
                            }
                        }

                        GlassPanel {
                            id: infoPanel
                            Layout.preferredWidth: sceneColumnLayout.infoPanelVisible ? 320 : 0
                            Layout.minimumWidth: sceneColumnLayout.infoPanelVisible ? 320 : 0
                            Layout.maximumWidth: sceneColumnLayout.infoPanelVisible ? 320 : 0
                            Layout.fillHeight: true
                            visible: sceneColumnLayout.infoPanelVisible
                            opacity: sceneColumnLayout.infoPanelVisible ? 1 : 0
                            enabled: sceneColumnLayout.infoPanelVisible
                            title: "参数信息"

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 10

                                Repeater {
                                    model:
                                        [
                                        "重力场 g = " + resourceHelper.getStandardGravity() + "m/s^2",
                                        "积分器: RK4 (Adaptive)",
                                        "碰撞模型: Impulse + Friction",
                                        "约束求解: Sequential Impulse",
                                        "误差阈值: 1e-6"
                                    ]

                                    delegate: Rectangle {
                                        id: repeatItem
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 42
                                        property bool hovered: false
                                        radius: 9
                                        color: hovered ? "#122c5d" : "#12365D"
                                        border.width: 1
                                        border.color: "#2E5D89"

                                        MouseArea {
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            onEntered: {
                                                repeatItem.hovered = true;
                                            }
                                            onExited: {
                                                repeatItem.hovered = false;
                                            }
                                        }

                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            anchors.left: parent.left
                                            anchors.leftMargin: 10
                                            text: modelData
                                            color: "#BDD8F3"
                                            font.pixelSize: 13
                                        }
                                    }
                                }

                            Item {
                                Layout.fillHeight: true
                            }
                            }
                        }

                        GlassPanel {
                            id: controlPanel
                            Layout.preferredWidth: 320
                            Layout.minimumWidth: 320
                            Layout.maximumWidth: 320
                            Layout.preferredHeight: 120
                            title: "仿真控制"

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 10

                                Rectangle {
                                    id: computeBtnRect
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 46
                                    property bool hovered: false
                                    radius: 10
                                    color: hovered ? "#1a3b83" : "#1A4E83"
                                    border.width: 1
                                    border.color: "#58B8FF"

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onEntered: {
                                            computeBtnRect.hovered = true;
                                        }
                                        onExited: {
                                            computeBtnRect.hovered = false;
                                        }
                                        onClicked: {
                                            sceneController.toggleRunning()
                                        }
                                    }
                                    Text {
                                        anchors.centerIn: parent
                                        text: sceneController.running ? "暂停仿真" : "开始仿真"
                                        color: "#EAF5FF"
                                        font.pixelSize: 14
                                        font.bold: true
                                    }
                                }

                                Item {
                                    Layout.fillHeight: true
                                }
                            }
                        }
                    }
                }

                GlassPanel {
                    id: dataStreamPanel
                    Layout.fillWidth: true
                    Layout.preferredHeight: sceneColumnLayout.dataStreamVisible ? 170 : 0
                    Layout.minimumHeight: sceneColumnLayout.dataStreamVisible ? 170 : 0
                    Layout.maximumHeight: sceneColumnLayout.dataStreamVisible ? 170 : 0
                    visible: sceneColumnLayout.dataStreamVisible
                    opacity: sceneColumnLayout.dataStreamVisible ? 1 : 0
                    enabled: sceneColumnLayout.dataStreamVisible
                    title: "数据流"

                    RowLayout {
                        anchors.fill: parent
                        spacing: 10

                        Repeater {
                            model: 5
                            Rectangle {
                                id: valueChannel
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                property bool hovered: false
                                radius: 10
                                color: hovered ? "#1a3b83FF" : "#112F52"
                                border.width: 1
                                border.color: "#2C5D88"

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onEntered: {
                                        valueChannel.hovered = true;
                                    }
                                    onExited: {
                                        valueChannel.hovered = false;
                                    }
                                }
                                Text {
                                    anchors.centerIn: parent
                                    text: "数据通道 " + (index + 1)
                                    color: "#8DB3D9"
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            radius: theme.radiusS
            color: "#102B4B"
            border.width: 1
            border.color: "#325E89"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12

                Label {
                    text: "Ready | Precision Mode"
                    color: theme.textSecondary
                    font.pixelSize: 12
                }

                Item {
                    Layout.fillWidth: true
                }

                Label {
                    text: "Solver Queue: 3 tasks"
                    color: theme.textDim
                    font.pixelSize: 12
                }
            }
        }
    }
}
