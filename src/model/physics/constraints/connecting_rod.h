/**
 * @file        connecting_rod.h
 * @brief       约束杆头文件，定义两个物理实体间的刚性距离约束。
 * @details     定义 ConnectingRod 类，提供两个物理实体之间的固定距离约束功能，
 *              既不允许拉伸也不允许压缩，适用于模拟刚性连杆、推杆等场景。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-18
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#ifndef EGRET_PHYSICS_CONNECTING_ROD_H
#define EGRET_PHYSICS_CONNECTING_ROD_H

#include "constraints_base.h"
#include "Eigen/Dense"

namespace egret
{
    class PhysicalEntity;

    /**
     * @brief       约束杆类，实现两个物理实体间的刚性距离约束。
     * @details     ConnectingRod 继承自 ConstraintsBase，提供两个物理实体之间的
     *              固定距离约束功能。与 ConnectingLine（柔性绳）不同，约束杆是
     *              双向刚性约束——既不允许拉伸也不允许压缩，始终保持两端距离恒定。
     *              适用于模拟刚性连杆、推杆、机械臂连杆等场景。
     *
     *              约束方程：C = ||p₁ - p₀|| - L = 0
     *              速度约束：Ċ = n · (v₁ - v₀) = 0
     *
     *              当前实现将实体视为质点（COM 到 COM 约束），
     *              不处理连接点偏移和角动量传递（预留扩展接口）。
     *
     * @invariant   m_length > 0，杆长必须为正值
     * @invariant   m_entityA / m_entityB 为非空实体指针
     * @invariant   约束严格保持等式约束，不区分拉伸与压缩
     * @remark      与 ConnectingLine 不同，约束杆在压缩方向也施加约束力
     * @see         ConstraintsBase, ConnectingLine, PhysicalEntity
     */
    class ConnectingRod : public ConstraintsBase
    {
    public:
        /**
         * @brief 构造约束杆。
         * @param length   杆长（m），必须 > 0
         * @param entityA  第一个物理实体
         * @param entityB  第二个物理实体
         * @param id       约束唯一 ID
         */
        ConnectingRod(double length, PhysicalEntity* entityA, PhysicalEntity* entityB, std::uint64_t id);

        ~ConnectingRod() override = default;

        [[nodiscard]] ConstraintType getType() const override;

        void applyVelocityConstraint([[maybe_unused]] double dt) override;

        void applyPositionConstraint([[maybe_unused]] double dt) override;

        void applyAngularVelocityConstraint([[maybe_unused]] double dt) override;

        [[nodiscard]] double computeConstraintError() const override;

        [[nodiscard]] double getReferenceLength() const override { return m_length; }

        /** 获取约束杆长度。 */
        [[nodiscard]] double getLength() const { return m_length; }

        /** 设置约束杆长度。 */
        void setLength(double length) { m_length = length; }

        [[nodiscard]] std::unique_ptr<PhysicsAbstract> clone(std::uint64_t id) const override;

    protected:
        ConnectingRod();

        explicit ConnectingRod(std::uint64_t id);

        /** 第一个实体指针。 */
        PhysicalEntity* m_entityA{nullptr};

        /** 第二个实体指针。 */
        PhysicalEntity* m_entityB{nullptr};

        /** 约束杆长度（m）。 */
        double m_length{};
    };
} // egret

#endif //EGRET_PHYSICS_CONNECTING_ROD_H
