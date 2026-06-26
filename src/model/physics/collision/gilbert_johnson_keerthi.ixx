//
// Created by jehor on 2026/6/23.
//

// #ifndef EGRET_PHYSICS_GILBERT_JOHNSON_KEERTHI_H
// #define EGRET_PHYSICS_GILBERT_JOHNSON_KEERTHI_H

module;
#include "physics_utils.h"
#include "physics_abstract.h"
#include "basic_utils.h"
#include "dimension_concept.h"
#include "shape_base.h"
#include <Eigen/Dense>
#include <set>
#include <unordered_set>
#include <functional>
export module collision:gilbert_johnson_keerthi;

export namespace egret
{
    template <unsigned int Dimension> requires ValidDimension<Dimension>
    struct GJKResult
    {
        static constexpr unsigned int dimension = Dimension;

        using vector_type = Eigen::Vector<double, Dimension>;

        using vertex_set_type = std::unordered_set<vector_type>;

        using matrix_type = Eigen::Matrix<double, Dimension, Eigen::Dynamic>;

        // 是否发生碰撞
        bool isCollided{false};

        /** @brief GJK 算法迭代后产生的单纯形矩阵
         * @note 列向量：单纯形的顶点
         * @note 行向量：空间坐标分量
         */
        matrix_type simplex{};

        /// @brief 与 simplex 各列对应的凸体 A 支撑点（按迭代顺序，供 EPA 使用）
        std::vector<vector_type> supportAHistory{};

        /// @brief 与 simplex 各列对应的凸体 B 支撑点（按迭代顺序，供 EPA 使用）
        std::vector<vector_type> supportBHistory{};

        /// @note 下述字段仅在 isCollided == false 时有作用
        // 两物体之间的距离
        double distance{};

        // 原始物体最近点对
        std::pair<vector_type, vector_type> minimumVerticesPair{};
    };


    template <unsigned int Dimension> requires ValidDimension<Dimension>
    class GilbertJohnsonKeerthi
    {
    public:
        using result_type = GJKResult<Dimension>;

        using vector_type = result_type::vector_type;

        using matrix_type = result_type::matrix_type;

        using vertex_set_type = result_type::vertex_set_type;

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

        GilbertJohnsonKeerthi() = default;

        /**
         * @brief 构造 GJK 算法实例
         * @param supportA  凸体 A 的支撑函数回调
         * @param supportB  凸体 B 的支撑函数回调
         * @param posA      凸体 A 的世界空间位置
         * @param rotA      凸体 A 的世界空间旋转矩阵
         * @param posB      凸体 B 的世界空间位置
         * @param rotB      凸体 B 的世界空间旋转矩阵
         * @param epsilon   原点包含判定容差（默认 1e-10）
         * @param maxIterations  最大迭代次数（默认 10^Dimension）
         */
        GilbertJohnsonKeerthi(SupportCallback supportA, SupportCallback supportB,
                              const vector_type& posA, const matrix_type& rotA,
                              const vector_type& posB, const matrix_type& rotB,
                              double epsilon = 1e-10,
                              int maxIterations = static_cast<int>(
                                  quickPower<std::uint64_t, std::uint64_t, Dimension>(10)));

        void initialize();

        result_type run();

    private:
        /// @brief GJK 收敛容差（原点包含判定的阈值）
        double m_epsilon{1e-10};

        /// @brief 最大迭代次数，防止无限循环
        int m_maxIterations{
            static_cast<int>(
                quickPower<std::uint64_t, std::uint64_t, Dimension>(10))
        };

        /// @brief 凸体 A 的支撑函数回调
        SupportCallback m_supportAFunc;

        /// @brief 凸体 B 的支撑函数回调
        SupportCallback m_supportBFunc;

        /// @brief 凸体 A 的世界空间位置
        vector_type m_positionA{vector_type::Zero()};

        /// @brief 凸体 B 的世界空间位置
        vector_type m_positionB{vector_type::Zero()};

        /// @brief 凸体 A 的世界空间旋转矩阵
        matrix_type m_rotationA{matrix_type::Identity()};

        /// @brief 凸体 B 的世界空间旋转矩阵
        matrix_type m_rotationB{matrix_type::Identity()};

        /**
         * @brief 原点包含性判定子函数 C_o(σ, d)
         * @details 枚举单纯形所有非空子集，通过 Gram 矩阵求解重心坐标，
         *          找到距离原点最近的子单纯形上的最近点。
         *          若 ||p*|| < ε 则原点在单纯形内部，返回 true。
         *          否则更新 m_currentDirection = -p* 并裁剪单纯形，返回 false。
         * @return true 原点在单纯形内部（碰撞），false 否则
         */
        bool containsOrigin();

        /**
         * @brief 未碰撞时构建结果数据（距离、最近点对）
         * @details 利用 computeOptimalSubset() 缓存的最优子集与重心坐标，
         *          计算两物体间的最小距离 d₀ 和最近点对 (a₀, b₀)。
         */
        void buildDataWhenNotCollided(result_type& result);

        /**
         * @brief 计算单纯形上距离原点最近的子单纯形
         * @details 枚举当前单纯形的所有非空子集，对每个子集构造 Gram 矩阵并求解重心坐标，
         *          筛选出所有 λ_i > 0 的有效子集，选择使 ||p*|| 最小的那个。
         *          结果缓存在 m_optimalSubsetIndices、m_optimalBarycentric、m_optimalClosestPoint 中。
         * @return 最优子集上最近点到原点的距离 ||p*||
         */
        double computeOptimalSubset();

        vector_type m_currentDirection{};

        /// @brief 当前单纯形矩阵，每列为一个闵可夫斯基差顶点
        matrix_type m_currentSimplex{};

        /// @brief 搜索过程中凸体 A、B 的所有支撑点集合
        std::pair<vertex_set_type, vertex_set_type> supportVerticesPair{};

        /// @brief 与单纯形各列对应的凸体 A 支撑点历史（按迭代顺序）
        std::vector<vector_type> m_supportAHistory{};

        /// @brief 与单纯形各列对应的凸体 B 支撑点历史（按迭代顺序）
        std::vector<vector_type> m_supportBHistory{};

        /// @brief 最优子单纯形的顶点索引（containsOrigin 缓存，供 buildDataWhenNotCollided 使用）
        std::vector<int> m_optimalSubsetIndices{};

        /// @brief 最优子单纯形的重心坐标 λ（containsOrigin 缓存）
        Eigen::VectorXd m_optimalBarycentric{};

        /// @brief 最优子单纯形上距离原点最近的点 p*（containsOrigin 缓存）
        vector_type m_optimalClosestPoint{};

        /**
         * @brief 向当前单纯形矩阵添加新列（闵可夫斯基差顶点）
         * @param newCol 新的闵可夫斯基差点
         */
        void addNewColToCurrentSimplex(vector_type newCol);
    };


    template <unsigned int Dimension> requires ValidDimension<Dimension>
    GilbertJohnsonKeerthi<Dimension>::GilbertJohnsonKeerthi(
        SupportCallback supportA, SupportCallback supportB,
        const vector_type& posA, const matrix_type& rotA,
        const vector_type& posB, const matrix_type& rotB,
        double epsilon, int maxIterations)
        : m_supportAFunc(std::move(supportA))
          , m_supportBFunc(std::move(supportB))
          , m_positionA(posA)
          , m_rotationA(rotA)
          , m_positionB(posB)
          , m_rotationB(rotB)
          , m_epsilon(epsilon)
          , m_maxIterations(maxIterations)
    {
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    void GilbertJohnsonKeerthi<Dimension>::initialize()
    {
        // 1. 选定初始方向：center(B) - center(A)
        m_currentDirection = m_positionB - m_positionA;

        // 若中心重合，使用默认方向 (0, 0, ..., 1)
        if (m_currentDirection.squaredNorm() < m_epsilon * m_epsilon) {
            m_currentDirection = vector_type(vector_type::Zero());
            m_currentDirection(Dimension - 1) = 1.0;
        }

        // 2. 获取第一个闵可夫斯基差支撑点
        const auto supportA = m_supportAFunc(m_currentDirection, m_positionA, m_rotationA);
        const auto supportB = m_supportBFunc(-m_currentDirection, m_positionB, m_rotationB);

        supportVerticesPair.first.insert(supportA);
        supportVerticesPair.second.insert(supportB);

        const vector_type supportP = supportA - supportB;

        // 3. 初始化单纯形并记录支撑历史
        m_supportAHistory.push_back(supportA);
        m_supportBHistory.push_back(supportB);
        addNewColToCurrentSimplex(supportP);

        // 4. 更新搜索方向为 -p₀
        m_currentDirection = -supportP;
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    GilbertJohnsonKeerthi<Dimension>::result_type GilbertJohnsonKeerthi<Dimension>::run()
    {
        result_type gjkResult;

        for (std::uint64_t iter = 0; iter < static_cast<std::uint64_t>(m_maxIterations); ++iter) {
            // 1. 获取新支撑点
            const auto supportA = m_supportAFunc(m_currentDirection, m_positionA, m_rotationA);
            const auto supportB = m_supportBFunc(-m_currentDirection, m_positionB, m_rotationB);
            //
            // supportVerticesPair.first.insert(supportA);
            // supportVerticesPair.second.insert(supportB);

            const vector_type supportP = supportA - supportB;

            // 2. 检查是否越过原点：若 p·d < 0，原点不可能在差集内部
            if (supportP.dot(m_currentDirection) < 0.0) {
                gjkResult.isCollided = false;
                buildDataWhenNotCollided(gjkResult);
                break;
            }

            // 3. 将新支撑点加入单纯形（并记录支撑历史）
            m_supportAHistory.push_back(supportA);
            m_supportBHistory.push_back(supportB);
            addNewColToCurrentSimplex(supportP);

            // 4. 检查单纯形是否包含原点
            if (containsOrigin()) {
                gjkResult.isCollided = true;
                break;
            }
            // containsOrigin() 在未碰撞时已更新 m_currentDirection 并裁剪单纯形
        }

        // 填充结果
        gjkResult.simplex = m_currentSimplex;
        // gjkResult.supportVerticesPair = supportVerticesPair;
        gjkResult.supportAHistory = m_supportAHistory;
        gjkResult.supportBHistory = m_supportBHistory;

        return gjkResult;
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    double GilbertJohnsonKeerthi<Dimension>::computeOptimalSubset()
    {
        const Eigen::Index N = m_currentSimplex.cols();

        double bestDistance = std::numeric_limits<double>::max();
        std::vector<int> bestSubset;
        Eigen::VectorXd bestLambda;
        vector_type bestClosestPoint = vector_type::Zero();

        // 枚举所有非空子集（位掩码方式）
        const int totalSubsets = 1 << static_cast<int>(N);
        for (int mask = 1; mask < totalSubsets; ++mask) {
            // 提取子集索引
            std::vector<int> subset;
            subset.reserve(static_cast<size_t>(N));
            for (int i = 0; i < static_cast<int>(N); ++i) {
                if (mask & (1 << i)) {
                    subset.push_back(i);
                }
            }

            const int m = static_cast<int>(subset.size()) - 1; // m = |S| - 1
            const int gramSize = m + 2; // (m+1) 顶点 + 1 拉格朗日乘子

            // 构造 Gram 矩阵 G
            // G(r, c) = v_ir · v_ic  (r, c = 0..m)
            // G(r, m+1) = G(m+1, r) = 1  (r = 0..m)
            // G(m+1, m+1) = 0
            Eigen::MatrixXd gramMatrix = Eigen::MatrixXd::Zero(gramSize, gramSize);
            for (int r = 0; r <= m; ++r) {
                const int ir = subset[r];
                for (int c = 0; c <= m; ++c) {
                    const int ic = subset[c];
                    gramMatrix(r, c) = m_currentSimplex.col(ir).dot(m_currentSimplex.col(ic));
                }
                gramMatrix(r, m + 1) = 1.0;
                gramMatrix(m + 1, r) = 1.0;
            }
            gramMatrix(m + 1, m + 1) = 0.0;

            // 构造右端向量 b = [0, 0, ..., 0, 1]ᵀ
            Eigen::VectorXd b = Eigen::VectorXd::Zero(gramSize);
            b(m + 1) = 1.0;

            // 求解线性系统 G · [λ, μ]ᵀ = b
            Eigen::FullPivLU<Eigen::MatrixXd> lu(gramMatrix);
            if (!lu.isInvertible()) {
                // 退化子集（如三点共线），跳过
                continue;
            }

            Eigen::VectorXd x = lu.solve(b);

            // 提取重心坐标 λ，检查是否全部为正
            bool allPositive = true;
            for (int j = 0; j <= m; ++j) {
                if (x(j) <= -m_epsilon) {
                    allPositive = false;
                    break;
                }
            }

            if (!allPositive) {
                continue;
            }

            // 计算最近点 p* = Σ λ_j v_ij
            vector_type pStar = vector_type::Zero();
            for (int j = 0; j <= m; ++j) {
                pStar += x(j) * m_currentSimplex.col(subset[j]);
            }

            const double distance = pStar.norm();

            if (distance < bestDistance) {
                bestDistance = distance;
                bestSubset = std::move(subset);
                bestLambda = x.head(m + 1);
                bestClosestPoint = pStar;
            }
        }

        // 缓存最优子集信息
        m_optimalSubsetIndices = std::move(bestSubset);
        m_optimalBarycentric = std::move(bestLambda);
        m_optimalClosestPoint = bestClosestPoint;

        return bestDistance;
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    bool GilbertJohnsonKeerthi<Dimension>::containsOrigin()
    {
        const double bestDistance = computeOptimalSubset();

        // 原点包含判定：||p*|| < ε
        if (bestDistance < m_epsilon) {
            // 碰撞：保留完整单纯形供 EPA 使用，不裁剪
            return true;
        }

        // 未碰撞：更新搜索方向为 -p*
        m_currentDirection = -m_optimalClosestPoint;

        // 裁剪单纯形，仅保留最优子集的顶点
        const auto newCols = static_cast<Eigen::Index>(m_optimalSubsetIndices.size());
        matrix_type newSimplex(Dimension, newCols);

        std::vector<vector_type> newSupportA;
        std::vector<vector_type> newSupportB;
        newSupportA.reserve(m_optimalSubsetIndices.size());
        newSupportB.reserve(m_optimalSubsetIndices.size());

        for (size_t j = 0; j < m_optimalSubsetIndices.size(); ++j) {
            const int idx = m_optimalSubsetIndices[j];
            newSimplex.col(static_cast<Eigen::Index>(j)) = m_currentSimplex.col(idx);
            if (idx < static_cast<int>(m_supportAHistory.size())) {
                newSupportA.push_back(m_supportAHistory[idx]);
                newSupportB.push_back(m_supportBHistory[idx]);
            }
        }

        m_currentSimplex = std::move(newSimplex);
        m_supportAHistory = std::move(newSupportA);
        m_supportBHistory = std::move(newSupportB);

        return false;
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    void GilbertJohnsonKeerthi<Dimension>::buildDataWhenNotCollided(result_type& result)
    {
        // 计算最优子集（结果缓存在成员变量中）
        const double distance = computeOptimalSubset();
        result.distance = distance;

        // 计算原始物体最近点对：
        // a₀ = Σ λ_i · a_i,  b₀ = Σ λ_i · b_i
        vector_type a0 = vector_type::Zero();
        vector_type b0 = vector_type::Zero();

        for (std::int64_t j = 0; j < m_optimalSubsetIndices.size(); ++j) {
            const int idx = m_optimalSubsetIndices[j];
            if (idx < static_cast<int>(m_supportAHistory.size())) {
                const double lambda = m_optimalBarycentric(j);
                a0 += lambda * m_supportAHistory[idx];
                b0 += lambda * m_supportBHistory[idx];
            }
        }

        result.minimumVerticesPair = {a0, b0};
    }

    template <unsigned int Dimension> requires ValidDimension<Dimension>
    void GilbertJohnsonKeerthi<Dimension>::addNewColToCurrentSimplex(vector_type newCol)
    {
        m_currentSimplex.conservativeResize(Eigen::NoChange, m_currentSimplex.cols() + 1);
        m_currentSimplex.col(m_currentSimplex.cols() - 1) = std::move(newCol);
    }

    // ──── 三维类型别名 ────
    using GJKResult3D = GJKResult<3>;
    using GilbertJohnsonKeerthi3D = GilbertJohnsonKeerthi<3>;
} // egret
//
// #endif //EGRET_PHYSICS_GILBERT_JOHNSON_KEERTHI_H
