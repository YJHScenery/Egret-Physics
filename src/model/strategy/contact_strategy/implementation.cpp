#include "frictionless_contact_resolver.h"
#include "physical_entity.h"
#include <cmath>
//
// Created by jehor on 2026/4/25.
//
#include "logger.h"

namespace egret
{
    void FrictionlessContactResolverStrategy::resolveContacts(SolverSceneSnapshotBase &scene,
                                                              const std::span<const SolverContactConstraint> constraints,
                                                              const double dt,
                                                              const SolverConfig &config,
                                                              SolverStats &stats) const
    {
        std::ignore = dt;

        auto bodies = scene.getBodies();

        for (std::uint32_t velIter = 0; velIter < config.velocityIterations; ++velIter)
        {
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

                Eigen::Vector3d rawNormal = constraint.contact.normal;
                if (!rawNormal.allFinite() || rawNormal.squaredNorm() < 1e-12)
                {
                    continue;
                }
                const Eigen::Vector3d normal = rawNormal.normalized();

                const Eigen::Vector3d velocityA = bodyA.entity->getSpeed();
                const Eigen::Vector3d angularA = bodyA.entity->getAngular();
                const Eigen::Vector3d velocityB = bodyB.entity->getSpeed();
                const Eigen::Vector3d angularB = bodyB.entity->getAngular();

                if (!velocityA.allFinite() || !velocityB.allFinite() || !angularA.allFinite() || !angularB.allFinite())
                {
                    continue;
                }

                const Eigen::Vector3d positionA = bodyA.entity->getPosition();
                const Eigen::Vector3d positionB = bodyB.entity->getPosition();
                const Eigen::Vector3d contactPosition = constraint.contact.position;

                const Eigen::Vector3d rA = contactPosition - positionA;
                const Eigen::Vector3d rB = contactPosition - positionB;

                const Eigen::Vector3d contactVelA = velocityA + angularA.cross(rA);
                const Eigen::Vector3d contactVelB = velocityB + angularB.cross(rB);

                const double relativeNormalVelocity = (contactVelB - contactVelA).dot(normal);

                const Eigen::Vector3d rA_cross_n = rA.cross(normal);
                const Eigen::Vector3d rB_cross_n = rB.cross(normal);

                double effectiveMass = bodyA.inverseMass + bodyB.inverseMass;
                effectiveMass += rA_cross_n.dot(bodyA.inverseInertiaTensor * rA_cross_n);
                effectiveMass += rB_cross_n.dot(bodyB.inverseInertiaTensor * rB_cross_n);

                if (!std::isfinite(effectiveMass) || effectiveMass <= 0.0)
                {
                    continue;
                }

                if (relativeNormalVelocity < 0.0)
                {

                    LOG_DEBUG("111");
                    const double impulseMagnitude = -(1.0 + constraint.restitution) * relativeNormalVelocity / effectiveMass;
                    if (!std::isfinite(impulseMagnitude))
                    {
                        continue;
                    }
                    const Eigen::Vector3d impulse = impulseMagnitude * normal;

                    if (bodyA.inverseMass > 0.0)
                    {
                        Eigen::Vector3d nextSpeed = velocityA - impulse * bodyA.inverseMass;
                        if (nextSpeed.allFinite())
                        {
                            bodyA.entity->setSpeed(nextSpeed);
                        }

                        const Eigen::Vector3d angularImpulse = rA.cross(-impulse);
                        Eigen::Vector3d nextAngular = angularA + bodyA.inverseInertiaTensor * angularImpulse;
                        if (nextAngular.allFinite())
                        {
                            bodyA.entity->setAngular(nextAngular);
                            LOG_DEBUG("222");
                        }
                    }

                    if (bodyB.inverseMass > 0.0)
                    {
                        Eigen::Vector3d nextSpeed = velocityB + impulse * bodyB.inverseMass;
                        if (nextSpeed.allFinite())
                        {
                            bodyB.entity->setSpeed(nextSpeed);
                        }

                        const Eigen::Vector3d angularImpulse = rB.cross(impulse);
                        Eigen::Vector3d nextAngular = angularB + bodyB.inverseInertiaTensor * angularImpulse;
                        if (nextAngular.allFinite())
                        {
                            // LOG_DEBUG("333");
                            LOG_DEBUG(QString::number(nextAngular.x()) + ", " + QString::number(nextAngular.y()) + ", " + QString::number(nextAngular.z()));
                            LOG_DEBUG(QString::number(rB.norm()));
                            LOG_DEBUG(QString::number(rB.cross(impulse).x()) + ", " + QString::number(rB.cross(impulse).y()) + ", " + QString::number(rB.cross(impulse).z()));
                            LOG_DEBUG(bodyB.inverseInertiaTensor.isZero() ? "1" : "0");
                            // 调试：打印完整的惯性张量
                            Eigen::Matrix3d invI = bodyB.inverseInertiaTensor;
                            LOG_DEBUG("inverseInertiaTensor: " +
                                      QString::number(invI(0, 0)) + ", " + QString::number(invI(0, 1)) + ", " + QString::number(invI(0, 2)) + "; " +
                                      QString::number(invI(1, 0)) + ", " + QString::number(invI(1, 1)) + ", " + QString::number(invI(1, 2)) + "; " +
                                      QString::number(invI(2, 0)) + ", " + QString::number(invI(2, 1)) + ", " + QString::number(invI(2, 2)));
                            LOG_DEBUG("angularImpulse: " +
                                      QString::number(angularImpulse.x()) + ", " + QString::number(angularImpulse.y()) + ", " + QString::number(angularImpulse.z()));
                            LOG_DEBUG("invI * angularImpulse: " +
                                      QString::number((invI * angularImpulse).x()) + ", " +
                                      QString::number((invI * angularImpulse).y()) + ", " +
                                      QString::number((invI * angularImpulse).z()));
                            bodyB.entity->setAngular(nextAngular);
                        }
                    }

                    // if (bodyA.inverseMass > 0.0) {
                    //
                    // }

                    // if (bodyB.inverseMass > 0.0) {
                    //
                    // }
                }
            }
        }

        for (std::uint32_t posIter = 0; posIter < config.positionIterations; ++posIter)
        {
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

                Eigen::Vector3d rawNormal = constraint.contact.normal;
                if (!rawNormal.allFinite() || rawNormal.squaredNorm() < 1e-12)
                {
                    continue;
                }
                const Eigen::Vector3d normal = rawNormal.normalized();
                const double invMassSum = bodyA.inverseMass + bodyB.inverseMass;

                if (!std::isfinite(invMassSum) || invMassSum <= 0.0)
                {
                    continue;
                }

                const double penetration = std::max(constraint.contact.penetration - config.penetrationSlop, 0.0);
                if (!std::isfinite(penetration))
                {
                    continue;
                }
                if (penetration > 0.0)
                {
                    const double correctionMagnitude = penetration * config.positionCorrectionBeta / invMassSum;
                    if (!std::isfinite(correctionMagnitude))
                    {
                        continue;
                    }
                    const Eigen::Vector3d correction = correctionMagnitude * normal;

                    if (bodyA.inverseMass > 0.0)
                    {
                        Eigen::Vector3d correctedPosition = bodyA.entity->getPosition() - correction * bodyA.inverseMass;

                        if (correctedPosition.allFinite())
                        {
                            bodyA.entity->setPosition(correctedPosition);
                            bodyA.transform->setTranslation(correctedPosition);
                        }
                    }

                    if (bodyB.inverseMass > 0.0)
                    {
                        Eigen::Vector3d correctedPosition = bodyB.entity->getPosition() + correction * bodyB.inverseMass;

                        if (correctedPosition.allFinite())
                        {
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