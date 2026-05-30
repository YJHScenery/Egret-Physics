/**
 * @file        qt_component_helper.cpp
 * @brief       Qt 组件助手实现文件，提供 Qt 相关组件工具函数。
 * @details     实现 QtComponentHelper 类的各项成员函数。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-23
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     View
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#include "qt_component_helper.h"

namespace egret
{
    void QtComponentHelper::showAboutQt()
    {
        QApplication::aboutQt();
    }
} // egret