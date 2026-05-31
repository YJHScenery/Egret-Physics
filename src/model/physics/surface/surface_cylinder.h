/**
 * @file        surface_cylinder.h
 * @brief       标准圆柱面头文件，定义参数化圆柱面。
 * @details     定义 SurfaceCylinder 类，继承自 SurfaceBase，
 *              表示由半径、母线长度、包围角度和姿态变换定义的圆柱面。
 *              支持全圆柱（theta=2π）和部分圆柱（theta<2π）。
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

#ifndef EGRET_PHYSICS_SURFACE_CYLINDER_H
#define EGRET_PHYSICS_SURFACE_CYLINDER_H

#include "surface_base.h"
#include "constants.h"

namespace egret
{
    /**
     * @brief       标准圆柱面类，表示参数化圆柱面。
     * @details     SurfaceCylinder 继承自 SurfaceBase，表示由半径、母线长度、
     *              包围角度和姿态变换定义的圆柱面。
     *
     *              局部坐标系约定（姿态默认时）：
     *              - 轴线方向：+Z
     *              - 轴线末端（顶部中心）：(0, 0, l)
     *              - 半径：r
     *              - 角度起始：+X 轴方向（theta=0）
     *              - 角度增方向：绕 +Z 逆时针旋转
     *
     *              曲面由以下部分组成：
     *              1. 侧面：距离轴线 r，0 ≤ z ≤ l，角度 ∈ [0, theta]
     *              2. 底部圆盘（z=0）：0 ≤ 径向距离 ≤ r，角度 ∈ [0, theta]
     *              3. 顶部圆盘（z=l）：0 ≤ 径向距离 ≤ r，角度 ∈ [0, theta]
     *              4. [theta < 2π] 起始端面（theta=0）：径向平面
     *              5. [theta < 2π] 终止端面（theta=thetaMax）：径向平面
     *
     * @invariant   m_radius > 0
     * @invariant   m_length > 0
     * @invariant   m_thetaMax ∈ (0, 2π]
     * @see         SurfaceBase, SurfaceSphere, SurfacePlane, SurfaceBezier
     */
    class SurfaceCylinder : public SurfaceBase
    {
    public:
        /**
         * @brief 构造标准圆柱面。
         * @param radius 圆柱半径。
         * @param length 母线长度（高度）。
         * @param thetaMax 包围角度范围 (0, 2π]。2π 表示完整圆柱。
         * @param transform 局部坐标到世界坐标的变换。
         */
        explicit SurfaceCylinder(double radius,
                                 double length,
                                 double thetaMax = 2.0 * egret::PI,
                                 const Transform& transform = Transform());

        std::unique_ptr<SurfaceBase> clone() const override;

        /** @brief 获取曲面类型，始终返回 SurfaceType::Cylinder。 */
        [[nodiscard]] SurfaceType getType() const override;

        /**
         * @brief 计算世界点到圆柱面的有符号距离。
         * @param worldPoint 世界空间中的查询点。
         * @return 有符号距离。
         */
        [[nodiscard]] double signedDistance(const Eigen::Vector3d& worldPoint) const override;

        /**
         * @brief 返回圆柱面上距离 worldPoint 最近的点。
         * @param worldPoint 世界空间中的查询点。
         * @return 曲面上最近点的世界坐标。
         */
        [[nodiscard]] Eigen::Vector3d closestPoint(const Eigen::Vector3d& worldPoint) const override;

        /**
         * @brief 返回圆柱面上最近点处的单位法线。
         * @param worldPoint 世界空间中的查询点。
         * @return 单位法线向量（世界空间）。
         */
        [[nodiscard]] Eigen::Vector3d normal(const Eigen::Vector3d& worldPoint) const override;

        /**
         * @brief 检测形状与圆柱面的碰撞。
         * @param shape 待检测形状。
         * @param shapeTransform 形状的世界变换。
         * @param manifold [输出] 接触流形。
         * @return 是否碰撞。
         */
        [[nodiscard]] bool collide(const ShapeBase* shape,
                                   const Transform& shapeTransform,
                                   ContactManifold& manifold) const override;

        /** @brief 获取圆柱半径。 */
        [[nodiscard]] double getRadius() const { return m_radius; }

        /** @brief 获取母线长度。 */
        [[nodiscard]] double getLength() const { return m_length; }

        /** @brief 获取包围角度。 */
        [[nodiscard]] double getThetaMax() const { return m_thetaMax; }

        /** @brief 获取曲面变换。 */
        [[nodiscard]] const Transform& getSurfaceTransform() const { return m_transform; }

    private:
        /**
         * @brief 曲面组成部分枚举，用于确定法线方向。
         */
        enum class CylinderFeature : uint8_t
        {
            Lateral,    ///< 侧面
            TopCap,     ///< 顶部圆盘
            BottomCap,  ///< 底部圆盘
            StartCap,   ///< 起始端面 (theta=0)
            EndCap      ///< 终止端面 (theta=thetaMax)
        };

        /** @brief 在局部坐标系中计算有符号距离。 */
        [[nodiscard]] double signedDistanceLocal(const Eigen::Vector3d& local) const;

        /** @brief 在局部坐标系中找最近点。 */
        [[nodiscard]] Eigen::Vector3d closestPointLocal(const Eigen::Vector3d& local) const;

        /** @brief 在局部坐标系中找最近点，同时返回所属特征。 */
        [[nodiscard]] std::pair<Eigen::Vector3d, CylinderFeature>
        closestPointWithFeatureLocal(const Eigen::Vector3d& local) const;

        /** @brief 根据特征类型计算局部法线。 */
        [[nodiscard]] Eigen::Vector3d normalLocal(const Eigen::Vector3d& pointLocal,
                                                   CylinderFeature feature) const;

        /** @brief 将任意世界空间 Transform 转换为圆柱局部空间的等效 Transform。 */
        [[nodiscard]] Transform localizeTransform(const Transform& worldTransform) const;

        double m_radius;        ///< 圆柱半径。
        double m_length;        ///< 母线长度（高度）。
        double m_thetaMax;      ///< 包围角度范围 (0, 2π]。
        Transform m_transform;  ///< 局部→世界变换。
        bool m_isFullCylinder;  ///< 是否为完整圆柱（theta=2π）。
    };
} // egret

#endif // EGRET_PHYSICS_SURFACE_CYLINDER_H
