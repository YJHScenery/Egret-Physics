/**
 * @file        continuous_collision_detection.h
 * @brief       CCD（连续碰撞检测）算法头文件
 * @details     定义 CcdResult 结果结构体和 ContinuousCollisionDetection 算法类。
 *              实现 TOI（首次接触时间）的二分搜索求解与碰撞响应冲量计算，
 *              底层统一使用 GJK（距离查询）和 EPA（穿透信息提取）算法。
 *              当前版本仅处理简单碰撞对，不处理复杂碰撞图。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-23
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 */

#ifndef EGRET_PHYSICS_CONTINUOUS_COLLISION_DETECTION_H
#define EGRET_PHYSICS_CONTINUOUS_COLLISION_DETECTION_H

#include <Eigen/Dense>
#include <optional>
#include "physical_entity.h"
#include "gilbert_johnson_keerthi.h"

namespace egret
{
    /**
     * @brief       CCD 算法的输出结果结构体
     * @details     包含首次接触时间（TOI）、碰撞法线、接触点位置和穿透深度。
     *              该结构体不使用 ContactPoint/ContactManifold，是 CCD 的原生输出类型。
     */
    struct CcdResult
    {
        /// @brief 首次接触时间（Time of Impact），范围 [0, dt]
        double toi{0.0};

        /// @brief 碰撞法线（单位向量，从物体 B 指向物体 A，即 A 的分离方向）
        Eigen::Vector3d normal{Eigen::Vector3d::Zero()};

        /// @brief 世界坐标系下的接触点位置
        Eigen::Vector3d contactPosition{Eigen::Vector3d::Zero()};

        /// @brief 穿透深度（TOI 时刻 ≈ 0）
        double penetration{0.0};
    };

    /**
     * @brief       CCD（连续碰撞检测）算法类
     * @details     对给定的两个 PhysicalEntity 执行连续碰撞检测，
     *              求解首次接触时间（TOI）并提取接触信息。
     *              内部统一使用 GJK 进行距离查询、EPA 进行穿透信息提取。
     *
     * @invariant   算法执行期间实体 Transform 会被临时修改（用于中间时刻查询），
     *              调用完成后恢复为初始值
     * @remark      当前版本仅支持简单碰撞对，不处理模块三的多碰撞对图求解
     * @see         GilbertJohnsonKeerthi, ExpandingPolytopeAlgorithm, CcdResult
     */
    class ContinuousCollisionDetection
    {
    public:
        /// @brief 二分搜索最大迭代次数
        static constexpr int kMaxToiIterations = 20;

        /// @brief TOI 收敛容差
        static constexpr double kToiEpsilon = 1e-8;

        /**
         * @brief       构造 CCD 算法实例
         * @param       entityA  碰撞实体 A
         * @param       entityB  碰撞实体 B
         */
        ContinuousCollisionDetection(PhysicalEntity* entityA, PhysicalEntity* entityB);

        /**
         * @brief       计算首次接触时间（TOI）并提取接触信息
         * @param       linearVelocityA  实体 A 的线速度（世界坐标系）
         * @param       angularVelocityA 实体 A 的角速度（世界坐标系，方向为轴，大小为弧度/秒）
         * @param       linearVelocityB  实体 B 的线速度
         * @param       angularVelocityB 实体 B 的角速度
         * @param       dt               时间步长
         * @return      std::optional<CcdResult>
         *              - 有值：检测到碰撞，包含 TOI 和接触信息
         *              - nullopt：在时间步内未发生碰撞
         */
        [[nodiscard]] std::optional<CcdResult> computeToi(
            const Eigen::Vector3d& linearVelocityA,
            const Eigen::Vector3d& angularVelocityA,
            const Eigen::Vector3d& linearVelocityB,
            const Eigen::Vector3d& angularVelocityB,
            double dt);

    private:
        PhysicalEntity* m_entityA;
        PhysicalEntity* m_entityB;

        // ─── 内部辅助方法 ───

        /**
         * @brief       在给定时刻 t 执行 GJK 碰撞检测
         * @param       linearVelA 实体 A 线速度
         * @param       angularVelA 实体 A 角速度
         * @param       linearVelB 实体 B 线速度
         * @param       angularVelB 实体 B 角速度
         * @param       t 目标时刻（相对于初始状态）
         * @return      true 表示碰撞，false 表示分离
         * @note        此方法会临时修改实体 Transform（调用后恢复），
         *              返回时 GJK 结果（含单纯形 / 支撑历史）保留在 m_lastGjkResult 中
         */
        bool gjkCollisionCheckAt(
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            const Eigen::Vector3d& linearAccelA,
            const Eigen::Vector3d& angularAccelA,
            const Eigen::Vector3d& linearAccelB,
            const Eigen::Vector3d& angularAccelB,
            double t);

        /**
         * @brief       在给定时刻 t 执行 GJK 距离查询
         * @param       linearVelA 实体 A 线速度
         * @param       angularVelA 实体 A 角速度
         * @param       linearVelB 实体 B 线速度
         * @param       angularVelB 实体 B 角速度
         * @param       t 目标时刻
         * @return      两物体间的最近距离（分离为正，穿透为负）
         */
        double gjkDistanceAt(
            const Eigen::Vector3d& linearVelA,
            const Eigen::Vector3d& angularVelA,
            const Eigen::Vector3d& linearVelB,
            const Eigen::Vector3d& angularVelB,
            const Eigen::Vector3d& linearAccelA,
            const Eigen::Vector3d& angularAccelA,
            const Eigen::Vector3d& linearAccelB,
            const Eigen::Vector3d& angularAccelB,
            double t);

        // ─── 状态保存与恢复 ───

        /// @brief 实体 A 的初始 Transform（算法入口时保存）
        Transform m_initialTransformA{};

        /// @brief 实体 B 的初始 Transform（算法入口时保存）
        Transform m_initialTransformB{};

        /// @brief 最近一次 GJK 结果缓存（用于 EPA 阶段）
        GJKResult3D m_lastGjkResult{};
    };

} // namespace egret

#endif // EGRET_PHYSICS_CONTINUOUS_COLLISION_DETECTION_H
