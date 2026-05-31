/**
 * @file        surface_sphere.h
 * @brief       标准（椭）球面头文件，定义解析式椭球面曲面。
 * @details     定义 SurfaceSphere 类，继承自 SurfaceBase，
 *              使用解析方法计算椭球面的有符号距离、最近点、法线和碰撞。
 *              支持标准球体（三个半轴相等）和椭球体（三个半轴不等）。
 *              由于 Bezier 曲面无法精确生成球面，此类提供精确的球面解析解。
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

#ifndef EGRET_PHYSICS_SURFACE_SPHERE_H
#define EGRET_PHYSICS_SURFACE_SPHERE_H

#include "surface_base.h"

namespace egret
{
    /**
     * @brief       标准（椭）球面类，使用解析公式定义椭球面。
     * @details     SurfaceSphere 继承自 SurfaceBase，表示一个由三个半轴参数定义的
     *              （椭）球面。在局部坐标系中，曲面方程为：
     *              (x/a)² + (y/b)² + (z/c)² = 1
     *              其中 (a,b,c) 为三个半轴长度。
     *              当 a = b = c 时退化为标准球面。
     *
     *              所有查询接口（signedDistance/closestPoint/normal）均使用解析方法，
     *              无需数值迭代，性能优异。
     *
     *              碰撞检测支持：
     *              - 对球体形状 (ShapeSphere)：使用解析法（球心距离判断）
     *              - 对其他形状：使用基类默认的支撑函数 + 最近点法
     *
     * @invariant   三个半轴分量均 > 0
     * @see         SurfaceBase, SurfacePlane, SurfaceCylinder, SurfaceBezier
     */
    class SurfaceSphere : public SurfaceBase
    {
    public:
        /**
         * @brief 构造一个（椭）球面。
         * @param radii 三个半轴长度 (x, y, z)，必须全为正。
         * @param transform 曲面局部坐标到世界坐标的变换。
         */
        explicit SurfaceSphere(const Eigen::Vector3d& radii,
                               const Transform& transform = Transform())
            : m_radii(radii.cwiseAbs())     // 确保半轴为正
            , m_transform(transform)
        {
        }


        std::unique_ptr<SurfaceBase> clone() const override
        {
            return std::make_unique<SurfaceSphere>(m_radii, m_transform);
        }

        /**
         * @brief 构造一个标准球面（三个半轴相等）。
         * @param radius 球体半径。
         * @param transform 曲面局部坐标到世界坐标的变换。
         */
        explicit SurfaceSphere(double radius,
                               const Transform& transform = Transform())
            : m_radii(radius, radius, radius)
            , m_transform(transform)
        {
        }

        /** @brief 获取曲面类型，始终返回 SurfaceType::Sphere。 */
        [[nodiscard]] SurfaceType getType() const override { return SurfaceType::Sphere; }

        /**
         * @brief 计算世界点到椭球面的有符号距离。
         * @details 实现步骤：
         *          1. 将点从世界空间变换到局部空间
         *          2. 缩放局部点使椭球退化为单位球
         *          3. 计算缩放后点到单位球面的距离
         *          返回值正负规则：
         *          - 正值：点在椭球外部
         *          - 负值：点在椭球内部
         *          注意：椭球的 signed distance 不是严格的欧几里得距离，
         *          而是经过缩放的近似度量。这里使用 f(P) = ||P_scaled|| - 1 作为 SDF 近似。
         * @param worldPoint 世界空间中的查询点。
         * @return 有符号距离。
         */
        [[nodiscard]] double signedDistance(const Eigen::Vector3d& worldPoint) const override
        {
            const Eigen::Vector3d local = m_transform.worldToLocal(worldPoint);
            // 将椭圆体缩放到单位球：scaled = (x/a, y/b, z/c)
            const Eigen::Vector3d scaled(
                local.x() / m_radii.x(),
                local.y() / m_radii.y(),
                local.z() / m_radii.z()
            );
            return scaled.norm() - 1.0;
        }

        /**
         * @brief 返回曲面上距离 worldPoint 最近的点。
         * @details 实现步骤：
         *          1. 将点变换到局部空间
           *          2. 缩放到单位球空间
           *          3. 在单位球上求最近点（归一化）
           *          4. 逆缩放回椭球空间
           *          5. 变换回世界空间
           * @param worldPoint 世界空间中的查询点。
           * @return 曲面上最近点的世界坐标。
           */
        [[nodiscard]] Eigen::Vector3d closestPoint(const Eigen::Vector3d& worldPoint) const override
        {
            const Eigen::Vector3d local = m_transform.worldToLocal(worldPoint);
            // 缩放到单位球空间
            const Eigen::Vector3d scaled(
                local.x() / m_radii.x(),
                local.y() / m_radii.y(),
                local.z() / m_radii.z()
            );
            const double norm = scaled.norm();
            if (norm < 1e-12) {
                // 点在椭球中心附近，选择 +X 轴方向作为参考
                const Eigen::Vector3d refPoint(m_radii.x(), 0.0, 0.0);
                return m_transform.localToWorld(refPoint);
            }
            // 单位球上最近点就是归一化方向
            const Eigen::Vector3d unitScaled = scaled / norm;
            // 逆缩放回椭球
            const Eigen::Vector3d closestLocal(
                unitScaled.x() * m_radii.x(),
                unitScaled.y() * m_radii.y(),
                unitScaled.z() * m_radii.z()
            );
            return m_transform.localToWorld(closestLocal);
        }

        /**
         * @brief 返回曲面上最近点处的单位法线。
         * @details 法线方向为椭球面在最近点处的梯度方向。
         *          在局部空间中，法线方向为 (x/a², y/b², z/c²) 归一化。
         * @param worldPoint 世界空间中的查询点。
         * @return 单位法线向量（世界空间）。
         */
        [[nodiscard]] Eigen::Vector3d normal(const Eigen::Vector3d& worldPoint) const override
        {
            const Eigen::Vector3d local = m_transform.worldToLocal(worldPoint);
            // 椭球在局部空间的梯度方向
            const Eigen::Vector3d gradLocal(
                local.x() / (m_radii.x() * m_radii.x()),
                local.y() / (m_radii.y() * m_radii.y()),
                local.z() / (m_radii.z() * m_radii.z())
            );
            const double gradNorm = gradLocal.norm();
            if (gradNorm < 1e-12) {
                // 中心处梯度为零，选择 +X 方向
                return m_transform.localToWorld(Eigen::Vector3d(1.0, 0.0, 0.0)).normalized();
            }
            const Eigen::Vector3d nLocal = gradLocal / gradNorm;
            // 将局部法线变换到世界空间（使用旋转部分，不考虑缩放）
            return m_transform.localToWorld(nLocal).normalized();
        }

        /**
         * @brief 检测形状与椭球面的碰撞。
         * @details 对球体形状 (ShapeSphere) 使用解析法：
         *          - 计算球心到椭球面的有符号距离
         *          - 如果球心距离 < 球半径，则发生碰撞
         *          - 接触法线为椭球面在最近点处的法线
         *          对其他形状，回退到基类默认实现。
         * @param shape 待检测形状。
         * @param shapeTransform 形状的世界变换。
         * @param manifold [输出] 接触流形。
         * @return 是否碰撞。
         */
        [[nodiscard]] bool collide(const ShapeBase* shape,
                                   const Transform& shapeTransform,
                                   ContactManifold& manifold) const override;

        /** @brief 获取椭球半轴。 */
        [[nodiscard]] const Eigen::Vector3d& getRadii() const { return m_radii; }

        /** @brief 获取曲面变换。 */
        [[nodiscard]] const Transform& getSurfaceTransform() const { return m_transform; }

        /**
         * @brief 将椭球面离散化为三角形网格。
         * @details 使用 UV 球体算法生成三角形网格：
         *          - 经线方向（绕 Z 轴）分为 subdivisions 段
         *          - 纬线方向（从北极到南极）分为 subdivisions/2 段
         *          - 顶点位置在局部空间计算后变换到世界空间
         *          - 法线通过椭球梯度计算后变换到世界空间
         * @param config 细分配置（subdivisions 控制经纬分段数）。
         * @return 三角形网格数据。
         */
        [[nodiscard]] TessellatedMesh tessellate(
            const TessellationConfig& config) const override;

    private:
        /** 三个半轴长度 (a, b, c)，对应 x/y/z 方向。 */
        Eigen::Vector3d m_radii;

        /** 局部坐标到世界坐标的变换。 */
        Transform m_transform;
    };
} // egret

#endif // EGRET_PHYSICS_SURFACE_SPHERE_H
