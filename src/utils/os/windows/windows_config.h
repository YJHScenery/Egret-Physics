/**
 * @file        windows_config.h
 * @brief       Windows 平台配置头文件，提供 Windows 特定配置接口。
 * @details     定义 Windows 平台相关的配置函数，如终端 UTF-8 编码设置等。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-07
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Utils
 * @defgroup    组名 (如果文件定义了一个模块组)
 */
#ifndef EGRET_PHYSICS_WINDOWS_CONFIG_H
#define EGRET_PHYSICS_WINDOWS_CONFIG_H
#include <cstdint>

void setWindowsTerminalUtf8();

std::uint64_t getWindowsCurrentProcessId();

#endif // EGRET_PHYSICS_WINDOWS_CONFIG_H
