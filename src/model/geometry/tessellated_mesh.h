//
// Created by jehor on 2026/6/6.
//

#ifndef EGRET_PHYSICS_GEOMETRY_UTILS_H
#define EGRET_PHYSICS_GEOMETRY_UTILS_H
#include <Eigen/Dense>
#include <functional>
#include <vector>

#include "aabb.h"


namespace egret
{
    /**
     * @brief       细分网格数据结构体。
     * @details     存储曲面离散化后的三角形网格数据，包括顶点位置、法线和三角形索引。
     *              由曲面类的 tessellate() 方法生成，供视图层渲染使用。
     *              顶点和法线为 double 精度（匹配物理引擎），
     *              视图层在填充 QQuick3DGeometry 时转换为 float。
     */
    struct TessellatedMesh
    {
        using IndexType = std::uint64_t;

        using LineIndexType = Eigen::Vector2<IndexType>;

        using TriangleIndexType = Eigen::Vector3<IndexType>;

        /** 顶点位置列表（世界空间坐标）。 */
        std::vector<Eigen::Vector3d> vertices{};

        /** 顶点法线列表（单位向量，每个顶点一条）。 */
        std::vector<Eigen::Vector3d> normals{};

        /** 三角形索引列表，每一个向量表达一个三角形的索引值。 */
        std::vector<TriangleIndexType> indices{};

        /**
         * @brief 检查网格是否有效（包含顶点和索引）。
         * @return 如果包含有效数据则返回 true。
         */
        [[nodiscard]] bool isValid() const noexcept;

        /**
         * @brief 返回三角形数量。
         * @return 三角形数目。
         */
        [[nodiscard]] std::size_t triangleCount() const noexcept;

        /**
         * @brief 返回顶点数量。
         * @return 顶点数目。
         */
        [[nodiscard]] std::size_t vertexCount() const noexcept;

        /** 清空所有数据。 */
        void clear() noexcept;

        [[nodiscard]] bool checkClosed() const;

        [[nodiscard]] bool checkManifold() const;

        [[nodiscard]] bool checkConvex() const;

        [[nodiscard]] AABB getAABB() const;

    private:
        /**
         * @brief 无向边的哈希函子，用于无序容器。
         * @details 对 (a,b) 和 (b,a) 产生相同哈希值。
         */
        struct EdgeHash
        {
            std::size_t operator()(const LineIndexType& v) const noexcept
            {
                auto a = v.x(), b = v.y();
                if (a > b) std::swap(a, b);
                return std::hash<IndexType>{}(a) ^ (std::hash<IndexType>{}(b) << 1);
            }
        };

        /**
         * @brief 无向边的相等比较函子。
         * @details (a,b) == (b,a) 视为相等。
         */
        struct EdgeEqual
        {
            bool operator()(const LineIndexType& a, const LineIndexType& b) const noexcept
            {
                return (a.x() == b.x() && a.y() == b.y()) ||
                       (a.x() == b.y() && a.y() == b.x());
            }
        };
    };
}
#endif //EGRET_PHYSICS_GEOMETRY_UTILS_H
