//
// Created by jehor on 2026/6/6.
//

#ifndef EGRET_PHYSICS_GEOMETRY_UTILS_H
#define EGRET_PHYSICS_GEOMETRY_UTILS_H
#include <Eigen/Dense>
#include <vector>


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
        /** 顶点位置列表（世界空间坐标）。 */
        std::vector<Eigen::Vector3d> vertices{};

        /** 顶点法线列表（单位向量，每个顶点一条）。 */
        std::vector<Eigen::Vector3d> normals{};

        /** 三角形索引列表，每三个值构成一个三角形。 */
        std::vector<std::uint32_t> indices{};

        /**
         * @brief 检查网格是否有效（包含顶点和索引）。
         * @return 如果包含有效数据则返回 true。
         */
        [[nodiscard]] bool isValid() const noexcept
        {
            return !vertices.empty() && !indices.empty() &&
                indices.size() % 3 == 0;
        }

        /**
         * @brief 返回三角形数量。
         * @return 三角形数目。
         */
        [[nodiscard]] std::size_t triangleCount() const noexcept
        {
            return indices.size() / 3;
        }

        /**
         * @brief 返回顶点数量。
         * @return 顶点数目。
         */
        [[nodiscard]] std::size_t vertexCount() const noexcept
        {
            return vertices.size();
        }

        /** 清空所有数据。 */
        void clear() noexcept
        {
            vertices.clear();
            normals.clear();
            indices.clear();
        }
    };
}
#endif //EGRET_PHYSICS_GEOMETRY_UTILS_H
