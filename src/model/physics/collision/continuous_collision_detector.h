/**
 * @file        continuous_collision_detector.h
 * @brief       CCD（Continuous Collision Detection）连续碰撞检测算法
 * @details     通过保守推进（Conservative Advancement）框架，在连续时间域内
 *              检测两个凸体之间的碰撞时间（TOI），避免离散检测的隧穿效应。
 *              支持 LINEAR / SCREW / ACCELERATED 三级运动模型。
 *              包含内部 GJK 距离查询与 EPA 碰撞信息提取。
 *
 *              核心原理：闵可夫斯基差 C(t) = A(t) ⊖ B(t) 上的距离函数 d(t)
 *              是 L-利普希茨连续的，保守步长 Δt ≤ d(t)/L 保证不遗漏碰撞。
 *
 * @author      jehor
 * @date        2026/6/25
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 */

#ifndef EGRET_PHYSICS_CONTINUOUS_COLLISION_DETECTOR_H
#define EGRET_PHYSICS_CONTINUOUS_COLLISION_DETECTOR_H

#include "dimension_concept.h"
#include "../../motion/generalization_rigid_motion.h"
#include "../../motion/motion_utils.h"
#include <Eigen/Dense>
#include <functional>
#include <limits>
#include <vector>
#include <unordered_set>
#include <algorithm>

namespace egret
{
    // CCDResult —— CCD 算法的输出结构体

    /**
     * @brief       CCD 连续碰撞检测结果
     * @details     包含碰撞状态、碰撞时间（TOI）、以及碰撞响应所需的几何信息。
     *              当 isCollided == true 时，法线/穿透深度/接触点/最近点对有效；
     *              当 isCollided == false 时，timeOfImpact 为无穷大。
     *
     * @tparam     Dimension  空间维度（≥ 2）
     */
    template <unsigned int Dimension> requires ValidDimension<Dimension>
    struct CCDResult
    {
        /// @brief 空间向量类型
        using vector_type = Eigen::Matrix<double, Dimension, 1>;

        /// @brief 是否发生碰撞
        bool isCollided{false};

        /// @brief 碰撞时间（Time of Impact），未碰撞时为 +∞
        double timeOfImpact{std::numeric_limits<double>::infinity()};

        /// @brief 碰撞法线（单位向量，方向为 B 指向 A，即分离方向）
        vector_type normal{vector_type::Zero()};

        /// @brief 穿透深度 δ = ‖p*‖
        double penetrationDepth{0.0};

        /// @brief 接触点 c = (a* + b*) / 2
        vector_type contactPoint{vector_type::Zero()};

        /// @brief 原始物体上的最近点对 (a* ∈ A, b* ∈ B)
        std::pair<vector_type, vector_type> closestPair{
            vector_type::Zero(), vector_type::Zero()
        };
    };


    // InternalGJKResult —— CCD 内部 GJK 距离查询结果

    /**
     * @brief       CCD 内部 GJK 距离查询的返回结构体
     * @details     包含碰撞判定、单纯形、支撑历史、分离距离和最近点对。
     *              供 CCD 主循环使用，碰撞时交由内部 EPA 进一步提取接触信息。
     *
     * @tparam     Dimension  空间维度（≥ 2）
     */
    template <unsigned int Dimension> requires ValidDimension<Dimension>
    struct InternalGJKResult
    {
        using vector_type = Eigen::Matrix<double, Dimension, 1>;
        using matrix_type = Eigen::Matrix<double, Dimension, Eigen::Dynamic>;

        /// @brief 是否发生碰撞（原点在闵可夫斯基差内部）
        bool isCollided{false};

        /// @brief GJK 最终单纯形矩阵（Dimension × k，每列为一个顶点）
        matrix_type simplex{};

        /// @brief 与 simplex 各列对应的凸体 A 支撑点（按迭代顺序）
        std::vector<vector_type> supportAHistory{};

        /// @brief 与 simplex 各列对应的凸体 B 支撑点（按迭代顺序）
        std::vector<vector_type> supportBHistory{};

        /// @brief 分离距离 d₀（仅在 isCollided == false 时有效）
        double distance{0.0};

        /// @brief 闵可夫斯基差上距原点最近的点 p*（仅在 isCollided == false 时有效）
        vector_type closestPointMinkowski{vector_type::Zero()};

        /// @brief 原始物体上的最近点对 (a₀, b₀)（仅在 isCollided == false 时有效）
        std::pair<vector_type, vector_type> closestPair{
            vector_type::Zero(), vector_type::Zero()
        };
    };


    // ContinuousCollisionDetector —— CCD 连续碰撞检测器

    /**
     * @brief       CCD 连续碰撞检测器
     * @details     通过保守推进框架，在时间区间 [0, T] 内追踪两个凸体的连续运动，
     *              精确计算碰撞时间（TOI）和碰撞信息。
     *
     *              使用方式：
     *              1. 构造检测器，传入两个刚体的运动状态、支撑函数回调以及运动模型
     *              2. 调用 run(T) 执行检测
     *              3. 从 CCDResult 读取结果
     *
     * @tparam     Dimension  空间维度（≥ 2）
     *
     * @invariant  保守推进保证不遗漏碰撞事件（无隧穿）
     * @invariant  内部 GJK/EPA 收敛容差通过 kEpsilon 控制
     */
    template <unsigned int Dimension> requires ValidDimension<Dimension>
    class ContinuousCollisionDetector
    {
    public:
        // ── 类型别名 ──

        /// @brief n 维列向量
        using vector_type = Eigen::Matrix<double, Dimension, 1>;

        /// @brief n×n 矩阵
        using matrix_type = Eigen::Matrix<double, Dimension, Dimension>;

        /// @brief 动态列数矩阵（单纯形等）
        using dynamic_matrix_type = Eigen::Matrix<double, Dimension, Eigen::Dynamic>;

        /// @brief 运动状态类型
        using motion_type = GeneralizationRigidMotion<Dimension>;

        /// @brief 支撑函数回调：给定世界空间方向、位置和旋转，返回世界空间支撑点
        /// @param worldDir  世界空间中的方向向量
        /// @param position  物体中心的世界空间位置
        /// @param rotation  物体的世界空间旋转矩阵 R ∈ SO(n)
        /// @return 世界空间中的支撑点
        using SupportCallback = std::function<vector_type(
            const vector_type& worldDir,
            const vector_type& position,
            const matrix_type& rotation
        )>;

        /// @brief CCD 结果类型
        using result_type = CCDResult<Dimension>;

        /// @brief 内部 GJK 结果类型
        using internal_gjk_result_type = InternalGJKResult<Dimension>;

        // ── 编译期常量 ──

        /// @brief GJK 原点包含判定容差（与旧版 collide_judge 的 kGjkEpsilon 对齐）
        static constexpr double kEpsilonGJK = 1e-8;

        /// @brief EPA 收敛容差（与旧版 collide_judge 的 kEpaTolerance 对齐，1e-6）
        static constexpr double kEpsilonEPA = 1e-6;

        /// @brief 分离距离容差（低于此值视为接触，与旧版 EPA 容差对齐）
        static constexpr double kEpsilonDistance = 1e-6;

        /// @brief 速度上界容差（低于此值视为无接近趋势）
        static constexpr double kEpsilonVelocity = 1e-12;

        /// @brief 最小时间步长（低于此值终止循环）
        static constexpr double kMinTimeStep = 1e-10;

        /// @brief GJK 最大迭代次数
        static constexpr int kMaxGJKIterations = 64;

        /// @brief EPA 最大迭代次数
        static constexpr int kMaxEPAIterations = 100;

        /// @brief TOI 精化最大迭代次数
        static constexpr int kMaxRefineIterations = 20;

        // ── 构造 ──

        /**
         * @brief   构造 CCD 检测器
         * @details 传入两个刚体的运动状态引用（只读）、支撑函数回调和运动模型。
         *          运动状态引用必须在检测器生命周期内保持有效。
         *
         * @param   motionA   物体 A 的运动状态（只读引用）
         * @param   motionB   物体 B 的运动状态（只读引用）
         * @param   supportA  物体 A 的支撑函数回调
         * @param   supportB  物体 B 的支撑函数回调
         * @param   model     运动模型（LINEAR / SCREW / ACCELERATED）
         */
        ContinuousCollisionDetector(
            const motion_type& motionA,
            const motion_type& motionB,
            SupportCallback supportA,
            SupportCallback supportB,
            MotionModel model
        );

        ~ContinuousCollisionDetector() = default;

        // ── 主检测接口 ──

        /**
         * @brief   执行连续碰撞检测
         * @details 在时间区间 [0, T] 内追踪物体运动，计算碰撞时间与碰撞信息。
         *          不执行 TOI 精化。
         *
         * @param   T  检测时间区间长度（> 0）
         * @return  CCDResult 包含碰撞状态、TOI 和碰撞/接触信息
         */
        result_type run(double T);

        /**
         * @brief   执行连续碰撞检测（带 TOI 精化）
         * @details 在保守推进检测到碰撞后，通过二分搜索精化 TOI 值。
         *
         * @param   T              检测时间区间长度（> 0）
         * @param   enableRefine   是否启用 TOI 精化
         * @param   maxRefineIter  精化最大迭代次数（默认 20）
         * @return  CCDResult 包含精化后的 TOI 和碰撞信息
         */
        result_type run(double T, bool enableRefine, int maxRefineIter = kMaxRefineIterations);

    private:
        // ── 速度上界计算 ──

        /**
         * @brief   计算时刻 t 的速度上界 L(t)
         * @details L = ‖v_A - v_B‖ + ‖Ω_A‖·r_A + ‖Ω_B‖·r_B
         *          反对称矩阵范数使用谱范数（最大奇异值），回退 Frobenius 范数。
         *
         * @param   t  当前时间（相对于检测起点）
         * @return  速度上界 L(t)
         */
        [[nodiscard]] double computeVelocityBound(double t) const;

        /**
         * @brief   获取物体在时刻 t 的瞬时速度
         * @param   motion  物体初始运动状态
         * @param   t       当前时间
         * @return  线速度向量与角速度矩阵的 pair
         */
        std::pair<vector_type, matrix_type> getVelocity(
            const motion_type& motion, double t) const;

        // ── 状态插值 ──

        /**
         * @brief   插值物体在时刻 t 的状态
         * @details 根据运动模型计算 t 时刻的位置和旋转矩阵。
         *
         *           LINEAR：  x(t) = x₀ + v₀·t,  R(t) = R₀
         *           SCREW：   x(t) = x₀ + v₀·t,  R(t) = R₀·exp(Ω₀·t)
         *           ACCELERATED：x(t) = x₀ + v₀·t + ½a·t²,
         *                       R(t) = R₀·exp(Ω₀·t + ½α·t²)
         *
         * @param   motion  物体初始运动状态
         * @param   t       当前时间
         * @return  位置向量与旋转矩阵的 pair
         */
        std::pair<vector_type, matrix_type> interpolateState(
            const motion_type& motion, double t) const;

        // ── 内部 GJK 距离查询 ──

        /**
         * @brief   在给定姿态下执行 GJK 距离查询
         * @details 检测两个物体在指定世界空间姿态下的碰撞状态与分离距离。
         *          返回值包含碰撞判定、单纯形、支撑历史和最近点对。
         *
         * @param   posA  物体 A 的世界空间位置
         * @param   rotA  物体 A 的世界空间旋转矩阵
         * @param   posB  物体 B 的世界空间位置
         * @param   rotB  物体 B 的世界空间旋转矩阵
         * @return  InternalGJKResult 碰撞判定与几何数据
         */
        internal_gjk_result_type computeGJKDistance(
            const vector_type& posA, const matrix_type& rotA,
            const vector_type& posB, const matrix_type& rotB) const;

        /**
         * @brief   GJK 原点包含性判定子函数 C_o(σ, d)
         * @details 枚举单纯形所有非空子集，通过 Gram 矩阵求解重心坐标，
         *          找到距离原点最近的子单纯形上的最近点。
         *          若 ‖p*‖ < ε 则原点在单纯形内部。
         *          否则更新搜索方向为 -p* 并裁剪单纯形。
         *
         * @param   simplex         当前单纯形矩阵（会被裁剪）
         * @param   supportAHistory  对应的 A 支撑历史（会被裁剪）
         * @param   supportBHistory  对应的 B 支撑历史（会被裁剪）
         * @param   searchDir        搜索方向（会被更新为 -p*）
         * @return  true 若原点在单纯形内部（碰撞），false 否则
         */
        bool containsOrigin(
            dynamic_matrix_type& simplex,
            std::vector<vector_type>& supportAHistory,
            std::vector<vector_type>& supportBHistory,
            vector_type& searchDir) const;

        /**
         * @brief   计算单纯形上距离原点最近的子单纯形
         * @details 枚举子集、求解 Gram 矩阵、筛选有效重心坐标、选取最优解。
         *
         * @param   simplex  单纯形矩阵
         * @param   outSubsetIndices  输出：最优子集的顶点索引
         * @param   outBarycentric    输出：最优子集的重心坐标 λ
         * @param   outClosestPoint   输出：最优子集上的最近点 p*
         * @return  最近点到原点的距离 ‖p*‖
         */
        double computeOptimalSubset(
            const dynamic_matrix_type& simplex,
            std::vector<int>& outSubsetIndices,
            Eigen::VectorXd& outBarycentric,
            vector_type& outClosestPoint) const;

        // ── 内部 EPA 碰撞信息提取 ──

        /**
         * @brief   从 GJK 碰撞单纯形出发，执行 EPA 提取碰撞信息
         * @details 以 GJK 单纯形为初始多面体，沿法向量方向获取新支撑点迭代扩展，
         *          逼近闵可夫斯基差边界，提取碰撞法线、穿透深度和接触点。
         *
         * @param   gjkResult  GJK 碰撞结果（isCollided == true）
         * @param   posA      物体 A 的世界空间位置
         * @param   rotA      物体 A 的世界空间旋转矩阵
         * @param   posB      物体 B 的世界空间位置
         * @param   rotB      物体 B 的世界空间旋转矩阵
         * @return  CCDResult 中的碰撞信息部分（法线、穿透深度、接触点、最近点对）
         */
        result_type computeEPA(
            const internal_gjk_result_type& gjkResult,
            const vector_type& posA, const matrix_type& rotA,
            const vector_type& posB, const matrix_type& rotB) const;

        // ── 接触信息提取（近接触情形） ──

        /**
         * @brief   从 GJK 最近点信息构造接触数据（分离距离极小但未穿透时）
         * @details 当 d < ε_d 而 GJK 报告未碰撞时，不再调用 EPA，
         *          直接利用 GJK 返回的最近点对构造接触法线和接触点。
         *
         * @param   gjkResult  GJK 分离结果（isCollided == false, distance < ε_d）
         * @param   t          当前时间
         * @return  CCDResult 包含接触信息（碰撞法线由 p* 方向确定）
         */
        result_type extractContactFromGJK(
            const internal_gjk_result_type& gjkResult, double t) const;

        // ── TOI 精化 ──

        /**
         * @brief   二分搜索精化 TOI
         * @details 在已知的分离时刻 tLo 和碰撞时刻 tHi 之间执行二分搜索，
         *          找到精确的碰撞时间。
         *
         * @param   tLo           已知分离的时刻
         * @param   tHi           已知碰撞的时刻
         * @param   maxIterations 最大迭代次数
         * @return  精化后的 CCDResult
         */
        result_type refineTOI(double tLo, double tHi, int maxIterations) const;

        // ── 成员变量 ──

        const motion_type& m_motionA; ///< 物体 A 的运动状态（只读引用）
        const motion_type& m_motionB; ///< 物体 B 的运动状态（只读引用）

        SupportCallback m_supportA; ///< 物体 A 的支撑函数回调
        SupportCallback m_supportB; ///< 物体 B 的支撑函数回调

        MotionModel m_model; ///< 当前运动模型

        double m_radiusA; ///< 物体 A 的包围半径 r_A
        double m_radiusB; ///< 物体 B 的包围半径 r_B

        double m_constVelocityBound; ///< LINEAR/SCREW 模型下的恒定速度上界（预计算）
    };


    // 方法实现

    // ── 构造 ──

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    ContinuousCollisionDetector<Dimension>::ContinuousCollisionDetector(
        const motion_type& motionA,
        const motion_type& motionB,
        SupportCallback supportA,
        SupportCallback supportB,
        MotionModel model
    )
        : m_motionA(motionA)
          , m_motionB(motionB)
          , m_supportA(std::move(supportA))
          , m_supportB(std::move(supportB))
          , m_model(model)
          , m_radiusA(motionA.boundingRadius())
          , m_radiusB(motionB.boundingRadius())
          , m_constVelocityBound(0.0)
    {
        // 预计算 LINEAR / SCREW 模型下的恒定速度上界
        if (m_model == MotionModel::Linear || m_model == MotionModel::Screw) {
            m_constVelocityBound = computeVelocityBound(0.0);
        }
    }

    // ── 主检测接口 ──

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    auto ContinuousCollisionDetector<Dimension>::run(double T) -> result_type
    {
        return run(T, false, 0);
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    auto ContinuousCollisionDetector<Dimension>::run(
        double T, bool enableRefine, int maxRefineIter) -> result_type
    {
        // ── 预处理：检查速度上界 ──
        if (m_model == MotionModel::Linear || m_model == MotionModel::Screw) {
            if (m_constVelocityBound < kEpsilonVelocity) {
                // 无相对运动趋势
                result_type noCollision;
                noCollision.isCollided = false;
                noCollision.timeOfImpact = std::numeric_limits<double>::infinity();
                return noCollision;
            }
        }

        // ── 主循环 ──
        double t{0.0};

        while (t < T) {
            // ① 插值当前时刻的状态
            const auto [posA, rotA] = interpolateState(m_motionA, t);
            const auto [posB, rotB] = interpolateState(m_motionB, t);

            // ② 调用内部 GJK 计算分离距离或碰撞判定
            const internal_gjk_result_type gjkResult = computeGJKDistance(
                posA, rotA, posB, rotB);

            if (gjkResult.isCollided) {
                // 已穿透 → 调用 EPA 提取碰撞信息
                result_type epaResult = computeEPA(gjkResult, posA, rotA, posB, rotB);
                epaResult.isCollided = true;
                epaResult.timeOfImpact = t;

                // TOI 精化
                if (enableRefine && t > 0.0) {
                    // 使用 EPA 最近点对计算接触法线方向的实际相对接近速度，
                    // 替代全局速度上界 m_constVelocityBound。
                    // 全局上界在擦边碰撞时会严重高估分离速度，导致 tLo 过近、
                    // 二分区间两端都处于穿透状态，精化收敛到错误的 TOI。
                    const auto [vA, omegaA] = getVelocity(m_motionA, t);
                    const auto [vB, omegaB] = getVelocity(m_motionB, t);

                    double approachSpeed = m_constVelocityBound; // 回退值

                    if constexpr (Dimension == 3) {
                        // 从反对称矩阵提取角速度向量
                        const Eigen::Vector3d angVelA(omegaA(2, 1), omegaA(0, 2), omegaA(1, 0));
                        const Eigen::Vector3d angVelB(omegaB(2, 1), omegaB(0, 2), omegaB(1, 0));

                        const auto& [aStar, bStar] = epaResult.closestPair;
                        const Eigen::Vector3d rA = aStar - posA;
                        const Eigen::Vector3d rB = bStar - posB;

                        const Eigen::Vector3d vContactA = vA + angVelA.cross(rA);
                        const Eigen::Vector3d vContactB = vB + angVelB.cross(rB);

                        // 正值 = 正在接近
                        const double relApproach = -(vContactB - vContactA).dot(epaResult.normal);
                        if (relApproach > kEpsilonVelocity) {
                            approachSpeed = relApproach;
                        }
                    }

                    const double tLo = std::max(0.0, t - (epaResult.penetrationDepth > 0.0
                                                              ? epaResult.penetrationDepth / std::max(
                                                                  approachSpeed, kEpsilonVelocity)
                                                              : kMinTimeStep));
                    return refineTOI(tLo, t, maxRefineIter);
                }

                return epaResult;
            }

            // ③ 提取分离距离
            const double d = gjkResult.distance;

            if (d < kEpsilonDistance) {
                // 距离极小 → 视为接触
                return extractContactFromGJK(gjkResult, t);
            }

            // ④ 确定速度上界
            double L{0.0};

            if (m_model == MotionModel::Linear || m_model == MotionModel::Screw) {
                L = m_constVelocityBound;
            }
            else {
                // ACCELERATED 模型：预估-校正
                L = computeVelocityBound(t);

                if (L < kEpsilonVelocity) {
                    result_type noCollision;
                    noCollision.isCollided = false;
                    noCollision.timeOfImpact = std::numeric_limits<double>::infinity();
                    return noCollision;
                }

                double dtCandidate = d / L;
                dtCandidate = std::min(dtCandidate, T - t);

                const double LEnd = computeVelocityBound(t + dtCandidate);
                L = std::max(L, LEnd);
            }

            if (L < kEpsilonVelocity) {
                result_type noCollision;
                noCollision.isCollided = false;
                noCollision.timeOfImpact = std::numeric_limits<double>::infinity();
                return noCollision;
            }

            // ⑤ 保守时间步长
            double dt = d / L;
            dt = std::min(dt, T - t);

            if (dt < kMinTimeStep) {
                result_type noCollision;
                noCollision.isCollided = false;
                noCollision.timeOfImpact = std::numeric_limits<double>::infinity();
                return noCollision;
            }

            // ⑥ 推进
            t += dt;
        }

        // ── 循环结束仍未碰撞 ──
        result_type noCollision;
        noCollision.isCollided = false;
        noCollision.timeOfImpact = std::numeric_limits<double>::infinity();
        return noCollision;
    }

    // ── 速度上界计算 ──

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    double ContinuousCollisionDetector<Dimension>::computeVelocityBound(double t) const
    {
        const auto [vA, omegaA] = getVelocity(m_motionA, t);
        const auto [vB, omegaB] = getVelocity(m_motionB, t);

        const double vRelNorm = (vA - vB).norm();

        // 使用谱范数（最大奇异值），回退到 Frobenius 范数
        double omegaANorm{0.0};
        double omegaBNorm{0.0};

        if constexpr (Dimension <= 3) {
            // 低维情况下谱范数计算代价低
            omegaANorm = spectralNorm<Dimension>(omegaA);
            omegaBNorm = spectralNorm<Dimension>(omegaB);
        }
        else {
            // 高维情况使用 Frobenius 范数作为保守上界
            // ‖Ω‖_2 ≤ ‖Ω‖_F = √(2·Σ_{i<j} Ω_{ij}²)
            omegaANorm = frobeniusNorm<Dimension>(omegaA);
            omegaBNorm = frobeniusNorm<Dimension>(omegaB);
        }

        return vRelNorm + omegaANorm * m_radiusA + omegaBNorm * m_radiusB;
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    auto ContinuousCollisionDetector<Dimension>::getVelocity(
        const motion_type& motion, double t) const -> std::pair<vector_type, matrix_type>
    {
        switch (m_model) {
        case MotionModel::Linear:
            return {motion.velocity(), matrix_type::Zero()};

        case MotionModel::Screw:
            return {motion.velocity(), motion.angularVelocity()};

        case MotionModel::Accelerated: {
            const vector_type v = motion.velocity() + motion.acceleration() * t;
            const matrix_type omega = motion.angularVelocity()
                + motion.angularAcceleration() * t;
            return {v, omega};
        }
        }

        return {vector_type::Zero(), matrix_type::Zero()};
    }

    // ── 状态插值 ──

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    auto ContinuousCollisionDetector<Dimension>::interpolateState(
        const motion_type& motion, double t) const -> std::pair<vector_type, matrix_type>
    {
        vector_type position{};
        matrix_type rotation{};

        switch (m_model) {
        case MotionModel::Linear: {
            position = motion.position() + motion.velocity() * t;
            rotation = motion.attitude();
            break;
        }

        case MotionModel::Screw: {
            position = motion.position() + motion.velocity() * t;
            const matrix_type Theta = motion.angularVelocity() * t;
            rotation = motion.attitude() * matrixExponential<Dimension>(Theta);
            break;
        }

        case MotionModel::Accelerated: {
            position = motion.position()
                + motion.velocity() * t
                + 0.5 * motion.acceleration() * (t * t);

            const matrix_type Theta = motion.angularVelocity() * t
                + 0.5 * motion.angularAcceleration() * (t * t);
            rotation = motion.attitude() * matrixExponential<Dimension>(Theta);
            break;
        }
        }

        return {position, rotation};
    }

    // ── 内部 GJK 距离查询 ──

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    auto ContinuousCollisionDetector<Dimension>::computeGJKDistance(
        const vector_type& posA, const matrix_type& rotA,
        const vector_type& posB, const matrix_type& rotB) const -> internal_gjk_result_type
    {
        internal_gjk_result_type result;

        // ── 闵可夫斯基差支撑函数 ──
        auto minkowskiSupport = [&](const vector_type& dir) -> vector_type
        {
            const vector_type sA = m_supportA(dir, posA, rotA);
            const vector_type sB = m_supportB(-dir, posB, rotB);
            return sA - sB;
        };

        // ── 初始化 ──
        // 初始方向：center(B) - center(A)
        vector_type dir = posB - posA;
        if (dir.squaredNorm() < kEpsilonGJK * kEpsilonGJK) {
            dir = vector_type::Zero();
            dir(Dimension - 1) = 1.0;
        }

        // 第一个支撑点
        const vector_type p0 = minkowskiSupport(dir);
        dynamic_matrix_type simplex(Dimension, 1);
        simplex.col(0) = p0;

        std::vector<vector_type> supportAHistory{};
        std::vector<vector_type> supportBHistory{};
        supportAHistory.push_back(m_supportA(dir, posA, rotA));
        supportBHistory.push_back(m_supportB(-dir, posB, rotB));

        // 更新搜索方向
        dir = -p0;

        // ── 主迭代循环 ──
        for (int iter = 0; iter < kMaxGJKIterations; ++iter) {
            // 获取新支撑点
            const vector_type p = minkowskiSupport(dir);
            supportAHistory.push_back(m_supportA(dir, posA, rotA));
            supportBHistory.push_back(m_supportB(-dir, posB, rotB));

            // 检查是否越过原点
            if (p.dot(dir) < 0.0) {
                // 原点不在差集内部 → 未碰撞
                result.isCollided = false;

                // 计算分离距离与最近点对
                std::vector<int> subsetIndices{};
                Eigen::VectorXd barycentric{};
                vector_type closestPoint{};
                const double distance = computeOptimalSubset(
                    simplex, subsetIndices, barycentric, closestPoint);

                result.distance = distance;
                result.closestPointMinkowski = closestPoint;

                // 计算原始物体最近点对
                vector_type a0 = vector_type::Zero();
                vector_type b0 = vector_type::Zero();
                for (Eigen::Index j = 0; j < static_cast<Eigen::Index>(subsetIndices.size()); ++j) {
                    const int idx = subsetIndices[static_cast<size_t>(j)];
                    if (idx < static_cast<int>(supportAHistory.size())) {
                        a0 += barycentric(j) * supportAHistory[static_cast<size_t>(idx)];
                        b0 += barycentric(j) * supportBHistory[static_cast<size_t>(idx)];
                    }
                }
                result.closestPair = {a0, b0};

                // 填充单纯形和支撑历史
                result.simplex = simplex;
                result.supportAHistory = supportAHistory;
                result.supportBHistory = supportBHistory;

                return result;
            }

            // 将新支撑点加入单纯形
            simplex.conservativeResize(Eigen::NoChange, simplex.cols() + 1);
            simplex.col(simplex.cols() - 1) = p;

            // 检查是否包含原点
            if (containsOrigin(simplex, supportAHistory, supportBHistory, dir)) {
                // 碰撞！
                result.isCollided = true;
                result.simplex = simplex;
                result.supportAHistory = supportAHistory;
                result.supportBHistory = supportBHistory;
                return result;
            }
            // containsOrigin 已更新 dir 并裁剪 simplex
        }

        // 迭代次数耗尽：视为未碰撞，使用最后一个单纯形计算距离
        result.isCollided = false;

        std::vector<int> subsetIndices{};
        Eigen::VectorXd barycentric{};
        vector_type closestPoint{};
        const double distance = computeOptimalSubset(
            simplex, subsetIndices, barycentric, closestPoint);

        result.distance = distance;
        result.closestPointMinkowski = closestPoint;

        vector_type a0 = vector_type::Zero();
        vector_type b0 = vector_type::Zero();
        for (Eigen::Index j = 0; j < static_cast<Eigen::Index>(subsetIndices.size()); ++j) {
            const int idx = subsetIndices[static_cast<size_t>(j)];
            if (idx < static_cast<int>(supportAHistory.size())) {
                a0 += barycentric(j) * supportAHistory[static_cast<size_t>(idx)];
                b0 += barycentric(j) * supportBHistory[static_cast<size_t>(idx)];
            }
        }
        result.closestPair = {a0, b0};
        result.simplex = simplex;
        result.supportAHistory = supportAHistory;
        result.supportBHistory = supportBHistory;

        return result;
    }

    // ── GJK 原点包含性判定 ──

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    bool ContinuousCollisionDetector<Dimension>::containsOrigin(
        dynamic_matrix_type& simplex,
        std::vector<vector_type>& supportAHistory,
        std::vector<vector_type>& supportBHistory,
        vector_type& searchDir) const
    {
        std::vector<int> subsetIndices{};
        Eigen::VectorXd barycentric{};
        vector_type closestPoint{};

        const double distance = computeOptimalSubset(
            simplex, subsetIndices, barycentric, closestPoint);

        // 原点包含判定
        if (distance < kEpsilonGJK) {
            return true;
        }

        // 未碰撞：更新搜索方向
        searchDir = -closestPoint;

        // 裁剪单纯形，仅保留最优子集的顶点
        const auto newCols = static_cast<Eigen::Index>(subsetIndices.size());
        dynamic_matrix_type newSimplex(Dimension, newCols);
        std::vector<vector_type> newSupportA{};
        std::vector<vector_type> newSupportB{};
        newSupportA.reserve(subsetIndices.size());
        newSupportB.reserve(subsetIndices.size());

        for (size_t j = 0; j < subsetIndices.size(); ++j) {
            const int idx = subsetIndices[j];
            newSimplex.col(static_cast<Eigen::Index>(j)) = simplex.col(idx);
            if (idx < static_cast<int>(supportAHistory.size())) {
                newSupportA.push_back(supportAHistory[static_cast<size_t>(idx)]);
                newSupportB.push_back(supportBHistory[static_cast<size_t>(idx)]);
            }
        }

        simplex = std::move(newSimplex);
        supportAHistory = std::move(newSupportA);
        supportBHistory = std::move(newSupportB);

        return false;
    }

    // ── GJK 最优子集计算 ──

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    double ContinuousCollisionDetector<Dimension>::computeOptimalSubset(
        const dynamic_matrix_type& simplex,
        std::vector<int>& outSubsetIndices,
        Eigen::VectorXd& outBarycentric,
        vector_type& outClosestPoint) const
    {
        const Eigen::Index N = simplex.cols();

        double bestDistance{std::numeric_limits<double>::max()};
        std::vector<int> bestSubset{};
        Eigen::VectorXd bestLambda{};
        vector_type bestClosestPoint = vector_type::Zero();

        // 枚举所有非空子集（位掩码）
        const int totalSubsets = 1 << static_cast<int>(N);
        for (int mask = 1; mask < totalSubsets; ++mask) {
            // 提取子集索引
            std::vector<int> subset{};
            subset.reserve(static_cast<size_t>(N));
            for (int i = 0; i < static_cast<int>(N); ++i) {
                if (mask & (1 << i)) {
                    subset.push_back(i);
                }
            }

            const int m = static_cast<int>(subset.size()) - 1;
            const int gramSize = m + 2;

            // 构造 Gram 矩阵
            Eigen::MatrixXd gramMatrix = Eigen::MatrixXd::Zero(gramSize, gramSize);
            for (int r = 0; r <= m; ++r) {
                const int ir = subset[r];
                for (int c = 0; c <= m; ++c) {
                    const int ic = subset[c];
                    gramMatrix(r, c) = simplex.col(ir).dot(simplex.col(ic));
                }
                gramMatrix(r, m + 1) = 1.0;
                gramMatrix(m + 1, r) = 1.0;
            }
            gramMatrix(m + 1, m + 1) = 0.0;

            // 右端向量 b = [0, ..., 0, 1]ᵀ
            Eigen::VectorXd b = Eigen::VectorXd::Zero(gramSize);
            b(m + 1) = 1.0;

            // 求解 G · [λ, μ]ᵀ = b
            Eigen::FullPivLU<Eigen::MatrixXd> lu(gramMatrix);
            if (!lu.isInvertible()) {
                continue;
            }

            Eigen::VectorXd x = lu.solve(b);

            // 检查重心坐标是否全为正
            bool allPositive{true};
            for (int j = 0; j <= m; ++j) {
                if (x(j) <= -kEpsilonGJK) {
                    allPositive = false;
                    break;
                }
            }

            if (!allPositive) {
                continue;
            }

            // 计算最近点 p* = Σ λ_j v_{i_j}
            vector_type pStar = vector_type::Zero();
            for (int j = 0; j <= m; ++j) {
                pStar += x(j) * simplex.col(subset[j]);
            }

            const double dist = pStar.norm();

            if (dist < bestDistance) {
                bestDistance = dist;
                bestSubset = std::move(subset);
                bestLambda = x.head(m + 1);
                bestClosestPoint = pStar;
            }
        }

        // 输出最优子集信息
        outSubsetIndices = std::move(bestSubset);
        outBarycentric = std::move(bestLambda);
        outClosestPoint = bestClosestPoint;

        return bestDistance;
    }

    // ── 内部 EPA 碰撞信息提取 ──

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    auto ContinuousCollisionDetector<Dimension>::computeEPA(
        const internal_gjk_result_type& gjkResult,
        const vector_type& posA, const matrix_type& rotA,
        const vector_type& posB, const matrix_type& rotB) const -> result_type
    {
        result_type epaResult{};

        // ── 闵可夫斯基差支撑函数 ──
        auto minkowskiSupport = [&](const vector_type& dir) -> vector_type
        {
            const vector_type sA = m_supportA(dir, posA, rotA);
            const vector_type sB = m_supportB(-dir, posB, rotB);
            return sA - sB;
        };

        // ── EPA 面的数据结构 ──
        struct EPAFace
        {
            std::vector<int> vertexIndices{};
            vector_type normal{vector_type::Zero()};
            double distance{0.0};
        };

        // ── 初始化多面体 ──
        dynamic_matrix_type vertices = gjkResult.simplex;
        std::vector<vector_type> supportAHist = gjkResult.supportAHistory;
        std::vector<vector_type> supportBHist = gjkResult.supportBHistory;

        const Eigen::Index initCols = vertices.cols();

        // 若初始顶点数不足 Dimension + 1，先扩展一次
        if (initCols < static_cast<Eigen::Index>(Dimension + 1)) {
            vector_type extDir{};

            if (initCols == 1) {
                extDir = vector_type::Zero();
                extDir(0) = 1.0;
            }
            else {
                // 构建偏移矩阵
                Eigen::Matrix<double, Dimension, Eigen::Dynamic> centered(
                    Dimension, initCols - 1);
                for (Eigen::Index j = 1; j < initCols; ++j) {
                    centered.col(j - 1) = vertices.col(j) - vertices.col(0);
                }
                Eigen::FullPivHouseholderQR<
                    Eigen::Matrix<double, Dimension, Eigen::Dynamic>
                > qr(centered);
                const Eigen::Index rank = qr.rank();
                if (rank < static_cast<Eigen::Index>(Dimension)) {
                    extDir = qr.matrixQ().col(rank);
                }
                else {
                    extDir = vector_type::Zero();
                    extDir(Dimension - 1) = 1.0;
                }
            }

            if (extDir.squaredNorm() > kEpsilonEPA * kEpsilonEPA) {
                extDir.normalize();
                const vector_type sP = minkowskiSupport(extDir);
                vertices.conservativeResize(Eigen::NoChange, vertices.cols() + 1);
                vertices.col(vertices.cols() - 1) = sP;
                supportAHist.push_back(m_supportA(extDir, posA, rotA));
                supportBHist.push_back(m_supportB(-extDir, posB, rotB));
            }
        }

        // ── 面计算辅助函数 ──
        auto computeFaceNormal = [&](EPAFace& face)
        {
            const int k = static_cast<int>(face.vertexIndices.size());
            if (k < 1) {
                face.normal = vector_type::Zero();
                face.distance = 0.0;
                return;
            }

            // 构建 V_i 矩阵
            Eigen::Matrix<double, Dimension, Eigen::Dynamic> Vi(Dimension, k);
            for (int c = 0; c < k; ++c) {
                Vi.col(c) = vertices.col(face.vertexIndices[c]);
            }

            const Eigen::VectorXd ones = Eigen::VectorXd::Ones(k);
            const Eigen::MatrixXd ViT = Vi.transpose();
            vector_type ni = ViT.completeOrthogonalDecomposition().solve(ones);

            const double niNorm = ni.norm();
            if (niNorm < kEpsilonEPA) {
                face.normal = vector_type::Zero();
                face.distance = 0.0;
                return;
            }

            face.normal = ni / niNorm;
            face.distance = 1.0 / niNorm;

            // 确保法向量指向多面体外部
            const vector_type v0 = vertices.col(face.vertexIndices[0]);
            if (face.normal.dot(v0) < 0.0) {
                face.normal = -face.normal;
            }
        };

        auto rebuildFaces = [&]() -> std::vector<EPAFace>
        {
            std::vector<EPAFace> faces{};
            const Eigen::Index nCols = vertices.cols();

            if (nCols < 2) {
                return faces;
            }

            for (Eigen::Index i = 0; i < nCols; ++i) {
                EPAFace face{};
                face.vertexIndices.reserve(static_cast<size_t>(nCols - 1));
                for (Eigen::Index j = 0; j < nCols; ++j) {
                    if (j != i) {
                        face.vertexIndices.push_back(static_cast<int>(j));
                    }
                }
                computeFaceNormal(face);
                if (face.normal.squaredNorm() > kEpsilonEPA * kEpsilonEPA) {
                    faces.push_back(std::move(face));
                }
            }
            return faces;
        };

        // 初始化面列表
        std::vector<EPAFace> faces = rebuildFaces();

        // ── EPA 主迭代 ──
        for (int iter = 0; iter < kMaxEPAIterations; ++iter) {
            if (faces.empty()) {
                break;
            }

            // 步骤 1：寻找最近面
            int closestIdx{0};
            double minDist = faces[0].distance;
            for (size_t i = 1; i < faces.size(); ++i) {
                if (faces[i].distance < minDist) {
                    minDist = faces[i].distance;
                    closestIdx = static_cast<int>(i);
                }
            }

            const EPAFace& closestFace = faces[closestIdx];
            const vector_type n = closestFace.normal;
            const double dMin = closestFace.distance;

            // 步骤 2：沿法向量方向获取新支撑点
            const vector_type supportP = minkowskiSupport(n);

            // 步骤 3：终止条件检查
            const double projection = supportP.dot(n);
            if (projection - dMin <= kEpsilonEPA) {
                // 收敛：提取结果
                const vector_type pStar = dMin * n;

                // 计算 p* 在最近面上的重心坐标
                const int k = static_cast<int>(closestFace.vertexIndices.size());
                std::vector<vector_type> faceVerts(static_cast<size_t>(k));
                std::vector<vector_type> faceSA(static_cast<size_t>(k));
                std::vector<vector_type> faceSB(static_cast<size_t>(k));

                for (int j = 0; j < k; ++j) {
                    const int idx = closestFace.vertexIndices[j];
                    faceVerts[j] = vertices.col(idx);
                    faceSA[j] = (idx < static_cast<int>(supportAHist.size()))
                                    ? supportAHist[static_cast<size_t>(idx)]
                                    : vector_type::Zero();
                    faceSB[j] = (idx < static_cast<int>(supportBHist.size()))
                                    ? supportBHist[static_cast<size_t>(idx)]
                                    : vector_type::Zero();
                }

                Eigen::VectorXd lambda(k);
                if (k >= 2) {
                    Eigen::Matrix<double, Dimension, Eigen::Dynamic> D(Dimension, k - 1);
                    for (int j = 1; j < k; ++j) {
                        D.col(j - 1) = faceVerts[j] - faceVerts[0];
                    }
                    const vector_type q = pStar - faceVerts[0];

                    Eigen::VectorXd lambdaPrime = D.completeOrthogonalDecomposition().solve(q);
                    lambda(0) = 1.0 - lambdaPrime.sum();
                    for (int j = 1; j < k; ++j) {
                        lambda(j) = lambdaPrime(j - 1);
                    }

                    for (int j = 0; j < k; ++j) {
                        lambda(j) = std::max(0.0, std::min(1.0, lambda(j)));
                    }
                    const double sum = lambda.sum();
                    if (sum > kEpsilonEPA) {
                        lambda /= sum;
                    }
                    else {
                        lambda = Eigen::VectorXd::Constant(k, 1.0 / static_cast<double>(k));
                    }
                }
                else {
                    lambda = Eigen::VectorXd::Constant(k, 1.0 / static_cast<double>(k));
                }

                // 计算原始物体上的最近点
                vector_type aStar = vector_type::Zero();
                vector_type bStar = vector_type::Zero();
                for (int j = 0; j < k; ++j) {
                    aStar += lambda(j) * faceSA[j];
                    bStar += lambda(j) * faceSB[j];
                }

                epaResult.normal = n;
                epaResult.penetrationDepth = dMin;
                epaResult.contactPoint = (aStar + bStar) * 0.5;
                epaResult.closestPair = {aStar, bStar};
                return epaResult;
            }

            // 步骤 4：扩展多面体
            const int newVertexIdx = static_cast<int>(vertices.cols());
            vertices.conservativeResize(Eigen::NoChange, vertices.cols() + 1);
            vertices.col(newVertexIdx) = supportP;
            supportAHist.push_back(m_supportA(n, posA, rotA));
            supportBHist.push_back(m_supportB(-n, posB, rotB));

            // 识别可见面
            std::vector<int> visibleFaceIndices{};
            visibleFaceIndices.reserve(faces.size());
            for (size_t i = 0; i < faces.size(); ++i) {
                if (faces[i].normal.dot(supportP) > faces[i].distance + kEpsilonEPA) {
                    visibleFaceIndices.push_back(static_cast<int>(i));
                }
            }

            if (visibleFaceIndices.empty()) {
                // 新顶点在多面体内部，回退
                vertices.conservativeResize(Eigen::NoChange, vertices.cols() - 1);
                supportAHist.pop_back();
                supportBHist.pop_back();

                // 使用当前最近面提取结果
                const vector_type pStar = dMin * n;
                const int k = static_cast<int>(closestFace.vertexIndices.size());
                std::vector<vector_type> faceSA(static_cast<size_t>(k));
                std::vector<vector_type> faceSB(static_cast<size_t>(k));

                for (int j = 0; j < k; ++j) {
                    const int idx = closestFace.vertexIndices[j];
                    faceSA[j] = (idx < static_cast<int>(supportAHist.size()))
                                    ? supportAHist[static_cast<size_t>(idx)]
                                    : vector_type::Zero();
                    faceSB[j] = (idx < static_cast<int>(supportBHist.size()))
                                    ? supportBHist[static_cast<size_t>(idx)]
                                    : vector_type::Zero();
                }

                // 简化：取面上顶点的平均
                vector_type aStar = vector_type::Zero();
                vector_type bStar = vector_type::Zero();
                for (int j = 0; j < k; ++j) {
                    aStar += faceSA[j];
                    bStar += faceSB[j];
                }
                aStar /= static_cast<double>(k);
                bStar /= static_cast<double>(k);

                epaResult.normal = n;
                epaResult.penetrationDepth = dMin;
                epaResult.contactPoint = (aStar + bStar) * 0.5;
                epaResult.closestPair = {aStar, bStar};
                return epaResult;
            }

            // 收集边界脊
            struct VectorHash
            {
                size_t operator()(const std::vector<int>& v) const
                {
                    size_t h{0};
                    for (int x : v) {
                        h ^= std::hash<int>{}(x) + 0x9e3779b9 + (h << 6) + (h >> 2);
                    }
                    return h;
                }
            };

            std::unordered_set<std::vector<int>, VectorHash> boundaryRidges{};
            for (int fi : visibleFaceIndices) {
                const EPAFace& face = faces[fi];
                const int nv = static_cast<int>(face.vertexIndices.size());
                for (int omit = 0; omit < nv; ++omit) {
                    std::vector<int> ridge{};
                    ridge.reserve(static_cast<size_t>(nv - 1));
                    for (int j = 0; j < nv; ++j) {
                        if (j != omit) {
                            ridge.push_back(face.vertexIndices[j]);
                        }
                    }
                    std::ranges::sort(ridge);
                    if (boundaryRidges.contains(ridge)) {
                        boundaryRidges.erase(ridge);
                    }
                    else {
                        boundaryRidges.insert(std::move(ridge));
                    }
                }
            }

            // 移除可见面
            std::ranges::sort(visibleFaceIndices, std::greater());
            for (int fi : visibleFaceIndices) {
                faces.erase(faces.begin() + fi);
            }

            // 用边界脊 + 新顶点构建新面
            for (const auto& ridge : boundaryRidges) {
                EPAFace newFace{};
                newFace.vertexIndices = ridge;
                newFace.vertexIndices.push_back(newVertexIdx);
                computeFaceNormal(newFace);
                if (newFace.normal.squaredNorm() > kEpsilonEPA * kEpsilonEPA) {
                    faces.push_back(std::move(newFace));
                }
            }
        }

        // 迭代耗尽：使用当前最近面
        if (!faces.empty()) {
            int closestIdx{0};
            double minDist = faces[0].distance;
            for (size_t i = 1; i < faces.size(); ++i) {
                if (faces[i].distance < minDist) {
                    minDist = faces[i].distance;
                    closestIdx = static_cast<int>(i);
                }
            }

            const EPAFace& face = faces[closestIdx];
            // 简化提取
            const int k = static_cast<int>(face.vertexIndices.size());
            vector_type aStar = vector_type::Zero();
            vector_type bStar = vector_type::Zero();
            for (int j = 0; j < k; ++j) {
                const int idx = face.vertexIndices[j];
                aStar += (idx < static_cast<int>(supportAHist.size()))
                             ? supportAHist[static_cast<size_t>(idx)]
                             : vector_type::Zero();
                bStar += (idx < static_cast<int>(supportBHist.size()))
                             ? supportBHist[static_cast<size_t>(idx)]
                             : vector_type::Zero();
            }
            aStar /= static_cast<double>(k);
            bStar /= static_cast<double>(k);

            epaResult.normal = face.normal;
            epaResult.penetrationDepth = minDist;
            epaResult.contactPoint = (aStar + bStar) * 0.5;
            epaResult.closestPair = {aStar, bStar};
        }

        return epaResult;
    }

    // ── 接触信息提取（近接触情形） ──

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    auto ContinuousCollisionDetector<Dimension>::extractContactFromGJK(
        const internal_gjk_result_type& gjkResult, double t) const -> result_type
    {
        result_type contactResult{};

        const vector_type& pStar = gjkResult.closestPointMinkowski;
        const double d = pStar.norm();

        contactResult.isCollided = true;
        contactResult.timeOfImpact = t;

        if (d > kEpsilonGJK) {
            // 分离法线：方向为 p* 的单位向量（B 指向 A）
            contactResult.normal = pStar / d;
        }
        else {
            // 退化：取任意方向
            contactResult.normal = vector_type::Zero();
            contactResult.normal(0) = 1.0;
        }

        contactResult.penetrationDepth = 0.0; // 近接触，穿透深度为零
        contactResult.closestPair = gjkResult.closestPair;

        const auto& [a0, b0] = gjkResult.closestPair;
        contactResult.contactPoint = (a0 + b0) * 0.5;

        return contactResult;
    }

    // ── TOI 精化 ──

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    auto ContinuousCollisionDetector<Dimension>::refineTOI(
        double tLo, double tHi, int maxIterations) const -> result_type
    {
        // 二分搜索精化 TOI
        for (int iter = 0; iter < maxIterations; ++iter) {
            const double tMid = (tLo + tHi) * 0.5;

            if (tHi - tLo < kMinTimeStep) {
                break;
            }

            const auto [posA, rotA] = interpolateState(m_motionA, tMid);
            const auto [posB, rotB] = interpolateState(m_motionB, tMid);

            const internal_gjk_result_type gjkResult = computeGJKDistance(
                posA, rotA, posB, rotB);

            if (gjkResult.isCollided || gjkResult.distance < kEpsilonDistance) {
                tHi = tMid;
            }
            else {
                tLo = tMid;
            }
        }

        // 在 tHi 处执行 EPA 提取最终碰撞信息
        const auto [posA, rotA] = interpolateState(m_motionA, tHi);
        const auto [posB, rotB] = interpolateState(m_motionB, tHi);

        const internal_gjk_result_type gjkResult = computeGJKDistance(
            posA, rotA, posB, rotB);

        result_type result{};

        if (gjkResult.isCollided) {
            result = computeEPA(gjkResult, posA, rotA, posB, rotB);
            result.isCollided = true;
            result.timeOfImpact = tHi;
        }
        else if (gjkResult.distance < kEpsilonDistance) {
            result = extractContactFromGJK(gjkResult, tHi);
        }
        else {
            // 回退：未检测到碰撞
            result.isCollided = false;
            result.timeOfImpact = std::numeric_limits<double>::infinity();
        }

        return result;
    }


    // 三维类型别名

    /// @brief 三维 CCD 结果
    using CCDResult3D = CCDResult<3>;

    /// @brief 三维 CCD 连续碰撞检测器
    using ContinuousCollisionDetector3D = ContinuousCollisionDetector<3>;
} // namespace egret

#endif // EGRET_PHYSICS_CONTINUOUS_COLLISION_DETECTOR_H
