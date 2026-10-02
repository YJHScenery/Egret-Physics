/**
 * @file        eps_common.h
 * @brief       EPS 文件格式的公共基础工具：FourCC 与小端序字节读写器。
 * @details     EPS 格式所有多字节量统一按小端序存储，且不直接把结构体内存布局落盘，
 *              而是通过 ByteWriter / ByteReader 逐字段读写。这样既避免了编译器结构体
 *              对齐与填充带来的不确定性，也不必依赖 #pragma pack。
 *
 * @author      jehor
 * @date        2026-10-02
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     Serialize
 * @defgroup    Eps (Egret Physics Simulation 文件格式)
 */

#ifndef EGRET_PHYSICS_EPS_COMMON_H
#define EGRET_PHYSICS_EPS_COMMON_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

#include "eps_crc32c.h"

namespace egret::eps
{
    /** 四个 ASCII 字符组成的块类型标识。 */
    using FourCC = std::array<char, 4>;

    /**
     * @brief  由四个字符构造 FourCC。
     * @param  a 第一个字符。
     * @param  b 第二个字符。
     * @param  c 第三个字符。
     * @param  d 第四个字符。
     * @return 构造结果。
     */
    [[nodiscard]] constexpr FourCC makeFourCC(const char a, const char b, const char c, const char d) noexcept
    {
        return FourCC{a, b, c, d};
    }

    /**
     * @brief  判断两个 FourCC 是否相同。
     * @param  lhs 左操作数。
     * @param  rhs 右操作数。
     * @return 相同返回 true。
     */
    [[nodiscard]] constexpr bool operator==(const FourCC& lhs, const FourCC& rhs) noexcept
    {
        return lhs[0] == rhs[0] && lhs[1] == rhs[1] && lhs[2] == rhs[2] && lhs[3] == rhs[3];
    }

    /**
     * @brief  判断两个 FourCC 是否不同。
     * @param  lhs 左操作数。
     * @param  rhs 右操作数。
     * @return 不同返回 true。
     */
    [[nodiscard]] constexpr bool operator!=(const FourCC& lhs, const FourCC& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    /**
     * @brief  把 FourCC 转成便于日志输出的视图。
     * @param  tag 块标识。
     * @return 长度为 4 的字符串视图（不是以 '\0' 结尾的 C 字符串）。
     */
    [[nodiscard]] inline std::string_view toString(const FourCC& tag) noexcept
    {
        return std::string_view{tag.data(), tag.size()};
    }

    /**
     * @brief       小端序字节写入器。
     * @details     ByteWriter 把基础类型按小端序追加到外部持有的字节缓冲区尾部，
     *              并支持回填已写入的位置（用于文件收尾时修正文件头字段）。
     *              缓冲区由调用方持有，写入器不拥有内存。
     *
     * @invariant   m_buffer 引用的缓冲区在写入器生命周期内保持有效
     */
    class ByteWriter
    {
    public:
        /**
         * @brief 绑定输出缓冲区。
         * @param buffer 目标缓冲区引用。
         */
        explicit ByteWriter(std::vector<std::byte>& buffer) noexcept :
            m_buffer(buffer)
        {
        }

        /**
         * @brief 追加 8 位无符号整数。
         * @param value 待写入的值。
         */
        void writeU8(const std::uint8_t value)
        {
            m_buffer.push_back(static_cast<std::byte>(value));
        }

        /**
         * @brief 追加 16 位无符号整数（小端）。
         * @param value 待写入的值。
         */
        void writeU16(const std::uint16_t value)
        {
            writeU8(static_cast<std::uint8_t>(value & 0xFFu));
            writeU8(static_cast<std::uint8_t>((value >> 8u) & 0xFFu));
        }

        /**
         * @brief 追加 32 位无符号整数（小端）。
         * @param value 待写入的值。
         */
        void writeU32(const std::uint32_t value)
        {
            for (int shift = 0; shift < 32; shift += 8) {
                writeU8(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
            }
        }

        /**
         * @brief 追加 64 位无符号整数（小端）。
         * @param value 待写入的值。
         */
        void writeU64(const std::uint64_t value)
        {
            for (int shift = 0; shift < 64; shift += 8) {
                writeU8(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
            }
        }

        /**
         * @brief 追加双精度浮点数（按 IEEE 754 位模式原样小端写出）。
         * @param value 待写入的值。
         */
        void writeF64(const double value)
        {
            std::uint64_t bits{0};
            std::memcpy(&bits, &value, sizeof(bits));
            writeU64(bits);
        }

        /**
         * @brief 追加 FourCC。
         * @param tag 块标识。
         */
        void writeFourCC(const FourCC& tag)
        {
            for (const char ch : tag) {
                writeU8(static_cast<std::uint8_t>(ch));
            }
        }

        /**
         * @brief 追加一段原始字节。
         * @param data 数据起始地址。
         * @param size 字节数。
         */
        void writeBytes(const void* data, const std::size_t size)
        {
            if (data == nullptr || size == 0) {
                return;
            }

            const auto* bytes = static_cast<const std::byte*>(data);
            m_buffer.insert(m_buffer.end(), bytes, bytes + size);
        }

        /**
         * @brief 追加 count 个 0x00 填充字节。
         * @param count 填充字节数。
         */
        void writeZeros(const std::size_t count)
        {
            m_buffer.insert(m_buffer.end(), count, std::byte{0});
        }

        /** 当前缓冲区字节数。 */
        [[nodiscard]] std::size_t size() const noexcept
        {
            return m_buffer.size();
        }

        /** 直接访问底层缓冲区。 */
        [[nodiscard]] std::vector<std::byte>& buffer() noexcept
        {
            return m_buffer;
        }

        /**
         * @brief  回填 32 位无符号整数。
         * @param  offset 目标偏移（相对缓冲区起始）。
         * @param  value 待写入的值。
         * @return 偏移与长度都合法返回 true。
         */
        [[nodiscard]] bool overwriteU32(const std::size_t offset, const std::uint32_t value)
        {
            if (offset + 4 > m_buffer.size()) {
                return false;
            }

            for (int i = 0; i < 4; ++i) {
                m_buffer[offset + static_cast<std::size_t>(i)] =
                    static_cast<std::byte>((value >> (8 * i)) & 0xFFu);
            }
            return true;
        }

        /**
         * @brief  回填 64 位无符号整数。
         * @param  offset 目标偏移（相对缓冲区起始）。
         * @param  value 待写入的值。
         * @return 偏移与长度都合法返回 true。
         */
        [[nodiscard]] bool overwriteU64(const std::size_t offset, const std::uint64_t value)
        {
            if (offset + 8 > m_buffer.size()) {
                return false;
            }

            for (int i = 0; i < 8; ++i) {
                m_buffer[offset + static_cast<std::size_t>(i)] =
                    static_cast<std::byte>((value >> (8 * i)) & 0xFFu);
            }
            return true;
        }

    private:
        /** 外部持有的输出缓冲区。 */
        std::vector<std::byte>& m_buffer;
    };

    /**
     * @brief       小端序字节读取器。
     * @details     ByteReader 在只读缓冲区上顺序读取基础类型。任何越界读取都不会抛出异常，
     *              而是把内部状态置为「出错」并返回 0；调用方在读取若干字段后统一检查 ok()。
     *
     * @invariant   m_data 指向的缓冲区在读取器生命周期内保持有效
     */
    class ByteReader
    {
    public:
        /**
         * @brief 绑定只读缓冲区。
         * @param data 数据起始地址。
         * @param size 数据字节数。
         */
        ByteReader(const std::uint8_t* data, const std::size_t size) noexcept :
            m_data(data), m_size(size)
        {
        }

        /**
         * @brief 绑定只读字节缓冲区。
         * @param buffer 字节缓冲区。
         */
        explicit ByteReader(const std::vector<std::byte>& buffer) noexcept :
            m_data(buffer.empty() ? nullptr : reinterpret_cast<const std::uint8_t*>(buffer.data())),
            m_size(buffer.size())
        {
        }

        /** 自打开以来是否没有发生越界读取。 */
        [[nodiscard]] bool ok() const noexcept
        {
            return m_ok;
        }

        /** 当前读取位置。 */
        [[nodiscard]] std::size_t offset() const noexcept
        {
            return m_offset;
        }

        /** 剩余可读字节数。 */
        [[nodiscard]] std::size_t remaining() const noexcept
        {
            return m_offset <= m_size ? m_size - m_offset : 0;
        }

        /**
         * @brief  当前读取位置的只读指针。
         * @return 缓冲区为空或已越界时返回 nullptr，否则返回当前读取位置。
         * @remark 调用方应先用 remaining() 确认后续要读取的字节数，再使用本指针；
         *         本方法不改变读取位置，配合 skip() 可以零拷贝地访问连续数据块。
         */
        [[nodiscard]] const std::uint8_t* cursor() const noexcept
        {
            if (m_data == nullptr || m_offset > m_size) {
                return nullptr;
            }
            return m_data + m_offset;
        }

        /** 读取 8 位无符号整数。 */
        std::uint8_t readU8() noexcept
        {
            if (!ensure(1)) {
                return 0;
            }
            return m_data[m_offset++];
        }

        /** 读取 16 位无符号整数（小端）。 */
        std::uint16_t readU16() noexcept
        {
            if (!ensure(2)) {
                return 0;
            }

            const std::uint16_t value = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(m_data[m_offset]) |
                static_cast<std::uint16_t>(static_cast<std::uint16_t>(m_data[m_offset + 1]) << 8u));
            m_offset += 2;
            return value;
        }

        /** 读取 32 位无符号整数（小端）。 */
        std::uint32_t readU32() noexcept
        {
            if (!ensure(4)) {
                return 0;
            }

            std::uint32_t value{0};
            for (int i = 0; i < 4; ++i) {
                value |= static_cast<std::uint32_t>(m_data[m_offset + static_cast<std::size_t>(i)]) << (8 * i);
            }
            m_offset += 4;
            return value;
        }

        /** 读取 64 位无符号整数（小端）。 */
        std::uint64_t readU64() noexcept
        {
            if (!ensure(8)) {
                return 0;
            }

            std::uint64_t value{0};
            for (int i = 0; i < 8; ++i) {
                value |= static_cast<std::uint64_t>(m_data[m_offset + static_cast<std::size_t>(i)]) << (8 * i);
            }
            m_offset += 8;
            return value;
        }

        /** 读取双精度浮点数。 */
        double readF64() noexcept
        {
            const std::uint64_t bits = readU64();
            double value{0.0};
            std::memcpy(&value, &bits, sizeof(value));
            return value;
        }

        /** 读取 FourCC。 */
        FourCC readFourCC() noexcept
        {
            FourCC tag{};
            if (!ensure(tag.size())) {
                return tag;
            }

            for (std::size_t i = 0; i < tag.size(); ++i) {
                tag[i] = static_cast<char>(m_data[m_offset + i]);
            }
            m_offset += tag.size();
            return tag;
        }

        /**
         * @brief  跳过若干字节。
         * @param  count 跳过的字节数。
         * @return 未越界返回 true。
         */
        bool skip(const std::size_t count) noexcept
        {
            if (!ensure(count)) {
                return false;
            }
            m_offset += count;
            return true;
        }

        /**
         * @brief  把读取位置移动到绝对偏移。
         * @param  offset 目标偏移。
         * @return 偏移在范围内返回 true。
         */
        bool seek(const std::size_t offset) noexcept
        {
            if (offset > m_size) {
                m_ok = false;
                return false;
            }
            m_offset = offset;
            return true;
        }

    private:
        /**
         * @brief  检查并预留 count 个字节。
         * @param  count 需要的字节数。
         * @return 可读返回 true；否则置错误标志并返回 false。
         */
        [[nodiscard]] bool ensure(const std::size_t count) noexcept
        {
            if (!m_ok || m_offset + count > m_size) {
                m_ok = false;
                return false;
            }
            return true;
        }

        /** 数据起始地址。 */
        const std::uint8_t* m_data{nullptr};

        /** 数据总字节数。 */
        std::size_t m_size{0};

        /** 当前读取位置。 */
        std::size_t m_offset{0};

        /** 读取状态标志。 */
        bool m_ok{true};
    };
}

#endif // EGRET_PHYSICS_EPS_COMMON_H
