/**
* @file        particle.cpp
 * @brief       粒子实现文件，定义最简单的物理实体。
 * @details     实现 Particle 类的各项成员函数。
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

#include "particle.h"

namespace egret
{
    Particle::Particle(const Eigen::Vector3d &position, const Eigen::Vector3d &speed, double mass, std::uint64_t id)
        : PhysicsAbstract(id), PhysicalEntity(position, speed, mass, id)
    {
    }

    bool Particle::hasRotation() const
    {
        return false;
    }

    Eigen::Vector3d Particle::getTorque()
    {
        // 质点不存在转动，返回零向量
        return Eigen::Vector3d::Zero();
    }

    Eigen::Vector3d Particle::getAngularMomentum()
    {
        // 质点不存在转动，返回零向量
        return Eigen::Vector3d::Zero();
    }

    double Particle::getKineticEnergy()
    {
        return 0.5 * m_mass * m_velocity.squaredNorm();
    }

    std::unique_ptr<PhysicsAbstract> Particle::clone(std::uint64_t id) const
    {
        auto entity = std::make_unique<Particle>(*this);
        entity->setId(id);
        return entity;
    }
} // egret
