/**
 * @file        support_surface.cpp
 * @brief       支撑曲面约束器实现文件。
 * @details     实现 SupportSurface 类的碰撞检测、冲量施加和实体管理功能。
 *
 * @ingroup     Constraints
 */

#include "support_surface.h"

#include "physical_entity.h"
#include "unique_id_generator.h"

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

    void SupportSurface::applyConstraintForces(double dt)
    {
        if (!m_enabled || m_surface == nullptr || m_penaltyStiffness <= 0.0) {
            return;
        }

        std::ignore = dt;

        // 曲面类型决定了罚力策略。平面曲面的 signedDistance 精确（欧几里得距离），
        // 罚力 + 冲量双修正工作良好。曲线曲面（球/柱/Bezier）的碰撞检测基于
        // 局部切平面近似，signedDistance 可能有偏差，高刚度罚力 + 冲量双修正
        // 容易导致过度弹跳。对曲线曲面降低有效罚刚度。
        const SurfaceType st = m_surface->getType();
        const double stiffnessScale = (st == SurfaceType::Plane) ? 1.0 : 0.1;
        const double effectiveStiffness = m_penaltyStiffness * stiffnessScale;

        // 遍历所有注册实体，对穿透或接近曲面的实体施加法向罚力
        for (auto* entity : m_registeredEntities) {
            if (entity == nullptr || entity->isFixed()) {
                continue;
            }

            auto shape = entity->getShape();
            if (shape == nullptr) {
                continue;
            }

            // 检测碰撞（获取穿透信息）
            ContactManifold manifold;
            if (!m_surface->collide(shape.get(), entity->getTransformCR(), manifold)) {
                continue;
            }

            for (const auto& contact : manifold) {
                if (contact.penetration <= 0.0) {
                    continue;
                }

                // 罚力：F = -k * penetration * normal
                // 方向指向曲面外部（法线方向）
                const double forceMagnitude = effectiveStiffness * contact.penetration;
                const Eigen::Vector3d penaltyForce = forceMagnitude * contact.normal;

                // 通过 upsertForce 注入实体力累加器（使用约束器自身的 ID 做 key）
                entity->upsertForce(Force{penaltyForce, m_id});

                // 如果接触点偏离质心，还需注入等效力矩
                const Eigen::Vector3d com = entity->getPosition();
                const Eigen::Vector3d r = contact.position - com;
                if (r.squaredNorm() > 1e-12) {
                    const Eigen::Vector3d penaltyTorque = r.cross(penaltyForce);
                    entity->upsertTorque(Torque{penaltyTorque, m_id});
                }
            }
        }
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
        : PhysicsAbstract(other)
        , ConstraintsBase(other)
        , m_surface(other.m_surface->clone())
        , m_restitution(other.m_restitution)
        , m_frictionCoeff(other.m_frictionCoeff)
        , m_penaltyStiffness(other.m_penaltyStiffness)
    {
    }

    SupportSurface& SupportSurface::operator=(const SupportSurface& other)
    {
        if (this == &other) {
            return *this;
        }

        PhysicsAbstract::operator=(other);
        ConstraintsBase::operator=(other);
        m_surface = other.m_surface->clone();
        m_restitution = other.m_restitution;
        m_frictionCoeff = other.m_frictionCoeff;
        m_penaltyStiffness = other.m_penaltyStiffness;
        return *this;
    }

    std::unique_ptr<PhysicsAbstract> SupportSurface::clone(std::uint64_t id) const
    {
        auto surface = std::make_unique<SupportSurface>(*this);
        surface->setId(id);
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
