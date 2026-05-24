#include "physical_entity.h"
#include "semi_implicit_euler_integrator.h"
//
// Created by jehor on 2026/4/25.
//
namespace egret
{
    void SemiImplicitEulerIntegratorStrategy::integrate(SolverSceneSnapshotBase& scene,
                                                    const double dt,
                                                    const SolverConfig& config,
                                                    SolverStats& stats) const
    {
        auto bodies = scene.getBodies();

        for (SolverBodyHandle& body : bodies) {
            if (body.entity == nullptr || body.transform == nullptr || !body.enableIntegration) {
                continue;
            }

            if (body.inverseMass <= 0.0) {
                continue;
            }

            body.entity->applyForce(dt);
            body.entity->applyTorque(dt, body.transform->getLocalToWorldMatrix());
            body.entity->rotate(dt, body.transform->getLocalToWorldMatrix());

            // Eigen::Vector3d position = body.entity->getPosition();
            // Eigen::Vector3d speed = body.entity->getSpeed();
            // position.z() = 0.0;
            // speed.z() = 0.0;
            // body.entity->setPosition(position);
            // body.entity->setSpeed(speed);
            // body.transform->setTranslation(position);
        }

        stats.resolvedContactCount += 0;
    }

}