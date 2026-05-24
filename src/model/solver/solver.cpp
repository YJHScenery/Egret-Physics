//
// 由 GitHub Copilot 于 2026/4/25 创建。
//

#include "solver.h"

#include <algorithm>
#include <cmath>
#include <QDebug>
#include <qgenericmatrix.h>
#include <QString>

#include "field_base.h"
#include "constraints_base.h"
#include "particle.h"
#include "physical_entity.h"
#include "broad_phase_strategy/brute_force_broad_phase.h"
#include "contact_strategy/frictionless_contact_resolver.h"
#include "integrator_strategy/semi_implicit_euler_integrator.h"
#include <QMatrix3x3>

namespace egret
{
    namespace
    {
        constexpr double kAabbFallbackTolerance = 1e-6;
        constexpr double kAabbFallbackPenetration = 0.002;

        bool buildAabbFallbackContact(const SolverBodyHandle &bodyA,
                                      const SolverBodyHandle &bodyB,
                                      ContactManifold &manifold)
        {
            if (bodyA.entity->getShape() == nullptr || bodyB.entity->getShape() == nullptr || bodyA.transform == nullptr || bodyB.transform == nullptr)
            {
                return false;
            }

            const AABB aabbA = bodyA.entity->getShape()->getAABB(*bodyA.transform);
            const AABB aabbB = bodyB.entity->getShape()->getAABB(*bodyB.transform);
            const Eigen::Vector3d overlapMin = aabbA.min.cwiseMax(aabbB.min);
            const Eigen::Vector3d overlapMax = aabbA.max.cwiseMin(aabbB.max);
            const Eigen::Vector3d overlap = overlapMax - overlapMin;

            if (overlap.x() < -kAabbFallbackTolerance || overlap.y() < -kAabbFallbackTolerance ||
                overlap.z() < -kAabbFallbackTolerance)
            {
                return false;
            }

            const Eigen::Vector3d stableOverlap = overlap.cwiseMax(0.0);
            int axisIndex = 0;
            double minOverlap = stableOverlap.x();
            for (int index = 1; index < 3; ++index)
            {
                if (stableOverlap[index] < minOverlap)
                {
                    minOverlap = stableOverlap[index];
                    axisIndex = index;
                }
            }

            Eigen::Vector3d normal = bodyB.transform->getTranslation() - bodyA.transform->getTranslation();
            if (!normal.allFinite() || normal.squaredNorm() < 1e-12)
            {
                normal = Eigen::Vector3d::UnitY();
            }
            else
            {
                normal.normalize();
            }

            if (normal[axisIndex] < 0.0)
            {
                normal = -normal;
            }

            ContactPoint contact{};
            contact.position = 0.5 * (overlapMin + overlapMax);
            contact.normal = normal;
            contact.penetration = std::max(minOverlap, kAabbFallbackPenetration);
            manifold.push_back(contact);
            return true;
        }
    }

    Solver::Solver(const SolverConfig &config,
                   std::unique_ptr<IntegratorStrategy> integrator,
                   std::unique_ptr<BroadPhaseStrategy> broadPhase,
                   std::unique_ptr<ContactResolverStrategy> contactResolver) : m_config(config),
                                                                               m_integrator(std::move(integrator)),
                                                                               m_broadPhase(std::move(broadPhase)),
                                                                               m_contactResolver(
                                                                                   std::move(contactResolver))
    {
    }

    SolverStepResult Solver::step(SolverSceneSnapshotBase &scene, const double dt)
    {
        SolverStepResult result{};
        result.dt = dt;

        if (dt <= 0.0)
        {
            return result;
        }

        updateBodyInertiaCache(scene);
        updateExternalForces(scene);

        std::vector<SolverBodyPair> pairs;
        std::vector<SolverContactConstraint> constraints;
        std::vector<CcdCollisionEvent> ccdEvents;

        if (m_config.enableBroadPhase)
        {
            runBroadPhase(scene, pairs, result.stats);
        }

        if (m_config.enableNarrowPhase)
        {
            if (m_config.enableToiQueue)
            {
                // qDebug() << "enable toi queue";
                runNarrowPhaseCcdEvents(scene, pairs, dt, ccdEvents, result.stats);
            }
            else
            {
                runNarrowPhaseCcd(scene, pairs, dt, constraints, result.stats);
            }
        }

        if (m_config.enableToiQueue && !ccdEvents.empty())
        {
            stepWithToiQueue(scene, dt, ccdEvents, result.stats);
        }
        else
        {
            if (m_config.enableContactResolution)
            {
                resolveContacts(scene, dt, constraints, result.stats);
            }

            if (m_config.enableIntegration)
            {
                integrate(scene, dt, result.stats);
            }
        }

        resolveConstraints(scene, dt, result.stats);
        updateEnergyStats(scene, result.stats);
        scene.advanceSimulationTime(dt);
        return result;
    }

    void Solver::setConfig(const SolverConfig &config)
    {
        m_config = config;
    }

    const SolverConfig &Solver::getConfig() const
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

    void Solver::updateBodyInertiaCache(SolverSceneSnapshotBase &scene) const
    {
        auto bodies = scene.getBodies();
        for (SolverBodyHandle &body : bodies)
        {
            qDebug() << "=== updateBodyInertiaCache ===";
            qDebug() << "body id:" << body.id << "entity:" << body.entity << "transform:" << body.transform << "inverseMass:" << body.inverseMass;

            if (body.entity == nullptr || body.transform == nullptr || body.inverseMass <= 0.0)
            {
                qDebug() << "Early return: entity/transfrom null or inverseMass <= 0";
                body.inverseInertiaTensor.setZero();
                continue;
            }

            auto shape = body.entity->getShape();
            qDebug() << "shape:" << shape.get();
            if (!shape)
            {
                qDebug() << "Early return: shape is null";
                body.inverseInertiaTensor.setZero();
                continue;
            }

            std::function<QMatrix3x3(Eigen::Matrix3d const &)> getQMat{
                [](Eigen::Matrix3d const &mat) -> QMatrix3x3
                {
                    QMatrix3x3 result;
                    for (int i = 0; i < 3; ++i)
                    {
                        for (int j = 0; j < 3; ++j)
                        {
                            result(i, j) = static_cast<float>(mat(i, j));
                        }
                    }
                    return result;
                }};
            const double mass = 1.0 / body.inverseMass;
            qDebug() << "mass:" << mass;
            const Eigen::Matrix3d I_local = shape->getInertiaTensor(mass);
            qDebug() << "I_local:\n"
                     << getQMat(I_local);
            const Eigen::Matrix3d R = body.transform->getRotationMatrix();
            const Eigen::Matrix3d I_world = R * I_local * R.transpose();
            qDebug() << "I_world:\n"
                     << getQMat(I_world);

            body.inverseInertiaTensor = I_world.inverse();

            qDebug() << "inverseInertiaTensor:\n"
                     << getQMat(body.inverseInertiaTensor);
            qDebug() << "=== End updateBodyInertiaCache ===";
        }
    }

    void Solver::updateExternalForces(SolverSceneSnapshotBase &scene) const
    {
        const auto bodies = scene.getBodies();
        const auto fields = scene.getFields();

        for (const SolverBodyHandle &body : bodies)
        {
            if (body.entity == nullptr)
            {
                continue;
            }

            if (m_config.clearForcesBeforeFieldApply)
            {
                const std::size_t forceCount = body.entity->getForcesCR().size();
                for (std::size_t index = 0; index < forceCount; ++index)
                {
                    body.entity->removeForce(0);
                }
            }

            if (!m_config.applyFieldsEachStep)
            {
                continue;
            }

            for (FieldBase *field : fields)
            {
                if (field != nullptr)
                {
                    field->applyToEntity(body.entity);
                }

                // if (auto fieldEntityPtr{dynamic_cast<Particle*>(field)}) {
                //     const Eigen::Vector3d& pos{fieldEntityPtr->getPositionCR()};
                //     qDebug() << "Position: " << pos.x() << pos.y() << pos.z();
                // }
            }
        }
    }

    void Solver::runBroadPhase(const SolverSceneSnapshotBase &scene,
                               std::vector<SolverBodyPair> &pairs,
                               SolverStats &stats) const
    {
        if (m_broadPhase != nullptr)
        {
            m_broadPhase->buildCandidatePairs(scene, m_config, pairs, stats);
            stats.broadPhasePairCount = pairs.size();
            // qDebug() << stats.broadPhasePairCount;
            // if (stats.broadPhasePairCount != 0) {
            //     qDebug() << stats.broadPhasePairCount;
            //     qDebug() << pairs.size();
            //     qDebug() << "not 0" << __func__;
            //     for (const auto& pair : pairs) {
            //         qDebug() << pair.bodyAIndex << " " << pair.bodyBIndex;
            //     }
            // }
        }
    }

    void Solver::runNarrowPhase(const SolverSceneSnapshotBase &scene,
                                const std::vector<SolverBodyPair> &pairs,
                                std::vector<SolverContactConstraint> &constraints,
                                SolverStats &stats)
    {
        const auto bodies = scene.getBodies();
        stats.narrowPhaseTestCount = pairs.size();

        for (const SolverBodyPair &pair : pairs)
        {

            if (pair.bodyAIndex >= bodies.size() || pair.bodyBIndex >= bodies.size())
            {
                continue;
            }

            const SolverBodyHandle &bodyA = bodies[pair.bodyAIndex];
            const SolverBodyHandle &bodyB = bodies[pair.bodyBIndex];

            if (bodyA.entity->getShape() == nullptr || bodyB.entity->getShape() == nullptr || bodyA.transform == nullptr || bodyB.transform == nullptr)
            {
                continue;
            }

            ContactManifold manifold;
            bool collided = bodyA.entity->getShape()->collide(bodyB.entity->getShape().get(), *bodyA.transform, *bodyB.transform, manifold);

            if (!collided)
            {
                ContactManifold reversedManifold;
                collided = bodyB.entity->getShape()->collide(bodyA.entity->getShape().get(), *bodyB.transform, *bodyA.transform, reversedManifold);
                if (collided)
                {
                    manifold.reserve(reversedManifold.size());
                    for (ContactPoint contact : reversedManifold)
                    {
                        contact.normal = -contact.normal;
                        manifold.push_back(contact);
                    }
                }
            }

            if (!collided)
            {
                collided = buildAabbFallbackContact(bodyA, bodyB, manifold);
                if (collided)
                {
                    qDebug() << "fallback aabb contact:" << pair.bodyAIndex << pair.bodyBIndex;
                }
            }
            if (!collided)
            {
                continue;
            }

            qDebug() << "Creating constraint: bodyAIndex=" << pair.bodyAIndex
                     << "bodyBIndex=" << pair.bodyBIndex
                     << "bodyA id=" << bodies[pair.bodyAIndex].id
                     << "bodyB id=" << bodies[pair.bodyBIndex].id;

            if (stats.narrowPhaseTestCount != 0)
            {
                qDebug() << "not 0" << __func__;
                for (auto &pair : pairs)
                {
                    qDebug() << pair.bodyAIndex << " " << pair.bodyBIndex;
                }// 在 constraints.push_back 之前添加：
                qDebug() << "Creating constraint: pair.bodyAIndex=" << pair.bodyAIndex
                         << "pair.bodyBIndex=" << pair.bodyBIndex
                         << "-> bodyA id=" << bodyA.id << "bodyB id=" << bodyB.id;
            }
            for (const ContactPoint &contact : manifold)
            {
                if (!contact.position.allFinite() || !contact.normal.allFinite() ||
                    !std::isfinite(contact.penetration) || contact.penetration <= 0.0 ||
                    contact.normal.squaredNorm() < 1e-12)
                {
                    continue;
                }
// 在 constraints.push_back 之前添加：
qDebug() << "Creating constraint: pair.bodyAIndex=" << pair.bodyAIndex
         << "pair.bodyBIndex=" << pair.bodyBIndex
         << "-> bodyA id=" << bodyA.id << "bodyB id=" << bodyB.id;
                const double restitution = std::isfinite(std::min(bodyA.restitution, bodyB.restitution))
                                               ? std::clamp(std::min(bodyA.restitution, bodyB.restitution), 0.0, 1.0)
                                               : 0.0;
                constraints.push_back(SolverContactConstraint{
                    .bodyAIndex = pair.bodyAIndex,
                    .bodyBIndex = pair.bodyBIndex,
                    .contact = contact,
                    .restitution = restitution});
            }
        }

        stats.contactConstraintCount = constraints.size();
    }

    void Solver::runNarrowPhaseCcd(const SolverSceneSnapshotBase &scene,
                                   const std::vector<SolverBodyPair> &pairs,
                                   double dt,
                                   std::vector<SolverContactConstraint> &constraints,
                                   SolverStats &stats)
    {
        const auto bodies = scene.getBodies();
        stats.narrowPhaseTestCount = pairs.size();

        for (const SolverBodyPair &pair : pairs)
        {
            if (pair.bodyAIndex >= bodies.size() || pair.bodyBIndex >= bodies.size())
            {
                continue;
            }

            const SolverBodyHandle &bodyA = bodies[pair.bodyAIndex];
            const SolverBodyHandle &bodyB = bodies[pair.bodyBIndex];

            if (bodyA.entity->getShape() == nullptr || bodyB.entity->getShape() == nullptr ||
                bodyA.transform == nullptr || bodyB.transform == nullptr ||
                bodyA.entity == nullptr || bodyB.entity == nullptr)
            {
                continue;
            }

            const Eigen::Vector3d linearVelA = bodyA.entity->getSpeed();
            const Eigen::Vector3d angularVelA = bodyA.entity->getAngular();
            const Eigen::Vector3d linearVelB = bodyB.entity->getSpeed();
            const Eigen::Vector3d angularVelB = bodyB.entity->getAngular();

            ContactManifold manifold;
            auto toiOpt = bodyA.entity->getShape()->continuousCollide(
                bodyB.entity->getShape().get(),
                *bodyA.transform,
                *bodyB.transform,
                linearVelA,
                angularVelA,
                linearVelB,
                angularVelB,
                dt,
                manifold);

            if (!toiOpt.has_value())
            {
                continue;
            }

            const double toi = toiOpt.value();
            if (toi < 0.0 || toi > dt)
            {
                continue;
            }

            for (const ContactPoint &contact : manifold)
            {
                if (!contact.position.allFinite() || !contact.normal.allFinite() ||
                    !std::isfinite(contact.penetration) || contact.normal.squaredNorm() < 1e-12)
                {
                    continue;
                }

                const double restitution = std::isfinite(std::min(bodyA.restitution, bodyB.restitution))
                                               ? std::clamp(std::min(bodyA.restitution, bodyB.restitution), 0.0, 1.0)
                                               : 0.0;
                constraints.push_back(SolverContactConstraint{
                    .bodyAIndex = pair.bodyAIndex,
                    .bodyBIndex = pair.bodyBIndex,
                    .contact = contact,
                    .restitution = restitution,
                    .toi = toi});
            }
        }

        stats.contactConstraintCount = constraints.size();
    }

    void Solver::runNarrowPhaseCcdEvents(const SolverSceneSnapshotBase &scene,
                                         const std::vector<SolverBodyPair> &pairs,
                                         double dt,
                                         std::vector<CcdCollisionEvent> &events,
                                         SolverStats &stats)
    {
        const auto bodies = scene.getBodies();
        stats.narrowPhaseTestCount = pairs.size();
        events.clear();

        for (const SolverBodyPair &pair : pairs)
        {
            if (pair.bodyAIndex >= bodies.size() || pair.bodyBIndex >= bodies.size())
            {
                continue;
            }

            const SolverBodyHandle &bodyA = bodies[pair.bodyAIndex];
            const SolverBodyHandle &bodyB = bodies[pair.bodyBIndex];

            if (bodyA.entity->getShape() == nullptr || bodyB.entity->getShape() == nullptr ||
                bodyA.transform == nullptr || bodyB.transform == nullptr ||
                bodyA.entity == nullptr || bodyB.entity == nullptr)
            {
                continue;
            }

            const Eigen::Vector3d linearVelA = bodyA.entity->getSpeed();
            const Eigen::Vector3d angularVelA = bodyA.entity->getAngular();
            const Eigen::Vector3d linearVelB = bodyB.entity->getSpeed();
            const Eigen::Vector3d angularVelB = bodyB.entity->getAngular();

            ContactManifold manifold;
            auto toiOpt = bodyA.entity->getShape()->continuousCollide(
                bodyB.entity->getShape().get(),
                *bodyA.transform,
                *bodyB.transform,
                linearVelA,
                angularVelA,
                linearVelB,
                angularVelB,
                dt,
                manifold);

            if (!toiOpt.has_value())
            {
                continue;
            }

            const double toi = toiOpt.value();
            if (toi < 0.0 || toi > dt)
            {
                continue;
            }

            bool validContacts = false;
            ContactManifold validManifold;
            for (const ContactPoint &contact : manifold)
            {
                if (!contact.position.allFinite() || !contact.normal.allFinite() ||
                    !std::isfinite(contact.penetration) || contact.normal.squaredNorm() < 1e-12)
                {
                    continue;
                }
                validManifold.push_back(contact);
                validContacts = true;
            }

            if (!validContacts)
            {
                continue;
            }

            const double restitution = std::isfinite(std::min(bodyA.restitution, bodyB.restitution))
                                           ? std::clamp(std::min(bodyA.restitution, bodyB.restitution), 0.0, 1.0)
                                           : 0.0;

            events.push_back(CcdCollisionEvent{
                .toi = toi,
                .bodyAIndex = pair.bodyAIndex,
                .bodyBIndex = pair.bodyBIndex,
                .manifold = std::move(validManifold),
                .restitution = restitution});
        }

        std::sort(events.begin(), events.end());
        stats.contactConstraintCount = events.size();
    }

    void Solver::stepWithToiQueue(SolverSceneSnapshotBase &scene,
                                  double dt,
                                  std::vector<CcdCollisionEvent> &events,
                                  SolverStats &stats) const
    {
        constexpr double kTimeEpsilon = 1e-10;
        constexpr double kBatchTimeEpsilon = 1e-6;
        constexpr std::size_t kMaxSubsteps = 64;

        double currentTime = 0.0;
        double remainingTime = dt;
        std::size_t substepCount = 0;

        while (remainingTime > kTimeEpsilon && !events.empty() && substepCount < kMaxSubsteps)
        {
            const double earliestToi = events.front().toi;
            const double timeToCollision = earliestToi - currentTime;

            if (timeToCollision > remainingTime + kTimeEpsilon)
            {
                integrate(scene, remainingTime, stats);
                currentTime += remainingTime;
                break;
            }

            if (timeToCollision > kTimeEpsilon)
            {
                integrate(scene, timeToCollision, stats);
                currentTime = earliestToi;
                remainingTime = dt - currentTime;
            }

            std::vector<SolverContactConstraint> currentConstraints;
            auto it = events.begin();
            while (it != events.end() && std::abs(it->toi - earliestToi) < kBatchTimeEpsilon)
            {
                for (const ContactPoint &contact : it->manifold)
                {
                    currentConstraints.push_back(SolverContactConstraint{
                        .bodyAIndex = it->bodyAIndex,
                        .bodyBIndex = it->bodyBIndex,
                        .contact = contact,
                        .restitution = it->restitution,
                        .toi = it->toi});
                }
                it = events.erase(it);
            }

            if (!currentConstraints.empty())
            {
                if (m_contactResolver != nullptr)
                {
                    m_contactResolver->resolveContacts(scene, currentConstraints, dt, m_config, stats);
                }
            }

            ++substepCount;
        }

        if (remainingTime > kTimeEpsilon)
        {
            integrate(scene, remainingTime, stats);
        }
    }

    void Solver::resolveContacts(SolverSceneSnapshotBase &scene,
                                 const double dt,
                                 const std::vector<SolverContactConstraint> &constraints,
                                 SolverStats &stats) const
    {
        if (m_contactResolver != nullptr)
        {
            m_contactResolver->resolveContacts(scene, constraints, dt, m_config, stats);
        }
    }

    void Solver::integrate(SolverSceneSnapshotBase &scene, const double dt, SolverStats &stats) const
    {
        if (m_integrator != nullptr)
        {
            m_integrator->integrate(scene, dt, m_config, stats);
        }
    }

    void Solver::resolveConstraints(SolverSceneSnapshotBase &scene,
                                    const double dt,
                                    SolverStats &stats) const
    {
        const auto constraints = scene.getConstraints();
        stats.constraintCount = constraints.size();

        for (ConstraintsBase *constraint : constraints)
        {
            if (constraint == nullptr || !constraint->isEnabled())
            {
                continue;
            }

            constraint->applyVelocityConstraint(dt);
            constraint->applyPositionConstraint(dt);
        }
    }

    void Solver::updateEnergyStats(const SolverSceneSnapshotBase &scene, SolverStats &stats)
    {
        const auto bodies = scene.getBodies();
        stats.bodyCount = bodies.size();
        stats.dynamicBodyCount = 0;
        stats.totalKineticEnergy = 0.0;

        for (const SolverBodyHandle &body : bodies)
        {
            if (body.entity == nullptr)
            {
                continue;
            }

            if (body.inverseMass <= 0.0)
            {
                continue;
            }

            ++stats.dynamicBodyCount;
            const Eigen::Vector3d speed = body.entity->getSpeed();
            stats.totalKineticEnergy += 0.5 * body.entity->getMass() * speed.squaredNorm();
        }
    }
}
