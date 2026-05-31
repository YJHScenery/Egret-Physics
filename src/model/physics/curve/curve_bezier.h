/**
 * @file        curve_bezier.h
 * @brief       N 阶贝塞尔曲线头文件，定义贝塞尔空间曲线。
 * @details     定义 CurveBezier 类，继承自 CurveBase，
 *              使用 N 个控制点定义 N-1 阶贝塞尔曲线。
 *              求值使用德卡斯特里奥（De Casteljau）算法，数值稳定性好。
 *              导数同时通过德卡斯特里奥算法的中间结果计算。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-31
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    Curve (空间曲线)
 */

#ifndef EGRET_PHYSICS_CURVE_BEZIER_H
#define EGRET_PHYSICS_CURVE_BEZIER_H

#include "curve_base.h"
#include <vector>

namespace egret
{
    /**
     * @brief       N 阶贝塞尔曲线类。
     * @details     CurveBezier 继承自 CurveBase，使用 N+1 个控制点定义
     *              N 阶贝塞尔曲线。曲线方程为：
     *
     *              C(t) = Σᵢ₌₀ⁿ Bᵢₙ(t) Pᵢ
     *
     *              其中 Bᵢₙ 为 N 次 Bernstein 基函数，Pᵢ 为控制点，
     *              t ∈ [0, 1]。
     *
     *              求值使用德卡斯特里奥算法，将高阶求值分解为一系列
     *              线性插值步骤，数值稳定性优于直接计算 Bernstein 多项式。
     *
     *              曲线导数通过德卡斯特里奥算法的中间结果直接计算：
     *              C'(t) = n * (Pⁿ⁻¹₁ - Pⁿ⁻¹₀)
     *
     * @invariant   m_controlPoints.size() >= 2，至少需要 2 个控制点（一阶/线性）
     * @see         CurveBase, CurveEllipticalArc, SurfaceCylindrical
     */
    class CurveBezier : public CurveBase
    {
    public:
        /**
         * @brief 构造 N 阶贝塞尔曲线。
         * @param controlPoints 控制点列表，长度为 N+1。
         *        至少需要 2 个控制点（一阶线性 Bezier）。
         *        点的顺序定义了曲线的走向。
         * @throws std::invalid_argument 如果 controlPoints 少于 2 个。
         */
        explicit CurveBezier(std::vector<Eigen::Vector3d> controlPoints);

        [[nodiscard]] std::unique_ptr<CurveBase> clone() const override;

        /** @brief 获取曲线类型，始终返回 CurveType::Bezier。 */
        [[nodiscard]] CurveType getType() const override;

        /**
         * @brief 计算贝塞尔曲线上参数 t 处的点。
         * @details 使用德卡斯特里奥算法求值。
         * @param t 曲线参数，t ∈ [0, 1]。
         * @return 曲线上对应点的三维坐标。
         */
        [[nodiscard]] Eigen::Vector3d evaluate(double t) const override;

        /**
         * @brief 计算贝塞尔曲线上参数 t 处的切向量。
         * @details 通过德卡斯特里奥算法的中间结果计算：
         *          C'(t) = n * (Pⁿ⁻¹₁ - Pⁿ⁻¹₀)
         *          其中 n 为曲线阶数。
         * @param t 曲线参数，t ∈ [0, 1]。
         * @return 切向量（未归一化）。
         */
        [[nodiscard]] Eigen::Vector3d derivative(double t) const override;

        /** @brief 获取参数范围下限，始终返回 0.0。 */
        [[nodiscard]] double getStartT() const override { return 0.0; }

        /** @brief 获取参数范围上限，始终返回 1.0。 */
        [[nodiscard]] double getEndT() const override { return 1.0; }

        /** @brief 获取控制点列表。 */
        [[nodiscard]] const std::vector<Eigen::Vector3d>& getControlPoints() const
        {
            return m_controlPoints;
        }

        /** @brief 获取曲线阶数。对于 N+1 个控制点，阶数为 N。 */
        [[nodiscard]] int getOrder() const
        {
            return static_cast<int>(m_controlPoints.size()) - 1;
        }

    private:
        /**
         * @brief 德卡斯特里奥算法求值。
         * @details 同时对参数 t 求曲线点和切向量。
         * @param t 曲线参数，t ∈ [0, 1]。
         * @return 一对值 (curvePoint, tangentVector)。
         */
        [[nodiscard]] std::pair<Eigen::Vector3d, Eigen::Vector3d>
        deCasteljau(double t) const;

        /** 控制点列表，大小为 N+1。 */
        std::vector<Eigen::Vector3d> m_controlPoints;
    };
} // egret

#endif // EGRET_PHYSICS_CURVE_BEZIER_H
