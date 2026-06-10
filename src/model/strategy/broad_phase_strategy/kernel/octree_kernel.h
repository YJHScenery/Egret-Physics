/**
 * @file        octree_kernel.h
 * @brief       八叉树空间划分内核（模板）
 * @details     定义 OctreeNode 和 Octree 模板类，实现三维空间八叉树划分
 *              及自碰撞检测。与求解器适配层解耦，可作为独立组件使用。
 *              参照 bvh_kernel.h 的模板化模式设计。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-10
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Strategy
 */
#ifndef EGRET_PHYSICS_OCTREE_KERNEL_H
#define EGRET_PHYSICS_OCTREE_KERNEL_H

#include <array>
#include <concepts>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "physical_entity.h"
#include "physics_utils.h"

namespace egret
{
    /**
     * @brief       八叉树节点结构体（模板）。
     * @details     存储该节点的包围盒、8 个子节点索引及归属于该节点的实体索引列表。
     *              叶子节点的所有子节点索引均为 -1。
     * @tparam      IndexType 有符号整数类型，用于子节点索引。
     */
    template <std::signed_integral IndexType>
    struct OctreeNode
    {
        /** 该节点覆盖的空间包围盒。 */
        AABB bbox{};

        /** 8 个子节点索引，-1 表示不存在该子节点。 */
        std::array<IndexType, 8> children{-1, -1, -1, -1, -1, -1, -1, -1};

        /** 存储于该节点的实体在 m_entities / m_aabbs 中的索引。 */
        std::vector<IndexType> entityIndices{};

        /**
         * @brief 判断节点是否为叶子（无任何子节点）。
         * @return 所有子节点索引均为 -1 时返回 true。
         */
        [[nodiscard]] bool isLeaf() const
        {
            return std::ranges::all_of(children, [](const IndexType val) { return val == -1; });
        }
    };

    /**
     * @brief       八叉树空间划分容器（模板）。
     * @details     将三维空间递归划分为 8 个等大八分区域。
     *              每个实体插入到能完全包围其 AABB 的最深节点；
     *              跨越八分边界的实体保留在父节点。
     *              碰撞检测自顶向下遍历，测试同节点及跨子树的实体对。
     * @tparam      IndexType 有符号整数，用于节点 / 实体索引。
     * @tparam      SizeType  无符号整数，用于叶容量和深度限制。
     */
    template <std::signed_integral IndexType, std::unsigned_integral SizeType>
    class Octree
    {
    public:
        /** 默认构造，使用 8 叶容量 / 6 深度的默认值。 */
        Octree() = default;

        /**
         * @brief 带参构造。
         * @param leafSize 叶子节点最大实体数。
         * @param maxDepth 八叉树最大深度。
         */
        Octree(SizeType leafSize, SizeType maxDepth)
            : m_leafSize(leafSize)
              , m_maxDepth(maxDepth)
        {
        }

        /**
         * @brief 从实体指针列表构建八叉树。
         * @param entities 参与碰撞检测的物理实体指针列表（非拥有）。
         */
        void build(std::vector<PhysicalEntity*> entities);

        /**
         * @brief 查询所有潜在的碰撞实体对。
         * @return 无序实体索引对 (i, j)，满足 i < j 且 AABB 相交。
         */
        [[nodiscard]] std::vector<std::pair<IndexType, IndexType>> findSelfCollisions() const;

    private:
        /** 八叉树节点数组。 */
        std::vector<OctreeNode<IndexType>> m_nodes{};

        /** 实体指针缓存。 */
        std::vector<PhysicalEntity*> m_entities{};

        /** 实体 AABB 缓存，与 m_entities 一一对应。 */
        std::vector<AABB> m_aabbs{};

        /** 叶子节点最大实体数。 */
        SizeType m_leafSize{8};

        /** 八叉树最大深度。 */
        SizeType m_maxDepth{6};

        /** 根节点索引，-1 表示空树。 */
        IndexType m_rootIndex{-1};

        /**
         * @brief 递归构建八叉树子树。
         * @param entityIndices 当前节点的实体索引列表。
         * @param bbox 当前节点覆盖的空间包围盒（正方体）。
         * @param depth 当前递归深度。
         * @return 新创建节点的索引。
         */
        IndexType buildRecursive(std::span<IndexType> entityIndices,
                                 const AABB& bbox,
                                 SizeType depth);

        /**
         * @brief 递归检测单个节点内的自碰撞。
         * @param nodeIndex 目标节点索引。
         * @param outPairs  输出的候选碰撞索引对。
         */
        void traverseSelfCollision(IndexType nodeIndex,
                                   std::vector<std::pair<IndexType, IndexType>>& outPairs) const;

        /**
         * @brief 检测单个实体与目标子树中所有实体的碰撞。
         * @param entityIdx 实体索引。
         * @param nodeIndex 目标节点索引。
         * @param outPairs  输出的候选碰撞索引对。
         */
        void traverseEntityVsNode(IndexType entityIdx,
                                  IndexType nodeIndex,
                                  std::vector<std::pair<IndexType, IndexType>>& outPairs) const;

        /**
         * @brief 检测两个不同子树之间的碰撞。
         * @param nodeAIndex 子树 A 的根节点索引。
         * @param nodeBIndex 子树 B 的根节点索引。
         * @param outPairs   输出的候选碰撞索引对。
         */
        void traverseNodeVsNode(IndexType nodeAIndex,
                                IndexType nodeBIndex,
                                std::vector<std::pair<IndexType, IndexType>>& outPairs) const;

        /**
         * @brief 添加无序碰撞对 (a, b)。
         * @details 确保 a < b 后再添加。
         */
        static void emitPair(IndexType a, IndexType b,
                             std::vector<std::pair<IndexType, IndexType>>& outPairs)
        {
            if (a > b) std::swap(a, b);
            outPairs.emplace_back(a, b);
        }
    };

    // 模板实现

    template <std::signed_integral IndexType, std::unsigned_integral SizeType>
    void Octree<IndexType, SizeType>::build(std::vector<PhysicalEntity*> entities)
    {
        m_entities = std::move(entities);
        m_nodes.clear();
        m_aabbs.clear();
        m_aabbs.reserve(m_entities.size());

        // ---- 第一步：收集有效实体的索引并计算场景 AABB ----
        std::vector<IndexType> validIndices{};
        validIndices.reserve(m_entities.size());

        AABB sceneAABB{};
        bool sceneAABBInitialized = false;

        for (IndexType i = 0; i < static_cast<IndexType>(m_entities.size()); ++i) {
            PhysicalEntity* entity = m_entities[i];

            if (entity == nullptr) {
                LOG_FATAL(std::format("Octree build: null entity at index {}", i));
                m_aabbs.push_back(AABB{});
                continue;
            }

            const auto& shape = entity->getShape();
            if (shape == nullptr) {
                LOG_FATAL(std::format("Octree build: null shape at index {}", i));
                m_aabbs.push_back(AABB{});
                continue;
            }

            const AABB aabb = shape->getAABB(entity->getTransformCR());
            m_aabbs.push_back(aabb);
            validIndices.push_back(i);

            if (!sceneAABBInitialized) {
                sceneAABB = aabb;
                sceneAABBInitialized = true;
            }
            else {
                sceneAABB.expand(aabb);
            }
        }

        if (validIndices.empty()) {
            m_rootIndex = -1;
            return;
        }

        // ---- 第二步：将场景 AABB 扩展为正方体（八叉树要求各轴等长） ----
        const Eigen::Vector3d extents = sceneAABB.max - sceneAABB.min;
        const double maxExtent = extents.maxCoeff();

        // 微小膨胀，避免实体恰好位于分割面上
        constexpr double kEpsilon = 1e-6;
        const double halfExpanded = maxExtent * 0.5 + kEpsilon;
        const Eigen::Vector3d center = (sceneAABB.min + sceneAABB.max) * 0.5;
        const AABB cubicAABB{
            center - Eigen::Vector3d::Constant(halfExpanded),
            center + Eigen::Vector3d::Constant(halfExpanded)
        };

        // ---- 第三步：递归构建 ----
        m_rootIndex = buildRecursive(validIndices, cubicAABB, 0);
    }

    template <std::signed_integral IndexType, std::unsigned_integral SizeType>
    std::vector<std::pair<IndexType, IndexType>>
    Octree<IndexType, SizeType>::findSelfCollisions() const
    {
        if (m_rootIndex < 0) return {};

        std::vector<std::pair<IndexType, IndexType>> result{};
        traverseSelfCollision(m_rootIndex, result);
        return result;
    }

    // 递归构建

    template <std::signed_integral IndexType, std::unsigned_integral SizeType>
    IndexType Octree<IndexType, SizeType>::buildRecursive(
        std::span<IndexType> entityIndices,
        const AABB& bbox,
        const SizeType depth)
    {
        OctreeNode<IndexType> node{};
        node.bbox = bbox;

        // 终止条件
        if (entityIndices.size() <= m_leafSize || depth >= m_maxDepth) {
            node.entityIndices.assign(entityIndices.begin(), entityIndices.end());
            m_nodes.push_back(std::move(node));
            return static_cast<IndexType>(m_nodes.size() - 1);
        }

        // ---- 计算 8 个八分区 ----
        const Eigen::Vector3d c = (bbox.min + bbox.max) * 0.5;

        const std::array<AABB, 8> octants{
            {
                // 0: (-x, -y, -z)
                AABB{bbox.min, c},
                // 1: (+x, -y, -z)
                AABB{
                    Eigen::Vector3d{c.x(), bbox.min.y(), bbox.min.z()},
                    Eigen::Vector3d{bbox.max.x(), c.y(), c.z()}
                },
                // 2: (-x, +y, -z)
                AABB{
                    Eigen::Vector3d{bbox.min.x(), c.y(), bbox.min.z()},
                    Eigen::Vector3d{c.x(), bbox.max.y(), c.z()}
                },
                // 3: (+x, +y, -z)
                AABB{
                    Eigen::Vector3d{c.x(), c.y(), bbox.min.z()},
                    Eigen::Vector3d{bbox.max.x(), bbox.max.y(), c.z()}
                },
                // 4: (-x, -y, +z)
                AABB{
                    Eigen::Vector3d{bbox.min.x(), bbox.min.y(), c.z()},
                    Eigen::Vector3d{c.x(), c.y(), bbox.max.z()}
                },
                // 5: (+x, -y, +z)
                AABB{
                    Eigen::Vector3d{c.x(), bbox.min.y(), c.z()},
                    Eigen::Vector3d{bbox.max.x(), c.y(), bbox.max.z()}
                },
                // 6: (-x, +y, +z)
                AABB{
                    Eigen::Vector3d{bbox.min.x(), c.y(), c.z()},
                    Eigen::Vector3d{c.x(), bbox.max.y(), bbox.max.z()}
                },
                // 7: (+x, +y, +z)
                AABB{c, bbox.max},
            }
        };

        // ---- 将实体分配到能完全包围其 AABB 的最深八分区 ----
        std::array<std::vector<IndexType>, 8> childEntities{};
        std::vector<IndexType> remaining{};

        for (const IndexType entityIdx : entityIndices) {
            const AABB& entityAABB = m_aabbs[entityIdx];
            bool placed = false;

            for (std::uint8_t oct = 0; oct < 8; ++oct) {
                const AABB& octAABB = octants[oct];
                if (octAABB.min.x() <= entityAABB.min.x() &&
                    octAABB.max.x() >= entityAABB.max.x() &&
                    octAABB.min.y() <= entityAABB.min.y() &&
                    octAABB.max.y() >= entityAABB.max.y() &&
                    octAABB.min.z() <= entityAABB.min.z() &&
                    octAABB.max.z() >= entityAABB.max.z()) {
                    childEntities[oct].push_back(entityIdx);
                    placed = true;
                    break;
                }
            }

            if (!placed) {
                remaining.push_back(entityIdx);
            }
        }

        node.entityIndices = std::move(remaining);

        // ---- 递归构建非空子节点 ----
        for (std::uint8_t oct = 0; oct < 8; ++oct) {
            if (childEntities[oct].empty()) {
                node.children[oct] = -1;
            }
            else {
                node.children[oct] = buildRecursive(childEntities[oct], octants[oct], depth + 1);
            }
        }

        m_nodes.push_back(std::move(node));
        return static_cast<IndexType>(m_nodes.size() - 1);
    }

    // 碰撞遍历

    template <std::signed_integral IndexType, std::unsigned_integral SizeType>
    void Octree<IndexType, SizeType>::traverseSelfCollision(
        const IndexType nodeIndex,
        std::vector<std::pair<IndexType, IndexType>>& outPairs) const
    {
        if (nodeIndex < 0) return;

        const OctreeNode<IndexType>& node = m_nodes[nodeIndex];

        // ---- 1. 同节点内实体的自碰撞 ----
        const auto& indices = node.entityIndices;
        for (std::size_t i = 0; i < indices.size(); ++i) {
            const IndexType idxA = indices[i];
            for (std::size_t j = i + 1; j < indices.size(); ++j) {
                const IndexType idxB = indices[j];
                if (m_aabbs[idxA].intersects(m_aabbs[idxB])) {
                    emitPair(idxA, idxB, outPairs);
                }
            }
        }

        // 叶子节点无需进一步处理
        if (node.isLeaf()) return;

        // ---- 2. 节点内实体与各子节点内实体的碰撞 ----
        for (const IndexType entityIdx : node.entityIndices) {
            for (const IndexType childIdx : node.children) {
                if (childIdx >= 0) {
                    traverseEntityVsNode(entityIdx, childIdx, outPairs);
                }
            }
        }

        // ---- 3. 递归处理每个子节点的自碰撞 ----
        for (const IndexType childIdx : node.children) {
            if (childIdx >= 0) {
                traverseSelfCollision(childIdx, outPairs);
            }
        }

        // ---- 4. 不同子节点之间的互碰撞 ----
        for (std::uint8_t a = 0; a < 8; ++a) {
            if (node.children[a] < 0) continue;
            for (std::uint8_t b = a + 1; b < 8; ++b) {
                if (node.children[b] < 0) continue;
                traverseNodeVsNode(node.children[a], node.children[b], outPairs);
            }
        }
    }

    template <std::signed_integral IndexType, std::unsigned_integral SizeType>
    void Octree<IndexType, SizeType>::traverseEntityVsNode(
        const IndexType entityIdx,
        const IndexType nodeIndex,
        std::vector<std::pair<IndexType, IndexType>>& outPairs) const
    {
        if (nodeIndex < 0) return;

        const OctreeNode<IndexType>& node = m_nodes[nodeIndex];

        // AABB 快速剔除
        if (!m_aabbs[entityIdx].intersects(node.bbox)) return;

        for (const IndexType otherIdx : node.entityIndices) {
            if (entityIdx == otherIdx) continue;
            if (m_aabbs[entityIdx].intersects(m_aabbs[otherIdx])) {
                emitPair(entityIdx, otherIdx, outPairs);
            }
        }

        if (!node.isLeaf()) {
            for (const IndexType childIdx : node.children) {
                if (childIdx >= 0) {
                    traverseEntityVsNode(entityIdx, childIdx, outPairs);
                }
            }
        }
    }

    template <std::signed_integral IndexType, std::unsigned_integral SizeType>
    void Octree<IndexType, SizeType>::traverseNodeVsNode(
        const IndexType nodeAIndex,
        const IndexType nodeBIndex,
        std::vector<std::pair<IndexType, IndexType>>& outPairs) const
    {
        if (nodeAIndex < 0 || nodeBIndex < 0) return;

        const OctreeNode<IndexType>& nodeA = m_nodes[nodeAIndex];
        const OctreeNode<IndexType>& nodeB = m_nodes[nodeBIndex];

        // AABB 快速剔除
        if (!nodeA.bbox.intersects(nodeB.bbox)) return;

        // ---- 两个都是叶子 ----
        if (nodeA.isLeaf() && nodeB.isLeaf()) {
            for (const IndexType idxA : nodeA.entityIndices) {
                for (const IndexType idxB : nodeB.entityIndices) {
                    if (idxA == idxB) continue;
                    if (m_aabbs[idxA].intersects(m_aabbs[idxB])) {
                        emitPair(idxA, idxB, outPairs);
                    }
                }
            }
            return;
        }

        // ---- A 是叶子，B 是内部节点 ----
        if (nodeA.isLeaf()) {
            for (const IndexType childB : nodeB.children) {
                if (childB >= 0) {
                    traverseNodeVsNode(nodeAIndex, childB, outPairs);
                }
            }
            return;
        }

        // ---- B 是叶子，A 是内部节点 ----
        if (nodeB.isLeaf()) {
            for (const IndexType childA : nodeA.children) {
                if (childA >= 0) {
                    traverseNodeVsNode(childA, nodeBIndex, outPairs);
                }
            }
            return;
        }

        // ---- 两个都是内部节点，递归所有子节点对 ----
        for (const IndexType childA : nodeA.children) {
            if (childA < 0) continue;
            for (const IndexType childB : nodeB.children) {
                if (childB < 0) continue;
                traverseNodeVsNode(childA, childB, outPairs);
            }
        }
    }
} // egret

#endif // EGRET_PHYSICS_OCTREE_KERNEL_H
