/**
* @file        implementation.cpp
 * @brief       接触求解器策略实现文件
 * @details     实现接触求解器策略的各项成员函数。
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
#include "frictionless_contact_resolver.h"
#include "physical_entity.h"
#include "frictional_contact_resolver.h"
#include <algorithm>
#include <cmath>
//
// Created by jehor on 2026/4/25.
//

namespace egret
{
    void FrictionlessContactResolverStrategy::resolveContacts(SolverSceneSnapshotBase& scene,
                                                              const std::span<const SolverContactConstraint>
                                                              constraints,
                                                              [[maybe_unused]] const double dt,
                                                              const SolverConfig& config,
                                                              SolverStats& stats) const
    {
        std::vector<SolverContactConstraint> frictionless(constraints.begin(), constraints.end());
        for (auto& c : frictionless) {
            c.frictionCoefficient = 0.0;
            c.dynamicFrictionCoefficient = 0.0;
        }
        FrictionalContactResolverStrategy resolver;
        resolver.resolveContacts(scene, frictionless, dt, config, stats);
    }
}
