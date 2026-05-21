//
// Created by jehor on 2026/4/25.
//

#ifndef EGRET_PHYSICS_PHYSICS_UTILS_H
#define EGRET_PHYSICS_PHYSICS_UTILS_H
#include "Eigen/Dense"
#include <utility>
#include <optional>

namespace egret
{
    struct Force
    {
        Eigen::Vector3d force;
        std::uint64_t id;
    };

    struct Torque
    {
        Eigen::Vector3d torque;
        std::uint64_t id;
    };

    struct Axis
    {
        Eigen::Vector3d basePoint;
        Eigen::Vector3d rotationAxis;
    };


    enum class ContactFeature
    {
        Unknown = 0,
        Vertex,
        Edge,
        Face
    };

    struct ContactPoint
    {
        // 接触点
        Eigen::Vector3d position; // 碰撞点在世界坐标系中的位置
        Eigen::Vector3d normal; // 碰撞法线
        double penetration; // 穿透深度
        ContactFeature featureA{ContactFeature::Unknown};
        ContactFeature featureB{ContactFeature::Unknown};
    };

    using ContactManifold = std::vector<ContactPoint>;

    struct AABB
    {
        Eigen::Vector3d min;
        Eigen::Vector3d max;
    };


}
#endif //EGRET_PHYSICS_PHYSICS_UTILS_H
