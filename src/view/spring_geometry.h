/**
 * @file        spring_geometry.h
 * @brief       弹簧螺旋线几何体头文件
 * @details     定义 SpringGeometry 类，继承 QQuick3DGeometry，
 *              生成等距等宽螺旋线，用于在 QML 中渲染弹簧约束器。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-15
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     View
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#ifndef EGRET_PHYSICS_SPRING_GEOMETRY_H
#define EGRET_PHYSICS_SPRING_GEOMETRY_H

#include <QtQuick3D/qquick3dgeometry.h>

namespace egret
{
    /**
     * @brief       弹簧螺旋线几何体类，生成可渲染的线框螺旋线。
     * @details     SpringGeometry 继承自 QQuick3DGeometry，生成一个沿 Z 轴
     *              从 0 到 1 的归一化螺旋线（LineStrip 图元），线圈半径恒为 1。
     *              在 QML 中可通过 scale 属性拉伸/压缩以匹配弹簧两端距离。
     *              参数：
     *              - coilCount：匝数（默认 10）
     *              - segmentsPerTurn：每匝分段数（默认 16，控制平滑度）
     *
     * @invariant   coilCount >= 1
     * @invariant   segmentsPerTurn >= 2
     * @see         QQuick3DGeometry, Model
     */
    class SpringGeometry : public QQuick3DGeometry
    {
        Q_OBJECT

        Q_PROPERTY(int coilCount READ coilCount WRITE setCoilCount NOTIFY coilCountChanged)
        Q_PROPERTY(int segmentsPerTurn READ segmentsPerTurn WRITE setSegmentsPerTurn NOTIFY segmentsPerTurnChanged)

    public:
        explicit SpringGeometry(QQuick3DObject* parent = nullptr);

        ~SpringGeometry() override = default;

        /** 获取匝数。 */
        [[nodiscard]] int coilCount() const { return m_coilCount; }

        /** 设置匝数并重建几何体。 */
        void setCoilCount(int count);

        /** 获取每匝分段数。 */
        [[nodiscard]] int segmentsPerTurn() const { return m_segmentsPerTurn; }

        /** 设置每匝分段数并重建几何体。 */
        void setSegmentsPerTurn(int segments);

    signals:
        /** 匝数变化信号。 */
        void coilCountChanged();

        /** 每匝分段数变化信号。 */
        void segmentsPerTurnChanged();

    private:
        /**
         * @brief 重建螺旋线。
         * @details 根据当前参数生成顶点数据和索引数据（LineStrip 图元）。
         */
        void rebuild();

        /** 匝数。 */
        int m_coilCount{10};

        /** 每匝分段数。 */
        int m_segmentsPerTurn{16};
    };
} // egret

#endif //EGRET_PHYSICS_SPRING_GEOMETRY_H
