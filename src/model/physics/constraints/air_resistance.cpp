/**
 * @file        air_resistance.cpp
 * @brief       空气阻力约束器实现文件
 * @details     实现 AirResistance 类的各项成员函数，
 *              包括三种平动阻力模型和旋转阻力的物理计算。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-xx
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#include "air_resistance.h"
#include "physical_entity.h"

namespace egret
{
    AirResistance::AirResistance(const std::uint64_t id)
        : PhysicsAbstract(id), ConstraintsBase(id)
    {
    }

    // ==================== 实体注册 ====================
    void AirResistance::registerEntity(PhysicalEntity* entity)
    {
        if (entity == nullptr || isRegistered(entity->getId())) return;
        ConstraintsBase::registerEntity(entity);
        m_entityDrags.push_back(EntityDragEntry{entity, m_defaultConfig});
    }

    bool AirResistance::unregisterEntity(const std::uint64_t entityId)
    {
        const bool removed = ConstraintsBase::unregisterEntity(entityId);
        if (removed) {
            const auto it = findEntry(entityId);
            if (it != m_entityDrags.end()) {
                m_entityDrags.erase(it);
            }
        }
        return removed;
    }

    void AirResistance::addEntity(PhysicalEntity* entity, const DragConfig& config)
    {
        if (entity == nullptr) return;
        if (isRegistered(entity->getId())) {
            setEntityDragConfig(entity->getId(), config);
            return;
        }
        ConstraintsBase::registerEntity(entity);
        m_entityDrags.push_back(EntityDragEntry{entity, config});
    }

    // ==================== 配置接口 ====================
    void AirResistance::setEntityDragConfig(const std::uint64_t entityId, const DragConfig& config)
    {
        const auto it = findEntry(entityId);
        if (it != m_entityDrags.end()) {
            it->config = config;
        }
    }

    DragConfig AirResistance::getEntityDragConfig(const std::uint64_t entityId) const
    {
        const auto it = findEntry(entityId);
        if (it != m_entityDrags.end()) {
            return it->config;
        }
        return m_defaultConfig;
    }

    bool AirResistance::removeEntityDragConfig(const std::uint64_t entityId)
    {
        const auto it = findEntry(entityId);
        if (it != m_entityDrags.end()) {
            it->config = m_defaultConfig;
            return true;
        }
        return false;
    }

    void AirResistance::setDefaultDragConfig(const DragConfig& config)
    {
        m_defaultConfig = config;
    }

    const DragConfig& AirResistance::getDefaultDragConfig() const
    {
        return m_defaultConfig;
    }

    // ==================== 查找辅助 ====================

    std::vector<AirResistance::EntityDragEntry>::iterator AirResistance::findEntry(const std::uint64_t entityId)
    {
        return std::ranges::find_if(m_entityDrags,
                                    [entityId](const EntityDragEntry& entry)
                                    {
                                        return entry.entity && entry.entity->getId() == entityId;
                                    });
    }

    std::vector<AirResistance::EntityDragEntry>::const_iterator AirResistance::findEntry(
        const std::uint64_t entityId) const
    {
        return std::ranges::find_if(m_entityDrags,
                                    [entityId](const EntityDragEntry& entry)
                                    {
                                        return entry.entity && entry.entity->getId() == entityId;
                                    });
    }

    ConstraintType AirResistance::getType() const
    {
        return ConstraintType::AirResistance;
    }

    std::unique_ptr<PhysicsAbstract> AirResistance::clone(const std::uint64_t id) const
    {
        auto airResistance = std::make_unique<AirResistance>(*this);
        airResistance->setId(id);
        return airResistance;
    }

    void AirResistance::applyConstraintForces()
    {
        for (const EntityDragEntry& entry : m_entityDrags) {
            PhysicalEntity* entity = entry.entity;
            if (entity == nullptr || !entity->isActive()) {
                continue;
            }

            // 静态体（质量为零或负值）不施加阻力
            if (entity->getMass() <= 0.0) {
                continue;
            }

            applyDragToEntity(entity, entry.config);
        }
    }

    void AirResistance::applyDragToEntity(PhysicalEntity* entity, const DragConfig& config)
    {
        // ---------- 平动阻力 ----------
        const Eigen::Vector3d velocity = entity->getVelocity();
        const double speedSqr = velocity.squaredNorm();

        if (speedSqr > kVelocityEpsilon * kVelocityEpsilon) {
            const double speed = std::sqrt(speedSqr);
            Eigen::Vector3d dragForce = Eigen::Vector3d::Zero();

            switch (config.model) {
            case DragModel::Linear: {
                // F = -b * v
                dragForce = -config.linearCoeff * velocity;
                break;
            }
            case DragModel::Quadratic: {
                // F = -0.5 * rho * Cd * A * |v| * v
                const double coeff = -0.5 * config.fluidDensity
                    * config.dragCoefficient
                    * config.crossSectionArea
                    * speed;
                dragForce = coeff * velocity;
                break;
            }
            case DragModel::Constant: {
                // F = -c * v̂
                const Eigen::Vector3d direction = velocity / speed;
                dragForce = -config.constantMagnitude * direction;
                break;
            }
            }

            entity->upsertForce(Force{dragForce, m_id});
        }

        // ---------- 旋转阻力 ----------
        if (config.enableRotationalDrag && entity->hasRotation()) {
            const Eigen::Vector3d angularVel = entity->getAngularVelocity();
            const double angularSpeedSqr = angularVel.squaredNorm();

            if (angularSpeedSqr > kVelocityEpsilon * kVelocityEpsilon) {
                const double angularSpeed = std::sqrt(angularSpeedSqr);
                Eigen::Vector3d dragTorque = Eigen::Vector3d::Zero();

                // 线性旋转阻力：τ -= b_rot * ω
                dragTorque += -config.rotationalLinearCoeff * angularVel;

                // 平方旋转阻力：τ -= c_rot * |ω| * ω
                dragTorque += -config.rotationalQuadraticCoeff * angularSpeed * angularVel;

                entity->upsertTorque(Torque{dragTorque, m_id});
            }
        }
    }
} // egret
