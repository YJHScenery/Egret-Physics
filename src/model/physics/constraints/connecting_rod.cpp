/**
 * @file        connecting_rod.cpp
 * @brief       连接杆约束器实现文件
 * @details     实现 ConnectingRod 类的各项成员函数，
 *              包含端点世界位置递推解析、刚性距离约束、
 *              杆间角度约束及连接管理。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-07-15
 * @version     2.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#include "connecting_rod.h"

#include <algorithm>
#include <cmath>

#include "physical_entity.h"

namespace egret
{

    // ========================================================================
    //  内部常量
    // ========================================================================

    namespace
    {
        /** 锚点重合判定阈值（m）。 */
        constexpr double kAnchorCoincidenceThreshold = 1e-10;

        /** 转动惯量行列式最小绝对值。 */
        constexpr double kInertiaDetThreshold = 1e-20;

        /** 角度约束误差容限。 */
        constexpr double kAngleErrorTolerance = 1e-6;

        /** 杆间角度约束的角速度修正增益。 */
        constexpr double kAngularConstraintGain = 0.3;

        /** 端点位置递推解析的最大深度（防止循环引用）。 */
        constexpr int kMaxResolveDepth = 16;

        /**
         * @brief 对单个物体施加角速度修正。
         * @param body 目标物体。
         * @param angularCorrection 角冲量（世界空间）。
         */
        void applyAngularCorrection(PhysicalEntity* body,
                                    const Eigen::Vector3d& angularCorrection)
        {
            if (body == nullptr || body->isFixed())
            {
                return;
            }

            const double mass = body->getMass();
            if (mass <= 0.0)
            {
                return;
            }

            auto shape = body->getShape();
            if (shape == nullptr)
            {
                return;
            }

            const Eigen::Matrix3d I_local = shape->getInertiaTensor(mass);
            const Eigen::Matrix3d R_mat = body->getTransformCR().getRotationMatrix();
            const Eigen::Matrix3d I_world = R_mat * I_local * R_mat.transpose();

            const double det = I_world.determinant();
            if (std::abs(det) < kInertiaDetThreshold)
            {
                return;
            }

            const Eigen::Matrix3d I_inv = I_world.inverse();
            const Eigen::Vector3d deltaOmega = I_inv * angularCorrection;
            const Eigen::Vector3d omega = body->getAngularVelocity();

            body->setAngular(omega + deltaOmega);
        }

        /**
         * @brief resolveEndpointWorldPos 的递归实现，带深度守卫。
         */
        bool resolveEndpointWorldPosImpl(const Endpoint& end, Eigen::Vector3d& outPos, int depth)
        {
            if (depth > kMaxResolveDepth)
            {
                return false;
            }

            if (end.isEntity())
            {
                if (end.entity == nullptr)
                {
                    return false;
                }
                outPos = end.entity->getTransformCR().localToWorld(end.localAnchor);
                return true;
            }

            // Rod 类型：优先使用缓存（打破双向递归）
            if (end.hasCachedPos)
            {
                outPos = end.cachedWorldPos;
                return true;
            }

            // 无缓存则递推解析
            if (end.connectedRod == nullptr)
            {
                return false;
            }

            const int otherEndIdx = end.connectedRodEndIndex;
            const Endpoint& otherEnd = end.connectedRod->endpointAt(otherEndIdx);

            return resolveEndpointWorldPosImpl(otherEnd, outPos, depth + 1);
        }

        /**
         * @brief resolveEndpointEntity 的递归实现，带深度守卫。
         */
        PhysicalEntity* resolveEndpointEntityImpl(const Endpoint& end, int depth)
        {
            if (depth > kMaxResolveDepth)
            {
                return nullptr;
            }

            if (end.isEntity())
            {
                return end.entity;
            }

            // Rod 类型：有缓存说明是纯杆间连接，枢轴无实体
            if (end.hasCachedPos)
            {
                return nullptr;
            }

            if (end.connectedRod == nullptr)
            {
                return nullptr;
            }

            const int otherEndIdx = end.connectedRodEndIndex;
            const Endpoint& otherEnd = end.connectedRod->endpointAt(otherEndIdx);

            return resolveEndpointEntityImpl(otherEnd, depth + 1);
        }

    }

    // ========================================================================
    //  静态工具方法
    // ========================================================================

    bool ConnectingRod::resolveEndpointWorldPos(const Endpoint& end, Eigen::Vector3d& outPos)
    {
        return resolveEndpointWorldPosImpl(end, outPos, 0);
    }



    PhysicalEntity* ConnectingRod::resolveEndpointEntity(const Endpoint& end)
    {
        return resolveEndpointEntityImpl(end, 0);
    }


    // ========================================================================
    //  构造与析构
    // ========================================================================

    ConnectingRod::ConnectingRod()
        : ConstraintsBase()
    {
    }

    ConnectingRod::ConnectingRod(const std::uint64_t id)
        : ConstraintsBase(id)
    {
    }

    ConnectingRod::ConnectingRod(const Endpoint& endA, const Endpoint& endB,
                                 const std::uint64_t id)
        : ConstraintsBase(id)
        , m_endA(endA)
        , m_endB(endB)
    {
        // 递推解析两端的世界位置以计算杆长
        Eigen::Vector3d worldA, worldB;
        if (resolveEndpointWorldPos(m_endA, worldA) &&
            resolveEndpointWorldPos(m_endB, worldB))
        {
            m_length = (worldB - worldA).norm();
            if (m_length < kAnchorCoincidenceThreshold)
            {
                m_length = 1.0;
            }
        }
        else
        {
            m_length = 1.0;
        }

        // 注册 Entity 类型端点对应的实体
        PhysicalEntity* entityA = resolveEndpointEntity(m_endA);
        PhysicalEntity* entityB = resolveEndpointEntity(m_endB);
        if (entityA != nullptr)
        {
            registerEntity(entityA);
        }
        if (entityB != nullptr && entityB != entityA)
        {
            registerEntity(entityB);
        }
    }

    ConnectingRod::ConnectingRod(const Endpoint& endA, const Endpoint& endB,
                                 const double length, const std::uint64_t id)
        : ConstraintsBase(id)
        , m_endA(endA)
        , m_endB(endB)
        , m_length(length > 0.0 ? length : 1.0)
    {
        PhysicalEntity* entityA = resolveEndpointEntity(m_endA);
        PhysicalEntity* entityB = resolveEndpointEntity(m_endB);
        if (entityA != nullptr)
        {
            registerEntity(entityA);
        }
        if (entityB != nullptr && entityB != entityA)
        {
            registerEntity(entityB);
        }
    }

    // ========================================================================
    //  约束接口
    // ========================================================================

    ConstraintType ConnectingRod::getType() const
    {
        return ConstraintType::ConnectingRod;
    }

    void ConnectingRod::applyVelocityConstraint(const double dt)
    {
        if (!m_enabled)
        {
            return;
        }

        // ---- 第一步：解析两端点世界位置 ----

        Eigen::Vector3d worldA, worldB;
        if (!resolveEndpointWorldPos(m_endA, worldA) ||
            !resolveEndpointWorldPos(m_endB, worldB))
        {
            return; // 端点解析失败（链断裂）
        }

        // ---- 第二步：刚性距离约束 ----

        const Eigen::Vector3d d = worldB - worldA;
        const double dist = d.norm();

        if (dist < kAnchorCoincidenceThreshold)
        {
            // 距离为零时不施加距离约束，但仍可处理角度约束
        }
        else
        {
            const Eigen::Vector3d n = d / dist;

            // 解析两端最终绑定的实体
            PhysicalEntity* bodyA = resolveEndpointEntity(m_endA);
            PhysicalEntity* bodyB = resolveEndpointEntity(m_endB);

            // Baumgarte 稳定化（共用）
            const double positionError = dist - m_length;
            double cDotBias = 0.0;
            if (dt > kAnchorCoincidenceThreshold && m_baumgarteBeta > 0.0)
            {
                cDotBias = (m_baumgarteBeta / dt) * positionError;
            }

            if (bodyA != nullptr && bodyB != nullptr)
            {
                // 标准双实体情况
                const Eigen::Vector3d posA = bodyA->getPositionCR();
                const Eigen::Vector3d posB = bodyB->getPositionCR();
                const Eigen::Vector3d rA = worldA - posA;
                const Eigen::Vector3d rB = worldB - posB;

                const Eigen::Vector3d vA = bodyA->getVelocity();
                const Eigen::Vector3d vB = bodyB->getVelocity();
                const Eigen::Vector3d omegaA = bodyA->getAngularVelocity();
                const Eigen::Vector3d omegaB = bodyB->getAngularVelocity();

                const double cDot = n.dot(vB - vA)
                                  + (rB.cross(n)).dot(omegaB)
                                  - (rA.cross(n)).dot(omegaA);

                const double effectiveMassInv =
                    computeEffectiveMassInv(n, rA, rB, bodyA, bodyB);

                if (effectiveMassInv >= kAnchorCoincidenceThreshold)
                {
                    const double lambda = -(cDot + cDotBias) / effectiveMassInv;
                    const Eigen::Vector3d impulse = lambda * n;
                    applyImpulseToBody(bodyA, -impulse, rA);
                    applyImpulseToBody(bodyB,  impulse, rB);
                }
            }
            else if (bodyA != nullptr)
            {
                // 仅 A 端有实体，B 端为缓存虚拟枢轴（纯链式连接）
                const double mA = bodyA->getMass();
                if (mA > 0.0)
                {
                    const Eigen::Vector3d posA = bodyA->getPositionCR();
                    const Eigen::Vector3d rA = worldA - posA;
                    const Eigen::Vector3d vA = bodyA->getVelocity();
                    const Eigen::Vector3d omegaA = bodyA->getAngularVelocity();

                    // Ċ = -n · vA - (rA × n) · ωA（枢轴不动）
                    const double cDot = -n.dot(vA) - (rA.cross(n)).dot(omegaA);

                    // 有效质量倒数 = 1/mA + 转动项
                    double invM = 1.0 / mA;
                    if (auto shape = bodyA->getShape())
                    {
                        const Eigen::Matrix3d I_local = shape->getInertiaTensor(mA);
                        const Eigen::Matrix3d R_mat = bodyA->getTransformCR().getRotationMatrix();
                        const Eigen::Matrix3d I_world = R_mat * I_local * R_mat.transpose();
                        if (std::abs(I_world.determinant()) >= kInertiaDetThreshold)
                        {
                            const Eigen::Vector3d rcn = rA.cross(n);
                            invM += rcn.dot(I_world.inverse() * rcn);
                        }
                    }

                    const double lambda = -(cDot + cDotBias) / invM;
                    applyImpulseToBody(bodyA, -lambda * n, rA);
                }
            }
            else if (bodyB != nullptr)
            {
                // 仅 B 端有实体，A 端为缓存虚拟枢轴
                const double mB = bodyB->getMass();
                if (mB > 0.0)
                {
                    const Eigen::Vector3d posB = bodyB->getPositionCR();
                    const Eigen::Vector3d rB = worldB - posB;
                    const Eigen::Vector3d vB = bodyB->getVelocity();
                    const Eigen::Vector3d omegaB = bodyB->getAngularVelocity();

                    const double cDot = n.dot(vB) + (rB.cross(n)).dot(omegaB);

                    double invM = 1.0 / mB;
                    if (auto shape = bodyB->getShape())
                    {
                        const Eigen::Matrix3d I_local = shape->getInertiaTensor(mB);
                        const Eigen::Matrix3d R_mat = bodyB->getTransformCR().getRotationMatrix();
                        const Eigen::Matrix3d I_world = R_mat * I_local * R_mat.transpose();
                        if (std::abs(I_world.determinant()) >= kInertiaDetThreshold)
                        {
                            const Eigen::Vector3d rcn = rB.cross(n);
                            invM += rcn.dot(I_world.inverse() * rcn);
                        }
                    }

                    const double lambda = -(cDot + cDotBias) / invM;
                    applyImpulseToBody(bodyB, lambda * n, rB);
                }
            }
        }

        // ---- 第三步：杆间角度约束 ----

        for (const RodConnection& conn : m_connections)
        {
            if (conn.state == RodJointState::Free)
            {
                continue;
            }

            // 去重：仅当 this < otherRod 时施加该对的角度约束
            if (reinterpret_cast<std::uintptr_t>(this) >=
                reinterpret_cast<std::uintptr_t>(conn.otherRod))
            {
                continue;
            }

            if (conn.state == RodJointState::Coplanar ||
                conn.state == RodJointState::FixedAngle)
            {
                applyCoplanarConstraint(conn);
            }

            if (conn.state == RodJointState::FixedAngle)
            {
                applyFixedAngleConstraint(conn);
            }
        }
    }

    void ConnectingRod::applyPositionConstraint(const double dt)
    {
        std::ignore = dt;

        if (!m_enabled)
        {
            return;
        }

        Eigen::Vector3d worldA, worldB;
        if (!resolveEndpointWorldPos(m_endA, worldA) ||
            !resolveEndpointWorldPos(m_endB, worldB))
        {
            return;
        }

        const Eigen::Vector3d d = worldB - worldA;
        const double dist = d.norm();

        if (dist < kAnchorCoincidenceThreshold)
        {
            return;
        }

        const double error = dist - m_length;
        if (std::abs(error) < kAnchorCoincidenceThreshold)
        {
            return;
        }

        const Eigen::Vector3d n = d / dist;

        PhysicalEntity* bodyA = resolveEndpointEntity(m_endA);
        PhysicalEntity* bodyB = resolveEndpointEntity(m_endB);

        if (bodyA == nullptr || bodyB == nullptr)
        {
            return;
        }

        const double mA = bodyA->getMass();
        const double mB = bodyB->getMass();

        if (mA <= 0.0 && mB <= 0.0)
        {
            return;
        }

        const double totalMass = mA + mB;

        double corrA = 0.0;
        double corrB = 0.0;

        if (totalMass > 0.0)
        {
            corrA = error * (mB / totalMass);
            corrB = error * (mA / totalMass);
        }
        else
        {
            return;
        }

        // 修正 COM 位置
        if (mA > 0.0)
        {
            bodyA->setPosition(bodyA->getPositionCR() + n * corrA);
        }
        if (mB > 0.0)
        {
            bodyB->setPosition(bodyB->getPositionCR() - n * corrB);
        }
    }

    void ConnectingRod::applyAngularVelocityConstraint(const double dt)
    {
        std::ignore = dt;
        // 角度约束已在 applyVelocityConstraint 中联合求解。
        // 当前求解器不调用此方法，保留空实现以符合接口约定。
    }

    double ConnectingRod::computeConstraintError() const
    {
        Eigen::Vector3d worldA, worldB;
        if (!resolveEndpointWorldPos(m_endA, worldA) ||
            !resolveEndpointWorldPos(m_endB, worldB))
        {
            return 0.0;
        }

        return std::abs((worldB - worldA).norm() - m_length);
    }

    std::unique_ptr<PhysicsAbstract> ConnectingRod::clone(const std::uint64_t id) const
    {
        auto rod = std::make_unique<ConnectingRod>(*this);
        rod->setId(id);

        // 清除所有外部引用 —— 连接关系需在新约束体系中重建
        rod->m_connections.clear();

        // 将 Rod 类型的端点重置为无效状态
        auto resetRodEndpoint = [](Endpoint& ep)
        {
            if (ep.isRod())
            {
                ep.type = EndpointType::Entity;
                ep.entity = nullptr;
                ep.localAnchor = Eigen::Vector3d::Zero();
                ep.connectedRod = nullptr;
                ep.connectedRodEndIndex = 0;
            }
        };
        resetRodEndpoint(rod->m_endA);
        resetRodEndpoint(rod->m_endB);

        return rod;
    }

    // ========================================================================
    //  端点与连接管理
    // ========================================================================

    void ConnectingRod::setEndpoint(const int endIndex, const Endpoint& endpoint)
    {
        Endpoint& ep = endpointAt(endIndex);

        // 如果之前是 Rod 类型，需要清理相关连接
        if (ep.isRod())
        {
            removeConnectionsForEnd(endIndex);
        }

        ep = endpoint;

        // 注册新端点对应的实体
        if (ep.isEntity() && ep.entity != nullptr)
        {
            registerEntity(ep.entity);
        }
    }

    PhysicalEntity* ConnectingRod::getBody(const int endIndex) const
    {
        return resolveEndpointEntity(endpointAt(endIndex));
    }

    void ConnectingRod::connectEndToRod(const int myEndIndex,
                                        ConnectingRod* const otherRod,
                                        const int otherEndIndex,
                                        const RodJointState state)
    {
        if (otherRod == nullptr || otherRod == this)
        {
            return;
        }

        Endpoint& myEnd = endpointAt(myEndIndex);
        Endpoint& otherEnd = otherRod->endpointAt(otherEndIndex);

        // 如果任一端点已有 Rod 连接，先断开（双向清理）
        if (myEnd.isRod())
        {
            disconnectEnd(myEndIndex);
        }
        if (otherEnd.isRod())
        {
            otherRod->disconnectEnd(otherEndIndex);
        }

        // ---- 计算当前 world 位置用于缓存 ----
        Eigen::Vector3d myCurWorld, otherCurWorld;
        const bool myOk = resolveEndpointWorldPosImpl(
            endpointAt(myEndIndex), myCurWorld, 0);
        const bool otherOk = resolveEndpointWorldPosImpl(
            otherRod->endpointAt(otherEndIndex), otherCurWorld, 0);

        // 取可解析的位置，或回退到中点
        Eigen::Vector3d pivotPos;
        if (myOk && otherOk)
        {
            pivotPos = 0.5 * (myCurWorld + otherCurWorld);
        }
        else if (myOk)
        {
            pivotPos = myCurWorld;
        }
        else if (otherOk)
        {
            pivotPos = otherCurWorld;
        }

        // ---- 双向设置端点 ----

        // 本端点 → 另一杆
        myEnd.type = EndpointType::Rod;
        myEnd.connectedRod = otherRod;
        myEnd.connectedRodEndIndex = otherEndIndex;
        myEnd.entity = nullptr;
        myEnd.localAnchor = Eigen::Vector3d::Zero();
        myEnd.cachedWorldPos = pivotPos;
        myEnd.hasCachedPos = true;

        // 另一杆端点 → 本杆
        otherEnd.type = EndpointType::Rod;
        otherEnd.connectedRod = this;
        otherEnd.connectedRodEndIndex = myEndIndex;
        otherEnd.entity = nullptr;
        otherEnd.localAnchor = Eigen::Vector3d::Zero();
        otherEnd.cachedWorldPos = pivotPos;
        otherEnd.hasCachedPos = true;

        // ---- 创建连接记录（仅本杆侧，去重在 apply 时处理） ----
        RodConnection conn;
        conn.otherRod = otherRod;
        conn.myEndIndex = myEndIndex;
        conn.otherEndIndex = otherEndIndex;
        conn.state = state;

        // 临时设置 state 用于捕获参考值
        const RodJointState savedState = conn.state;
        conn.state = state;
        if (state != RodJointState::Free)
        {
            captureConnectionRefValues(conn);
        }
        conn.state = savedState;

        m_connections.push_back(conn);
    }

    void ConnectingRod::disconnectEnd(const int endIndex)
    {
        Endpoint& ep = endpointAt(endIndex);

        if (!ep.isRod())
        {
            return;
        }

        // 双向清理：同时断开另一杆对应端点的连接
        if (ep.connectedRod != nullptr)
        {
            Endpoint& otherEp = ep.connectedRod->endpointAt(ep.connectedRodEndIndex);
            if (otherEp.isRod() && otherEp.connectedRod == this)
            {
                otherEp.type = EndpointType::Entity;
                otherEp.entity = nullptr;
                otherEp.localAnchor = Eigen::Vector3d::Zero();
                otherEp.connectedRod = nullptr;
                otherEp.connectedRodEndIndex = 0;
                otherEp.hasCachedPos = false;
                ep.connectedRod->removeConnectionsForEnd(ep.connectedRodEndIndex);
            }
        }

        // 清理本杆连接记录
        removeConnectionsForEnd(endIndex);

        // 重置端点为无效 Entity 状态
        ep.type = EndpointType::Entity;
        ep.entity = nullptr;
        ep.localAnchor = Eigen::Vector3d::Zero();
        ep.connectedRod = nullptr;
        ep.connectedRodEndIndex = 0;
        ep.hasCachedPos = false;
    }

    void ConnectingRod::setJointState(const int connIndex, const RodJointState state)
    {
        if (connIndex < 0 || connIndex >= static_cast<int>(m_connections.size()))
        {
            return;
        }

        RodConnection& conn = m_connections[connIndex];
        conn.state = state;

        if (state != RodJointState::Free)
        {
            captureConnectionRefValues(conn);
        }
    }

    RodJointState ConnectingRod::getJointState(const int connIndex) const
    {
        if (connIndex < 0 || connIndex >= static_cast<int>(m_connections.size()))
        {
            return RodJointState::Free;
        }
        return m_connections[connIndex].state;
    }

    // ========================================================================
    //  私有：端点访问
    // ========================================================================

    Endpoint& ConnectingRod::endpointAt(const int index)
    {
        return (index == 0) ? m_endA : m_endB;
    }

    const Endpoint& ConnectingRod::endpointAt(const int index) const
    {
        return (index == 0) ? m_endA : m_endB;
    }

    void ConnectingRod::removeConnectionsForEnd(const int endIndex)
    {
        m_connections.erase(
            std::remove_if(m_connections.begin(), m_connections.end(),
                           [endIndex](const RodConnection& c)
                           {
                               return c.myEndIndex == endIndex;
                           }),
            m_connections.end());
    }

    // ========================================================================
    //  私有：约束辅助
    // ========================================================================

    double ConnectingRod::computeEffectiveMassInv(const Eigen::Vector3d& n,
                                                   const Eigen::Vector3d& rA,
                                                   const Eigen::Vector3d& rB,
                                                   const PhysicalEntity* bodyA,
                                                   const PhysicalEntity* bodyB)
    {
        double invMass = 0.0;

        const double mA = bodyA->getMass();
        const double mB = bodyB->getMass();

        if (mA > 0.0) { invMass += 1.0 / mA; }
        if (mB > 0.0) { invMass += 1.0 / mB; }

        auto addRotationalTerm = [&](const PhysicalEntity* body, const Eigen::Vector3d& r)
        {
            if (body == nullptr) { return; }
            const double mass = body->getMass();
            if (mass <= 0.0) { return; }

            auto shape = body->getShape();
            if (shape == nullptr) { return; }

            const Eigen::Matrix3d I_local = shape->getInertiaTensor(mass);
            const Eigen::Matrix3d R_mat = body->getTransformCR().getRotationMatrix();
            const Eigen::Matrix3d I_world = R_mat * I_local * R_mat.transpose();

            const double det = I_world.determinant();
            if (std::abs(det) < kInertiaDetThreshold) { return; }

            const Eigen::Matrix3d I_inv = I_world.inverse();
            const Eigen::Vector3d rCrossN = r.cross(n);
            const Eigen::Vector3d IInvCrossN = I_inv * rCrossN;
            invMass += rCrossN.dot(IInvCrossN);
        };

        addRotationalTerm(bodyA, rA);
        addRotationalTerm(bodyB, rB);

        return invMass;
    }

    void ConnectingRod::applyImpulseToBody(PhysicalEntity* const body,
                                           const Eigen::Vector3d& impulse,
                                           const Eigen::Vector3d& r)
    {
        if (body == nullptr || body->isFixed())
        {
            return;
        }

        const double mass = body->getMass();
        if (mass <= 0.0)
        {
            return;
        }

        // 线速度
        const Eigen::Vector3d v = body->getVelocity();
        body->setSpeed(v + impulse / mass);

        // 角速度
        if (r.squaredNorm() > kAnchorCoincidenceThreshold)
        {
            auto shape = body->getShape();
            if (shape == nullptr) { return; }

            const Eigen::Matrix3d I_local = shape->getInertiaTensor(mass);
            const Eigen::Matrix3d R_mat = body->getTransformCR().getRotationMatrix();
            const Eigen::Matrix3d I_world = R_mat * I_local * R_mat.transpose();

            const double det = I_world.determinant();
            if (std::abs(det) < kInertiaDetThreshold) { return; }

            const Eigen::Matrix3d I_inv = I_world.inverse();
            const Eigen::Vector3d torque = r.cross(impulse);
            const Eigen::Vector3d deltaOmega = I_inv * torque;
            const Eigen::Vector3d omega = body->getAngularVelocity();
            body->setAngular(omega + deltaOmega);
        }
    }

    void ConnectingRod::applyAngularCorrection(PhysicalEntity* body, const Eigen::Vector3d& angularCorrection)
    {
        std::ignore = body;
        std::ignore = angularCorrection;
    }

    // ========================================================================
    //  私有：杆间角度约束
    // ========================================================================

    void ConnectingRod::captureConnectionRefValues(RodConnection& conn)
    {
        if (conn.otherRod == nullptr)
        {
            return;
        }

        ConnectingRod* other = conn.otherRod;

        // 获取本杆两个端点的世界位置
        Eigen::Vector3d myEndWorld, myOtherEndWorld;
        if (!resolveEndpointWorldPos(endpointAt(conn.myEndIndex), myEndWorld) ||
            !resolveEndpointWorldPos(endpointAt(1 - conn.myEndIndex), myOtherEndWorld))
        {
            return;
        }

        // 获取另一杆两个端点的世界位置
        Eigen::Vector3d otherEndWorld, otherOtherEndWorld;
        if (!resolveEndpointWorldPos(other->endpointAt(conn.otherEndIndex), otherEndWorld) ||
            !resolveEndpointWorldPos(other->endpointAt(1 - conn.otherEndIndex), otherOtherEndWorld))
        {
            return;
        }

        // 两杆方向（从共享端点指向各自的另一端）
        const Eigen::Vector3d d1 = (myOtherEndWorld - myEndWorld).normalized();
        const Eigen::Vector3d d2 = (otherOtherEndWorld - otherEndWorld).normalized();

        // 共面：捕获参考平面法线
        const Eigen::Vector3d cross = d1.cross(d2);
        const double crossNorm = cross.norm();
        if (crossNorm > kAngleErrorTolerance)
        {
            conn.refPlaneNormal = cross / crossNorm;
        }
        else
        {
            // 两杆平行，取任意垂直于 d1 的方向
            Eigen::Vector3d arbitrary = (std::abs(d1.x()) < 0.9)
                ? Eigen::Vector3d::UnitX()
                : Eigen::Vector3d::UnitY();
            conn.refPlaneNormal = d1.cross(arbitrary).normalized();
        }

        if (conn.state == RodJointState::FixedAngle)
        {
            conn.refCosAngle = std::clamp(d1.dot(d2), -1.0, 1.0);
        }
    }

    void ConnectingRod::applyCoplanarConstraint(const RodConnection& conn)
    {
        if (conn.otherRod == nullptr)
        {
            return;
        }

        ConnectingRod* other = conn.otherRod;

        // 解析四端点世界位置
        Eigen::Vector3d myPivot, myFar;
        if (!resolveEndpointWorldPos(endpointAt(conn.myEndIndex), myPivot) ||
            !resolveEndpointWorldPos(endpointAt(1 - conn.myEndIndex), myFar))
        {
            return;
        }

        Eigen::Vector3d otherPivot, otherFar;
        if (!resolveEndpointWorldPos(other->endpointAt(conn.otherEndIndex), otherPivot) ||
            !resolveEndpointWorldPos(other->endpointAt(1 - conn.otherEndIndex), otherFar))
        {
            return;
        }

        // 方向（从各自远端指向 pivot，保持与 capture 时一致的语义）
        // 实际上 d1 是从 myPivot 到 myFar 的方向
        const Eigen::Vector3d d1 = (myFar - myPivot).normalized();
        const Eigen::Vector3d d2 = (otherFar - otherPivot).normalized();

        // 当前平面法线
        const Eigen::Vector3d nCurr = d1.cross(d2);
        const double nCurrNorm = nCurr.norm();
        if (nCurrNorm < kAngleErrorTolerance)
        {
            return;
        }
        const Eigen::Vector3d nCurrUnit = nCurr / nCurrNorm;

        // 误差：当前法线中垂直于参考法线的分量
        const double dotVal = std::clamp(nCurrUnit.dot(conn.refPlaneNormal), -1.0, 1.0);
        const double errorMag = std::sqrt(std::max(0.0, 1.0 - dotVal * dotVal));

        if (errorMag < kAngleErrorTolerance)
        {
            return;
        }

        // 旋转轴
        Eigen::Vector3d rotAxis = nCurrUnit.cross(conn.refPlaneNormal);
        const double rotAxisNorm = rotAxis.norm();
        if (rotAxisNorm < kAngleErrorTolerance)
        {
            return;
        }
        rotAxis /= rotAxisNorm;

        const Eigen::Vector3d angularImpulse = kAngularConstraintGain * errorMag * rotAxis;

        // 施加角冲量到两杆远端实体；若 pivot 处有实体也参与
        PhysicalEntity* entityMyFar = resolveEndpointEntity(endpointAt(1 - conn.myEndIndex));
        PhysicalEntity* entityOtherFar = resolveEndpointEntity(other->endpointAt(1 - conn.otherEndIndex));
        PhysicalEntity* entityMyPivot = resolveEndpointEntity(endpointAt(conn.myEndIndex));

        if (entityMyFar != nullptr)
        {
            applyAngularCorrection(entityMyFar, angularImpulse);
        }
        if (entityOtherFar != nullptr)
        {
            applyAngularCorrection(entityOtherFar, angularImpulse);
        }
        if (entityMyPivot != nullptr &&
            entityMyPivot != entityMyFar &&
            entityMyPivot != entityOtherFar)
        {
            applyAngularCorrection(entityMyPivot, -2.0 * angularImpulse);
        }
    }

    void ConnectingRod::applyFixedAngleConstraint(const RodConnection& conn)
    {
        if (conn.otherRod == nullptr)
        {
            return;
        }

        ConnectingRod* other = conn.otherRod;

        // 解析四端点世界位置
        Eigen::Vector3d myPivot, myFar;
        if (!resolveEndpointWorldPos(endpointAt(conn.myEndIndex), myPivot) ||
            !resolveEndpointWorldPos(endpointAt(1 - conn.myEndIndex), myFar))
        {
            return;
        }

        Eigen::Vector3d otherPivot, otherFar;
        if (!resolveEndpointWorldPos(other->endpointAt(conn.otherEndIndex), otherPivot) ||
            !resolveEndpointWorldPos(other->endpointAt(1 - conn.otherEndIndex), otherFar))
        {
            return;
        }

        const Eigen::Vector3d d1 = (myFar - myPivot).normalized();
        const Eigen::Vector3d d2 = (otherFar - otherPivot).normalized();

        const double cosCurrent = std::clamp(d1.dot(d2), -1.0, 1.0);
        const double cosError = cosCurrent - conn.refCosAngle;

        if (std::abs(cosError) < kAngleErrorTolerance)
        {
            return;
        }

        // 旋转轴：垂直于两杆方向
        Eigen::Vector3d rotAxis = d1.cross(d2);
        const double rotAxisNorm = rotAxis.norm();
        if (rotAxisNorm < kAngleErrorTolerance)
        {
            return;
        }
        rotAxis /= rotAxisNorm;

        const double sinCurrent = rotAxisNorm;
        if (sinCurrent < kAngleErrorTolerance)
        {
            return;
        }

        // Δθ ≈ -Δcos / sin
        const double angleCorrection = -cosError / sinCurrent;
        const double clampedCorrection = std::clamp(angleCorrection, -0.5, 0.5);

        const Eigen::Vector3d angularImpulse =
            kAngularConstraintGain * clampedCorrection * rotAxis;

        PhysicalEntity* entityMyFar = resolveEndpointEntity(endpointAt(1 - conn.myEndIndex));
        PhysicalEntity* entityOtherFar = resolveEndpointEntity(other->endpointAt(1 - conn.otherEndIndex));

        if (entityMyFar != nullptr)
        {
            applyAngularCorrection(entityMyFar, angularImpulse);
        }
        if (entityOtherFar != nullptr)
        {
            applyAngularCorrection(entityOtherFar, -angularImpulse);
        }
    }
} // egret
