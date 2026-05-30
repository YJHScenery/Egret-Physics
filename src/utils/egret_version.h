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
#include <concepts>
#include <boost/hana/string.hpp>

namespace egret
{
    template <typename T>
    constexpr T EGRET_PHYSICS_VERSION_MAJOR = 1;

    template <typename T>
    constexpr T EGRET_PHYSICS_VERSION_MINOR = 0;

    template <typename T>
    constexpr T EGRET_PHYSICS_VERSION_PATCH = 0;

    constexpr unsigned int EGRET_PHYSICS_VERSION_MAJOR_UINT = EGRET_PHYSICS_VERSION_MAJOR<int>;
    constexpr unsigned int EGRET_PHYSICS_VERSION_MINOR_UINT = EGRET_PHYSICS_VERSION_MINOR<int>;
    constexpr unsigned int EGRET_PHYSICS_VERSION_PATCH_UINT = EGRET_PHYSICS_VERSION_PATCH<int>;


    template <auto N>
        requires std::unsigned_integral<decltype(N)>
    constexpr int countDigits()
    {
        if constexpr (N == 0) return 1;
        int count = 0;
        auto n = N;
        while (n > 0) {
            ++count;
            n /= 10;
        }
        return count;
    }

    template <auto N>
        requires std::unsigned_integral<decltype(N)>
    constexpr auto toDigitArray()
    {
        constexpr int digits = countDigits<N>();
        std::array<char, digits> result{};
        auto n = N;
        int idx = digits;
        do {
            result[--idx] = static_cast<char>('0' + n % 10);
            n /= 10;
        }
        while (n > 0);
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

    template <typename T, T First, T... Rest>
        requires std::unsigned_integral<T>
    struct VersionBuilder
    {
        static constexpr auto firstPart = toDigitArray<First>();
        static constexpr auto restPart = VersionBuilder<T, Rest...>::result;
        static constexpr auto withDot = concat(std::array<char, 1>{'.'}, restPart);
        static constexpr auto result = concat(firstPart, withDot);
    };

    template <typename T, T Last>
        requires std::unsigned_integral<T>
    struct VersionBuilder<T, Last>
    {
        static constexpr auto result = toDigitArray<Last>();
    };

    /**
     * @brief 将 boost::hana::string 转换为编译期 std::array 的辅助结构体
     */
    template <typename>
    struct HanaStringToArray;

    template <char... Cs>
    struct HanaStringToArray<boost::hana::string<Cs...>>
    {
        static constexpr std::array<char, sizeof...(Cs)> value{Cs...};
    };

    /**
     * @brief 使用可定制的连接符递归构建版本号字符串的辅助结构体
     * @details 将版本号数字序列递归地拼接为 "x<Hyphen>y<Hyphen>z..." 格式的字符数组
     */
    template <typename Hyphen, typename T, T... Values>
        requires std::unsigned_integral<T> && (sizeof...(Values) >= 1)
    struct HyphenVersionBuilder;

    template <typename Hyphen, typename T, T First, T... Rest>
        requires std::unsigned_integral<T>
    struct HyphenVersionBuilder<Hyphen, T, First, Rest...>
    {
        static constexpr auto firstPart = toDigitArray<First>();
        static constexpr auto restPart = HyphenVersionBuilder<Hyphen, T, Rest...>::result;
        static constexpr auto hyphenArr = HanaStringToArray<Hyphen>::value;
        static constexpr auto withHyphen = concat(hyphenArr, restPart);
        static constexpr auto result = concat(firstPart, withHyphen);
    };

    template <typename Hyphen, typename T, T Last>
        requires std::unsigned_integral<T>
    struct HyphenVersionBuilder<Hyphen, T, Last>
    {
        static constexpr auto result = toDigitArray<Last>();
    };

    template <typename T, T... VersionNum>
        requires (std::unsigned_integral<T> && sizeof...(VersionNum) >= 1)
    struct VersionFormatter
    {
        static constexpr auto result = VersionBuilder<T, VersionNum...>::result;
        static constexpr std::string_view value{result.data(), result.size()};
    };

    template <typename T, T... VersionNum>
    static constexpr std::string_view version_v = VersionFormatter<T, VersionNum...>::value;

    static constexpr std::string_view EGRET_VERSION_STRING = version_v<
        unsigned int,
        EGRET_PHYSICS_VERSION_MAJOR_UINT,
        EGRET_PHYSICS_VERSION_MINOR_UINT,
        EGRET_PHYSICS_VERSION_PATCH_UINT>;

    /**
     * @brief 带前缀、版本号连接符和后缀的版本号格式化器
     *
     * 生成格式：<Prefix><VersionNum<Hyphen>VersionNum...><Suffix>
     * 例如：Prefix="v", Hyphen=".", Suffix="-release", Values={1,0,0,1}
     *       → "v1.0.0.1-release"
     *
     * @tparam Prefix  版本号前缀（boost::hana::string）
     * @tparam Hyphen  版本号数字之间的连接符（boost::hana::string）
     * @tparam Suffix  版本号后缀（boost::hana::string）
     * @tparam T       版本号数字的无符号整型类型
     * @tparam Values  版本号数字包（至少一个元素）
     */
    template <
        typename Prefix,
        typename Hyphen,
        typename Suffix,
        typename T,
        T... Values
    >
        requires (std::unsigned_integral<T> && sizeof...(Values) >= 1)
    struct FormattedVersionFormatter
    {
        static constexpr auto prefixArr = HanaStringToArray<Prefix>::value;
        static constexpr auto versionArr = HyphenVersionBuilder<Hyphen, T, Values...>::result;
        static constexpr auto suffixArr = HanaStringToArray<Suffix>::value;

        static constexpr auto t1 = concat(prefixArr, versionArr);
        static constexpr auto result = concat(t1, suffixArr);

        static constexpr std::string_view value{result.data(), result.size()};
    };

    /**
     * @brief formatted_version_v 变量模板
     *
     * 用法示例：
     * @code
     * using VP  = decltype(BOOST_HANA_STRING("v"));
     * using VH  = decltype(BOOST_HANA_STRING("."));
     * using VS  = decltype(BOOST_HANA_STRING("-release"));
     *
     * constexpr auto ver = formatted_version_v<VP, VH, VS, unsigned int, 1, 0, 0, 1>;
     * // ver == "v1.0.0.1-release"
     * @endcode
     */
    template <
        typename Prefix,
        typename Hyphen,
        typename Suffix,
        typename T,
        T... Values
    >
    static constexpr std::string_view formatted_version_v =
        FormattedVersionFormatter<Prefix, Hyphen, Suffix, T, Values...>::value;

#define CREATE_FORMATTED_VERSION_V(Prefix, Hyphen, Suffix, T, ...) \
formatted_version_v< \
decltype(BOOST_HANA_STRING(Prefix)), \
decltype(BOOST_HANA_STRING(Hyphen)), \
decltype(BOOST_HANA_STRING(Suffix)), \
T, \
##__VA_ARGS__>

    static constexpr std::string_view WHOLE_EGRET_VERSION_STRING = CREATE_FORMATTED_VERSION_V("v", ".", "-insiders",
        unsigned int,
        EGRET_PHYSICS_VERSION_MAJOR_UINT,
        EGRET_PHYSICS_VERSION_MINOR_UINT,
        EGRET_PHYSICS_VERSION_PATCH_UINT)    ;
}


#endif //EGRET_PHYSICS_EGRET_VERSION_H
