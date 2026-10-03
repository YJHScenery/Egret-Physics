/**
 * @file        solve_command.h
 * @brief       --solve 功能：离线求解场景并写出仿真文件。
 * @details     读取场景 JSON，交给 OfflineSimulationRunner 以固定推进步长求解，
 *              按采样间隔抽帧写入 EPSX 文件，并把进度打印到标准输出。
 *
 * @author      jehor
 * @date        2026-10-02
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     Utils
 * @defgroup    Command (命令行接口)
 */

#ifndef EGRET_PHYSICS_SOLVE_COMMAND_H
#define EGRET_PHYSICS_SOLVE_COMMAND_H

#include "command_line.h"

namespace egret::command
{
    /**
     * @brief  执行一次离线求解。
     * @param  request 命令行请求，需包含 sceneFile，可选 outputFile 与采样参数。
     * @return 进程退出码：0 成功，1 运行失败，2 参数错误。
     */
    [[nodiscard]] int runSolve(const Request& request);
}

#endif // EGRET_PHYSICS_SOLVE_COMMAND_H
