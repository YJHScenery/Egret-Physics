/**
 * @file        shape_capsule.h
 * @brief       胶囊体刚体形状头文件，定义胶囊体形状。
 * @details     定义 ShapeCapsule 类，继承自 ShapeBase，表示胶囊体形状，
 *              用于物理碰撞检测和渲染。
 *              胶囊体由半径为 r 的端半球和高度为 h 的中央圆柱体组成，
 *              几何中心位于原点，母线与 z 轴平行。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-20
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#ifndef EGRET_PHYSICS_SHAPE_CAPSULE_H
#define EGRET_PHYSICS_SHAPE_CAPSULE_H
#include "shape_base.h"

namespace egret
{

    /**
     * @brief       胶囊体形状类，表示胶囊体刚体的碰撞形状。
     * @details     ShapeCapsule 继承自 ShapeBase，表示一个以原点为中心、
     *              沿Z轴方向的胶囊体形状，用于物理碰撞检测和转动惯量计算。
     *              胶囊体由半径为 m_radius 的端半球和高度为 m_height 的中央圆柱体组成：
     *              \f$\mathcal{Cs} = \{ (x,y,z) \mid \exists c \in [-\frac{h}{2}, \frac{h}{2}],
     *              x^2 + y^2 + (z-c)^2 \leq r^2 \}\f$
     *              总轴向跨度为 \f$h + 2r\f$，其中圆柱段从 \f$-h/2\f$ 到 \f$h/2\f$。
     *              支撑函数处处可导，便于 GJK 算法高效运行。
     *
     * @invariant   m_radius > 0，端半球半径必须为正值
     * @invariant   m_height > 0，中央圆柱高度必须为正值
     * @invariant   typeId() 返回 ShapeType::Capsule
     * @invariant   质心位于原点 (0, 0, 0)
     * @remark      胶囊体形状不包含位置信息，位置由 Transform 管理
     * @see         ShapeBase, ShapeCylinder, Transform
     */
    class ShapeCapsule : public ShapeBase
    {
    public:
        /**
         * @brief       构造胶囊体形状。
         * @param radius 端半球半径，必须 > 0。
         * @param height 中央圆柱高度，必须 > 0。
         */
        ShapeCapsule(double radius, double height);

        [[nodiscard]] ShapeType getType() const override;

        // --- 几何属性访问器 ---

        /** @brief 获取端半球半径。 */
        [[nodiscard]] double getRadius() const { return m_radius; }

        /** @brief 获取中央圆柱高度。 */
        [[nodiscard]] double getHeight() const { return m_height; }

        /** @brief 获取中央圆柱半高。 */
        [[nodiscard]] double getHalfHeight() const { return m_height * 0.5; }

        // --- ShapeBase 虚函数覆盖 ---

        [[nodiscard]] double getVolume() const override;
        [[nodiscard]] Eigen::Vector3d getCenterOfMass() const override;
        [[nodiscard]] Eigen::Matrix3d getInertiaTensor(double mass) const override;
        [[nodiscard]] AABB getAABB(const Transform &transform) const override;
        [[nodiscard]] ShapeLoadInfo getLoadInfo() const override;

        /**
         * @brief       GJK 支撑函数。
         * @details     胶囊体支撑函数处处可导，给定归一化方向 \f$\mathbf{d}\f$，
         *              支撑点为 \f$(r d_x,\; r d_y,\; r d_z + \frac{h}{2}\operatorname{sgn}(d_z))\f$。
         * @param direction 世界坐标系中的搜索方向。
         * @param transform 形状的局部到世界变换。
         * @return 世界坐标系中沿该方向的最远点。
         */
        [[nodiscard]] Eigen::Vector3d support(const Eigen::Vector3d &direction,
                                              const Transform &transform) const override;

        // --- 参数修改 ---

        /** @brief 设置端半球半径，小于 1e-12 时标记形状无效。 */
        void setRadius(double radius);

        /** @brief 设置中央圆柱高度，小于 1e-12 时标记形状无效。 */
        void setHeight(double height);

    protected:
        ShapeCapsule() = default;

        double m_radius{};   ///< 端半球半径
        double m_height{};   ///< 中央圆柱高度
    };

} // egret

#endif // EGRET_PHYSICS_SHAPE_CAPSULE_H
