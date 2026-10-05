/**
 * @file        analysis_types.cpp
 * @brief       数据分析模块公共数据类型的实现（元信息表与查找函数）。
 * @details     通道元信息表按 Channel 枚举顺序排列，编译期静态断言其长度与 kChannelCount 一致，
 *              新增通道却忘记补表时会在编译期直接失败，而不是运行期返回占位项。
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

#include "analysis_types.h"

#include <array>

namespace egret::analysis
{
    const char* entityKindKey(const EntityKind kind) noexcept
    {
        switch (kind)
        {
        case EntityKind::Body:
            return "body";
        case EntityKind::Constraint:
            return "constraint";
        case EntityKind::Field:
            return "field";
        }
        return "unknown";
    }

    const char* entityKindDisplayName(const EntityKind kind) noexcept
    {
        switch (kind)
        {
        case EntityKind::Body:
            return "刚体";
        case EntityKind::Constraint:
            return "约束";
        case EntityKind::Field:
            return "力场";
        }
        return "未知实体";
    }

    namespace
    {
        /** 未知通道占位项。 */
        constexpr ChannelInfo kUnknownChannelInfo{
            Channel::TimeSeconds, "unknown", "未知通道", "", EntityKind::Body, false, false};

        /** 生成一条通道元信息。 */
        constexpr ChannelInfo makeBodyChannel(const Channel channel,
                                              const char* key,
                                              const char* displayName,
                                              const char* unit,
                                              const bool derived = false)
        {
            return ChannelInfo{channel, key, displayName, unit, EntityKind::Body, derived, true};
        }

        /** 生成一条约束通道元信息（当前写出端未采集有效值）。 */
        constexpr ChannelInfo makeConstraintChannel(const Channel channel,
                                                    const char* key,
                                                    const char* displayName,
                                                    const char* unit)
        {
            return ChannelInfo{channel, key, displayName, unit, EntityKind::Constraint, false, false};
        }

        /** 生成一条力场通道元信息（当前写出端未采集有效值）。 */
        constexpr ChannelInfo makeFieldChannel(const Channel channel,
                                               const char* key,
                                               const char* displayName,
                                               const char* unit)
        {
            return ChannelInfo{channel, key, displayName, unit, EntityKind::Field, false, false};
        }

        /**
         * @brief 通道元信息表。
         * @details 顺序必须与 Channel 枚举完全一致。
         */
        constexpr std::array<ChannelInfo, kChannelCount> kChannelTable{
            makeBodyChannel(Channel::TimeSeconds, "time", "时间", "s"),
            makeBodyChannel(Channel::FrameIndex, "frameIndex", "帧号", ""),
            makeBodyChannel(Channel::StepIndex, "stepIndex", "步号", ""),
            makeBodyChannel(Channel::PositionX, "positionX", "位置-X", "m"),
            makeBodyChannel(Channel::PositionY, "positionY", "位置-Y", "m"),
            makeBodyChannel(Channel::PositionZ, "positionZ", "位置-Z", "m"),
            makeBodyChannel(Channel::RotationX, "rotationX", "姿态四元数-X", ""),
            makeBodyChannel(Channel::RotationY, "rotationY", "姿态四元数-Y", ""),
            makeBodyChannel(Channel::RotationZ, "rotationZ", "姿态四元数-Z", ""),
            makeBodyChannel(Channel::RotationW, "rotationW", "姿态四元数-W", ""),
            makeBodyChannel(Channel::LinearVelocityX, "velocityX", "速度-X", "m/s"),
            makeBodyChannel(Channel::LinearVelocityY, "velocityY", "速度-Y", "m/s"),
            makeBodyChannel(Channel::LinearVelocityZ, "velocityZ", "速度-Z", "m/s"),
            makeBodyChannel(Channel::AngularVelocityX, "angularVelocityX", "角速度-X", "rad/s"),
            makeBodyChannel(Channel::AngularVelocityY, "angularVelocityY", "角速度-Y", "rad/s"),
            makeBodyChannel(Channel::AngularVelocityZ, "angularVelocityZ", "角速度-Z", "rad/s"),
            makeBodyChannel(Channel::Mass, "mass", "质量", "kg"),
            makeBodyChannel(Channel::Restitution, "restitution", "恢复系数", ""),
            makeBodyChannel(Channel::BodyStateFlags, "stateFlags", "状态标志", ""),
            makeConstraintChannel(Channel::ConstraintRestLength, "constraintRestLength", "约束-标称长度", "m"),
            makeConstraintChannel(Channel::ConstraintCurrentLength, "constraintCurrentLength", "约束-当前长度", "m"),
            makeConstraintChannel(Channel::ConstraintError, "constraintError", "约束-误差", "m"),
            makeConstraintChannel(Channel::ConstraintForce, "constraintForce", "约束-力大小", "N"),
            makeFieldChannel(Channel::FieldVectorX, "fieldVectorX", "力场-矢量-X", ""),
            makeFieldChannel(Channel::FieldVectorY, "fieldVectorY", "力场-矢量-Y", ""),
            makeFieldChannel(Channel::FieldVectorZ, "fieldVectorZ", "力场-矢量-Z", ""),
            makeFieldChannel(Channel::FieldStrength, "fieldStrength", "力场-场强", ""),
            makeBodyChannel(Channel::PositionMagnitude, "positionMagnitude", "位置模长", "m", true),
            makeBodyChannel(Channel::Speed, "speed", "速率", "m/s", true),
            makeBodyChannel(Channel::AngularSpeed, "angularSpeed", "角速度模长", "rad/s", true),
            makeBodyChannel(Channel::EulerRoll, "eulerRoll", "姿态-横滚角", "rad", true),
            makeBodyChannel(Channel::EulerPitch, "eulerPitch", "姿态-俯仰角", "rad", true),
            makeBodyChannel(Channel::EulerYaw, "eulerYaw", "姿态-偏航角", "rad", true),
            makeBodyChannel(Channel::KineticTranslational, "kineticTranslational", "平动动能", "J", true),
        };

        static_assert(kChannelTable.size() == kChannelCount,
                      "通道元信息表与 Channel 枚举长度不一致，请同步补充");
    }

    const ChannelInfo& channelInfo(const Channel channel) noexcept
    {
        const auto index = static_cast<std::size_t>(channel);
        if (index >= kChannelTable.size())
        {
            return kUnknownChannelInfo;
        }
        return kChannelTable[index];
    }

    bool isEntityChannel(const Channel channel) noexcept
    {
        switch (channel)
        {
        case Channel::TimeSeconds:
        case Channel::FrameIndex:
        case Channel::StepIndex:
            return false;
        default:
            return true;
        }
    }

    const ExtractedSeries* ExtractionResult::find(const EntityRef& entity, const Channel channel) const
    {
        for (const ExtractedSeries& series : series)
        {
            if (series.entity == entity && series.channel == channel)
            {
                return &series;
            }
        }
        return nullptr;
    }
}
