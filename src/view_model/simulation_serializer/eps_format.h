/**
 * @file        eps_format.h
 * @brief       EPS（Egret Physics Simulation）文件格式的常量与内存记录结构。
 * @details     本文件只描述「格式长什么样」，不含任何读写实现：
 *              - 文件级常量（魔数、版本、各层定长头长度、对齐要求）；
 *              - 块类型 FourCC 与标志位定义；
 *              - 采样间隔 / 推进步长的常量与抽帧比推导；
 *              - 帧内各段（刚体 / 约束 / 力场）的内存记录结构与其字节步长。
 *
 *              逐字段落盘格式见 docs/EPS 模拟数据文件格式规范（草案）.md。
 *              当前实现为 v1：仅使用 float64 精度、不压缩、定长帧段，
 *              文件头中的压缩与 float32 标志位保留但尚未启用。
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

#ifndef EGRET_PHYSICS_EPS_FORMAT_H
#define EGRET_PHYSICS_EPS_FORMAT_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <Eigen/Dense>

#include "eps_common.h"

namespace egret::eps
{
    // ---------------------------------------------------------------------
    //  文件级常量
    // ---------------------------------------------------------------------

    /**
     * 文件魔数：0x89 'E' 'P' 'S' CR LF SUB LF。
     * 首字节大于 0x7F 且含 CR/LF，任何文本模式传输造成的换行转换都会立刻破坏它，
     * 从而被识别为「不是 EPS 文件」而不是被当成文本解析。
     */
    inline constexpr std::array<std::uint8_t, 8> kFileMagic{
        0x89u, static_cast<std::uint8_t>('E'), static_cast<std::uint8_t>('P'),
        static_cast<std::uint8_t>('S'), 0x0Du, 0x0Au, 0x1Au, 0x0Au};

    /** 格式主版本号。 */
    inline constexpr std::uint16_t kVersionMajor{1};

    /** 格式次版本号。 */
    inline constexpr std::uint16_t kVersionMinor{0};

    /** 文件头固定长度（字节）。 */
    inline constexpr std::size_t kFileHeaderBytes{64};

    /** 数据块头固定长度（字节）。 */
    inline constexpr std::size_t kChunkHeaderBytes{24};

    /** 帧头固定长度（字节）。 */
    inline constexpr std::size_t kFrameHeaderBytes{24};

    /** 帧段头固定长度（字节）。 */
    inline constexpr std::size_t kSectionHeaderBytes{32};

    /** 块与帧段的对齐粒度（字节）。 */
    inline constexpr std::size_t kAlignment{8};

    // ---------------------------------------------------------------------
    //  块类型与段类型
    // ---------------------------------------------------------------------

    /** 场景描述块标记。 */
    inline constexpr FourCC kTagScene{makeFourCC('S', 'C', 'N', 'E')};

    /** 元数据块标记。 */
    inline constexpr FourCC kTagMeta{makeFourCC('M', 'E', 'T', 'A')};

    /** 帧数据块标记。 */
    inline constexpr FourCC kTagFrame{makeFourCC('F', 'R', 'A', 'M')};

    /** 帧索引块标记。 */
    inline constexpr FourCC kTagIndex{makeFourCC('I', 'N', 'D', 'X')};

    /** 尾块标记。 */
    inline constexpr FourCC kTagEnd{makeFourCC('E', 'N', 'D', '!')};

    /** 刚体帧段标记。 */
    inline constexpr FourCC kSectionBody{makeFourCC('B', 'O', 'D', 'Y')};

    /** 约束帧段标记。 */
    inline constexpr FourCC kSectionConstraint{makeFourCC('C', 'O', 'N', 'S')};

    /** 力场帧段标记。 */
    inline constexpr FourCC kSectionField{makeFourCC('F', 'L', 'D', 'S')};

    /** 曲线帧段标记（保留：曲线当前按静态几何处理）。 */
    inline constexpr FourCC kSectionCurve{makeFourCC('C', 'R', 'V', 'S')};

    /** 凸体网格帧段标记（保留：网格当前按静态几何处理）。 */
    inline constexpr FourCC kSectionMesh{makeFourCC('M', 'E', 'S', 'H')};

    // ---------------------------------------------------------------------
    //  标志位
    // ---------------------------------------------------------------------

    /**
     * @brief 文件头 flags 字段的位定义。
     * @details 位 0 固定为 1（小端序）；其余位为保留能力开关，
     *          当前写入端只会置位 kFlagLittleEndian。
     */
    enum class FileFlags : std::uint32_t
    {
        /** 小端序（固定置位）。 */
        LittleEndian = 1u << 0u,

        /** 几何量使用 float32（保留，尚未实现）。 */
        Float32Geometry = 1u << 1u,

        /** 帧载荷经 zstd 压缩（保留，尚未实现）。 */
        ZstdFrames = 1u << 2u,

        /** 帧块长度可变（保留，尚未实现）。 */
        VariableStride = 1u << 3u,

        /** 场景块经 zstd 压缩（保留，尚未实现）。 */
        ZstdScene = 1u << 4u,
    };

    /**
     * @brief  判断标志位是否被置位。
     * @param  flags 标志位集合。
     * @param  bit 待检查的位。
     * @return 置位返回 true。
     */
    [[nodiscard]] constexpr bool hasFlag(const std::uint32_t flags, const FileFlags bit) noexcept
    {
        return (flags & static_cast<std::uint32_t>(bit)) != 0u;
    }

    /** 刚体段 stateFlags 位：实体处于休眠（冻结）状态。 */
    inline constexpr std::uint32_t kBodyStateAsleep{1u << 0u};

    /** 刚体段 stateFlags 位：实体参与碰撞检测。 */
    inline constexpr std::uint32_t kBodyStateCollision{1u << 1u};

    /** 刚体段 stateFlags 位：实体参与积分。 */
    inline constexpr std::uint32_t kBodyStateIntegration{1u << 2u};

    /** 刚体段 stateFlags 位：实体是静态体（质量为 0）。 */
    inline constexpr std::uint32_t kBodyStateStatic{1u << 3u};

    /** 约束段 flags 位：约束处于启用状态。 */
    inline constexpr std::uint32_t kConstraintFlagEnabled{1u << 0u};

    /** 力场段 flags 位：场处于启用状态。 */
    inline constexpr std::uint32_t kFieldFlagEnabled{1u << 0u};

    // ---------------------------------------------------------------------
    //  采样与抽帧
    // ---------------------------------------------------------------------

    /** 默认求解推进步长：0.5 ms，以纳秒表示。 */
    inline constexpr std::uint64_t kDefaultSolverStepNs{500'000ull};

    /** 默认采样间隔：10 ms，以纳秒表示。 */
    inline constexpr std::uint64_t kDefaultSampleIntervalNs{10'000'000ull};

    /** 允许的采样间隔（毫秒）。 */
    inline constexpr std::array<std::uint32_t, 5> kAllowedSampleIntervalsMs{1u, 2u, 5u, 10u, 20u};

    /**
     * @brief  把秒换算为纳秒。
     * @param  seconds 秒数。
     * @return 四舍五入后的纳秒数。
     */
    [[nodiscard]] inline std::uint64_t secondsToNanoseconds(const double seconds) noexcept
    {
        constexpr double kNanosecondsPerSecond{1.0e9};
        return static_cast<std::uint64_t>(seconds * kNanosecondsPerSecond + 0.5);
    }

    /**
     * @brief  把纳秒换算为秒。
     * @param  nanoseconds 纳秒数。
     * @return 秒数。
     */
    [[nodiscard]] inline double nanosecondsToSeconds(const std::uint64_t nanoseconds) noexcept
    {
        constexpr double kNanosecondsPerSecond{1.0e9};
        return static_cast<double>(nanoseconds) / kNanosecondsPerSecond;
    }

    /**
     * @brief  推导抽帧比 k = 采样间隔 / 求解推进步长。
     * @param  solverStepNs 求解推进步长（纳秒），必须大于 0。
     * @param  sampleIntervalNs 采样间隔（纳秒），必须大于 0。
     * @return 能整除时返回抽帧比，否则返回 std::nullopt。
     */
    [[nodiscard]] inline std::optional<std::uint64_t> decimationFor(const std::uint64_t solverStepNs,
                                                                   const std::uint64_t sampleIntervalNs) noexcept
    {
        if (solverStepNs == 0 || sampleIntervalNs == 0 || sampleIntervalNs % solverStepNs != 0) {
            return std::nullopt;
        }
        return sampleIntervalNs / solverStepNs;
    }

    /**
     * @brief  判断采样间隔是否为允许的五档之一。
     * @param  sampleIntervalNs 采样间隔（纳秒）。
     * @return 允许返回 true。
     */
    [[nodiscard]] inline bool isAllowedSampleInterval(const std::uint64_t sampleIntervalNs) noexcept
    {
        for (const std::uint32_t intervalMs : kAllowedSampleIntervalsMs) {
            if (sampleIntervalNs == static_cast<std::uint64_t>(intervalMs) * 1'000'000ull) {
                return true;
            }
        }
        return false;
    }

    // ---------------------------------------------------------------------
    //  帧内记录结构（内存表示，落盘时逐字段写出）
    // ---------------------------------------------------------------------

    /** 单个刚体在一帧中的完整状态。 */
    struct BodyState
    {
        /** 稳定实体 ID。 */
        std::uint64_t id{0};

        /** 在场景实体表中的槽位。 */
        std::uint32_t slot{0};

        /** 状态标志位，取值见 kBodyState* 常量。 */
        std::uint32_t stateFlags{kBodyStateCollision | kBodyStateIntegration};

        /** 世界坐标位置（米）。 */
        Eigen::Vector3d position{Eigen::Vector3d::Zero()};

        /** 姿态四元数 (x, y, z, w)。 */
        Eigen::Quaterniond rotation{Eigen::Quaterniond::Identity()};

        /** 线速度（米/秒）。 */
        Eigen::Vector3d linearVelocity{Eigen::Vector3d::Zero()};

        /** 角速度（弧度/秒）。 */
        Eigen::Vector3d angularVelocity{Eigen::Vector3d::Zero()};

        /** 质量（千克）；0 表示静态体。 */
        double mass{0.0};

        /** 恢复系数。 */
        double restitution{1.0};
    };

    /** 单个约束在一帧中的状态。 */
    struct ConstraintState
    {
        /** 稳定约束 ID。 */
        std::uint64_t id{0};

        /** 在场景约束表中的槽位。 */
        std::uint32_t slot{0};

        /** 约束类型枚举值，与场景描述块中的 type 对应。 */
        std::uint32_t type{0};

        /** 标志位，取值见 kConstraintFlagEnabled。 */
        std::uint32_t flags{kConstraintFlagEnabled};

        /** 关联实体 A 的 ID；0 表示无。 */
        std::uint64_t entityAId{0};

        /** 关联实体 B 的 ID；0 表示无。 */
        std::uint64_t entityBId{0};

        /** 约束在实体 A 上的世界锚点。 */
        Eigen::Vector3d anchorAWorld{Eigen::Vector3d::Zero()};

        /** 世界锚点 B（或固定锚点）。 */
        Eigen::Vector3d anchorBWorld{Eigen::Vector3d::Zero()};

        /** 约束标称长度或目标长度。 */
        double restLength{0.0};

        /** 当前实际长度。 */
        double currentLength{0.0};

        /** 当前约束误差。 */
        double error{0.0};

        /** 当前约束力或力矩大小。 */
        double forceMagnitude{0.0};
    };

    /** 单个力场在一帧中的状态。 */
    struct FieldState
    {
        /** 稳定场 ID。 */
        std::uint64_t id{0};

        /** 在场景场表中的槽位。 */
        std::uint32_t slot{0};

        /** 场类型枚举值。 */
        std::uint32_t type{0};

        /** 标志位，取值见 kFieldFlagEnabled。 */
        std::uint32_t flags{kFieldFlagEnabled};

        /** 场方向 / 重力矢量 / 主轴。 */
        Eigen::Vector3d vector{Eigen::Vector3d::Zero()};

        /** 场中心（点场与质心场使用）。 */
        Eigen::Vector3d center{Eigen::Vector3d::Zero()};

        /** 场强或常量系数。 */
        double strength{0.0};
    };

    /** 一帧的完整数据。 */
    struct FrameState
    {
        /** 帧号，从 0 开始。 */
        std::uint32_t frameIndex{0};

        /** 该帧对应的求解器步号。 */
        std::uint64_t stepIndex{0};

        /** 该帧对应的仿真时刻（纳秒）。 */
        std::uint64_t simTimeNs{0};

        /** 刚体记录。 */
        std::vector<BodyState> bodies{};

        /** 约束记录。 */
        std::vector<ConstraintState> constraints{};

        /** 力场记录。 */
        std::vector<FieldState> fields{};
    };

    // ---------------------------------------------------------------------
    //  定长记录的字节步长（float64 模式）
    // ---------------------------------------------------------------------

    /** BODY 段单元素字节数。 */
    inline constexpr std::uint32_t kBodyStrideFloat64{144};

    /** CONS 段单元素字节数。 */
    inline constexpr std::uint32_t kConstraintStrideFloat64{128};

    /** FLDS 段单元素字节数。 */
    inline constexpr std::uint32_t kFieldStrideFloat64{88};

    /** 索引块单条目字节数。 */
    inline constexpr std::uint32_t kIndexEntryBytes{16};

    /**
     * @brief  计算一帧（不含块头与填充）的载荷字节数。
     * @param  bodyCount 刚体数。
     * @param  constraintCount 约束数。
     * @param  fieldCount 力场数。
     * @return 帧载荷字节数。
     */
    [[nodiscard]] constexpr std::uint64_t framePayloadBytes(const std::uint64_t bodyCount,
                                                            const std::uint64_t constraintCount,
                                                            const std::uint64_t fieldCount) noexcept
    {
        const std::uint64_t sections =
            (bodyCount > 0 ? kSectionHeaderBytes + bodyCount * kBodyStrideFloat64 : 0ull) +
            (constraintCount > 0 ? kSectionHeaderBytes + constraintCount * kConstraintStrideFloat64 : 0ull) +
            (fieldCount > 0 ? kSectionHeaderBytes + fieldCount * kFieldStrideFloat64 : 0ull);
        return kFrameHeaderBytes + sections;
    }
}

#endif // EGRET_PHYSICS_EPS_FORMAT_H
