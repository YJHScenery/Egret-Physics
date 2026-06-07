/**
 * @file        windows_config.cpp
 * @brief       Windows 平台配置实现文件，提供 Windows 特定配置接口。
 * @details     实现 Windows 平台相关的配置函数，如终端 UTF-8 编码设置等。
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
#include "windows_config.h"

#include <sys/stat.h>

#if _WIN32

#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00

#include <Windows.h>

void setTerminalUtf8()
{
    SetConsoleOutputCP(CP_UTF8);
}

std::uint64_t getCurrentProcessId()
{
    return GetCurrentProcessId();
}


#else

#include <unistd.h>

void setTerminalUtf8()
{
    // TODO: 需要实现跨平台逻辑

    return;
}

std::uint64_t getCurrentProcessId()
{
    // TODO: 需要实现跨平台逻辑

    return static_cast<std::uint64_t>(getpid());
}


#endif
