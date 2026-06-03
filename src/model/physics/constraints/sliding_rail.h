/**
 * @file        sliding_rail.h
 * @brief       滑轨约束器头文件，定义基于空间曲线的滑轨约束。
 * @details     定义 SlidingRail 类，继承自 ConstraintsBase，
 *              接收一个 CurveBase 空间曲线作为轨道的几何描述。
 *              与 SupportSurface（支撑曲面）类似，支持多个独立实体注册，
 *              每个实体独立受约束，互不影响。
 *
 *              约束机制以力性约束为主（applyConstraintForces），
 *              通过读取实体力列表，计算力的法线方向（垂直于曲线切向）分量，
 *              并通过 upsertForce 注入反作用力将其抵消，
 *              使实体只能沿曲线切向运动。
 *
 *              位置、速度、角速度约束仅作为微调手段，在实体因数值误差
 *              明显偏离轨道时做轻柔修正，不做强制快照。
 *
 *              实体管理通过基类 ConstraintsBase 的 registerEntity /
 *              unregisterEntity 接口统一实现，所有受约束实体存储在
 *              m_registeredEntities 中。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-02
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    Constraints (约束器)
 */

#ifndef EGRET_PHYSICS_SLIDING_RAIL_H
#define EGRET_PHYSICS_SLIDING_RAIL_H

#include <memory>
#include <optional>
#include <unordered_map>

#include "constraints_base.h"
#include "curve_base.h"

namespace egret
{
    /**
     * @brief       滑轨约束器类，将实体约束到空间曲线上运动。
     * @details     SlidingRail 继承自 ConstraintsBase，接收一个 CurveBase
     *              空间曲线作为滑轨轨道，对注册的物理实体施加轨道约束。
     *
     *              行为说明：
     *              - 以力性约束为主（applyConstraintForces），
     *                每帧读取实体合力，分解为切向（沿轨道）和法向（垂直轨道），
     *                注入法向反作用力抵消偏离轨道的力分量。
     *              - 位置、速度、角速度约束仅做微调：
     *                - applyPositionConstraint：仅在实体偏离轨道超过阈值时，
     *                  轻柔回拉部分偏移量。
     *                - applyVelocityConstraint：仅在法向速度超过阈值时，
     *                  衰减法向分量。
     *                - applyAngularVelocityConstraint：仅在朝向偏差
     *                  超过阈值时，轻修角速度。
     *              - 摩擦力为预留接口，当前仅存储参数、注释占位，
     *                不参与实际约束计算。
     *
     *              初始化处理（需求 3.1）：
     *              - 实体注册后首次出现在约束求解中时，若其位置不在轨道上，
     *                applyPositionConstraint 会快速将其拉入轨道（较大的拉回系数）。
     *              - 实体在轨道上的参数 t 通过 m_entityParamMap 持续追踪。
     *
     * @invariant   m_curve 是有效的 CurveBase 实例
     * @invariant   m_entityParamMap 为每个注册实体维护参数 t 映射
     * @invariant   m_frictionCoeff ≥ 0，当前不参与约束计算
     * @invariant   m_positionTolerance > 0，位置微调激活阈值
     * @invariant   m_velocityTolerance > 0，速度微调激活阈值
     * @see         ConstraintsBase, CurveBase, PhysicalEntity
     */
    class SlidingRail final : public ConstraintsBase
    {
    public:
        /**
         * @brief 构造滑轨约束器。
         * @param id    约束器唯一标识符。
         * @param curve 曲线轨道所有权（必须有效）。
         */
        explicit SlidingRail(std::uint64_t id,
                             std::unique_ptr<CurveBase> curve);

        ~SlidingRail() override = default;

        // 禁止拷贝，允许移动
        SlidingRail(SlidingRail&&)  noexcept = default;
        SlidingRail& operator=(SlidingRail&&) = default;

        // ==================== 约束接口重写 ====================

        /** @brief 返回约束类型，始终返回 ConstraintType::SlidingRail。 */
        [[nodiscard]] ConstraintType getType() const override;

        /**
         * @brief 应用角速度修正（微调）。
         * @details 检测实体朝向与曲线切向的偏差，
         *          若超过 m_angularTolerance，计算轻柔的角速度修正。
         *          仅作为微调手段，不做强制对齐。
         * @param dt 固定时间步长。
         */
        void applyAngularVelocityConstraint(double dt) override;

        /**
         * @brief 应用位置修正（微调）。
         * @details 检测实体位置与曲线最近点的偏移距离，
         *          若超过 m_positionTolerance，将实体轻柔回拉一部分。
         *          对于首次出现的实体（m_entityParamMap 中无记录），
         *          使用较大的拉回系数快速进入轨道。
         * @param dt 固定时间步长（未使用）。
         */
        void applyPositionConstraint(double dt) override;

        /**
         * @brief 应用速度修正（微调）。
         * @details 检测实体速度的法向（垂直于曲线切向）分量，
         *          若超过 m_velocityTolerance，衰减法向分量。
         * @param dt 固定时间步长（未使用）。
         */
        void applyVelocityConstraint(double dt) override;

        /**
         * @brief 应用约束力（主要约束手段）。
         * @details 遍历注册实体，读取合力列表，
         *          分解为切向分量（沿轨道方向）和法向分量（垂直轨道方向），
         *          注入法向反作用力以抵消偏离轨道的力分量。
         *          若作用点（曲线最近点）偏离质心，同时注入等效力矩。
         * @param dt 固定时间步长。
         */
        void applyConstraintForces(double dt) override;

        /**
         * @brief 计算约束误差。
         * @details 计算所有注册实体到曲线最近点的最大距离，作为误差度量。
         * @return 最大偏差距离（无偏差时返回 0）。
         */
        [[nodiscard]] double computeConstraintError() const override;

        /**
         * @brief 深度复制约束器。
         * @param id 新约束器的唯一标识符。
         * @return 指向新 SlidingRail 对象的 unique_ptr。
         */
        [[nodiscard]] std::unique_ptr<PhysicsAbstract> clone(std::uint64_t id) const override;

        // ==================== 曲线管理 ====================

        /**
         * @brief 获取曲线轨道指针（只读）。
         * @return 曲线常量指针。
         */
        [[nodiscard]] const CurveBase* getCurve() const { return m_curve.get(); }

        /**
         * @brief 获取曲线轨道指针（可变）。
         * @return 曲线可变指针。
         */
        [[nodiscard]] CurveBase* getCurve() { return m_curve.get(); }

        /**
         * @brief 替换曲线轨道。
         * @param newCurve 新的曲线所有权（必须有效）。
         */
        void setCurve(std::unique_ptr<CurveBase> newCurve)
        {
            m_curve = std::move(newCurve);
            m_entityParamMap.clear(); // 轨道变更，参数失效，需要重新初始化
        }

        // ==================== 微调参数 ====================

        /**
         * @brief 设置位置微调阈值。
         * @details 实体偏离轨道距离超过此值时触发位置回拉。
         * @param tolerance ≥ 0 的阈值（米）。设为 0 可禁用位置微调。
         */
        void setPositionTolerance(double tolerance) { m_positionTolerance = std::max(0.0, tolerance); }

        /**
         * @brief 获取位置微调阈值。
         * @return 当前位置微调阈值（米）。
         */
        [[nodiscard]] double getPositionTolerance() const { return m_positionTolerance; }

        /**
         * @brief 设置速度微调阈值。
         * @details 法向速度超过此值时触发法向速度衰减。
         * @param tolerance ≥ 0 的阈值（m/s）。设为 0 可禁用速度微调。
         */
        void setVelocityTolerance(double tolerance) { m_velocityTolerance = std::max(0.0, tolerance); }

        /**
         * @brief 获取速度微调阈值。
         * @return 当前速度微调阈值（m/s）。
         */
        [[nodiscard]] double getVelocityTolerance() const { return m_velocityTolerance; }

        /**
         * @brief 设置角速度微调阈值。
         * @details 朝向偏差超过此值时触发角速度修正。
         * @param tolerance ≥ 0 的阈值（弧度）。设为 0 可禁用角度微调。
         */
        void setAngularTolerance(double tolerance) { m_angularTolerance = std::max(0.0, tolerance); }

        /**
         * @brief 获取角速度微调阈值。
         * @return 当前角速度微调阈值（弧度）。
         */
        [[nodiscard]] double getAngularTolerance() const { return m_angularTolerance; }

        /**
         * @brief 设置拉回系数。
         * @details 位置微调时，实际回拉量 = 偏差 × 拉回系数。
         *          取值范围 (0, 1]，值越大回拉越强。
         *          首次出现的实体使用首次拉回系数（m_firstSnapFactor）。
         * @param factor 拉回系数。
         */
        void setPullBackFactor(double factor)
        {
            m_pullBackFactor = std::clamp(factor, 0.0, 1.0);
        }

        /**
         * @brief 获取拉回系数。
         * @return 当前拉回系数。
         */
        [[nodiscard]] double getPullBackFactor() const { return m_pullBackFactor; }

        /**
         * @brief 设置首次拉回系数。
         * @details 实体首次被约束求解时，若不在轨道上，使用此系数快速进入轨道。
         * @param factor 首次拉回系数，通常大于 m_pullBackFactor。
         */
        void setFirstSnapFactor(double factor)
        {
            m_firstSnapFactor = std::clamp(factor, 0.0, 1.0);
        }

        /**
         * @brief 获取首次拉回系数。
         * @return 当前首次拉回系数。
         */
        [[nodiscard]] double getFirstSnapFactor() const { return m_firstSnapFactor; }

        /**
         * @brief 设置法向速度衰减系数。
         * @details 速度微调时，法向速度乘以 (1 - 衰减系数) 来减少法向分量。
         *          取值范围 [0, 1]，0 表示不衰减，1 表示完全移除法向分量。
         * @param factor 衰减系数。
         */
        void setNormalDampingFactor(double factor)
        {
            m_normalDampingFactor = std::clamp(factor, 0.0, 1.0);
        }

        /**
         * @brief 获取法向速度衰减系数。
         * @return 当前衰减系数。
         */
        [[nodiscard]] double getNormalDampingFactor() const { return m_normalDampingFactor; }

        // ==================== 摩擦力预留接口 ====================

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

        // ==================== 默认构造 / 拷贝 ====================

        SlidingRail() = default;

        SlidingRail(const SlidingRail& other);

        SlidingRail& operator=(const SlidingRail& other);

    private:

        // ==================== 辅助方法 ====================

        /**
         * @brief 查找距离给定点最近的曲线点。
         * @details 采样 + 三分搜索精化。
         *          1. 在 [getStartT(), getEndT()] 均匀采样 k_sampleCount 个点
         *          2. 找出最近采样点
         *          3. 在其邻域内做 k_refineIterations 次三分搜索精化
         * @param point         要投影的三维点。
         * @param tInitialGuess 初始参数猜测值（可选）。
         * @return 最近曲线点的参数 t 和坐标 paired。
         */
        [[nodiscard]] std::pair<double, Eigen::Vector3d> findClosestPointOnCurve(
            const Eigen::Vector3d& point,
            std::optional<double> tInitialGuess = std::nullopt) const;

        /**
         * @brief 获取曲线在参数 t 处的单位切向量。
         * @param t 曲线参数。
         * @return 归一化切向量。若导数接近零，返回零向量。
         */
        [[nodiscard]] Eigen::Vector3d getUnitTangent(double t) const;

        /**
         * @brief 对单个实体施加约束反力（主要约束机制）。
         * @details 读取实体合力，分解为切向和法向分量，注入法向反作用力。
         * @param entity   物理实体指针。
         * @param t        实体在曲线上的参数 t。
         * @param closest  曲线上最近点的坐标。
         * @param dt       固定时间步长。
         */
        void applyReactionForce(PhysicalEntity* entity,
                                double t,
                                const Eigen::Vector3d& closest,
                                double dt) const;

        // ==================== 常量 ====================

        /** 最近点采样点数。 */
        static constexpr std::size_t k_sampleCount = 100;

        /** 三分搜索精化迭代次数。 */
        static constexpr int k_refineIterations = 5;

        /** 三分搜索精化搜索半径（占参数范围的比率）。 */
        static constexpr double k_refineRadius = 0.02;

    private:
        /** 曲线轨道所有权。 */
        std::unique_ptr<CurveBase> m_curve{};

        /**
         * 实体参数映射。
         * key: 实体 ID，value: 曲线参数 t。
         * 用于追踪每个实体在曲线上的当前位置。
         */
        std::unordered_map<std::uint64_t, double> m_entityParamMap{};

        // ==================== 微调参数 ====================

        /**
         * 位置微调阈值（米）。
         * 实体偏离轨道距离超过此值时触发位置回拉。
         * 默认 0.01 m。
         */
        double m_positionTolerance{0.01};

        /**
         * 速度微调阈值（m/s）。
         * 法向速度超过此值时触发法向速度衰减。
         * 默认 0.1 m/s。
         */
        double m_velocityTolerance{0.1};

        /**
         * 角速度微调阈值（弧度）。
         * 朝向偏差超过此值时触发角速度修正。
         * 默认 0.05 rad（约 3°）。
         */
        double m_angularTolerance{0.05};

        /**
         * 位置回拉系数。
         * 实际回拉量 = 偏差 × 回拉系数。
         * 默认 0.3，即每次修正拉回 30% 的偏移距离。
         */
        double m_pullBackFactor{0.3};

        /**
         * 首次拉回系数。
         * 实体的首次约束求解时使用此系数。
         * 默认 0.8，快速将实体拉入轨道。
         */
        double m_firstSnapFactor{0.8};

        /**
         * 法向速度衰减系数。
         * 法向速度衰减量 = 法向速度 × 衰减系数。
         * 默认 0.5，即削减 50% 的法向速度分量。
         */
        double m_normalDampingFactor{0.5};

        // ==================== 摩擦力预留 ====================

        /**
         * @brief 摩擦系数（预留，当前未使用）。
         * @details 预留用于后续版本的动/静摩擦力实现。
         *
         * TODO: [Friction] 摩擦力实现路径：
         * 1. 在 applyConstraintForces 中，获取切向力分量 F_parallel
         * 2. 计算最大静摩擦力 F_friction_max = m_frictionCoeff * ‖F_normal‖
         * 3. 若 ‖F_parallel‖ ≤ F_friction_max，用静摩擦力完全抵消切向力
         * 4. 否则，施加动摩擦力 F_dynamic = -m_frictionCoeff * ‖F_normal‖ * T̂
         * 5. 注意修正力矩 τ_friction = r × F_friction（若作用点偏离质心）
         *
         * TODO: [Friction] applyVelocityConstraint 中的摩擦：
         * 在法向速度衰减后，若切向速度接近零，可施加静摩擦使实体停在轨道上。
         *
         * TODO: [Friction] 区分静/动摩擦系数（m_staticFriction / m_dynamicFriction）
         * 当前仅使用单一 m_frictionCoeff。
         */
        double m_frictionCoeff{0.0};
    };
} // egret

#endif //EGRET_PHYSICS_SLIDING_RAIL_H
