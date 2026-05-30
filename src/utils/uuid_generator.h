//
// Created by jehor on 2026/5/30.
//

#ifndef EGRET_PHYSICS_UUID_GENERATOR_H
#define EGRET_PHYSICS_UUID_GENERATOR_H

#include <cstdint>
#include <atomic>
#include <random>
#include <chrono>
#include <mutex>
#include <memory>

namespace egret
{
    class UuidGenerator
    {
    public:
        // 禁止拷贝和赋值
        UuidGenerator(const UuidGenerator&) = delete;
        UuidGenerator& operator=(const UuidGenerator&) = delete;

        // 获取单例实例
        static UuidGenerator& instance();

        // 生成UUID
        uint64_t generate();


        // 重置计数器（可选）
        void resetCounter();

        // 获取当前计数器值（可选）
        uint64_t getCounter() const;

    private:
        // 私有构造函数
        UuidGenerator();

        // 析构函数
        ~UuidGenerator() = default;

        uint64_t getMachineId();

        static uint64_t getProcessId();

        std::atomic<uint64_t> m_counter;
        uint64_t m_machineId;
        uint64_t m_processId;
        std::random_device m_rd;
        std::mt19937_64 m_gen;
        std::uniform_int_distribution<uint64_t> m_dist;
        std::mutex m_mutex;
    };
} // egret

#endif //EGRET_PHYSICS_UUID_GENERATOR_H
