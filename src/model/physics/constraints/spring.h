/**
 * @file        spring.h
 * @brief       弹簧约束器头文件
 * @details     定义Spring类，提供两物体间的弹簧力约束功能，
 *              遵循胡克定律 F = -k·Δx。
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

#ifndef EGRET_PHYSICS_SPRING_H
#define EGRET_PHYSICS_SPRING_H

#include "constraints_base.h"
#include "Eigen/Dense"

namespace egret
{
    /**
     * @brief       弹簧约束器类，实现两物体间的胡克定律弹簧力。
     * @details     Spring 继承自 ConstraintsBase，提供两个物理实体之间的
     *              理想弹簧力约束。弹簧力遵循胡克定律 F = -k·Δx，
     *              其中 Δx 为当前距离与原长的差值。
     *              与 ConnectingLine 不同，弹簧不限制长度上限，而是
     *              持续施加恢复力。弹簧采用力注入方式工作，通过重写
     *              applyConstraintForces() 将胡克力注册到实体的力累加器中，
     *              由积分器统一处理。
     *
     * @invariant   m_stiffness > 0，弹性系数必须为正值
     * @invariant   m_restLength >= 0，原长必须为非负值
     * @invariant   m_entityA / m_entityB 包含两个物理实体
     * @remark      理想弹簧没有长度上限，完全遵循胡克定律
     * @see         ConstraintsBase, ConnectingLine, PhysicalEntity
     */
    class Spring : public ConstraintsBase
    {
    public:
        /**
         * @brief 构造弹簧约束器。
         * @param stiffness  弹性系数 k（N/m），必须 > 0
         * @param restLength 弹簧原长（m）。若 ≤ 0，则在构造时取两实体初始距离
         * @param entityA    第一个物理实体
         * @param entityB    第二个物理实体
         * @param id         约束唯一 ID
         */
        Spring(double stiffness, double restLength,
               PhysicalEntity* entityA, PhysicalEntity* entityB,
               std::uint64_t id);

        ~Spring() override = default;

        [[nodiscard]] ConstraintType getType() const override;

        // 力注入路径 —— 胡克定律
        void applyConstraintForces() override;

        // 以下虚函数为空实现，弹簧不直接修正速度/位置
        void applyVelocityConstraint([[maybe_unused]] double dt) override { }

        void applyPositionConstraint([[maybe_unused]] double dt) override {  }

        void applyAngularVelocityConstraint([[maybe_unused]] double dt) override {  }

        [[nodiscard]] double computeConstraintError() const override;

        [[nodiscard]] std::unique_ptr<PhysicsAbstract> clone(std::uint64_t id) const override;

        /** 获取参考长度（原长），用于渲染和调试。 */
        [[nodiscard]] double getReferenceLength() const override { return m_restLength; }

        /** 获取弹性系数。 */
        [[nodiscard]] double getStiffness() const { return m_stiffness; }

        /** 设置弹性系数。 */
        void setStiffness(double stiffness) { m_stiffness = stiffness; }

        /** 获取原长。 */
        [[nodiscard]] double getRestLength() const { return m_restLength; }

        /** 设置原长。 */
        void setRestLength(double restLength) { m_restLength = restLength; }

        /** @brief 设置沿弹簧方向的非负阻尼系数。 */
        void setDamping(double damping) { m_damping = std::max(0.0, damping); }
        /** @brief 获取沿弹簧方向的阻尼系数。 */
        [[nodiscard]] double getDamping() const { return m_damping; }

        Spring() = delete;
    protected:
        /** 起点实体指针。 */
        PhysicalEntity* m_entityA{nullptr};

        /** 终点实体指针。 */
        PhysicalEntity* m_entityB{nullptr};

    private:


        /** 弹性系数 k（N/m）。 */
        double m_stiffness{};

        /** 弹簧原长（m）。 */
        double m_restLength{};
        /** 沿弹簧方向的阻尼系数。 */
        double m_damping{0.0};
    };
} // egret

#endif //EGRET_PHYSICS_SPRING_H
