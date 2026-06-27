/**
 * @file        continuous_collision_detector.ixx
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

module;
#include "dimension_concept.h"
#include "motion/generalization_rigid_motion.h"
#include "motion/motion_utils.h"
#include <Eigen/Dense>
#include <functional>
#include <limits>
#include <vector>
#include <unordered_set>
#include <algorithm>

#include "boost/asio/associated_executor.hpp"
export module collision:continuous_collision_detector;

import :gilbert_johnson_keerthi;
import :expanding_polytope_algorithm;

export namespace egret
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

        // ── GJK 距离查询 ──

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
            const internal_gjk_result_type& gjkResult,
            const vector_type& posA, const vector_type& posB,
            double t) const;

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
                return extractContactFromGJK(gjkResult, posA, posB, t);
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

    // ── 内部 GJK 距离查询（委托独立 GilbertJohnsonKeerthi） ──

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    auto ContinuousCollisionDetector<Dimension>::computeGJKDistance(
        const vector_type& posA, const matrix_type& rotA,
        const vector_type& posB, const matrix_type& rotB) const -> internal_gjk_result_type
    {
        // 构造独立 GJK 实例，复用支撑函数回调
        GilbertJohnsonKeerthi<Dimension> gjk(m_supportA, m_supportB, posA, rotA, posB, rotB,
                                             kEpsilonGJK, kMaxGJKIterations);
        gjk.initialize();
        const GJKResult<Dimension> gjkResult = gjk.run();

        // 将 GJKResult 映射到 internal_gjk_result_type
        internal_gjk_result_type result{};
        result.isCollided = gjkResult.isCollided;
        result.simplex = gjkResult.simplex;
        result.supportAHistory = gjkResult.supportAHistory;
        result.supportBHistory = gjkResult.supportBHistory;

        if (!gjkResult.isCollided) {
            result.distance = gjkResult.distance;
            result.closestPair = gjkResult.minimumVerticesPair;
            result.closestPointMinkowski = gjkResult.minimumVerticesPair.first
                - gjkResult.minimumVerticesPair.second;
        }

        return result;
    }

    // ── 内部 EPA 碰撞信息提取 ──
    template <unsigned int Dimension> requires ValidDimension<Dimension>
    auto ContinuousCollisionDetector<Dimension>::computeEPA(
        const internal_gjk_result_type& gjkResult,
        const vector_type& posA, const matrix_type& rotA,
        const vector_type& posB, const matrix_type& rotB) const -> result_type
    {
        // 将 InternalGJKResult 转换为 GJKResult 供独立 EPA 使用
        GJKResult<Dimension> gjkRes{};
        gjkRes.isCollided = true;
        gjkRes.simplex = gjkResult.simplex;
        gjkRes.supportAHistory = gjkResult.supportAHistory;
        gjkRes.supportBHistory = gjkResult.supportBHistory;

        // 构造独立 EPA 实例，复用支撑函数回调
        ExpandingPolytopeAlgorithm<Dimension> epa(
            gjkRes,
            m_supportA, m_supportB,
            posA, rotA, posB, rotB,
            kEpsilonEPA, kMaxEPAIterations);


        const EPAResult<Dimension> epaRes = epa.run();

        // 将 EPAResult 映射到 CCDResult
        result_type ccdResult{};
        ccdResult.normal = epaRes.normal;
        ccdResult.penetrationDepth = epaRes.penetrationDepth;
        ccdResult.contactPoint = epaRes.contactPoint;
        ccdResult.closestPair = epaRes.closestPair;

        return ccdResult;
    }

    // ── 接触信息提取（近接触情形） ──
    template <unsigned int Dimension> requires ValidDimension<Dimension>
    auto ContinuousCollisionDetector<Dimension>::extractContactFromGJK(
        const internal_gjk_result_type& gjkResult,
        const vector_type& posA, const vector_type& posB,
        double t) const -> result_type
    {
        result_type contactResult{};

        const vector_type& pStar = gjkResult.closestPointMinkowski;
        const double d = pStar.norm();

        contactResult.isCollided = true;
        contactResult.timeOfImpact = t;

        // 近接触时 d 极小（< kEpsilonGJK），pStar/d 会放大 GJK 中 Gram 矩阵
        // 求逆引入的数值噪声（~1e-15 → 法线偏差 ~1e-7 rad），累积导致漂移。
        // 当 d 不足时回退到稳定的物体中心连线方向。
        const vector_type centerDir = posB - posA;
        const double centerDist = centerDir.norm();

        if (d > centerDist * 1e-4 && d > kEpsilonGJK) {
            // d 足够大：GJK 最近点法线可靠
            contactResult.normal = pStar / d;
        }
        else if (centerDist > kEpsilonGJK) {
            // 回退到物体中心连线方向
            contactResult.normal = centerDir / centerDist;
        }
        else {
            // 两物体中心重合：任意方向
            contactResult.normal = vector_type::Zero();
            contactResult.normal(Dimension - 1) = 1.0;
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
            result = extractContactFromGJK(gjkResult, posA, posB, tHi);
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
    ///
    using CCDResult2D = CCDResult<2>;
    using CCDResult3D = CCDResult<3>;

    /// @brief 三维 CCD 连续碰撞检测器
    using ContinuousCollisionDetector2D = ContinuousCollisionDetector<2>;
    using ContinuousCollisionDetector3D = ContinuousCollisionDetector<3>;
} // namespace egret

// #endif // EGRET_PHYSICS_CONTINUOUS_COLLISION_DETECTOR_H
