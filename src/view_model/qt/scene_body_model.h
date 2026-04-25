//
// 由 GitHub Copilot 于 2026/4/25 创建。
//

#ifndef EGRET_PHYSICS_SCENE_BODY_MODEL_H
#define EGRET_PHYSICS_SCENE_BODY_MODEL_H

#include <QAbstractListModel>
#include <QColor>
#include <QHash>
#include <QString>
#include <QVariant>

#include <vector>

namespace egret
{
    /**
     * @brief 单个可渲染实体的 Qt 数据对象。
     *
     * 该结构专门服务于 QML 渲染，不包含任何物理逻辑。
     */
    struct SceneBodyVisualItem
    {
        /** 实体 ID。 */
        quint64 id{0};

        /** 渲染类型：sphere、box、particle。 */
        QString kind;

        /** 左上角 X 坐标。 */
        double x{0.0};

        /** 左上角 Y 坐标。 */
        double y{0.0};

        /** 宽度。 */
        double width{0.0};

        /** 高度。 */
        double height{0.0};

        /** 颜色。 */
        QColor color;

        /** 标签。 */
        QString label;
    };

    /**
     * @brief QML 可直接使用的实体列表模型。
     *
     * 该模型只负责展示，不持有物理对象；数据更新由 ViewModel 层统一驱动。
     */
    class SceneBodyModel final : public QAbstractListModel
    {
        Q_OBJECT

    public:
        /** 模型角色枚举。 */
        enum BodyRole
        {
            IdRole = Qt::UserRole + 1,
            KindRole,
            XRole,
            YRole,
            WidthRole,
            HeightRole,
            ColorRole,
            LabelRole,
        };

        /** 默认构造。 */
        explicit SceneBodyModel(QObject* parent = nullptr);

        /** 默认析构。 */
        ~SceneBodyModel() override = default;

        /** 行数。 */
        [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;

        /** 模型数据。 */
        [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

        /** 角色名称。 */
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        /**
         * @brief 用新的数据覆盖整个模型。
         * @param items 新的实体渲染快照。
         */
        void setItems(const std::vector<SceneBodyVisualItem>& items);

        /**
         * @brief 清空模型。
         */
        void clear();

    private:
        /** 当前模型数据。 */
        std::vector<SceneBodyVisualItem> m_items;
    };
}

#endif // EGRET_PHYSICS_SCENE_BODY_MODEL_H
