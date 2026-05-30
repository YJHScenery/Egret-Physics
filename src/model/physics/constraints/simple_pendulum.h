/**
* @file        simple_pendulum.h
 * @brief       单摆约束器头文件
 * @details     定义SimplePendulum类，提供单摆约束功能，
 *              包括锚点和摆动物体之间的距离约束。
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
#ifndef EGRET_PHYSICS_SIMPLE_PENDULUM_H
#define EGRET_PHYSICS_SIMPLE_PENDULUM_H
#include "connecting_line.h"
#include "particle.h"

namespace egret
{
    class Particle;

    /**
     * @brief       单摆约束器类，实现锚点与摆动物体间的距离约束。
     * @details     SimplePendulum 继承自 ConnectingLine，提供锚点（固定点）与
     *              摆动物体之间的距离约束功能，用于模拟单摆运动。
     *              锚点是质量为0的固定点，摆动物体是质量大于0的运动物体。
     *              支持路径点用于处理绳子遇到阻碍物的情况。
     * 
     * @invariant   m_anchor 是质量为0的固定粒子（锚点）
     * @invariant   m_entity 是质量大于0的摆动物体
     * @invariant   m_length > 0，摆长必须为正值
     * @remark      单摆是特殊的连接线约束，一端固定，适合模拟悬挂物体
     * @see         ConnectingLine, Particle, PhysicalEntity
     */
    class SimplePendulum : public ConnectingLine
    {
    public:
        SimplePendulum(double length, const Eigen::Vector3d& anchorPos, PhysicalEntity* entity, std::uint64_t id);

        SimplePendulum(double length, const Eigen::Vector3d& anchorPos, PhysicalEntity* entity, std::uint64_t id,
                       const std::initializer_list<Eigen::Vector3d>& turningPositions);

        ~SimplePendulum() override = default;

        void setAnchorPos(const Eigen::Vector3d& anchorPos);

        [[nodiscard]] Eigen::Vector3d getAnchorPos() const;

        [[nodiscard]] PhysicalEntity* getSwingBody() const { return m_entity; }

        void applyVelocityConstraint(double dt) override;

        void applyPositionConstraint(double dt) override;

        std::unique_ptr<PhysicsAbstract> clone(std::uint64_t id) const override;

    private:
        SimplePendulum();

        Particle m_anchor{{}, {}, 0, 0}; // 锚点（固定点）
        PhysicalEntity* m_entity{nullptr}; // 摆动物体
    };
} // egret

#endif //EGRET_PHYSICS_SIMPLE_PENDULUM_H
