/**
 * @file        epsx_crc32c.h
 * @brief       EPSX 文件格式使用的 CRC-32C（Castagnoli）校验算法。
 * @details     提供编译期生成的查找表与一次调用的校验接口，用于为 EPSX 文件头、
 *              数据块载荷与帧段载荷逐层计算校验和。
 *              约定与 iSCSI（RFC 3720 附录 B）一致：初值 0xFFFFFFFF，输入按字节反射处理，
 *              结果再次取反。
 *
 *              选择 CRC-32C 而非 CRC-32 的原因：主流 x86/ARM 均提供 CRC-32C 的硬件指令，
 *              且本格式的目标是「逐块定位损坏」，CRC-32C 的检错能力优于 CRC-32。
 *
 * @author      jehor
 * @date        2026-10-02
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     Serialize
 * @defgroup    EPSX (Egret Physics Simulation 文件格式)
 */

#ifndef EGRET_PHYSICS_EPSX_CRC32C_H
#define EGRET_PHYSICS_EPSX_CRC32C_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace egret::epsx
{
    namespace detail
    {
        /** 反射形式下的 CRC-32C（Castagnoli）生成多项式。 */
        inline constexpr std::uint32_t kCrc32cReflectedPolynomial{0x82F63B78u};

        /**
         * @brief       编译期生成的 256 项 CRC-32C 查找表。
         * @details     使用 constexpr 构造函数在编译期完成建表，运行期只做查表，
         *              避免静态初始化顺序问题与启动开销。
         */
        struct Crc32cTable
        {
            /** 查找表内容。 */
            std::array<std::uint32_t, 256> values{};

            /** 构造时按位生成整张表。 */
            constexpr Crc32cTable()
            {
                for (std::uint32_t index = 0; index < 256; ++index) {
                    std::uint32_t crc{index};
                    for (int bit = 0; bit < 8; ++bit) {
                        crc = (crc & 1u) != 0u
                                  ? (crc >> 1u) ^ kCrc32cReflectedPolynomial
                                  : (crc >> 1u);
                    }
                    values[index] = crc;
                }
            }
        };

        /** 全局唯一的查找表实例。 */
        inline constexpr Crc32cTable kCrc32cTable{};
    }

    /**
     * @brief  计算一段内存的 CRC-32C 校验和。
     * @param  data 数据起始地址；size 为 0 时允许传入 nullptr。
     * @param  size 数据字节数。
     * @return 32 位校验和。
     */
    [[nodiscard]] inline std::uint32_t crc32c(const void* data, const std::size_t size) noexcept
    {
        if (data == nullptr || size == 0) {
            return 0u;
        }

        const auto* bytes = static_cast<const std::uint8_t*>(data);
        std::uint32_t crc{0xFFFFFFFFu};

        for (std::size_t i = 0; i < size; ++i) {
            crc = detail::kCrc32cTable.values[(crc ^ bytes[i]) & 0xFFu] ^ (crc >> 8u);
        }

        return crc ^ 0xFFFFFFFFu;
    }
}

#endif // EGRET_PHYSICS_EPSX_CRC32C_H
