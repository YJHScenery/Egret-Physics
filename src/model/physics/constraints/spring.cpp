/**
 * @file        spring.cpp
 * @brief       弹簧约束器实现文件
 * @details     实现Spring类的各项成员函数。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-15
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#include "spring.h"
#include "physical_entity.h"

namespace egret
{
    Spring::Spring(const double stiffness, double restLength,
                   PhysicalEntity* entityA, PhysicalEntity* entityB,
                   const std::uint64_t id)
        : ConstraintsBase(id)
        , m_stiffness(stiffness)
        , m_entityA(entityA)
        , m_entityB(entityB)
    {
        ConstraintsBase::registerEntity(entityA);
        ConstraintsBase::registerEntity(entityB);

        // 若未指定原长（≤0），取两实体的初始距离
        if (restLength <= 0.0)
        {
            const Eigen::Vector3d posA = entityA->getPositionCR();
            const Eigen::Vector3d posB = entityB->getPositionCR();
            m_restLength = (posB - posA).norm();
            if (m_restLength < 1e-10)
            {
                m_restLength = 1.0; // 安全回退
            }
        }
        else
        {
            m_restLength = restLength;
        }
    }

    ConstraintType Spring::getType() const
    {
        return ConstraintType::Spring;
    }

    void Spring::applyConstraintForces()
    {

        if (m_entityA == nullptr || m_entityB == nullptr)
        {
            return;
        }

        PhysicalEntity* entityA = m_entityA;
        PhysicalEntity* entityB = m_entityB;


        const Eigen::Vector3d posA = entityA->getPositionCR();
        const Eigen::Vector3d posB = entityB->getPositionCR();

        const Eigen::Vector3d delta = posB - posA;
        const double dist = delta.norm();

        // 重合时方向未定义，不做处理
        if (dist < 1e-10)
        {
            return;
        }

        // 形变量：Δx = dist - restLength
        const double dx = dist - m_restLength;

        // 胡克定律：F = -k · Δx（标量）
        const double forceMagnitude = -m_stiffness * dx;

        // 单位方向向量（A → B）
        const Eigen::Vector3d direction = delta / dist;

        // A 受力 +F·n，B 受力 -F·n（牛顿第三定律）
        const Eigen::Vector3d forceOnA =  -forceMagnitude * direction;
        const Eigen::Vector3d forceOnB = forceMagnitude * direction;

        entityA->upsertForce(Force{forceOnA, m_id});
        entityB->upsertForce(Force{forceOnB, m_id});
    }

    double Spring::computeConstraintError() const
    {
        if (m_entityA == nullptr || m_entityB == nullptr)
        {
            return 0.0;
        }

        const PhysicalEntity* entityA = m_entityA;
        const PhysicalEntity* entityB = m_entityB;


        const double dist = (entityB->getPositionCR() - entityA->getPositionCR()).norm();
        return std::abs(dist - m_restLength);
    }

    std::unique_ptr<PhysicsAbstract> Spring::clone(const std::uint64_t id) const
    {
        auto spring = std::make_unique<Spring>(*this);
        spring->setId(id);
        return spring;
    }
} // egret
