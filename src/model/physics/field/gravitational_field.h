/**
* @file        gravitational_field.h
 * @brief       引力场头文件
 * @details     定义GravitationalField类，提供质点间的万有引力功能，
 *              场源本身也可以是一个运动的物理实体。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-26
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#ifndef EGRET_PHYSICS_GRAVITATIONAL_FIELD_H
#define EGRET_PHYSICS_GRAVITATIONAL_FIELD_H
#include "field_base.h"
#include "particle.h"
#include "constants.h"

namespace egret
{
    /**
     * @brief       引力场类，实现质点间的万有引力场。
     * @details     GravitationalField 继承自 FieldBase 和 Particle，提供质点间的
     *              万有引力功能，场源本身也可以是一个运动的物理实体。
     *              引力场是保守场，势能遵循牛顿万有引力定律。
     *              引力场在空间中各点的场向量不同（非均匀场）。
     *              支持设置耦合系数（默认为万有引力常数G）。
     * 
     * @invariant   m_coupling > 0，耦合系数必须为正值
     * @invariant   static_minDistanceSquared > 0，最小距离平方用于避免奇点
     * @invariant   m_fixed 表示场源是否固定（不运动）
     * @remark      引力场是非均匀场，场向量随距离变化，适合模拟天体运动
     * @see         FieldBase, Particle, PhysicalEntity
     */
    class GravitationalField : public FieldBase, public Particle
    {
    public:
        GravitationalField();

        GravitationalField(const Eigen::Vector3d& position,
                           const Eigen::Vector3d& speed,
                           double mass,
                           double coupling = G,
                           bool fixed = false);

        Eigen::Vector3d sample(const Eigen::Vector3d& position) override;

        static void setMinDistanceSquared(double minDistance);

        [[nodiscard]] static double getMinDistanceSquared();

        double potential(const Eigen::Vector3d& position) override;

        void applyToEntity(PhysicalEntity* entity) override;

        void applyForce(double time) override;

        void setFixed(bool fixed);

        [[nodiscard]] bool isFixed() const override;

        void setCouplingCoefficient(double coefficient);

        [[nodiscard]] double getCouplingCoefficient() const;

    private:
        double m_coupling{G};

        bool m_fixed{false};

        static inline double static_minDistanceSquared = 1e-4;
    };
} // egret

#endif // EGRET_PHYSICS_GRAVITATIONAL_FIELD_H
