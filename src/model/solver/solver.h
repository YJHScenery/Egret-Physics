/**
 * @file        solver.h
 * @brief       物理求解器头文件，定义固定步长求解器。
 * @details     定义 Solver 类，作为物理模拟固定步长求解器的实现，
 *              接收场景快照和固定 dt 进行物理计算。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-04
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Solver
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#ifndef EGRET_PHYSICS_SOLVER_H
#define EGRET_PHYSICS_SOLVER_H

#include <memory>
#include <vector>

#include "solver_scene_snapshot.h"

namespace egret
{
    class IntegratorStrategy;
    class BroadPhaseStrategy;
    class ContactResolverStrategy;
    class SleepStrategy;

    /**
     * @brief       带策略扩展点的流水线式求解器类。
     * @details     Solver 实现带策略扩展点的流水线式求解器。
     *              该类先给出第一版实现所需的具体 API 形状。
     *              采用策略模式，通过 IntegratorStrategy、BroadPhaseStrategy、
     *              ContactResolverStrategy 等策略组合实现求解流水线。
     *              支持运行时替换策略，灵活调整求解行为。
     *
     * @invariant   m_integrator 是积分策略实例
     * @invariant   m_broadPhase 是广相位策略实例
     * @invariant   m_contactResolver 是接触解算策略实例
     * @invariant   m_config 是求解器配置
     * @remark      Solver 是具体实现类，可以直接实例化
     * @see         IntegratorStrategy, BroadPhaseStrategy, ContactResolverStrategy
     */
    class Solver final
    {
    public:
        /**
         * @brief 使用配置和必需策略构造求解器。
         * @param config 初始运行时配置。
         * @param integrator 积分策略（第一版建议使用半隐式 Euler）。
         * @param broadPhase 广相位策略（AABB 候选对生成）。
         * @param contactResolver 接触解算策略（无摩擦法向冲量）。
         */
        Solver(const SolverConfig& config,
               std::unique_ptr<IntegratorStrategy> integrator,
               std::unique_ptr<BroadPhaseStrategy> broadPhase,
               std::unique_ptr<ContactResolverStrategy> contactResolver);

        /** 默认析构函数。 */
        ~Solver();

        /**
         * @brief 对当前世界快照执行一次固定步长求解。
         * @param scene 可变场景快照适配器。
         * @param dt 固定步长，单位为秒。
         * @return 每步统计与附加元信息。
         */
        [[nodiscard]] SolverStepResult step(SolverSceneSnapshotBase& scene, double dt);

        /**
         * @brief 替换运行时配置。
         * @param config 新的求解器配置。
         */
        void setConfig(const SolverConfig& config);

        /**
         * @brief 返回当前运行时配置。
         * @return 当前生效配置的只读引用。
         */
        [[nodiscard]] const SolverConfig& getConfig() const;

        /**
         * @brief 在运行时替换积分策略。
         * @param integrator 新的积分策略实例。
         */
        void setIntegrator(std::unique_ptr<IntegratorStrategy> integrator);

        /**
         * @brief 在运行时替换广相位策略。
         * @param broadPhase 新的广相位策略实例。
         */
        void setBroadPhase(std::unique_ptr<BroadPhaseStrategy> broadPhase);

        /**
         * @brief 在运行时替换接触解算策略。
         * @param contactResolver 新的接触解算策略实例。
         */
        void setContactResolver(std::unique_ptr<ContactResolverStrategy> contactResolver);

        /**
         * @brief 在运行时替换休眠策略。
         * @details 传入 nullptr 可关闭休眠机制。缺省为 nullptr（向后兼容）。
         * @param sleepStrategy 新的休眠策略实例。
         */
        void setSleepStrategy(std::unique_ptr<SleepStrategy> sleepStrategy);

        /**
         * @brief 返回当前休眠策略指针。
         * @return 休眠策略指针，未设置时返回 nullptr。
         */
        [[nodiscard]] SleepStrategy* getSleepStrategy();

        /**
         * @brief 渲染帧结束回调。
         * @details 转发给休眠策略的 onRenderFrameEnd()，用于结算渲染帧内的冻结计数。
         *          未设置休眠策略时为空操作。
         */
        void endFrame();

        /**
         * @brief 重置休眠策略的全部内部状态。
         * @details 场景重置或实体集合重建时调用，防止陈旧 ID 与新实体冲突。
         */
        void resetSleepStates();

        [[nodiscard]] std::string_view getIntegratorName() const;

        [[nodiscard]] std::string_view getBroadStrategyName() const;

    private:
        /**
         * @brief 更新所有物体的逆惯量矩阵缓存。
         * @param scene 可变场景快照。
         */
        void updateBodyInertiaCache(SolverSceneSnapshotBase& scene) const;

        /**
         * @brief 第 1 阶段：维护外力累加器并应用场。
         * @param scene 可变场景快照。
         */
        void updateExternalForces(SolverSceneSnapshotBase& scene) const;

        /**
         * @brief 第 1.5 阶段：应用约束力（如弹簧力）到实体力累加器。
         * @details 在 updateExternalForces 之后、积分之前调用。
         *          力型约束（Spring 等）通过此接口将约束力注入实体，
         *          与场的力一起在积分阶段被处理。
         * @param scene 可变场景快照。
         * @param dt 固定步长。
         * @param stats 可变步骤统计。
         */
        void applyConstraintForces(SolverSceneSnapshotBase& scene, [[maybe_unused]] double dt, [[maybe_unused]] SolverStats& stats) const;

        /**
         * @brief 第 2 阶段：生成广相位候选对。
         * @param scene 场景快照。
         * @param pairs 输出候选对。
         * @param stats 可变步骤统计。
         */
        void runBroadPhase(const SolverSceneSnapshotBase& scene, std::vector<SolverBodyPair>& pairs,
                           SolverStats& stats) const;

        // /**
        //  * @brief 第 3 阶段：生成窄相位接触约束。
        //  * @param scene 场景快照。
        //  * @param pairs 广相位生成的候选对。
        //  * @param constraints 输出接触约束。
        //  * @param stats 可变步骤统计。
        //  */
        // static void runNarrowPhase(const SolverSceneSnapshotBase& scene,
        //                            const std::vector<SolverBodyPair>& pairs,
        //                            std::vector<SolverContactConstraint>& constraints,
        //                            SolverStats& stats);

        /**
         * @brief 第 3 阶段（CCD版本）：生成窄相位接触约束（基于连续碰撞检测）。
         * @param scene 场景快照。
         * @param pairs 广相位生成的候选对。
         * @param dt 固定步长。
         * @param constraints 输出接触约束。
         * @param stats 可变步骤统计。
         */
        static void runNarrowPhaseCcd(const SolverSceneSnapshotBase& scene,
                                      const std::vector<SolverBodyPair>& pairs,
                                      double dt,
                                      std::vector<SolverContactConstraint>& constraints,
                                      SolverStats& stats);

        /**
         * @brief 第 3 阶段（TOI事件队列版本）：收集所有CCD碰撞事件。
         * @param scene 场景快照。
         * @param pairs 广相位生成的候选对。
         * @param dt 固定步长。
         * @param events 输出碰撞事件列表（按TOI排序）。
         * @param stats 可变步骤统计。
         */
        static void runNarrowPhaseCcdEvents(const SolverSceneSnapshotBase& scene,
                                            const std::vector<SolverBodyPair>& pairs,
                                            double dt,
                                            std::vector<CcdCollisionEvent>& events,
                                            SolverStats& stats);

        /**
         * @brief 使用TOI事件队列执行时间步进求解。
         * @param scene 可变场景快照。
         * @param dt 固定步长。
         * @param events 碰撞事件列表。
         * @param stats 可变步骤统计。
         */
        void stepWithToiQueue(SolverSceneSnapshotBase& scene,
                              double dt,
                              std::vector<CcdCollisionEvent>& events,
                              SolverStats& stats) const;

        /**
         * @brief 第 4 阶段：接触解算（法向冲量 + 位置修正）。
         * @param scene 可变场景快照。
         * @param dt 固定步长。
         * @param constraints 接触约束。
         * @param stats 可变步骤统计。
         */
        void resolveContacts(SolverSceneSnapshotBase& scene,
                             double dt,
                             const std::vector<SolverContactConstraint>& constraints,
                             SolverStats& stats) const;

        /**
         * @brief 第 5 阶段：使用所选积分策略推进状态。
         * @param scene 可变场景快照。
         * @param dt 固定步长。
         * @param stats 可变步骤统计。
         */
        void integrate(SolverSceneSnapshotBase& scene, double dt, SolverStats& stats) const;

        /**
         * @brief 在积分前应用速度约束和角速度约束。
         * @details 冲量型约束（速度修正、角速度修正）必须在积分之前执行，
         *          确保积分使用的是已修正的速度。
         * @param scene 可变场景快照。
         * @param dt 固定步长。
         * @param stats 可变步骤统计。
         */
        void applyVelocityAndAngularConstraints(SolverSceneSnapshotBase& scene, double dt, SolverStats& stats) const;

        /**
         * @brief 在积分后应用位置约束。
         * @details 位置修正（穿透恢复等）在积分之后执行，
         *          用于消除积分引入的微小穿透。
         * @param scene 可变场景快照。
         * @param dt 固定步长。
         * @param stats 可变步骤统计。
         */
        void applyPositionConstraints(SolverSceneSnapshotBase& scene, double dt, SolverStats& stats) const;

        /**
         * @brief 在积分后计算用于诊断的能量统计。
         * @param scene 场景快照。
         * @param stats 可变步骤统计。
         */
        static void updateEnergyStats(const SolverSceneSnapshotBase& scene, SolverStats& stats);

        /** 所有阶段共用的运行时配置。 */
        SolverConfig m_config{};

        /** 时间积分阶段使用的策略。 */
        std::unique_ptr<IntegratorStrategy> m_integrator;

        /** 广相位阶段使用的策略。 */
        std::unique_ptr<BroadPhaseStrategy> m_broadPhase;

        /** 接触解算阶段使用的策略。 */
        std::unique_ptr<ContactResolverStrategy> m_contactResolver;

        /** 休眠判定阶段使用的策略（缺省 nullptr 表示禁用休眠）。 */
        std::unique_ptr<SleepStrategy> m_sleepStrategy;
    };
}

#endif // EGRET_PHYSICS_SOLVER_H
