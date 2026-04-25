//
// Created by jehor on 2026/4/24.
//

#ifndef EGRET_PHYSICS_GEOMETRY_H
#define EGRET_PHYSICS_GEOMETRY_H
#include <optional>
#include "Eigen/Dense"

static_assert(__cplusplus >= 202002L);

namespace egret
{
    struct Force
    {
        std::optional<Eigen::Vector3d> applyPosition;
        Eigen::Vector3d force;
        std::uint64_t id;
    };

    struct Axis
    {
        Eigen::Vector3d basePoint;
        Eigen::Vector3d rotationAxis;
    };


    struct ContactPoint
    {
        // 接触点
        Eigen::Vector3d position; // 碰撞点在世界坐标系中的位置
        Eigen::Vector3d normal; // 碰撞法线
        double penetration; // 穿透深度
    };

    using ContactManifold = std::vector<ContactPoint>;

    struct AABB
    {
        Eigen::Vector3d min;
        Eigen::Vector3d max;
    };



}
#endif //EGRET_PHYSICS_GEOMETRY_H
