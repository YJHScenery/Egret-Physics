/**
 * @file contact_manifold_builder.h
 * @brief 构建盒体面接触与圆柱端面接触的稳定多点流形。
 */
#pragma once
#include "solver_types.h"

namespace egret
{
    /** @brief 在给定时刻从真实面特征生成流形；不支持的特征返回原始接触。 */
    std::vector<CcdContactData> buildContactManifold(PhysicalEntity& a, PhysicalEntity& b,
        double time, const Eigen::Vector3d& normal, double penetration,
        const Eigen::Vector3d& point);
}
