/**
 * @file        support_surface.h
 * @brief       支撑曲面约束器头文件，定义基于曲面的广泛约束器。
 * @details     定义 SupportSurface 类，继承自 ConstraintsBase，
 *              接收一个 SurfaceBase 曲面作为支撑面的解析式，作用于任何注册的实体。
 *              与其他约束器不同，SupportSurface 对实体不做限制——任何实体都可以注册到
 *              约束器中。约束器在求解阶段检测实体与曲面之间的碰撞，并施加法向冲量
 *              （带恢复系数）和摩擦力（预留接口）。
 *
 *              实体管理通过基类 ConstraintsBase 的 registerEntity / unregisterEntity
 *              接口统一实现，所有受约束实体存储在 m_registeredEntities 中。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-01
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    Constraints (约束器)
 */

#ifndef EGRET_PHYSICS_SUPPORT_SURFACE_H
#define EGRET_PHYSICS_SUPPORT_SURFACE_H

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "constraints_base.h"
#include "surface_base.h"

namespace egret
{
    class SurfaceBase;

    /**
     * @brief       支撑曲面约束器类，实现曲面支撑约束。
     * @details     SupportSurface 继承自 ConstraintsBase，接收一个 SurfaceBase 曲面
     *              作为支撑面，对注册的物理实体施加曲面碰撞约束。
     *
     *              行为说明：
     *              - 每个求解步遍历注册实体，检测实体形状是否与曲面碰撞
     *              - 如果碰撞，施加法向冲量（带恢复系数）使实体弹性反弹
     *              - 摩擦力接口已预留（m_frictionCoeff），但暂不激活
     *              - 不使用 ConstraintsBase::applyPositionConstraint（用户要求）
     *              - 冲量施加通过直接修改实体的速度和角速度实现
     *
     *              恢复系数来源：
     *              - 优先使用 SupportSurface 自身的 m_restitution
     *              - 如果未设置（负值），则使用实体自身的 restitution 属性
     *
     *              使用示例：
     *              @code
     *              auto surface = std::make_unique<SurfacePlane>(anchor, normal);
     *              auto constraint = std::make_shared<SupportSurface>(id, std::move(surface));
     *              constraint->setRestitution(0.5);
     *              constraint->registerEntity(entity1);
     *              constraint->registerEntity(entity2);
     *              scene.registerConstraint("Ground", constraint);
     *              @endcode
     *
     * @invariant   m_surface 是有效的曲面实例
     * @invariant   m_restitution ∈ [0, 1] 或为负值（使用实体自身值）
     * @invariant   m_frictionCoeff ∈ [0, ∞)，0 表示无摩擦
     * @see         ConstraintsBase, SurfaceBase, PhysicalEntity
     */
    class SupportSurface final : public ConstraintsBase
    {
    public:
        /**
         * @brief 构造支撑曲面约束器。
         * @param id 约束器唯一标识符。
         * @param surface 曲面所有权。
         */
        explicit SupportSurface(std::uint64_t id,
                                std::unique_ptr<SurfaceBase> surface);

        ~SupportSurface() override = default;

        // 禁止拷贝，允许移动

        SupportSurface(SupportSurface&&) = default;

        SupportSurface& operator=(SupportSurface&&) = default;

        // ==================== 约束接口重写 ====================

        /** @brief 返回约束类型，始终返回 ConstraintType::SupportSurface。 */
        [[nodiscard]] ConstraintType getType() const override;

        /**
         * @brief 应用速度修正（冲量阶段）。
         * @details 遍历注册的实体，检测与曲面的碰撞，对碰撞实体施加法向冲量。
         *          冲量计算基于恢复系数和接触点相对速度。
         *          同时处理角速度修正（刚体旋转响应）。
         * @param dt 固定时间步长。
         */
        void applyVelocityConstraint(double dt) override;

        /**
         * @brief 应用位置修正。
         * @details 用户要求不使用位置修正函数，此方法为空。
         * @param dt 固定时间步长（未使用）。
         */
        void applyPositionConstraint(double dt) override { /* 用户要求不使用位置修正 */ }

        /**
         * @brief 应用角速度修正。
         * @details 角速度修正已在 applyVelocityConstraint 中通过冲量-力矩关系处理。
         * @param dt 固定时间步长（未使用）。
         */
        void applyAngularVelocityConstraint(double dt) override { /* 在速度修正中已处理 */ }

        /**
         * @brief 计算约束误差。
         * @details 计算所有注册实体中最大的穿透深度，作为误差度量。
         * @return 最大穿透深度（无穿透时返回 0）。
         */
        [[nodiscard]] double computeConstraintError() const override;

        /**
         * @brief 将约束力注入实体力累加器（积分前罚力阶段）。
         * @details 在积分前对穿透或接近曲面的实体施加法向罚力，
         *          防止实体在积分步中深入穿透曲面。
         *          罚力公式：F = -k * penetration * normal
         *          其中 k 为罚刚度 (m_penaltyStiffness)。
         *          该力通过 upsertForce 注入实体力累加器，
         *          与场力一起在积分阶段被处理。
         * @param dt 固定时间步长。
         */
        void applyConstraintForces(double dt) override;

        // ==================== 曲面管理 ====================

        /**
         * @brief 获取曲面指针（只读）。
         * @return 曲面常量指针。
         */
        [[nodiscard]] const SurfaceBase* getSurface() const { return m_surface.get(); }

        /**
         * @brief 获取曲面指针（可变）。
         * @return 曲面可变指针。
         */
        [[nodiscard]] SurfaceBase* getSurface() { return m_surface.get(); }

        /**
         * @brief 替换曲面。
         * @param newSurface 新的曲面所有权。
         */
        void setSurface(std::unique_ptr<SurfaceBase> newSurface)
        {
            m_surface = std::move(newSurface);
        }

        // ==================== 物理参数 ====================

        /**
         * @brief 设置恢复系数。
         * @details 设置为此约束器自己的恢复系数，覆盖实体的自带值。
         *          设置为负值（默认）则退回到使用实体自身的恢复系数。
         * @param restitution 恢复系数，范围 [0, 1]，负值表示使用实体自身值。
         */
        void setRestitution(double restitution) { m_restitution = restitution; }

        /**
         * @brief 获取恢复系数。
         * @return 当前设置的恢复系数（负值表示使用实体自身值）。
         */
        [[nodiscard]] double getRestitution() const { return m_restitution; }

        /**
         * @brief 设置摩擦系数（预留接口）。
         * @details 当前版本摩擦力尚未实现，此接口保留供后续版本使用。
         *          设置后对约束行为无影响，仅存储参数。
         * @param frictionCoeff 摩擦系数，≥ 0。
         */
        void setFrictionCoeff(double frictionCoeff) { m_frictionCoeff = std::max(0.0, frictionCoeff); }

        /**
         * @brief 获取摩擦系数。
         * @return 当前设置的摩擦系数。
         */
        [[nodiscard]] double getFrictionCoeff() const { return m_frictionCoeff; }

        /**
         * @brief 设置罚刚度。
         * @details 控制积分前法向罚力的强度，值越大对穿透抵抗越强。
         *          典型范围：1e3 ~ 1e6 N/m。
         *          设置为 0 可完全禁用罚力阶段（仅用冲量）。
         * @param stiffness 罚刚度 (N/m)，必须 ≥ 0。
         */
        void setPenaltyStiffness(double stiffness) { m_penaltyStiffness = std::max(0.0, stiffness); }

        /**
         * @brief 获取罚刚度。
         * @return 当前罚刚度 (N/m)。
         */
        [[nodiscard]] double getPenaltyStiffness() const { return m_penaltyStiffness; }

        SupportSurface() = default;

        SupportSurface(const SupportSurface& other);

        SupportSurface& operator=(const SupportSurface& other);

        std::unique_ptr<PhysicsAbstract> clone(std::uint64_t id) const override;
    private:
        /**
         * @brief 对单个碰撞实体施加法向冲量。
         * @details 计算接触点处的法向相对速度，应用恢复系数计算冲量大小，
         *          同时作用于线速度和角速度。
         *
         *          公式推导：
         *          接触点相对速度 v_rel = v_cm + ω × r
         *          法向分量 v_n = dot(v_rel, n)
         *          冲量大小 j = -(1 + e) * v_n / (1/m + dot((I⁻¹ · (r × n)) × r, n))
         *          其中 r = contactPoint - centerOfMass, e = 恢复系数
         *          对于静态曲面（无限质量），分母中只有实体项。
         *
         * @param entity 碰撞实体指针。
         * @param contact 接触点数据（位置、法线、穿透深度）。
         * @param dt 固定时间步长。
         */
        void applyContactImpulse(PhysicalEntity* entity,
                                 const ContactPoint& contact,
                                 double dt);



    private:
        /** 曲面所有权。 */
        std::unique_ptr<SurfaceBase> m_surface;

        /**
         * 恢复系数。负值表示使用实体自身的恢复系数。
         */
        double m_restitution{-1.0};

        /**
         * 摩擦系数（预留，当前未使用）。
         */
        double m_frictionCoeff{0.0};

        /**
         * 罚刚度，控制积分前法向罚力的强度。
         * 值越大，对穿透的抵抗越强，但过大会导致数值不稳定。
         * 典型值范围：1e3 ~ 1e6 (N/m)。
         */
        double m_penaltyStiffness{1.0e5};
    };
} // egret

#endif // EGRET_PHYSICS_SUPPORT_SURFACE_H
