/**
 * @file        ccd_solver_adapter.h
 * @brief       Solver 与维度泛化 CCD 的桥接层（轻量级声明）
 * @details     仅暴露两个公共接口函数和一个轻量级结果结构体。
 *              所有重型模板头文件（CCD/GJK/EPA/GeneralizationRigidMotion）
 *              仅在 .cpp 实现文件中引入，避免翻译单元膨胀（bigobj）。
 *
 * @author      jehor
 * @date        2026/7/10
 * @version     2.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 */

#ifndef EGRET_PHYSICS_CCD_SOLVER_ADAPTER_H
#define EGRET_PHYSICS_CCD_SOLVER_ADAPTER_H

#include <Eigen/Dense>
#include <limits>
#include <optional>
#include <vector>

#include "../physics_utils.h"

namespace egret
{

// 前向声明 —— 避免引入重型头文件
class PhysicalEntity;

// ============================================================================
// AdapterCcdResult —— CCD 适配器结果（轻量级）
// ============================================================================

/**
 * @brief       CCD 连续碰撞检测适配器结果
 * @details     轻量级结构体，包含 CCD 检测的全部输出字段。
 *              字段名与 CCDResult<3> 对齐，避免在调用方引入泛型模板头文件。
 *
 * @note       未碰撞时 isCollided == false，timeOfImpact 为 +∞。
 */
struct AdapterCcdResult
{
    /// @brief 是否发生碰撞
    bool isCollided{false};

    /// @brief 碰撞时间（TOI），未碰撞时为 +∞
    double timeOfImpact{std::numeric_limits<double>::infinity()};

    /// @brief 碰撞法线（单位向量，B 指向 A 的分离方向）
    Eigen::Vector3d normal{Eigen::Vector3d::Zero()};

    /// @brief 穿透深度 δ = ‖p*‖
    double penetrationDepth{0.0};

    /// @brief 接触点 c = (a* + b*) / 2
    Eigen::Vector3d contactPoint{Eigen::Vector3d::Zero()};
};

// ============================================================================
// 公共接口
// ============================================================================

/**
 * @brief   对两个 PhysicalEntity 执行静态 GJK+EPA 碰撞检测
 * @details 内部创建 GilbertJohnsonKeerthi<3> 和 ExpandingPolytopeAlgorithm<3>，
 *          执行检测并返回 ContactManifold。
 *          若未碰撞返回 std::nullopt。
 *
 * @param   entityA  实体 A（非空）
 * @param   entityB  实体 B（非空）
 * @return  碰撞时返回包含 ContactPoint 的 manifold，否则返回 nullopt
 */
std::optional<ContactManifold> runGjkEpaOnEntities(
    PhysicalEntity* entityA,
    PhysicalEntity* entityB);

/**
 * @brief   对两个 PhysicalEntity 执行连续碰撞检测（CCD）
 * @details 将实体状态转换为 GeneralizationRigidMotion<3>，
 *          创建 ContinuousCollisionDetector<3> 并使用 Screw 模型检测。
 *          启用 TOI 精化以获得更精确的碰撞时间。
 *
 * @param   entityA  实体 A（非空）
 * @param   entityB  实体 B（非空）
 * @param   dt       检测时间区间长度（秒，> 0）
 * @return  碰撞时返回 AdapterCcdResult，否则返回 nullopt
 */
std::optional<AdapterCcdResult> runCcdOnEntities(
    PhysicalEntity* entityA,
    PhysicalEntity* entityB,
    double dt);

} // namespace egret

#endif // EGRET_PHYSICS_CCD_SOLVER_ADAPTER_H
