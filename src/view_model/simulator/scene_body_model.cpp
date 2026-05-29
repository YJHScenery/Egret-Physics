//
// 由 GitHub Copilot 于 2026/4/25 创建。
//

#include "scene_body_model.h"
#include <magic_enum.hpp>

namespace magic_enum::customize {
    template <>
    struct enum_range<egret::SceneBodyModel::BodyRole> {
        static constexpr int min = Qt::UserRole;
        static constexpr int max = 280; // 留下一定的余量
    };
}


namespace egret
{
    SceneBodyModel::SceneBodyModel(QObject* parent) : QAbstractListModel(parent)
    {
    }

    int SceneBodyModel::rowCount(const QModelIndex& parent) const
    {
        if (parent.isValid()) {
            return 0;
        }
        return static_cast<int>(m_items.size());
    }

    QVariant SceneBodyModel::data(const QModelIndex& index, const int role) const
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= rowCount(QModelIndex())) {
            return {};
        }

        const SceneBodyVisualItem& item = m_items[static_cast<std::size_t>(index.row())];
        switch (role) {
        case IdRole: {
            return QVariant::fromValue(item.id);
        }
        case KindRole: {
            return QVariant::fromValue(item.kind);
        }
        case PositionRole: {
            QVariantList position;
            for (int i = 0; i < 3; ++i) {
                position.append(item.centerPos[i]);
            }
            return position;
        }
        case ScaleRole:{
            QVariantList scale;
            for (int i = 0; i < 3; ++i) {
                scale.append(item.scale[i]);
            }
            return scale;
        }
        case RotationRole:{
            QVariantList rotation;
            for (int i = 0; i < 4; ++i) {
                rotation.append(item.rotation[i]);
            }
            return rotation;
        }
        case VelocityRole:{
            QVariantList speed;
            for (int i = 0; i < 3; ++i) {
                speed.append(item.velocity[i]);
            }
            return speed;
        }
        case AngularVelocityRole:{
            QVariantList angularSpeed;
            for (int i = 0; i < 3; ++i) {
                angularSpeed.append(item.angularVelocity[i]);
            }
            return angularSpeed;
        }
        case ColorRole:{
            return QVariant{item.color};
        }
        case LabelRole:{
            return QVariant{item.label};
        }
        default:
            return {};
        }
    }

    QHash<int, QByteArray> SceneBodyModel::roleNames() const
    {
        QHash<int, QByteArray> roles;

        for (const auto role_value : magic_enum::enum_values<BodyRole>()) {
            std::string_view name_view = magic_enum::enum_name(role_value);
            QString roleName{"body"};
            roleName.append(QString::fromUtf8(name_view.data(), name_view.size()));
            if (roleName.endsWith("Role")) {
                roleName.chop(4);
            }

            roles[static_cast<int>(role_value)] = roleName.toUtf8();
        }

        qDebug() << "SceneBodyModel::roleNames(): " << roles;

        return roles;
    }

    void SceneBodyModel::setItems(const QList<SceneBodyVisualItem>& items)
    {
        const bool sameSize = m_items.size() == items.size();
        bool sameIds = sameSize;
        if (sameSize) {
            for (std::size_t i = 0; i < items.size(); ++i) {
                if (m_items[i].id != items[i].id) {
                    sameIds = false;
                    break;
                }
            }
        }

        if (sameSize && sameIds) {
            m_items = items;
            if (!m_items.empty()) {
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

    void SceneBodyModel::clear()
    {
        beginResetModel();
        m_items.clear();
        endResetModel();
    }
}
