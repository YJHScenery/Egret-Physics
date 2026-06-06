/**
 * @file        convex_mesh.cpp
 * @brief       闭合凸体 Mesh 形状实现文件。
 * @details     实现 ConvexMesh 类的构造、几何验证、质量属性计算及碰撞查询函数。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-06
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 */

#include "convex_mesh.h"

#include <limits>
#include <map>
#include <set>
#include <utility>

namespace egret
{

// 构造函数
ConvexMesh::ConvexMesh(const TessellatedMesh &mesh)
{
    // ---- 1. 拷贝顶点 ----
    m_vertices = mesh.vertices;

    // ---- 2. 拷贝并打包三角形索引 ----
    const auto &srcIndices = mesh.indices;
    const std::size_t triCount = srcIndices.size() / 3;
    m_triangles.reserve(triCount);
    for (std::size_t i = 0; i < triCount; ++i) {
        const std::size_t base = i * 3;
        m_triangles.push_back({
            static_cast<std::uint32_t>(srcIndices[base]),
            static_cast<std::uint32_t>(srcIndices[base + 1]),
            static_cast<std::uint32_t>(srcIndices[base + 2])
        });
    }

    // ---- 3. 基本有效性检查 ----
    const auto vertexCount = m_vertices.size();
    if (vertexCount < 4 || m_triangles.size() < 4) {
        setValid(false);
        return;
    }

    // 检查所有索引在有效范围内
    for (const auto &tri : m_triangles) {
        if (tri[0] >= vertexCount || tri[1] >= vertexCount || tri[2] >= vertexCount) {
            setValid(false);
            return;
        }
    }

    // ---- 4. 计算面法线并检测退化三角形 ----
    m_faceNormals.reserve(m_triangles.size());
    constexpr double kDegenerateEpsilon = 1e-12;
    for (const auto &tri : m_triangles) {
        const Eigen::Vector3d &v0 = m_vertices[tri[0]];
        const Eigen::Vector3d &v1 = m_vertices[tri[1]];
        const Eigen::Vector3d &v2 = m_vertices[tri[2]];

        const Eigen::Vector3d e1 = v1 - v0;
        const Eigen::Vector3d e2 = v2 - v0;
        const Eigen::Vector3d normal = e1.cross(e2);
        const double area2 = normal.squaredNorm();

        if (area2 < kDegenerateEpsilon) {
            // 退化三角形（面积为零），拒绝构造
            setValid(false);
            return;
        }

        m_faceNormals.push_back(normal.normalized());
    }

    // ---- 5. 统一面法线方向（通过有向体积判断） ----
    double signedVolume = 0.0;
    for (std::size_t i = 0; i < m_triangles.size(); ++i) {
        const auto &tri = m_triangles[i];
        const Eigen::Vector3d &v0 = m_vertices[tri[0]];
        const Eigen::Vector3d &v1 = m_vertices[tri[1]];
        const Eigen::Vector3d &v2 = m_vertices[tri[2]];
        // 有向体积 = (1/6) * v0 · (v1 × v2)
        signedVolume += v0.dot(v1.cross(v2));
    }

    // 若有向体积为负，翻转三角形绕序与法线使其一致向外
    if (signedVolume < 0.0) {
        for (auto &tri : m_triangles) {
            std::swap(tri[1], tri[2]);
        }
        for (auto &n : m_faceNormals) {
            n = -n;
        }
    }

    // ---- 6. 封闭性检测 ----
    if (!checkClosedness()) {
        setValid(false);
        return;
    }

    // ---- 7. 凸体检测 ----
    if (!checkConvexity()) {
        setValid(false);
        return;
    }

    // ---- 8. 计算体积与质心，平移顶点使质心归零 ----
    computeMassProperties();

    // ---- 9. 计算单位密度转动惯量矩阵（关于质心即原点） ----
    computeInertiaTensorPerUnitDensity();

    // ---- 10. 计算局部 AABB ----
    m_localAABBMin = Eigen::Vector3d::Constant(std::numeric_limits<double>::infinity());
    m_localAABBMax = Eigen::Vector3d::Constant(-std::numeric_limits<double>::infinity());
    for (const auto &v : m_vertices) {
        m_localAABBMin = m_localAABBMin.cwiseMin(v);
        m_localAABBMax = m_localAABBMax.cwiseMax(v);
    }
}

// ShapeBase 接口
ShapeType ConvexMesh::getType() const
{
    return ShapeType::ConvexMesh;
}

double ConvexMesh::getVolume() const
{
    return m_volume;
}

Eigen::Vector3d ConvexMesh::getCenterOfMass() const
{
    return Eigen::Vector3d::Zero();
}

Eigen::Matrix3d ConvexMesh::getInertiaTensor(const double mass) const
{
    if (m_volume <= 0.0) {
        return Eigen::Matrix3d::Zero();
    }
    // I(mass) = (mass / volume) * I_per_unit_volume
    return (mass / m_volume) * m_inertiaPerUnitDensity;
}

AABB ConvexMesh::getAABB(const Transform &transform) const
{
    Eigen::Vector3d worldMin = Eigen::Vector3d::Constant(std::numeric_limits<double>::infinity());
    Eigen::Vector3d worldMax = Eigen::Vector3d::Constant(-std::numeric_limits<double>::infinity());

    for (const auto &v : m_vertices) {
        const Eigen::Vector3d wv = transform.localToWorld(v);
        worldMin = worldMin.cwiseMin(wv);
        worldMax = worldMax.cwiseMax(wv);
    }

    return {worldMin, worldMax};
}

ShapeLoadInfo ConvexMesh::getLoadInfo() const
{
    ShapeLoadInfo info{};
    info.typeId = static_cast<std::uint32_t>(ShapeType::ConvexMesh);

    // 将顶点扁平化存储
    std::vector<double> flatVertices;
    flatVertices.reserve(m_vertices.size() * 3);
    for (const auto &v : m_vertices) {
        flatVertices.push_back(v.x());
        flatVertices.push_back(v.y());
        flatVertices.push_back(v.z());
    }
    info.numberParams["vertices"] = std::move(flatVertices);

    // 将索引扁平化存储（uint32 → double）
    std::vector<double> flatIndices;
    flatIndices.reserve(m_triangles.size() * 3);
    for (const auto &tri : m_triangles) {
        flatIndices.push_back(static_cast<double>(tri[0]));
        flatIndices.push_back(static_cast<double>(tri[1]));
        flatIndices.push_back(static_cast<double>(tri[2]));
    }
    info.numberParams["indices"] = std::move(flatIndices);

    return info;
}

Eigen::Vector3d ConvexMesh::support(const Eigen::Vector3d &direction,
                                     const Transform &transform) const
{
    // 将世界空间方向旋转到局部空间
    const Eigen::Vector3d localDir = transform.getRotation().conjugate() * direction;

    // 穷举搜索 dot 最大的顶点
    double maxDot = -std::numeric_limits<double>::infinity();
    Eigen::Vector3d bestVertex = Eigen::Vector3d::Zero();
    for (const auto &v : m_vertices) {
        const double dot = v.dot(localDir);
        if (dot > maxDot) {
            maxDot = dot;
            bestVertex = v;
        }
    }

    // 变换回世界空间
    return transform.localToWorld(bestVertex);
}

// 封闭性检测
bool ConvexMesh::checkClosedness() const
{
    // 使用 map 统计每条无序边的出现次数
    // key = (minIndex, maxIndex)，value = 出现次数
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> edgeCount;

    for (const auto &tri : m_triangles) {
        const std::uint32_t i0 = tri[0];
        const std::uint32_t i1 = tri[1];
        const std::uint32_t i2 = tri[2];

        // 三条边，存储时统一排序使 (小, 大)
        auto addEdge = [&edgeCount](const std::uint32_t a, const std::uint32_t b) {
            const auto key = (a < b) ? std::make_pair(a, b) : std::make_pair(b, a);
            ++edgeCount[key];
        };

        addEdge(i0, i1);
        addEdge(i1, i2);
        addEdge(i2, i0);
    }

    // 每条边必须恰好出现在两个三角形中（即出现 2 次）
    for (const auto &[edge, count] : edgeCount) {
        if (count != 2) {
            return false; // 存在边界边或非流形边
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// 凸体检测
// ---------------------------------------------------------------------------

bool ConvexMesh::checkConvexity() const
{
    constexpr double kConvexEpsilon = 1e-9;

    const std::size_t faceCount = m_triangles.size();
    const std::size_t vertexCount = m_vertices.size();

    for (std::size_t fi = 0; fi < faceCount; ++fi) {
        const auto &tri = m_triangles[fi];
        const Eigen::Vector3d &normal = m_faceNormals[fi];

        // 取三角形第一个顶点作为平面上一点
        const Eigen::Vector3d &planePoint = m_vertices[tri[0]];

        // 收集当前面的三个顶点索引，用于跳过
        const std::set<std::uint32_t> faceVerts = {tri[0], tri[1], tri[2]};

        // 检查所有不在此面的顶点是否都在平面后侧（或平面上）
        for (std::size_t vi = 0; vi < vertexCount; ++vi) {
            if (faceVerts.count(static_cast<std::uint32_t>(vi)) > 0) {
                continue; // 跳过面自身的顶点
            }

            const double dist = (m_vertices[vi] - planePoint).dot(normal);
            if (dist > kConvexEpsilon) {
                // 存在顶点在面平面的正半空间（外侧），网格非凸
                return false;
            }
        }
    }

    return true;
}

// 质量属性计算：体积与质心归零
void ConvexMesh::computeMassProperties()
{
    double totalSignedVolume = 0.0;
    Eigen::Vector3d weightedCom = Eigen::Vector3d::Zero();

    for (const auto &tri : m_triangles) {
        const Eigen::Vector3d &v0 = m_vertices[tri[0]];
        const Eigen::Vector3d &v1 = m_vertices[tri[1]];
        const Eigen::Vector3d &v2 = m_vertices[tri[2]];

        // 以原点为公共顶点的四面体有向体积
        const double tetVolume = v0.dot(v1.cross(v2)) / 6.0;
        totalSignedVolume += tetVolume;

        // 四面体质心（四个顶点的算术平均，原点为第四个顶点）
        const Eigen::Vector3d tetCentroid = (v0 + v1 + v2) / 4.0;
        weightedCom += tetVolume * tetCentroid;
    }

    // 体积取绝对值（有向体积符号取决于三角形绕序，几何体积恒正）
    m_volume = std::abs(totalSignedVolume);

    if (m_volume <= 0.0) {
        // 退化体积，不应到达此处（前置检测已过滤）
        return;
    }

    const Eigen::Vector3d centerOfMass = weightedCom / m_volume;

    // 将所有顶点平移，使质心归零
    for (auto &v : m_vertices) {
        v -= centerOfMass;
    }

    // 面法线仅依赖顶点差，平移不改变，无需更新
}

// ---------------------------------------------------------------------------
// 单位密度转动惯量矩阵计算（关于质心即原点）
// ---------------------------------------------------------------------------

void ConvexMesh::computeInertiaTensorPerUnitDensity()
{
    // M = ∫_V r⊗r dV，关于原点（即质心）
    Eigen::Matrix3d M = Eigen::Matrix3d::Zero();

    for (const auto &tri : m_triangles) {
        const Eigen::Vector3d &a = m_vertices[tri[0]];
        const Eigen::Vector3d &b = m_vertices[tri[1]];
        const Eigen::Vector3d &c = m_vertices[tri[2]];

        // 四面体 (origin, a, b, c) 的有向体积
        const double detABC = a.dot(b.cross(c));
        const double tetVolume = detABC / 6.0;

        // ∫_T r⊗r dV = (V/20) * [Σ_k V_k⊗V_k + S⊗S]，其中 S = Σ_k V_k，V_k 为四面体顶点
        // 对于 V₀ = origin：S = a + b + c，V = detABC / 6，系数 = detABC / 120
        // 展开后：对角项系数为 2，交叉项系数为 1
        const double coeff = detABC / 120.0;

        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                // 对角项 (a⊗a, b⊗b, c⊗c) 系数为 2
                const double diag = 2.0 * (a[row] * a[col] + b[row] * b[col] + c[row] * c[col]);
                // 交叉项 (a⊗b + b⊗a + a⊗c + c⊗a + b⊗c + c⊗b) 系数为 1
                const double cross = a[row] * b[col] + b[row] * a[col]
                                   + a[row] * c[col] + c[row] * a[col]
                                   + b[row] * c[col] + c[row] * b[col];
                M(row, col) += coeff * (diag + cross);
            }
        }
    }

    // I = trace(M)·I₃ - M
    const double traceM = M.trace();
    m_inertiaPerUnitDensity = traceM * Eigen::Matrix3d::Identity() - M;
}

} // egret