//
// Created by jehor on 2026/6/23.
//

#ifndef EGRET_PHYSICS_GILBERT_JOHNSON_KEERTHI_H
#define EGRET_PHYSICS_GILBERT_JOHNSON_KEERTHI_H
#include "physics_utils.h"
#include "physics_abstract.h"
#include "basic_utils.h"
#include "shape_base.h"
#include <Eigen/Dense>
#include <set>
#include <unordered_set>

#include "physical_entity.h"

namespace egret
{
    template <unsigned int Dimension>
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

    using GJKResult2D = GJKResult<2>;
    using GJKResult3D = GJKResult<3>;


    class GilbertJohnsonKeerthi
    {
    public:
        using result_type = GJKResult3D;

        using vector_type = result_type::vector_type;

        using matrix_type = result_type::matrix_type;

        using vertex_set_type = result_type::vertex_set_type;


        GilbertJohnsonKeerthi() = default;

        GilbertJohnsonKeerthi(PhysicalEntity* entityA, PhysicalEntity* entityB);

        void initialize();

        result_type run();

    private:
        /// @brief GJK 收敛容差（原点包含判定的阈值）
        static constexpr double kEpsilon = 1e-10;

        /// @brief 最大迭代次数，防止无限循环
        static constexpr int kMaxIterations = quickPower<std::uint64_t, std::uint64_t, result_type::dimension>(10);

        std::pair<PhysicalEntity*, PhysicalEntity*> m_entities{nullptr, nullptr};

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


} // egret

#endif //EGRET_PHYSICS_GILBERT_JOHNSON_KEERTHI_H
