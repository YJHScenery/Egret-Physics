/**
 * @file        support_surface.cpp
 * @brief       支撑曲面约束器实现文件。
 * @details     实现 SupportSurface 类的碰撞检测、冲量施加和实体管理功能。
 *
 * @ingroup     Constraints
 */

#include "support_surface.h"

#include "physical_entity.h"
#include "uuid_generator.h"

namespace egret
{
    SupportSurface::SupportSurface(std::uint64_t id,
                                   std::unique_ptr<SurfaceBase> surface)
        : PhysicsAbstract(id)
          , ConstraintsBase(id)
          , m_surface(std::move(surface))
    {
    }

    ConstraintType SupportSurface::getType() const
    {
        return ConstraintType::SupportSurface;
    }

    std::vector<PhysicalEntity*> SupportSurface::getConstrainedEntities()
    {
        std::vector<PhysicalEntity*> result;
        result.reserve(m_registeredEntities.size() + m_physicalEntities.size());

        // 添加注册实体
        for (auto* entity : m_registeredEntities) {
            if (entity != nullptr) {
                result.push_back(entity);
            }
        }

        // 添加基类列表中的实体（去重）
        for (auto* entity : m_physicalEntities) {
            if (entity != nullptr && m_registeredEntities.find(entity) == m_registeredEntities.end()) {
                result.push_back(entity);
            }
        }

        return result;
    }

    const std::vector<PhysicalEntity*>& SupportSurface::getConstrainedEntities() const
    {
        // 注意：基类的 m_physicalEntities 可能为空（我们主要使用 m_registeredEntities）
        return m_physicalEntities;
    }

    void SupportSurface::registerEntity(PhysicalEntity* entity)
    {
        if (entity == nullptr) {
            return;
        }

        const auto [it, inserted] = m_registeredEntities.insert(entity);
        if (inserted) {
            m_entityIdMap[entity->getId()] = entity;
        }
    }

    bool SupportSurface::unregisterEntity(std::uint64_t entityId)
    {
        auto it = m_entityIdMap.find(entityId);
        if (it == m_entityIdMap.end()) {
            return false;
        }

        m_registeredEntities.erase(it->second);
        m_entityIdMap.erase(it);
        return true;
    }

    bool SupportSurface::isRegistered(std::uint64_t entityId) const
    {
        return m_entityIdMap.find(entityId) != m_entityIdMap.end();
    }

    void SupportSurface::applyVelocityConstraint(double dt)
    {
        if (!m_enabled || m_surface == nullptr) {
            return;
        }

        // 遍历所有注册实体，检测碰撞并施加冲量
        for (auto* entity : m_registeredEntities) {
            if (entity == nullptr) {
                continue;
            }

            // 跳过静态/固定实体（质量为0）
            if (entity->isFixed()) {
                continue;
            }

            // 获取实体形状
            auto shape = entity->getShape();
            if (shape == nullptr) {
                continue;
            }

            // 检测实体形状与曲面的碰撞
            ContactManifold manifold;
            if (!m_surface->collide(shape.get(), entity->getTransformCR(), manifold)) {
                continue;
            }

            // 对每个接触点施加冲量
            for (const auto& contact : manifold) {
                applyContactImpulse(entity, contact, dt);
            }
        }
    }

    void SupportSurface::applyContactImpulse(PhysicalEntity* entity,
                                             const ContactPoint& contact,
                                             double /*dt*/)
    {
        if (entity == nullptr || entity->isFixed()) {
            return;
        }

        const double entityMass = entity->getMass();
        if (entityMass <= 0.0) {
            return;
        }

        const double invMass = 1.0 / entityMass;
        const Eigen::Vector3d n = contact.normal;
        const Eigen::Vector3d cp = contact.position;
        const Eigen::Vector3d com = entity->getPosition();

        // 接触点相对于质心的偏移
        const Eigen::Vector3d r = cp - com;

        // 获取实体的速度和角速度
        const Eigen::Vector3d v = entity->getVelocity();
        const Eigen::Vector3d omega = entity->getAngularVelocity();

        // 计算接触点处的速度
        const Eigen::Vector3d vContact = v + omega.cross(r);

        // 法向相对速度分量
        const double vNormal = vContact.dot(n);

        // 如果实体正在远离曲面（vNormal > 0），不需要施加冲量
        if (vNormal > 1e-8) {
            return;
        }

        // 确定恢复系数
        double e = m_restitution;
        if (e < 0.0) {
            // 使用实体自身的恢复系数
            e = std::clamp(entity->getRestitution(), 0.0, 1.0);
        }
        e = std::clamp(e, 0.0, 1.0);

        // 计算有效质量（考虑旋转效应）
        // 对于刚体：有效质量倒数 = 1/m + dot((I⁻¹ · (r × n)) × r, n)
        double effectiveMassInv = invMass;

        // 尝试获取实体的转动惯量矩阵
        // 对于 RigidBody，我们可以访问其形状计算转动惯量
        auto shape = entity->getShape();
        if (shape != nullptr && entity->getAngularVelocity().squaredNorm() >= 0.0) {
            // 计算世界空间下的逆转动惯量
            const Eigen::Matrix3d R = entity->getTransformCR().getRotationMatrix();
            const Eigen::Matrix3d I_local = shape->getInertiaTensor(entityMass);
            const Eigen::Matrix3d I_world = R * I_local * R.transpose();

            // 避免奇异矩阵（质量为零或退化形状）
            double det = I_world.determinant();
            if (std::abs(det) > 1e-20) {
                const Eigen::Matrix3d I_inv_world = I_world.inverse();

                // r × n
                const Eigen::Vector3d rCrossN = r.cross(n);
                // I⁻¹ · (r × n)
                const Eigen::Vector3d IInvCrossN = I_inv_world * rCrossN;
                // (I⁻¹ · (r × n)) × r
                const Eigen::Vector3d term = IInvCrossN.cross(r);
                // dot(term, n)
                effectiveMassInv += term.dot(n);
            }
        }

        // 避免除零
        if (std::abs(effectiveMassInv) < 1e-20) {
            return;
        }

        // 法向冲量大小：j = -(1 + e) * v_n / effectiveMassInv
        const double j = -(1.0 + e) * vNormal / effectiveMassInv;

        // 冲量向量
        const Eigen::Vector3d jVector = j * n;

        // 施加线速度修正
        entity->setSpeed(v + jVector * invMass);

        // 施加角速度修正（如果有转动惯量）
        if (r.squaredNorm() > 1e-12 && shape != nullptr) {
            const Eigen::Matrix3d R = entity->getTransformCR().getRotationMatrix();
            const Eigen::Matrix3d I_local = shape->getInertiaTensor(entityMass);
            const Eigen::Matrix3d I_world = R * I_local * R.transpose();

            double det = I_world.determinant();
            if (std::abs(det) > 1e-20) {
                const Eigen::Matrix3d I_inv_world = I_world.inverse();
                // Δω = I⁻¹ · (r × J)
                const Eigen::Vector3d deltaOmega = I_inv_world * (r.cross(jVector));
                entity->setAngular(omega + deltaOmega);
            }
        }
    }

    SupportSurface::SupportSurface(const SupportSurface& other)
    {
        m_id = UuidGenerator::instance().generate();
        m_physicalEntities = other.m_physicalEntities;
        m_enabled = other.m_enabled;
        m_surface = other.m_surface->clone();
        m_registeredEntities = other.m_registeredEntities;
        m_entityIdMap = other.m_entityIdMap;
        m_restitution = other.m_restitution;
        m_frictionCoeff = other.m_frictionCoeff;
    }

    SupportSurface& SupportSurface::operator=(const SupportSurface& other)
    {
        if (this == &other) {
            return *this;
        }

        m_id = UuidGenerator::instance().generate();
        m_physicalEntities = other.m_physicalEntities;
        m_enabled = other.m_enabled;
        m_surface = other.m_surface->clone();
        m_registeredEntities = other.m_registeredEntities;
        m_entityIdMap = other.m_entityIdMap;
        m_restitution = other.m_restitution;
        m_frictionCoeff = other.m_frictionCoeff;
        return *this;
    }

    std::unique_ptr<PhysicsAbstract> SupportSurface::clone(std::uint64_t id) const
    {
        auto surface = std::make_unique<SupportSurface>(*this);
        surface->m_id = id;
        return surface;
    }

    double SupportSurface::computeConstraintError() const
    {
        if (m_surface == nullptr) {
            return 0.0;
        }

        double maxPenetration = 0.0;

        for (auto* entity : m_registeredEntities) {
            if (entity == nullptr || entity->isFixed()) {
                continue;
            }

            auto shape = entity->getShape();
            if (shape == nullptr) {
                continue;
            }

            ContactManifold manifold;
            if (!m_surface->collide(shape.get(), entity->getTransformCR(), manifold)) {
                continue;
            }

            for (const auto& contact : manifold) {
                if (contact.penetration > maxPenetration) {
                    maxPenetration = contact.penetration;
                }
            }
        }

        return maxPenetration;
    }
} // egret
