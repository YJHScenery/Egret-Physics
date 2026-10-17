/**
 * @file        solver.cpp
 * @brief       物理求解器实现文件。
 * @details     实现 Solver 类的各项成员函数。
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

#include "solver.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <fstream>
#include <QDebug>
#include <qgenericmatrix.h>
#include <QString>

#include "field_base.h"
#include "constraints_base.h"
#include "particle.h"
#include "physical_entity.h"
#include "broad_phase_strategy/brute_force_broad_strategy.h"
#include "contact_strategy/frictionless_contact_resolver.h"
#include "integrator_strategy/semi_implicit_euler_integrator.h"
#include "sleep_strategy/sleep_strategy.h"
#include "ccd_solver_adapter.h"
#include <QMatrix3x3>

namespace egret
{
    namespace
    {
        /** CCD 事件允许的最大穿透深度（米），超过则钳制，防止退化 EPA 结果造成位置跳变。 */
        constexpr double kMaxCcdPenetration = 1.0;
    }

    // namespace
    // {
    //     constexpr double kAabbFallbackTolerance = 1e-6;
    //     constexpr double kAabbFallbackPenetration = 0.002;
    //
    //     bool buildAabbFallbackContact(const SolverBodyHandle& bodyA,
    //                                   const SolverBodyHandle& bodyB,
    //                                   ContactManifold& manifold)
    //     {
    //         if (bodyA.entity->getShape() == nullptr || bodyB.entity->getShape() == nullptr) {
    //             return false;
    //         }
    //
    //         const AABB aabbA = bodyA.entity->getShape()->getAABB(bodyA.entity->getTransformCR());
    //         const AABB aabbB = bodyB.entity->getShape()->getAABB(bodyB.entity->getTransformCR());
    //         const Eigen::Vector3d overlapMin = aabbA.min.cwiseMax(aabbB.min);
    //         const Eigen::Vector3d overlapMax = aabbA.max.cwiseMin(aabbB.max);
    //         const Eigen::Vector3d overlap = overlapMax - overlapMin;
    //
    //         if (overlap.x() < -kAabbFallbackTolerance || overlap.y() < -kAabbFallbackTolerance ||
    //             overlap.z() < -kAabbFallbackTolerance) {
    //             return false;
    //         }
    //
    //         const Eigen::Vector3d stableOverlap = overlap.cwiseMax(0.0);
    //         int axisIndex = 0;
    //         double minOverlap = stableOverlap.x();
    //         for (int index = 1; index < 3; ++index) {
    //             if (stableOverlap[index] < minOverlap) {
    //                 minOverlap = stableOverlap[index];
    //                 axisIndex = index;
    //             }
    //         }
    //
    //         Eigen::Vector3d normal = bodyB.entity->getTransformCR().getTranslation() - bodyA.entity->getTransformCR().
    //             getTranslation();
    //         if (!normal.allFinite() || normal.squaredNorm() < 1e-12) {
    //             normal = Eigen::Vector3d::UnitY();
    //         }
    //         else {
    //             normal.normalize();
    //         }
    //
    //         if (normal[axisIndex] < 0.0) {
    //             normal = -normal;
    //         }
    //
    //         ContactPoint contact{};
    //         contact.position = 0.5 * (overlapMin + overlapMax);
    //         contact.normal = normal;
    //         contact.penetration = std::max(minOverlap, kAabbFallbackPenetration);
    //         manifold.push_back(contact);
    //         return true;
    //     }
    // }

    Solver::Solver(const SolverConfig& config,
                   std::unique_ptr<IntegratorStrategy> integrator,
                   std::unique_ptr<BroadPhaseStrategy> broadPhase,
                   std::unique_ptr<ContactResolverStrategy> contactResolver) : m_config(config),
                                                                               m_integrator(std::move(integrator)),
                                                                               m_broadPhase(std::move(broadPhase)),
                                                                               m_contactResolver(
                                                                                   std::move(contactResolver))
    {

    }

    Solver::~Solver() = default;


    SolverStepResult Solver::step(SolverSceneSnapshotBase& scene, const double dt)
    {
        SolverStepResult result{};
        result.dt = dt;

        if (dt <= 0.0) {
            return result;
        }

        updateExternalForces(scene);
        applyConstraintForces(scene, dt, result.stats);
        if (m_sleepStrategy) {
            m_sleepStrategy->evaluate(scene,dt);
            for (auto& body:scene.getBodies())
                if (body.entity && !body.entity->isAsleep() && !body.entity->isFixed() && body.inverseMass<=0)
                    body.inverseMass=1.0/body.entity->getMass();
        }
        updateBodyInertiaCache(scene);

        std::vector<SolverBodyPair> pairs;
        std::vector<SolverContactConstraint> constraints;
        std::vector<CcdCollisionEvent> ccdEvents;
        std::vector<SolverBodyPair> resolvedPairs;

        if (m_config.enableBroadPhase) {
            runBroadPhase(scene, pairs, result.stats, dt);
        }

        if (m_config.enableNarrowPhase) {
            if (m_config.enableToiQueue) {
                // qDebug() << "enable toi queue";
                runNarrowPhaseCcdEvents(scene, pairs, dt, ccdEvents, result.stats);
            }
            else {
                runNarrowPhaseCcd(scene, pairs, dt, constraints, result.stats);
            }
        }

        if (m_config.enableToiQueue && !ccdEvents.empty()) {
            stepWithToiQueue(scene, dt, ccdEvents, result.stats, resolvedPairs);
        }
        else {
            if (m_config.enableContactResolution) {
                resolveContacts(scene, dt, constraints, result.stats);
                for (const auto& c : constraints) resolvedPairs.push_back({c.bodyAIndex,c.bodyBIndex});
            }

            // 速度/角速度约束必须在积分前执行，确保积分使用修正后的速度
            applyVelocityAndAngularConstraints(scene, dt, result.stats);

            if (m_config.enableIntegration) {
                integrate(scene, dt, result.stats);
            }
        }

        if (m_config.enableContactResolution && m_config.enableNarrowPhase) {
            updateBodyInertiaCache(scene);
            std::vector<SolverBodyPair> finalPairs;
            if (m_config.enableBroadPhase) runBroadPhase(scene, finalPairs, result.stats);
            std::vector<SolverContactConstraint> finalContacts;
            runNarrowPhaseCcd(scene, finalPairs, 1e-8, finalContacts, result.stats);
            std::erase_if(finalContacts, [](const auto& c) { return c.toi > 1e-10; });
            // 冲击恢复已在 TOI 阶段完成，步末仅补齐受力后的支撑速度。
            for (auto& contact : finalContacts) {
                if (std::any_of(resolvedPairs.begin(),resolvedPairs.end(),[&](const auto& pair) {
                    return pair.bodyAIndex==contact.bodyAIndex && pair.bodyBIndex==contact.bodyBIndex;
                })) contact.restitution = 0.0;
            }
            resolveContacts(scene, dt, finalContacts, result.stats);
            if (m_sleepStrategy) m_sleepStrategy->evaluateSupported(scene,dt,finalContacts);
        }

        // 位置约束在积分后执行，用于消除积分引入的微小穿透
        applyPositionConstraints(scene, dt, result.stats);
        updateEnergyStats(scene, result.stats);
        scene.advanceSimulationTime(dt);
        return result;
    }

    void Solver::setConfig(const SolverConfig& config)
    {
        m_config = config;
    }

    const SolverConfig& Solver::getConfig() const
    {
        return m_config;
    }

    void Solver::setIntegrator(std::unique_ptr<IntegratorStrategy> integrator)
    {
        m_integrator = std::move(integrator);
    }

    void Solver::setBroadPhase(std::unique_ptr<BroadPhaseStrategy> broadPhase)
    {
        m_broadPhase = std::move(broadPhase);
    }

    void Solver::setContactResolver(std::unique_ptr<ContactResolverStrategy> contactResolver)
    {
        m_contactResolver = std::move(contactResolver);
    }

    void Solver::setSleepStrategy(std::unique_ptr<SleepStrategy> sleepStrategy)
    {
        m_sleepStrategy = std::move(sleepStrategy);
    }

    SleepStrategy* Solver::getSleepStrategy()
    {
        return m_sleepStrategy.get();
    }

    void Solver::endFrame()
    {
        if (m_sleepStrategy != nullptr) {
            m_sleepStrategy->onRenderFrameEnd();
        }
    }

    void Solver::resetSleepStates()
    {
        if (m_sleepStrategy != nullptr) {
            m_sleepStrategy->reset();
        }
    }

    std::string_view Solver::getIntegratorName() const
    {
        return m_integrator->name();
    }

    std::string_view Solver::getBroadStrategyName() const
    {
        return m_broadPhase->name();
    }

    void Solver::updateBodyInertiaCache(SolverSceneSnapshotBase& scene) const
    {
        auto bodies = scene.getBodies();
        for (SolverBodyHandle& body : bodies) {
            if (body.entity == nullptr || body.inverseMass <= 0.0) {
                body.inverseInertiaTensor.setZero();
                continue;
            }

            auto shape = body.entity->getShape();
            if (!shape) {
                body.inverseInertiaTensor.setZero();
                continue;
            }

            const double mass = 1.0 / body.inverseMass;
            const Eigen::Matrix3d I_local = shape->getInertiaTensor(mass);
            const Eigen::Matrix3d R = body.entity->getTransformCR().getRotationMatrix();
            const Eigen::Matrix3d I_world = R * I_local * R.transpose();
            // 细棒等形状存在零主惯量，使用对称伪逆，不能直接逆矩阵。
            body.inverseInertiaTensor.setZero();
            if (!I_world.allFinite()) continue;
            Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> decomposition(0.5*(I_world+I_world.transpose()));
            if (decomposition.info()!=Eigen::Success) continue;
            const auto values=decomposition.eigenvalues();
            const double cutoff=std::max(1e-15,values.cwiseAbs().maxCoeff()*1e-12);
            Eigen::Vector3d inverse=Eigen::Vector3d::Zero();
            for (int k=0;k<3;++k) if (values[k]>cutoff) inverse[k]=1.0/values[k];
            body.inverseInertiaTensor=decomposition.eigenvectors()*inverse.asDiagonal()*decomposition.eigenvectors().transpose();
        }
    }

    void Solver::updateExternalForces(SolverSceneSnapshotBase& scene) const
    {
        const auto bodies = scene.getBodies();
        const auto fields = scene.getFields();

        for (const SolverBodyHandle& body : bodies) {
            if (body.entity == nullptr) {
                continue;
            }

            if (m_config.clearForcesBeforeFieldApply) {
                const std::size_t forceCount = body.entity->getForcesCR().size();
                for (std::size_t index = 0; index < forceCount; ++index) {
                    body.entity->removeForce(0);
                }
            }

            if (m_config.clearTorquesBeforeFieldApply) {
                body.entity->clearTorques();
            }

            if (!m_config.applyFieldsEachStep) {
                continue;
            }

            for (FieldBase* field : fields) {
                if (field != nullptr) {
                    field->applyToEntity(body.entity);
                }

                // if (auto fieldEntityPtr{dynamic_cast<Particle*>(field)}) {
                //     const Eigen::Vector3d& pos{fieldEntityPtr->getPositionCR()};
                //     qDebug() << "Position: " << pos.x() << pos.y() << pos.z();
                // }
            }
        }
    }

    void Solver::applyConstraintForces(SolverSceneSnapshotBase& scene,
                                       [[maybe_unused]] const double dt, [[maybe_unused]] SolverStats& stats) const
    {
        const auto constraints = scene.getConstraints();

        for (ConstraintsBase* constraint : constraints)
        {
            if (constraint == nullptr || !constraint->isEnabled())
            {
                continue;
            }

            constraint->applyConstraintForces();
        }
    }

    void Solver::runBroadPhase(const SolverSceneSnapshotBase& scene,
                               std::vector<SolverBodyPair>& pairs,
                               SolverStats& stats, double horizon) const
    {
        if (m_broadPhase != nullptr) {
            m_broadPhase->buildCandidatePairs(scene, m_config, pairs, stats);
            stats.broadPhasePairCount = pairs.size();
        }
        if (horizon <= 0.0) return;
        // 现有空间索引只查询当前包围盒，补充扫掠候选以免快速碰撞在步末才被发现。
        const auto bodies=scene.getBodies();
        std::vector<AABB> swept(bodies.size());
        std::vector<bool> valid(bodies.size(),false);
        for (std::size_t i=0;i<bodies.size();++i) {
            const auto& body=bodies[i];
            if (!body.entity || !body.enableCollision || !body.entity->getShape()) continue;
            auto bounds=body.entity->getShape()->getAABB(body.entity->getTransformCR());
            if (!bounds.min.allFinite() || !bounds.max.allFinite()) continue;
            const Eigen::Vector3d displacement=horizon*body.entity->getVelocity();
            const Eigen::Vector3d acceleration=body.inverseMass*body.entity->getJoinForce();
            const double radius=0.5*(bounds.max-bounds.min).norm();
            const double angularSweep=std::min(2.0,horizon*body.entity->getAngularVelocity().norm())*radius;
            const Eigen::Vector3d padding=0.5*horizon*horizon*acceleration.cwiseAbs()
                +Eigen::Vector3d::Constant(angularSweep+2e-4);
            swept[i].min=bounds.min+displacement.cwiseMin(0.0)-padding;
            swept[i].max=bounds.max+displacement.cwiseMax(0.0)+padding;
            valid[i]=true;
        }
        std::set<std::pair<std::size_t,std::size_t>> unique;
        for (const auto& pair:pairs) unique.emplace(std::min(pair.bodyAIndex,pair.bodyBIndex),std::max(pair.bodyAIndex,pair.bodyBIndex));
        for (std::size_t i=0;i<bodies.size();++i) for (std::size_t j=i+1;j<bodies.size();++j) {
            if (!valid[i] || !valid[j] || (bodies[i].entity->isFixed() && bodies[j].entity->isFixed())) continue;
            if ((swept[i].max.array()>=swept[j].min.array()).all() &&
                (swept[j].max.array()>=swept[i].min.array()).all()) unique.emplace(i,j);
        }
        pairs.clear();
        for (const auto& [a,b]:unique) pairs.push_back({a,b});
        stats.broadPhasePairCount=pairs.size();
    }

    // void Solver::runNarrowPhase(const SolverSceneSnapshotBase& scene,
    //                             const std::vector<SolverBodyPair>& pairs,
    //                             std::vector<SolverContactConstraint>& constraints,
    //                             SolverStats& stats)
    // {
    //     const auto bodies = scene.getBodies();
    //     stats.narrowPhaseTestCount = pairs.size();
    //
    //     for (const SolverBodyPair& pair : pairs) {
    //         if (pair.bodyAIndex >= bodies.size() || pair.bodyBIndex >= bodies.size()) {
    //             continue;
    //         }
    //
    //         const SolverBodyHandle& bodyA = bodies[pair.bodyAIndex];
    //         const SolverBodyHandle& bodyB = bodies[pair.bodyBIndex];
    //
    //         if (bodyA.entity->getShape() == nullptr || bodyB.entity->getShape() == nullptr) {
    //             continue;
    //         }
    //
    //         auto manifoldOpt = runGjkEpaOnEntities(bodyA.entity, bodyB.entity);
    //         bool collided = manifoldOpt.has_value();
    //         ContactManifold manifold{};
    //
    //         if (collided) {
    //             manifold = std::move(manifoldOpt.value());
    //         } else {
    //             collided = buildAabbFallbackContact(bodyA, bodyB, manifold);
    //         }
    //         if (!collided) {
    //             continue;
    //         }
    //
    //         for (const ContactPoint& contact : manifold) {
    //             if (!contact.position.allFinite() || !contact.normal.allFinite() ||
    //                 !std::isfinite(contact.penetration) || contact.penetration <= 0.0 ||
    //                 contact.normal.squaredNorm() < 1e-12) {
    //                 continue;
    //             }
    //             const double restitution = std::isfinite(
    //                                            std::min(bodyA.entity->getRestitution(), bodyB.entity->getRestitution()))
    //                                            ? std::clamp(
    //                                                std::min(bodyA.entity->getRestitution(),
    //                                                         bodyB.entity->getRestitution()), 0.0, 1.0)
    //                                            : 0.0;
    //             constraints.push_back(SolverContactConstraint{
    //                 .bodyAIndex = pair.bodyAIndex,
    //                 .bodyBIndex = pair.bodyBIndex,
    //                 .contactPosition = contact.position,
    //                 .contactNormal = contact.normal,
    //                 .penetration = contact.penetration,
    //                 .restitution = restitution
    //             });
    //         }
    //     }
    //
    //     stats.contactConstraintCount = constraints.size();
    // }

    void Solver::runNarrowPhaseCcd(const SolverSceneSnapshotBase& scene,
                                   const std::vector<SolverBodyPair>& pairs,
                                   double dt,
                                   std::vector<SolverContactConstraint>& constraints,
                                   SolverStats& stats)
    {
        const auto bodies = scene.getBodies();
        stats.narrowPhaseTestCount = pairs.size();

        for (const SolverBodyPair& pair : pairs) {
            if (pair.bodyAIndex >= bodies.size() || pair.bodyBIndex >= bodies.size()) {
                continue;
            }

            const SolverBodyHandle& bodyA = bodies[pair.bodyAIndex];
            const SolverBodyHandle& bodyB = bodies[pair.bodyBIndex];

            if (bodyA.entity == nullptr || bodyB.entity == nullptr ||
                bodyA.entity->getShape() == nullptr || bodyB.entity->getShape() == nullptr) {
                continue;
            }

            auto resultOpt = runCcdOnEntities(bodyA.entity, bodyB.entity, dt);

            if (!resultOpt.has_value()) {
                continue;
            }

            constexpr double kEpsTime = 1e-10;

            const AdapterCcdResult& result = resultOpt.value();
            // 拒绝 TOI 过于接近 dt 的碰撞，与旧版 continuousCollideGeneric 行为一致
            if (result.timeOfImpact < 0.0 || result.timeOfImpact > dt - kEpsTime) {
                continue;
            }

            if (!result.normal.allFinite() || result.normal.squaredNorm() < 1e-12 ||
                !result.contactPoint.allFinite()) {
                continue;
            }

            // 穿透深度净化：非有限值直接丢弃；异常大值钳制，防止位置修正跳变
            if (!std::isfinite(result.penetrationDepth)) {
                continue;
            }
            const double penetrationDepth = std::clamp(result.penetrationDepth, 0.0, kMaxCcdPenetration);

            const double restitution = std::isfinite(
                                           std::min(bodyA.entity->getRestitution(), bodyB.entity->getRestitution()))
                                           ? std::clamp(
                                               std::min(bodyA.entity->getRestitution(),
                                                        bodyB.entity->getRestitution()), 0.0, 1.0)
                                           : 0.0;
            const auto manifold = result.manifold.empty()
                ? std::vector<CcdContactData>{{result.contactPoint,result.normal,penetrationDepth}}
                : result.manifold;
            for (const auto& contact : manifold) {
                constraints.push_back(SolverContactConstraint{
                    .bodyAIndex = pair.bodyAIndex,
                    .bodyBIndex = pair.bodyBIndex,
                    .contactPosition = contact.position,
                    .contactNormal = contact.normal,
                    .penetration = std::clamp(contact.penetration,0.0,kMaxCcdPenetration),
                    .restitution = restitution,
                    .toi = result.timeOfImpact
                });
            }
        }

        stats.contactConstraintCount = constraints.size();
    }

    void Solver::runNarrowPhaseCcdEvents(const SolverSceneSnapshotBase& scene,
                                         const std::vector<SolverBodyPair>& pairs,
                                         double dt,
                                         std::vector<CcdCollisionEvent>& events,
                                         SolverStats& stats)
    {
        const auto bodies = scene.getBodies();
        stats.narrowPhaseTestCount = pairs.size();
        events.clear();

        for (const SolverBodyPair& pair : pairs) {
            if (pair.bodyAIndex >= bodies.size() || pair.bodyBIndex >= bodies.size()) {
                continue;
            }

            const SolverBodyHandle& bodyA = bodies[pair.bodyAIndex];
            const SolverBodyHandle& bodyB = bodies[pair.bodyBIndex];

            if (bodyA.entity == nullptr || bodyB.entity == nullptr ||
                bodyA.entity->getShape() == nullptr || bodyB.entity->getShape() == nullptr) {
                continue;
            }

            auto resultOpt = runCcdOnEntities(bodyA.entity, bodyB.entity, dt);

            if (!resultOpt.has_value()) {
                continue;
            }

            constexpr double kEpsTime = 1e-10;

            const AdapterCcdResult& result = resultOpt.value();
            if (result.timeOfImpact < 0.0 || result.timeOfImpact > dt - kEpsTime) {
                continue;
            }

            if (!result.normal.allFinite() || result.normal.squaredNorm() < 1e-12 ||
                !result.contactPoint.allFinite()) {
                continue;
            }

            // 穿透深度净化：非有限值直接丢弃；异常大值钳制，防止位置修正跳变
            if (!std::isfinite(result.penetrationDepth)) {
                continue;
            }
            const double penetrationDepth = std::clamp(result.penetrationDepth, 0.0, kMaxCcdPenetration);

            const double restitution = std::isfinite(std::min(bodyA.entity->getRestitution(),
                                                              bodyB.entity->getRestitution()))
                                           ? std::clamp(std::min(bodyA.entity->getRestitution(),
                                                                 bodyB.entity->getRestitution()), 0.0, 1.0)
                                           : 0.0;

            CcdContactData contactData{
                .position = result.contactPoint,
                .normal = result.normal,
                .penetration = penetrationDepth
            };

            events.push_back(CcdCollisionEvent{
                .toi = result.timeOfImpact,
                .bodyAIndex = pair.bodyAIndex,
                .bodyBIndex = pair.bodyBIndex,
                .manifold = result.manifold.empty() ? std::vector<CcdContactData>{contactData} : result.manifold,
                .restitution = restitution
            });
        }

        std::sort(events.begin(), events.end());
        stats.contactConstraintCount = events.size();
    }

    void Solver::stepWithToiQueue(SolverSceneSnapshotBase& scene,
                                  double dt,
                                  std::vector<CcdCollisionEvent>& events,
                                  SolverStats& stats, std::vector<SolverBodyPair>& resolvedPairs) const
    {
        constexpr double kTimeEpsilon = 1e-10;
        constexpr double kBatchTimeEpsilon = 1e-6;
        constexpr std::size_t kMaxSubsteps = 64;

        double currentTime = 0.0;
        double remainingTime = dt;
        std::size_t substepCount = 0;

        while (remainingTime > kTimeEpsilon && !events.empty() && substepCount < kMaxSubsteps) {
            const double earliestToi = events.front().toi;
            const double timeToCollision = earliestToi - currentTime;

            if (timeToCollision > remainingTime + kTimeEpsilon) {
                integrate(scene, remainingTime, stats);
                currentTime += remainingTime;
                // 关键：将剩余时间清零，避免循环结束后的最终积分重复推进同一段时间
                remainingTime = 0.0;
                break;
            }

            if (timeToCollision > kTimeEpsilon) {
                integrate(scene, timeToCollision, stats);
                currentTime = earliestToi;
                remainingTime = dt - currentTime;
            }

            std::vector<SolverContactConstraint> currentConstraints;
            auto it = events.begin();
            while (it != events.end() && std::abs(it->toi - earliestToi) < kBatchTimeEpsilon) {
                for (const CcdContactData& contactData : it->manifold) {
                    currentConstraints.push_back(SolverContactConstraint{
                        .bodyAIndex = it->bodyAIndex,
                        .bodyBIndex = it->bodyBIndex,
                        .contactPosition = contactData.position,
                        .contactNormal = contactData.normal,
                        .penetration = contactData.penetration,
                        .restitution = it->restitution,
                        .toi = it->toi
                    });
                }
                it = events.erase(it);
            }

            if (!currentConstraints.empty()) {
                // 同一时刻把已有支撑边一并加入，冲击不能只看到事件队列中的新边。
                std::vector<SolverBodyPair> supportPairs;
                runBroadPhase(scene,supportPairs,stats,1e-8);
                std::vector<SolverContactConstraint> supportContacts;
                runNarrowPhaseCcd(scene,supportPairs,1e-8,supportContacts,stats);
                const auto eventContactCount=currentConstraints.size();
                for (auto& contact:supportContacts) {
                    if (contact.toi>kTimeEpsilon) continue;
                    const bool alreadyPresent=std::any_of(currentConstraints.begin(),currentConstraints.begin()+eventContactCount,[&](const auto& row) {
                        return row.bodyAIndex==contact.bodyAIndex && row.bodyBIndex==contact.bodyBIndex;
                    });
                    if (!alreadyPresent) {
                        contact.restitution=0.0;
                        currentConstraints.push_back(contact);
                    }
                }
                if (m_contactResolver != nullptr) {
                    resolveContacts(scene,dt,currentConstraints,stats);
                    for (const auto& c : currentConstraints) resolvedPairs.push_back({c.bodyAIndex,c.bodyBIndex});
                }
            }

            ++substepCount;
            if (remainingTime > kTimeEpsilon) {
                updateBodyInertiaCache(scene);
                std::vector<SolverBodyPair> refreshedPairs;
                runBroadPhase(scene, refreshedPairs, stats, remainingTime);
                runNarrowPhaseCcdEvents(scene, refreshedPairs, remainingTime, events, stats);
                // 零时刻接触已经解算，作为持续支撑交给步末阶段；继续处理其它未来冲击。
                std::erase_if(events, [](const auto& event) { return event.toi <= kTimeEpsilon; });
                for (auto& event : events) event.toi += currentTime;
            }
        }

        if (remainingTime > kTimeEpsilon) {
            integrate(scene, remainingTime, stats);
        }
    }

    void Solver::resolveContacts(SolverSceneSnapshotBase& scene,
                                 const double dt,
                                 const std::vector<SolverContactConstraint>& constraints,
                                 SolverStats& stats) const
    {
        if (m_contactResolver != nullptr) {
            auto bodies=scene.getBodies();
            std::vector<bool> wake(bodies.size(),false);
            for (const auto& c:constraints) {
                if (c.bodyAIndex>=bodies.size() || c.bodyBIndex>=bodies.size()) continue;
                auto* a=bodies[c.bodyAIndex].entity; auto* b=bodies[c.bodyBIndex].entity;
                if (!a || !b || a->isFixed() || b->isFixed() || a->isAsleep()==b->isAsleep()) continue;
                if ((b->getVelocity()-a->getVelocity()).dot(c.contactNormal)<-m_config.restitutionVelocityThreshold)
                    wake[c.bodyAIndex]=wake[c.bodyBIndex]=true;
            }
            bool changed=true;
            while (changed) {
                changed=false;
                for (const auto& c:constraints) {
                    if (c.bodyAIndex>=bodies.size() || c.bodyBIndex>=bodies.size()) continue;
                    auto* a=bodies[c.bodyAIndex].entity; auto* b=bodies[c.bodyBIndex].entity;
                    if (!a || !b || a->isFixed() || b->isFixed()) continue;
                    if (wake[c.bodyAIndex]!=wake[c.bodyBIndex]) {
                        wake[c.bodyAIndex]=wake[c.bodyBIndex]=true; changed=true;
                    }
                }
            }
            for (std::size_t i=0;i<bodies.size();++i) if (wake[i] && bodies[i].entity->isAsleep()) {
                bodies[i].entity->setAsleep(false);
                bodies[i].inverseMass=1.0/bodies[i].entity->getMass();
            }
            updateBodyInertiaCache(scene);
            m_contactResolver->resolveContacts(scene, constraints, dt, m_config, stats);
        }
    }

    void Solver::integrate(SolverSceneSnapshotBase& scene, const double dt, SolverStats& stats) const
    {
        if (m_integrator != nullptr) {
            m_integrator->integrate(scene, dt, m_config, stats);
        }
    }

    void Solver::applyVelocityAndAngularConstraints(SolverSceneSnapshotBase& scene,
                                                     const double dt,
                                                     SolverStats& stats) const
    {
        const auto constraints = scene.getConstraints();
        stats.constraintCount = constraints.size();

        for (ConstraintsBase* constraint : constraints) {
            if (constraint == nullptr || !constraint->isEnabled()) {
                continue;
            }

            constraint->applyVelocityConstraint(dt);
            constraint->applyAngularVelocityConstraint(dt);
        }
    }

    void Solver::applyPositionConstraints(SolverSceneSnapshotBase& scene,
                                          const double dt,
                                          SolverStats& stats) const
    {
        const auto constraints = scene.getConstraints();

        for (ConstraintsBase* constraint : constraints) {
            if (constraint == nullptr || !constraint->isEnabled()) {
                continue;
            }

            if (m_config.enableContactResolution && m_contactResolver &&
                (constraint->getType()==ConstraintType::ConnectingRod || constraint->getType()==ConstraintType::ConnectingLine)) continue;
            constraint->applyPositionConstraint(dt);
        }
    }

    void Solver::updateEnergyStats(const SolverSceneSnapshotBase& scene, SolverStats& stats)
    {
        const auto bodies = scene.getBodies();
        stats.bodyCount = bodies.size();
        stats.dynamicBodyCount = 0;
        stats.frozenBodyCount = 0;
        stats.totalKineticEnergy = 0.0;

        for (const SolverBodyHandle& body : bodies) {
            if (body.entity == nullptr) {
                continue;
            }

            if (body.entity->isAsleep()) {
                ++stats.frozenBodyCount;
            }

            if (body.inverseMass <= 0.0) {
                continue;
            }

            ++stats.dynamicBodyCount;
            const Eigen::Vector3d speed = body.entity->getVelocity();
            stats.totalKineticEnergy += 0.5 * body.entity->getMass() * speed.squaredNorm();
        }
    }
}
