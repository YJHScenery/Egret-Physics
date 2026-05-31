/**
 * @file        curve_helix.h
 * @brief       圆柱螺旋线头文件，定义圆柱螺旋空间曲线。
 * @details     定义 CurveHelix 类，继承自 CurveBase，
 *              表示沿圆柱面旋转上升的螺旋线。
 *              曲线方程：
 *              C(t) = (R·cos(t), R·sin(t), c·t)
 *              其中 R 为旋转半径，c 为轴向速度系数。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-07-23
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    Curve (空间曲线)
 */

#ifndef EGRET_PHYSICS_CURVE_HELIX_H
#define EGRET_PHYSICS_CURVE_HELIX_H

#include "curve_base.h"
#include <numbers>

namespace egret
{
    /**
     * @brief       圆柱螺旋线类。
     * @details     CurveHelix 继承自 CurveBase，表示三维空间中沿圆柱面
     *              旋转并匀速上升的螺旋线。曲线方程为：
     *
     *              C(t) = (R·cos(t), R·sin(t), c·t)
     *
     *              其中 R 为圆锥半径，c 为轴向速度系数。
     *              t 从 0 到 m_endT，2π 为一个完整螺圈。
     *
     *              切线方向：
     *              C'(t) = (-R·sin(t), R·cos(t), c)
     *
     * @invariant   R > 0
     * @see         CurveBase, CurveBezier, CurveConicSection
     */
    class CurveHelix : public CurveBase
    {
    public:
        /**
         * @brief 构造圆柱螺旋线。
         * @param radius     旋转半径 R（必须 > 0）。
         * @param axialSpeed 轴向速度系数 c。每弧度 z 变化 c 单位。
         * @param endT       参数 t 的上限（默认 2π，即一圈）。
         *                   起点始终为 0，终点为 endT。
         * @throws std::invalid_argument 如果 radius <= 0。
         */
        explicit CurveHelix(
            double radius,
            double axialSpeed,
            double endT = 2.0 * std::numbers::pi);

        [[nodiscard]] std::unique_ptr<CurveBase> clone() const override;

        /** @brief 获取曲线类型，始终返回 CurveType::Helix。 */
        [[nodiscard]] CurveType getType() const override;

        /**
         * @brief 计算螺旋线上参数 t 处的点。
         * @param t 曲线参数，t ∈ [0, endT]。
         * @return 三维空间中的点坐标。
         */
        [[nodiscard]] Eigen::Vector3d evaluate(double t) const override;

        /**
         * @brief 计算螺旋线上参数 t 处的切向量。
         * @param t 曲线参数，t ∈ [0, endT]。
         * @return 切向量（未归一化）。
         */
        [[nodiscard]] Eigen::Vector3d derivative(double t) const override;

        /** @brief 获取参数范围下限，始终返回 0.0。 */
        [[nodiscard]] double getStartT() const override { return 0.0; }

        /** @brief 获取参数范围上限。 */
        [[nodiscard]] double getEndT() const override { return m_endT; }

        /** @brief 获取旋转半径。 */
        [[nodiscard]] double getRadius() const { return m_radius; }

        /** @brief 获取轴向速度系数。 */
        [[nodiscard]] double getAxialSpeed() const { return m_axialSpeed; }

    private:
        /** 旋转半径。 */
        double m_radius;

        /** 轴向速度系数（每弧度 z 增量）。 */
        double m_axialSpeed;

        /** 参数 t 上限。 */
        double m_endT;
    };
} // egret

#endif // EGRET_PHYSICS_CURVE_HELIX_H
