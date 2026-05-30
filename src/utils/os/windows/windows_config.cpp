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

#if (__has_include(<Windows.h>))
#include <Windows.h>
void setWindowsTerminalUtf8()
{
    SetConsoleOutputCP(CP_UTF8);
}
#else
void setWindowsTerminalUtf8()
{
    return;
}
#endif
