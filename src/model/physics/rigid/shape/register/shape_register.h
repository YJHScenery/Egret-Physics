/**
* @file        shape_register.h
 * @brief       形状注册器头文件
 * @details     定义ShapeRegister类，提供碰撞检测函数的注册和查找功能。
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
        const Eigen::Vector3d& angularVelA,
        const ShapeBase* shapeB, const Transform& initTransB,
        const Eigen::Vector3d& linearVelB,
        const Eigen::Vector3d& angularVelB,
        double dt,
        ContactManifold& manifold)>;

    /**
     * @brief       形状碰撞检测注册器类，管理碰撞检测函数的注册和查找。
     * @details     ShapeRegister 是一个单例类，提供碰撞检测函数的注册和查找功能。
     *              采用策略模式，不同形状组合的碰撞检测函数可以动态注册。
     *              支持静态碰撞检测（CollideFunc）和连续碰撞检测（ContiCollideFunc）。
     *              注册时自动处理对称性（约定 id1 <= id2）。
     * 
     * @invariant   m_collideRegistry 存储已注册的碰撞检测函数
     * @invariant   m_continuousJudge 存储已注册的连续碰撞检测函数
     * @remark      ShapeRegister 是单例，全局唯一实例
     * @see         ShapeBase, Transform, ContactManifold
     */
    class ShapeRegister
    {
    public:
        static ShapeRegister& instance();

        // 注册判定函数（自动处理对称性，约定 id1 <= id2）
        void registerJudge(std::uint32_t id1, std::uint32_t id2, CollideFunc func);

        void registerContinuousJudge(std::uint32_t id1, std::uint32_t id2, ContiCollideFunc func);

        // 查询判定函数
        CollideFunc* findJudge(std::uint32_t id1, std::uint32_t id2) ;

        // 查询连续判定函数
        ContiCollideFunc* findContinuousJudge(std::uint32_t id1, std::uint32_t id2);

    private:
        std::map<std::pair<std::uint32_t, std::uint32_t>, CollideFunc> m_collideRegistry;

        std::map<std::pair<std::uint32_t, std::uint32_t>, ContiCollideFunc> m_continuousJudge;
    };





} // egret

#endif //EGRET_PHYSICS_SHAPE_REGISTER_H
