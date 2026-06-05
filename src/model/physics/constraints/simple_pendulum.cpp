/**
 * @file        simple_pendulum.cpp
 * @brief       单摆约束器实现文件
 * @details     实现SimplePendulum类的各项成员函数，
 *              包含自适应能量补偿逻辑。
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

#include "simple_pendulum.h"
#include "physical_entity.h"
#include <cmath>

namespace egret
{
    // ========================================================================
    // 构造 / 拷贝
    // ========================================================================

    SimplePendulum::SimplePendulum() = default;

    SimplePendulum::SimplePendulum(const SimplePendulum& other)
        : ConstraintsBase(other)
          , m_anchor(other.m_anchor)
          , m_entity(other.m_entity)
          , m_length(other.m_length)
    {
        m_hasForceHistory = false;
        m_lastForceDir = Eigen::Vector3d::Zero();
    }

    SimplePendulum& SimplePendulum::operator=(const SimplePendulum& other)
    {
        if (this != &other) {
            ConstraintsBase::operator=(other);
            m_anchor = other.m_anchor;
            m_entity = other.m_entity;
            m_length = other.m_length;
            m_hasForceHistory = false;
            m_lastForceDir = Eigen::Vector3d::Zero();
        }
        return *this;
    }

    SimplePendulum::SimplePendulum(double length, const Eigen::Vector3d& anchorPos, PhysicalEntity* entity,
                                   std::uint64_t id)
        : ConstraintsBase(id)
          , m_anchor(anchorPos, Eigen::Vector3d{0, 0, 0}, 0.0, id)
          , m_entity(entity)
          , m_length(length)
    {
        ConstraintsBase::registerEntity(&m_anchor);
        ConstraintsBase::registerEntity(entity);
    }

    // ========================================================================
    // 锚点操作
    // ========================================================================
    void SimplePendulum::setAnchorPos(const Eigen::Vector3d& anchorPos)
    {
        m_anchor.setPosition(anchorPos);
    }

    Eigen::Vector3d SimplePendulum::getAnchorPos() const
    {
        return m_anchor.getPosition();
    }

    // ========================================================================
    // 约束类型
    // ========================================================================
    ConstraintType SimplePendulum::getType() const
    {
        return ConstraintType::SimplePendulum;
    }

    // ========================================================================
    // 速度约束（能量守恒重定向）
    // ========================================================================
    void SimplePendulum::applyVelocityConstraint(double dt)
    {
        std::ignore = dt;

        if (m_entity == nullptr) {
            return;
        }

        Eigen::Vector3d anchorPos = m_anchor.getPositionCR();
        Eigen::Vector3d entityPos = m_entity->getPositionCR();
        Eigen::Vector3d dp = entityPos - anchorPos;
        double dist = dp.norm();

        if (dist <= m_length || dist < 1e-10) {
            return;
        }

        Eigen::Vector3d n = dp / dist;
        Eigen::Vector3d vAnchor = m_anchor.getVelocity();
        Eigen::Vector3d vEntity = m_entity->getVelocity();
        double dv = n.dot(vEntity - vAnchor);

        double mEntity = m_entity->getMass();
        if (mEntity <= 0) {
            return;
        }

        // 仅当物体向外运动时激活
        if (dv <= 0) {
            return;
        }

        // 能量守恒速度重定向：去除向外径向分量，保持速率不变
        Eigen::Vector3d vTangent = vEntity - dv * n;
        double tangentNorm = vTangent.norm();
        if (tangentNorm > 1e-10) {
            m_entity->setSpeed(vTangent * (vEntity.norm() / tangentNorm));
        }
    }

    // ========================================================================
    // 位置约束（含自适应能量补偿）
    // ========================================================================

    void SimplePendulum::applyPositionConstraint(double dt)
    {
        std::ignore = dt;

        if (m_entity == nullptr) {
            return;
        }

        Eigen::Vector3d anchorPos = m_anchor.getPositionCR();
        Eigen::Vector3d entityPos = m_entity->getPositionCR();
        Eigen::Vector3d dp = entityPos - anchorPos;
        double dist = dp.norm();

        if (dist <= m_length || dist < 1e-10) {
            return;
        }

        double error = dist - m_length;
        Eigen::Vector3d n = dp / dist;

        // 修正位置
        m_entity->setPosition(entityPos - n * error);

        // 自适应能量补偿
        Eigen::Vector3d vEntity = m_entity->getVelocity();
        m_entity->setSpeed(compensateEnergy(vEntity, n, error));
    }

    // ========================================================================
    // 自适应能量补偿（内联实现）
    // ========================================================================

    Eigen::Vector3d SimplePendulum::compensateEnergy(
        const Eigen::Vector3d& currentV,
        const Eigen::Vector3d& correctionN,
        double error) const
    {
        const double mEntity = m_entity->getMass();
        if (mEntity <= 0.0) {
            return currentV;
        }

        // ---- 检测合力方向稳定性 ----
        const Eigen::Vector3d currentForce = m_entity->getJoinForce();
        const double forceNorm = currentForce.norm();

        bool useConservative = false;

        if (forceNorm > 1e-10) {
            const Eigen::Vector3d forceDir = currentForce / forceNorm;

            if (!m_hasForceHistory) {
                m_hasForceHistory = true;
                m_lastForceDir = forceDir;
                useConservative = true;
            } else {
                const double similarity = m_lastForceDir.dot(forceDir);
                m_lastForceDir = forceDir;
                useConservative = (similarity >= kForceStabilityThreshold);
            }
        }
        // 无力作用：不补偿势能，仅做动能守恒重定向

        // ---- 计算目标速率 ----
        double targetSpeedSq = currentV.squaredNorm();

        if (useConservative) {
            // 势能补偿：v'² = v² - 2·error·(a·n)
            const Eigen::Vector3d acceleration = currentForce / mEntity;
            targetSpeedSq += -2.0 * error * acceleration.dot(correctionN);
            if (targetSpeedSq < 0.0) {
                targetSpeedSq = 0.0;
            }
        }
        // 否则保持 |v| 不变（targetSpeedSq 不变）

        const double targetSpeed = std::sqrt(targetSpeedSq);

        // ---- 重定向到切向 ----
        const double radialV = correctionN.dot(currentV);
        const Eigen::Vector3d vTangent = currentV - radialV * correctionN;
        const double tangentNorm = vTangent.norm();

        if (tangentNorm < 1e-10) {
            return Eigen::Vector3d::Zero();
        }

        return vTangent * (targetSpeed / tangentNorm);
    }

    // ========================================================================
    // 角速度约束（不参与）
    // ========================================================================

    void SimplePendulum::applyAngularVelocityConstraint(double dt)
    {
        std::ignore = dt;
    }

    // ========================================================================
    // 约束误差计算
    // ========================================================================

    double SimplePendulum::computeConstraintError() const
    {
        if (m_entity == nullptr) {
            return 0.0;
        }

        const double dist = (m_entity->getPositionCR() - m_anchor.getPositionCR()).norm();
        return std::max(0.0, dist - m_length);
    }

    // ========================================================================
    // 克隆
    // ========================================================================

    std::unique_ptr<PhysicsAbstract> SimplePendulum::clone(const std::uint64_t id) const
    {
        auto pendulum = std::make_unique<SimplePendulum>(*this);
        pendulum->setId(id);
        return pendulum;
    }
} // egret
