//
// Created by jehor on 2026/4/25.
//

#include "connecting_line.h"
#include "physical_entity.h"

namespace egret
{
    // static_assert(sizeof(PhysicalEntity) > 0, "PhysicalEntity must be complete");

    ConnectingLine::ConnectingLine() : ConstraintsBase(generateID(ConstraintType::ConnectingLine))
    {
    }

    ConnectingLine::ConnectingLine(const std::uint64_t id) : ConstraintsBase(id)
    {
    }

    ConnectingLine::ConnectingLine(double length, PhysicalEntity* entityStart, PhysicalEntity* entityEnd) : m_length(
        length)
    {
        m_physicalEntities.reserve(2);
        m_physicalEntities.push_back(entityStart);
        m_physicalEntities.push_back(entityEnd);
    }

    ConnectingLine::ConnectingLine(double length, PhysicalEntity* entityStart, PhysicalEntity* entityEnd,
                                   const std::initializer_list<Eigen::Vector3d>& turningPositions) : ConnectingLine(
        length, entityStart, entityEnd)
    {
        m_pathPositions = turningPositions;
    }

    void ConnectingLine::addPathTurningPoint(size_t index, const Eigen::Vector3d& pos)
    {
        if (index >= m_pathPositions.size()) {
            return;
        }
        m_pathPositions.insert(m_pathPositions.begin() + static_cast<long long>(index), pos);
    }

    void ConnectingLine::removePathTurningPoint(size_t index)
    {
        if (index >= m_pathPositions.size()) {
            return;
        }
        m_pathPositions.erase(m_pathPositions.begin() + static_cast<long long>(index));
    }

    void ConnectingLine::changePathTurningPoint(size_t index, const Eigen::Vector3d& newPos)
    {
        if (index >= m_pathPositions.size()) {
            return;
        }
        m_pathPositions[index] = newPos;
    }

    size_t ConnectingLine::getPathPosSize() const
    {
        return m_pathPositions.size();
    }

    ConstraintType ConnectingLine::getType() const
    {
        return ConstraintType::ConnectingLine;
    }

    // std::vector<PhysicalEntity*> ConnectingLine::getConstrainedEntities()
    // {
    //     return m_physicalEntities;
    // }
    //
    // const std::vector<PhysicalEntity*>& ConnectingLine::getConstrainedEntities() const
    // {
    //     return m_physicalEntities;
    // }

    void ConnectingLine::applyVelocityConstraint(double dt)
    {
        if (m_physicalEntities.size() < 2) {
            return;
        }

        if (m_pathPositions.empty()) {
            // 无路径点，简单的两点距离速度约束
            Eigen::Vector3d p0 = m_physicalEntities[0]->getPositionCR();
            Eigen::Vector3d p1 = m_physicalEntities[1]->getPositionCR();
            Eigen::Vector3d dp = p1 - p0;
            double dist = dp.norm();

            // 如果距离小于约束长度，不施加速度约束
            if (dist <= m_length || dist < 1e-10) {
                return;
            }

            // 单位方向向量
            Eigen::Vector3d n = dp / dist;

            // 获取速度
            Eigen::Vector3d v0 = m_physicalEntities[0]->getSpeed();
            Eigen::Vector3d v1 = m_physicalEntities[1]->getSpeed();

            // 相对速度在约束方向上的分量
            // 约束方程: C = ||p1-p0|| - L = 0
            // 速度约束: Ċ = n · (v1 - v0)
            double dv = n.dot(v1 - v0);

            // 如果相对速度是收缩方向（dv <= 0），不施加约束
            // 只有当距离超过限制且物体正在远离时才施加约束
            if (dv <= 0) {
                return;
            }

            // 计算有效质量
            double m0 = m_physicalEntities[0]->getMass();
            double m1 = m_physicalEntities[1]->getMass();

            // 处理质量为零的情况
            if (m0 <= 0 && m1 <= 0) {
                return;
            }


            // 有效质量: M_eff = 1/(1/m0 + 1/m1)
            double invM0 = (m0 > 0) ? 1.0 / m0 : 0.0;
            double invM1 = (m1 > 0) ? 1.0 / m1 : 0.0;
            double invMeff = invM0 + invM1;

            if (invMeff < 1e-10) {
                return;
            }

            // 计算冲量标量
            // λ = -Ċ / M_eff
            double lambda = -dv / invMeff;

            // 应用冲量
            // Δv0 = -λ * n / m0
            // Δv1 = λ * n / m1
            if (m0 > 0) {
                m_physicalEntities[0]->setSpeed(v0 - lambda * n * invM0);
            }
            if (m1 > 0) {
                m_physicalEntities[1]->setSpeed(v1 + lambda * n * invM1);
            }
            return;
        }

        // 带路径点的折线速度约束（简化实现）
        // 对于折线约束，速度约束比较复杂，这里采用简化策略：
        // 只在物体远离时施加阻尼
        Eigen::Vector3d p0 = m_physicalEntities[0]->getPositionCR();
        Eigen::Vector3d p1 = m_physicalEntities[1]->getPositionCR();
        Eigen::Vector3d dp = p1 - p0;
        double dist = dp.norm();

        // 如果直接距离小于约束长度，不施加约束
        if (dist <= m_length || dist < 1e-10) {
            return;
        }

        Eigen::Vector3d n = dp / dist;
        Eigen::Vector3d v0 = m_physicalEntities[0]->getSpeed();
        Eigen::Vector3d v1 = m_physicalEntities[1]->getSpeed();

        double dv = n.dot(v1 - v0);

        // 只有当物体正在远离时才施加约束
        if (dv <= 0) {
            return;
        }

        double m0 = m_physicalEntities[0]->getMass();
        double m1 = m_physicalEntities[1]->getMass();

        if (m0 <= 0 && m1 <= 0) {
            return;
        }

        double invM0 = (m0 > 0) ? 1.0 / m0 : 0.0;
        double invM1 = (m1 > 0) ? 1.0 / m1 : 0.0;
        double invMeff = invM0 + invM1;

        if (invMeff < 1e-10) {
            return;
        }

        double lambda = -dv / invMeff;

        if (m0 > 0) {
            m_physicalEntities[0]->setSpeed(v0 - lambda * n * invM0);
        }
        if (m1 > 0) {
            m_physicalEntities[1]->setSpeed(v1 + lambda * n * invM1);
        }
    }

    void ConnectingLine::applyPositionConstraint(double dt)
    {
        if (m_physicalEntities.size() < 2) {
            return;
        }

        if (m_pathPositions.empty()) {
            // 无路径点，退化为简单的两点距离约束
            Eigen::Vector3d p0 = m_physicalEntities[0]->getPositionCR();
            Eigen::Vector3d p1 = m_physicalEntities[1]->getPositionCR();
            double dist = (p1 - p0).norm();

            if (dist <= m_length || dist < 1e-10) {
                return;
            }

            // 计算需要修正的距离
            double error = dist - m_length;
            Eigen::Vector3d direction = (p1 - p0) / dist;

            double m0 = m_physicalEntities[0]->getMass();
            double m1 = m_physicalEntities[1]->getMass();

            // 处理质量无限大的情况
            if (m0 <= 0 && m1 <= 0) {
                return; // 两个都是固定物体，无法修正
            }

            // 按质量比例分配修正量
            double corr0 = (m1 > 0) ? error * m1 / (m0 + m1) : 0.0;
            double corr1 = (m0 > 0) ? error * m0 / (m0 + m1) : 0.0;

            Eigen::Vector3d newPos0 = p0 + direction * corr0;
            Eigen::Vector3d newPos1 = p1 - direction * corr1;

            m_physicalEntities[0]->setPosition(newPos0);
            m_physicalEntities[1]->setPosition(newPos1);
            return;
        }

        // 带路径点的折线约束
        // 1. 首先检查两个物体之间的直接距离
        // 如果直接距离已经小于等于约束长度，说明物体已经在约束范围内，不需要修正
        Eigen::Vector3d startPos = m_physicalEntities[0]->getPositionCR();
        Eigen::Vector3d endPos = m_physicalEntities[1]->getPositionCR();
        double directDist = (endPos - startPos).norm();

        // 如果两个物体已经非常接近（距离小于等于约束长度），不施加约束
        // 这防止了当物体碰撞时约束器错误地施加力
        if (directDist <= m_length || directDist < 1e-10) {
            return;
        }

        // 2. 计算当前折线总长度
        double totalLength = 0.0;
        std::vector<double> segmentLengths;

        Eigen::Vector3d prevPos = startPos;
        segmentLengths.push_back((m_pathPositions[0] - prevPos).norm());
        totalLength += segmentLengths.back();

        for (size_t i = 0; i < m_pathPositions.size() - 1; ++i) {
            segmentLengths.push_back((m_pathPositions[i + 1] - m_pathPositions[i]).norm());
            totalLength += segmentLengths.back();
        }

        segmentLengths.push_back((endPos - m_pathPositions.back()).norm());
        totalLength += segmentLengths.back();

        // 3. 检查约束是否违反
        if (totalLength <= m_length) {
            return;
        }

        // 4. 计算修正量
        double error = totalLength - m_length;

        // 5. 按距离比例分配修正量给两个物体
        double m0 = m_physicalEntities[0]->getMass();
        double m1 = m_physicalEntities[1]->getMass();

        if (m0 <= 0 && m1 <= 0) {
            return; // 两个都是固定物体
        }

        // 修正方向：从起点到终点
        Eigen::Vector3d direction = (endPos - startPos);
        double fullDist = direction.norm();
        if (fullDist < 1e-10) {
            return; // 两点重合
        }
        direction.normalize();

        // 按质量比例计算两端的修正量
        double totalMass = m0 + m1;
        double corr0 = (m1 > 0) ? error * m1 / totalMass : 0.0;
        double corr1 = (m0 > 0) ? error * m0 / totalMass : 0.0;

        // 6. 修正物体位置
        Eigen::Vector3d newStartPos = startPos + direction * corr0;
        Eigen::Vector3d newEndPos = endPos - direction * corr1;

        m_physicalEntities[0]->setPosition(newStartPos);
        m_physicalEntities[1]->setPosition(newEndPos);
    }

    double ConnectingLine::computeConstraintError() const
    {
        if (m_physicalEntities.size() < 2 || m_pathPositions.empty()) {
            return 0.0;
        }

        double totalLength = 0.0;
        Eigen::Vector3d prevPos = m_physicalEntities[0]->getPositionCR();

        // 累加起始点到第一个路径点的距离
        totalLength += (m_pathPositions[0] - prevPos).norm();

        // 累加路径点之间的距离
        for (size_t i = 0; i < m_pathPositions.size() - 1; ++i) {
            totalLength += (m_pathPositions[i + 1] - m_pathPositions[i]).norm();
        }

        // 累加最后一个路径点到结束点的距离
        Eigen::Vector3d endPos = m_physicalEntities[1]->getPositionCR();
        totalLength += (endPos - m_pathPositions.back()).norm();

        // 返回超出长度（如果 totalLength <= m_length，误差为 0）
        return std::max(0.0, totalLength - m_length);
    }

    void ConnectingLine::applyAngularVelocityConstraint(double dt)
    {
        std::ignore = dt;
        // 不参与角速度变化。
    }
} // egret
