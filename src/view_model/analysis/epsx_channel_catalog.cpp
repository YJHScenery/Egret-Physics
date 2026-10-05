/**
 * @file        epsx_channel_catalog.cpp
 * @brief       EPSX 场景描述块（SCNE）解析实现。
 * @details     解析遵循「尽力而为」原则：任何键缺失只记录告警并给出默认值，
 *              只有 JSON 本身无法解析时才返回失败。这样即使遇到由其他工具生成、
 *              字段不完整的 EPSX 文件，数据分析工作区仍然能列出可用的实体与通道。
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

#include "epsx_channel_catalog.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

namespace egret::analysis
{
    namespace
    {
        /**
         * @brief  把 JSON 值当作无符号整数读取。
         * @param  value JSON 值。
         * @return 字符串按十进制解析；数值直接截断取整；其他类型返回 0。
         */
        std::uint64_t jsonToU64(const QJsonValue& value)
        {
            if (value.isString())
            {
                bool ok = false;
                const std::uint64_t parsed = value.toString().toULongLong(&ok);
                return ok ? parsed : 0ull;
            }
            if (value.isDouble())
            {
                const double number = value.toDouble();
                return number > 0.0 ? static_cast<std::uint64_t>(number) : 0ull;
            }
            return 0ull;
        }

        /**
         * @brief  读取对象中的无符号整数字段。
         * @param  object JSON 对象。
         * @param  key 键名。
         * @param  fallback 缺省值。
         * @return 字段值；缺键或类型不符时返回 fallback。
         */
        std::uint64_t readU64(const QJsonObject& object, const QString& key, const std::uint64_t fallback)
        {
            const QJsonValue value = object.value(key);
            if (value.isUndefined() || value.isNull())
            {
                return fallback;
            }
            return jsonToU64(value);
        }

        /**
         * @brief  读取对象中的 double 字段。
         * @param  object JSON 对象。
         * @param  key 键名。
         * @param  fallback 缺省值。
         * @return 字段值；缺键或类型不符时返回 fallback。
         */
        double readDouble(const QJsonObject& object, const QString& key, const double fallback)
        {
            const QJsonValue value = object.value(key);
            if (value.isDouble())
            {
                return value.toDouble();
            }
            if (value.isString())
            {
                bool ok = false;
                const double parsed = value.toString().toDouble(&ok);
                return ok ? parsed : fallback;
            }
            return fallback;
        }

        /**
         * @brief  读取对象中的布尔字段。
         * @param  object JSON 对象。
         * @param  key 键名。
         * @param  fallback 缺省值。
         * @return 字段值；缺键或类型不符时返回 fallback。
         */
        bool readBool(const QJsonObject& object, const QString& key, const bool fallback)
        {
            const QJsonValue value = object.value(key);
            return value.isBool() ? value.toBool() : fallback;
        }

        /**
         * @brief  读取对象中的字符串字段。
         * @param  object JSON 对象。
         * @param  key 键名。
         * @return 字段值；缺键或类型不符时返回空串。
         */
        QString readString(const QJsonObject& object, const QString& key)
        {
            const QJsonValue value = object.value(key);
            if (value.isString())
            {
                return value.toString();
            }
            if (value.isDouble())
            {
                return QString::number(value.toDouble(), 'g', 17);
            }
            return QString{};
        }

        /**
         * @brief  生成实体缺省名称。
         * @param  kind 实体类别。
         * @param  id 实体 ID。
         * @return 形如「刚体 #12」的名称。
         */
        QString fallbackName(const EntityKind kind, const std::uint64_t id)
        {
            return QStringLiteral("%1 #%2")
                .arg(QString::fromUtf8(entityKindDisplayName(kind)))
                .arg(id);
        }

        /**
         * @brief  从 `entities[]` 的单条记录构造刚体描述。
         * @param  object JSON 对象。
         * @return 实体描述。
         */
        EntityDescriptor parseEntityEntry(const QJsonObject& object)
        {
            EntityDescriptor descriptor{};
            descriptor.ref.kind = EntityKind::Body;
            descriptor.ref.id = readU64(object, QStringLiteral("id"), 0ull);
            descriptor.ref.slot = static_cast<std::uint32_t>(readU64(object, QStringLiteral("slot"), 0ull));
            descriptor.name = readString(object, QStringLiteral("name"));
            descriptor.mass = readDouble(object, QStringLiteral("mass"), 0.0);
            descriptor.restitution = readDouble(object, QStringLiteral("restitution"), 1.0);
            descriptor.fixed = readBool(object, QStringLiteral("fixed"), false);
            if (descriptor.name.isEmpty())
            {
                descriptor.name = fallbackName(EntityKind::Body, descriptor.ref.id);
            }
            return descriptor;
        }

        /**
         * @brief  从 `models[]` 的单条记录构造实体描述。
         * @param  object JSON 对象。
         * @param  outKind 输出该记录对应的实体类别。
         * @return 类别可识别时返回 true。
         */
        bool parseModelEntry(const QJsonObject& object, EntityDescriptor* descriptor, EntityKind* outKind)
        {
            const int category = static_cast<int>(readU64(object, QStringLiteral("category"), 0ull));
            EntityKind kind = EntityKind::Body;
            if (category == 1)
            {
                kind = EntityKind::Field;
            }
            else if (category == 2)
            {
                kind = EntityKind::Constraint;
            }
            else if (category != 0)
            {
                return false;
            }

            descriptor->ref.kind = kind;
            descriptor->ref.id = readU64(object, QStringLiteral("id"), 0ull);
            descriptor->ref.slot = static_cast<std::uint32_t>(readU64(object, QStringLiteral("slot"), 0ull));
            descriptor->name = readString(object, QStringLiteral("name"));
            descriptor->mass = readDouble(object, QStringLiteral("mass"), 0.0);
            descriptor->restitution = readDouble(object, QStringLiteral("restitution"), 1.0);
            descriptor->fixed = readBool(object, QStringLiteral("fixed"), false);
            descriptor->type = static_cast<std::uint32_t>(readU64(object, QStringLiteral("type"), 0ull));
            if (descriptor->name.isEmpty())
            {
                descriptor->name = fallbackName(kind, descriptor->ref.id);
            }
            *outKind = kind;
            return true;
        }
    }

    const QVector<EntityDescriptor>& SceneCatalog::entitiesOf(const EntityKind kind) const
    {
        switch (kind)
        {
        case EntityKind::Constraint:
            return constraints;
        case EntityKind::Field:
            return fields;
        case EntityKind::Body:
        default:
            return bodies;
        }
    }

    bool buildSceneCatalog(const std::string& sceneJson, SceneCatalog* out, QString* errorMessage)
    {
        if (out == nullptr)
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = QStringLiteral("场景目录输出指针为空");
            }
            return false;
        }

        *out = SceneCatalog{};

        QJsonParseError parseError{};
        const QJsonDocument document =
            QJsonDocument::fromJson(QByteArray::fromStdString(sceneJson), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = QStringLiteral("场景描述块不是合法 JSON：%1").arg(parseError.errorString());
            }
            return false;
        }

        const QJsonObject root = document.object();
        out->format = readString(root, QStringLiteral("format"));
        out->formatVersion = readString(root, QStringLiteral("formatVersion"));
        out->generator = readString(root, QStringLiteral("generator"));

        // ---- 采样配置 ----
        const QJsonObject sampling = root.value(QStringLiteral("sampling")).toObject();
        out->solverStepNs = readU64(sampling, QStringLiteral("solverStepNs"), 0ull);
        out->sampleIntervalNs = readU64(sampling, QStringLiteral("intervalNs"), 0ull);
        out->decimation = readU64(sampling, QStringLiteral("decimation"), 0ull);
        out->declaredFrameCount = readU64(sampling, QStringLiteral("frameCount"), 0ull);
        out->declaredDurationNs = readU64(sampling, QStringLiteral("durationNs"), 0ull);

        // ---- 帧段声明 ----
        const QJsonArray sections = root.value(QStringLiteral("frameSections")).toArray();
        for (const QJsonValueConstRef& value : sections)
        {
            const QString type = readString(value.toObject(), QStringLiteral("type"));
            if (type == QStringLiteral("BODY"))
            {
                out->hasBodySection = true;
            }
            else if (type == QStringLiteral("CONS"))
            {
                out->hasConstraintSection = true;
            }
            else if (type == QStringLiteral("FLDS"))
            {
                out->hasFieldSection = true;
            }
        }

        // ---- 刚体清单：优先 entities[] ----
        const QJsonArray entities = root.value(QStringLiteral("entities")).toArray();
        for (const QJsonValueConstRef& value : entities)
        {
            if (!value.isObject())
            {
                continue;
            }
            out->bodies.append(parseEntityEntry(value.toObject()));
        }

        // ---- 约束 / 力场清单：来自 models[]（若存在） ----
        const QJsonArray models = root.value(QStringLiteral("models")).toArray();
        for (const QJsonValueConstRef& value : models)
        {
            if (!value.isObject())
            {
                continue;
            }
            EntityDescriptor descriptor{};
            EntityKind kind = EntityKind::Body;
            if (!parseModelEntry(value.toObject(), &descriptor, &kind))
            {
                continue;
            }
            switch (kind)
            {
            case EntityKind::Constraint:
                out->constraints.append(descriptor);
                break;
            case EntityKind::Field:
                out->fields.append(descriptor);
                break;
            case EntityKind::Body:
            default:
                if (out->bodies.isEmpty())
                {
                    out->bodies.append(descriptor);
                }
                break;
            }
        }

        // ---- 一致性告警 ----
        if (out->bodies.isEmpty() && out->hasBodySection)
        {
            out->warnings.append(QStringLiteral("文件包含刚体帧段，但场景描述中没有实体清单。"));
        }
        if (out->constraints.isEmpty() && out->hasConstraintSection)
        {
            out->warnings.append(
                QStringLiteral("文件包含约束帧段，但场景描述未提供约束名称与类型，"
                               "约束实体将在抽取时按帧内记录自动识别。"));
        }
        if (out->fields.isEmpty() && out->hasFieldSection)
        {
            out->warnings.append(
                QStringLiteral("文件包含力场帧段，但场景描述未提供力场名称与类型，"
                               "力场实体将在抽取时按帧内记录自动识别。"));
        }
        return true;
    }

    QVector<Channel> channelsFor(const EntityKind kind)
    {
        QVector<Channel> channels{};
        channels.reserve(static_cast<qsizetype>(kChannelCount));
        for (std::size_t index = 0; index < kChannelCount; ++index)
        {
            const auto channel = static_cast<Channel>(index);
            const ChannelInfo& info = channelInfo(channel);
            if (!isEntityChannel(channel))
            {
                continue;
            }
            if (info.owner == kind)
            {
                channels.append(channel);
            }
        }
        return channels;
    }
}
