/**
 * @file        connecting_rod.cpp
 * @brief       约束杆实现文件，实现两个物理实体间的刚性距离约束。
 * @details     实现 ConnectingRod 类的各项成员函数，包括双向速度约束、
 *              双向位置约束和约束误差计算。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-18
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#include "connecting_rod.h"
#include "physical_entity.h"

namespace egret
{
    ConnectingRod::ConnectingRod()
        : ConstraintsBase()
    {
    }

    ConnectingRod::ConnectingRod(const std::uint64_t id)
        : ConstraintsBase(id)
    {
    }

    ConnectingRod::ConnectingRod(const double length, PhysicalEntity* entityA, PhysicalEntity* entityB,
                                 const std::uint64_t id)
        : ConstraintsBase(id)
        , m_length(length)
        , m_entityA(entityA)
        , m_entityB(entityB)
    {
        ConstraintsBase::registerEntity(entityA);
        ConstraintsBase::registerEntity(entityB);
    }

    ConstraintType ConnectingRod::getType() const
    {
        return ConstraintType::ConnectingRod;
    }

    void ConnectingRod::applyVelocityConstraint([[maybe_unused]] const double dt)
    {
        if (m_entityA == nullptr || m_entityB == nullptr) {
            return;
        }

        const Eigen::Vector3d p0 = m_entityA->getPositionCR();
        const Eigen::Vector3d p1 = m_entityB->getPositionCR();
        const Eigen::Vector3d dp = p1 - p0;
        const double dist = dp.norm();

        // 防止除零
        if (dist < 1e-10) {
            return;
        }

        // 单位方向向量（从 A 指向 B）
        const Eigen::Vector3d n = dp / dist;

        // 获取速度
        const Eigen::Vector3d v0 = m_entityA->getVelocity();
        const Eigen::Vector3d v1 = m_entityB->getVelocity();

        // 相对速度沿杆方向的分量
        // 约束方程：C = ||p₁-p₀|| - L = 0
        // 速度约束：Ċ = n · (v₁ - v₀) = 0
        const double vRel = n.dot(v1 - v0);

        // 如果相对速度几乎为零，无需修正
        if (std::abs(vRel) < 1e-12) {
            return;
        }

        // 计算有效质量
        const double m0 = m_entityA->getMass();
        const double m1 = m_entityB->getMass();

        // 两个都是固定物体，无法修正
        if (m0 <= 0 && m1 <= 0) {
            return;
        }

        const double invM0 = (m0 > 0) ? 1.0 / m0 : 0.0;
        const double invM1 = (m1 > 0) ? 1.0 / m1 : 0.0;
        const double invMeff = invM0 + invM1;

        if (invMeff < 1e-10) {
            return;
        }

        // 双向冲量：与 ConnectingLine 的关键区别——
        // 无论是拉开（vRel > 0）还是压拢（vRel < 0），都施加冲量
        // λ = -vRel / M_eff
        const double lambda = -vRel / invMeff;

        // 应用冲量
        if (m0 > 0) {
            m_entityA->setSpeed(v0 - lambda * n * invM0);
        }
        if (m1 > 0) {
            m_entityB->setSpeed(v1 + lambda * n * invM1);
        }
    }

    void ConnectingRod::applyPositionConstraint([[maybe_unused]] const double dt)
    {
        if (m_entityA == nullptr || m_entityB == nullptr) {
            return;
        }

        const Eigen::Vector3d p0 = m_entityA->getPositionCR();
        const Eigen::Vector3d p1 = m_entityB->getPositionCR();
        const Eigen::Vector3d dp = p1 - p0;
        const double dist = dp.norm();

        // 防止除零
        if (dist < 1e-10) {
            return;
        }

        // 单位方向向量
        const Eigen::Vector3d n = dp / dist;

        // 计算位置误差：正值表示距离过大（需拉回），负值表示距离过小（需推开）
        const double error = dist - m_length;

        // 误差足够小，无需修正
        if (std::abs(error) < 1e-10) {
            return;
        }

        const double m0 = m_entityA->getMass();
        const double m1 = m_entityB->getMass();

        // 两个都是固定物体，无法修正
        if (m0 <= 0 && m1 <= 0) {
            return;
        }

        // 按质量比例分配修正量
        const double totalMass = m0 + m1;

        // error > 0（距离太大）：两实体向中间靠拢
        // error < 0（距离太小）：两实体向两侧推开
        // 两种情况下，修正公式统一
        const double corrA = (m1 > 0 && totalMass > 0) ? error * m1 / totalMass : 0.0;
        const double corrB = (m0 > 0 && totalMass > 0) ? error * m0 / totalMass : 0.0;

        // 应用位置修正
        if (m0 > 0) {
            m_entityA->setPosition(p0 + n * corrA);
        }
        if (m1 > 0) {
            m_entityB->setPosition(p1 - n * corrB);
        }
    }

    void ConnectingRod::applyAngularVelocityConstraint([[maybe_unused]] const double dt)
    {
        // 当前实现仅处理 COM 到 COM 的质点约束，
        // 角动量传递（刚体连接点偏移导致的角速度约束）将在后续版本中扩展。
    }

    double ConnectingRod::computeConstraintError() const
    {
        if (m_entityA == nullptr || m_entityB == nullptr) {
            return 0.0;
        }

        const Eigen::Vector3d dp = m_entityB->getPositionCR() - m_entityA->getPositionCR();
        const double dist = dp.norm();
        return std::abs(dist - m_length);
    }

    std::unique_ptr<PhysicsAbstract> ConnectingRod::clone(const std::uint64_t id) const
    {
        auto constraint = std::make_unique<ConnectingRod>(*this);
        constraint->setId(id);
        return constraint;
    }
} // egret
