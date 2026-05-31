/**
 * @file        bezier_curve_geometry.h
 * @brief       贝塞尔曲线几何体头文件
 * @details     定义 BezierCurveGeometry 类，继承 QQuick3DGeometry，
 *              生成任意阶贝塞尔曲线的 LineStrip 图元，用于在 QML 中渲染松弛约束的弧线。
 *              控制点由 QML 以列表形式传入，内部持有 CurveBezier 实例，
 *              使用德卡斯特里奥算法求值。
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

#include <QVariantList>
#include <memory>

#include "curve_bezier.h"

namespace egret
{
    /**
     * @brief       贝塞尔曲线几何体类，生成可渲染的线框弧线。
     * @details     BezierCurveGeometry 继承自 QQuick3DGeometry，生成一条
     *              贝塞尔曲线（LineStrip 图元），顶点坐标为世界空间坐标。
     *              控制点列表由 QML 动态传入（QVector3D 列表），
     *              内部持有 CurveBezier 实例，使用德卡斯特里奥算法求值，
     *              支持 2 个点（线性）到任意数量的控制点。
     *
     * @invariant   m_curve 非空，包含至少 2 个控制点
     * @invariant   segments >= 2
     * @see         QQuick3DGeometry, Model, EgretLineMaterial
     */
    class BezierCurveGeometry : public QQuick3DGeometry
    {
        Q_OBJECT

        Q_PROPERTY(QVariantList controlPoints READ controlPoints
                   WRITE setControlPoints NOTIFY controlPointsChanged)
        Q_PROPERTY(int segments READ segments WRITE setSegments NOTIFY segmentsChanged)

    public:
        explicit BezierCurveGeometry(QQuick3DObject* parent = nullptr);

        ~BezierCurveGeometry() override = default;

        /** 获取控制点列表（QVector3D 列表，供 QML 读取）。 */
        [[nodiscard]] QVariantList controlPoints() const;

        /**
         * @brief 设置控制点列表并重建几何体。
         * @details 每个元素应为 QVector3D，至少 2 个点。
         *          内部构造为 CurveBezier 实例。
         *          元素数量不足 2 时不更新。
         */
        void setControlPoints(const QVariantList& pts);

        /** 获取分段数。 */
        [[nodiscard]] int segments() const { return m_segments; }

        /** 设置分段数（≥ 2）并重建几何体。 */
        void setSegments(int segs);

    signals:
        /** 控制点列表变化信号。 */
        void controlPointsChanged();

        /** 分段数变化信号。 */
        void segmentsChanged();

    private:
        /**
         * @brief 重建贝塞尔曲线。
         * @details 根据 m_curve 和 segments 参数，
         *          使用德卡斯特里奥算法生成 LineStrip 顶点数据。
         */
        void rebuild();

        /** CurveBezier 实例（唯一所有权）。 */
        std::unique_ptr<CurveBezier> m_curve;

        /** 分段数（≥ 2），默认 20。 */
        int m_segments{20};
    };
} // egret

#endif //EGRET_PHYSICS_BEZIER_CURVE_GEOMETRY_H
