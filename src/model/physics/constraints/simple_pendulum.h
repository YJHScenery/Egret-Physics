//
// Created by jehor on 2026/5/23.
//

#ifndef EGRET_PHYSICS_SIMPLE_PENDULUM_H
#define EGRET_PHYSICS_SIMPLE_PENDULUM_H
#include "connecting_line.h"
#include "particle.h"

namespace egret
{
    class Particle;

    /**
     * @brief 单摆约束器
     * 
     * 单摆约束器用于模拟绳子/摆链等只能被拉伸而不能被压缩的约束。
     * 特点：
     * 1. 锚点（anchor）是质量为0的固定点
     * 2. 摆动物体（entity）是质量大于0的运动物体
     * 3. 支持路径点（用于处理绳子遇到阻碍的情况）
     * 
     * 使用场景：
     * - 悬挂物体（吊灯、摆钟等）
     * - 绳子连接的场景
     * - 摆动过程中遇到阻碍物的情况
     */
    class SimplePendulum : public ConnectingLine
    {
    public:
        SimplePendulum();

        SimplePendulum(double length, const Eigen::Vector3d& anchorPos, PhysicalEntity* entity);

        SimplePendulum(double length, const Eigen::Vector3d& anchorPos, PhysicalEntity* entity,
                       const std::initializer_list<Eigen::Vector3d>& turningPositions);

        ~SimplePendulum() override = default;

        void setAnchorPos(const Eigen::Vector3d& anchorPos);

        [[nodiscard]] Eigen::Vector3d getAnchorPos() const;

        [[nodiscard]] PhysicalEntity* getSwingBody() const { return m_entity; }

        void applyVelocityConstraint(double dt) override;

        void applyPositionConstraint(double dt) override;

    private:
        Particle m_anchor{}; // 锚点（固定点）
        PhysicalEntity* m_entity{nullptr}; // 摆动物体
    };
} // egret

#endif //EGRET_PHYSICS_SIMPLE_PENDULUM_H
