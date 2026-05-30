/**
* @file         egret_version.h
 * @brief       项目版本号管理，包含各级版本号编译期常量
 * @details     定义物理引擎的版本号信息，以及 C++ 标准要求的编译期断言检查。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-30
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Utils
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#ifndef EGRET_PHYSICS_EGRET_VERSION_H
#define EGRET_PHYSICS_EGRET_VERSION_H

#include <array>
#include <string_view>
#include <algorithm>


template <typename T>
constexpr T EGRET_PHYSICS_VERSION_MAJOR = 1;

template <typename T>
constexpr T EGRET_PHYSICS_VERSION_MINOR = 0;

template <typename T>
constexpr T EGRET_PHYSICS_VERSION_PATCH = 0;

constexpr int EGRET_PHYSICS_VERSION_MAJOR_INT = EGRET_PHYSICS_VERSION_MAJOR<int>;
constexpr int EGRET_PHYSICS_VERSION_MINOR_INT = EGRET_PHYSICS_VERSION_MINOR<int>;
constexpr int EGRET_PHYSICS_VERSION_PATCH_INT = EGRET_PHYSICS_VERSION_PATCH<int>;


namespace egret
{
    template <int N>
    requires (N >= 0)
    constexpr int countDigits() {
        if constexpr (N == 0) return 1;
        int count = 0;
        int n = N;
        while (n > 0) {
            ++count;
            n /= 10;
        }
        return count;
    }

    template <int N>
    constexpr auto intToArray() {
        constexpr int digits = countDigits<N>();
        std::array<char, digits> result{};
        int n = N;
        int idx = digits;
        do {
            result[--idx] = '0' + n % 10;
            n /= 10;
        } while (n > 0);
        return result;
    }

    template <typename T, size_t N1, size_t N2>
    constexpr auto concat(const std::array<T, N1>& a, const std::array<T, N2>& b)
    {
        std::array<T, N1 + N2> result{};
        size_t i = 0;
        for (T c : a) {
            result[i++] = c;
        }
        for (T c : b) {
            result[i++] = c;
        }
        return result;
    }

    template <int Major, int Minor, int Patch>
    struct VersionFormatter
    {
        static constexpr auto major_part = intToArray<Major>();
        static constexpr auto dot = std::array<char, 1>{'.'};
        static constexpr auto minor_part = intToArray<Minor>();
        static constexpr auto patch_part = intToArray<Patch>();

        static constexpr auto temp1 = concat(major_part, dot);
        static constexpr auto temp2 = concat(temp1, minor_part);
        static constexpr auto temp3 = concat(temp2, dot);
        static constexpr auto result = concat(temp3, patch_part);

        static constexpr std::string_view value{result.data(), result.size()};
    };

    template <int Major, int Minor, int Patch>
    static constexpr std::string_view version_v = VersionFormatter<Major, Minor, Patch>::value;

    static constexpr std::string_view EGRET_VERSION_STRING = version_v<EGRET_PHYSICS_VERSION_MAJOR_INT, EGRET_PHYSICS_VERSION_MINOR_INT, EGRET_PHYSICS_VERSION_PATCH_INT>;
}


#endif //EGRET_PHYSICS_EGRET_VERSION_H
