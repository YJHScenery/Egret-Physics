/**
 * @file        particle.h
 * @brief       粒子头文件，定义最简单的物理实体。
 * @details     定义 Particle 类，继承自 PhysicalEntity，代表一个具有位置、速度的粒子，
 *              不考虑旋转惯性，用于简化的物理模拟场景。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-30
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#ifndef EGRET_PHYSICS_PARTICLE_H
#define EGRET_PHYSICS_PARTICLE_H
#include "physical_entity.h"

namespace egret
{

    /**
     * @brief       质点类，表示不考虑旋转惯性的简化物理实体。
     * @details     Particle 继承自 PhysicalEntity，代表一个具有位置、速度、质量的质点，
     *              不考虑旋转惯性，用于简化的物理模拟场景（如粒子系统、点质量模型）。
     *              质点的转动惯量为零，角动量始终为零，力矩不产生效果。
     *              适合用于模拟尘埃、烟雾、小型物体等不需要考虑旋转的场景。
     *
     * @invariant   m_mass >= 0，质量为0表示静态质点
     * @invariant   角动量始终返回零向量，力矩不产生效果
     * @remark      质点不响应旋转动力学，适合大规模粒子系统
     * @see         PhysicalEntity, RigidBody, Force
     */
    class Particle : public PhysicalEntity
    {
    public:
        Particle(const Eigen::Vector3d &position, const Eigen::Vector3d &speed, double mass, std::uint64_t id);

        [[nodiscard]] bool hasRotation() const override;

        Eigen::Vector3d getTorque() override;

        Eigen::Vector3d getAngularMomentum() override;

        double getKineticEnergy() override;

        [[nodiscard]] std::unique_ptr<PhysicsAbstract> clone(std::uint64_t id) const override;

    protected:
        Particle() = default;
    };

} // egret

#endif // EGRET_PHYSICS_PARTICLE_H
