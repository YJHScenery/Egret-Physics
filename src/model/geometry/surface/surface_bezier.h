/**
 * @file        surface_bezier.h
 * @brief       双三次 Bezier 曲面头文件，定义一般曲面。
 * @details     定义 SurfaceBezier 类，继承自 SurfaceBase，
 *              使用双三次 Bezier 曲面公式创建一般曲面。
 *              最近点和法线计算使用数值方法（牛顿迭代）。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-01
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup     Surface (曲面几何体)
 */

#ifndef EGRET_PHYSICS_SURFACE_BEZIER_H
#define EGRET_PHYSICS_SURFACE_BEZIER_H

#include "surface_base.h"

namespace egret
{
    /**
     * @brief       双三次 Bezier 曲面类。
     * @details     SurfaceBezier 继承自 SurfaceBase，使用 4x4 控制点网格定义
     *              双三次 Bezier 曲面。曲面方程为：
     *
     *              S(u,v) = Σᵢ₌₀³ Σⱼ₌₀³ Bᵢ₃(u) Bⱼ₃(v) Pᵢⱼ
     *
     *              其中 Bᵢ₃ 为三次 Bernstein 基函数，Pᵢⱼ 为 4x4 控制点矩阵，
     *              (u,v) ∈ [0,1]²。
     *
     *              最近点计算使用数值方法（牛顿法迭代求 ||S(u,v)-P||² 最小值）。
     *              法线通过偏导数叉积计算：N = ∂S/∂u × ∂S/∂v。
     *
     * @invariant   m_controlPoints 为 4x4 的有效控制点矩阵
     * @see         SurfaceBase, SurfaceSphere, SurfacePlane, SurfaceCylinder
     */
    class SurfaceBezier : public SurfaceBase
    {
    public:
        /**
         * @brief 构造双三次 Bezier 曲面。
         * @param controlPoints 4x4 控制点矩阵。
         *        controlPoints[i][j] 对应参数 (u=i/3, v=j/3) 处的控制点。
         */
        explicit SurfaceBezier(
            const std::array<std::array<Eigen::Vector3d, 4>, 4>& controlPoints);

        std::unique_ptr<SurfaceBase> clone() const override;

        /** @brief 获取曲面类型，始终返回 SurfaceType::Bezier。 */
        [[nodiscard]] SurfaceType getType() const override;

        /**
         * @brief 计算世界点到 Bezier 曲面的有符号距离。
         * @param worldPoint 世界空间中的查询点。
         * @return 有符号距离（近似值）。
         */
        [[nodiscard]] double signedDistance(const Eigen::Vector3d& worldPoint) const override;

        /**
         * @brief 返回 Bezier 曲面上距离 worldPoint 最近的点。
         * @param worldPoint 世界空间中的查询点。
         * @return 曲面上最近点的世界坐标。
         */
        [[nodiscard]] Eigen::Vector3d closestPoint(const Eigen::Vector3d& worldPoint) const override;

        /**
         * @brief 返回 Bezier 曲面上最近点处的单位法线。
         * @param worldPoint 世界空间中的查询点。
         * @return 单位法线向量。
         */
        [[nodiscard]] Eigen::Vector3d normal(const Eigen::Vector3d& worldPoint) const override;

        /**
         * @brief 检测形状与 Bezier 曲面的碰撞。
         * @param shape 待检测形状。
         * @param shapeTransform 形状的世界变换。
         * @param manifold [输出] 接触流形。
         * @return 是否碰撞。
         */
        [[nodiscard]] bool collide(const ShapeBase* shape,
                                   const Transform& shapeTransform,
                                   ContactManifold& manifold) const override;

        /** @brief 获取控制点矩阵。 */
        [[nodiscard]] const std::array<std::array<Eigen::Vector3d, 4>, 4>&
        getControlPoints() const { return m_controlPoints; }

        /**
         * @brief 将双三次 Bezier 曲面离散化为三角形网格。
         * @details 在 (u,v) ∈ [0,1]² 上均匀采样生成三角形网格：
         *          - u/v 方向各分为 subdivisions 段
         *          - 每个采样点的位置通过 evaluate() 计算
         *          - 法线通过 deriveU() 和 deriveV() 的叉积计算
         *          控制点已在世界空间，无需额外变换。
         * @param config 细分配置（subdivisions 控制 u/v 采样密度）。
         * @return 三角形网格数据。
         */
        [[nodiscard]] TessellatedMesh tessellate(
            const TessellationConfig& config) const override;

    private:
        /**
         * @brief 计算 Bezier 曲面 S(u,v) 的值。
         * @param u 参数 u ∈ [0,1]。
         * @param v 参数 v ∈ [0,1]。
         * @return S(u,v) 的世界坐标。
         */
        [[nodiscard]] Eigen::Vector3d evaluate(double u, double v) const;

        /** @brief 计算 ∂S/∂u。 */
        [[nodiscard]] Eigen::Vector3d deriveU(double u, double v) const;

        /** @brief 计算 ∂S/∂v。 */
        [[nodiscard]] Eigen::Vector3d deriveV(double u, double v) const;

        /**
         * @brief 从曲面上的空间点反求近似的参数 (u,v)。
         * @param surfacePoint 曲面上的点。
         * @return (u, v) 参数对。
         */
        [[nodiscard]] std::pair<double, double> findParameters(
            const Eigen::Vector3d& surfacePoint) const;

        /**
         * @brief 预计算 Bezier 系数矩阵。
         * @details 将标准 Bernstein 基展开为幂基形式。
         */
        void precomputeCoefficients();

        /** 4x4 控制点矩阵。controlPoints[i][j] 对应 (u=i/3, v=j/3)。 */
        std::array<std::array<Eigen::Vector3d, 4>, 4> m_controlPoints;

        /** 预计算的幂基系数矩阵。m_coeffs[pi][pj] 对应 u^pi * v^pj 的系数向量。 */
        std::array<std::array<Eigen::Vector3d, 4>, 4> m_coeffs{};

        /**
         * @brief 最后一次 closestPoint 调用的参数值缓存。
         * @details 避免 collide/signedDistance 重复调用 findParameters 时产生歧义。
         */
        mutable double m_lastU{0.0};
        mutable double m_lastV{0.0};
    };
} // egret

#endif // EGRET_PHYSICS_SURFACE_BEZIER_H
