/**
 * @file        simple_pendulum.h
 * @brief       单摆约束器头文件
 * @details     定义SimplePendulum类，提供单摆约束功能，
 *              包括锚点和摆动物体之间的距离约束。
 *              直接继承 ConstraintsBase，自持摆长和路径点。
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

#include "constraints_base.h"
#include "particle.h"
#include <Eigen/Dense>

namespace egret
{
    class Particle;

    /**
     * @brief       单摆约束器类，实现锚点与摆动物体间的距离约束。
     * @details     SimplePendulum 直接继承 ConstraintsBase，提供锚点（固定点）与
     *              摆动物体之间的距离约束功能，用于模拟单摆运动。
     *              锚点是质量为0的固定粒子，摆动物体是质量大于0的运动物体。
     *              支持路径点用于处理绳子遇到阻碍物的情况。
     *
     *              绳长达到上限时使用单侧速度和位置投影。
     *
     * @invariant   m_anchor 是质量为0的固定粒子（锚点）
     * @invariant   m_entity 是质量大于0的摆动物体
     * @invariant   m_length > 0，摆长必须为正值
     * @remark      单摆是特殊的距离约束，一端固定，适合模拟悬挂物体
     * @see         ConstraintsBase, Particle, PhysicalEntity
     */
    class SimplePendulum : public ConstraintsBase
    {
    public:
        /**
         * @brief 构造单摆约束器。
         * @param length     摆长（约束长度）
         * @param anchorPos  锚点位置
         * @param entity     摆动物体指针
         * @param id         约束器唯一标识符
         */
        SimplePendulum(double length, const Eigen::Vector3d& anchorPos, PhysicalEntity* entity, std::uint64_t id);

        ~SimplePendulum() override = default;

        // ---- 锚点操作 ----
        void setAnchorPos(const Eigen::Vector3d& anchorPos);

        [[nodiscard]] Eigen::Vector3d getAnchorPos() const;

        [[nodiscard]] PhysicalEntity* getSwingBody() const { return m_entity; }

        // ---- ConstraintsBase 纯虚方法实现 ----
        [[nodiscard]] ConstraintType getType() const override;

        void applyVelocityConstraint([[maybe_unused]] double dt) override;

        void applyPositionConstraint([[maybe_unused]] double dt) override;

        void applyAngularVelocityConstraint([[maybe_unused]] double dt) override;

        [[nodiscard]] double computeConstraintError() const override;

        [[nodiscard]] double getReferenceLength() const override { return m_length; }

        [[nodiscard]] double getLength() const { return m_length; }

        [[nodiscard]] std::unique_ptr<PhysicsAbstract> clone(std::uint64_t id) const override;

        SimplePendulum();

        SimplePendulum(const SimplePendulum& other);

        SimplePendulum& operator=(const SimplePendulum& other);
    private:
        Particle m_anchor{{}, {}, 0, 0};               // 锚点（固定粒子）
        PhysicalEntity* m_entity{nullptr};              // 摆动物体
        double m_length{};                  // 摆长（约束长度）

    };
} // egret

#endif // EGRET_PHYSICS_SIMPLE_PENDULUM_H
