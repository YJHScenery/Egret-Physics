//
// 由 GitHub Copilot 于 2026/4/25 创建。
//

#include "scene_body_model.h"

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
        case IdRole:
            return QVariant::fromValue(item.id);
        case KindRole:
            return item.kind;
        case XRole:
            return item.x;
        case YRole:
            return item.y;
        case WidthRole:
            return item.width;
        case HeightRole:
            return item.height;
        case CenterXRole:
            return item.centerX;
        case CenterYRole:
            return item.centerY;
        case CenterZRole:
            return item.centerZ;
        case SpeedXRole:
            return item.speedX;
        case SpeedYRole:
            return item.speedY;
        case SpeedZRole:
            return item.speedZ;
        case SizeXRole:
            return item.sizeX;
        case SizeYRole:
            return item.sizeY;
        case SizeZRole:
            return item.sizeZ;
        case ColorRole:
            return item.color;
        case LabelRole:
            return item.label;
        default:
            return {};
        }
    }

    QHash<int, QByteArray> SceneBodyModel::roleNames() const
    {
        return {
            {IdRole, "bodyId"},
            {KindRole, "shapeKind"},
            {XRole, "bodyX"},
            {YRole, "bodyY"},
            {WidthRole, "bodyWidth"},
            {HeightRole, "bodyHeight"},
            {CenterXRole, "bodyCenterX"},
            {CenterYRole, "bodyCenterY"},
            {CenterZRole, "bodyCenterZ"},
            {SpeedXRole, "bodySpeedX"},
            {SpeedYRole, "bodySpeedY"},
            {SpeedZRole, "bodySpeedZ"},
            {SizeXRole, "bodySizeX"},
            {SizeYRole, "bodySizeY"},
            {SizeZRole, "bodySizeZ"},
            {ColorRole, "bodyColor"},
            {LabelRole, "bodyLabel"},
        };
    }

    void SceneBodyModel::setItems(const std::vector<SceneBodyVisualItem>& items)
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
