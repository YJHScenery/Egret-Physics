/**
 * @file        epsx_channel_catalog.h
 * @brief       从 EPSX 场景描述块（SCNE）解析出的实体清单与通道可用性目录。
 * @details     SceneCatalog 是「界面上能选什么」的唯一依据：
 *
 *              - 实体名称、质量、恢复系数、是否固定等显示信息；
 *              - 采样配置（求解步长、采样间隔、抽帧比、声明帧数、总时长）；
 *              - 帧段声明（是否含 BODY / CONS / FLDS），用于判断约束与力场通道是否存在；
 *              - 解析过程中的告警（例如缺少 models 数组、ID 用 double 表示可能失真）。
 *
 *              `models[]` 只存在于编辑器导出的场景 JSON 中；离线求解器写出的 EPSX
 *              其 SCNE 只有 `entities[]`（刚体）与 `frameSections[]`。因此本目录以
 *              `entities[]` 为刚体来源，`models[]` 仅作为约束 / 力场的补充来源。
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

#ifndef EGRET_PHYSICS_EPSX_CHANNEL_CATALOG_H
#define EGRET_PHYSICS_EPSX_CHANNEL_CATALOG_H

#include <QString>
#include <QStringList>
#include <QVector>

#include <cstdint>
#include <string>

#include "analysis_types.h"

namespace egret::analysis
{
    /**
     * @brief       场景描述块解析结果。
     * @details     所有字段都允许缺省：EPSX 读取器本身只依赖 `entities` 之外的少量键，
     *              这里的解析是「尽力而为」，缺键只记警告，不让整个文件打不开。
     */
    struct SceneCatalog
    {
        /** 格式标识（通常为 "EPSX"）。 */
        QString format{};

        /** 格式版本。 */
        QString formatVersion{};

        /** 生成者描述。 */
        QString generator{};

        /** 求解推进步长（纳秒）。 */
        std::uint64_t solverStepNs{0};

        /** 采样间隔（纳秒）。 */
        std::uint64_t sampleIntervalNs{0};

        /** 抽帧比。 */
        std::uint64_t decimation{0};

        /** 场景声明的帧数（实际帧数以读取器为准）。 */
        std::uint64_t declaredFrameCount{0};

        /** 声明总时长（纳秒）。 */
        std::uint64_t declaredDurationNs{0};

        /** 帧段声明：是否包含刚体段。 */
        bool hasBodySection{false};

        /** 帧段声明：是否包含约束段。 */
        bool hasConstraintSection{false};

        /** 帧段声明：是否包含力场段。 */
        bool hasFieldSection{false};

        /** 刚体清单。 */
        QVector<EntityDescriptor> bodies{};

        /** 约束清单（可能为空：离线求解器不写形状 / 约束描述）。 */
        QVector<EntityDescriptor> constraints{};

        /** 力场清单（可能为空）。 */
        QVector<EntityDescriptor> fields{};

        /** 解析告警，供界面提示使用。 */
        QStringList warnings{};

        /**
         * @brief  按类别取出实体清单。
         * @param  kind 实体类别。
         * @return 对应清单的常量引用。
         */
        [[nodiscard]] const QVector<EntityDescriptor>& entitiesOf(EntityKind kind) const;
    };

    /**
     * @brief  解析 SCNE 块的 JSON 文本。
     * @param  sceneJson SCNE 载荷原文（UTF-8）。
     * @param  out 解析结果，不可为 nullptr。
     * @param  errorMessage 失败时写入原因，可为 nullptr。
     * @return JSON 可解析且顶层为对象时返回 true；此时 out 中可能仍有告警。
     */
    [[nodiscard]] bool buildSceneCatalog(const std::string& sceneJson,
                                         SceneCatalog* out,
                                         QString* errorMessage);

    /**
     * @brief  列出某一类实体可选择的通道（含当前未采集的通道，界面据此置灰）。
     * @param  kind 实体类别。
     * @return 通道列表，顺序与 Channel 枚举一致。
     */
    [[nodiscard]] QVector<Channel> channelsFor(EntityKind kind);
}

#endif // EGRET_PHYSICS_EPSX_CHANNEL_CATALOG_H
