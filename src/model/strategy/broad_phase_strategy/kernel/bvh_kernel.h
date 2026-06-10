//
// Created by jehor on 2026/6/9.
//

#ifndef EGRET_PHYSICS_BVH_H
#define EGRET_PHYSICS_BVH_H
#include <concepts>
#include "physical_entity.h"
#include "physics_utils.h"

namespace egret
{
    template <std::signed_integral IndexType>
    struct BVHNode
    {
        AABB bbox;
        IndexType leftChild{};
        IndexType rightChild{};

        std::vector<IndexType> entityIndices{};

        [[nodiscard]] bool isLeaf() const;
    };

    template <std::signed_integral IndexType>
    bool BVHNode<IndexType>::isLeaf() const
    {
        return leftChild == -1 && rightChild == -1;
    }


    template <std::signed_integral IndexType, std::unsigned_integral SizeType>
    class BVH
    {
    public:
        BVH() = default;

        BVH(SizeType leafSize, SizeType maxDepth) : m_leafSize{leafSize}, m_maxDepth{maxDepth}
        {
        }

        void build(std::vector<PhysicalEntity*> entities);

        std::vector<std::pair<IndexType, IndexType>> findSelfCollisions() const;

    private:
        std::vector<BVHNode<IndexType>> nodes{};
        std::vector<PhysicalEntity*> m_entities{};

        SizeType m_leafSize{8};
        SizeType m_maxDepth{16};

        IndexType m_rootIndex{};

        IndexType buildRecursive(std::span<IndexType> entityIndices, IndexType depth);

        void traverseCollision(IndexType nodeAIndex, IndexType nodeBIndex,
                               std::vector<std::pair<IndexType, IndexType>>& collisions) const;
    };

    template <std::signed_integral IndexType, std::unsigned_integral SizeType>
    void BVH<IndexType, SizeType>::build(std::vector<PhysicalEntity*> entities)
    {
        m_entities = std::move(entities);
        nodes.clear();
        std::vector<IndexType> indices(m_entities.size());
        std::iota(indices.begin(), indices.end(), 0);
        m_rootIndex = buildRecursive(indices, 0);
    }


    template <std::signed_integral IndexType, std::unsigned_integral SizeType>
    std::vector<std::pair<IndexType, IndexType>> BVH<IndexType, SizeType>::findSelfCollisions() const
    {
        if (nodes.empty()) return {};
        std::vector<std::pair<IndexType, IndexType>> result;
        traverseCollision(m_rootIndex, m_rootIndex, result);

        return result;
    }

    template <std::signed_integral IndexType, std::unsigned_integral SizeType>
    IndexType BVH<IndexType, SizeType>::buildRecursive(std::span<IndexType> entityIndices, IndexType depth)
    {
        BVHNode<IndexType> node{};

        PhysicalEntity* pEntity0{m_entities[entityIndices[0]]};

        if (pEntity0 == nullptr) {
            LOG_FATAL(std::format("Find Null Pointer: {}", reinterpret_cast<std::uint64_t>(pEntity0)));
            return -1;
        }

        AABB combined{pEntity0->getShape()->getAABB(pEntity0->getTransformCR())};

        for (IndexType index : entityIndices.subspan(1)) {
            PhysicalEntity* entityPtr{m_entities[index]};
            if (entityPtr == nullptr) {
                LOG_FATAL(std::format("Find Null Pointer: {}", reinterpret_cast<std::uint64_t>(entityPtr)));
                return -1;
            }
            combined.expand(entityPtr->getShape()->getAABB(entityPtr->getTransformCR()));
        }
        node.bbox = combined;

        if (entityIndices.size() <= m_leafSize || depth > m_maxDepth) {
            node.leftChild = node.rightChild = -1;
            node.entityIndices.assign(entityIndices.begin(), entityIndices.end());
            nodes.push_back(node);
            return static_cast<IndexType>(nodes.size() - 1);
        }

        Eigen::Vector3d extents{combined.max - combined.min,};

        std::uint8_t axis{0};
        if (extents.y() > extents.x()) {
            axis = 1;
        }
        if (extents.z() > extents[axis]) {
            axis = 2;
        }

        std::ranges::sort(entityIndices, [&, axis](IndexType a, IndexType b)
        {
            const AABB a_AABB = m_entities[a]->getShape()->getAABB(m_entities[a]->getTransformCR());
            const AABB b_AABB = m_entities[b]->getShape()->getAABB(m_entities[b]->getTransformCR());

            const double centerA = (a_AABB.max[axis] + a_AABB.min[axis]) / 2.0;
            const double centerB = (b_AABB.max[axis] + b_AABB.min[axis]) / 2.0;

            return centerA < centerB;
        });

        SizeType mid = entityIndices.size() / 2;
        std::span<IndexType> leftSpan = entityIndices.subspan(0, mid);
        std::span<IndexType> rightSpan = entityIndices.subspan(mid);

        IndexType leftIndex = buildRecursive(leftSpan, depth + 1);
        IndexType rightIndex = buildRecursive(rightSpan, depth + 1);

        node.leftChild = leftIndex;
        node.rightChild = rightIndex;

        nodes.push_back(node);
        return static_cast<IndexType>(nodes.size() - 1);
    }


    template <std::signed_integral IndexType, std::unsigned_integral SizeType>
    void BVH<IndexType, SizeType>::traverseCollision(
        IndexType nodeAIndex, IndexType nodeBIndex,
        std::vector<std::pair<IndexType, IndexType>>& collisions) const
    {
        const BVHNode<IndexType>& nodeA = nodes[nodeAIndex];
        const BVHNode<IndexType>& nodeB = nodes[nodeBIndex];

        if (!nodeA.bbox.intersects(nodeB.bbox))
            return;

        // 情况1：两个都是叶子节点
        if (nodeA.isLeaf() && nodeB.isLeaf()) {
            if (nodeAIndex == nodeBIndex) {
                // 同一个叶子：只添加无序对 (i, j) 且 i < j
                const auto& indices = nodeA.entityIndices;
                for (size_t i = 0; i < indices.size(); ++i) {
                    for (size_t j = i + 1; j < indices.size(); ++j) {
                        IndexType a = indices[i];
                        IndexType b = indices[j];
                        // 确保 a < b（如果原始索引顺序不保证，可加swap，但这里i<j已保证索引不同）
                        if (a > b) std::swap(a, b);
                        collisions.emplace_back(a, b);
                    }
                }
            }
            else {
                // 不同叶子：所有组合，保证 aIdx < bIdx
                for (IndexType aIdx : nodeA.entityIndices) {
                    for (IndexType bIdx : nodeB.entityIndices) {
                        if (aIdx == bIdx) continue; // 同一个实体不可能在两个不同叶子，但防御
                        if (aIdx > bIdx) std::swap(aIdx, bIdx);
                        collisions.emplace_back(aIdx, bIdx);
                    }
                }
            }
            return;
        }

        // 情况2：至少有一个是内部节点
        // 关键：当 nodeAIndex == nodeBIndex 时，避免对称递归
        if (nodeAIndex == nodeBIndex) {
            // 自碰撞：只递归 (left,left), (left,right), (right,right)
            traverseCollision(nodeA.leftChild, nodeA.leftChild, collisions);
            traverseCollision(nodeA.leftChild, nodeA.rightChild, collisions);
            traverseCollision(nodeA.rightChild, nodeA.rightChild, collisions);
            return;
        }

        // 情况3：不同节点的内部/叶子混合递归
        if (nodeA.isLeaf()) {
            traverseCollision(nodeAIndex, nodeB.leftChild, collisions);
            traverseCollision(nodeAIndex, nodeB.rightChild, collisions);
        }
        else if (nodeB.isLeaf()) {
            traverseCollision(nodeA.leftChild, nodeBIndex, collisions);
            traverseCollision(nodeA.rightChild, nodeBIndex, collisions);
        }
        else {
            // 两个都是内部节点且不同
            traverseCollision(nodeA.leftChild, nodeB.leftChild, collisions);
            traverseCollision(nodeA.leftChild, nodeB.rightChild, collisions);
            traverseCollision(nodeA.rightChild, nodeB.leftChild, collisions);
            traverseCollision(nodeA.rightChild, nodeB.rightChild, collisions);
        }
    }
}


#endif //EGRET_PHYSICS_BVH_H
