/**
 * @file        clipboard_helper.cpp
 * @brief       编辑器剪贴板辅助类实现文件。
 * @details     实现 ClipboardHelper 的各项成员函数，包括实体复制、剪切、粘贴及偏移逻辑。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-07-11
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     View
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#include "clipboard_helper.h"
#include "view_model/editor/editor_manager.h"
#include "view_model/serialize/model_item_data.h"
#include "utils/unique_id_generator.h"

namespace egret
{
    ClipboardHelper::ClipboardHelper(QObject* parent)
        : QObject(parent)
    {
    }

    ClipboardHelper* ClipboardHelper::instance()
    {
        static ClipboardHelper helper;
        return &helper;
    }

    bool ClipboardHelper::copyEntities(const QStringList& ids)
    {
        // 清空旧剪贴板并重置粘贴计数
        m_clipboard = QJsonArray();
        m_pasteCount = 0;

        if (ids.isEmpty()) {
            emit operationCompleted(false, QStringLiteral("没有选中任何实体"));
            emit contentChanged();
            return false;
        }

        EditorManager* em = EditorManager::instance();
        int copiedCount = 0;

        for (const QString& id : ids) {
            ModelItemData* model = em->getModel(id);
            if (!model) {
                continue;
            }

            // 序列化实体为 JSON
            QJsonObject json = model->toJson();

            // 仅对 Shape 实体 (category == 0) 应用位置偏移；场和约束器不做偏移
            if (model->category() == 0 && json.contains("pos") && json["pos"].isArray()) {
                QJsonArray posArr = json["pos"].toArray();
                if (posArr.size() >= 3) {
                    posArr[0] = posArr[0].toDouble() + static_cast<double>(m_baseOffset.x());
                    posArr[1] = posArr[1].toDouble() + static_cast<double>(m_baseOffset.y());
                    posArr[2] = posArr[2].toDouble() + static_cast<double>(m_baseOffset.z());
                    json["pos"] = posArr;
                }
            }

            if (json.contains("name") && json["name"].isString()) {
                m_nameCount[json["name"].toString()]++;
            }
            m_clipboard.append(json);
            ++copiedCount;
        }

        const bool success = (copiedCount > 0);
        emit operationCompleted(success,
                                success
                                    ? QStringLiteral("已复制 %1 个实体").arg(copiedCount)
                                    : QStringLiteral("未找到可复制的实体"));
        emit contentChanged();
        return success;
    }

    bool ClipboardHelper::cutEntities(const QStringList& ids)
    {
        // 先复制
        if (!copyEntities(ids)) {
            return false;
        }

        // 再删除原实体
        EditorManager* em = EditorManager::instance();
        const int cutCount = em->removeModels(ids);

        emit operationCompleted(true,
                                QStringLiteral("已剪切 %1 个实体").arg(cutCount));
        // contentChanged 已在 copyEntities 中触发
        return true;
    }

    QStringList ClipboardHelper::pasteEntities()
    {
        if (m_clipboard.isEmpty()) {
            emit operationCompleted(false, QStringLiteral("剪贴板为空"));
            return {};
        }

        // 计算本次粘贴的累加偏移量
        const QVector3D accumulatedOffset = m_baseOffset * static_cast<float>(m_pasteCount);
        ++m_pasteCount;

        EditorManager* em = EditorManager::instance();
        QStringList newIds;
        QHash<QString, QString> idMap;
        for (const auto& value : m_clipboard) {
            const QString oldId = value.toObject().value("id").toString();
            if (oldId.isEmpty() || idMap.contains(oldId)) continue;
            QString newId;
            do { newId = QString::number(UniqueIDGenerator::instance().generate()); }
            while (em->getModel(newId) || idMap.values().contains(newId));
            idMap.insert(oldId, newId);
        }
        QList<ModelItemData*> models;

        for (const QJsonValueRef& val : m_clipboard) {
            if (!val.isObject()) {
                continue;
            }

            QJsonObject json = val.toObject();
            const QString originalId = json.value("id").toString();
            if (!idMap.contains(originalId)) continue;

            // 仅对 Shape 实体 (category == 0) 应用位置累加偏移
            int cat = json.value("category").toInt(-1);
            if (cat == 0 && json.contains("pos") && json["pos"].isArray()) {
                QJsonArray posArr = json["pos"].toArray();
                if (posArr.size() >= 3) {
                    posArr[0] = posArr[0].toDouble() + static_cast<double>(accumulatedOffset.x());
                    posArr[1] = posArr[1].toDouble() + static_cast<double>(accumulatedOffset.y());
                    posArr[2] = posArr[2].toDouble() + static_cast<double>(accumulatedOffset.z());
                    json["pos"] = posArr;
                }
            }

            // 同时复制对象与约束时，约束必须指向新对象。
            const QJsonArray oldTargets = json.value("entity_ids").toArray();
            QJsonArray newTargets;
            bool allTargetsCopied = !oldTargets.isEmpty();
            for (const auto& target : oldTargets) {
                const QString targetId = target.toString();
                if (!idMap.contains(targetId)) allTargetsCopied = false;
                newTargets.append(idMap.value(targetId, targetId));
            }
            if (!oldTargets.isEmpty()) json["entity_ids"] = newTargets;
            QJsonObject constraintData = json.value("constraint_data").toObject();
            const QJsonArray oldOverrides = constraintData.value("entity_overrides").toArray();
            if (!oldOverrides.isEmpty()) {
                QJsonArray newOverrides;
                for (const auto& value : oldOverrides) {
                    QJsonObject entry = value.toObject();
                    const QString targetId = entry.value("entity_id").toString();
                    entry["entity_id"] = idMap.value(targetId, targetId);
                    newOverrides.append(entry);
                }
                constraintData["entity_overrides"] = newOverrides;
                json["constraint_data"] = constraintData;
            }

            // 跟随被复制的对象平移导轨控制点和单摆锚点。
            if (allTargetsCopied && cat == 2) {
                const QVector3D offset = m_baseOffset + accumulatedOffset;
                QJsonObject data = json.value("constraint_data").toObject();
                for (const auto& key : {"start", "control1", "control2", "end", "anchor_pos"}) {
                    QJsonArray point = data.value(key).toArray();
                    if (point.size() < 3) continue;
                    point[0] = point[0].toDouble() + offset.x();
                    point[1] = point[1].toDouble() + offset.y();
                    point[2] = point[2].toDouble() + offset.z();
                    data[key] = point;
                }
                json["constraint_data"] = data;
                QJsonObject anchor = json.value("anchor_pos").toObject();
                if (!anchor.isEmpty()) {
                    anchor["x"] = anchor.value("x").toDouble() + offset.x();
                    anchor["y"] = anchor.value("y").toDouble() + offset.y();
                    anchor["z"] = anchor.value("z").toDouble() + offset.z();
                    json["anchor_pos"] = anchor;
                }
            }

            // 从 JSON 反序列化创建新实体
            auto* newModel = new ModelItemData();
            newModel->fromJson(json);

            // 生成全新 ID（避免与原有实体冲突）
            newModel->setId(idMap.value(originalId));

            newModel->setName(newModel->name() + "-" + QString::number(m_nameCount.value(newModel->name())));

            m_nameCount[json.value("name").toString()]++;

            newIds.append(newModel->id());
            models.append(newModel);
        }

        if (!models.isEmpty()) em->addModels(models);

        emit operationCompleted(!newIds.isEmpty(),
                                newIds.isEmpty()
                                    ? QStringLiteral("粘贴失败")
                                    : QStringLiteral("已粘贴 %1 个实体").arg(newIds.size()));
        return newIds;
    }

    void ClipboardHelper::clear()
    {
        m_clipboard = QJsonArray();
        m_pasteCount = 0;
        emit contentChanged();
    }

    bool ClipboardHelper::hasContent() const
    {
        return !m_clipboard.isEmpty();
    }

    int ClipboardHelper::contentCount() const
    {
        return m_clipboard.size();
    }
} // egret
