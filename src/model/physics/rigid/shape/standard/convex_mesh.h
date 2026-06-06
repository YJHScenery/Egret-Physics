/**
 * @file        convex_mesh.h
 * @brief       闭合凸体 Mesh 形状头文件，定义任意凸体刚体的碰撞形状。
 * @details     定义 ConvexMesh 类，继承自 ShapeBase，表示通过 TessellatedMesh 构建的凸体网格。
 *              构造时执行封闭性检测与凸体检测，失败则标记为 invalid。
 *              所有几何查询（支撑点、AABB、转动惯量）均通过穷举顶点计算。
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
#include "geometry_utils.h"

namespace egret
{
    /**
     * @brief       闭合凸体 Mesh 形状，表示任意凸体刚体的碰撞形状。
     * @details     ConvexMesh 继承自 ShapeBase，表示一个通过 TessellatedMesh 所构建的凸体网格。
     *              用于物理碰撞检测和转动惯量计算。
     *              此形状不具有良好的解析性质，其碰撞检测、转动惯量矩阵、AABB 盒子都必须通过穷举计算。
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
        /**
         * @brief       从 TessellatedMesh 构造凸体形状。
         * @details     拷贝网格数据后依次执行：顶点/索引有效性检查 → 退化三角形检测 →
         *              面法线计算与定向 → 封闭性检测 → 凸体检测。
         *              通过后计算体积与质心，将顶点平移使质心归零，
         *              最后计算单位密度转动惯量矩阵与局部 AABB。
         *              任一检测失败则调用 setValid(false)。
         * @param mesh 输入的细分网格数据。
         */
        explicit ConvexMesh(const TessellatedMesh &mesh);

        // ---- ShapeBase 接口 ----
        /** @brief 返回 ShapeType::ConvexMesh。 */
        [[nodiscard]] ShapeType getType() const override;

        /** @brief 返回闭合凸体的体积。 */
        [[nodiscard]] double getVolume() const override;

        /** @brief 返回质心位置，恒为零向量（构造时已归零）。 */
        [[nodiscard]] Eigen::Vector3d getCenterOfMass() const override;

        /** @brief 计算给定质量下的转动惯量矩阵（关于质心）。 */
        [[nodiscard]] Eigen::Matrix3d getInertiaTensor(double mass) const override;

        /** @brief 穷举计算世界空间下的 AABB 包围盒。 */
        [[nodiscard]] AABB getAABB(const Transform &transform) const override;

        /** @brief 返回序列化信息，包含顶点和索引数据。 */
        [[nodiscard]] ShapeLoadInfo getLoadInfo() const override;

        /**
         * @brief       GJK 支撑函数：返回在给定方向上最远的顶点（世界坐标）。
         * @details     将世界方向变换到局部空间，穷举搜索 dot 最大的顶点，
         *              再通过 Transform 变换回世界坐标。
         * @param direction 世界空间中的搜索方向（不必归一化）。
         * @param transform 从局部到世界的刚体变换。
         * @return 世界空间中最远的支撑点。
         */
        [[nodiscard]] Eigen::Vector3d support(const Eigen::Vector3d &direction,
                                               const Transform &transform) const override;

        // ---- 访问器 ----

        /** @brief 返回局部空间顶点列表（质心位于原点）。 */
        [[nodiscard]] const std::vector<Eigen::Vector3d> &getVertices() const { return m_vertices; }

        /** @brief 返回三角形索引列表。 */
        [[nodiscard]] const std::vector<std::array<std::uint32_t, 3>> &getTriangles() const { return m_triangles; }

        /** @brief 返回顶点数量。 */
        [[nodiscard]] std::size_t getVertexCount() const { return m_vertices.size(); }

        /** @brief 返回三角形数量。 */
        [[nodiscard]] std::size_t getTriangleCount() const { return m_triangles.size(); }

    private:
        /**
         * @brief       封闭性检测：验证网格是否为闭合流形。
         * @details     构建边映射表，检查每条边是否恰好出现在两个三角形中。
         *              若存在只出现一次的边界边，则网格不封闭。
         * @return 若网格封闭则返回 true，否则返回 false。
         */
        [[nodiscard]] bool checkClosedness() const;

        /**
         * @brief       凸体检测：验证网格是否构成凸多面体。
         * @details     对每个面，检查所有非该面顶点的其余顶点是否均位于
         *              该面平面的同一侧（非正半空间）。
         *              此检测依赖面法线已定向为统一向外。
         * @return 若网格为凸体则返回 true，否则返回 false。
         */
        [[nodiscard]] bool checkConvexity() const;

        /**
         * @brief       计算体积与质心，并将所有顶点平移使质心归零。
         * @details     使用四面体分解法（以原点为公共顶点）：
         *              - 有向体积 V_i = (1/6) * v0 · (v1 × v2)
         *              - 质心贡献 C_i = (v0 + v1 + v2) / 4
         *              总体积 V = Σ V_i，质心 = (Σ V_i * C_i) / V。
         *              然后将所有顶点减去质心向量，使 m_centerOfMass = (0,0,0)。
         */
        void computeMassProperties();

        /**
         * @brief       计算单位密度的转动惯量矩阵（关于当前原点即质心）。
         * @details     使用四面体分解法计算 ∫_V r⊗r dV 矩阵 M，
         *              则 I_per_unit_volume = tr(M)·I₃ - M。
         *              调用此函数前必须先调用 computeMassProperties() 确保质心已归零。
         */
        void computeInertiaTensorPerUnitDensity();

        // ---- 数据成员 ----

        /** 局部空间顶点（质心位于原点）。 */
        std::vector<Eigen::Vector3d> m_vertices;

        /** 每个面的单位外法线，与 m_triangles 一一对应。 */
        std::vector<Eigen::Vector3d> m_faceNormals;

        /** 三角形索引数组，每个元素为三个顶点索引。 */
        std::vector<std::array<std::uint32_t, 3>> m_triangles;

        /** 闭合凸体总体积。 */
        double m_volume = 0.0;

        /** 单位密度的转动惯量矩阵（关于质心），对称 3×3。 */
        Eigen::Matrix3d m_inertiaPerUnitDensity = Eigen::Matrix3d::Zero();

        /** 局部空间 AABB 最小点（用于 getLoadInfo 序列化恢复）。 */
        Eigen::Vector3d m_localAABBMin = Eigen::Vector3d::Zero();

        /** 局部空间 AABB 最大点。 */
        Eigen::Vector3d m_localAABBMax = Eigen::Vector3d::Zero();
    };
} // egret

#endif //EGRET_PHYSICS_CONVEX_MESH_H
