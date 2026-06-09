/**
* @file        connecting_line.cpp
 * @brief       连接线约束器实现文件
 * @details     实现ConnectingLine类的各项成员函数。
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

#include "connecting_line.h"
#include "physical_entity.h"

namespace egret
{
    ConnectingLine::ConnectingLine() = default;

    ConnectingLine::ConnectingLine(const std::uint64_t id) : ConstraintsBase(id)
    {
    }

    ConnectingLine::ConnectingLine(double length, PhysicalEntity* entityStart, PhysicalEntity* entityEnd, const std::uint64_t id)
        : ConstraintsBase(id), m_length(length)
          , m_entityA(entityStart)
          , m_entityB(entityEnd)
    {
        ConstraintsBase::registerEntity(entityStart);
        ConstraintsBase::registerEntity(entityEnd);
    }

    ConstraintType ConnectingLine::getType() const
    {
        return ConstraintType::ConnectingLine;
    }

    void ConnectingLine::applyVelocityConstraint([[maybe_unused]] double dt)
    {
        if (m_entityA == nullptr || m_entityB == nullptr) {
            return;
        }

        Eigen::Vector3d p0 = m_entityA->getPositionCR();
        Eigen::Vector3d p1 = m_entityB->getPositionCR();
        Eigen::Vector3d dp = p1 - p0;
        double dist = dp.norm();

        if (dist <= m_length || dist < 1e-10) {
            return;
        }

        Eigen::Vector3d n = dp / dist;
        Eigen::Vector3d v0 = m_entityA->getVelocity();
        Eigen::Vector3d v1 = m_entityB->getVelocity();
        double dv = n.dot(v1 - v0);

        double m0 = m_entityA->getMass();
        double m1 = m_entityB->getMass();

        if (m0 <= 0 && m1 <= 0) {
            return;
        }

        if (dv <= 0) {
            return;
        }

        // 能量守恒的速度重定向：消除相对速度的径向分量，
        // 同时保持相对速度大小不变，将能量重新分配到切向。
        {
            Eigen::Vector3d vRel = v1 - v0;
            Eigen::Vector3d vRelTangent = vRel - dv * n;
            double tangentNorm = vRelTangent.norm();
            if (tangentNorm > 1e-10) {
                Eigen::Vector3d vRelNew = vRelTangent * (vRel.norm() / tangentNorm);
                Eigen::Vector3d deltaVRel = vRelNew - vRel;
                double totalMass = m0 + m1;
                if (m0 > 0) {
                    m_entityA->setSpeed(v0 - deltaVRel * (m1 / totalMass));
                }
                if (m1 > 0) {
                    m_entityB->setSpeed(v1 + deltaVRel * (m0 / totalMass));
                }
            }
        }
    }

    void ConnectingLine::applyPositionConstraint([[maybe_unused]] double dt)
    {
        if (m_entityA == nullptr || m_entityB == nullptr) {
            return;
        }

        Eigen::Vector3d p0 = m_entityA->getPositionCR();
        Eigen::Vector3d p1 = m_entityB->getPositionCR();
        double dist = (p1 - p0).norm();

        if (dist <= m_length || dist < 1e-10) {
            return;
        }

        double error = dist - m_length;
        Eigen::Vector3d direction = (p1 - p0) / dist;

        double m0 = m_entityA->getMass();
        double m1 = m_entityB->getMass();

        if (m0 <= 0 && m1 <= 0) {
            return;
        }

        double corr0 = (m1 > 0) ? error * m1 / (m0 + m1) : 0.0;
        double corr1 = (m0 > 0) ? error * m0 / (m0 + m1) : 0.0;

        m_entityA->setPosition(p0 + direction * corr0);
        m_entityB->setPosition(p1 - direction * corr1);

        // 能量守恒的速度重定向：消除相对径向分量，保持速率不变。
        // 两体位置修正为质心保持，总势能不变，仅需重定向相对速度。
        {
            Eigen::Vector3d v0 = m_entityA->getVelocity();
            Eigen::Vector3d v1 = m_entityB->getVelocity();
            Eigen::Vector3d vRel = v1 - v0;
            double dvRel = direction.dot(vRel);
            if (dvRel > 0) {
                Eigen::Vector3d vRelTangent = vRel - dvRel * direction;
                double tangentNorm = vRelTangent.norm();
                if (tangentNorm > 1e-10) {
                    Eigen::Vector3d vRelNew = vRelTangent * (vRel.norm() / tangentNorm);
                    Eigen::Vector3d deltaVRel = vRelNew - vRel;
                    double totalMass = m0 + m1;
                    if (m0 > 0) {
                        m_entityA->setSpeed(v0 - deltaVRel * (m1 / totalMass));
                    }
                    if (m1 > 0) {
                        m_entityB->setSpeed(v1 + deltaVRel * (m0 / totalMass));
                    }
                }
            }
        }
    }

    double ConnectingLine::computeConstraintError() const
    {
        if (m_entityA == nullptr || m_entityB == nullptr) {
            return 0.0;
        }

        const double dist = (m_entityB->getPositionCR() - m_entityA->getPositionCR()).norm();
        return std::max(0.0, dist - m_length);
    }

    void ConnectingLine::applyAngularVelocityConstraint([[maybe_unused]] double dt)
    {
        // 不参与角速度变化。
    }

    std::unique_ptr<PhysicsAbstract> ConnectingLine::clone(const std::uint64_t id) const
    {
        auto constraint = std::make_unique<ConnectingLine>(*this);
        constraint->setId(id);
        return constraint;
    }

    ConnectingLine::ConnectingLine(const ConnectingLine& other)
    {
        m_entityA = other.m_entityA;
        m_entityB = other.m_entityB;
        m_length = other.m_length;
    }

    ConnectingLine& ConnectingLine::operator=(const ConnectingLine& other)
    {
        if (this != &other) {
            m_entityA = other.m_entityA;
            m_entityB = other.m_entityB;
            m_length = other.m_length;
        }
        return *this;
    }
} // egret
