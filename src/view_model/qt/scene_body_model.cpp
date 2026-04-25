//
// 由 GitHub Copilot 于 2026/4/25 创建。
//

#include "scene_body_model.h"

namespace egret
{
    SceneBodyModel::SceneBodyModel(QObject* parent): QAbstractListModel(parent)
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
        if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
            return {};
        }

        const SceneBodyVisualItem& item = m_items[static_cast<std::size_t>(index.row())];
        switch (role) {
        case IdRole: return QVariant::fromValue(item.id);
        case KindRole: return item.kind;
        case XRole: return item.x;
        case YRole: return item.y;
        case WidthRole: return item.width;
        case HeightRole: return item.height;
        case ColorRole: return item.color;
        case LabelRole: return item.label;
        default: return {};
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
            {ColorRole, "bodyColor"},
            {LabelRole, "bodyLabel"},
        };
    }

    void SceneBodyModel::setItems(const std::vector<SceneBodyVisualItem>& items)
    {
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
