/**
 * @file        basic_utils.h
 * @brief       物理引擎基础工具宏定义文件，包含版本号和编译期断言。
 * @details     定义物理引擎的版本号信息，以及 C++ 标准要求的编译期断言检查。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-25
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Utils
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#ifndef EGRET_PHYSICS_GEOMETRY_H
#define EGRET_PHYSICS_GEOMETRY_H


#include <type_traits>
#include <concepts>
#include <type_traits>

#define EGRET_PHYSICS_APPLICATION_DISPLAY_NAME "Egret Physics Studio"

#define EGRET_PHYSICS_APPLICATION_NAME "EgretPhysicsStudio"

namespace egret
{
    template <typename Base, std::integral Exponent, Exponent Ex>
    constexpr Base quickPower(Base base)
    {
        if constexpr (Ex == 0)
        {
            return Base(1);
        }
        else if constexpr (Ex < 0)
        {
            return Base(1) / quickPower<Base, Exponent, -Ex>(base);
        }
        else if constexpr (Ex == 1)
        {
            return base;
        }
        else if constexpr (Ex % 2 == 0)
        {
            auto half = quickPower<Base, Exponent, Ex / 2>(base);
            return half * half;
        }
        else
        {
            return base * quickPower<Base, Exponent, Ex - 1>(base);
        }
    }

    constexpr auto quickSquare = []<typename Base>(Base x)
    {
        return quickPower<Base, int, 2>(x);
    };
}
#endif // EGRET_PHYSICS_GEOMETRY_H
