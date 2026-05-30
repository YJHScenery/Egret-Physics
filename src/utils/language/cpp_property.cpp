/**
 * @file        cpp_property.cpp
 * @brief       C++ 属性机制检查文件，确保使用 C++20 标准。
 * @details     通过静态断言确保代码使用 C++20 及以上标准编译，保证特性兼容性。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-25
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Utils
 * @defgroup    组名 (如果文件定义了一个模块组)
 */
static_assert(__cplusplus >= 202002L, "此项目必须使用 C++20 及以上标准");
