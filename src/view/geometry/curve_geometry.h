/**
 * @file        curve_geometry.h
 * @brief       曲线几何体头文件，将 CurveBase 曲线离散化为 Qt Quick3D 可渲染的线条。
 * @details     定义 CurveGeometry 类，继承 QQuick3DGeometry，
 *              接收一个 CurveBase 指针，通过 sample() 接口将解析曲线
 *              转换为线条顶点数据（LineStrip 图元），供 QML 的 Model 组件渲染。
 *
 *              与 BezierCurveGeometry（接受 QML 控制点列表）和 SpringGeometry
 *              （接受匝数参数）不同，CurveGeometry 面向 C++ 端已构造好的
 *              CurveBase 派生类实例，适合渲染来自模型层的曲线轨道
 *              （如 SlidingRail 的 Curve）。
 *
 *              使用方式（C++）：
 *              @code
 *                  auto helix = std::make_unique<CurveHelix>(50.0, 10.0, 6.28);
 *                  auto* geom = new CurveGeometry();
 *                  geom->setCurve(helix.get());
 *                  // 将 geom 暴露给 QML 上下文...
 *              @endcode
 *
 *              使用方式（QML + C++ 上下文属性）：
 *              @code
 *                  Model {
 *                      source: curveGeometry  // CurveGeometry 实例
 *                      materials: EgretLineMaterial { lineColor: "#00FF88" }
 *                  }
 *              @endcode
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-07-23
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     View
 */

#ifndef EGRET_PHYSICS_CURVE_GEOMETRY_H
#define EGRET_PHYSICS_CURVE_GEOMETRY_H

#include <memory>

#include <QtQuick3D/qquick3dgeometry.h>

#include "curve_base.h"

namespace egret
{
    /**
     * @brief       曲线几何体类，将解析曲线转换为 Qt Quick3D 可渲染的线条。
     * @details     CurveGeometry 继承自 QQuick3DGeometry，接收一个 CurveBase
     *              指针并通过其 sample() 方法生成 LineStrip 顶点序列数据。
     *              支持通过 Q_PROPERTY 在 QML 中调整采样精度。
     *
     *              顶点格式：
     *              - 位置（PositionSemantic, F32Type, 3 分量）
     *
     *              图元类型：LineStrip（连续折线，无索引）。
     *
     * @invariant   m_curve 为 nullptr 时不生成几何体
     * @see         QQuick3DGeometry, CurveBase, CurvePoints, SampleConfig
     */
    class CurveGeometry : public QQuick3DGeometry
    {
        Q_OBJECT

        Q_PROPERTY(int segments READ segments WRITE setSegments
                   NOTIFY segmentsChanged)

    public:
        /**
         * @brief 构造曲线几何体。
         * @param parent Qt Quick3D 父对象。
         */
        explicit CurveGeometry(QQuick3DObject* parent = nullptr);

        ~CurveGeometry() override = default;

        /**
         * @brief 设置要渲染的曲线。
         * @details 内部通过 clone() 创建曲线的副本，不持有传入指针的所有权。
         *          曲线变化后需重新调用此方法以更新几何体。
         * @param curve 指向曲线基类的共享指针。
         */
        void setCurve(const std::shared_ptr<CurveBase>& curve);

        /**
         * @brief 从原始指针设置曲线（便捷重载）。
         * @details 内部自动 clone，不持有传入指针的所有权。
         * @param curve 指向曲线基类的指针。
         */
        void setCurve(const CurveBase* curve);

        /** @brief 获取当前采样分段数。 */
        [[nodiscard]] int segments() const { return m_segments; }

        /**
         * @brief 设置采样分段数并重建几何体。
         * @param segs 分段数，最小值 2，典型值 64~512。
         */
        void setSegments(int segs);

    signals:
        /** 分段数变化信号。 */
        void segmentsChanged();

    private:
        /**
         * @brief 重建几何体数据。
         * @details 调用 m_curve->sample() 获取顶点序列，
         *          然后转换为 QQuick3DGeometry 所需的缓冲区格式。
         *          同时完成物理坐标系到渲染坐标系的转换。
         */
        void rebuild();

        /** 曲线对象的内部副本。 */
        std::unique_ptr<CurveBase> m_curve;

        /** 采样分段数（默认 256）。 */
        int m_segments{256};
    };
} // egret

#endif //EGRET_PHYSICS_CURVE_GEOMETRY_H
