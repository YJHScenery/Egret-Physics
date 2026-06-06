/**
 * @file        scene_constraint_model.cpp
 * @brief       场景约束视觉数据模型实现文件，提供可渲染约束的 Qt 数据模型。
 * @details     实现 SceneConstraintModel 类的各项成员函数。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-06-15
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     ViewModel
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#include "scene_constraint_model.h"
#include <magic_enum.hpp>

namespace magic_enum::customize
{
    template <>
    struct enum_range<egret::SceneConstraintModel::ConstraintRole>
    {
        static constexpr int min = Qt::UserRole;
        static constexpr int max = Qt::UserRole + 16;
    };
}

namespace egret
{
    SceneConstraintModel::SceneConstraintModel(QObject *parent) : QAbstractListModel(parent)
    {
    }

    int SceneConstraintModel::rowCount(const QModelIndex &parent) const
    {
        if (parent.isValid())
        {
            return 0;
        }
        return static_cast<int>(m_items.size());
    }

    QVariant SceneConstraintModel::data(const QModelIndex &index, const int role) const
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= rowCount(QModelIndex()))
        {
            return {};
        }

        const SceneConstraintVisualItem &item = m_items[static_cast<std::size_t>(index.row())];
        switch (role)
        {
        case IdRole:
        {
            return QVariant::fromValue(item.basic.id);
        }
        case KindRole:
        {
            return QVariant::fromValue(item.basic.kind);
        }
        case EntityIdARole:
        {
            return QVariant::fromValue(item.entityIdA);
        }
        case EntityIdBRole:
        {
            return QVariant::fromValue(item.entityIdB);
        }
        case PositionARole:
        {
            QVariantList position;
            for (int i = 0; i < 3; ++i)
            {
                position.append(item.positionA[i]);
            }
            return position;
        }
        case PositionBRole:
        {
            QVariantList position;
            for (int i = 0; i < 3; ++i)
            {
                position.append(item.positionB[i]);
            }
            return position;
        }
        case ConstraintLengthRole:
        {
            return QVariant::fromValue(item.constraintLength);
        }
        case CurrentDistanceRole:
        {
            return QVariant::fromValue(item.currentDistance);
        }
        case ColorRole:
        {
            return QVariant{item.basic.color};
        }
        case LabelRole:
        {
            return QVariant{item.basic.label};
        }
        default:
            return {};
        }
    }

    QHash<int, QByteArray> SceneConstraintModel::roleNames() const
    {
        QHash<int, QByteArray> roles;

        for (const auto role_value : magic_enum::enum_values<ConstraintRole>())
        {
            std::string_view name_view = magic_enum::enum_name(role_value);
            QString roleName{"constraint"};
            roleName.append(QString::fromUtf8(name_view.data(), name_view.size()));
            if (roleName.endsWith("Role"))
            {
                roleName.chop(4);
            }

            roles[static_cast<int>(role_value)] = roleName.toUtf8();
        }

        // qDebug() << "SceneConstraintModel::roleNames(): " << roles;

        return roles;
    }

    void SceneConstraintModel::setItems(const QList<SceneConstraintVisualItem> &items)
    {
        const bool sameSize = m_items.size() == items.size();
        bool sameIds = sameSize;
        if (sameSize)
        {
            for (std::size_t i = 0; i < items.size(); ++i)
            {
                if (m_items[i].basic.id != items[i].basic.id)
                {
                    sameIds = false;
                    break;
                }
            }
        }

        if (sameSize && sameIds)
        {
            m_items = items;
            if (!m_items.empty())
            {
                const QModelIndex first = index(0, 0);
                const QModelIndex last = index(static_cast<int>(m_items.size()) - 1, 0);
                emit dataChanged(first, last);
            }
            return;
        }

        beginResetModel();
        m_items = items;
        endResetModel();
    }

    void SceneConstraintModel::clear()
    {
        beginResetModel();
        m_items.clear();
        endResetModel();
    }
}
