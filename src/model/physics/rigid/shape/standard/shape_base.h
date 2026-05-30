/**
 * @file        shape_base.h
 * @brief       物理形状基类头文件，定义所有物理形状的公共接口。
 * @details     定义 ShapeBase 类，作为所有物理形状（球体、长方体、圆柱等）的基类，
 *              提供形状识别、碰撞信息等通用接口。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-26
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#ifndef EGRET_PHYSICS_RIGID_SHAPE_BASE_H
#define EGRET_PHYSICS_RIGID_SHAPE_BASE_H
#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>

#include <Eigen/Dense>

#include "basic_utils.h"
#include "physics_utils.h"
#include "transform.h"
#include "shape_register.h"
#include <magic_enum.hpp>

namespace egret
{
    enum class ShapeType : std::uint32_t
    {
        Unknown = 0,

        Box,
        Cylinder,
        CylindricalShell,
        Disk,
        Ring,
        Rod,
        Sphere,
        SphericalShell,

        Abstract = 127,

        Other = 255
    };

    [[nodiscard]] std::string reflectShapeToString(const ShapeType type);

}

// ReSharper disable once CppRedundantNamespaceDefinition
namespace magic_enum::customize
{
    template <>
    struct enum_range<egret::ShapeType>
    {
        static constexpr int min = 0;
        static constexpr int max = 255;
    };
}

namespace egret
{
    class SceneRenderItem;

    struct ShapeLoadInfo
    {
        std::uint32_t typeId;
        std::map<std::string, std::string> strParams;
        std::map<std::string, std::vector<double>> numberParams;
    };

    /**
     * @brief       物理形状基类，定义所有物理形状的公共接口。
     * @details     ShapeBase 是所有物理形状（球体、长方体、圆柱等）的抽象基类，
     *              提供形状识别、碰撞检测、体积计算、质心计算、转动惯量计算等通用接口。
     *              采用策略模式，通过 ShapeRegister 注册碰撞检测函数。
     *              支持 GJK 算法的支撑函数（support）用于碰撞检测。
     *              派生类需要实现 getVolume、getCenterOfMass、getInertiaTensor、support 等虚函数。
     *
     * @invariant   typeId() 返回有效的 ShapeType 枚举值
     * @invariant   getVolume() >= 0，体积为非负值
     * @invariant   getCenterOfMass() 返回形状局部坐标系中的质心位置
     * @remark      形状类不包含位置信息，位置由 Transform 管理
     * @see         ShapeSphere, ShapeBox, ShapeCylinder, Transform, ShapeRegister
     */
    class ShapeBase
    {
    public:
        virtual ~ShapeBase() = default;

        [[nodiscard]] virtual ShapeType getType() const;

        [[nodiscard]] virtual double getVolume() const = 0; // 计算体积

        [[nodiscard]] virtual Eigen::Vector3d getCenterOfMass() const = 0; // 计算质心（均匀物体）

        [[nodiscard]] virtual Eigen::Matrix3d getInertiaTensor(double mass) const = 0; // 计算转动惯量，通过矩阵能够计算出任意轴的转动惯量

        // // 球和任意其他形状碰撞
        // [[nodiscard]] virtual bool collideWith(const ShapeBase* other,
        //     const Transform& thisTrans, const Transform& otherTrans, ContactManifold& manifold) const = 0;

        // 世界包围盒
        [[nodiscard]] virtual AABB getAABB(const Transform &transform) const = 0;

        [[nodiscard]] bool collide(const ShapeBase *other,
                                   const Transform &thisTrans, const Transform &otherTrans,
                                   ContactManifold &manifold) const;

        [[nodiscard]] std::optional<double> continuousCollide(const ShapeBase *other,
                                                              const Transform &initTransA,
                                                              const Transform &initTransB,
                                                              const Eigen::Vector3d &thisLinearVel,
                                                              const Eigen::Vector3d &thisAngularVel,
                                                              // 角速度向量（方向为轴，大小为弧度/秒）
                                                              const Eigen::Vector3d &otherLinearVel,
                                                              const Eigen::Vector3d &otherAngularVel,
                                                              double dt,
                                                              ContactManifold &manifold // 输出碰撞信息（深度≈0）)
        ) const;

        // 形状自描述的加载信息，默认仅提供 typeId。
        [[nodiscard]] virtual ShapeLoadInfo getLoadInfo() const;

        // GJK 算法支撑函数
        [[nodiscard]] virtual Eigen::Vector3d support(const Eigen::Vector3d &direction,
                                                      const Transform &transform) const = 0;
    };
} // egret

#endif // EGRET_PHYSICS_RIGID_SHAPE_BASE_H
