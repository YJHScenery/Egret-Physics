/**
* @file        implementation.cpp
 * @brief       积分器策略实现文件
 * @details     实现积分器策略的各项成员函数。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-04
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Strategy
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#include "physical_entity.h"
#include "semi_implicit_euler_integrator.h"

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
