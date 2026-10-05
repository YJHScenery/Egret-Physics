/**
 * @file        epsx_series_extractor.cpp
 * @brief       EPSX 序列抽取实现。
 * @details     抽取流程：打开文件 → 解析场景目录 → 建立「实体 × 通道」累加器
 *              → 按抽帧比逐帧解码并写入列 → 在列上补齐派生量 → 收集结果。
 *
 *              性能约定：
 *              - 只解码被选中的槽位对应的记录，其余记录直接跳过；
 *              - 所有列在开读前一次性 reserve，避免逐点扩容；
 *              - 进度与取消每 kProgressInterval 帧检查一次，开销可忽略。
 *
 *              已知限制：本轮使用 EpsxReader::readFrame 逐帧读取（每帧含块头校验），
 *              单实体数十万帧可接受；更极端的帧数将在线性顺序读取 API 落地后改为流式解码。
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

#include "epsx_series_extractor.h"

#include "epsx_channel_catalog.h"
#include "epsx_reader.h"

#include <QHash>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <utility>

namespace egret::analysis
{
    namespace
    {
        using egret::epsx::BodyState;
        using egret::epsx::ConstraintState;
        using egret::epsx::EpsxReader;
        using egret::epsx::FieldState;
        using egret::epsx::FrameState;

        /** 进度与取消检查的帧间隔。 */
        constexpr std::uint64_t kProgressInterval{256};

        /** 单实体的通道累加器。 */
        struct EntityAccumulator
        {
            /** 实体引用。 */
            EntityRef ref{};

            /** 该实体需要写入的原始列。 */
            std::array<bool, kChannelCount> needed{};

            /** 该实体需要输出到结果的列（含派生列）。 */
            std::array<bool, kChannelCount> requested{};

            /** 列数据，下标为 Channel。 */
            std::array<Column, kChannelCount> columns{};
        };

        /**
         * @brief  三维向量模长。
         * @param  x X 分量。
         * @param  y Y 分量。
         * @param  z Z 分量。
         * @return 模长。
         */
        double vectorNorm(const double x, const double y, const double z)
        {
            return std::sqrt(x * x + y * y + z * z);
        }

        /**
         * @brief  把数值限制到 [-1, 1]，避免 asin 定义域越界。
         * @param  value 输入值。
         * @return 限制后的值。
         */
        double clampUnit(const double value)
        {
            return std::clamp(value, -1.0, 1.0);
        }

        /**
         * @brief       从四元数解算 ZYX 欧拉角。
         * @details     约定：先绕 Z 偏航、再绕 Y 俯仰、最后绕 X 横滚，
         *              与常见物理教材的 yaw-pitch-roll 一致。
         * @param  qx 四元数 x 分量。
         * @param  qy 四元数 y 分量。
         * @param  qz 四元数 z 分量。
         * @param  qw 四元数 w 分量。
         * @param  euler 输出数组，依次为 roll、pitch、yaw。
         */
        void eulerFromQuaternion(const double qx,
                                 const double qy,
                                 const double qz,
                                 const double qw,
                                 double* euler)
        {
            const double norm = std::sqrt(qx * qx + qy * qy + qz * qz + qw * qw);
            if (norm < 1e-12)
            {
                euler[0] = 0.0;
                euler[1] = 0.0;
                euler[2] = 0.0;
                return;
            }

            const double x = qx / norm;
            const double y = qy / norm;
            const double z = qz / norm;
            const double w = qw / norm;

            euler[0] = std::atan2(2.0 * (w * x + y * z), 1.0 - 2.0 * (x * x + y * y));
            euler[1] = std::asin(clampUnit(2.0 * (w * y - z * x)));
            euler[2] = std::atan2(2.0 * (w * z + x * y), 1.0 - 2.0 * (y * y + z * z));
        }

        /**
         * @brief  计算派生通道所需依赖的原始通道。
         * @param  channel 派生通道。
         * @param  needed 输出依赖集合，下标为 Channel。
         */
        void collectDerivedSources(const Channel channel, std::array<bool, kChannelCount>& needed)
        {
            const auto mark = [&needed](const Channel dependency)
            {
                needed[static_cast<std::size_t>(dependency)] = true;
            };

            switch (channel)
            {
            case Channel::PositionMagnitude:
                mark(Channel::PositionX);
                mark(Channel::PositionY);
                mark(Channel::PositionZ);
                break;
            case Channel::Speed:
                mark(Channel::LinearVelocityX);
                mark(Channel::LinearVelocityY);
                mark(Channel::LinearVelocityZ);
                break;
            case Channel::AngularSpeed:
                mark(Channel::AngularVelocityX);
                mark(Channel::AngularVelocityY);
                mark(Channel::AngularVelocityZ);
                break;
            case Channel::EulerRoll:
            case Channel::EulerPitch:
            case Channel::EulerYaw:
                mark(Channel::RotationX);
                mark(Channel::RotationY);
                mark(Channel::RotationZ);
                mark(Channel::RotationW);
                break;
            case Channel::KineticTranslational:
                mark(Channel::Mass);
                mark(Channel::LinearVelocityX);
                mark(Channel::LinearVelocityY);
                mark(Channel::LinearVelocityZ);
                break;
            default:
                break;
            }
        }

        /**
         * @brief  读取刚体记录中的原始通道值。
         * @param  channel 通道。
         * @param  body 刚体记录。
         * @return 通道值；非刚体原始通道返回 0。
         */
        double bodyChannelValue(const Channel channel, const BodyState& body)
        {
            switch (channel)
            {
            case Channel::PositionX:
                return body.position.x();
            case Channel::PositionY:
                return body.position.y();
            case Channel::PositionZ:
                return body.position.z();
            case Channel::RotationX:
                return body.rotation.x();
            case Channel::RotationY:
                return body.rotation.y();
            case Channel::RotationZ:
                return body.rotation.z();
            case Channel::RotationW:
                return body.rotation.w();
            case Channel::LinearVelocityX:
                return body.linearVelocity.x();
            case Channel::LinearVelocityY:
                return body.linearVelocity.y();
            case Channel::LinearVelocityZ:
                return body.linearVelocity.z();
            case Channel::AngularVelocityX:
                return body.angularVelocity.x();
            case Channel::AngularVelocityY:
                return body.angularVelocity.y();
            case Channel::AngularVelocityZ:
                return body.angularVelocity.z();
            case Channel::Mass:
                return body.mass;
            case Channel::Restitution:
                return body.restitution;
            case Channel::BodyStateFlags:
                return static_cast<double>(body.stateFlags);
            default:
                return 0.0;
            }
        }

        /**
         * @brief  读取约束记录中的原始通道值。
         * @param  channel 通道。
         * @param  constraint 约束记录。
         * @return 通道值。
         */
        double constraintChannelValue(const Channel channel, const ConstraintState& constraint)
        {
            switch (channel)
            {
            case Channel::ConstraintRestLength:
                return constraint.restLength;
            case Channel::ConstraintCurrentLength:
                return constraint.currentLength;
            case Channel::ConstraintError:
                return constraint.error;
            case Channel::ConstraintForce:
                return constraint.forceMagnitude;
            default:
                return 0.0;
            }
        }

        /**
         * @brief  读取力场记录中的原始通道值。
         * @param  channel 通道。
         * @param  field 力场记录。
         * @return 通道值。
         */
        double fieldChannelValue(const Channel channel, const FieldState& field)
        {
            switch (channel)
            {
            case Channel::FieldVectorX:
                return field.vector.x();
            case Channel::FieldVectorY:
                return field.vector.y();
            case Channel::FieldVectorZ:
                return field.vector.z();
            case Channel::FieldStrength:
                return field.strength;
            default:
                return 0.0;
            }
        }

        /**
         * @brief  在列上补齐派生通道。
         * @param  accumulator 实体累加器。
         * @param  sampleCount 实际样本数。
         */
        void computeDerivedColumns(EntityAccumulator& accumulator, const std::size_t sampleCount)
        {
            const auto columnOf = [&accumulator](const Channel channel) -> const Column&
            {
                return accumulator.columns[static_cast<std::size_t>(channel)];
            };
            const auto isFilled = [&accumulator](const Channel channel) -> bool
            {
                return accumulator.needed[static_cast<std::size_t>(channel)];
            };

            const auto fill = [&accumulator, sampleCount](const Channel channel) -> Column&
            {
                Column& column = accumulator.columns[static_cast<std::size_t>(channel)];
                column.resize(sampleCount);
                return column;
            };

            const bool hasPosition = isFilled(Channel::PositionX) && isFilled(Channel::PositionY) &&
                                     isFilled(Channel::PositionZ);
            const bool hasVelocity = isFilled(Channel::LinearVelocityX) &&
                                     isFilled(Channel::LinearVelocityY) &&
                                     isFilled(Channel::LinearVelocityZ);
            const bool hasAngular = isFilled(Channel::AngularVelocityX) &&
                                    isFilled(Channel::AngularVelocityY) &&
                                    isFilled(Channel::AngularVelocityZ);
            const bool hasRotation = isFilled(Channel::RotationX) && isFilled(Channel::RotationY) &&
                                     isFilled(Channel::RotationZ) && isFilled(Channel::RotationW);

            if (accumulator.requested[static_cast<std::size_t>(Channel::PositionMagnitude)] && hasPosition)
            {
                Column& target = fill(Channel::PositionMagnitude);
                for (std::size_t i = 0; i < sampleCount; ++i)
                {
                    target[i] = vectorNorm(columnOf(Channel::PositionX)[i],
                                           columnOf(Channel::PositionY)[i],
                                           columnOf(Channel::PositionZ)[i]);
                }
            }

            if (accumulator.requested[static_cast<std::size_t>(Channel::Speed)] && hasVelocity)
            {
                Column& target = fill(Channel::Speed);
                for (std::size_t i = 0; i < sampleCount; ++i)
                {
                    target[i] = vectorNorm(columnOf(Channel::LinearVelocityX)[i],
                                           columnOf(Channel::LinearVelocityY)[i],
                                           columnOf(Channel::LinearVelocityZ)[i]);
                }
            }

            if (accumulator.requested[static_cast<std::size_t>(Channel::AngularSpeed)] && hasAngular)
            {
                Column& target = fill(Channel::AngularSpeed);
                for (std::size_t i = 0; i < sampleCount; ++i)
                {
                    target[i] = vectorNorm(columnOf(Channel::AngularVelocityX)[i],
                                           columnOf(Channel::AngularVelocityY)[i],
                                           columnOf(Channel::AngularVelocityZ)[i]);
                }
            }

            const bool wantRoll = accumulator.requested[static_cast<std::size_t>(Channel::EulerRoll)];
            const bool wantPitch = accumulator.requested[static_cast<std::size_t>(Channel::EulerPitch)];
            const bool wantYaw = accumulator.requested[static_cast<std::size_t>(Channel::EulerYaw)];
            if ((wantRoll || wantPitch || wantYaw) && hasRotation)
            {
                Column* rollColumn = wantRoll ? &fill(Channel::EulerRoll) : nullptr;
                Column* pitchColumn = wantPitch ? &fill(Channel::EulerPitch) : nullptr;
                Column* yawColumn = wantYaw ? &fill(Channel::EulerYaw) : nullptr;
                for (std::size_t i = 0; i < sampleCount; ++i)
                {
                    double euler[3]{0.0, 0.0, 0.0};
                    eulerFromQuaternion(columnOf(Channel::RotationX)[i],
                                        columnOf(Channel::RotationY)[i],
                                        columnOf(Channel::RotationZ)[i],
                                        columnOf(Channel::RotationW)[i],
                                        euler);
                    if (rollColumn != nullptr)
                    {
                        (*rollColumn)[i] = euler[0];
                    }
                    if (pitchColumn != nullptr)
                    {
                        (*pitchColumn)[i] = euler[1];
                    }
                    if (yawColumn != nullptr)
                    {
                        (*yawColumn)[i] = euler[2];
                    }
                }
            }

            if (accumulator.requested[static_cast<std::size_t>(Channel::KineticTranslational)] &&
                hasVelocity && isFilled(Channel::Mass))
            {
                Column& target = fill(Channel::KineticTranslational);
                for (std::size_t i = 0; i < sampleCount; ++i)
                {
                    const double speed = vectorNorm(columnOf(Channel::LinearVelocityX)[i],
                                                    columnOf(Channel::LinearVelocityY)[i],
                                                    columnOf(Channel::LinearVelocityZ)[i]);
                    target[i] = 0.5 * columnOf(Channel::Mass)[i] * speed * speed;
                }
            }
        }
    }

    bool EpsxSeriesExtractor::extract(const ExtractionRequest& request,
                                      const ExtractionCallbacks& callbacks,
                                      ExtractionResult* out,
                                      ExtractionReport* report,
                                      QString* errorMessage)
    {
        const auto startedAt = std::chrono::steady_clock::now();

        if (out == nullptr)
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = QStringLiteral("抽取结果输出指针为空");
            }
            return false;
        }
        *out = ExtractionResult{};
        if (report != nullptr)
        {
            *report = ExtractionReport{};
        }

        const auto fail = [errorMessage](const QString& message)
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = message;
            }
            return false;
        };

        if (request.filePath.trimmed().isEmpty())
        {
            return fail(QStringLiteral("未指定 EPSX 文件路径"));
        }
        if (request.stride == 0)
        {
            return fail(QStringLiteral("抽帧比必须大于等于 1"));
        }
        if (request.entities.isEmpty() && !request.includeAllConstraints && !request.includeAllFields)
        {
            return fail(QStringLiteral("没有选择任何需要分析的实体"));
        }
        if (request.channels.isEmpty())
        {
            return fail(QStringLiteral("没有选择任何需要分析的通道"));
        }

        // ---- 1. 打开文件并解析场景目录 ----
        EpsxReader reader(request.filePath);
        QString openError{};
        if (!reader.open(&openError))
        {
            return fail(openError.isEmpty() ? QStringLiteral("无法打开 EPSX 文件") : openError);
        }

        SceneCatalog catalog{};
        QString catalogError{};
        if (!buildSceneCatalog(reader.sceneJson(), &catalog, &catalogError))
        {
            // 目录解析失败不致命：退化为「只按用户选择的实体与 ID 抽取」
            catalog = SceneCatalog{};
        }

        const std::uint64_t totalFrames = reader.frameCount();
        if (totalFrames == 0)
        {
            return fail(QStringLiteral("文件中没有可读取的帧"));
        }

        out->sourcePath = request.filePath;
        out->generator = catalog.generator;
        out->totalFrameCount = totalFrames;
        out->stride = request.stride;
        out->sampleIntervalNs =
            reader.sampleIntervalNs() != 0 ? reader.sampleIntervalNs() : catalog.sampleIntervalNs;
        out->solverStepNs = catalog.solverStepNs;

        const std::uint64_t plannedSamples = (totalFrames - 1) / request.stride + 1;

        // ---- 2. 建立实体清单（含按需自动发现的约束 / 力场）----
        QVector<EntityDescriptor> entityList = catalog.bodies;
        QVector<EntityDescriptor> constraintList = catalog.constraints;
        QVector<EntityDescriptor> fieldList = catalog.fields;

        bool needConstraintDiscovery = request.includeAllConstraints;
        bool needFieldDiscovery = request.includeAllFields;

        if (needConstraintDiscovery || needFieldDiscovery)
        {
            FrameState probe{};
            QString probeError{};
            if (!reader.readFrame(0, &probe, &probeError))
            {
                return fail(probeError.isEmpty() ? QStringLiteral("无法读取第 0 帧用于识别实体")
                                                 : probeError);
            }

            if (needConstraintDiscovery)
            {
                QHash<qulonglong, bool> known{};
                for (const EntityDescriptor& descriptor : constraintList)
                {
                    known.insert(static_cast<qulonglong>(descriptor.ref.id), true);
                }
                for (const ConstraintState& state : probe.constraints)
                {
                    if (known.contains(static_cast<qulonglong>(state.id)))
                    {
                        continue;
                    }
                    EntityDescriptor descriptor{};
                    descriptor.ref.kind = EntityKind::Constraint;
                    descriptor.ref.id = state.id;
                    descriptor.ref.slot = state.slot;
                    descriptor.type = state.type;
                    descriptor.name = QStringLiteral("约束 #%1（类型 %2）").arg(state.id).arg(state.type);
                    constraintList.append(descriptor);
                    known.insert(static_cast<qulonglong>(state.id), true);
                }
            }

            if (needFieldDiscovery)
            {
                QHash<qulonglong, bool> known{};
                for (const EntityDescriptor& descriptor : fieldList)
                {
                    known.insert(static_cast<qulonglong>(descriptor.ref.id), true);
                }
                for (const FieldState& state : probe.fields)
                {
                    if (known.contains(static_cast<qulonglong>(state.id)))
                    {
                        continue;
                    }
                    EntityDescriptor descriptor{};
                    descriptor.ref.kind = EntityKind::Field;
                    descriptor.ref.id = state.id;
                    descriptor.ref.slot = state.slot;
                    descriptor.type = state.type;
                    descriptor.name = QStringLiteral("力场 #%1（类型 %2）").arg(state.id).arg(state.type);
                    fieldList.append(descriptor);
                    known.insert(static_cast<qulonglong>(state.id), true);
                }
            }
        }

        // ---- 3. 组装待抽取实体：请求显式指定的 + 自动纳入的 ----
        QVector<EntityDescriptor> selected{};
        QHash<qulonglong, bool> selectedBodyIds{};
        for (const EntityRef& ref : request.entities)
        {
            EntityDescriptor descriptor{};
            descriptor.ref = ref;

            const QVector<EntityDescriptor>& pool = (ref.kind == EntityKind::Body)     ? entityList
                : (ref.kind == EntityKind::Constraint)                                 ? constraintList
                                                                                       : fieldList;
            for (const EntityDescriptor& candidate : pool)
            {
                if (candidate.ref.id == ref.id)
                {
                    descriptor = candidate;
                    break;
                }
            }
            descriptor.ref = ref;
            if (descriptor.name.isEmpty())
            {
                descriptor.name = QStringLiteral("%1 #%2")
                                      .arg(QString::fromUtf8(entityKindDisplayName(ref.kind)))
                                      .arg(ref.id);
            }
            if (ref.kind == EntityKind::Body && selectedBodyIds.contains(static_cast<qulonglong>(ref.id)))
            {
                continue;
            }
            if (ref.kind == EntityKind::Body)
            {
                selectedBodyIds.insert(static_cast<qulonglong>(ref.id), true);
            }
            selected.append(descriptor);
        }

        if (request.includeAllConstraints)
        {
            for (const EntityDescriptor& descriptor : constraintList)
            {
                selected.append(descriptor);
            }
        }
        if (request.includeAllFields)
        {
            for (const EntityDescriptor& descriptor : fieldList)
            {
                selected.append(descriptor);
            }
        }

        if (selected.isEmpty())
        {
            return fail(QStringLiteral("按当前选择没有匹配到任何实体"));
        }

        // ---- 4. 建立累加器 ----
        std::array<bool, kChannelCount> requestedChannels{};
        for (const Channel channel : request.channels)
        {
            if (!isEntityChannel(channel))
            {
                continue;
            }
            requestedChannels[static_cast<std::size_t>(channel)] = true;
        }

        std::vector<EntityAccumulator> accumulators{};
        accumulators.reserve(selected.size());
        QHash<qulonglong, int> bodyIndexById{};
        QHash<qulonglong, int> constraintIndexById{};
        QHash<qulonglong, int> fieldIndexById{};

        for (const EntityDescriptor& descriptor : selected)
        {
            EntityAccumulator accumulator{};
            accumulator.ref = descriptor.ref;

            for (std::size_t index = 0; index < kChannelCount; ++index)
            {
                const auto channel = static_cast<Channel>(index);
                if (!requestedChannels[index] || channelInfo(channel).owner != descriptor.ref.kind)
                {
                    continue;
                }
                accumulator.requested[index] = true;
                if (channelInfo(channel).derived)
                {
                    collectDerivedSources(channel, accumulator.needed);
                }
                else
                {
                    accumulator.needed[index] = true;
                }
            }

            bool hasAnyRequested = false;
            for (const bool flag : accumulator.requested)
            {
                hasAnyRequested = hasAnyRequested || flag;
            }
            if (!hasAnyRequested)
            {
                continue;
            }

            for (std::size_t index = 0; index < kChannelCount; ++index)
            {
                if (accumulator.needed[index])
                {
                    accumulator.columns[index].reserve(static_cast<std::size_t>(plannedSamples));
                }
                if (accumulator.requested[index] && !accumulator.needed[index])
                {
                    accumulator.columns[index].reserve(static_cast<std::size_t>(plannedSamples));
                }
            }

            const int accumulatorIndex = static_cast<int>(accumulators.size());
            accumulators.push_back(std::move(accumulator));
            switch (descriptor.ref.kind)
            {
            case EntityKind::Constraint:
                constraintIndexById.insert(static_cast<qulonglong>(descriptor.ref.id), accumulatorIndex);
                break;
            case EntityKind::Field:
                fieldIndexById.insert(static_cast<qulonglong>(descriptor.ref.id), accumulatorIndex);
                break;
            case EntityKind::Body:
            default:
                bodyIndexById.insert(static_cast<qulonglong>(descriptor.ref.id), accumulatorIndex);
                break;
            }
        }

        if (accumulators.empty())
        {
            return fail(QStringLiteral("所选通道与所选实体类别不匹配"));
        }

        // ---- 5. 逐帧解码并写入列 ----
        out->time.reserve(static_cast<std::size_t>(plannedSamples));
        out->frameIndex.reserve(static_cast<std::size_t>(plannedSamples));
        out->stepIndex.reserve(static_cast<std::size_t>(plannedSamples));

        bool cancelled = false;
        for (std::uint64_t sample = 0; sample < plannedSamples; ++sample)
        {
            if (callbacks.cancelled && callbacks.cancelled())
            {
                cancelled = true;
                break;
            }

            const std::uint64_t frameIndexValue = sample * request.stride;
            if (frameIndexValue >= totalFrames)
            {
                break;
            }

            FrameState frame{};
            QString frameError{};
            if (!reader.readFrame(frameIndexValue, &frame, &frameError))
            {
                return fail(frameError.isEmpty()
                                ? QStringLiteral("读取第 %1 帧失败").arg(frameIndexValue)
                                : frameError);
            }

            out->time.push_back(epsx::nanosecondsToSeconds(frame.simTimeNs));
            out->frameIndex.push_back(frame.frameIndex);
            out->stepIndex.push_back(frame.stepIndex);

            for (const BodyState& body : frame.bodies)
            {
                const auto found = bodyIndexById.constFind(static_cast<qulonglong>(body.id));
                if (found == bodyIndexById.constEnd())
                {
                    continue;
                }
                EntityAccumulator& accumulator = accumulators[found.value()];
                for (std::size_t index = 0; index < kChannelCount; ++index)
                {
                    if (!accumulator.needed[index])
                    {
                        continue;
                    }
                    const auto channel = static_cast<Channel>(index);
                    if (channelInfo(channel).derived)
                    {
                        continue;
                    }
                    accumulator.columns[index].pushBack(bodyChannelValue(channel, body));
                }
            }

            for (const ConstraintState& constraint : frame.constraints)
            {
                const auto found = constraintIndexById.constFind(static_cast<qulonglong>(constraint.id));
                if (found == constraintIndexById.constEnd())
                {
                    continue;
                }
                EntityAccumulator& accumulator = accumulators[found.value()];
                for (std::size_t index = 0; index < kChannelCount; ++index)
                {
                    if (!accumulator.needed[index])
                    {
                        continue;
                    }
                    const auto channel = static_cast<Channel>(index);
                    if (channelInfo(channel).derived)
                    {
                        continue;
                    }
                    accumulator.columns[index].pushBack(constraintChannelValue(channel, constraint));
                }
            }

            for (const FieldState& field : frame.fields)
            {
                const auto found = fieldIndexById.constFind(static_cast<qulonglong>(field.id));
                if (found == fieldIndexById.constEnd())
                {
                    continue;
                }
                EntityAccumulator& accumulator = accumulators[found.value()];
                for (std::size_t index = 0; index < kChannelCount; ++index)
                {
                    if (!accumulator.needed[index])
                    {
                        continue;
                    }
                    const auto channel = static_cast<Channel>(index);
                    if (channelInfo(channel).derived)
                    {
                        continue;
                    }
                    accumulator.columns[index].pushBack(fieldChannelValue(channel, field));
                }
            }

            if (callbacks.progress && (sample % kProgressInterval == 0))
            {
                callbacks.progress(frameIndexValue, totalFrames);
            }
        }

        const std::size_t actualSamples = out->time.size();
        out->sampledCount = actualSamples;

        // ---- 6. 补齐派生列并收集结果 ----
        for (EntityAccumulator& accumulator : accumulators)
        {
            computeDerivedColumns(accumulator, actualSamples);
        }

        for (EntityAccumulator& accumulator : accumulators)
        {
            for (std::size_t index = 0; index < kChannelCount; ++index)
            {
                if (!accumulator.requested[index])
                {
                    continue;
                }
                // 请求了派生通道但依赖不足时，列长度为 0，跳过以免出现长度不一致的序列
                if (accumulator.columns[index].size() != actualSamples)
                {
                    continue;
                }
                ExtractedSeries series{};
                series.entity = accumulator.ref;
                series.channel = static_cast<Channel>(index);
                series.values = std::move(accumulator.columns[index]);
                out->series.append(std::move(series));
            }
        }

        // 只保留真正产出序列的实体，保证「实体清单」与「序列集合」一致
        for (const EntityDescriptor& descriptor : selected)
        {
            for (const ExtractedSeries& series : out->series)
            {
                if (series.entity == descriptor.ref)
                {
                    out->entities.append(descriptor);
                    break;
                }
            }
        }

        if (callbacks.progress)
        {
            callbacks.progress(cancelled ? out->sampledCount * request.stride : totalFrames, totalFrames);
        }

        if (report != nullptr)
        {
            report->decodedFrames = actualSamples;
            report->sampledCount = actualSamples;
            report->cancelled = cancelled;
            report->elapsedSeconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - startedAt).count();
        }
        return true;
    }
}
