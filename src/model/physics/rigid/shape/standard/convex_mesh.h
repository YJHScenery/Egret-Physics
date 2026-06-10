/**
 * @file        convex_mesh.h
 * @brief       闭合凸体 Mesh 形状头文件，定义任意凸体刚体的碰撞形状。
 * @details     定义 ConvexMesh 类，继承自 ShapeBase，表示通过 TessellatedMesh 构建的凸体网格。
 *              构造时执行封闭性检测与凸体检测，失败则标记为 invalid。
 *              支撑点查询通过爬山法在顶点邻接图上加速，AABB 与转动惯量通过穷举计算。
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

#ifndef EGRET_PHYSICS_CONVEX_MESH_H
#define EGRET_PHYSICS_CONVEX_MESH_H
#include <array>
#include <cstdint>

#include "shape_base.h"
#include "tessellated_mesh.h"

namespace egret
{
    /**
     * @brief       闭合凸体 Mesh 形状，表示任意凸体刚体的碰撞形状。
     * @details     ConvexMesh 继承自 ShapeBase，表示一个通过 TessellatedMesh 所构建的凸体网格。
     *              用于物理碰撞检测和转动惯量计算。
     *              此形状不具有良好的解析性质，其转动惯量矩阵、AABB 盒子通过穷举计算。
     *              支撑点查询通过爬山法在预构建的顶点邻接图上加速，期望 O(√n) 时间。
     *              构造时，会对模型执行封闭与凸体检查，如果不符合要求则标记为 invalid。
     *
     * @invariant   getType() 返回 ShapeType::ConvexMesh
     * @invariant   质心通过四面体积分获得并强制推送至坐标系原点 (0, 0, 0)
     * @invariant   m_vertices 为局部空间（质心位于原点）的顶点坐标
     * @remark      形状的位置信息交由 Transform 管理
     * @see         ShapeBase, Transform, AABB, TessellatedMesh
     */
    class ConvexMesh : public ShapeBase
    {
    public:
        explicit ConvexMesh(TessellatedMesh tessellatedMesh);

        [[nodiscard]] ShapeType getType() const override;

        [[nodiscard]] double getVolume() const override;

        [[nodiscard]] Eigen::Vector3d getCenterOfMass() const override;

        [[nodiscard]] Eigen::Matrix3d getInertiaTensor(double mass) const override;

        [[nodiscard]] AABB getAABB(const Transform& transform) const override;

        [[nodiscard]] Eigen::Vector3d support(const Eigen::Vector3d& direction,
                                              const Transform& transform) const override;

        // 形状自描述的加载信息，默认仅提供 typeId。
        [[nodiscard]] ShapeLoadInfo getLoadInfo() const override;

        /**
         * @brief 获取内部细分网格数据的常量引用。
         * @details 返回已平移到质心原点的顶点数据，供视图层构建渲染几何体使用。
         * @return 细分网格数据的常量引用。
         */
        [[nodiscard]] const TessellatedMesh& getTessellatedMesh() const { return m_dataMesh; }

    private:
        TessellatedMesh m_dataMesh{};

        /// 顶点邻接表，用于支撑函数爬山法加速查询
        std::vector<std::vector<TessellatedMesh::IndexType>> m_vertexNeighbors;

        /// 6 轴极值顶点索引（顺序：+x, -x, +y, -y, +z, -z），用于爬山法初始猜测
        std::array<TessellatedMesh::IndexType, 6> m_extremalVertices{};

        /// 上次支撑点查询的顶点索引，用于爬山法热启动
        mutable TessellatedMesh::IndexType m_supportHint{0};

        mutable double m_volumeCache{0.0};

        mutable Eigen::Matrix3d m_inertiaTensorCache{Eigen::Matrix3d::Zero()};

        mutable AABB m_aabbCache{AABB()};

        double calculateVolume() const;

        Eigen::Matrix3d calculateInertiaTensor() const;

        void offsetToCenter();

        /**
         * @brief 从三角形索引构建顶点邻接表，同时扫描 6 轴极值顶点。
         * @details 遍历所有三角形，提取无向边并去重，存入 m_vertexNeighbors。
         *          同时记录各轴向最大/最小坐标的顶点索引存入 m_extremalVertices。
         */
        void buildVertexAdjacency();

        const std::function<double(const TessellatedMesh::TriangleIndexType& triangle)> calculateSingleVolume{
            [this](const TessellatedMesh::TriangleIndexType& triangle)
            {
                return 1.0 / 6.0 * m_dataMesh.vertices.at(triangle.x()).dot(
                m_dataMesh.vertices.at(triangle.y()).cross(
                    m_dataMesh.vertices.at(triangle.z())
                ));
            }
        };
    };
} // egret

#endif //EGRET_PHYSICS_CONVEX_MESH_H
