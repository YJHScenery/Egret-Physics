/**
 * @file        kinetic_sleep_strategy.cpp
 * @brief       基于运动指标的休眠策略具体实现文件
 * @details     实现 KineticSleepStrategy 的冻结判定、冻结快照记录与受力比对解冻逻辑。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-08-31
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Strategy
 * @defgroup    SleepStrategy
 */

#include "kinetic_sleep_strategy.h"

#include <algorithm>
#include <format>
#include <string>
#include <tuple>
#include <unordered_set>
#include <vector>
#include <numeric>
#include <map>
#include <limits>

#include "async_logger.h"
#include "physical_entity.h"
#include "sleep_metric.h"
#include "solver_scene_snapshot.h"
#include "solver_types.h"

namespace egret
{
    KineticSleepStrategy::KineticSleepStrategy(SleepConfig config)
        : m_config(config), m_metric(makeSleepMetric(config.metricType))
    {
    }

    KineticSleepStrategy::~KineticSleepStrategy() = default;

    void KineticSleepStrategy::evaluate(SolverSceneSnapshotBase &scene, const double dt)
    {
        std::ignore = dt;

        if (!m_config.enabled) {
            return;
        }

        auto bodies = scene.getBodies();

        // 收集本步存活实体 ID，用于清理已被删除实体的陈旧状态
        std::unordered_set<std::uint64_t> liveIds;
        liveIds.reserve(bodies.size());

        for (SolverBodyHandle &body : bodies) {
            if (body.entity == nullptr) {
                continue;
            }

            PhysicalEntity *const entity = body.entity;
            const std::uint64_t id = entity->getId();
            liveIds.insert(id);

            // 固定实体（质量 <= 0）不参与休眠判定
            if (entity->isFixed()) {
                continue;
            }

            if (isAsleep(id)) {
                // 冻结状态：每个计算帧检查受力情况，与冻结时基线比对
                checkWakeUp(entity);
                continue;
            }

            // 清醒状态：检查运动指标是否符合冻结候选条件
            SleepCountState &state = m_states[id];
            state.entity = entity;
            state.frameEvaluated = true;
            if (!isMetricBelowThreshold(*entity)) {
                state.frameAllPassed = false;
            }
        }

        // 清理陈旧状态：实体已从场景中删除时同步移除其计数与快照
        std::erase_if(m_states, [&liveIds](const auto &entry)
        {
            return !liveIds.contains(entry.first);
        });
        std::erase_if(m_frozenSnapshots, [&liveIds](const auto &entry)
        {
            return !liveIds.contains(entry.first);
        });
    }

    void KineticSleepStrategy::evaluateSupported(SolverSceneSnapshotBase& scene, double dt,
        std::span<const SolverContactConstraint> contacts)
    {
        m_contactTiming=true;
        if (!m_config.enabled || dt<=0.0) return;
        auto bodies=scene.getBodies();
        std::vector<std::size_t> parent(bodies.size());
        std::iota(parent.begin(),parent.end(),0);
        /** @brief 查找动态实体所在支撑岛。 */
        auto root=[&](std::size_t i) { while (parent[i]!=i) { parent[i]=parent[parent[i]]; i=parent[i]; } return i; };
        for (const auto& c:contacts) {
            if (c.bodyAIndex>=bodies.size() || c.bodyBIndex>=bodies.size()) continue;
            auto* a=bodies[c.bodyAIndex].entity; auto* b=bodies[c.bodyBIndex].entity;
            if (a && b && !a->isFixed() && !b->isFixed()) parent[root(c.bodyBIndex)]=root(c.bodyAIndex);
        }
        std::map<std::size_t,std::vector<std::size_t>> islands;
        for (std::size_t i=0;i<bodies.size();++i) if (bodies[i].entity && !bodies[i].entity->isFixed()) islands[root(i)].push_back(i);
        std::unordered_set<std::uint64_t> live;
        for (const auto& [island,nodes]:islands) {
            bool supported=false, low=true;
            std::uint64_t signature=1469598103934665603ull;
            for (const auto& c:contacts) {
                if (c.bodyAIndex>=bodies.size() || c.bodyBIndex>=bodies.size()) continue;
                auto* a=bodies[c.bodyAIndex].entity; auto* b=bodies[c.bodyBIndex].entity;
                if (!a || !b) continue;
                if ((!a->isFixed() && root(c.bodyAIndex)==island) || (!b->isFixed() && root(c.bodyBIndex)==island)) {
                    supported=supported || a->isFixed() || b->isFixed();
                    signature^=a->getId()+31*b->getId(); signature*=1099511628211ull;
                }
            }
            for (auto index:nodes) {
                auto* entity=bodies[index].entity; const auto id=entity->getId(); live.insert(id);
                const bool wasAsleep=entity->isAsleep();
                if (wasAsleep) checkWakeUp(entity);
                if (wasAsleep && !entity->isAsleep()) low=false;
                if (!isMetricBelowThreshold(*entity)) low=false;
                // 自由空间允许零外力休眠；重力下最高点没有支撑，不能冻结。
                if (!supported && entity->getJoinForce().norm()>m_config.wakeUpAbsoluteForceThreshold) low=false;
                if (m_supportSignatures.contains(id) && m_supportSignatures[id]!=signature) low=false;
                m_supportSignatures[id]=signature;
            }
            double duration=std::numeric_limits<double>::infinity();
            for (auto index:nodes) {
                auto* entity=bodies[index].entity; const auto id=entity->getId();
                if (!low) {
                    m_stableSeconds[id]=0.0;
                    if (entity->isAsleep()) { entity->setAsleep(false); m_frozenSnapshots.erase(id); }
                } else m_stableSeconds[id]+=dt;
                duration=std::min(duration,m_stableSeconds[id]);
            }
            if (low && duration>=m_config.frozenFrameCount/60.0)
                for (auto index:nodes) if (!bodies[index].entity->isAsleep()) freezeEntity(bodies[index].entity);
        }
        std::erase_if(m_stableSeconds,[&](const auto& item) { return !live.contains(item.first); });
        std::erase_if(m_supportSignatures,[&](const auto& item) { return !live.contains(item.first); });
        std::erase_if(m_frozenSnapshots,[&](const auto& item) { return !live.contains(item.first); });
    }

    void KineticSleepStrategy::onRenderFrameEnd()
    {
        if (m_contactTiming) return;
        if (!m_config.enabled) {
            return;
        }

        std::vector<std::uint64_t> idsToFreeze;
        idsToFreeze.reserve(m_states.size());

        for (auto &[id, state] : m_states) {
            if (!state.frameEvaluated) {
                // 本渲染帧没有评估过任何计算帧，保持当前计数不变
                continue;
            }

            if (state.frameAllPassed) {
                // 本渲染帧内所有计算帧的指标均低于阈值 → 冻结计数 +1
                ++state.passedRenderFrames;
                if (state.passedRenderFrames >= m_config.frozenFrameCount) {
                    idsToFreeze.push_back(id);
                    continue;
                }
            }
            else {
                // 任一计算帧未通过 → 连续计数清零，下一渲染帧重新累计
                state.passedRenderFrames = 0;
            }

            // 重置本渲染帧标志，准备下一渲染帧
            state.frameAllPassed = true;
            state.frameEvaluated = false;
        }

        for (const std::uint64_t id : idsToFreeze) {
            const auto it = m_states.find(id);
            if (it != m_states.end() && it->second.entity != nullptr) {
                freezeEntity(it->second.entity);
            }
        }
    }

    bool KineticSleepStrategy::isAsleep(const std::uint64_t id) const
    {
        return m_frozenSnapshots.contains(id);
    }

    void KineticSleepStrategy::setConfig(const SleepConfig &config)
    {
        m_config = config;
        m_metric = makeSleepMetric(config.metricType);
    }

    const SleepConfig &KineticSleepStrategy::getConfig() const
    {
        return m_config;
    }

    void KineticSleepStrategy::reset()
    {
        m_states.clear();
        m_frozenSnapshots.clear();
        m_stableSeconds.clear();
        m_supportSignatures.clear();
        m_contactTiming=false;
    }

    std::string_view KineticSleepStrategy::name() const
    {
        static const std::string strategyName{"KineticSleep"};
        return strategyName;
    }

    bool KineticSleepStrategy::isMetricBelowThreshold(PhysicalEntity &entity) const
    {
        const double metricValue = m_metric->compute(entity);

        switch (m_config.metricType) {
        case SleepMetricType::SpeedSquared:
            return metricValue < m_config.speedSquaredThreshold;
        case SleepMetricType::KineticEnergy:
            return metricValue < m_config.kineticEnergyThreshold;
        case SleepMetricType::Momentum:
            return metricValue < m_config.momentumThreshold;
        }

        return false;
    }

    void KineticSleepStrategy::freezeEntity(PhysicalEntity *entity)
    {
        const std::uint64_t id = entity->getId();

        const Eigen::Vector3d joinForce = entity->getJoinForce();

        FrozenSnapshot snapshot{};
        snapshot.position = entity->getPosition();
        snapshot.velocity = entity->getVelocity();
        snapshot.acceleration = joinForce / entity->getMass();
        snapshot.force = joinForce;

        // 冻结：清零线速度与角速度，实体静止在某处
        entity->setAsleep(true);

        m_frozenSnapshots[id] = snapshot;
        m_states.erase(id);

        LOG_INFO(std::format(
            "SleepStrategy: entity [{}] frozen. position=({:.4f}, {:.4f}, {:.4f}) "
            "velocity=({:.4f}, {:.4f}, {:.4f}) acceleration=({:.4f}, {:.4f}, {:.4f}) "
            "force=({:.4f}, {:.4f}, {:.4f})",
            id,
            snapshot.position.x(), snapshot.position.y(), snapshot.position.z(),
            snapshot.velocity.x(), snapshot.velocity.y(), snapshot.velocity.z(),
            snapshot.acceleration.x(), snapshot.acceleration.y(), snapshot.acceleration.z(),
            snapshot.force.x(), snapshot.force.y(), snapshot.force.z()));
    }

    void KineticSleepStrategy::checkWakeUp(PhysicalEntity *entity)
    {
        const std::uint64_t id = entity->getId();
        const auto it = m_frozenSnapshots.find(id);
        if (it == m_frozenSnapshots.end()) {
            return;
        }

        // 状态同步：若实体已被外部直接唤醒，清除快照避免不一致
        if (!entity->isAsleep()) {
            m_frozenSnapshots.erase(it);
            return;
        }

        const FrozenSnapshot snapshot = it->second;
        const Eigen::Vector3d currentForce = entity->getJoinForce();
        const double baselineNorm = snapshot.force.norm();
        const double diffNorm = (currentForce - snapshot.force).norm();

        bool shouldWake = false;
        if (baselineNorm > m_config.wakeUpAbsoluteForceThreshold) {
            // 正常情况：向量差模长超过基线模长的 wakeUpForceRatio（默认 1%）即解冻
            shouldWake = diffNorm > m_config.wakeUpForceRatio * baselineNorm;
        }
        else {
            // 基线近零兜底：避免 1% × 0 = 0 导致任意微扰都解冻
            shouldWake = diffNorm > m_config.wakeUpAbsoluteForceThreshold;
        }

        if (!shouldWake) {
            return;
        }

        entity->setAsleep(false);
        m_frozenSnapshots.erase(it);

        LOG_INFO(std::format(
            "SleepStrategy: entity [{}] woken up. forceDiffNorm={:.6f} "
            "baselineForce=({:.4f}, {:.4f}, {:.4f}) currentForce=({:.4f}, {:.4f}, {:.4f})",
            id, diffNorm,
            snapshot.force.x(), snapshot.force.y(), snapshot.force.z(),
            currentForce.x(), currentForce.y(), currentForce.z()));
    }
}
