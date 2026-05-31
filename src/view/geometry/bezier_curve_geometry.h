/**
 * @file        bezier_curve_geometry.h
 * @brief       贝塞尔曲线几何体头文件
 * @details     定义 BezierCurveGeometry 类，继承 QQuick3DGeometry，
 *              生成二次贝塞尔曲线的 LineStrip 图元，用于在 QML 中渲染松弛约束的弧线。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-15
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     View
 */

#ifndef EGRET_PHYSICS_BEZIER_CURVE_GEOMETRY_H
#define EGRET_PHYSICS_BEZIER_CURVE_GEOMETRY_H

#include <QtQuick3D/qquick3dgeometry.h>
#include <QtGui/qvector3d.h>

namespace egret
{
    /**
     * @brief       贝塞尔曲线几何体类，生成可渲染的线框弧线。
     * @details     BezierCurveGeometry 继承自 QQuick3DGeometry，生成一条
     *              二次贝塞尔曲线（LineStrip 图元），顶点坐标为世界空间坐标。
     *              三个控制点 pointA、controlPoint、pointB 由 QML 动态传入，
     *              当任一控制点改变时自动重建几何体。segments 控制曲线平滑度。
     *
     *              曲线公式：P(t) = (1-t)²·A + 2(1-t)t·C + t²·B，t ∈ [0,1]
     *
     * @invariant   segments >= 2
     * @see         QQuick3DGeometry, Model, EgretLineMaterial
     */
    class BezierCurveGeometry : public QQuick3DGeometry
    {
        Q_OBJECT

        Q_PROPERTY(QVector3D pointA READ pointA WRITE setPointA NOTIFY pointAChanged)
        Q_PROPERTY(QVector3D controlPoint READ controlPoint WRITE setControlPoint NOTIFY controlPointChanged)
        Q_PROPERTY(QVector3D pointB READ pointB WRITE setPointB NOTIFY pointBChanged)
        Q_PROPERTY(int segments READ segments WRITE setSegments NOTIFY segmentsChanged)

    public:
        explicit BezierCurveGeometry(QQuick3DObject* parent = nullptr);

        ~BezierCurveGeometry() override = default;

        /** 获取起点 A。 */
        [[nodiscard]] QVector3D pointA() const { return m_pointA; }

        /** 设置起点 A 并重建几何体。 */
        void setPointA(const QVector3D& pt);

        /** 获取控制点 C。 */
        [[nodiscard]] QVector3D controlPoint() const { return m_controlPoint; }

        /** 设置控制点 C 并重建几何体。 */
        void setControlPoint(const QVector3D& pt);

        /** 获取终点 B。 */
        [[nodiscard]] QVector3D pointB() const { return m_pointB; }

        /** 设置终点 B 并重建几何体。 */
        void setPointB(const QVector3D& pt);

        /** 获取分段数。 */
        [[nodiscard]] int segments() const { return m_segments; }

        /** 设置分段数并重建几何体。 */
        void setSegments(int segs);

    signals:
        /** 起点 A 变化信号。 */
        void pointAChanged();

        /** 控制点 C 变化信号。 */
        void controlPointChanged();

        /** 终点 B 变化信号。 */
        void pointBChanged();

        /** 分段数变化信号。 */
        void segmentsChanged();

    private:
        /**
         * @brief 重建贝塞尔曲线。
         * @details 根据当前 pointA、controlPoint、pointB 和 segments 参数
         *          生成 LineStrip 顶点数据（世界空间坐标）。
         */
        void rebuild();

        /** 起点 A。 */
        QVector3D m_pointA;

        /** 控制点 C。 */
        QVector3D m_controlPoint;

        /** 终点 B。 */
        QVector3D m_pointB;

        /** 分段数（≥ 2），默认 20。 */
        int m_segments{20};
    };
} // egret

#endif //EGRET_PHYSICS_BEZIER_CURVE_GEOMETRY_H
