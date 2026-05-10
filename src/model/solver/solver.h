//
// 由 GitHub Copilot 于 2026/4/25 创建。
//

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

    /**
     * @brief 模型层固定步长求解器的抽象接口。
     *
     * 约定：
     * 1. 接收一个场景快照和一个固定 dt。
     * 2. 通过非拥有式句柄原地修改场景状态。
     * 3. 返回本帧统计信息。
     */
    class SolverBase
    {
    public:
        /** 接口安全所需的虚析构函数。 */
        virtual ~SolverBase() = default;

        /**
         * @brief 对当前世界快照执行一次固定步长求解。
         * @param scene 可变场景快照适配器。
         * @param dt 固定步长，单位为秒。
         * @return 每步统计与附加元信息。
         */
        [[nodiscard]] virtual SolverStepResult step(SolverSceneSnapshotBase& scene, double dt) = 0;

        /**
         * @brief 替换运行时配置。
         * @param config 新的求解器配置。
         */
        virtual void setConfig(const SolverConfig& config) = 0;

        /**
         * @brief 返回当前运行时配置。
         * @return 当前生效配置的只读引用。
         */
        [[nodiscard]] virtual const SolverConfig& getConfig() const = 0;
    };

    /**
     * @brief 带策略扩展点的流水线式求解器声明。
     *
     * 该类先给出第一版实现所需的具体 API 形状。方法体可以放到 .cpp 文件中实现，
     * 而不改变对外契约。
     */
    class Solver final : public SolverBase
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

        /** 默认化的虚析构函数。 */
        ~Solver() override = default;

        /** 见 SolverBase::step。 */
        [[nodiscard]] SolverStepResult step(SolverSceneSnapshotBase& scene, double dt) override;

        /** 见 SolverBase::setConfig。 */
        void setConfig(const SolverConfig& config) override;

        /** 见 SolverBase::getConfig。 */
        [[nodiscard]] const SolverConfig& getConfig() const override;

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

    private:
        /**
         * @brief 第 1 阶段：维护外力累加器并应用场。
         * @param scene 可变场景快照。
         */
        void updateExternalForces(SolverSceneSnapshotBase& scene) const;

        /**
         * @brief 第 2 阶段：生成广相位候选对。
         * @param scene 场景快照。
         * @param pairs 输出候选对。
         * @param stats 可变步骤统计。
         */
        void runBroadPhase(const SolverSceneSnapshotBase& scene,
                           std::vector<SolverBodyPair>& pairs,
                           SolverStats& stats) const;

        /**
         * @brief 第 3 阶段：生成窄相位接触约束。
         * @param scene 场景快照。
         * @param pairs 广相位生成的候选对。
         * @param constraints 输出接触约束。
         * @param stats 可变步骤统计。
         */
        static void runNarrowPhase(const SolverSceneSnapshotBase& scene,
                            const std::vector<SolverBodyPair>& pairs,
                            std::vector<SolverContactConstraint>& constraints,
                            SolverStats& stats) ;

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
         * @brief 在积分后计算用于诊断的能量统计。
         * @param scene 场景快照。
         * @param stats 可变步骤统计。
         */
        static void updateEnergyStats(const SolverSceneSnapshotBase& scene, SolverStats& stats) ;

        /** 所有阶段共用的运行时配置。 */
        SolverConfig m_config{};

        /** 时间积分阶段使用的策略。 */
        std::unique_ptr<IntegratorStrategy> m_integrator;

        /** 广相位阶段使用的策略。 */
        std::unique_ptr<BroadPhaseStrategy> m_broadPhase;

        /** 接触解算阶段使用的策略。 */
        std::unique_ptr<ContactResolverStrategy> m_contactResolver;
    };
}

#endif // EGRET_PHYSICS_SOLVER_H
