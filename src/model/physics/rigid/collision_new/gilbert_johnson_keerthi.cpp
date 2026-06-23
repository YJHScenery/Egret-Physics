//
// Created by jehor on 2026/6/23.
//

#include "gilbert_johnson_keerthi.h"
#include <limits>

namespace egret
{
    GilbertJohnsonKeerthi::GilbertJohnsonKeerthi(PhysicalEntity* entityA, PhysicalEntity* entityB)
        : m_entities({entityA, entityB})
    {
    }

    void GilbertJohnsonKeerthi::initialize()
    {
        const Transform& transA = m_entities.first->getTransformCR();
        const Transform& transB = m_entities.second->getTransformCR();

        // 1. 选定初始方向：center(B) - center(A)
        m_currentDirection = transB.getTranslation() - transA.getTranslation();

        // 若中心重合，使用默认方向 (0, 0, 1)
        if (m_currentDirection.squaredNorm() < kEpsilon * kEpsilon) {
            m_currentDirection = vector_type(0.0, 0.0, 1.0);
        }

        // 2. 获取第一个闵可夫斯基差支撑点
        const auto supportA = m_entities.first->getShape()->support(m_currentDirection, transA);
        const auto supportB = m_entities.second->getShape()->support(-m_currentDirection, transB);

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

    GilbertJohnsonKeerthi::result_type GilbertJohnsonKeerthi::run()
    {
        result_type gjkResult;

        for (std::uint64_t iter = 0; iter < kMaxIterations; ++iter) {
            const Transform& transA = m_entities.first->getTransformCR();
            const Transform& transB = m_entities.second->getTransformCR();

            // 1. 获取新支撑点
            const auto supportA = m_entities.first->getShape()->support(m_currentDirection, transA);
            const auto supportB = m_entities.second->getShape()->support(-m_currentDirection, transB);
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

    double GilbertJohnsonKeerthi::computeOptimalSubset()
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
                if (x(j) <= -kEpsilon) {
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

    bool GilbertJohnsonKeerthi::containsOrigin()
    {
        const double bestDistance = computeOptimalSubset();

        // 原点包含判定：||p*|| < ε
        if (bestDistance < kEpsilon) {
            // 碰撞：保留完整单纯形供 EPA 使用，不裁剪
            return true;
        }

        // 未碰撞：更新搜索方向为 -p*
        m_currentDirection = -m_optimalClosestPoint;

        // 裁剪单纯形，仅保留最优子集的顶点
        const auto newCols = static_cast<Eigen::Index>(m_optimalSubsetIndices.size());
        matrix_type newSimplex(3, newCols);
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

    void GilbertJohnsonKeerthi::buildDataWhenNotCollided(result_type& result)
    {
        // 计算最优子集（结果缓存在成员变量中）
        const double distance = computeOptimalSubset();
        result.distance = distance;

        // 计算原始物体最近点对：
        // a₀ = Σ λ_i · a_i,  b₀ = Σ λ_i · b_i
        vector_type a0 = vector_type::Zero();
        vector_type b0 = vector_type::Zero();

        for (size_t j = 0; j < m_optimalSubsetIndices.size(); ++j) {
            const int idx = m_optimalSubsetIndices[j];
            if (idx < static_cast<int>(m_supportAHistory.size())) {
                const double lambda = m_optimalBarycentric(j);
                a0 += lambda * m_supportAHistory[idx];
                b0 += lambda * m_supportBHistory[idx];
            }
        }

        result.minimumVerticesPair = {a0, b0};
    }

    void GilbertJohnsonKeerthi::addNewColToCurrentSimplex(vector_type newCol)
    {
        m_currentSimplex.conservativeResize(Eigen::NoChange, m_currentSimplex.cols() + 1);
        m_currentSimplex.col(m_currentSimplex.cols() - 1) = std::move(newCol);
    }
} // egret
