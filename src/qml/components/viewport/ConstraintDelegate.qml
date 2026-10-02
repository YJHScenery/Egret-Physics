/**
 * @file        ConstraintDelegate.qml
 * @brief       视口约束渲染委托
 * @details     对应 SceneConstraintModel 的单个约束条目。内部按约束类型分支：
 *              非弹簧约束渲染为细长盒体线段，弹簧渲染为螺旋线，松弛时叠加
 *              贝塞尔弧线。两端坐标已由 C++ 转换到渲染坐标系。
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

import EgretPhysics 1.0

import "qrc:/materials/materials"
import "ViewportMath.js" as ViewportMath

Node {
    id: constraintNode

    /** 约束两端位置（C++ 中已转换为渲染坐标系）。 */
    property var renderPosA: Qt.vector3d(
        constraintPositionA[0],
        constraintPositionA[1],
        constraintPositionA[2])
    property var renderPosB: Qt.vector3d(
        constraintPositionB[0],
        constraintPositionB[1],
        constraintPositionB[2])

    /** 方向向量与长度。 */
    property var direction: renderPosB.minus(renderPosA)
    property var entityDist: direction.length()

    /** 松弛系数：0 = 完全松弛，1 = 完全绷紧。 */
    property var slackRatio: constraintConstraintLength > 0
        ? Math.min(constraintCurrentDistance / constraintConstraintLength, 1.0)
        : 1.0

    /** 只对连接两个实体的约束可见。 */
    visible: constraintKind > 0 && entityDist > 0.001

    /**
     * @brief  计算从默认 Z 轴指向目标方向的旋转四元数。
     * @param  dir 目标方向（vector3d）。
     * @return 旋转四元数。
     */
    function rotationFromZTo(dir) {
        const components = ViewportMath.rotationFromZTo(dir.x, dir.y, dir.z);
        return Qt.quaternion(components[0], components[1], components[2], components[3]);
    }

    /**
     * @brief  计算二维平面上与方向垂直的单位向量（用于松弛曲线偏移）。
     * @param  dir 目标方向（vector3d）。
     * @return 垂直单位向量。
     */
    function perpendicular2D(dir) {
        const components = ViewportMath.perpendicular2D(dir.x, dir.y, dir.z);
        return Qt.vector3d(components[0], components[1], components[2]);
    }

    // ---- 主体直线（非弹簧类型） ----
    // 使用薄盒体渲染为线段，长度为两端实体间的实际距离
    Model {
        visible: (constraintKind === 1 || constraintKind === 2 || constraintKind === 3)
            && constraintNode.slackRatio >= 0.9

        x: constraintNode.renderPosA.x + constraintNode.direction.x * 0.5
        y: constraintNode.renderPosA.y + constraintNode.direction.y * 0.5
        z: constraintNode.renderPosA.z + constraintNode.direction.z * 0.5

        source: "#Cube"
        scale: Qt.vector3d(0.01, 0.01, 0.01 * constraintNode.entityDist)
        rotation: constraintNode.rotationFromZTo(constraintNode.direction)

        materials: PrincipledMaterial {
            baseColor: constraintColor
            roughness: 0.35
            specularAmount: 0.5
        }
    }

    // ---- 弹簧螺旋线（仅 Spring 类型） ----
    // 使用 SpringGeometry 生成等距等宽螺旋线（LineStrip 图元），
    // 几何体沿 Z 轴从 0 → 1，因此定位到 A，scale.z 拉伸后自然覆盖到 B
    Model {
        visible: constraintKind === 5

        x: constraintNode.renderPosA.x
        y: constraintNode.renderPosA.y
        z: constraintNode.renderPosA.z

        geometry: SpringGeometry {
            coilCount: 32
            segmentsPerTurn: 32
        }

        // 螺旋半径固定
        property real springCoilScale: 8
        scale: Qt.vector3d(springCoilScale, springCoilScale, constraintNode.entityDist)
        rotation: constraintNode.rotationFromZTo(constraintNode.direction)

        materials: EgretLineMaterial {
            lineColor: constraintColor
        }
    }

    // ---- 松弛曲线指示器（非弹簧类型） ----
    // 当约束松弛时（slackRatio < 0.9），沿贝塞尔曲线渲染弧线，
    // 曲线控制点偏移量与松弛程度成正比
    property real curveOffset: slackRatio < 0.9
        ? (1.0 - slackRatio) * constraintConstraintLength * 1.5
        : 0.0

    // 曲线分段数，随距离增加而增多以保证平滑
    property int curveSegments: slackRatio < 0.9
        ? Math.max(32, Math.min(32, Math.ceil(entityDist * 0.03)))
        : 0

    // 控制点 = 中点 + 垂直偏移
    property var midPt: renderPosA.plus(direction.times(0.5))
    property var perpDir: constraintNode.perpendicular2D(direction)
    property var controlPt: midPt.plus(perpDir.times(curveOffset))

    // 贝塞尔曲线（LineStrip 图元）
    Model {
        visible: (constraintKind === 1 || constraintKind === 2) && constraintNode.slackRatio < 0.9

        geometry: BezierCurveGeometry {
            controlPoints: [
                constraintNode.renderPosA,
                constraintNode.controlPt,
                constraintNode.renderPosB
            ]
            segments: constraintNode.curveSegments
        }

        materials: EgretLineMaterial {
            lineColor: constraintColor
        }
    }
}
