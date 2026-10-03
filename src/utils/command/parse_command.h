/**
 * @file        parse_command.h
 * @brief       --parse / -p 功能：解析仿真文件的指定帧。
 * @details     打开 EPSX 文件，读取指定帧，把帧内全部实体、约束、力场数据
 *              以单行 JSON 打印到标准输出，便于脚本与人工核对。
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

#ifndef EGRET_PHYSICS_PARSE_COMMAND_H
#define EGRET_PHYSICS_PARSE_COMMAND_H

#include "command_line.h"

namespace egret::command
{
    /**
     * @brief  解析仿真文件的指定帧。
     * @param  request 命令行请求，需包含 parseTarget 与 parseFrame，可选 parseFile。
     * @return 进程退出码：0 成功，1 运行失败，2 参数错误。
     */
    [[nodiscard]] int runParse(const Request& request);
}

#endif // EGRET_PHYSICS_PARSE_COMMAND_H
