/**
 * @file        parse_command.cpp
 * @brief       --parse 功能的实现。
 * @details     打开 EPSX 文件、读取指定帧，并把帧内刚体 / 约束 / 力场数据
 *              整理成单行 JSON 输出，供脚本与人工核对求解结果。
 *
 * @author      jehor
 * @date        2026-10-02
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     Utils
 * @defgroup    Command (命令行接口)
 */

#include "parse_command.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <cstdio>

#include "epsx_reader.h"
#include "offline_simulation_runner.h"

namespace egret::command
{
    namespace
    {
        /**
         * @brief  把三维向量转为 JSON 数组。
         * @param  value 向量。
         * @return JSON 数组。
         */
        QJsonArray toJsonArray(const Eigen::Vector3d& value)
        {
            QJsonArray array{};
            array.append(value.x());
            array.append(value.y());
            array.append(value.z());
            return array;
        }
    }

    int runParse(const Request& request)
    {
        QTextStream out(stdout);
        QTextStream err(stderr);

        if (request.parseTarget != QStringLiteral("epsx")) {
            err << "不支持的解析对象：" << request.parseTarget << "（当前仅支持 epsx）\n";
            err.flush();
            return 2;
        }

        const QString path = request.parseFile.isEmpty()
                                 ? OfflineSimulationRunner::defaultSimulationFilePath()
                                 : request.parseFile;

        epsx::EpsxReader reader(path);
        QString error;
        if (!reader.open(&error)) {
            err << error << "\n";
            err.flush();
            return 1;
        }

        if (request.parseFrame < 0 ||
            static_cast<std::uint64_t>(request.parseFrame) >= reader.frameCount()) {
            err << "帧号 " << request.parseFrame << " 越界（文件共 " << reader.frameCount() << " 帧）\n";
            err.flush();
            return 1;
        }

        epsx::FrameState frame{};
        if (!reader.readFrame(static_cast<std::uint64_t>(request.parseFrame), &frame, &error)) {
            err << error << "\n";
            err.flush();
            return 1;
        }

        // 从场景描述块取出槽位到名称的映射，便于阅读输出。
        QHash<int, QString> slotNames{};
        QHash<int, QJsonObject> slotMaterials{};
        QHash<QString, QJsonObject> modelMaterials{};
        const QJsonObject sceneRoot =
            QJsonDocument::fromJson(QByteArray::fromStdString(reader.sceneJson())).object();
        for (const QJsonValueConstRef& value : sceneRoot.value(QStringLiteral("models")).toArray()) {
            const QJsonObject model = value.toObject();
            modelMaterials.insert(model.value(QStringLiteral("id")).toString(), model);
        }
        for (const QJsonValueConstRef& value : sceneRoot.value(QStringLiteral("entities")).toArray()) {
            const QJsonObject entity = value.toObject();
            slotNames.insert(entity.value(QStringLiteral("slot")).toInt(),
                             entity.value(QStringLiteral("name")).toString());
            slotMaterials.insert(entity.value(QStringLiteral("slot")).toInt(), entity);
        }

        QJsonObject root{};
        root[QStringLiteral("frameIndex")] = static_cast<int>(frame.frameIndex);
        root[QStringLiteral("stepIndex")] = static_cast<double>(frame.stepIndex);
        root[QStringLiteral("simTimeNs")] = static_cast<double>(frame.simTimeNs);
        root[QStringLiteral("intervalNs")] = static_cast<double>(reader.sampleIntervalNs());
        root[QStringLiteral("solverStepNs")] = static_cast<double>(reader.metadata().solverStepNs);
        root[QStringLiteral("frameCount")] = static_cast<double>(reader.frameCount());
        root[QStringLiteral("hasTrailer")] = reader.hasTrailer();

        QJsonArray bodies{};
        for (const epsx::BodyState& body : frame.bodies) {
            QJsonObject entry{};
            entry[QStringLiteral("id")] = static_cast<double>(body.id);
            entry[QStringLiteral("slot")] = static_cast<int>(body.slot);
            entry[QStringLiteral("name")] = slotNames.value(static_cast<int>(body.slot));
            entry[QStringLiteral("stateFlags")] = static_cast<int>(body.stateFlags);
            entry[QStringLiteral("position")] = toJsonArray(body.position);
            entry[QStringLiteral("rotation")] =
                QJsonArray{body.rotation.x(), body.rotation.y(), body.rotation.z(), body.rotation.w()};
            entry[QStringLiteral("linearVelocity")] = toJsonArray(body.linearVelocity);
            entry[QStringLiteral("angularVelocity")] = toJsonArray(body.angularVelocity);
            entry[QStringLiteral("mass")] = body.mass;
            entry[QStringLiteral("restitution")] = body.restitution;
            const QJsonObject material = slotMaterials.value(static_cast<int>(body.slot));
            const QJsonObject model = modelMaterials.value(QString::number(body.id));
            entry[QStringLiteral("staticFriction")] = material.value(QStringLiteral("staticFriction"))
                .toDouble(model.value(QStringLiteral("static_friction")).toDouble(0.5));
            entry[QStringLiteral("dynamicFriction")] = material.value(QStringLiteral("dynamicFriction"))
                .toDouble(model.value(QStringLiteral("dynamic_friction")).toDouble(0.5));
            bodies.append(entry);
        }
        root[QStringLiteral("bodies")] = bodies;

        QJsonArray constraints{};
        for (const epsx::ConstraintState& constraint : frame.constraints) {
            QJsonObject entry{};
            entry[QStringLiteral("id")] = static_cast<double>(constraint.id);
            entry[QStringLiteral("slot")] = static_cast<int>(constraint.slot);
            entry[QStringLiteral("type")] = static_cast<int>(constraint.type);
            entry[QStringLiteral("enabled")] = (constraint.flags & epsx::kConstraintFlagEnabled) != 0u;
            entry[QStringLiteral("entityAId")] = static_cast<double>(constraint.entityAId);
            entry[QStringLiteral("entityBId")] = static_cast<double>(constraint.entityBId);
            entry[QStringLiteral("currentLength")] = constraint.currentLength;
            entry[QStringLiteral("error")] = constraint.error;
            constraints.append(entry);
        }
        root[QStringLiteral("constraints")] = constraints;

        QJsonArray fields{};
        for (const epsx::FieldState& field : frame.fields) {
            QJsonObject entry{};
            entry[QStringLiteral("id")] = static_cast<double>(field.id);
            entry[QStringLiteral("slot")] = static_cast<int>(field.slot);
            entry[QStringLiteral("type")] = static_cast<int>(field.type);
            entry[QStringLiteral("enabled")] = (field.flags & epsx::kFieldFlagEnabled) != 0u;
            entry[QStringLiteral("vector")] = toJsonArray(field.vector);
            entry[QStringLiteral("strength")] = field.strength;
            fields.append(entry);
        }
        root[QStringLiteral("fields")] = fields;

        out << QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)) << "\n";
        out.flush();
        return 0;
    }
}
