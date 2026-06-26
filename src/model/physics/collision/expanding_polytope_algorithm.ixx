/**
 * @file        expanding_polytope_algorithm.ixx
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

// #ifndef EGRET_PHYSICS_EXPANDING_POLYTOPE_ALGORITHM_H
// #define EGRET_PHYSICS_EXPANDING_POLYTOPE_ALGORITHM_H
module;
#include <Eigen/Dense>
#include <vector>
#include <array>
#include <utility>
#include <functional>
#include "dimension_concept.h"
export module collision: expanding_polytope_algorithm;

import :gilbert_johnson_keerthi;

export namespace egret
{
    /**
     * @brief       EPA 算法结果结构体
     * @details     包含碰撞响应所需的全部几何信息：碰撞法线、穿透深度、接触点、最近点对。
     */
    template<unsigned int Dimension> requires ValidDimension<Dimension>
    struct EPAResult
    {
        using vector_type = Eigen::Matrix<double, Dimension, 1>;

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
     * @see         GilbertJohnsonKeerthi, GJKResult, EPAResult
     */
    template<unsigned int Dimension> requires ValidDimension<Dimension>
    class ExpandingPolytopeAlgorithm
    {
    public:
        using vector_type = Eigen::Matrix<double, Dimension, 1>;
        using matrix_type = Eigen::Matrix<double, Dimension, Eigen::Dynamic>;

        /// @brief 支撑函数回调：给定世界空间方向、位置和旋转，返回世界空间支撑点
        using SupportCallback = std::function<vector_type(
            const vector_type& worldDir,
            const vector_type& position,
            const matrix_type& rotation
        )>;

        /// @brief EPA 收敛容差
        double m_epsilon{1e-10};

        /// @brief 最大迭代次数
        int m_maxIterations{100};

        /**
         * @brief 使用 GJK 结果构造 EPA 算法实例
         * @param gjkResult GJK 阶段产生的结果（必须 isCollided == true）
         * @param supportA  凸体 A 的支撑函数回调
         * @param supportB  凸体 B 的支撑函数回调
         * @param posA      凸体 A 的世界空间位置
         * @param rotA      凸体 A 的世界空间旋转矩阵
         * @param posB      凸体 B 的世界空间位置
         * @param rotB      凸体 B 的世界空间旋转矩阵
         * @param epsilon   容差
         * @param maxIterations 最大迭代次数
         */
        ExpandingPolytopeAlgorithm(const GJKResult<Dimension>& gjkResult,
                                   SupportCallback supportA,
                                   SupportCallback supportB,
                                   const vector_type& posA, const matrix_type& rotA,
                                   const vector_type& posB, const matrix_type& rotB,
                                   double epsilon = 1e-10,
                                   int maxIterations = 100);

        /**
         * @brief 运行 EPA 迭代，提取碰撞信息
         * @return EPAResult 包含碰撞法线、穿透深度、接触点及最近点对
         */
        EPAResult<Dimension> run();

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
             * @brief 获取面中省略第 i 个顶点后的脊（(Dimension-2)-单纯形）
             * @param i 被省略的顶点索引（0 到 vertexCount-1）
             * @return 脊的顶点索引列表（已排序）
             */
            [[nodiscard]] std::vector<int> getRidge(int i) const
            {
                const int n = static_cast<int>(vertexIndices.size());
                std::vector<int> ridge;
                ridge.reserve(static_cast<size_t>(n - 1));
                for (int j = 0; j < n; ++j) {
                    if (j != i) {
                        ridge.push_back(vertexIndices[j]);
                    }
                }
                std::ranges::sort(ridge);
                return ridge;
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

        // ──── 支撑函数回调 ────

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

        // ──── 私有方法 ────

        /**
         * @brief 从 GJK 单纯形初始化多面体
         * @details 将 GJK 单纯形顶点复制到 m_vertices，生成初始面列表，
         *          复制有序支撑历史。
         */
        void initializeFromGJK(const GJKResult<Dimension>& gjkResult);

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
        [[nodiscard]] EPAResult<Dimension> extractResult(const Face& closestFace) const;
    };

    // 构造 / 主入口

    template<unsigned int Dimension> requires ValidDimension<Dimension>
    ExpandingPolytopeAlgorithm<Dimension>::ExpandingPolytopeAlgorithm(
        const GJKResult<Dimension>& gjkResult,
        SupportCallback supportA,
        SupportCallback supportB,
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
        initializeFromGJK(gjkResult);
    }

    template<unsigned int Dimension> requires ValidDimension<Dimension>
    EPAResult<Dimension> ExpandingPolytopeAlgorithm<Dimension>::run()
    {
        // 若初始单纯形顶点数不足 Dimension + 1（非满维），先执行一次扩展
        if (m_vertices.cols() < static_cast<Eigen::Index>(Dimension + 1)) {
            // 找一个不在当前顶点仿射张成空间内的方向
            vector_type dir;
            const Eigen::Index cols = m_vertices.cols();
            if (cols == 1) {
                // 单点：取第一个标准基向量
                dir = vector_type::Zero();
                dir(0) = 1.0;
            } else {
                // 构建偏移矩阵 D = [v_1 - v_0, ..., v_{cols-1} - v_0]
                Eigen::Matrix<double, Dimension, Eigen::Dynamic> centered(Dimension, cols - 1);
                for (Eigen::Index j = 1; j < cols; ++j) {
                    centered.col(j - 1) = m_vertices.col(j) - m_vertices.col(0);
                }
                // QR 分解获取正交补
                Eigen::FullPivHouseholderQR<
                    Eigen::Matrix<double, Dimension, Eigen::Dynamic>
                > qr(centered);
                const Eigen::Index rank = qr.rank();
                if (rank < static_cast<Eigen::Index>(Dimension)) {
                    dir = qr.matrixQ().col(rank);
                } else {
                    // 已经满维（不应出现于此分支），使用备选方向
                    dir = vector_type::Zero();
                    dir(Dimension - 1) = 1.0;
                }
            }

            if (dir.squaredNorm() > m_epsilon * m_epsilon) {
                dir.normalize();

                const auto sA = m_supportAFunc(dir, m_positionA, m_rotationA);
                const auto sB = m_supportBFunc(-dir, m_positionB, m_rotationB);
                const vector_type sP = sA - sB;

                expandPolytope(sP, sA, sB);
            }
        }

        for (int iter = 0; iter < m_maxIterations; ++iter) {
            // 步骤 1：寻找最近面
            if (m_faces.empty()) {
                break;
            }

            int closestIdx = 0;
            double minDist = m_faces[0].distance;
            for (size_t i = 1; i < m_faces.size(); ++i) {
                if (m_faces[i].distance < minDist) {
                    minDist = m_faces[i].distance;
                    closestIdx = static_cast<int>(i);
                }
            }

            const Face& closestFace = m_faces[closestIdx];
            const vector_type n = closestFace.normal;
            const double dMin = closestFace.distance;

            // 步骤 2：沿法向量方向获取新支撑点
            const auto supportA = m_supportAFunc(n, m_positionA, m_rotationA);
            const auto supportB = m_supportBFunc(-n, m_positionB, m_rotationB);
            const vector_type supportP = supportA - supportB;

            // 步骤 3：终止条件检查  p · n - d_min ≤ ε
            const double projection = supportP.dot(n);
            if (projection - dMin <= m_epsilon) {
                // 收敛：提取结果
                return extractResult(closestFace);
            }

            // 步骤 4：扩展多面体
            expandPolytope(supportP, supportA, supportB);
        }

        // 迭代次数耗尽时的回退：使用当前最近面提取结果
        if (!m_faces.empty()) {
            int closestIdx = 0;
            double minDist = m_faces[0].distance;
            for (size_t i = 1; i < m_faces.size(); ++i) {
                if (m_faces[i].distance < minDist) {
                    minDist = m_faces[i].distance;
                    closestIdx = static_cast<int>(i);
                }
            }
            return extractResult(m_faces[closestIdx]);
        }

        return EPAResult<Dimension>{};
    }

    // 初始化
    template<unsigned int Dimension> requires ValidDimension<Dimension>
    void ExpandingPolytopeAlgorithm<Dimension>::initializeFromGJK(const GJKResult<Dimension>& gjkResult)
    {
        const Eigen::Index N = gjkResult.simplex.cols();

        // 复制单纯形顶点
        m_vertices = gjkResult.simplex;

        // 复制有序支撑历史（与单纯形列一一对应）
        const size_t historySize = gjkResult.supportAHistory.size();
        m_supportA.reserve(historySize);
        m_supportB.reserve(historySize);
        for (size_t i = 0; i < historySize && i < static_cast<size_t>(N); ++i) {
            m_supportA.push_back(gjkResult.supportAHistory[i]);
            m_supportB.push_back(gjkResult.supportBHistory[i]);
        }

        // 若支撑历史不足，从无序集合中尽力匹配
        // （正常情况下 GJK 的有序历史应完整覆盖所有单纯形列）
        if (m_supportA.size() < static_cast<size_t>(N)) {
            // 回退：对于无法匹配的顶点，支撑点置零
            m_supportA.resize(static_cast<size_t>(N), vector_type::Zero());
            m_supportB.resize(static_cast<size_t>(N), vector_type::Zero());
        }

        // 生成初始面列表
        computeFaces();
    }

    // 面计算

    template<unsigned int Dimension> requires ValidDimension<Dimension>
    void ExpandingPolytopeAlgorithm<Dimension>::computeFaces()
    {
        m_faces.clear();

        const Eigen::Index N = m_vertices.cols();
        if (N < 2) {
            // 退化：顶点数不足，无法构成面
            return;
        }

        // 按附录1：每个面 Γ_i 由除 v_i 外的所有顶点构成
        for (Eigen::Index i = 0; i < N; ++i) {
            Face face;
            face.vertexIndices.reserve(static_cast<size_t>(N - 1));
            for (Eigen::Index j = 0; j < N; ++j) {
                if (j != i) {
                    face.vertexIndices.push_back(static_cast<int>(j));
                }
            }
            computeFaceNormal(face);
            m_faces.push_back(std::move(face));
        }
    }

    template<unsigned int Dimension> requires ValidDimension<Dimension>
    void ExpandingPolytopeAlgorithm<Dimension>::computeFaceNormal(Face& face) const
    {
        const int k = static_cast<int>(face.vertexIndices.size());  // 面顶点数

        if (k < 1) {
            face.normal = vector_type::Zero();
            face.distance = 0.0;
            return;
        }

        // 构建 V_i 矩阵（Dimension × k）：每列为面上一个顶点坐标
        Eigen::Matrix<double, Dimension, Eigen::Dynamic> Vi(Dimension, k);
        for (int c = 0; c < k; ++c) {
            Vi.col(c) = m_vertices.col(face.vertexIndices[c]);
        }

        // 右端向量 1_k = [1, 1, ..., 1]ᵀ
        const Eigen::VectorXd ones = Eigen::VectorXd::Ones(k);

        // 统一使用伪逆求解 V_i^T n_i = 1_k（处理满维与非满维情形）
        const Eigen::MatrixXd ViT = Vi.transpose();  // k × Dimension
        vector_type ni = ViT.completeOrthogonalDecomposition().solve(ones);

        const double niNorm = ni.norm();
        if (niNorm < m_epsilon) {
            face.normal = vector_type::Zero();
            face.distance = 0.0;
            return;
        }

        // 单位外法向量：u_i = n_i / ||n_i||
        face.normal = ni / niNorm;

        // 原点到面的距离：d_i = 1 / ||n_i||
        face.distance = 1.0 / niNorm;

        // 确保法向量指向多面体外部（背离原点）
        // 原点在内部时应有 n · origin = 0 < d，故 n 自然指向外部
        // 验证：取面上第一个顶点，应有 n · v ≈ d
        // 若 n · v < 0（即法向量指向内部），翻转法向量
        const vector_type v0 = m_vertices.col(face.vertexIndices[0]);
        if (face.normal.dot(v0) < 0.0) {
            face.normal = -face.normal;
        }
    }

    // 面可见性判定
    template<unsigned int Dimension> requires ValidDimension<Dimension>
    bool ExpandingPolytopeAlgorithm<Dimension>::isFaceVisibleFromPoint(
        const Face& face, const vector_type& point) const
    {
        // 点在面的外侧 ⇔ n · p > d + ε
        return face.normal.dot(point) > face.distance + m_epsilon;
    }

    // 多面体扩展
    template<unsigned int Dimension> requires ValidDimension<Dimension>
    void ExpandingPolytopeAlgorithm<Dimension>::expandPolytope(
        const vector_type& newVertex,
        const vector_type& supportA,
        const vector_type& supportB)
    {
        // 添加新顶点
        const int newVertexIdx = static_cast<int>(m_vertices.cols());
        m_vertices.conservativeResize(Eigen::NoChange, m_vertices.cols() + 1);
        m_vertices.col(newVertexIdx) = newVertex;
        m_supportA.push_back(supportA);
        m_supportB.push_back(supportB);

        // 1. 识别从新顶点可见的面
        std::vector<int> visibleFaceIndices;
        visibleFaceIndices.reserve(m_faces.size());
        for (size_t i = 0; i < m_faces.size(); ++i) {
            if (isFaceVisibleFromPoint(m_faces[i], newVertex)) {
                visibleFaceIndices.push_back(static_cast<int>(i));
            }
        }

        // 若没有可见面，说明新顶点在多面体内部，不扩展
        if (visibleFaceIndices.empty()) {
            // 回退：移除刚添加的顶点
            m_vertices.conservativeResize(Eigen::NoChange, m_vertices.cols() - 1);
            m_supportA.pop_back();
            m_supportB.pop_back();
            return;
        }

        // 2. 收集边界脊（在可见面中恰好出现一次的 (Dimension-2)-单纯形）
        // 使用排序顶点索引向量的哈希集合来计数
        struct VectorHash {
            size_t operator()(const std::vector<int>& v) const {
                size_t h = 0;
                for (int x : v) {
                    h ^= std::hash<int>{}(x) + 0x9e3779b9 + (h << 6) + (h >> 2);
                }
                return h;
            }
        };
        std::unordered_set<std::vector<int>, VectorHash> boundaryRidges;
        for (int fi : visibleFaceIndices) {
            const Face& face = m_faces[fi];
            const int nv = static_cast<int>(face.vertexIndices.size());
            // 枚举脊：对面上每个顶点，省略该顶点得到一个 (Dimension-2)-单纯形
            for (int omit = 0; omit < nv; ++omit) {
                std::vector<int> ridge;
                ridge.reserve(static_cast<size_t>(nv - 1));
                for (int j = 0; j < nv; ++j) {
                    if (j != omit) {
                        ridge.push_back(face.vertexIndices[j]);
                    }
                }
                std::ranges::sort(ridge);
                // 切换计数：出现奇数次→边界脊，偶数次→内部脊
                if (boundaryRidges.contains(ridge)) {
                    boundaryRidges.erase(ridge);
                } else {
                    boundaryRidges.insert(std::move(ridge));
                }
            }
        }

        // 3. 移除所有可见面（从后向前删除以保持索引有效）
        std::ranges::sort(visibleFaceIndices, std::greater());
        for (int fi : visibleFaceIndices) {
            m_faces.erase(m_faces.begin() + fi);
        }

        // 4. 为每条边界脊创建新面（由脊的所有顶点 + 新顶点构成）
        for (const auto& ridge : boundaryRidges) {
            Face newFace;
            newFace.vertexIndices = ridge;
            newFace.vertexIndices.push_back(newVertexIdx);
            computeFaceNormal(newFace);

            // 仅当法向量有效时添加
            if (newFace.normal.squaredNorm() > m_epsilon * m_epsilon) {
                m_faces.push_back(std::move(newFace));
            }
        }
    }

    // 结果提取
    template<unsigned int Dimension> requires ValidDimension<Dimension>
    EPAResult<Dimension> ExpandingPolytopeAlgorithm<Dimension>::extractResult(const Face& closestFace) const
    {
        EPAResult<Dimension> result;

        const vector_type& n = closestFace.normal;
        const double d = closestFace.distance;

        // 碰撞法线
        result.normal = n;

        // 穿透深度
        result.penetrationDepth = d;

        // 闵可夫斯基差边界上的最近点 p* = d · n
        const vector_type pStar = d * n;

        // 计算 p* 在最近面上的重心坐标
        const int k = static_cast<int>(closestFace.vertexIndices.size());

        // 收集面上顶点和对应原始支撑点
        std::vector<vector_type> faceVertices(k);
        std::vector<vector_type> faceSupportA(k);
        std::vector<vector_type> faceSupportB(k);

        for (int j = 0; j < k; ++j) {
            const int idx = closestFace.vertexIndices[j];
            faceVertices[j] = m_vertices.col(idx);
            faceSupportA[j] = (idx < static_cast<int>(m_supportA.size()))
                ? m_supportA[idx] : vector_type::Zero();
            faceSupportB[j] = (idx < static_cast<int>(m_supportB.size()))
                ? m_supportB[idx] : vector_type::Zero();
        }

        // 求解重心坐标：p* = Σ λ_j v_j, Σ λ_j = 1, λ_j ≥ 0
        // 令 δ_j = v_j - v_0 (j = 1..k-1)，则 p* - v_0 = D · λ'
        // 使用伪逆统一处理所有维度
        Eigen::VectorXd lambda(k);

        if (k >= 2) {
            // 构建偏移矩阵 D = [δ_1 | ... | δ_{k-1}]
            Eigen::Matrix<double, Dimension, Eigen::Dynamic> D(Dimension, k - 1);
            for (int j = 1; j < k; ++j) {
                D.col(j - 1) = faceVertices[j] - faceVertices[0];
            }
            const vector_type q = pStar - faceVertices[0];

            // 伪逆求解 λ' = D⁺ q
            Eigen::VectorXd lambdaPrime = D.completeOrthogonalDecomposition().solve(q);
            lambda(0) = 1.0 - lambdaPrime.sum();
            for (int j = 1; j < k; ++j) {
                lambda(j) = lambdaPrime(j - 1);
            }

            // 钳制到 [0, 1] 并重新归一化
            for (int j = 0; j < k; ++j) {
                lambda(j) = std::max(0.0, std::min(1.0, lambda(j)));
            }
            const double sum = lambda.sum();
            if (sum > m_epsilon) {
                lambda /= sum;
            } else {
                lambda = Eigen::VectorXd::Constant(k, 1.0 / static_cast<double>(k));
            }
        } else {
            // 单点退化：直接使用该点
            lambda = Eigen::VectorXd::Constant(k, 1.0 / static_cast<double>(k));
        }

        // 计算原始物体上的最近点
        vector_type aStar = vector_type::Zero();
        vector_type bStar = vector_type::Zero();
        for (int j = 0; j < k; ++j) {
            aStar += lambda(j) * faceSupportA[j];
            bStar += lambda(j) * faceSupportB[j];
        }

        result.closestPair = {aStar, bStar};

        // 接触点取中点
        result.contactPoint = (aStar + bStar) * 0.5;

        return result;
    }

// ──── 三维类型别名 ────
using EPAResult3D = EPAResult<3>;
using ExpandingPolytopeAlgorithm3D = ExpandingPolytopeAlgorithm<3>;

} // namespace egret
// #endif // EGRET_PHYSICS_EXPANDING_POLYTOPE_ALGORITHM_H
