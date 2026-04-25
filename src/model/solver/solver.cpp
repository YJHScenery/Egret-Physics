//
// 由 GitHub Copilot 于 2026/4/25 创建。
//

#include "solver.h"

#include <algorithm>
#include <cmath>

#include "field_base.h"
#include "physical_entity.h"
#include "../strategy/implementations/brute_force_broad_phase.h"
#include "../strategy/implementations/frictionless_contact_resolver.h"
#include "../strategy/implementations/semi_implicit_euler_integrator.h"

namespace egret
{
    Solver::Solver(SolverConfig config,
                   std::unique_ptr<IntegratorStrategy> integrator,
                   std::unique_ptr<BroadPhaseStrategy> broadPhase,
                   std::unique_ptr<ContactResolverStrategy> contactResolver) : m_config(std::move(config)),
                                                                               m_integrator(std::move(integrator)),
                                                                               m_broadPhase(std::move(broadPhase)),
                                                                               m_contactResolver(std::move(contactResolver))
    {
    }

    SolverStepResult Solver::step(ISolverSceneSnapshot &scene, const double dt)
    {
        SolverStepResult result{};
        result.dt = dt;

        if (dt <= 0.0)
        {
            return result;
        }

        updateExternalForces(scene);

        std::vector<SolverBodyPair> pairs;
        std::vector<SolverContactConstraint> constraints;

        if (m_config.enableBroadPhase)
        {
            runBroadPhase(scene, pairs, result.stats);
        }

        if (m_config.enableNarrowPhase)
        {
            runNarrowPhase(scene, pairs, constraints, result.stats);
        }

        if (m_config.enableContactResolution)
        {
            resolveContacts(scene, dt, constraints, result.stats);
        }

        if (m_config.enableIntegration)
        {
            integrate(scene, dt, result.stats);
        }

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

    void Solver::updateExternalForces(ISolverSceneSnapshot &scene) const
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
            }
        }
    }

    void Solver::runBroadPhase(const ISolverSceneSnapshot &scene,
                               std::vector<SolverBodyPair> &pairs,
                               SolverStats &stats) const
    {
        if (m_broadPhase != nullptr)
        {
            m_broadPhase->buildCandidatePairs(scene, m_config, pairs, stats);
            stats.broadPhasePairCount = pairs.size();
        }
    }

    void Solver::runNarrowPhase(const ISolverSceneSnapshot &scene,
                                const std::vector<SolverBodyPair> &pairs,
                                std::vector<SolverContactConstraint> &constraints,
                                SolverStats &stats) const
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

            if (bodyA.shape == nullptr || bodyB.shape == nullptr || bodyA.transform == nullptr || bodyB.transform == nullptr)
            {
                continue;
            }

            ContactManifold manifold;
            bool collided = bodyA.shape->collideWith(bodyB.shape, *bodyA.transform, *bodyB.transform, manifold);

            if (!collided)
            {
                ContactManifold reversedManifold;
                collided = bodyB.shape->collideWith(bodyA.shape, *bodyB.transform, *bodyA.transform, reversedManifold);
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
                continue;
            }

            for (const ContactPoint &contact : manifold)
            {
                constraints.push_back(SolverContactConstraint{
                    .bodyAIndex = pair.bodyAIndex,
                    .bodyBIndex = pair.bodyBIndex,
                    .contact = contact,
                    .restitution = std::min(bodyA.restitution, bodyB.restitution)});
            }
        }

        stats.contactConstraintCount = constraints.size();
    }

    void Solver::resolveContacts(ISolverSceneSnapshot &scene,
                                 const double dt,
                                 const std::vector<SolverContactConstraint> &constraints,
                                 SolverStats &stats) const
    {
        if (m_contactResolver != nullptr)
        {
            m_contactResolver->resolveContacts(scene, constraints, dt, m_config, stats);
        }
    }

    void Solver::integrate(ISolverSceneSnapshot &scene, const double dt, SolverStats &stats) const
    {
        if (m_integrator != nullptr)
        {
            m_integrator->integrate(scene, dt, m_config, stats);
        }
    }

    void Solver::updateEnergyStats(const ISolverSceneSnapshot &scene, SolverStats &stats) const
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

namespace egret
{
    void SemiImplicitEulerIntegratorStrategy::integrate(ISolverSceneSnapshot &scene,
                                                        const double dt,
                                                        const SolverConfig &config,
                                                        SolverStats &stats) const
    {
        static_cast<void>(config);
        const auto bodies = scene.getBodies();

        for (const SolverBodyHandle &body : bodies)
        {
            if (body.entity == nullptr || body.transform == nullptr || !body.enableIntegration)
            {
                continue;
            }

            if (body.inverseMass <= 0.0)
            {
                continue;
            }

            body.entity->applyForce(dt);
            body.transform->setTranslation(body.entity->getPosition());
        }

        stats.resolvedContactCount += 0;
    }

    void BruteForceBroadPhaseStrategy::buildCandidatePairs(const ISolverSceneSnapshot &scene,
                                                           const SolverConfig &config,
                                                           std::vector<SolverBodyPair> &outPairs,
                                                           SolverStats &stats) const
    {
        static_cast<void>(config);
        const auto bodies = scene.getBodies();
        outPairs.clear();

        for (std::size_t i = 0; i < bodies.size(); ++i)
        {
            const SolverBodyHandle &bodyA = bodies[i];
            if (!bodyA.enableCollision || bodyA.shape == nullptr || bodyA.transform == nullptr)
            {
                continue;
            }

            const AABB aabbA = bodyA.shape->getAABB(*bodyA.transform);

            for (std::size_t j = i + 1; j < bodies.size(); ++j)
            {
                const SolverBodyHandle &bodyB = bodies[j];
                if (!bodyB.enableCollision || bodyB.shape == nullptr || bodyB.transform == nullptr)
                {
                    continue;
                }

                const AABB aabbB = bodyB.shape->getAABB(*bodyB.transform);
                const bool overlapX = aabbA.max.x() >= aabbB.min.x() && aabbB.max.x() >= aabbA.min.x();
                const bool overlapY = aabbA.max.y() >= aabbB.min.y() && aabbB.max.y() >= aabbA.min.y();
                const bool overlapZ = aabbA.max.z() >= aabbB.min.z() && aabbB.max.z() >= aabbA.min.z();

                if (overlapX && overlapY && overlapZ)
                {
                    outPairs.push_back(SolverBodyPair{i, j});
                }
            }
        }

        stats.broadPhasePairCount = outPairs.size();
    }

    void FrictionlessContactResolverStrategy::resolveContacts(ISolverSceneSnapshot &scene,
                                                              std::span<const SolverContactConstraint> constraints,
                                                              double dt,
                                                              const SolverConfig &config,
                                                              SolverStats &stats) const
    {
        static_cast<void>(dt);

        auto bodies = scene.getBodies();

        for (const SolverContactConstraint &constraint : constraints)
        {
            if (constraint.bodyAIndex >= bodies.size() || constraint.bodyBIndex >= bodies.size())
            {
                continue;
            }

            SolverBodyHandle &bodyA = bodies[constraint.bodyAIndex];
            SolverBodyHandle &bodyB = bodies[constraint.bodyBIndex];

            if (bodyA.entity == nullptr || bodyB.entity == nullptr || bodyA.transform == nullptr || bodyB.transform == nullptr)
            {
                continue;
            }

            const Eigen::Vector3d normal = constraint.contact.normal.normalized();
            const Eigen::Vector3d velocityA = bodyA.entity->getSpeed();
            const Eigen::Vector3d velocityB = bodyB.entity->getSpeed();
            const double relativeNormalVelocity = (velocityB - velocityA).dot(normal);
            const double invMassSum = bodyA.inverseMass + bodyB.inverseMass;

            if (invMassSum <= 0.0)
            {
                continue;
            }

            if (relativeNormalVelocity < 0.0)
            {
                const double impulseMagnitude = -(1.0 + constraint.restitution) * relativeNormalVelocity / invMassSum;
                const Eigen::Vector3d impulse = impulseMagnitude * normal;

                if (bodyA.inverseMass > 0.0)
                {
                    bodyA.entity->setSpeed(velocityA - impulse * bodyA.inverseMass);
                }

                if (bodyB.inverseMass > 0.0)
                {
                    bodyB.entity->setSpeed(velocityB + impulse * bodyB.inverseMass);
                }
            }

            const double penetration = std::max(constraint.contact.penetration - config.penetrationSlop, 0.0);
            if (penetration > 0.0)
            {
                const double correctionMagnitude = penetration * config.positionCorrectionBeta / invMassSum;
                const Eigen::Vector3d correction = correctionMagnitude * normal;

                if (bodyA.inverseMass > 0.0)
                {
                    const Eigen::Vector3d correctedPosition = bodyA.entity->getPosition() - correction * bodyA.inverseMass;
                    bodyA.entity->setPosition(correctedPosition);
                    bodyA.transform->setTranslation(correctedPosition);
                }

                if (bodyB.inverseMass > 0.0)
                {
                    const Eigen::Vector3d correctedPosition = bodyB.entity->getPosition() + correction * bodyB.inverseMass;
                    bodyB.entity->setPosition(correctedPosition);
                    bodyB.transform->setTranslation(correctedPosition);
                }
            }

            ++stats.resolvedContactCount;
        }
    }
}
