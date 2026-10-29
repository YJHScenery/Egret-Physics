/**
 * @file        simple_pendulum.cpp
 * @brief       单摆约束器实现文件
 * @details     实现SimplePendulum类的各项成员函数，
 *              使用单侧位置和速度投影限制绳长。
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
        : PhysicsAbstract(other), ConstraintsBase(other)
          , m_anchor(other.m_anchor)
          , m_entity(other.m_entity)
          , m_length(other.m_length)
    {
        ConstraintsBase::unregisterEntity(other.m_anchor.getId());
        ConstraintsBase::registerEntity(&m_anchor);
    }

    SimplePendulum& SimplePendulum::operator=(const SimplePendulum& other)
    {
        if (this != &other) {
            ConstraintsBase::operator=(other);
            m_anchor = other.m_anchor;
            m_entity = other.m_entity;
            m_length = other.m_length;
            ConstraintsBase::unregisterEntity(other.m_anchor.getId());
            ConstraintsBase::registerEntity(&m_anchor);
        }
        return *this;
    }

    SimplePendulum::SimplePendulum(double length, const Eigen::Vector3d& anchorPos, PhysicalEntity* entity,
                                   std::uint64_t id)
        : PhysicsAbstract(id), ConstraintsBase(id)
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
    // 速度约束
    // ========================================================================
    void SimplePendulum::applyVelocityConstraint([[maybe_unused]] double dt)
    {
        if (m_entity == nullptr || m_entity->isFixed()) {
            return;
        }

        Eigen::Vector3d anchorPos = m_anchor.getPositionCR();
        Eigen::Vector3d entityPos = m_entity->getPositionCR();
        Eigen::Vector3d dp = entityPos - anchorPos;
        double dist = dp.norm();

        if (dist < m_length - 1e-9 || dist < 1e-10) {
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

        // 绷紧时去掉向外的径向速度，不凭空增加切向能量。
        m_entity->setSpeed(vEntity - dv * n);
    }

    // ========================================================================
    // 位置约束
    // ========================================================================

    void SimplePendulum::applyPositionConstraint([[maybe_unused]] double dt)
    {

        if (m_entity == nullptr || m_entity->isFixed()) {
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

        applyVelocityConstraint(dt);
    }

    // ========================================================================
    // 角速度约束（不参与）
    // ========================================================================

    void SimplePendulum::applyAngularVelocityConstraint([[maybe_unused]] double dt)
    {

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
