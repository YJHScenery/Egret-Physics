/**
 * @file        expanding_polytope_algorithm.h
 * @brief       EPA（Expanding Polytope Algorithm）算法头文件
 * @details     以 GJK 最终单纯形为初始多面体，通过迭代扩展逼近闵可夫斯基差边界，
 *              提取碰撞法线、穿透深度和接触点等碰撞响应所需信息。
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

#ifndef EGRET_PHYSICS_EXPANDING_POLYTOPE_ALGORITHM_H
#define EGRET_PHYSICS_EXPANDING_POLYTOPE_ALGORITHM_H

#include <Eigen/Dense>
#include <vector>
#include <array>
#include <utility>
#include "gilbert_johnson_keerthi.h"
#include "physical_entity.h"

namespace egret
{
    /**
     * @brief       EPA 算法结果结构体
     * @details     包含碰撞响应所需的全部几何信息：碰撞法线、穿透深度、接触点、最近点对。
     */
    struct EPAResult
    {
        using vector_type = Eigen::Vector3d;

        /// @brief 碰撞法线（单位向量，指向分离方向）
        vector_type normal{vector_type::Zero()};

        /// @brief 穿透深度 δ = ||p*||
        double penetrationDepth{0.0};

        /// @brief 接触点 c = (a* + b*) / 2
        vector_type contactPoint{vector_type::Zero()};

        /// @brief 闵可夫斯基差边界上的最近点对 (a* ∈ A, b* ∈ B)
        std::pair<vector_type, vector_type> closestPair{
            vector_type::Zero(), vector_type::Zero()
        };
    };

    /**
     * @brief       EPA（Expanding Polytope Algorithm）算法类
     * @details     以 GJK 阶段产生的碰撞单纯形为初始多面体，
     *              通过迭代在法向量方向上获取新支撑点来扩展多面体，
     *              逐步逼近闵可夫斯基差 A⊖B 的边界，最终提取碰撞法线、穿透深度和接触点。
     *
     * @invariant   多面体始终包含原点（由 GJK 保证初始状态，扩展维持该性质）
     * @invariant   所有面的法向量指向多面体外部（即背离原点方向）
     * @remark      该类假定输入 GJKResult 的 isCollided == true
     * @see         GilbertJohnsonKeerthi, GJKResult3D, EPAResult
     */
    class ExpandingPolytopeAlgorithm
    {
    public:
        using vector_type = Eigen::Vector3d;
        using matrix_type = Eigen::Matrix<double, 3, Eigen::Dynamic>;

        /// @brief EPA 收敛容差
        static constexpr double kEpsilon = 1e-10;

        /// @brief 最大迭代次数
        static constexpr int kMaxIterations = 100;

        /**
         * @brief 使用 GJK 结果构造 EPA 算法实例
         * @param gjkResult GJK 阶段产生的结果（必须 isCollided == true）
         * @param entityA   碰撞实体 A
         * @param entityB   碰撞实体 B
         */
        ExpandingPolytopeAlgorithm(const GJKResult3D& gjkResult,
                                   PhysicalEntity* entityA,
                                   PhysicalEntity* entityB);

        /**
         * @brief 运行 EPA 迭代，提取碰撞信息
         * @return EPAResult 包含碰撞法线、穿透深度、接触点及最近点对
         */
        EPAResult run();

    private:
        /**
         * @brief 多面体的一个面（3D 中通常为三角形，初始阶段可能为边或点）
         */
        struct Face
        {
            /// @brief 面顶点的索引（指向 m_vertices 的列）
            std::vector<int> vertexIndices{};

            /// @brief 单位外法向量（指向多面体外部，即背离原点）
            vector_type normal{vector_type::Zero()};

            /// @brief 原点到该面所在平面的距离 d = n · v（v 为面上任意顶点）
            double distance{0.0};

            /**
             * @brief 获取面的第 i 条边（两个顶点索引）
             * @param i 边索引（0 到 vertexCount-1）
             * @return 边的两个顶点索引（按环绕顺序）
             */
            [[nodiscard]] std::pair<int, int> getEdge(int i) const
            {
                const int n = static_cast<int>(vertexIndices.size());
                return {vertexIndices[i], vertexIndices[(i + 1) % n]};
            }
        };

        // ──── 多面体数据 ────

        /// @brief 多面体顶点矩阵（每列为一个闵可夫斯基差点）
        matrix_type m_vertices{};

        /// @brief 多面体的面列表
        std::vector<Face> m_faces{};

        /// @brief 与 m_vertices 各列对应的凸体 A 支撑点
        std::vector<vector_type> m_supportA{};

        /// @brief 与 m_vertices 各列对应的凸体 B 支撑点
        std::vector<vector_type> m_supportB{};

        // ──── 实体引用 ────

        PhysicalEntity* m_entityA{nullptr};
        PhysicalEntity* m_entityB{nullptr};

        // ──── 私有方法 ────

        /**
         * @brief 从 GJK 单纯形初始化多面体
         * @details 将 GJK 单纯形顶点复制到 m_vertices，生成初始面列表，
         *          复制有序支撑历史。
         */
        void initializeFromGJK(const GJKResult3D& gjkResult);

        /**
         * @brief 为当前多面体计算所有面的法向量与距离
         * @details 按附录1公式：满维时直接求解 V_i^T n_i = 1，
         *          非满维时使用 Moore-Penrose 伪逆。
         *          确保法向量统一指向多面体外部。
         */
        void computeFaces();

        /**
         * @brief 计算单面的法向量与距离（使用伪逆统一处理满维/非满维）
         * @param face 要计算的面（顶点索引已设置）
         */
        void computeFaceNormal(Face& face) const;

        /**
         * @brief 判断面是否从给定点可见
         * @param face  待检查的面
         * @param point 检查点（闵可夫斯基差空间中的点）
         * @return true 如果点在面的外侧（n · p > d + ε）
         */
        [[nodiscard]] bool isFaceVisibleFromPoint(const Face& face,
                                                   const vector_type& point) const;

        /**
         * @brief 扩展多面体：将新顶点加入并重建凸包
         * @param newVertex 新闵可夫斯基差顶点
         * @param supportA  对应的凸体 A 支撑点
         * @param supportB  对应的凸体 B 支撑点
         */
        void expandPolytope(const vector_type& newVertex,
                            const vector_type& supportA,
                            const vector_type& supportB);

        /**
         * @brief 从最近面提取碰撞结果
         * @param closestFace 距离原点最近的面
         * @return EPAResult 碰撞法线、穿透深度、接触点、最近点对
         */
        [[nodiscard]] EPAResult extractResult(const Face& closestFace) const;
    };

} // namespace egret

#endif // EGRET_PHYSICS_EXPANDING_POLYTOPE_ALGORITHM_H
