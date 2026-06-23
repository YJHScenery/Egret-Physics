/**
 * @file        expanding_polytope_algorithm.cpp
 * @brief       EPA（Expanding Polytope Algorithm）算法实现
 * @details     以 GJK 最终单纯形为初始多面体，通过迭代扩展逼近闵可夫斯基差边界，
 *              提取碰撞法线、穿透深度和接触点。
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

#include "expanding_polytope_algorithm.h"
#include <limits>
#include <unordered_set>
#include <algorithm>

namespace egret
{
    // 构造 / 主入口

    ExpandingPolytopeAlgorithm::ExpandingPolytopeAlgorithm(
        const GJKResult3D& gjkResult,
        PhysicalEntity* entityA,
        PhysicalEntity* entityB)
        : m_entityA(entityA)
        , m_entityB(entityB)
    {
        initializeFromGJK(gjkResult);
    }

    EPAResult ExpandingPolytopeAlgorithm::run()
    {
        // 若初始单纯形顶点数不足 4（非满维），先执行一次扩展构建四面体
        if (m_vertices.cols() < 4) {
            // 找一个不共面的方向获取支撑点
            vector_type dir;
            if (m_vertices.cols() == 1) {
                dir = vector_type(1.0, 0.0, 0.0);
            } else if (m_vertices.cols() == 2) {
                // 取与两点连线正交的方向
                const vector_type edge = m_vertices.col(0) - m_vertices.col(1);
                dir = vector_type(-edge.y(), edge.x(), 0.0);
                if (dir.squaredNorm() < kEpsilon * kEpsilon) {
                    dir = vector_type(0.0, -edge.z(), edge.y());
                }
            } else { // 3 顶点（三角形）
                const vector_type e1 = m_vertices.col(1) - m_vertices.col(0);
                const vector_type e2 = m_vertices.col(2) - m_vertices.col(0);
                dir = e1.cross(e2);
            }

            if (dir.squaredNorm() > kEpsilon * kEpsilon) {
                dir.normalize();
                const Transform& transA = m_entityA->getTransformCR();
                const Transform& transB = m_entityB->getTransformCR();

                const auto sA = m_entityA->getShape()->support(dir, transA);
                const auto sB = m_entityB->getShape()->support(-dir, transB);
                const vector_type sP = sA - sB;

                expandPolytope(sP, sA, sB);
            }
        }

        for (int iter = 0; iter < kMaxIterations; ++iter) {
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
            const Transform& transA = m_entityA->getTransformCR();
            const Transform& transB = m_entityB->getTransformCR();

            const auto supportA = m_entityA->getShape()->support(n, transA);
            const auto supportB = m_entityB->getShape()->support(-n, transB);
            const vector_type supportP = supportA - supportB;

            // 步骤 3：终止条件检查  p · n - d_min ≤ ε
            const double projection = supportP.dot(n);
            if (projection - dMin <= kEpsilon) {
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

        return EPAResult{};
    }

    // 初始化

    void ExpandingPolytopeAlgorithm::initializeFromGJK(const GJKResult3D& gjkResult)
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

    void ExpandingPolytopeAlgorithm::computeFaces()
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

    void ExpandingPolytopeAlgorithm::computeFaceNormal(Face& face) const
    {
        const int k = static_cast<int>(face.vertexIndices.size());  // 面顶点数

        if (k < 1) {
            face.normal = vector_type::Zero();
            face.distance = 0.0;
            return;
        }

        // 构建 V_i 矩阵（3 × k）：每列为面上一个顶点坐标
        Eigen::Matrix<double, 3, Eigen::Dynamic> Vi(3, k);
        for (int c = 0; c < k; ++c) {
            Vi.col(c) = m_vertices.col(face.vertexIndices[c]);
        }

        // 右端向量 1_k = [1, 1, ..., 1]ᵀ
        const Eigen::VectorXd ones = Eigen::VectorXd::Ones(k);

        vector_type ni;

        if (k == 3) {
            // 满维情形（三角形面）：直接求解 V_i^T n_i = 1_3
            const Eigen::Matrix3d ViT = Vi.transpose();
            // 对于非退化三角形，ViT 可逆
            const Eigen::FullPivLU<Eigen::Matrix3d> lu(ViT);
            if (lu.isInvertible()) {
                ni = lu.solve(ones);
            } else {
                // 回退：使用伪逆
                ni = ViT.completeOrthogonalDecomposition().solve(ones);
            }
        } else {
            // 非满维情形：n_i = V_i · (V_i^T V_i)^+ · 1_k
            // 等价于求 V_i^T n_i = 1_k 的最小范数解
            const Eigen::MatrixXd ViT = Vi.transpose();  // k × 3
            ni = ViT.completeOrthogonalDecomposition().solve(ones);
        }

        const double niNorm = ni.norm();
        if (niNorm < kEpsilon) {
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

    bool ExpandingPolytopeAlgorithm::isFaceVisibleFromPoint(
        const Face& face, const vector_type& point) const
    {
        // 点在面的外侧 ⇔ n · p > d + ε
        return face.normal.dot(point) > face.distance + kEpsilon;
    }

    // 多面体扩展

    void ExpandingPolytopeAlgorithm::expandPolytope(
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

        // 2. 收集边界边（在可见面中恰好出现一次的边）
        // 使用无序集合来计数：key = (minIdx, maxIdx) 的哈希对
        std::unordered_set<uint64_t> edgeCount;
        for (int fi : visibleFaceIndices) {
            const Face& face = m_faces[fi];
            const int nv = static_cast<int>(face.vertexIndices.size());
            for (int e = 0; e < nv; ++e) {
                const auto [a, b] = face.getEdge(e);
                const int lo = std::min(a, b);
                const int hi = std::max(a, b);
                const uint64_t key = (static_cast<uint64_t>(lo) << 32) | static_cast<uint64_t>(hi);

                // 切换计数：出现奇数次→边界边，偶数次→内部边
                if (edgeCount.count(key)) {
                    edgeCount.erase(key);
                } else {
                    edgeCount.insert(key);
                }
            }
        }

        // 3. 移除所有可见面（从后向前删除以保持索引有效）
        std::sort(visibleFaceIndices.begin(), visibleFaceIndices.end(), std::greater<int>());
        for (int fi : visibleFaceIndices) {
            m_faces.erase(m_faces.begin() + fi);
        }

        // 4. 为每条边界边创建新三角形面（由边 + 新顶点构成）
        for (uint64_t key : edgeCount) {
            const int a = static_cast<int>(key >> 32);
            const int b = static_cast<int>(key & 0xFFFFFFFF);

            Face newFace;
            newFace.vertexIndices = {a, b, newVertexIdx};
            computeFaceNormal(newFace);

            // 仅当法向量有效时添加
            if (newFace.normal.squaredNorm() > kEpsilon * kEpsilon) {
                m_faces.push_back(std::move(newFace));
            }
        }
    }

    // 结果提取

    EPAResult ExpandingPolytopeAlgorithm::extractResult(const Face& closestFace) const
    {
        EPAResult result;

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
        Eigen::VectorXd lambda;

        if (k == 3) {
            // 三角形面：使用重心坐标插值
            // 使用边向量方法：p* - v_0 = λ_1 (v_1 - v_0) + λ_2 (v_2 - v_0)
            const vector_type e01 = faceVertices[1] - faceVertices[0];
            const vector_type e02 = faceVertices[2] - faceVertices[0];
            const vector_type e0p = pStar - faceVertices[0];

            // 求解 2×2 正规方程
            const double a11 = e01.dot(e01);
            const double a12 = e01.dot(e02);
            const double a22 = e02.dot(e02);
            const double b1 = e01.dot(e0p);
            const double b2 = e02.dot(e0p);

            const double det = a11 * a22 - a12 * a12;
            lambda = Eigen::VectorXd(3);

            if (std::abs(det) > kEpsilon) {
                const double lambda1 = (a22 * b1 - a12 * b2) / det;
                const double lambda2 = (a11 * b2 - a12 * b1) / det;
                lambda(0) = 1.0 - lambda1 - lambda2;
                lambda(1) = lambda1;
                lambda(2) = lambda2;

                // 钳制到 [0, 1] 并重新归一化
                for (int j = 0; j < 3; ++j) {
                    lambda(j) = std::max(0.0, std::min(1.0, lambda(j)));
                }
                const double sum = lambda.sum();
                if (sum > kEpsilon) {
                    lambda /= sum;
                } else {
                    lambda = Eigen::VectorXd::Constant(3, 1.0 / 3.0);
                }
            } else {
                // 退化三角形：均匀分配
                lambda = Eigen::VectorXd::Constant(3, 1.0 / 3.0);
            }
        } else if (k == 2) {
            // 线段面：线性插值
            const vector_type v0 = faceVertices[0];
            const vector_type v1 = faceVertices[1];
            const vector_type edge = v1 - v0;
            const double edgeLenSq = edge.squaredNorm();

            lambda = Eigen::VectorXd(2);
            if (edgeLenSq > kEpsilon) {
                double t = (pStar - v0).dot(edge) / edgeLenSq;
                t = std::max(0.0, std::min(1.0, t));
                lambda(0) = 1.0 - t;
                lambda(1) = t;
            } else {
                lambda(0) = 0.5;
                lambda(1) = 0.5;
            }
        } else {
            // 单点或退化：直接使用该点
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

} // namespace egret
