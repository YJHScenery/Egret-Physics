/**
 * @file        SimulationViewport.qml
 * @brief       共享 3D 仿真视口组件
 * @details     把原 SimulatorUnit 内部的 View3D 场景（环境、相机、光照、坐标系
 *              以及刚体 / 约束 / 曲线 / 凸体网格四个 Repeater3D）抽取为独立组件，
 *              供仿真器与离线播放器复用。
 *
 *              组件只负责「渲染 + 相机姿态计算」：鼠标交互（轨道旋转、缩放、
 *              拾取、右键菜单）由使用方以子项形式叠加在本组件之上，通过本组件
 *              暴露的属性与函数驱动相机，从而让仿真器的物体拾取与播放器的
 *              只读浏览互不干扰。
 *
 * @author      jehor
 * @date        2026-10-02
 * @version     1.0.0
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 * @ingroup     View
 */

import QtQuick 2.15
import QtQuick3D 6.9

import "qrc:/components/components"
import "qrc:/components/components/viewport"

Item {
    id: viewportRoot

    // ============ 相机轨道状态 ============

    /** 轨道偏航角（度）。 */
    property real orbitYaw: 45

    /** 轨道俯仰角（度），有效范围 -90 ~ 90。 */
    property real orbitPitch: 55

    /** 轨道半径（世界单位）。 */
    property real orbitDistance: 900

    /** 轨道注视点 x。 */
    property real orbitTargetX: 0

    /** 轨道注视点 y。 */
    property real orbitTargetY: 0

    /** 轨道注视点 z。 */
    property real orbitTargetZ: 0

    /** 当前高亮的实体 ID，空串表示无选中。 */
    property string selectedBodyId: ""

    // ============ 数据来源 ============

    /**
     * @brief 视口渲染的数据来源。
     * @details 默认绑定实时仿真器（上下文属性 sceneController）。离线仿真工作区会注入
     *          自己的场景控制器，从而在不影响实时仿真的前提下预览「待求解场景」，
     *          且未加载场景时只显示坐标系与网格。
     */
    property var bodyModelSource: sceneController.bodyModel

    /** 约束模型来源，默认实时仿真器。 */
    property var constraintModelSource: sceneController.constraintModel

    /** 曲线模型来源，默认实时仿真器。 */
    property var curveModelSource: sceneController.curveModel

    /** 凸体网格模型来源，默认实时仿真器。 */
    property var convexMeshModelSource: sceneController.convexMeshModel

    /** 网格线数量。 */
    property int gridCount: 41

    /** 网格线间距（世界单位）。 */
    property real gridSpacing: 50

    /** 上一次鼠标位置（供外部交互代码记录拖拽增量）。 */
    property point lastMousePoint: Qt.point(0, 0)

    /** 鼠标按下位置（供外部交互代码区分点击与拖拽）。 */
    property point pressPoint: Qt.point(0, 0)

    // ============ 对外暴露的子对象 ============

    /** 坐标系辅助显示，供使用方切换网格 / 坐标轴显示。 */
    property alias coordinateSystem: coordinateSystem

    // ============ 相机计算 ============

    /**
     * @brief  把数值限制在 [minValue, maxValue] 区间内。
     * @param  value 待限制的数值。
     * @param  minValue 下界。
     * @param  maxValue 上界。
     * @return 限制后的数值。
     */
    function clamp(value, minValue, maxValue) {
        return Math.max(minValue, Math.min(maxValue, value));
    }

    /**
     * @brief  导出当前相机状态（位置、注视点、朝向、视锥参数）。
     * @return 相机状态字典。
     */
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

    /**
     * @brief  把物理坐标系 (x, y, z) 转换为渲染坐标系 (y, z, x)。
     * @param  x 物理坐标 x。
     * @param  y 物理坐标 y。
     * @param  z 物理坐标 z。
     * @return 渲染坐标系下的向量。
     */
    function toRenderVector(x, y, z) {
        return Qt.vector3d(y, z, x)
    }

    /** @brief 按当前轨道参数刷新相机位置与注视点。 */
    function updateCameraPose() {
        updateGridSpacing();
        updateGridLineCount();
        const cam = cameraPhysicsPosition();
        const target = toRenderVector(orbitTargetX, orbitTargetY, orbitTargetZ);
        sceneCamera.position = toRenderVector(cam.x, cam.y, cam.z);
        sceneCamera.lookAt(target);
    }

    /** @brief 随相机距离自适应网格间距。 */
    function updateGridSpacing() {
        const distance = orbitDistance;
        const newSpacing = distance < 400 ? 20 : (distance < 1200 ? 50 : 100);
        if (Math.abs(newSpacing - gridSpacing) > 0.1) {
            gridSpacing = newSpacing;
        }
    }

    /** @brief 随视锥覆盖范围自适应网格线数量。 */
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

    /**
     * @brief  由轨道参数计算相机在物理坐标系下的位置。
     * @return 相机位置向量（物理坐标系）。
     */
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

    /**
     * @brief  在视口内做一次拾取。
     * @param  x 视口内相对 x 坐标（像素）。
     * @param  y 视口内相对 y 坐标（像素）。
     * @return 拾取结果，未命中时 objectHit 为空。
     */
    function pickAt(x, y) {
        return sceneView.pick(x, y);
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
            castsShadow: true
            shadowMapQuality: Light.ShadowMapQualityHigh
            softShadowQuality: Light.PCF32
        }

        DirectionalLight {
            eulerRotation: Qt.vector3d(-120, -30, -70)
            brightness: 0.7
        }

        Node {
            id: worldRoot

            Coordinate {
                id: coordinateSystem
                orbitDistance: viewportRoot.orbitDistance
                gridStep: viewportRoot.gridSpacing
                maxGridLines: viewportRoot.gridCount
                gridExtent: (viewportRoot.gridCount - 1) * viewportRoot.gridSpacing * 0.5
                camera: sceneCamera
                viewportWidth: sceneView.width
                viewportHeight: sceneView.height
            }

            // ---- 刚体渲染 ----
            Repeater3D {
                model: viewportRoot.bodyModelSource

                delegate: BodyDelegate {
                    selectedBodyId: viewportRoot.selectedBodyId
                }
            }

            // ---- 约束器渲染 ----
            // 坐标转换已在 C++ buildConstraintRenderItems() 中完成
            // (x,y,z) → (y,z,x)，与 buildRenderItems() 保持一致
            Repeater3D {
                model: viewportRoot.constraintModelSource

                delegate: ConstraintDelegate {}
            }

            // ---- 曲线轨道渲染（SlidingRail） ----
            // 坐标转换已在 CurveGeometry::rebuild() 中完成
            Repeater3D {
                model: viewportRoot.curveModelSource

                delegate: CurveDelegate {}
            }

            // ---- 凸体网格渲染（ConvexMesh） ----
            // 坐标转换已在 ConvexMeshGeometry::rebuild() 中完成
            Repeater3D {
                model: viewportRoot.convexMeshModelSource

                delegate: ConvexMeshDelegate {}
            }
        }
    }
}
