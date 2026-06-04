//
// Created by jehor on 2026/6/4.
//

#include "multi_process_brute_force_broad_phase.h"
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/use_future.hpp>

#include "physical_entity.h"


namespace egret
{
    void MultiProcessBruteForceBroadPhaseStrategy::buildCandidatePairs(const SolverSceneSnapshotBase &scene,
        const SolverConfig &config, std::vector<SolverBodyPair> &outPairs, SolverStats &stats) const
    {
         std::ignore = config;
        const auto bodies = scene.getBodies();
        outPairs.clear();

        // ---------- 1. 预处理：筛选有效物体，预提取 AABB ----------
        struct ValidBody {
            std::size_t originalIndex;   // 原始 bodies 中的索引
            AABB aabb;
        };

        std::vector<ValidBody> validBodies;
        validBodies.reserve(bodies.size());

        for (std::size_t i = 0; i < bodies.size(); ++i) {
            const SolverBodyHandle& body = bodies[i];
            if (!body.enableCollision || body.entity->getShape() == nullptr)
                continue;
            if (!body.entity->getTransformCR().getTranslation().allFinite())
                continue;

            const AABB aabb = body.entity->getShape()->getAABB(body.entity->getTransformCR());
            if (!aabb.min.allFinite() || !aabb.max.allFinite())
                continue;

            validBodies.push_back({i, aabb});
        }

        const std::size_t n = validBodies.size();
        if (n == 0) {
            stats.broadPhasePairCount = 0;
            return;
        }

        // ---------- 2. 准备并行处理 ----------
        const unsigned int numThreads = (concurrency == 0) ? 4 : concurrency;

        // 每个线程独立的结果容器
        std::vector<std::vector<SolverBodyPair>> threadResults(numThreads);

        // 动态任务分配：原子索引指向下一个待处理的 validBodies 下标
        std::atomic<std::size_t> nextIndex{0};

        // 提交任务：每个线程不断获取下一个 i，处理其与后续所有 j 的碰撞
        std::vector<std::future<void>> futures;
        futures.reserve(numThreads);

        for (unsigned int t = 0; t < numThreads; ++t) {
            futures.push_back(boost::asio::post(threadPool, boost::asio::use_future([&, t]() {
                auto& localPairs = threadResults[t];
                // 预分配一定容量，减少动态扩容
                localPairs.reserve(1024);

                while (true) {
                    std::size_t i = nextIndex.fetch_add(1);
                    if (i >= n) break;

                    const auto& bodyA = validBodies[i];
                    const AABB& aabbA = bodyA.aabb;
                    const std::size_t origI = bodyA.originalIndex;

                    // 内层循环：与所有 j > i 的物体比较
                    for (std::size_t j = i + 1; j < n; ++j) {
                        const auto& bodyB = validBodies[j];
                        const AABB& aabbB = bodyB.aabb;

                        // AABB 重叠测试
                        if (aabbA.max.x() >= aabbB.min.x() && aabbB.max.x() >= aabbA.min.x() &&
                            aabbA.max.y() >= aabbB.min.y() && aabbB.max.y() >= aabbA.min.y() &&
                            aabbA.max.z() >= aabbB.min.z() && aabbB.max.z() >= aabbA.min.z()) {
                            localPairs.emplace_back(origI, bodyB.originalIndex);
                        }
                    }
                }
            })));
        }

        // 等待所有任务完成
        for (auto& fut : futures) {
            fut.get();
        }

        // ---------- 3. 合并各线程结果 ----------
        std::size_t totalPairs = 0;
        for (const auto& vec : threadResults) {
            totalPairs += vec.size();
        }
        outPairs.reserve(totalPairs);

        for (const auto& vec : threadResults) {
            outPairs.insert(outPairs.end(), vec.begin(), vec.end());
        }

        stats.broadPhasePairCount = outPairs.size();
    }
} // egret