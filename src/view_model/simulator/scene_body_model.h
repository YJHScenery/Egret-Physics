/**
 * @file        scene_body_model.h
 * @brief       场景实体数据模型头文件，提供可渲染实体的 Qt 数据模型。
 * @details     定义 SceneBodyModel 类，继承自 QAbstractListModel，
 *              用于在 QML 中展示物理场景中的实体列表。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-05-04
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     ViewModel
 * @defgroup    组名 (如果文件定义了一个模块组)
 */

#ifndef EGRET_PHYSICS_SCENE_BODY_MODEL_H
#define EGRET_PHYSICS_SCENE_BODY_MODEL_H

#include <QAbstractListModel>
#include <QColor>
#include <QHash>
#include <QString>
#include <QVariant>
#include <magic_enum.hpp>
#include <vector>

namespace egret
{
    /**
     * @brief       单个可渲染实体的 Qt 数据对象。
     * @details     SceneBodyVisualItem 是单个可渲染实体的 Qt 数据对象结构体。
     *              该结构专门服务于 QML 渲染，不包含任何物理逻辑。
     *              包含实体ID、渲染类型、位置、缩放、旋转、速度、颜色、标签等。
     *              用于 SceneBodyModel 的数据存储。
     *
     * @invariant   id 是稳定的实体标识符
     * @invariant   kind 是有效的渲染类型枚举
     * @see         SceneBodyModel, SceneRenderItem
     */
    struct SceneBodyVisualItem
    {
        /** 实体 ID。 */
        quint64 id{0};

        /** 渲染类型：sphere、box、particle。 */
        quint32 kind;

        /** 渲染中心位置（x, y, z）。 */
        double centerPos[3]{0.0, 0.0, 0.0};

        double scale[3]{1.0, 1.0, 1.0};

        /** 旋转四元数 (w, x, y, z)。 */
        double rotation[4]{1.0, 0.0, 0.0, 0.0};

        double velocity[3]{0.0, 0.0, 0.0};

        double angularVelocity[3]{0.0, 0.0, 0.0};

        /** 颜色。 */
        QString color;

        /** 标签。 */
        QString label;
    };

    /**
     * @brief       QML 可直接使用的实体列表模型。
     * @details     SceneBodyModel 继承自 QAbstractListModel，
     *              是 QML 可直接使用的实体列表模型。
     *              该模型只负责展示，不持有物理对象；数据更新由 ViewModel 层统一驱动。
     *              支持 QML 的 ListView、Repeater 等组件绑定。
     *              提供角色名称映射，支持 QML 属性绑定。
     *
     * @invariant   m_items 包含所有可渲染实体数据
     * @remark      SceneBodyModel 是 QAbstractListModel，支持 QML 数据绑定
     * @see         SceneBodyVisualItem, SceneManagerViewModel, QAbstractListModel
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
            PositionRole,
            ScaleRole,
            RotationRole,
            VelocityRole,
            AngularVelocityRole,
            ColorRole,
            LabelRole,
        };

        /** 默认构造。 */
        explicit SceneBodyModel(QObject *parent = nullptr);

        /** 默认析构。 */
        ~SceneBodyModel() override = default;

        /** 行数。 */ //  = QModelIndex()
        [[nodiscard]] int rowCount(const QModelIndex &parent) const override;

        /** 模型数据。 */ //  = Qt::DisplayRole
        [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;

        /** 角色名称。 */
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        /**
         * @brief 用新的数据覆盖整个模型。
         * @param items 新的实体渲染快照。
         */
        void setItems(const QList<SceneBodyVisualItem> &items);

        /**
         * @brief 清空模型。
         */
        void clear();

    private:
        /** 当前模型数据。 */
        QList<SceneBodyVisualItem> m_items;
    };
}

#endif // EGRET_PHYSICS_SCENE_BODY_MODEL_H
