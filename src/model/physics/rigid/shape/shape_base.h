//
// Created by jehor on 2026/4/24.
//

#ifndef EGRET_PHYSICS_RIGID_SHAPE_BASE_H
#define EGRET_PHYSICS_RIGID_SHAPE_BASE_H
#include "Eigen/Dense"
#include "geometry.h"

namespace egret {

class ShapeBase {
public:
    virtual ~ShapeBase() = default;
    [[nodiscard]] virtual double getVolume() const = 0; // 计算体积

    [[nodiscard]] virtual Eigen::Vector3d getCenterOfMass() const = 0; // 计算质心（均匀物体）

    [[nodiscard]] virtual Eigen::Matrix3d getInertiaTensor(double mass) const = 0; // 计算转动惯量，通过矩阵能够计算出任意轴的转动惯量

    // 球和任意其他形状碰撞
    [[nodiscard]] virtual bool collideWith(const ShapeBase* other,
        const Transform& thisTrans, const Transform& otherTrans, ContactManifold& manifold) const = 0;

    // 世界包围盒
    [[nodiscard]] virtual AABB getAABB(const Transform& transform) const = 0;

};

} // egret

#endif //EGRET_PHYSICS_RIGID_SHAPE_BASE_H
