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
    SimplePendulum::SimplePendulum() : ConnectingLine(generateID(ConstraintType::SimplePendulum))
    {
    }

    SimplePendulum::SimplePendulum(double length, const Eigen::Vector3d& anchorPos, PhysicalEntity* entity)
        : ConnectingLine(generateID(ConstraintType::SimplePendulum))
          , m_anchor(anchorPos, Eigen::Vector3d{0, 0, 0}, 0.0) // 锚点质量为0（固定点）
          , m_entity(entity)
    {
        // 将锚点和摆动物体加入到约束实体列表
        m_physicalEntities.reserve(2);
        m_physicalEntities.push_back(&m_anchor);
        m_physicalEntities.push_back(entity);
        // 设置约束长度
        m_length = length;
    }

    SimplePendulum::SimplePendulum(double length, const Eigen::Vector3d& anchorPos, PhysicalEntity* entity,
                                   const std::initializer_list<Eigen::Vector3d>& turningPositions)
        : ConnectingLine(generateID(ConstraintType::SimplePendulum))
          , m_anchor(anchorPos, Eigen::Vector3d{0, 0, 0}, 0.0) // 锚点质量为0（固定点）
          , m_entity(entity)
    {
        // 将锚点和摆动物体加入到约束实体列表
        m_physicalEntities.reserve(2);
        m_physicalEntities.push_back(&m_anchor);
        m_physicalEntities.push_back(entity);
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

        // 如果相对速度是收缩方向（dv <= 0），不施加约束
        // 只有当摆动物体正在远离锚点时才施加约束
        if (dv <= 0) {
            return;
        }

        // 获取摆动物体的质量（锚点质量为0，不参与修正）
        double mEntity = m_entity->getMass();

        if (mEntity <= 0) {
            return; // 摆动物体是固定的，无法修正
        }

        // 有效质量就是摆动物体的质量
        double invMeff = 1.0 / mEntity;

        // 计算冲量标量
        // λ = -Ċ / M_eff
        double lambda = -dv / invMeff;

        // 应用冲量：只修正摆动物体
        // Δv = λ * n / m
        Eigen::Vector3d deltaV = lambda * n / mEntity;
        m_entity->setSpeed(vEntity + deltaV);
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
    }
} // egret
