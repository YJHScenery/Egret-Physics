//
// Created by jehor on 2026/6/6.
//
#include "tessellated_mesh.h"

#include <ranges>
#include <unordered_map>
#include <unordered_set>

namespace egret
{
    bool TessellatedMesh::isValid() const noexcept
    {
        return !vertices.empty() && !indices.empty();
    }

    std::size_t TessellatedMesh::triangleCount() const noexcept
    {
        return indices.size();
    }

    std::size_t TessellatedMesh::vertexCount() const noexcept
    {
        return vertices.size();
    }

    void TessellatedMesh::clear() noexcept
    {
        vertices.clear();
        normals.clear();
        indices.clear();
    }

    bool TessellatedMesh::checkClosed() const
    {
        if (indices.empty()) return false;

        std::unordered_map<LineIndexType, std::uint64_t, EdgeHash, EdgeEqual> lineCountMap;
        lineCountMap.reserve(indices.size() * 3);

        for (const TriangleIndexType& triangle : indices) {
            const auto a = triangle.x();
            const auto b = triangle.y();
            const auto c = triangle.z();

            // 每条边计数，超过 2 则尽早退出（非流形或非闭合）
            if (++lineCountMap[LineIndexType{a, b}] > 2) return false;
            if (++lineCountMap[LineIndexType{b, c}] > 2) return false;
            if (++lineCountMap[LineIndexType{c, a}] > 2) return false;
        }

        // 闭合充要条件：每条边恰好出现 2 次
        for (const auto& [edge, count] : lineCountMap) {
            if (count != 2) return false;
        }
        return true;
    }

    bool TessellatedMesh::checkManifold() const
    {
        if (indices.empty()) return false;

        // ---- 第 1 步：边流形检查 ----
        // 每条边只能被 1 个三角形（边界边）或 2 个三角形（内部边）共享
        std::unordered_map<LineIndexType, std::uint64_t, EdgeHash, EdgeEqual> edgeCount;
        edgeCount.reserve(indices.size() * 3);

        for (const TriangleIndexType& tri : indices) {
            const auto a = tri.x(), b = tri.y(), c = tri.z();
            ++edgeCount[LineIndexType{a, b}];
            ++edgeCount[LineIndexType{b, c}];
            ++edgeCount[LineIndexType{c, a}];
        }

        for (const auto& [edge, count] : edgeCount) {
            if (count != 1 && count != 2) return false;
        }

        // ---- 第 2 步：构建顶点 → 三角形邻接 ----
        std::unordered_map<IndexType, std::vector<std::size_t>> vertexTriangles;
        for (std::size_t ti = 0; ti < indices.size(); ++ti) {
            const auto& tri = indices[ti];
            vertexTriangles[tri.x()].push_back(ti);
            vertexTriangles[tri.y()].push_back(ti);
            vertexTriangles[tri.z()].push_back(ti);
        }

        // ---- 第 3 步：顶点链环检查 ----
        // 对每个顶点，其链环（link）必须为单一闭合环（内部顶点）或单一开链（边界顶点）
        for (const auto& [v, triangleList] : vertexTriangles) {
            // 3a. 构建链环邻接图：对每个包含 v 的三角形，其对边即为链环边
            std::unordered_map<IndexType, std::vector<IndexType>> linkAdj;

            for (std::size_t ti : triangleList) {
                const auto& tri = indices[ti];
                IndexType u, w;
                if (tri.x() == v) {
                    u = tri.y();
                    w = tri.z();
                }
                else if (tri.y() == v) {
                    u = tri.x();
                    w = tri.z();
                }
                else {
                    u = tri.x();
                    w = tri.y();
                }

                // 退化三角形检测：对边两端点相同
                if (u == w) return false;

                linkAdj[u].push_back(w);
                linkAdj[w].push_back(u);
            }

            // 3b. 度数检查：链环中每个顶点度数只能为 1 或 2
            IndexType degree1Count = 0;
            for (const auto& [linkV, neighbors] : linkAdj) {
                const auto deg = neighbors.size();
                if (deg == 1) {
                    ++degree1Count;
                }
                else if (deg != 2) {
                    return false; // 度数 > 2 → 非流形
                }
            }

            // 边界情况：只能有 0 个（内部顶点）或 2 个（边界顶点）度数为 1 的节点
            if (degree1Count != 0 && degree1Count != 2) return false;

            // 3c. 连通性检查：链环必须为单一连通分量
            if (!linkAdj.empty()) {
                std::unordered_set<IndexType> visited;
                std::vector<IndexType> stack;
                const auto start = linkAdj.begin()->first;
                stack.push_back(start);
                visited.insert(start);

                while (!stack.empty()) {
                    const auto curr = stack.back();
                    stack.pop_back();
                    for (const auto neighbor : linkAdj.at(curr)) {
                        if (!visited.count(neighbor)) {
                            visited.insert(neighbor);
                            stack.push_back(neighbor);
                        }
                    }
                }

                if (visited.size() != linkAdj.size()) return false;
            }
        }

        return true;
    }

    bool TessellatedMesh::checkConvex() const
    {
        // 凸体必须是闭合网格
        if (!checkClosed()) return false;

        constexpr double kEpsilon = 1e-9;

        for (const TriangleIndexType& tri : indices) {
            const auto& p0 = vertices[tri.x()];
            const auto& p1 = vertices[tri.y()];
            const auto& p2 = vertices[tri.z()];

            // 使用叉积计算面法向量（无需归一化，符号已足够）
            const Eigen::Vector3d normal = (p1 - p0).cross(p2 - p0);
            const double areaSq = normal.squaredNorm();
            if (areaSq < kEpsilon * kEpsilon) return false; // 退化三角形

            // 检查所有顶点是否位于该面支撑平面的同一侧
            bool hasPositive = false;
            bool hasNegative = false;

            for (const auto& v : vertices) {
                const double d = normal.dot(v - p0);
                if (d > kEpsilon) {
                    hasPositive = true;
                }
                else if (d < -kEpsilon) {
                    hasNegative = true;
                }

                // 顶点分布在面两侧 → 非凸
                if (hasPositive && hasNegative) return false;
            }
        }

        return true;
    }

    AABB TessellatedMesh::getAABB() const
    {
        Eigen::Vector3d min{vertices[0]};
        Eigen::Vector3d max{vertices[0]};

        for (const auto& v : vertices) {
            min.x() = std::min(min.x(), v.x());
            max.x() = std::max(max.x(), v.x());
            min.y() = std::min(min.y(), v.y());
            max.y() = std::max(max.y(), v.y());
            min.z() = std::min(min.z(), v.z());
            max.z() = std::max(max.z(), v.z());
        }
        return {min, max};
    }

}
