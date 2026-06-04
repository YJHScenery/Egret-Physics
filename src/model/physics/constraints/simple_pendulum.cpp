/**
* @file        simple_pendulum.cpp
 * @brief       单摆约束器实现文件
 * @details     实现SimplePendulum类的各项成员函数。
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

namespace egret
{
    SimplePendulum::SimplePendulum() = default;

    SimplePendulum::SimplePendulum(double length, const Eigen::Vector3d& anchorPos, PhysicalEntity* entity,
                                   std::uint64_t id)
        : ConnectingLine(id) // 锚点质量为0（固定点）
          , m_anchor(anchorPos, Eigen::Vector3d{0, 0, 0}, 0.0, id)
          , m_entity(entity)
    {
        // 将锚点和摆动物体注册到约束器
        ConstraintsBase::registerEntity(&m_anchor);
        ConstraintsBase::registerEntity(entity);
        // 设置约束长度
        m_length = length;
    }

    SimplePendulum::SimplePendulum(double length, const Eigen::Vector3d& anchorPos, PhysicalEntity* entity,
                                   const std::uint64_t id,
                                   const std::initializer_list<Eigen::Vector3d>& turningPositions)
        : ConnectingLine(id)
          , m_anchor(anchorPos, Eigen::Vector3d{0, 0, 0}, 0.0, id) // 锚点质量为0（固定点）
          , m_entity(entity)
    {
        // 将锚点和摆动物体注册到约束器
        ConstraintsBase::registerEntity(&m_anchor);
        ConstraintsBase::registerEntity(entity);
        // 设置约束长度
        m_length = length;
        // 设置路径点
        m_pathPositions = turningPositions;
    }

    void SimplePendulum::setAnchorPos(const Eigen::Vector3d& anchorPos)
    {
        m_anchor.setPosition(anchorPos);
    }

    Eigen::Vector3d SimplePendulum::getAnchorPos() const
    {
        return m_anchor.getPosition();
    }

    void SimplePendulum::applyVelocityConstraint(double dt)
    {
        if (m_entity == nullptr) {
            return;
        }

        // 获取位置
        Eigen::Vector3d anchorPos = m_anchor.getPositionCR();
        Eigen::Vector3d entityPos = m_entity->getPositionCR();

        // 计算方向和距离
        Eigen::Vector3d dp = entityPos - anchorPos;
        double dist = dp.norm();

        // 如果绳子未被拉伸（距离 <= 长度），不激活约束
        if (dist <= m_length || dist < 1e-10) {
            return;
        }

        // 单位方向向量（从锚点指向摆动物体）
        Eigen::Vector3d n = dp / dist;

        // 获取速度
        Eigen::Vector3d vAnchor = m_anchor.getVelocity();
        Eigen::Vector3d vEntity = m_entity->getVelocity();

        // 相对速度在约束方向上的分量
        // Ċ = n · (v_entity - v_anchor)
        double dv = n.dot(vEntity - vAnchor);

        // 获取摆动物体的质量（锚点质量为0，不参与修正）
        double mEntity = m_entity->getMass();

        if (mEntity <= 0) {
            return; // 摆动物体是固定的，无法修正
        }

        // 如果相对速度是收缩方向（dv <= 0），不施加约束
        // 只有当摆动物体正在远离锚点时才施加约束
        if (dv <= 0) {
            return;
        }

        // 能量守恒的速度重定向：将速度中的向外径向分量去除，
        // 同时将去除的能量转移到切向分量上，保持速率不变。
        // 物理上，理想绳约束力始终垂直于速度（不做功），
        // 仅改变速度方向，不改变其大小。
        Eigen::Vector3d vTangent = vEntity - dv * n;
        double tangentNorm = vTangent.norm();
        if (tangentNorm > 1e-10) {
            m_entity->setSpeed(vTangent * (vEntity.norm() / tangentNorm));
        }
    }

    void SimplePendulum::applyPositionConstraint(double dt)
    {
        if (m_entity == nullptr) {
            return;
        }

        // 获取位置
        Eigen::Vector3d anchorPos = m_anchor.getPositionCR();
        Eigen::Vector3d entityPos = m_entity->getPositionCR();

        // 计算方向和距离
        Eigen::Vector3d dp = entityPos - anchorPos;
        double dist = dp.norm();

        // 如果绳子未被拉伸（距离 <= 长度），不激活约束
        if (dist <= m_length || dist < 1e-10) {
            return;
        }

        // 计算需要修正的距离
        double error = dist - m_length;

        // 单位方向向量（从锚点指向摆动物体）
        Eigen::Vector3d n = dp / dist;

        // 修正摆动物体的位置（锚点不能移动）
        Eigen::Vector3d newEntityPos = entityPos - n * error;
        m_entity->setPosition(newEntityPos);

        // 位置修正改变了重力势能，需同步调整动能以保持总能量守恒。
        // ΔPE = m·g·Δp = m·g·(-n·error) = -m·error·(g·n)
        // 令 ΔKE = -ΔPE，即 ½·m·(v'² - v²) = m·error·(g·n)
        // → v'² = v² - 2·error·(a·n)    （a = F/m，对单摆即重力加速度g）
        Eigen::Vector3d vEntity = m_entity->getVelocity();
        double mEntity = m_entity->getMass();
        Eigen::Vector3d acceleration = m_entity->getJoinForce() / mEntity;

        double speedSq = vEntity.squaredNorm();
        double energyComp = -2.0 * error * acceleration.dot(n);
        double targetSpeedSq = speedSq + energyComp;
        if (targetSpeedSq < 0.0) {
            targetSpeedSq = 0.0;
        }
        double targetSpeed = std::sqrt(targetSpeedSq);

        // 重定向速度到切向，使用势能补偿后的目标速率
        double radialV = n.dot(vEntity);
        Eigen::Vector3d vTangent = vEntity - radialV * n;
        double tangentNorm = vTangent.norm();
        if (tangentNorm > 1e-10) {
            m_entity->setSpeed(vTangent * (targetSpeed / tangentNorm));
        }
    }

    std::unique_ptr<PhysicsAbstract> SimplePendulum::clone(const std::uint64_t id) const
    {
        auto pendulum = std::make_unique<SimplePendulum>(*this);
        pendulum->setId(id);
        return pendulum;
    }
} // egret
