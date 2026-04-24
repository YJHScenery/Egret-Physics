//
// Created by jehor on 2026/4/24.
//

#ifndef EGRET_PHYSICS_GEOMETRY_H
#define EGRET_PHYSICS_GEOMETRY_H
#include <optional>
#include "Eigen/Dense"

namespace egret
{
    struct Force
    {
        std::optional<Eigen::Vector3d> applyPosition;
        Eigen::Vector3d force;
    };

    struct Axis
    {
        Eigen::Vector3d basePoint;
        Eigen::Vector3d rotationAxis;
    };

    struct Transform
    {
        Eigen::Vector3d translation;  // 位置（平移）
        Eigen::Quaterniond rotation;  // 旋转（四元数，无万向锁）
        Eigen::Vector3d scale;  // 缩放（通常 (1,1,1) 表示原大小）

        Eigen::Matrix4d localToWorld;  // 局部坐标 -> 世界坐标 的矩阵
        Eigen::Matrix4d worldToLocal;  // 世界坐标 -> 局部坐标 的矩阵（逆矩阵）
        bool matrixDirty{};  // 标记是否需要重新计算矩阵
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
