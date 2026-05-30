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
#include <magic_enum.hpp>


namespace egret
{
    enum class ShapeType: std::uint8_t
    {
        Unknown = 0,

        Box, Cylinder, CylindricalShell, Disk, Ring, Rod, Sphere, SphericalShell,

        Abstract = 127,

        Other = 255
    };


}

// ReSharper disable once CppRedundantNamespaceDefinition
namespace magic_enum::customize {
    template <>
    struct enum_range<egret::ShapeType> {
        static constexpr int min = 0;
        static constexpr int max = 255;
    };
}

namespace egret
{
    class SceneRenderItem;

    struct ShapeLoadInfo
    {
        // using ValueType = std::variant<double, std::int64_t, bool, std::string, Eigen::Vector3d, Eigen::Quaterniond>;

        std::uint32_t typeId;
        std::map<std::string, std::string> strParams;
        std::map<std::string, std::vector<double>> numberParams;

        // std::unordered_map<std::string, ValueType> parameters;
    };




    class ShapeBase
    {
    public:
        virtual ~ShapeBase() = default;

        [[nodiscard]] virtual ShapeType typeId() const;

        [[nodiscard]] virtual double getVolume() const = 0; // 计算体积

        [[nodiscard]] virtual Eigen::Vector3d getCenterOfMass() const = 0; // 计算质心（均匀物体）

        [[nodiscard]] virtual Eigen::Matrix3d getInertiaTensor(double mass) const = 0; // 计算转动惯量，通过矩阵能够计算出任意轴的转动惯量

        // // 球和任意其他形状碰撞
        // [[nodiscard]] virtual bool collideWith(const ShapeBase* other,
        //     const Transform& thisTrans, const Transform& otherTrans, ContactManifold& manifold) const = 0;

        // 世界包围盒
        [[nodiscard]] virtual AABB getAABB(const Transform& transform) const = 0;

        [[nodiscard]] bool collide(const ShapeBase* other,
                                   const Transform& thisTrans, const Transform& otherTrans,
                                   ContactManifold& manifold) const;

        [[nodiscard]] std::optional<double> continuousCollide(const ShapeBase* other,
                                                              const Transform& initTransA,
                                                              const Transform& initTransB,
                                                              const Eigen::Vector3d& thisLinearVel,
                                                              const Eigen::Vector3d& thisAngularVel,
                                                              // 角速度向量（方向为轴，大小为弧度/秒）
                                                              const Eigen::Vector3d& otherLinearVel,
                                                              const Eigen::Vector3d& otherAngularVel,
                                                              double dt,
                                                              ContactManifold& manifold // 输出碰撞信息（深度≈0）)
        ) const;

        // 形状自描述的加载信息，默认仅提供 typeId。
        [[nodiscard]] virtual ShapeLoadInfo getLoadInfo() const;

        // GJK 算法支撑函数
        [[nodiscard]] virtual Eigen::Vector3d support(const Eigen::Vector3d& direction,
                                                      const Transform& transform) const = 0;
    };
} // egret

#endif //EGRET_PHYSICS_RIGID_SHAPE_BASE_H
