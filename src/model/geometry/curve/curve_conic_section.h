/**
 * @file        curve_conic_section.h
 * @brief       圆锥曲线头文件，定义椭圆/抛物线/双曲线弧段。
 * @details     定义 ConicType 枚举和 CurveConicSection 类，继承自 CurveBase，
 *              统一表示三种圆锥曲线的弧段。
 *
 *              曲线方程：
 *              - 椭圆   (Ellipse):   C(θ) = center + cosθ·u + sinθ·v
 *              - 抛物线 (Parabola):  C(t) = center + t²·u + t·v
 *              - 双曲线 (Hyperbola): C(θ) = center + coshθ·u + sinhθ·v
 *
 *              u、v 为正交向量，具体含义因类型而异（见构造参数说明）。
 *              参数 t ∈ [0, 1] 映射到 [startParam, startParam + sweepParam]。
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

#ifndef EGRET_PHYSICS_CURVE_CONIC_SECTION_H
#define EGRET_PHYSICS_CURVE_CONIC_SECTION_H

#include "curve_base.h"

namespace egret
{
    /**
     * @brief       圆锥曲线类型枚举。
     * @details     标识不同圆锥曲线类型，基于离心率 e 分类：
     *              - Ellipse:   e < 1，使用三角函数参数
     *              - Parabola:  e = 1，使用多项式参数
     *              - Hyperbola: e > 1，使用双曲函数参数
     */
    enum class ConicType : std::uint64_t
    {
        /** 椭圆（含圆）。 */
        Ellipse = 0,

        /** 抛物线。 */
        Parabola,

        /** 双曲线。 */
        Hyperbola,
    };

    /**
     * @brief       圆锥曲线弧段类。
     * @details     CurveConicSection 继承自 CurveBase，统一表示椭圆弧、
     *              抛物线弧和双曲线弧。参数 t ∈ [0, 1] 映射到实际参数。
     *
     *              构造函数参数 t 的含义因类型而异：
     *              - Ellipse:  θ = startParam + t * sweepParam（弧度）
     *              - Parabola: p = startParam + t * sweepParam（自由参数）
     *              - Hyperbola: θ = startParam + t * sweepParam（弧度）
     *
     * @invariant   u 和 v 向量正交（u · v = 0）
     * @see         CurveBase, CurveBezier, SurfaceCylindrical
     */
    class CurveConicSection : public CurveBase
    {
    public:
        /**
         * @brief 构造圆锥曲线弧段。
         * @param type       曲线类型（Ellipse / Parabola / Hyperbola）。
         * @param center     中心（椭圆/双曲线）或顶点（抛物线）。
         * @param axisU      椭圆半长轴 / 抛物线轴向 / 双曲线实半轴。
         * @param axisV      椭圆半短轴 / 抛物线垂直方向 / 双曲线虚半轴。
         *                    应与 axisU 正交。
         * @param startParam 起始参数（椭圆/双曲线：弧度；抛物线：t 值）。
         * @param sweepParam 扫略范围（椭圆/双曲线：弧度；抛物线：t 变化量）。
         *                    正值表示逆时针（椭圆/双曲线）。
         */
        explicit CurveConicSection(
            ConicType type,
            const Eigen::Vector3d& center,
            const Eigen::Vector3d& axisU,
            const Eigen::Vector3d& axisV,
            double startParam,
            double sweepParam);

        [[nodiscard]] std::unique_ptr<CurveBase> clone() const override;

        /** @brief 获取曲线类型，始终返回 CurveType::ConicSection。 */
        [[nodiscard]] CurveType getType() const override;

        /**
         * @brief 计算圆锥曲线上参数 t 处的点。
         * @param t 曲线参数，t ∈ [0, 1]。
         * @return 曲线上对应点的三维坐标。
         */
        [[nodiscard]] Eigen::Vector3d evaluate(double t) const override;

        /**
         * @brief 计算圆锥曲线上参数 t 处的切向量。
         * @param t 曲线参数，t ∈ [0, 1]。
         * @return 切向量（未归一化）。
         */
        [[nodiscard]] Eigen::Vector3d derivative(double t) const override;

        /** @brief 获取参数范围下限，始终返回 0.0。 */
        [[nodiscard]] double getStartT() const override { return 0.0; }

        /** @brief 获取参数范围上限，始终返回 1.0。 */
        [[nodiscard]] double getEndT() const override { return 1.0; }

        /** @brief 获取圆锥曲线类型。 */
        [[nodiscard]] ConicType getConicType() const { return m_conicType; }

        /** @brief 获取中心/顶点。 */
        [[nodiscard]] const Eigen::Vector3d& getCenter() const { return m_center; }

        /** @brief 获取 axisU 向量。 */
        [[nodiscard]] const Eigen::Vector3d& getAxisU() const { return m_axisU; }

        /** @brief 获取 axisV 向量。 */
        [[nodiscard]] const Eigen::Vector3d& getAxisV() const { return m_axisV; }

        /** @brief 获取起始参数。 */
        [[nodiscard]] double getStartParam() const { return m_startParam; }

        /** @brief 获取扫略范围。 */
        [[nodiscard]] double getSweepParam() const { return m_sweepParam; }

    private:
        /** 圆锥曲线类型。 */
        ConicType m_conicType;

        /** 中心（椭圆/双曲线）或顶点（抛物线）。 */
        Eigen::Vector3d m_center;

        /** 椭圆半长轴 / 抛物线轴向 / 双曲线实半轴。 */
        Eigen::Vector3d m_axisU;

        /** 椭圆半短轴 / 抛物线垂直方向 / 双曲线虚半轴。 */
        Eigen::Vector3d m_axisV;

        /** 起始参数（椭圆/双曲线：弧度；抛物线：t 值）。 */
        double m_startParam;

        /** 扫略范围（椭圆/双曲线：弧度；抛物线：t 变化量）。 */
        double m_sweepParam;
    };
} // egret

#endif // EGRET_PHYSICS_CURVE_CONIC_SECTION_H
