/**
 * @file        analysis_types.h
 * @brief       数据分析模块的公共数据类型与被抽取数据的存储结构。
 * @details     本文件只描述「数据分析需要哪些量、这些量以什么形式在内存中存在」，
 *              不含任何 EPSX 读取、统计或拟合逻辑：
 *
 *              - 实体类别（刚体 / 约束 / 力场）与通道枚举；
 *              - 通道元信息表（稳定英文键、中文显示名、单位、是否派生、当前写出端是否采集）；
 *              - 列式存储 Column（P1 直接持有 std::vector，P2 再扩展为可映射到临时文件），
 *                以及抽取结果 ExtractionResult；
 *              - 抽取请求 ExtractionRequest 与进度 / 取消回调类型。
 *
 *              全量序列一律按「列」存储而不是「每帧一个对象」，这样数十万到数百万样本
 *              仍然可以连续访问、按需抽稀、按需拟合；详见
 *              docs/数据分析工作区设计方案.md §4.6。
 *
 * @author      jehor
 * @date        2026-10-05
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     ViewModel
 * @defgroup    Analysis (数据分析)
 */

#ifndef EGRET_PHYSICS_ANALYSIS_TYPES_H
#define EGRET_PHYSICS_ANALYSIS_TYPES_H

#include <QString>
#include <QVector>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace egret::analysis
{
    // ---------------------------------------------------------------------
    //  实体类别
    // ---------------------------------------------------------------------

    /** 可分析的实体类别。 */
    enum class EntityKind : std::uint8_t
    {
        /** 刚体（BODY 段）。 */
        Body = 0,

        /** 约束（CONS 段）。 */
        Constraint = 1,

        /** 力场（FLDS 段）。 */
        Field = 2,
    };

    /**
     * @brief  实体类别的稳定英文键（用于导出与脚本）。
     * @param  kind 实体类别。
     * @return 静态字符串，取值 "body" / "constraint" / "field"。
     */
    [[nodiscard]] const char* entityKindKey(EntityKind kind) noexcept;

    /**
     * @brief  实体类别的中文显示名。
     * @param  kind 实体类别。
     * @return 静态字符串，取值 "刚体" / "约束" / "力场"。
     */
    [[nodiscard]] const char* entityKindDisplayName(EntityKind kind) noexcept;

    // ---------------------------------------------------------------------
    //  通道
    // ---------------------------------------------------------------------

    /**
     * @brief       可抽取的数据通道。
     * @details     枚举顺序即元信息表的顺序；新增通道时必须同时补 channelInfo 表，
     *              否则 channelInfo() 会返回未知占位项。
     */
    enum class Channel : std::uint16_t
    {
        /** 仿真时间（秒），所有序列共用的横轴。 */
        TimeSeconds = 0,

        /** 帧号。 */
        FrameIndex,

        /** 求解步号。 */
        StepIndex,

        /** 世界坐标 X（m）。 */
        PositionX,

        /** 世界坐标 Y（m）。 */
        PositionY,

        /** 世界坐标 Z（m）。 */
        PositionZ,

        /** 姿态四元数 x 分量。 */
        RotationX,

        /** 姿态四元数 y 分量。 */
        RotationY,

        /** 姿态四元数 z 分量。 */
        RotationZ,

        /** 姿态四元数 w 分量。 */
        RotationW,

        /** 线速度 X（m/s）。 */
        LinearVelocityX,

        /** 线速度 Y（m/s）。 */
        LinearVelocityY,

        /** 线速度 Z（m/s）。 */
        LinearVelocityZ,

        /** 角速度 X（rad/s）。 */
        AngularVelocityX,

        /** 角速度 Y（rad/s）。 */
        AngularVelocityY,

        /** 角速度 Z（rad/s）。 */
        AngularVelocityZ,

        /** 质量（kg）。 */
        Mass,

        /** 恢复系数（无量纲）。 */
        Restitution,

        /** 刚体状态标志位（位域）。 */
        BodyStateFlags,

        /** 约束标称长度（单位随约束类型）。 */
        ConstraintRestLength,

        /** 约束当前长度。 */
        ConstraintCurrentLength,

        /** 约束误差。 */
        ConstraintError,

        /** 约束力 / 力矩大小。 */
        ConstraintForce,

        /** 力场矢量 X。 */
        FieldVectorX,

        /** 力场矢量 Y。 */
        FieldVectorY,

        /** 力场矢量 Z。 */
        FieldVectorZ,

        /** 场强或常量系数。 */
        FieldStrength,

        /** 派生：位置模长 |r|（m）。 */
        PositionMagnitude,

        /** 派生：速率 |v|（m/s）。 */
        Speed,

        /** 派生：角速度模长 |ω|（rad/s）。 */
        AngularSpeed,

        /** 派生：姿态欧拉角 roll（rad）。 */
        EulerRoll,

        /** 派生：姿态欧拉角 pitch（rad）。 */
        EulerPitch,

        /** 派生：姿态欧拉角 yaw（rad）。 */
        EulerYaw,

        /** 派生：平动动能 0.5 m |v|²（J）。 */
        KineticTranslational,

        /** 枚举计数，必须位于最后。 */
        Count
    };

    /** 通道总数。 */
    inline constexpr std::size_t kChannelCount{static_cast<std::size_t>(Channel::Count)};

    /**
     * @brief       单个通道的元信息。
     * @details     populated 表示「当前 EPSX 写出端是否真的采集了这个量」：
     *              BODY 段的量均已采集；CONS / FLDS 段目前只写 ID、槽位、类型与启用标志，
     *              锚点、矢量、场强等字段恒为 0，因此界面需要据此把这些通道置灰。
     */
    struct ChannelInfo
    {
        /** 通道枚举值。 */
        Channel channel{Channel::TimeSeconds};

        /** 稳定英文键（导出列名 / 脚本用）。 */
        const char* key{"unknown"};

        /** 中文显示名。 */
        const char* displayName{"未知通道"};

        /** 单位（UTF-8 文本，无量纲时为空串）。 */
        const char* unit{""};

        /** 该通道属于哪类实体。 */
        EntityKind owner{EntityKind::Body};

        /** 是否为派生量（由其他通道计算得到）。 */
        bool derived{false};

        /** 当前 EPSX 写出端是否采集了该量。 */
        bool populated{true};
    };

    /**
     * @brief  查询通道元信息。
     * @param  channel 通道。
     * @return 元信息引用；枚举越界时返回未知占位项。
     */
    [[nodiscard]] const ChannelInfo& channelInfo(Channel channel) noexcept;

    /**
     * @brief  判断通道是否由某个具体实体承载（而非全局横轴）。
     * @param  channel 通道。
     * @return 属于刚体 / 约束 / 力场返回 true；时间与帧号返回 false。
     */
    [[nodiscard]] bool isEntityChannel(Channel channel) noexcept;

    // ---------------------------------------------------------------------
    //  实体
    // ---------------------------------------------------------------------

    /** 实体的稳定引用（ID 是 EPSX 中对齐各帧记录的依据）。 */
    struct EntityRef
    {
        /** 实体类别。 */
        EntityKind kind{EntityKind::Body};

        /** 稳定实体 ID。 */
        std::uint64_t id{0};

        /** 场景实体表槽位（仅同类实体内部有意义）。 */
        std::uint32_t slot{0};
    };

    /**
     * @brief  实体引用相等比较。
     * @param  lhs 左操作数。
     * @param  rhs 右操作数。
     * @return 类别与 ID 均相同返回 true。
     */
    [[nodiscard]] inline bool operator==(const EntityRef& lhs, const EntityRef& rhs) noexcept
    {
        return lhs.kind == rhs.kind && lhs.id == rhs.id;
    }

    /**
     * @brief  实体引用不等比较。
     * @param  lhs 左操作数。
     * @param  rhs 右操作数。
     * @return 类别或 ID 不同返回 true。
     */
    [[nodiscard]] inline bool operator!=(const EntityRef& lhs, const EntityRef& rhs) noexcept
    {
        return !(lhs == rhs);
    }

    /** 场景描述块中解析出的实体描述。 */
    struct EntityDescriptor
    {
        /** 实体引用。 */
        EntityRef ref{};

        /** 实体名称（缺失时由调用方生成占位名）。 */
        QString name{};

        /** 质量（kg）。 */
        double mass{0.0};

        /** 恢复系数。 */
        double restitution{1.0};

        /** 是否固定（静态体）。 */
        bool fixed{false};

        /** 约束 / 力场类型枚举值；刚体为 0。 */
        std::uint32_t type{0};
    };

    // ---------------------------------------------------------------------
    //  列式存储
    // ---------------------------------------------------------------------

    /**
     * @brief       一列 double 数据。
     * @details     P1 阶段直接持有 std::vector<double>；P2 阶段会在此基础上增加
     *              「超出内存阈值时写入临时文件并内存映射」的后备实现，调用方接口保持不变。
     *
     * @invariant   size() 等于底层容器元素个数
     */
    class Column
    {
    public:
        /**
         * @brief  预留容量，避免逐点扩容。
         * @param  count 预留的样本个数。
         */
        void reserve(const std::size_t count)
        {
            m_values.reserve(count);
        }

        /**
         * @brief  追加一个样本。
         * @param  value 样本值。
         */
        void pushBack(const double value)
        {
            m_values.push_back(value);
        }

        /**
         * @brief  调整长度（新元素清零）。
         * @param  count 目标长度。
         */
        void resize(const std::size_t count)
        {
            m_values.resize(count, 0.0);
        }

        /** 清空数据。 */
        void clear() noexcept
        {
            m_values.clear();
        }

        /** 样本个数。 */
        [[nodiscard]] std::size_t size() const noexcept
        {
            return m_values.size();
        }

        /** 是否为空。 */
        [[nodiscard]] bool empty() const noexcept
        {
            return m_values.empty();
        }

        /**
         * @brief  只读下标访问。
         * @param  index 下标。
         * @return 样本值。
         */
        [[nodiscard]] double operator[](const std::size_t index) const noexcept
        {
            return m_values[index];
        }

        /**
         * @brief  可写下标访问。
         * @param  index 下标。
         * @return 样本引用。
         */
        double& operator[](const std::size_t index) noexcept
        {
            return m_values[index];
        }

        /** 只读连续数据指针。 */
        [[nodiscard]] const double* data() const noexcept
        {
            return m_values.data();
        }

        /** 可写连续数据指针。 */
        [[nodiscard]] double* data() noexcept
        {
            return m_values.data();
        }

        /** 底层容器（仅供需要整体遍历的内部实现使用）。 */
        [[nodiscard]] const std::vector<double>& values() const noexcept
        {
            return m_values;
        }

    private:
        /** 实际存储。 */
        std::vector<double> m_values{};
    };

    // ---------------------------------------------------------------------
    //  抽取请求与结果
    // ---------------------------------------------------------------------

    /** 一次抽取请求。 */
    struct ExtractionRequest
    {
        /** 源 EPSX 文件路径。 */
        QString filePath{};

        /** 需要抽取的实体（刚体）。 */
        QVector<EntityRef> entities{};

        /** 需要抽取的通道（不含时间 / 帧号，抽取器会自动附加公共横轴）。 */
        QVector<Channel> channels{};

        /** 二次抽帧比：每 stride 帧取一个样本，必须 ≥ 1。 */
        std::uint32_t stride{1};

        /** 是否自动纳入文件中的全部约束实体（场景描述未命名时仍然可抽取）。 */
        bool includeAllConstraints{false};

        /** 是否自动纳入文件中的全部力场实体。 */
        bool includeAllFields{false};
    };

    /** 抽取过程中的进度与取消回调。 */
    struct ExtractionCallbacks
    {
        /** 进度回调：已完成帧数 / 总帧数；置空则不回调。 */
        std::function<void(std::uint64_t completed, std::uint64_t total)> progress{};

        /** 取消判定：返回 true 表示调用方要求提前结束；置空表示不可取消。 */
        std::function<bool()> cancelled{};
    };

    /** 单个「实体 × 通道」的抽取结果。 */
    struct ExtractedSeries
    {
        /** 所属实体。 */
        EntityRef entity{};

        /** 通道。 */
        Channel channel{Channel::TimeSeconds};

        /** 与时间列等长的样本值。 */
        Column values{};
    };

    /**
     * @brief       一次抽取的完整结果。
     * @details     所有序列共享同一条时间轴（同一 EPSX 文件中各帧时刻唯一），
     *              因此 time / frameIndex / stepIndex 只存一份，序列只存纵轴数值。
     *
     * @invariant   对任意 series，series.values.size() == time.size()
     */
    struct ExtractionResult
    {
        /** 源文件路径。 */
        QString sourcePath{};

        /** 生成者描述（来自 SCNE）。 */
        QString generator{};

        /** 文件中的总帧数。 */
        std::uint64_t totalFrameCount{0};

        /** 实际抽取到的样本数。 */
        std::uint64_t sampledCount{0};

        /** 本次使用的抽帧比。 */
        std::uint32_t stride{1};

        /** 文件声明的采样间隔（纳秒）。 */
        std::uint64_t sampleIntervalNs{0};

        /** 文件声明的求解推进步长（纳秒）。 */
        std::uint64_t solverStepNs{0};

        /** 时间轴（秒），长度为 sampledCount。 */
        std::vector<double> time{};

        /** 帧号轴，长度为 sampledCount。 */
        std::vector<std::uint32_t> frameIndex{};

        /** 求解步号轴，长度为 sampledCount。 */
        std::vector<std::uint64_t> stepIndex{};

        /** 被抽取实体的描述信息。 */
        QVector<EntityDescriptor> entities{};

        /** 抽取出的序列集合。 */
        QVector<ExtractedSeries> series{};

        /**
         * @brief  按「实体 + 通道」查找序列。
         * @param  entity 实体引用。
         * @param  channel 通道。
         * @return 命中返回指针，未命中返回 nullptr。
         */
        [[nodiscard]] const ExtractedSeries* find(const EntityRef& entity, Channel channel) const;
    };
}

#endif // EGRET_PHYSICS_ANALYSIS_TYPES_H
