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
#include <unordered_set>
#include <utility>

#include "tessellated_mesh.h"

namespace egret
{
    ConvexMesh::ConvexMesh(TessellatedMesh tessellatedMesh)
    {
        if (!(tessellatedMesh.checkClosed() && tessellatedMesh.checkManifold() && tessellatedMesh.checkConvex())) {
            setValid(false);
            LOG_WARN(std::format("Fail to generate Convex Mesh"));
            LOG_DEBUG(std::to_string(tessellatedMesh.checkClosed()));
            LOG_DEBUG(std::to_string(tessellatedMesh.checkManifold()));
            LOG_DEBUG(std::to_string(tessellatedMesh.checkConvex()));
            return;
        }
        m_dataMesh = std::move(tessellatedMesh);
        m_volumeCache = calculateVolume();
        m_inertiaTensorCache = calculateInertiaTensor();
        m_aabbCache = m_dataMesh.getAABB();

        offsetToCenter();
        buildVertexAdjacency();
    }

    ShapeType ConvexMesh::getType() const
    {
        return ShapeType::ConvexMesh;
    }

    double ConvexMesh::getVolume() const
    {
        if (!m_volumeCache) {
            m_volumeCache = calculateVolume();
        }
        return m_volumeCache;
    }

    Eigen::Vector3d ConvexMesh::getCenterOfMass() const
    {
        return Eigen::Vector3d::Zero();
    }

    Eigen::Matrix3d ConvexMesh::getInertiaTensor(double mass) const
    {
        if (m_inertiaTensorCache.norm() == 0) {
            m_inertiaTensorCache = calculateInertiaTensor();
        }
        return mass * m_inertiaTensorCache;
    }

    AABB ConvexMesh::getAABB(const Transform& transform) const
    {
        if ((m_aabbCache.max - m_aabbCache.min).norm() <= 1e-12) {
            m_aabbCache = m_dataMesh.getAABB();
        }
        return m_aabbCache;
    }

    Eigen::Vector3d ConvexMesh::support(const Eigen::Vector3d& direction, const Transform& transform) const
    {
        // 将方向转换到局部坐标系（仅旋转，方向向量不受平移和缩放影响）
        const Eigen::Vector3d localDir{transform.getRotation().conjugate() * direction};

        const auto& verts{m_dataMesh.vertices};

        // 根据方向的主轴符号选择极值顶点作为初始猜测
        TessellatedMesh::IndexType current;
        {
            const double absX{std::abs(localDir.x())};
            const double absY{std::abs(localDir.y())};
            const double absZ{std::abs(localDir.z())};

            if (absX >= absY && absX >= absZ) {
                current = (localDir.x() > 0.0) ? m_extremalVertices[0] : m_extremalVertices[1];
            } else if (absY >= absZ) {
                current = (localDir.y() > 0.0) ? m_extremalVertices[2] : m_extremalVertices[3];
            } else {
                current = (localDir.z() > 0.0) ? m_extremalVertices[4] : m_extremalVertices[5];
            }
        }

        // 若缓存顶点有更好的投影，则使用缓存
        {
            const double hintDot{localDir.dot(verts[m_supportHint])};
            const double currentDot{localDir.dot(verts[current])};
            if (hintDot > currentDot) {
                current = m_supportHint;
            }
        }

        double bestDot{localDir.dot(verts[current])};

        // 爬山法：沿邻接边贪婪地向更高投影方向移动
        // 对凸多面体，局部最大值即全局最大值
        bool improved{true};
        while (improved) {
            improved = false;
            for (const auto& neighbor : m_vertexNeighbors[current]) {
                const double dot{localDir.dot(verts[neighbor])};
                if (dot > bestDot) {
                    bestDot = dot;
                    current = neighbor;
                    improved = true;
                    break;
                }
            }
        }

        // 缓存本次结果，用于后续查询的热启动
        m_supportHint = current;

        // 将局部支撑点变换到世界坐标
        return transform.localToWorld(verts[current]);
    }

    ShapeLoadInfo ConvexMesh::getLoadInfo() const
    {
        // LOG_WARN(std::format("ConvexMesh Cannot use function {}.", __func__));
        return ShapeBase::getLoadInfo();
    }

    double ConvexMesh::calculateVolume() const
    {
        double volume{0.0};
        for (const auto& triangle : m_dataMesh.indices) {
            volume += calculateSingleVolume(triangle);
        }
        return std::abs(volume);
    }

    Eigen::Matrix3d ConvexMesh::calculateInertiaTensor() const
    {
        Eigen::Matrix3d inertiaTensor;

        for (const auto& triangle : m_dataMesh.indices) {
            Eigen::Matrix3d moment{Eigen::Matrix3d::Identity()};
            moment *= (calculateSingleVolume(triangle) / 20.0);
            std::array<Eigen::Vector3d, 3> points{m_dataMesh.vertices.at(triangle.x()), m_dataMesh.vertices.at(triangle.y()), m_dataMesh.vertices.at(triangle.z())};
            for (const Eigen::Vector3d& outP: points) {
                for (const Eigen::Vector3d& inP: points) {
                    moment += (outP * inP.transpose() + inP * outP.transpose()) / 2.0;
                }
            }

            inertiaTensor += moment.trace() * Eigen::Matrix3d::Identity() - moment;
        }

        return inertiaTensor;
    }

    void ConvexMesh::buildVertexAdjacency()
    {
        const auto vertexCount{m_dataMesh.vertexCount()};

        // 使用临时 unordered_set 收集每个顶点的邻居，自动去重
        std::vector<std::unordered_set<TessellatedMesh::IndexType>> neighborSets(vertexCount);

        for (const auto& tri : m_dataMesh.indices) {
            const auto a{tri.x()};
            const auto b{tri.y()};
            const auto c{tri.z()};

            neighborSets[a].insert(b);
            neighborSets[a].insert(c);
            neighborSets[b].insert(a);
            neighborSets[b].insert(c);
            neighborSets[c].insert(a);
            neighborSets[c].insert(b);
        }

        // 转为紧凑的 vector 存储
        m_vertexNeighbors.resize(vertexCount);
        for (std::size_t i{0}; i < vertexCount; ++i) {
            m_vertexNeighbors[i].assign(neighborSets[i].begin(), neighborSets[i].end());
        }

        // 扫描 6 轴极值顶点
        m_extremalVertices.fill(0);
        for (std::size_t i{0}; i < vertexCount; ++i) {
            const auto& v{m_dataMesh.vertices[i]};

            if (v.x() > m_dataMesh.vertices[m_extremalVertices[0]].x()) { m_extremalVertices[0] = i; }
            if (v.x() < m_dataMesh.vertices[m_extremalVertices[1]].x()) { m_extremalVertices[1] = i; }
            if (v.y() > m_dataMesh.vertices[m_extremalVertices[2]].y()) { m_extremalVertices[2] = i; }
            if (v.y() < m_dataMesh.vertices[m_extremalVertices[3]].y()) { m_extremalVertices[3] = i; }
            if (v.z() > m_dataMesh.vertices[m_extremalVertices[4]].z()) { m_extremalVertices[4] = i; }
            if (v.z() < m_dataMesh.vertices[m_extremalVertices[5]].z()) { m_extremalVertices[5] = i; }
        }
    }

    void ConvexMesh::offsetToCenter()
    {
        Eigen::Vector3d offset{};
        for (const auto& triangle : m_dataMesh.indices) {
            offset += calculateSingleVolume(triangle) * (
                m_dataMesh.vertices.at(triangle.x()) +
                m_dataMesh.vertices.at(triangle.y()) +
                m_dataMesh.vertices.at(triangle.z())
            );
        }
        offset /= 4 * getVolume();
        for (auto& point : m_dataMesh.vertices) {
            point -= offset;
        }
    }
} // egret
