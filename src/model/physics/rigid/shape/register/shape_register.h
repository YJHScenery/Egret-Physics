//
// Created by jehor on 2026/5/6.
//

#ifndef EGRET_PHYSICS_SHAPE_REGISTER_H
#define EGRET_PHYSICS_SHAPE_REGISTER_H


#include <utility>
#include <map>
#include <functional>
#include <string>
#include "physics_utils.h"


namespace egret
{
    class Transform;
    class ShapeBase;

    using CollideFunc = std::function<bool(const ShapeBase*, const Transform&, const ShapeBase*, const Transform&, ContactManifold& manifold)>;

    using ContiCollideFunc = std::function<std::optional<double>(
        const ShapeBase* shapeA, const Transform& initTransA,
        const Eigen::Vector3d& linearVelA,
        const Eigen::Vector3d& angularVelA,   // 角速度向量（方向为轴，大小为弧度/秒）
        const ShapeBase* shapeB, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        ContactManifold& manifold)>;

    class ShapeRegister
    {
    public:
        static ShapeRegister& instance();

        // 注册判定函数（自动处理对称性，约定 id1 <= id2）
        void registerJudge(const std::string& id1, const std::string& id2, CollideFunc func);

        void registerContinuousJudge(const std::string& id1, const std::string& id2, ContiCollideFunc func);

        // 查询判定函数
        CollideFunc* findJudge(const std::string& id1, const std::string& id2) ;

        // 查询连续判定函数
        ContiCollideFunc* findContinuousJudge(const std::string& id1, const std::string& id2);

    private:
        std::map<std::pair<std::string, std::string>, CollideFunc> m_collideRegistry;

        std::map<std::pair<std::string, std::string>, ContiCollideFunc> m_continuousJudge;
    };


    constexpr char TYPE_ID_STANDARD_BOX[]{"standard_box"};
    constexpr char TYPE_ID_STANDARD_CYLINDER[]{"standard_cylinder"};
    constexpr char TYPE_ID_STANDARD_CYLINDRICAL_SHELL[]{"standard_cylinder_shell"};
    constexpr char TYPE_ID_STANDARD_DISK[]{"standard_disk"};
    constexpr char TYPE_ID_STANDARD_RING[]{"standard_ring"};
    constexpr char TYPE_ID_STANDARD_ROD[]{"standard_rod"};
    constexpr char TYPE_ID_STANDARD_SPHERE[]{"standard_sphere"};
    constexpr char TYPE_ID_STANDARD_SPHERICAL_SHELL[]{"standard_spherical_shell"};
} // egret

#endif //EGRET_PHYSICS_SHAPE_REGISTER_H
