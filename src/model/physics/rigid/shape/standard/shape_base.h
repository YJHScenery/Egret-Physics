//
// Created by jehor on 2026/4/24.
//

#ifndef EGRET_PHYSICS_RIGID_SHAPE_BASE_H
#define EGRET_PHYSICS_RIGID_SHAPE_BASE_H
#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>

#include "Eigen/Dense"
#include "basic_utils.h"
#include "physics_utils.h"
#include "transform.h"
#include "shape_register.h"

namespace egret {

class SceneRenderItem;

struct ShapeLoadInfo
{
    using ValueType = std::variant<double, std::int64_t, bool, std::string, Eigen::Vector3d>;

    std::string typeId;
    std::unordered_map<std::string, ValueType> parameters;
};

class ShapeBase {
public:
    virtual ~ShapeBase() = default;

    [[nodiscard]] virtual const std::string& typeId() const = 0;

    [[nodiscard]] virtual double getVolume() const = 0; // 计算体积

    [[nodiscard]] virtual Eigen::Vector3d getCenterOfMass() const = 0; // 计算质心（均匀物体）

    [[nodiscard]] virtual Eigen::Matrix3d getInertiaTensor(double mass) const = 0; // 计算转动惯量，通过矩阵能够计算出任意轴的转动惯量

    // // 球和任意其他形状碰撞
    // [[nodiscard]] virtual bool collideWith(const ShapeBase* other,
    //     const Transform& thisTrans, const Transform& otherTrans, ContactManifold& manifold) const = 0;

    // 世界包围盒
    [[nodiscard]] virtual AABB getAABB(const Transform& transform) const = 0;

    [[nodiscard]] bool collide(const ShapeBase* other,
        const Transform& thisTrans, const Transform& otherTrans, ContactManifold& manifold) const;

    // 形状自描述的加载信息，默认仅提供 typeId。
    [[nodiscard]] virtual ShapeLoadInfo getLoadInfo() const;

    [[nodiscard]] virtual SceneRenderItem getBasicRenderInfo(const Eigen::Vector3d& position) const = 0;
};

} // egret

#endif //EGRET_PHYSICS_RIGID_SHAPE_BASE_H
