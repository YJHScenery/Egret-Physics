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
#include <algorithm>
#include <numeric>
#include <ranges>
#include <cmath>

#define EGRET_PHYSICS_APPLICATION_DISPLAY_NAME "Egret Physics Studio"

#define EGRET_PHYSICS_APPLICATION_NAME "EgretPhysicsStudio"

#define DEBUG_MODE 1

namespace egret
{
    /** @brief 快速乘方运算
     *  @note 指数必须为编译期非负整数常量
    */
    template <typename Base, std::integral Exponent, Exponent Ex>
    constexpr Base quickPower(Base base)
    {
        if constexpr (Ex == 0) {
            return Base(1);
        }
        else if constexpr (Ex < 0) {
            return Base(1) / quickPower<Base, Exponent, -Ex>(base);
        }
        else if constexpr (Ex == 1) {
            return base;
        }
        else if constexpr (Ex % 2 == 0) {
            auto half = quickPower<Base, Exponent, Ex / 2>(base);
            return half * half;
        }
        else {
            return base * quickPower<Base, Exponent, Ex - 1>(base);
        }
    }

    constexpr auto quickSquare = []<typename Base>(Base x)
    {
        return quickPower<Base, int, 2>(x);
    };


    /// @brief  类型支持 sqrt、quickSquare 和加法运算
    template <typename T>
    concept NumericWithOps = requires(T a, T b)
    {
        { std::sqrt(a) } -> std::convertible_to<T>;
        { quickSquare(a) } -> std::convertible_to<T>;
        { a + b } -> std::convertible_to<T>;
    };

    /// @brief 计算容器中元素的平方和的算术平方根
    template <NumericWithOps T, std::ranges::input_range Container>
        requires std::same_as<T, std::ranges::range_value_t<Container>>
    constexpr T sqrtSumOfSquares(const Container& container)
    {
        auto sum = std::accumulate(std::ranges::begin(container), std::ranges::end(container), T(0),
                                   [](T acc, const T& val) { return acc + quickSquare(val); }
        );
        return std::sqrt(sum);
    }

    // 零参数版本（单独重载，避免空包时 common_type 推导失败）
    constexpr double sqrtSumOfSquares()
    {
        return 0.0;
    }

    // 变参版本：适用于一个或多个参数
    template <NumericWithOps... Args>
    constexpr auto sqrtSumOfSquares(Args... args)
    {
        using Common = std::common_type_t<Args...>;
        Common sum = (quickSquare(static_cast<Common>(args)) + ...);
        return std::sqrt(sum);
    }
}

#include <Eigen/Dense>
#include <functional>

// 前向声明辅助结构体
// ReSharper disable once CppRedundantNamespaceDefinition
namespace std {
    template<int n>
    struct hash<Eigen::Matrix<double, n, 1>> {
        size_t operator()(const Eigen::Matrix<double, n, 1>& vec) const {
            return hash_impl(vec, std::integral_constant<int, n>{});
        }
    private:
        // 递归终止：当索引为 0 时，返回初始哈希值
        static size_t hash_impl(const Eigen::Matrix<double, n, 1>& vec, std::integral_constant<int, 0>) {
            return 0; // 或者返回一个种子值，如 0x9e3779b9
        }

        // 递归展开：从最后一个元素向前累加
        template<int N>
        static size_t hash_impl(const Eigen::Matrix<double, n, 1>& vec, std::integral_constant<int, N>) {
            // 先计算前 N-1 个元素的哈希
            size_t h = hash_impl(vec, std::integral_constant<int, N - 1>{});
            // 混合当前元素 (第 N-1 个元素，因为索引从 0 开始)
            h ^= std::hash<double>{}(vec[N - 1]) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };
}

#endif // EGRET_PHYSICS_GEOMETRY_H
