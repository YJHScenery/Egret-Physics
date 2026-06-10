/**
 * @file        scene_convex_mesh_model.cpp
 * @brief       场景凸体网格视觉数据模型实现文件，提供可渲染凸体网格的 Qt 数据模型。
 * @details     实现 SceneConvexMeshModel 类的各项成员函数。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-10
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     ViewModel
 */

#include "scene_convex_mesh_model.h"

#include <QSet>
#include <magic_enum.hpp>
#include <QQuaternion>
#include <QVector3D>
#include <ranges>

namespace magic_enum::customize
{
    template <>
    struct enum_range<egret::SceneConvexMeshModel::ConvexMeshRole>
    {
        static constexpr int min = Qt::UserRole;
        static constexpr int max = Qt::UserRole + 16;
    };
}

namespace egret
{
    SceneConvexMeshModel::SceneConvexMeshModel(QObject* parent)
        : QAbstractListModel(parent)
    {
    }

    int SceneConvexMeshModel::rowCount(const QModelIndex& parent) const
    {
        if (parent.isValid()) {
            return 0;
        }
        return static_cast<int>(m_items.size());
    }

    QVariant SceneConvexMeshModel::data(const QModelIndex& index, int role) const
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= rowCount(QModelIndex())) {
            return {};
        }

        const SceneConvexMeshVisualItem& item = m_items[static_cast<qsizetype>(index.row())];
        switch (role) {
        case ConvexMeshIdRole:
            return QVariant::fromValue(item.bodyId);
        case ConvexMeshGeometryRole:
            return QVariant::fromValue(static_cast<QObject*>(item.meshGeometry));
        case ConvexMeshPositionRole: {
            QVector3D pos{
                static_cast<float>(item.position[0]),
                static_cast<float>(item.position[1]),
                static_cast<float>(item.position[2])};
            return QVariant::fromValue(pos);
        }
        case ConvexMeshScaleRole: {
            QVector3D scl{
                static_cast<float>(item.scale[0]),
                static_cast<float>(item.scale[1]),
                static_cast<float>(item.scale[2])};
            return QVariant::fromValue(scl);
        }
        case ConvexMeshRotationRole: {
            QQuaternion rot{
                static_cast<float>(item.rotation[0]),
                static_cast<float>(item.rotation[1]),
                static_cast<float>(item.rotation[2]),
                static_cast<float>(item.rotation[3])};
            return QVariant::fromValue(rot);
        }
        case ConvexMeshColorRole:
            return QVariant{item.color};
        default:
            return {};
        }
    }

    QHash<int, QByteArray> SceneConvexMeshModel::roleNames() const
    {
        QHash<int, QByteArray> roles;

        for (const auto roleValue : magic_enum::enum_values<ConvexMeshRole>()) {
            std::string_view nameView = magic_enum::enum_name(roleValue);
            QString roleName{QString::fromUtf8(nameView.data(), static_cast<int>(nameView.size()))};

            // 去掉 "Role" 后缀
            if (roleName.endsWith("Role")) {
                roleName.chop(4);
            }

            // 首字母小写（QML 属性命名惯例为 camelCase）
            if (roleName.length() > 0) {
                roleName[0] = roleName[0].toLower();
            }

            roles[static_cast<int>(roleValue)] = roleName.toUtf8();
        }

        return roles;
    }

    void SceneConvexMeshModel::setItems(const std::vector<SceneConvexMeshRenderItem>& renderItems)
    {
        // 收集新数据中的刚体 ID 集合
        QSet<quint64> newIds;
        QList<SceneConvexMeshVisualItem> newItems;
        newItems.reserve(static_cast<int>(renderItems.size()));

        for (auto& renderItem : renderItems) {
            const quint64 id = renderItem.bodyId;
            newIds.insert(id);

            // 获取或创建 ConvexMeshGeometry，并更新网格数据
            ConvexMeshGeometry* geom = getOrCreateGeometry(id);
            if (renderItem.mesh) {
                geom->setMesh(*renderItem.mesh);
            }

            SceneConvexMeshVisualItem visualItem{};
            visualItem.bodyId = id;
            visualItem.meshGeometry = geom;

            // 填充变换数据（已在 WorldSceneManager 中完成坐标转换）
            visualItem.position = {
                renderItem.position.x(),
                renderItem.position.y(),
                renderItem.position.z()};
            visualItem.scale = {
                renderItem.scale.x(),
                renderItem.scale.y(),
                renderItem.scale.z()};
            visualItem.rotation = {
                renderItem.rotation.w(),
                renderItem.rotation.x(),
                renderItem.rotation.y(),
                renderItem.rotation.z()};

            // ConvexMesh 默认颜色
            visualItem.color = renderItem.color.isEmpty()
                               ? QStringLiteral("#E74C3C")
                               : renderItem.color;

            newItems.append(std::move(visualItem));
        }

        // 清理已移除的几何体缓存
        for (auto it = m_geometryCache.begin(); it != m_geometryCache.end(); ) {
            if (!newIds.contains(it->first)) {
                it = m_geometryCache.erase(it);
            } else {
                ++it;
            }
        }

        // 更新模型数据
        beginResetModel();
        m_items = std::move(newItems);
        endResetModel();
    }

    void SceneConvexMeshModel::clear()
    {
        beginResetModel();
        m_items.clear();
        m_geometryCache.clear();
        endResetModel();
    }

    ConvexMeshGeometry* SceneConvexMeshModel::getOrCreateGeometry(std::uint64_t bodyId)
    {
        const auto key = static_cast<quint64>(bodyId);
        auto it = m_geometryCache.find(key);
        if (it != m_geometryCache.end()) {
            return it->second.get();
        }

        // 创建新的 ConvexMeshGeometry（无 parent，由 unique_ptr 管理所有权）
        auto [insertedIt, inserted] = m_geometryCache.emplace(
            key, std::make_unique<ConvexMeshGeometry>(nullptr));
        return insertedIt->second.get();
    }
} // egret