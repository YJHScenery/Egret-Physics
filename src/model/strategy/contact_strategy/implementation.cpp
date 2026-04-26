#include "frictionless_contact_resolver.h"
#include "physical_entity.h"
#include <cmath>
//
// Created by jehor on 2026/4/25.
//
namespace egret
{
    void FrictionlessContactResolverStrategy::resolveContacts(ISolverSceneSnapshot& scene,
                                                              const std::span<const SolverContactConstraint> constraints,
                                                              const double dt,
                                                              const SolverConfig& config,
                                                              SolverStats& stats) const
    {
        std::ignore = dt;

        auto bodies = scene.getBodies();

        for (std::uint32_t velIter = 0; velIter < config.velocityIterations; ++velIter) {
            for (const SolverContactConstraint& constraint : constraints) {
                if (constraint.bodyAIndex >= bodies.size() || constraint.bodyBIndex >= bodies.size()) {
                    continue;
                }

                SolverBodyHandle& bodyA = bodies[constraint.bodyAIndex];
                SolverBodyHandle& bodyB = bodies[constraint.bodyBIndex];

                if (bodyA.entity == nullptr || bodyB.entity == nullptr || bodyA.transform == nullptr || bodyB.transform ==
                    nullptr) {
                    continue;
                }

                Eigen::Vector3d rawNormal = constraint.contact.normal;
                if (config.lockToXYPlane) {
                    rawNormal.z() = 0.0;
                }
                if (!rawNormal.allFinite() || rawNormal.squaredNorm() < 1e-12) {
                    continue;
                }
                const Eigen::Vector3d normal = rawNormal.normalized();
                const Eigen::Vector3d velocityA = bodyA.entity->getSpeed();
                const Eigen::Vector3d velocityB = bodyB.entity->getSpeed();
                if (!velocityA.allFinite() || !velocityB.allFinite()) {
                    continue;
                }
                const double relativeNormalVelocity = (velocityB - velocityA).dot(normal);
                const double invMassSum = bodyA.inverseMass + bodyB.inverseMass;

                if (!std::isfinite(invMassSum) || invMassSum <= 0.0) {
                    continue;
                }

                if (relativeNormalVelocity < 0.0) {
                    const double impulseMagnitude = -(1.0 + constraint.restitution) * relativeNormalVelocity / invMassSum;
                    if (!std::isfinite(impulseMagnitude)) {
                        continue;
                    }
                    const Eigen::Vector3d impulse = impulseMagnitude * normal;

                    if (bodyA.inverseMass > 0.0) {
                        const Eigen::Vector3d nextSpeed = velocityA - impulse * bodyA.inverseMass;
                        if (nextSpeed.allFinite()) {
                            Eigen::Vector3d clampedSpeed = nextSpeed;
                            if (config.lockToXYPlane) {
                                clampedSpeed.z() = 0.0;
                            }
                            bodyA.entity->setSpeed(clampedSpeed);
                        }
                    }

                    if (bodyB.inverseMass > 0.0) {
                        const Eigen::Vector3d nextSpeed = velocityB + impulse * bodyB.inverseMass;
                        if (nextSpeed.allFinite()) {
                            Eigen::Vector3d clampedSpeed = nextSpeed;
                            if (config.lockToXYPlane) {
                                clampedSpeed.z() = 0.0;
                            }
                            bodyB.entity->setSpeed(clampedSpeed);
                        }
                    }
                }
            }
        }

        for (std::uint32_t posIter = 0; posIter < config.positionIterations; ++posIter) {
            for (const SolverContactConstraint& constraint : constraints) {
                if (constraint.bodyAIndex >= bodies.size() || constraint.bodyBIndex >= bodies.size()) {
                    continue;
                }

                SolverBodyHandle& bodyA = bodies[constraint.bodyAIndex];
                SolverBodyHandle& bodyB = bodies[constraint.bodyBIndex];

                if (bodyA.entity == nullptr || bodyB.entity == nullptr || bodyA.transform == nullptr || bodyB.transform ==
                    nullptr) {
                    continue;
                }

                Eigen::Vector3d rawNormal = constraint.contact.normal;
                if (config.lockToXYPlane) {
                    rawNormal.z() = 0.0;
                }
                if (!rawNormal.allFinite() || rawNormal.squaredNorm() < 1e-12) {
                    continue;
                }
                const Eigen::Vector3d normal = rawNormal.normalized();
                const double invMassSum = bodyA.inverseMass + bodyB.inverseMass;

                if (!std::isfinite(invMassSum) || invMassSum <= 0.0) {
                    continue;
                }

                const double penetration = std::max(constraint.contact.penetration - config.penetrationSlop, 0.0);
                if (!std::isfinite(penetration)) {
                    continue;
                }
                if (penetration > 0.0) {
                    const double correctionMagnitude = penetration * config.positionCorrectionBeta / invMassSum;
                    if (!std::isfinite(correctionMagnitude)) {
                        continue;
                    }
                    const Eigen::Vector3d correction = correctionMagnitude * normal;

                    if (bodyA.inverseMass > 0.0) {
                        Eigen::Vector3d correctedPosition = bodyA.entity->getPosition() - correction * bodyA.
                            inverseMass;
                        if (config.lockToXYPlane) {
                            correctedPosition.z() = 0.0;
                        }
                        if (correctedPosition.allFinite()) {
                            bodyA.entity->setPosition(correctedPosition);
                            bodyA.transform->setTranslation(correctedPosition);
                        }
                    }

                    if (bodyB.inverseMass > 0.0) {
                        Eigen::Vector3d correctedPosition = bodyB.entity->getPosition() + correction * bodyB.
                            inverseMass;
                        if (config.lockToXYPlane) {
                            correctedPosition.z() = 0.0;
                        }
                        if (correctedPosition.allFinite()) {
                            bodyB.entity->setPosition(correctedPosition);
                            bodyB.transform->setTranslation(correctedPosition);
                        }
                    }
                }

                ++stats.resolvedContactCount;
            }
        }
    }
}