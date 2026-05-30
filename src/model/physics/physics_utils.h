/**
 * @file        physics_utils.h
 * @brief       物理工具头文件，定义物理引擎中使用的数学和物理工具函数。
 * @details     定义 Force、Velocity 等物理工具结构体和辅助函数，
 *              供物理引擎内部使用。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-30
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */
#ifndef EGRET_PHYSICS_PHYSICS_UTILS_H
#define EGRET_PHYSICS_PHYSICS_UTILS_H
#include "Eigen/Dense"
#include <utility>
#include <optional>
#include <memory>

#include "logger.h"

namespace egret
{
    /**
     * @brief       力结构体，存储力向量和实体ID。
     * @details     Force 是力结构体，存储力向量和实体ID。
     *              用于物理引擎的力计算和传递。
     *              包含力向量（force）和实体标识符（id）。
     *
     * @invariant   force 是有效的力向量
     * @invariant   id 是有效的实体标识符
     * @see         Torque, PhysicalEntity
     */
    struct Force
    {
        Eigen::Vector3d force;
        std::uint64_t id;
    };

    /**
     * @brief       力矩结构体，存储力矩向量和实体ID。
     * @details     Torque 是力矩结构体，存储力矩向量和实体ID。
     *              用于物理引擎的力矩计算和传递。
     *              包含力矩向量（torque）和实体标识符（id）。
     *
     * @invariant   torque 是有效的力矩向量
     * @invariant   id 是有效的实体标识符
     * @see         Force, PhysicalEntity
     */
    struct Torque
    {
        Eigen::Vector3d torque;
        std::uint64_t id;
    };

    /**
     * @brief       轴结构体，存储旋转轴的基点和方向。
     * @details     Axis 是轴结构体，存储旋转轴的基点和方向。
     *              用于物理引擎的旋转约束和运动计算。
     *              包含基点（basePoint）和旋转轴方向（rotationAxis）。
     *
     * @invariant   rotationAxis 是单位向量（归一化）
     * @see         Transform, PhysicalEntity
     */
    struct Axis
    {
        Eigen::Vector3d basePoint;
        Eigen::Vector3d rotationAxis;
    };

    /**
     * @brief       接触特征枚举，定义接触点的几何特征类型。
     * @details     ContactFeature 是接触特征枚举，定义接触点的几何特征类型。
     *              包含 Unknown（未知）、Vertex（顶点）、Edge（边）、Face（面）四种类型。
     *              用于碰撞检测的接触点特征识别。
     *
     * @see         ContactPoint, ContactManifold
     */
    enum class ContactFeature
    {
        Unknown = 0,
        Vertex,
        Edge,
        Face
    };

    /**
     * @brief       接触点结构体，存储碰撞检测的接触信息。
     * @details     ContactPoint 是接触点结构体，存储碰撞检测的接触信息。
     *              包含接触点位置、碰撞法线、穿透深度、接触特征等。
     *              用于物理引擎的碰撞检测和接触解算。
     *
     * @invariant   position 是有效的接触点位置
     * @invariant   normal 是单位向量（归一化）
     * @invariant   penetration >= 0，穿透深度为非负值
     * @see         ContactManifold, ContactFeature
     */
    struct ContactPoint
    {
        Eigen::Vector3d position;
        Eigen::Vector3d normal;
        double penetration;
        ContactFeature featureA{ContactFeature::Unknown};
        ContactFeature featureB{ContactFeature::Unknown};
    };

    /**
     * @brief       接触 manifold 类型，存储多个接触点。
     * @details     ContactManifold 是接触 manifold 类型，存储多个接触点。
     *              用于物理引擎的碰撞检测和接触解算。
     *              一个 manifold 可以包含多个接触点，用于处理复杂碰撞。
     *
     * @see         ContactPoint, SolverContactConstraint
     */
    using ContactManifold = std::vector<ContactPoint>;

    /**
     * @brief       AABB 包围盒结构体，存储轴对齐包围盒的最小和最大点。
     * @details     AABB 是 AABB 包围盒结构体，存储轴对齐包围盒的最小和最大点。
     *              用于物理引擎的广相位碰撞检测和空间划分。
     *              AABB 是最简单的包围盒类型，计算效率高。
     *
     * @invariant   min <= max，最小点坐标小于最大点坐标
     * @see         BroadPhaseStrategy, ShapeBase
     */
    struct AABB
    {
        Eigen::Vector3d min;
        Eigen::Vector3d max;
    };

    template <typename Derived, typename Base>
        requires(std::is_base_of_v<Base, Derived>)
    std::unique_ptr<Derived> unique_ptr_cast(std::unique_ptr<Base>&& ptr)
    {
        // 使用 dynamic_cast 进行安全检查（需要多态类型）
        Derived* d = dynamic_cast<Derived*>(ptr.get());
        if (!d) {
            LOG_FATAL_LITERAL("Cannot Cast This Pointer");
            return nullptr; // 或者抛出异常
        }

        // 释放原指针的所有权
        ptr.release();
        return std::unique_ptr<Derived>(d);
    }
}
#endif // EGRET_PHYSICS_PHYSICS_UTILS_H
