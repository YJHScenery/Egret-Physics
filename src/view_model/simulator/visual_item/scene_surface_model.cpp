/**
 * @file        scene_surface_model.cpp
 * @brief       场景曲面视觉数据模型实现文件，提供可渲染曲面的 Qt 数据模型。
 * @details     实现 SceneSurfaceModel 类的各项成员函数。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-07-15
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     ViewModel
 */

#include "scene_surface_model.h"

#include <QSet>
#include <magic_enum.hpp>
#include <ranges>

namespace magic_enum::customize
{
    template <>
    struct enum_range<egret::SceneSurfaceModel::SurfaceRole>
    {
        static constexpr int min = Qt::UserRole;
        static constexpr int max = Qt::UserRole + 16;
    };
}

namespace egret
{
    SceneSurfaceModel::SceneSurfaceModel(QObject* parent)
        : QAbstractListModel(parent)
    {
    }

    int SceneSurfaceModel::rowCount(const QModelIndex& parent) const
    {
        if (parent.isValid()) {
            return 0;
        }
        return static_cast<int>(m_items.size());
    }

    QVariant SceneSurfaceModel::data(const QModelIndex& index, int role) const
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= rowCount(QModelIndex())) {
            return {};
        }

        const SceneSurfaceVisualItem& item = m_items[static_cast<qsizetype>(index.row())];
        switch (role) {
        case SurfaceIdRole:
            return QVariant::fromValue(item.constraintId);
        case SurfaceKindRole:
            return QVariant::fromValue(item.surfaceKind);
        case SurfaceGeometryRole:
            return QVariant::fromValue(static_cast<QObject*>(item.surfaceGeometry));
        case SurfaceColorRole:
            return QVariant{item.color};
        default:
            return {};
        }
    }

    QHash<int, QByteArray> SceneSurfaceModel::roleNames() const
    {
        QHash<int, QByteArray> roles;

        for (const auto roleValue : magic_enum::enum_values<SurfaceRole>()) {
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

    void SceneSurfaceModel::setItems(const std::vector<SceneSurfaceRenderItem>& renderItems)
    {
        // 收集新数据中的约束 ID 集合
        QSet<quint64> newIds;
        QList<SceneSurfaceVisualItem> newItems;
        newItems.reserve(static_cast<int>(renderItems.size()));

        for (const auto& renderItem : renderItems) {
            const quint64 id = renderItem.constraintId;
            newIds.insert(id);

            // 获取或创建 SurfaceGeometry，并更新曲面数据
            SurfaceGeometry* geom = getOrCreateGeometry(id);
            if (renderItem.surface) {
                // 获取曲面指针（原始指针，setSurface 内部会 clone）
                geom->setSurface(renderItem.surface.get());

                // 对无限大平面应用显示范围
                if (static_cast<SurfaceType>(renderItem.surfaceType) == SurfaceType::Plane) {
                    geom->setPlaneExtent(m_planeExtent);
                }
            }

            SceneSurfaceVisualItem visualItem{};
            visualItem.constraintId = id;
            visualItem.surfaceKind = renderItem.surfaceType;
            visualItem.surfaceGeometry = geom;

            // 根据曲面类型分配默认颜色
            switch (static_cast<SurfaceType>(renderItem.surfaceType)) {
            case SurfaceType::Plane:
                visualItem.color = "#4A90D9";   // 蓝色
                break;
            case SurfaceType::Bezier:
                visualItem.color = "#9B59B6";   // 紫色
                break;
            default:
                visualItem.color = "#7F8C8D";   // 灰色
                break;
            }

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

    void SceneSurfaceModel::clear()
    {
        beginResetModel();
        m_items.clear();
        m_geometryCache.clear();
        endResetModel();
    }

    void SceneSurfaceModel::setPlaneExtent(double extent)
    {
        m_planeExtent = extent;
        // 立即对已有的 Plane 几何体应用
        for (const auto& geom : m_geometryCache | std::views::values) {
            geom->setPlaneExtent(m_planeExtent);
        }
    }

    SurfaceGeometry* SceneSurfaceModel::getOrCreateGeometry(std::uint64_t constraintId)
    {
        const auto key = static_cast<quint64>(constraintId);
        auto it = m_geometryCache.find(key);
        if (it != m_geometryCache.end()) {
            return it->second.get();
        }

        // 创建新的 SurfaceGeometry（无 parent，由 unique_ptr 管理所有权）
        // 注意：不传入 parent 以避免 Qt 对象树与 unique_ptr 双重释放
        // 先 emplace 再取指针，避免 unique_ptr 被 move 后 rawPtr 失效
        auto [insertedIt, inserted] = m_geometryCache.emplace(
            key, std::make_unique<SurfaceGeometry>(nullptr));
        return insertedIt->second.get();
    }
} // egret
