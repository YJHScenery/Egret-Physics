//
// Created by jehor on 2026/5/30.
//

#include "unique_id_generator.h"

#include "os/windows/windows_config.h"

namespace egret
{
    UniqueIDGenerator& UniqueIDGenerator::instance()
    {
        static UniqueIDGenerator instance;
        return instance;
    }

    uint64_t UniqueIDGenerator::generate()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // 时间戳部分（38位）- 毫秒级精度，可持续约8.7年
        const std::int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        const uint64_t timestamp = now & 0x3FFFFFFFFFULL; // 取低38位

        // 机器ID（10位，0-1023）
        const uint64_t machine = m_machineId & 0x3FF;

        // 进程ID（10位，0-1023）
        const uint64_t process = m_processId & 0x3FF;

        // 计数器（16位，0-65535）
        const uint64_t counter = (++m_counter) & 0xFFFF;

        // 组合成64位ID
        // 格式: [38位时间戳][10位机器ID][10位进程ID][6位保留][16位计数器]
        const uint64_t uniqueId = (timestamp << 26) | (machine << 16) | (process << 6) | counter;

        return uniqueId;
    }

    void UniqueIDGenerator::resetCounter()

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_counter = 0;
    }

    uint64_t UniqueIDGenerator::getCounter() const
    {
        return m_counter.load();
    }

    UniqueIDGenerator::UniqueIDGenerator() :
        m_counter(0),
        m_gen(m_rd()),
        m_dist(0, 0xFFFF)
    {
        // 初始化机器ID（0-1023）
        m_machineId = getMachineId();
        // 初始化进程ID（0-1023）
        m_processId = getProcessId();
    }

    uint64_t UniqueIDGenerator::getMachineId()
    {
        // 使用硬件信息或配置文件生成唯一ID
        // 简化版本：使用随机数
        std::uniform_int_distribution<uint64_t> dist(0, 1023);
        return dist(m_gen);
    }

    uint64_t UniqueIDGenerator::getProcessId()
    {
        return getCurrentProcessId() & 0x3FFull;
    }
} // egret
