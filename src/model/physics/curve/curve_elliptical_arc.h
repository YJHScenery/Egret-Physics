/**
 * @file        curve_elliptical_arc.h
 * @brief       椭圆弧曲线头文件，定义椭圆弧空间曲线。
 * @details     定义 CurveEllipticalArc 类，继承自 CurveBase，
 *              使用中心、半轴向量和角度范围定义椭圆弧。
 *              椭圆弧可以表示圆形（半长轴=半短轴）、完整椭圆或部分弧段。
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

#ifndef EGRET_PHYSICS_CURVE_ELLIPTICAL_ARC_H
#define EGRET_PHYSICS_CURVE_ELLIPTICAL_ARC_H

#include "curve_base.h"

namespace egret
{
    /**
     * @brief       椭圆弧曲线类。
     * @details     CurveEllipticalArc 继承自 CurveBase，表示三维空间中的
     *              椭圆弧段。曲线方程为：
     *
     *              C(θ) = center + cos(θ) * u + sin(θ) * v
     *
     *              其中 u 为半长轴向量，v 为半短轴向量（两者正交），
     *              θ 从 startAngle 到 startAngle + sweepAngle。
     *              参数 t ∈ [0, 1] 映射到 θ = startAngle + t * sweepAngle。
     *
     *              当 u 和 v 长度相等时表示圆弧。
     *              当 sweepAngle = 2π 时表示完整椭圆。
     *
     * @invariant   u 和 v 向量正交（u · v = 0）
     * @see         CurveBase, CurveBezier, SurfaceCylindrical
     */
    class CurveEllipticalArc : public CurveBase
    {
    public:
        /**
         * @brief 构造椭圆弧。
         * @param center       椭圆中心点。
         * @param semiMajorAxis 半长轴向量（方向 = 长轴方向，长度 = 半长轴长度 a）。
         * @param semiMinorAxis 半短轴向量（方向 = 短轴方向，长度 = 半短轴长度 b）。
         *                      应与半长轴正交。
         * @param startAngle    起始角度，单位为弧度。从半长轴方向开始测量。
         * @param sweepAngle    扫略角度，单位为弧度。正值表示逆时针方向。
         *                      可以是任意值（用于多圈弧），但通常 (-2π, 2π)。
         */
        explicit CurveEllipticalArc(
            const Eigen::Vector3d& center,
            const Eigen::Vector3d& semiMajorAxis,
            const Eigen::Vector3d& semiMinorAxis,
            double startAngle,
            double sweepAngle);

        [[nodiscard]] std::unique_ptr<CurveBase> clone() const override;

        /** @brief 获取曲线类型，始终返回 CurveType::EllipticalArc。 */
        [[nodiscard]] CurveType getType() const override;

        /**
         * @brief 计算椭圆弧上参数 t 处的点。
         * @param t 曲线参数，t ∈ [0, 1]。
         * @return 椭圆弧上对应点的三维坐标。
         */
        [[nodiscard]] Eigen::Vector3d evaluate(double t) const override;

        /**
         * @brief 计算椭圆弧上参数 t 处的切向量。
         * @param t 曲线参数，t ∈ [0, 1]。
         * @return 切向量（未归一化）。
         */
        [[nodiscard]] Eigen::Vector3d derivative(double t) const override;

        /** @brief 获取参数范围下限，始终返回 0.0。 */
        [[nodiscard]] double getStartT() const override { return 0.0; }

        /** @brief 获取参数范围上限，始终返回 1.0。 */
        [[nodiscard]] double getEndT() const override { return 1.0; }

        /** @brief 获取椭圆中心。 */
        [[nodiscard]] const Eigen::Vector3d& getCenter() const { return m_center; }

        /** @brief 获取半长轴向量。 */
        [[nodiscard]] const Eigen::Vector3d& getSemiMajorAxis() const
        {
            return m_semiMajorAxis;
        }

        /** @brief 获取半短轴向量。 */
        [[nodiscard]] const Eigen::Vector3d& getSemiMinorAxis() const
        {
            return m_semiMinorAxis;
        }

        /** @brief 获取起始角度（弧度）。 */
        [[nodiscard]] double getStartAngle() const { return m_startAngle; }

        /** @brief 获取扫略角度（弧度）。 */
        [[nodiscard]] double getSweepAngle() const { return m_sweepAngle; }

    private:
        /** 椭圆中心。 */
        Eigen::Vector3d m_center;

        /** 半长轴向量。 */
        Eigen::Vector3d m_semiMajorAxis;

        /** 半短轴向量。 */
        Eigen::Vector3d m_semiMinorAxis;

        /** 起始角度（弧度）。 */
        double m_startAngle;

        /** 扫略角度（弧度）。 */
        double m_sweepAngle;
    };
} // egret

#endif // EGRET_PHYSICS_CURVE_ELLIPTICAL_ARC_H
