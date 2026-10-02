/**
 * @file        offline_simulation_runner.h
 * @brief       离线求解器：按固定推进步长求解场景并写出 EPS 文件。
 * @details     OfflineSimulationRunner 把「求解」从「实时渲染」中彻底剥离出来：
 *
 *              1. 由编辑器导出的场景 JSON 重建 WorldSceneManager（与实时模拟使用同一套策略组合）；
 *              2. 以固定推进步长 h（默认 0.5 ms）连续求解，累计总时长；
 *              3. 每 k = 采样间隔 / h 步抽出一帧写入 EPS 文件，第 0 帧为初始状态。
 *
 *              求解过程不创建任何 Qt 模型，也不依赖定时器，因此不受渲染帧率影响，
 *              同一场景与同一配置可以得到完全一致的结果。
 *
 * @author      jehor
 * @date        2026-10-02
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     Simulator
 * @defgroup    Offline (离线求解)
 */

#ifndef EGRET_PHYSICS_OFFLINE_SIMULATION_RUNNER_H
#define EGRET_PHYSICS_OFFLINE_SIMULATION_RUNNER_H

#include <QString>

#include <cstdint>

#include "eps_format.h"
#include "solver_types.h"

namespace egret
{
    /**
     * @brief       离线求解与导出的执行器。
     * @details     所有接口均为静态，调用方无需持有实例。
     *
     * @invariant   run() 返回 true 时输出文件已正常收尾（含索引块与尾块）
     */
    class OfflineSimulationRunner final
    {
    public:
        /**
         * @brief 一次离线求解的请求参数。
         */
        struct Request
        {
            /** 编辑器导出的场景 JSON（与 loadSceneFromJsonString 的输入格式一致）。 */
            QString sceneJson{};

            /** 输出文件路径。 */
            QString outputPath{};

            /** 需要求解的仿真总时长（秒）。 */
            double durationSeconds{60.0};

            /** 求解推进步长（纳秒），默认 0.5 ms。 */
            std::uint64_t solverStepNs{eps::kDefaultSolverStepNs};

            /** 采样间隔（纳秒），默认 10 ms。 */
            std::uint64_t sampleIntervalNs{eps::kDefaultSampleIntervalNs};

            /** 求解器运行时配置（迭代次数、穿透容差等）。 */
            SolverConfig solverConfig{};
        };

        /**
         * @brief 一次离线求解的执行结果。
         */
        struct Report
        {
            /** 实际执行的求解步数。 */
            std::uint64_t stepCount{0};

            /** 实际写入的帧数。 */
            std::uint64_t frameCount{0};

            /** 求解与写盘消耗的真实时间（秒）。 */
            double wallClockSeconds{0.0};

            /** 输出的文件大小（字节）。 */
            std::uint64_t fileBytes{0};

            /** 场景中的刚体 / 约束 / 力场数量。 */
            std::uint32_t bodyCount{0};
            std::uint32_t constraintCount{0};
            std::uint32_t fieldCount{0};
        };

        /**
         * @brief  执行一次离线求解并写出 EPS 文件。
         * @param  request 请求参数。
         * @param  report 输出执行结果，可为 nullptr。
         * @param  errorMessage 失败时写入错误描述，可为 nullptr。
         * @return 成功返回 true。
         */
        [[nodiscard]] static bool run(const Request& request, Report* report, QString* errorMessage);

    private:
        /** 本类只提供静态接口，禁止实例化。 */
        OfflineSimulationRunner() = delete;
    };
}

#endif // EGRET_PHYSICS_OFFLINE_SIMULATION_RUNNER_H
