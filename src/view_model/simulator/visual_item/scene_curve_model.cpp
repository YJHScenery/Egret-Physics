/**
 * @file        scene_curve_model.cpp
 * @brief       场景曲线视觉数据模型实现文件，提供可渲染曲线的 Qt 数据模型。
 * @details     实现 SceneCurveModel 类的各项成员函数。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-07-23
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     ViewModel
 */

#include "scene_curve_model.h"

#include <QSet>
#include <magic_enum.hpp>
#include <ranges>

namespace magic_enum::customize
{
    template <>
    struct enum_range<egret::SceneCurveModel::CurveRole>
    {
        static constexpr int min = Qt::UserRole;
        static constexpr int max = Qt::UserRole + 16;
    };
}

namespace egret
{
    SceneCurveModel::SceneCurveModel(QObject* parent)
        : QAbstractListModel(parent)
    {
    }

    int SceneCurveModel::rowCount(const QModelIndex& parent) const
    {
        if (parent.isValid()) {
            return 0;
        }
        return static_cast<int>(m_items.size());
    }

    QVariant SceneCurveModel::data(const QModelIndex& index, int role) const
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= rowCount(QModelIndex())) {
            return {};
        }

        const SceneCurveVisualItem& item = m_items[static_cast<qsizetype>(index.row())];
        switch (role) {
        case CurveIdRole:
            return QVariant::fromValue(item.constraintId);
        case CurveKindRole:
            return QVariant::fromValue(item.curveKind);
        case CurveGeometryRole:
            return QVariant::fromValue(static_cast<QObject*>(item.curveGeometry));
        case CurveColorRole:
            return QVariant{item.color};
        default:
            return {};
        }
    }

    QHash<int, QByteArray> SceneCurveModel::roleNames() const
    {
        QHash<int, QByteArray> roles;

        for (const auto roleValue : magic_enum::enum_values<CurveRole>()) {
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

    void SceneCurveModel::setItems(const std::vector<SceneCurveRenderItem>& renderItems)
    {
        // 收集新数据中的约束 ID 集合
        QSet<quint64> newIds;
        QList<SceneCurveVisualItem> newItems;
        newItems.reserve(static_cast<int>(renderItems.size()));

        for (const auto& renderItem : renderItems) {
            const quint64 id = renderItem.constraintId;
            newIds.insert(id);

            // 获取或创建 CurveGeometry，并更新曲线数据
            CurveGeometry* geom = getOrCreateGeometry(id);
            if (renderItem.curve) {
                geom->setCurve(renderItem.curve.get());
            }

            SceneCurveVisualItem visualItem{};
            visualItem.constraintId = id;
            visualItem.curveKind = renderItem.curveType;
            visualItem.curveGeometry = geom;

            // 根据曲线类型分配默认颜色，若 renderItem 已有颜色则优先使用
            if (!renderItem.color.isEmpty()) {
                visualItem.color = renderItem.color;
            } else {
                switch (static_cast<CurveType>(renderItem.curveType)) {
                case CurveType::Bezier:
                    visualItem.color = "#E67E22";   // 橙色
                    break;
                case CurveType::ConicSection:
                    visualItem.color = "#2ECC71";   // 绿色
                    break;
                case CurveType::Helix:
                    visualItem.color = "#3498DB";   // 蓝色
                    break;
                default:
                    visualItem.color = "#95A5A6";   // 灰色
                    break;
                }
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

    void SceneCurveModel::clear()
    {
        beginResetModel();
        m_items.clear();
        m_geometryCache.clear();
        endResetModel();
    }

    CurveGeometry* SceneCurveModel::getOrCreateGeometry(std::uint64_t constraintId)
    {
        const auto key = static_cast<quint64>(constraintId);
        auto it = m_geometryCache.find(key);
        if (it != m_geometryCache.end()) {
            return it->second.get();
        }

        // 创建新的 CurveGeometry（无 parent，由 unique_ptr 管理所有权）
        auto [insertedIt, inserted] = m_geometryCache.emplace(
            key, std::make_unique<CurveGeometry>(nullptr));
        return insertedIt->second.get();
    }
} // egret
